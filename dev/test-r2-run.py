#!/usr/bin/env python3
"""Exercise collection against the real evidence validator with synthetic child reports."""

import argparse
import contextlib
import copy
import importlib.util
import io
import json
import os
from pathlib import Path
import shlex
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch


def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(filename))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


runner = module("sole_comb_runner", "r2-run.py")
evidence = module("sole_comb_evidence_fixture", "test-r2-risk.py")


def bench_parser():
    # Mirrors the option shapes of bench.py main().
    parser = argparse.ArgumentParser(prog="bench.py")
    parser.add_argument("name")
    parser.add_argument("command", nargs="?")
    parser.add_argument("--argv", action="store_true")
    parser.add_argument("--toolchain")
    parser.add_argument("--runs", type=int)
    parser.add_argument("--json")
    parser.add_argument("--accept-line")
    parser.add_argument("--input", action="append", default=[])
    parser.add_argument("--require-threads", action="store_true")
    parser.add_argument("--cache-state", choices=("cold", "warm", "unknown"), default="unknown")
    return parser


class CollectionTests(unittest.TestCase):
    def setUp(self):
        self.fixture = evidence.EvidenceTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root = self.fixture.root
        manifest = self.fixture.manifest
        self.plan = {"schema": 1,
                     "probe": {k: ([r["path"] for r in v] if k == "sources" else v["path"])
                               for k, v in manifest["probe"].items()},
                     "workloads": {w: {k: pair[k]["path"] for k in ("bend", "sole")}
                                   for w, pair in manifest["workloads"].items()},
                     "empty": manifest["empty"]["path"], "startup": {},
                     "native": {"unavailable": manifest["native"]["unavailable"]["path"]}}
        for endpoint, entry in manifest["startup"].items():
            self.plan["startup"][endpoint] = {"origin": entry["origin"]["path"], "input": entry["input"]["path"],
                                               "arguments": ["check"], "accept_line": "ACCEPT", "cache_state": "cold"}
        self.plan_path = self.root / "run-plan.json"
        self.output = self.root / "collection"
        self.calls = []
        self.cpus = {"bun": 60, "node-worker": 80, "native": 1}
        self.change = lambda name, report: None
        self.after = lambda name: None
        self.return_code = lambda report: {"PASS": 0, "ERROR": 1, "UNMET": 3, "INTERRUPTED": 130}[report["status"]]

    def write_plan(self):
        self.plan_path.write_text(json.dumps(self.plan))

    def fake_child(self, args, **kwargs):
        if Path(args[2]).name == "pin-check.py":
            return subprocess.CompletedProcess(args, 0, "pins", "")
        self.assertEqual(Path(args[2]).name, "bench.py")
        self.assertFalse(kwargs.get("shell", False))
        # Parse as bench.py main() does, so the fake rejects exactly what the real harness rejects.
        options = bench_parser().parse_args(args[3:])
        self.assertEqual(options.runs, 5)
        self.assertTrue(options.require_threads and options.argv)
        name = options.name
        self.calls.append(name)
        argv = shlex.split(options.command)
        inputs = options.input
        output = Path(options.json)
        accept = options.accept_line
        cache = options.cache_state
        cpu = 100 if "denominator" in name else next((v for k, v in self.cpus.items() if k in name), 5)
        if name.startswith("startup") or name.endswith("empty"):
            cpu = 5
        ref = self.fixture.leg(argv, cpu, inputs, accept, cache)
        report = json.loads(Path(ref["path"]).read_text())
        self.change(name, report)
        output.write_text(json.dumps(report))
        self.after(name)
        return subprocess.CompletedProcess(args, self.return_code(report))

    def run_cli(self, *extra):
        self.write_plan()
        with patch.object(runner, "run_benchmark", side_effect=self.fake_child), \
                patch.object(runner.r2.subprocess, "run", side_effect=self.fake_child), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            code = runner.main(["--plan", str(self.plan_path), "--toolchain", str(self.fixture.pins_path),
                                "--output", str(self.output), *extra])
        journal = self.output / "collection.json"
        return code, json.loads(journal.read_text()) if journal.exists() else None

    def result(self):
        return json.loads((self.output / "r2-risk.json").read_text())

    def test_prepare_copies_startup_and_binds_imports_without_running_children(self):
        dependency = self.root / "dependency.bend"
        dependency.write_text("import Base\n")
        Path(self.plan["workloads"]["small"]["bend"]).write_text("import dependency.bend as D\n")
        code, journal = self.run_cli("--prepare-only")
        self.assertEqual((code, journal["status"], self.calls), (0, "PREPARED", []))
        manifest = json.loads((self.output / "manifest.json").read_text())
        self.assertEqual(manifest["rounds"], [])
        self.assertEqual(manifest["workloads"]["small"]["bend_imports"][0]["path"], str(dependency))
        for entry in manifest["startup"].values():
            self.assertNotEqual(entry["bundle"]["path"], entry["origin"]["path"])
            self.assertEqual(Path(entry["bundle"]["path"]).read_bytes(), Path(entry["origin"]["path"]).read_bytes())
        self.assertFalse((self.output / "r2-risk.json").exists())

    def test_amber_runs_serial_legs_and_real_assembler(self):
        before = self.fixture.pins_path.read_bytes()
        code, journal = self.run_cli()
        self.assertEqual((code, journal["status"], journal["endpoint"], journal["verdict"]), (0, "COMPLETE", "bun", "AMBER"))
        expected = ["startup-bun", "startup-node-worker", "round-1-denominator-small", "round-1-denominator-conversion"]
        expected += [f"round-1-{endpoint}-{w}" for endpoint in ("bun", "node-worker") for w in ("small", "conversion", "empty")]
        self.assertEqual(self.calls, expected)
        self.assertEqual(self.result()["rounds"][0]["decision"]["ratios"]["bun"], {"small": .6, "conversion": .6})
        self.assertEqual(self.fixture.pins_path.read_bytes(), before)

    def test_startup_accept_line_with_leading_dash_reaches_bench(self):
        self.plan["startup"]["bun"]["accept_line"] = "-ACCEPT"
        code, journal = self.run_cli()
        self.assertEqual((code, journal["status"]), (0, "COMPLETE"))
        report = json.loads(Path(journal["legs"][0]["measurement"]).read_text())
        self.assertEqual((journal["legs"][0]["name"], report["accept_line"]), ("startup-bun", "-ACCEPT"))

    def test_green_collects_fresh_confirmation_after_first_round(self):
        self.cpus.update(bun=30, **{"node-worker": 40})
        code, journal = self.run_cli()
        self.assertEqual((code, journal["verdict"]), (0, "GREEN"))
        self.assertEqual(len(self.calls), 18)
        self.assertEqual(self.calls[10], "round-2-denominator-small")
        self.assertEqual(len(self.result()["rounds"]), 2)
        self.assertTrue(self.result()["confirmation_required"])

    def test_fail_is_complete_with_exit_four(self):
        self.cpus.update(bun=150, **{"node-worker": 180})
        code, journal = self.run_cli()
        self.assertEqual((code, journal["status"], journal["verdict"]), (4, "COMPLETE", "FAIL"))
        self.assertEqual(len(self.calls), 10)

    def test_confirmation_can_degrade_green(self):
        self.cpus.update(bun=30, **{"node-worker": 80})
        def change(name, report):
            if name.startswith("round-2-bun") and not name.endswith("empty"):
                for sample in report["samples"]:
                    sample.update(cpu_ms=70, user_cpu_ms=65, system_cpu_ms=5)
        self.change = change
        self.assertEqual(self.run_cli()[0], 0)
        self.assertEqual(self.result()["verdict"], "AMBER")

    def test_confirmation_endpoint_switch_is_unmet(self):
        self.cpus.update(bun=30, **{"node-worker": 40})
        def change(name, report):
            if name.startswith("round-2-bun") and not name.endswith("empty"):
                for sample in report["samples"]:
                    sample.update(cpu_ms=70, user_cpu_ms=65, system_cpu_ms=5)
        self.change = change
        code, journal = self.run_cli()
        self.assertEqual((code, journal["status"], journal["endpoint"]), (3, "UNMET", None))
        self.assertIn("fastest endpoint changed", journal["reason"])

    def test_disqualified_endpoint_never_drives_the_confirmation_round(self):
        # A GREEN disqualified endpoint must not trigger a second round when the eligible one is AMBER.
        self.cpus.update(bun=60, **{"node-worker": 30})
        original = runner.Collection.prepare

        def prepared(collection):
            original(collection)
            collection.manifest["disqualified"] = ["node-worker"]
        with patch.object(runner.Collection, "prepare", prepared):
            code, journal = self.run_cli()
        self.assertEqual((code, journal["status"], journal["endpoint"], journal["verdict"]),
                         (0, "COMPLETE", "bun", "AMBER"))
        self.assertEqual(len(self.calls), 10)
        self.assertEqual(self.result()["rounds"][0]["decision"]["dropped"], ["node-worker"])

    def test_native_remains_informational(self):
        binary = self.fixture.artifact("probe-native", "native")
        self.plan["native"] = {"binary": binary["path"], "build_log": self.plan["probe"]["build_log"]}
        code, journal = self.run_cli()
        self.assertEqual((code, journal["endpoint"]), (0, "bun"))
        self.assertEqual(self.calls[2:5], ["native-small", "native-conversion", "native-empty"])
        self.assertTrue(self.result()["native"]["info_only"])

    def native_unmet(self, leg, reason):
        binary = self.fixture.artifact("probe-native", "native")
        self.plan["native"] = {"binary": binary["path"], "build_log": self.plan["probe"]["build_log"]}
        def change(name, report):
            if name == f"native-{leg}":
                report.update(status="UNMET", reason=reason, samples=report["samples"][:1])
                report["samples"][0].update(accepted=False, threads_peak=None,
                                            timed_out=reason == "child deadline exceeded")
        self.change = change
        return self.run_cli()

    def test_native_unmet_is_recorded_and_collection_continues(self):
        for leg, reason in (("empty", "child thread count unavailable"), ("conversion", "child deadline exceeded")):
            with self.subTest(leg=leg):
                self.setUp()
                code, journal = self.native_unmet(leg, reason)
                self.assertEqual((code, journal["status"], journal["endpoint"]), (0, "COMPLETE", "bun"))
                self.assertEqual(self.result()["native"]["measurements"][leg]["status"], "UNMET")
                self.assertIn("round-1-denominator-small", self.calls)

    def test_native_load_unmet_still_stops(self):
        code, journal = self.native_unmet("empty", "load wait exhausted")
        self.assertEqual((code, journal["status"], journal["endpoint"]), (3, "UNMET", None))
        self.assertNotIn("round-1-denominator-small", self.calls)

    def test_unmet_stops_immediately_and_keeps_raw_report(self):
        def change(name, report):
            report.update(status="UNMET", samples=[], reason="load wait exhausted")
        self.change = change
        code, journal = self.run_cli()
        self.assertEqual((code, journal["status"], len(self.calls)), (3, "UNMET", 1))
        self.assertTrue((self.output / "measurements/startup-bun.json").exists())
        self.assertFalse((self.output / "r2-risk.json").exists())

    def test_only_child_failures_can_drop_a_candidate(self):
        def change(name, report):
            if name == "round-1-bun-small":
                report.update(status="ERROR", reason=runner.r2.CHILD_FAILURE, samples=report["samples"][:1])
                report["samples"][0].update(accepted=False, exit_code=1, accept_line_found=False)
        self.change = change
        code, journal = self.run_cli()
        self.assertEqual((code, journal["endpoint"]), (0, "node-worker"))

    def test_harness_error_is_not_an_endpoint_rejection(self):
        def change(name, report):
            if name == "round-1-bun-small":
                report.update(status="ERROR", reason="observer failed")
        self.change = change
        code, journal = self.run_cli()
        self.assertEqual((code, journal["status"], len(self.calls)), (1, "ERROR", 5))
        self.assertFalse((self.output / "r2-risk.json").exists())

    def test_mutation_between_legs_is_refused(self):
        self.after = lambda name: Path(self.plan["probe"]["sources"][0]).write_text("changed")
        code, journal = self.run_cli()
        self.assertEqual((code, journal["status"], len(self.calls)), (1, "ERROR", 1))
        self.assertIn("input changed", journal["reason"])
        self.assertEqual(journal["legs"][-1]["status"], "ERROR")

    def test_missing_report_marks_leg_error(self):
        child = self.fake_child

        def no_report(args, **kwargs):
            if Path(args[2]).name == "bench.py" and args[3] == "startup-bun":
                self.calls.append(args[3])
                return subprocess.CompletedProcess(args, 2)
            return child(args, **kwargs)
        self.fake_child = no_report
        code, journal = self.run_cli()
        self.assertEqual((code, journal["status"], journal["legs"][-1]["status"]), (1, "ERROR", "ERROR"))
        self.assertIn("startup-bun.json", journal["legs"][-1]["reason"])

    def test_pin_mutation_is_refused(self):
        self.after = lambda name: self.fixture.pins_path.write_text("{}")
        code, journal = self.run_cli()
        self.assertEqual((code, journal["status"], len(self.calls)), (1, "ERROR", 1))

    def test_exit_code_must_match_report(self):
        self.return_code = lambda report: 1
        code, journal = self.run_cli()
        self.assertEqual((code, journal["status"], len(self.calls)), (1, "ERROR", 1))

    def test_interruption_preserves_attempt(self):
        self.after = lambda name: (_ for _ in ()).throw(KeyboardInterrupt())
        code, journal = self.run_cli()
        self.assertEqual((code, journal["status"], len(self.calls)), (130, "INTERRUPTED", 1))
        self.assertTrue((self.output / "measurements/startup-bun.json").exists())

    def test_harness_interruption_report_stops_collection(self):
        self.change = lambda name, report: report.update(status="INTERRUPTED", samples=[],
                                                         reason="cancelled by operator")
        code, journal = self.run_cli()
        self.assertEqual((code, journal["status"], len(self.calls)), (130, "INTERRUPTED", 1))
        self.assertTrue((self.output / "measurements/startup-bun.json").exists())
        self.assertFalse((self.output / "r2-risk.json").exists())

    def test_harness_runs_outside_the_collector_process_group(self):
        # A terminal Ctrl-C reaches the foreground group only; the collector forwards one SIGINT itself.
        marker = self.root / "harness-group.txt"
        code = "import os,sys; from pathlib import Path; Path(sys.argv[1]).write_text(str(os.getpgrp()))"
        self.assertEqual(runner.run_benchmark([sys.executable, "-c", code, str(marker)]).returncode, 0)
        self.assertNotEqual(int(marker.read_text()), os.getpgrp())

    def test_existing_output_is_never_reused(self):
        self.output.mkdir()
        sentinel = self.output / "collection.json"
        sentinel.write_text('{"existing":true}')
        code, _ = self.run_cli()
        self.assertEqual((code, self.calls, sentinel.read_text()), (1, [], '{"existing":true}'))

    def test_no_writes_in_prior_art_checkout(self):
        self.output = self.root / "attest" / "new-run"
        code, _ = self.run_cli()
        self.assertEqual((code, self.calls, self.output.exists()), (1, [], False))

    def test_no_writes_in_project_tree(self):
        (self.root / "repo").mkdir()
        self.output = self.root / "repo" / "run"
        with patch.object(runner, "ROOT", self.root / "repo"):
            code, _ = self.run_cli()
        self.assertEqual((code, self.calls, self.output.exists()), (1, [], False))

    def test_no_writes_in_assay_checkout(self):
        self.output = self.root / "assay" / "run"
        code, _ = self.run_cli()
        self.assertEqual((code, self.calls, self.output.exists()), (1, [], False))

    def test_no_writes_in_tool_checkouts(self):
        pins = json.loads(self.fixture.pins_path.read_text())
        pins["kanon"] = {"checkout": str(self.root / "kanon")}
        pins["bend"]["checkout"] = str(self.root / "bendco")
        self.fixture.pins_path.write_text(json.dumps(pins))
        for checkout in ("kanon", "bendco"):
            with self.subTest(checkout=checkout):
                self.calls = []
                self.output = self.root / checkout / "run"
                code, _ = self.run_cli()
                self.assertEqual((code, self.calls, self.output.exists()), (1, [], False))

    def test_probe_sources_must_be_outside_project(self):
        scratch = tempfile.TemporaryDirectory()
        self.addCleanup(scratch.cleanup)
        self.output = Path(scratch.name) / "run"
        with patch.object(runner, "ROOT", self.root):
            code, _ = self.run_cli()
        self.assertEqual((code, self.calls, self.output.exists()), (1, [], False))

    def test_relative_paths_and_shell_metacharacters_are_literal(self):
        source = self.root / "a 'quoted' $(touch nope).sole"
        source.write_text("sole twin")
        self.plan["workloads"]["small"]["sole"] = source.name
        self.plan["startup"]["bun"]["arguments"] = ["check", "a b", "$(touch nope)"]
        code, _ = self.run_cli()
        self.assertEqual(code, 0)
        result = self.result()
        self.assertEqual(result["rounds"][0]["endpoints"]["bun"]["small"]["evidence"]["command"][-1], str(source))
        self.assertEqual(result["startup"]["bun"]["evidence"]["command"][-3:-1], ["a b", "$(touch nope)"])

    def test_invalid_plan_is_refused_before_any_write_or_measurement(self):
        original = copy.deepcopy(self.plan)
        cases = [lambda p: p.update(schema=True), lambda p: p.update(extra=1),
                 lambda p: p.update(disqualified=["node-worker"]),
                 lambda p: p["startup"]["bun"].update(cache_state="unknown"),
                 lambda p: p["startup"]["bun"].update(accept_line="ACCEPT\n"),
                 lambda p: p["workloads"]["conversion"].update(sole=p["workloads"]["small"]["sole"]),
                 lambda p: p["probe"].update(sources=p["probe"]["sources"] * 2),
                 lambda p: p.update(empty=p["workloads"]["small"]["sole"])]
        for change in cases:
            with self.subTest(change=change):
                self.plan = copy.deepcopy(original)
                change(self.plan)
                code, _ = self.run_cli()
                self.assertEqual((code, self.calls, self.output.exists()), (1, [], False))

    def test_probe_import_must_be_listed(self):
        Path(self.plan["probe"]["sources"][0]).write_text("import hidden.bend as H\n")
        (self.root / "hidden.bend").write_text("import Base\n")
        code, _ = self.run_cli()
        self.assertEqual((code, self.calls, self.output.exists()), (1, [], False))

    def test_copy_failure_preserves_preparation_error(self):
        with patch.object(runner.shutil, "copyfile", side_effect=OSError("copy failed")):
            code, journal = self.run_cli()
        self.assertEqual((code, journal["status"], journal["reason"], self.calls), (1, "ERROR", "copy failed", []))

    def test_interrupt_reaches_harness_and_reaps_measured_child(self):
        # Signal only the collector, as an API cancellation can do. The real bench
        # measurement owns a separate process group and must get its cleanup turn.
        worker = self.root / "cancel-worker.py"
        pidfile = self.root / "measured.pid"
        childcode = "import os,time; from pathlib import Path; Path(" + repr(str(pidfile)) + ").write_text(str(os.getpid())); time.sleep(60)"
        worker.write_text(
            "import importlib.util,sys\n"
            "spec=importlib.util.spec_from_file_location('bench', " + repr(str(runner.ROOT / "dev/bench.py")) + ")\n"
            "bench=importlib.util.module_from_spec(spec); spec.loader.exec_module(bench)\n"
            "try: bench.measure([sys.executable, '-c', " + repr(childcode) + "], 60)\n"
            "except KeyboardInterrupt: sys.exit(130)\n")
        collector = self.root / "cancel-collector.py"
        collector.write_text(
            "import importlib.util,sys\n"
            "spec=importlib.util.spec_from_file_location('runner', " + repr(str(runner.ROOT / "dev/r2-run.py")) + ")\n"
            "runner=importlib.util.module_from_spec(spec); spec.loader.exec_module(runner)\n"
            "try: runner.run_benchmark([sys.executable, " + repr(str(worker)) + "])\n"
            "except KeyboardInterrupt: sys.exit(130)\n")
        with subprocess.Popen([sys.executable, str(collector)], start_new_session=True,
                              stdout=subprocess.DEVNULL, stderr=subprocess.PIPE) as process:
            try:
                deadline = time.monotonic() + 10
                while not pidfile.exists() and time.monotonic() < deadline and process.poll() is None:
                    time.sleep(.02)
                self.assertTrue(pidfile.exists(), "measured child did not start")
                measured = int(pidfile.read_text())
                process.send_signal(signal.SIGINT)
                _, errors = process.communicate(timeout=10)
                self.assertEqual(process.returncode, 130, errors.decode())
                with self.assertRaises(ProcessLookupError):
                    os.kill(measured, 0)
            finally:
                if process.poll() is None:
                    process.kill()
                if pidfile.exists():
                    try:
                        os.killpg(int(pidfile.read_text()), signal.SIGKILL)
                    except ProcessLookupError:
                        pass


if __name__ == "__main__":
    unittest.main()
