#!/usr/bin/env python3
"""Regression tests for measurement validity, refusal and process cleanup."""

import contextlib
import copy
import errno
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location("sole_comb_bench", Path(__file__).with_name("bench.py"))
bench = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bench)
LIMITS = {"load_ceiling": 8.0, "load_wait_max_s": 10, "leg_deadline_s": 3}
THREADS_SUPPORTED = sys.platform == "darwin" or sys.platform.startswith("linux")


def sample(cpu=10, peak=1, **overrides):
    return {"wall_ms": cpu * 2, "cpu_ms": cpu, "user_cpu_ms": cpu * 0.8,
            "system_cpu_ms": cpu * 0.2, "exit_code": 0, "timed_out": False,
            "load1_peak": peak, "threads_peak": 2, "accept_line_found": True,
            **overrides}


class Clock:
    def __init__(self):
        self.value = 0

    def now(self):
        return self.value

    def sleep(self, seconds):
        self.value += seconds


class LegTests(unittest.TestCase):
    def run_samples(self, samples, **kwargs):
        pending = iter(samples)
        report = {"name": "test"}
        options = {"runner": lambda *args: copy.deepcopy(next(pending)),
                   "load": lambda: 1.0, "emit": lambda line: None, **kwargs}
        code = bench.run_leg(report, LIMITS, ["unused"], **options)
        return code, report

    def test_warmup_excluded_and_five_timed_runs(self):
        code, report = self.run_samples([sample(9000), *[sample(i) for i in range(1, 6)]])
        self.assertEqual(code, 0)
        self.assertEqual(report["summary"]["cpu_ms"]["median"], 3)
        self.assertEqual(report["summary"]["runs"], 5)
        self.assertEqual([row["run"] for row in report["samples"]], ["warm", 1, 2, 3, 4, 5])

    def test_load_rise_after_child_discards_and_repeats_same_run(self):
        loads = iter([1, 1, 1, 9, 1, 1])
        code, report = self.run_samples([sample(), sample(1000), sample(4)], runs=1, load=lambda: next(loads))
        self.assertEqual(code, 0)
        self.assertEqual(report["summary"]["cpu_ms"]["median"], 4)
        self.assertEqual([row["run"] for row in report["samples"]], ["warm", 1, 1])
        self.assertFalse(report["samples"][1]["accepted"])

    def test_load_spike_during_child_discards_warmup(self):
        code, report = self.run_samples([sample(peak=9), sample(), sample()], runs=1)
        self.assertEqual(code, 0)
        self.assertEqual([row["run"] for row in report["samples"]], ["warm", "warm", 1])

    def test_ceiling_boundary_is_accepted(self):
        code, _ = self.run_samples([sample(peak=8), sample(peak=8)], runs=1, load=lambda: 8)
        self.assertEqual(code, 0)

    def test_busy_machine_never_starts_child(self):
        clock = Clock()
        code, report = self.run_samples([], load=lambda: 9, now=clock.now, sleep=clock.sleep, poll_s=3)
        self.assertEqual((code, clock.value), (3, 10))
        self.assertEqual(report["samples"], [])
        self.assertNotIn("summary", report)

    def test_load_wait_then_recovery(self):
        clock = Clock()
        loads = iter([9, 1, 1, 1, 1])
        code, report = self.run_samples([sample(), sample()], runs=1, load=lambda: next(loads),
                                        now=clock.now, sleep=clock.sleep, poll_s=3)
        self.assertEqual((code, clock.value, report["status"]), (0, 3, "PASS"))

    def test_repeated_discard_is_bounded_even_if_pre_run_load_is_low(self):
        clock = Clock()

        def runner(*args):
            clock.sleep(5)
            return sample(peak=9)

        code, report = self.run_samples([], runner=runner, now=clock.now)
        self.assertEqual((code, len(report["samples"])), (3, 2))
        self.assertNotIn("summary", report)

    def test_failure_never_emits_median_even_after_successful_samples(self):
        for failure, expected in [(sample(exit_code=1), 1), (sample(timed_out=True), 3),
                                  (sample(accept_line_found=False), 1)]:
            with self.subTest(failure=failure):
                code, report = self.run_samples([sample(), sample(), failure], accept_line="All terms check.")
                self.assertEqual(code, expected)
                self.assertNotIn("summary", report)
                self.assertFalse(report["samples"][-1]["accepted"])

    def test_missing_threads_are_explicit_and_strict_mode_refuses(self):
        code, report = self.run_samples([sample(threads_peak=None)], require_threads=True)
        self.assertEqual(code, 3)
        self.assertIsNone(report["samples"][0]["threads_peak"])

    def test_nonfinite_loads_cannot_pass_or_break_json(self):
        for value in (float("nan"), float("inf")):
            loads = iter([1, value])
            code, report = self.run_samples([sample()], load=lambda: next(loads))
            self.assertEqual(code, 3)
            json.dumps(report, allow_nan=False)

    def test_invalid_limits_are_rejected(self):
        for key in LIMITS:
            for value in (0, -1, float("nan"), float("inf"), True, "8"):
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    bench.limits({**LIMITS, key: value})


class ChildTests(unittest.TestCase):
    def test_wait4_cpu_and_real_acceptance_line(self):
        result = bench.measure([sys.executable, "-c", "sum(range(500000)); print('All terms check.')"], 5, "All terms check.")
        self.assertEqual(result["exit_code"], 0)
        self.assertGreater(result["cpu_ms"], 0)
        self.assertAlmostEqual(result["cpu_ms"], result["user_cpu_ms"] + result["system_cpu_ms"])
        self.assertTrue(result["accept_line_found"])
        self.assertFalse(result["timed_out"])
        if THREADS_SUPPORTED:
            self.assertIsNotNone(result["threads_peak"])
            self.assertGreater(result["thread_observations"], 0)
        else:
            self.assertIsNone(result["threads_peak"])
            self.assertIsNotNone(result["thread_observation_error"])

    def test_thread_count_is_the_child_thread_number(self):
        # A sample in the zombie window can record ESRCH, so the error field is not asserted.
        one = bench.measure([sys.executable, "-c", "import time; time.sleep(0.3)"], 5)
        two = bench.measure([sys.executable, "-c", "import threading,time; "
                             "threading.Thread(target=time.sleep, args=(0.6,)).start(); time.sleep(0.3)"], 5)
        if THREADS_SUPPORTED:
            self.assertGreater(one["thread_observations"], 0)
            self.assertEqual(one["threads_peak"], 1)
            self.assertGreaterEqual(two["threads_peak"], 2)
        else:
            self.assertIsNone(one["threads_peak"])
            self.assertIsNotNone(one["thread_observation_error"])

    def test_observer_records_mid_run_load_peak_and_nonfinite_error(self):
        # Call 0 is the start value; call 1 is the first observer poll. A counter never runs out.
        for spike, peak, error in [(9.0, 9.0, None), (float("nan"), 1.0, "non-finite load average")]:
            with self.subTest(spike=spike):
                calls = iter(range(10 ** 6))
                with patch.object(bench.os, "getloadavg",
                                  side_effect=lambda: (spike if next(calls) == 1 else 1.0, 1.0, 1.0)):
                    result = bench.measure([sys.executable, "-c", "import time; time.sleep(0.2)"], 5)
                self.assertEqual(result["load1_peak"], peak)
                self.assertEqual(result["load_observation_error"], error)

    def test_timeout_survives_eperm_from_zombie_process_group(self):
        real = os.killpg

        def eperm(pid, sig):
            try:
                real(pid, sig)
            except ProcessLookupError:
                pass
            raise PermissionError(errno.EPERM, "Operation not permitted")

        command = [sys.executable, "-c", "import time; time.sleep(20)"]
        with patch.object(bench.os, "killpg", eperm):
            result = bench.measure(command, 0.3)
            report = {"name": "test"}
            code = bench.run_leg(report, {**LIMITS, "load_ceiling": 1000, "leg_deadline_s": 0.3}, command,
                                 runs=1, load=lambda: 1.0, emit=lambda line: None)
        self.assertTrue(result["timed_out"])
        self.assertEqual((code, report["status"], len(report["samples"])), (3, "UNMET", 1))

    def test_zero_exit_without_acceptance_line_is_not_accept(self):
        result = bench.measure([sys.executable, "-c", "print('not checked')"], 5, "All terms check.")
        self.assertEqual(result["exit_code"], 0)
        self.assertFalse(result["accept_line_found"])

    def test_timeout_kills_descendant_before_delayed_write(self):
        with tempfile.TemporaryDirectory() as folder:
            marker = Path(folder) / "leaked"
            grandchild = f"import time; from pathlib import Path; time.sleep(1); Path({str(marker)!r}).write_text('leaked')"
            child = f"import subprocess,sys,time; subprocess.Popen([sys.executable,'-c',{grandchild!r}]); print('started',flush=True); time.sleep(20)"
            result = bench.measure([sys.executable, "-c", child], 0.5)
            self.assertTrue(result["timed_out"])
            self.assertIn("started", result["stdout"]["excerpt"])
            time.sleep(1.1)
            self.assertFalse(marker.exists(), "timeout left the child process group running")

    def test_output_matching_handles_ansi_and_refuses_substrings(self):
        for output, expected in [(b"\x1b[32mAll terms check.\x1b[0m\n", True),
                                 (b"prefix All terms check.\n", False),
                                 (b"x" * 65536 + b"All terms check.\n", False)]:
            with self.subTest(expected=expected):
                record, matched = bench.output_record(io.BytesIO(output), "All terms check.")
                self.assertEqual(matched, expected)
                self.assertLessEqual(len(record["excerpt"]), 4096)
                self.assertEqual(record["bytes"], len(output))


class CliTests(unittest.TestCase):
    def run_preflight(self, load, pin_code=0):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            pins, result = root / "pins.json", root / "result.json"
            pins.write_text(json.dumps(LIMITS))
            argv = ["bench.py", "fixture", "--preflight", "--toolchain", str(pins), "--json", str(result)]
            with patch.object(sys, "argv", argv), patch.object(bench.os, "getloadavg", return_value=(load, 1, 1)), \
                    patch.object(bench.subprocess, "run", return_value=subprocess.CompletedProcess([], pin_code, "pins", "")), \
                    patch.object(bench, "run_leg", side_effect=AssertionError("preflight must not run a timed leg")), \
                    contextlib.redirect_stdout(io.StringIO()):
                code = bench.main()
            return code, json.loads(result.read_text())

    def test_preflight_busy_is_not_a_completed_load_wait_or_r2_verdict(self):
        code, report = self.run_preflight(9)
        self.assertEqual((code, report["status"]), (3, "NOT_READY"))
        self.assertEqual(report["samples"], [])
        self.assertNotIn("summary", report)

    def test_preflight_ready_has_no_benchmark_samples(self):
        code, report = self.run_preflight(8)
        self.assertEqual((code, report["status"]), (0, "READY"))
        self.assertEqual(report["samples"], [])

    def test_pin_failure_stops_preflight(self):
        code, report = self.run_preflight(1, pin_code=1)
        self.assertEqual((code, report["status"]), (1, "ERROR"))
        self.assertEqual(report["pin_check"]["exit_code"], 1)

    def test_argv_mode_runs_without_shell(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            pins, result = root / "pins.json", root / "result.json"
            pins.write_text(json.dumps({**LIMITS, "load_ceiling": 1000}))
            argv = ["bench.py", "fixture", "/usr/bin/true", "--argv", "--runs", "1",
                    "--toolchain", str(pins), "--json", str(result)]
            with patch.object(sys, "argv", argv), \
                    patch.object(bench.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "pins", "")), \
                    contextlib.redirect_stdout(io.StringIO()):
                code = bench.main()
            report = json.loads(result.read_text())
        self.assertEqual((code, report["status"]), (0, "PASS"))
        self.assertEqual(report["command"], ["/usr/bin/true"])
        self.assertEqual(report["launcher"], "direct")


if __name__ == "__main__":
    unittest.main(verbosity=2)
