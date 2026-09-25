#!/usr/bin/env python3
"""Check preparation failure paths and its handoff to the real collector."""

import contextlib
import copy
import errno
import importlib.util
import io
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import threading
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch


def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(filename))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


prepare = module("sole_comb_preparation", "r2-prepare.py")
collection_tests = module("sole_comb_collection_fixtures", "test-r2-run.py")


class PreparationTests(unittest.TestCase):
    def setUp(self):
        fixture = collection_tests.CollectionTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        self.root = fixture.root
        self.pins_path = fixture.fixture.pins_path
        pins = copy.deepcopy(fixture.fixture.pins)
        pins["build_deadline_s"] = 1200
        self.pins_path.write_text(json.dumps(pins))
        self.plan = copy.deepcopy(fixture.plan)
        self.entry = Path(self.plan["probe"]["sources"][0])
        self.plan["probe"] = {"entry": str(self.entry)}
        self.plan.pop("native")
        self.plan["rejects"] = {}
        for name in prepare.REJECTIONS:
            path = self.root / f"reject-{name}.sole"
            path.write_text(f"invalid {name}\n")
            self.plan["rejects"][name] = str(path)
        self.plan_path = self.root / "prepare-plan.json"
        self.output = self.root / "prepared"
        self.calls = []
        self.argvs = {}
        self.failures = {}
        # The default checker answers from the input it was given, never from the log name.
        self.checker = lambda text: ("REJECT\n", 1) if text.startswith("invalid") else ("ACCEPT\n", 0)
        self.after = lambda name: None
        self.skip_output = False

    def expected_call(self, name, argv, deadline):
        """Check the exact command and deadline of one step, then answer as a correct child would."""
        pins = json.loads(self.pins_path.read_text())
        resolved = lambda path: str(Path(path).resolve())
        endpoint = next((e for e in (*prepare.r2.CANDIDATES, "native") if name.startswith(f"{e}-")), None)
        if endpoint is not None:
            case = name[len(endpoint) + 1:]
            source = (self.plan["empty"] if case == "empty" else
                      self.plan["rejects"][case[len("reject-"):]] if case.startswith("reject-") else
                      self.plan["workloads"][case]["sole"])
            bundle = self.output / ("probe-native.exe" if endpoint == "native" else "probe.js")
            self.assertEqual(argv, prepare.r2.command(pins, endpoint, str(bundle), [resolved(source)]))
            self.assertEqual(deadline, pins["leg_deadline_s"])
            return self.checker(Path(argv[-1]).read_text())
        if name.startswith("startup-"):
            entry = self.plan["startup"][name[len("startup-"):]]
            copy_path = self.output / "startup" / Path(entry["origin"]).name
            self.assertEqual(argv, prepare.r2.command(pins, name[len("startup-"):], str(copy_path),
                                                      [*entry["arguments"], resolved(entry["input"])]))
            self.assertEqual(deadline, pins["leg_deadline_s"])
            return entry["accept_line"] + "\n", 0
        if name.startswith("denominator-"):
            source = self.plan["workloads"][name[len("denominator-"):]]["bend"]
            self.assertEqual(argv, [pins["bend"]["binary"], resolved(source), "--check-only"])
            self.assertEqual(deadline, pins["leg_deadline_s"])
            return "All terms check.\n", 0
        self.assertTrue(name.startswith(("build-", "pins-")), name)
        self.assertEqual(deadline, pins["build_deadline_s"])
        return "ACCEPT\n", 0

    def fake_execute(self, argv, cwd, env, deadline, stdout, stderr):
        name = stdout.stem
        self.calls.append(name)
        self.argvs[name] = argv
        self.assertEqual(cwd, self.output)
        self.assertEqual(env["BEND_NO_TELEMETRY"], "1")
        self.assertEqual(env["BEND_LIB"], str(self.output / "bend-cache"))
        content, code = self.expected_call(name, argv, deadline)
        failure = self.failures.get(name)
        if isinstance(failure, tuple):
            code, content = failure
        stdout.write_text(content)
        stderr.write_text(f"stderr for {name}\n")
        if name.startswith("build-") and code == 0 and not self.skip_output:
            artifact = Path(argv[argv.index("-o") + 1])
            artifact.write_text("built probe\n")
            if name == "build-native":
                artifact.chmod(0o755)
        self.after(name)
        if isinstance(failure, BaseException):
            raise failure
        return code

    def run_prepare(self):
        self.plan_path.write_text(json.dumps(self.plan))
        with patch.object(prepare, "execute", self.fake_execute), contextlib.redirect_stdout(io.StringIO()), \
                contextlib.redirect_stderr(io.StringIO()):
            return prepare.main(["--plan", str(self.plan_path), "--toolchain", str(self.pins_path),
                                 "--output", str(self.output)])

    def record(self):
        return json.loads((self.output / "preparation.json").read_text())

    def collector(self):
        result = prepare.runner.Collection(self.output / "run-plan.json", self.pins_path, self.root / "measurement")
        result.prepare()
        return result

    def test_ready_plan_passes_collector_and_binds_all_inputs(self):
        self.assertEqual(self.run_prepare(), 0)
        record = self.record()
        self.assertEqual(record["status"], "READY")
        self.assertIsNone(record["endpoint"])
        self.assertIsNone(record["verdict"])
        self.assertEqual(record["qualified_candidates"], {"bun": True, "node-worker": True})
        self.assertEqual(record["disqualified"], [])
        self.assertTrue(record["native_qualified"])
        self.assertEqual(self.calls[0], "pins-before")
        self.assertEqual(self.calls[-1], "pins-after")
        self.assertEqual(sum("-reject-" in n for n in self.calls), 12)
        rejects = {str(Path(p).resolve()) for p in self.plan["rejects"].values()}
        for endpoint in (*prepare.r2.CANDIDATES, "native"):
            with self.subTest(endpoint=endpoint):
                self.assertEqual({self.argvs[f"{endpoint}-reject-{n}"][-1] for n in prepare.REJECTIONS}, rejects)
        collector = self.collector()
        collector.verify()
        self.assertEqual(collector.manifest["rounds"], [])
        self.assertIn(self.plan["rejects"]["type"], collector.files)
        self.assertEqual(collector.manifest["native"]["binary"]["path"], str(self.output / "probe-native.exe"))

    def test_build_log_combines_stdout_and_stderr(self):
        self.assertEqual(self.run_prepare(), 0)
        plan = json.loads((self.output / "run-plan.json").read_text())
        for key in ("probe", "native"):
            with self.subTest(key=key):
                log = Path(plan[key]["build_log"]).read_text()
                self.assertIn("ACCEPT", log)
                self.assertIn("stderr for build-", log)
        self.collector().verify()

    def test_native_build_failure_is_informational_with_both_logs(self):
        self.failures["build-native"] = (1, "native compile failed\n")
        self.assertEqual(self.run_prepare(), 0)
        collector = self.collector()
        native = json.loads(Path(collector.manifest["native"]["unavailable"]["path"]).read_text())
        self.assertEqual(native["build"]["exit_code"], 1)
        self.assertIn("stderr", native["build"])
        self.assertNotIn("native-small", self.calls)

    def test_native_deadline_is_informational(self):
        self.failures["build-native"] = subprocess.TimeoutExpired(["compiler"], 1200)
        self.assertEqual(self.run_prepare(), 0)
        self.assertEqual(self.record()["steps"][2]["status"], "UNMET")
        self.assertIn("unavailable", self.collector().manifest["native"])

    def test_native_verdict_failure_is_not_a_candidate(self):
        self.failures["native-conversion"] = (1, "REJECT\n")
        self.assertEqual(self.run_prepare(), 0)
        self.assertFalse(self.record()["native_qualified"])
        self.assertIn("unavailable", self.collector().manifest["native"])

    def test_javascript_failure_stops_and_preserves_logs(self):
        self.failures["build-javascript"] = (1, "compile failed\n")
        self.assertEqual(self.run_prepare(), 1)
        self.assertEqual(self.calls, ["pins-before", "build-javascript"])
        self.assertFalse((self.output / "run-plan.json").exists())
        self.assertEqual(self.record()["status"], "FAILED")
        self.assertIn("stderr", self.record()["steps"][-1])

    def test_missing_build_artifact_is_error(self):
        self.skip_output = True
        self.assertEqual(self.run_prepare(), 1)
        self.assertEqual(self.record()["steps"][-1]["status"], "ERROR")

    def test_build_deadline_returns_unmet(self):
        self.failures["build-javascript"] = subprocess.TimeoutExpired(["compiler"], 1200)
        self.assertEqual(self.run_prepare(), 3)
        self.assertEqual(self.record()["status"], "UNMET")
        self.assertFalse((self.output / "run-plan.json").exists())

    def test_pin_failure_stops_before_build(self):
        self.failures["pins-before"] = (1, "pin drift\n")
        self.assertEqual(self.run_prepare(), 1)
        self.assertEqual(self.calls, ["pins-before"])

    def test_pin_failure_after_checks_does_not_publish(self):
        self.failures["pins-after"] = (1, "pin drift\n")
        self.assertEqual(self.run_prepare(), 1)
        self.assertFalse((self.output / "run-plan.json").exists())

    def test_denominator_requires_exact_acceptance_line(self):
        self.failures["denominator-small"] = (0, "prefix All terms check.\n")
        self.assertEqual(self.run_prepare(), 1)
        self.assertNotIn("bun-small", self.calls)

    def test_rejection_requires_exit_one_exact_line_and_no_accept(self):
        for case in ((0, "REJECT\n"), (1, "ACCEPT\nREJECT\n"), (-signal.SIGSEGV, "REJECT\n"),
                     (2, "REJECT\n"), (1, "not REJECT\n"), (1, "")):
            with self.subTest(case=case):
                self.output = self.root / f"prepared-{len(self.calls)}"
                self.failures["bun-reject-type"] = case
                self.assertEqual(self.run_prepare(), 0)
                self.assertEqual(self.record()["qualified_candidates"], {"bun": False, "node-worker": True})
                self.assertEqual(self.record()["disqualified"], ["bun"])

    def test_always_accept_probe_cannot_qualify(self):
        for endpoint in prepare.r2.CANDIDATES:
            for name in prepare.REJECTIONS:
                self.failures[f"{endpoint}-reject-{name}"] = (0, "ACCEPT\n")
        self.assertEqual(self.run_prepare(), 1)
        self.assertFalse(any(self.record()["qualified_candidates"].values()))

    def test_qualification_checks_argv_inputs_and_exit_edges(self):
        default = self.checker
        lex_only = lambda text: ("REJECT\n", 1) if text == "invalid lex\n" else ("ACCEPT\n", 0)
        cases = {"lex-only checker": lambda: setattr(self, "checker", lex_only),
                 "positive exit one": lambda: self.failures.update(
                     {f"{e}-small": (1, "ACCEPT\n") for e in prepare.r2.CANDIDATES}),
                 "denominator exit one": lambda: self.failures.update({"denominator-small": (1, "All terms check.\n")}),
                 "startup without its acceptance line": lambda: self.failures.update({"startup-bun": (0, "usage\n")})}
        for label, arrange in cases.items():
            with self.subTest(case=label):
                self.output = self.root / f"edges-{label.replace(' ', '-')}"
                self.failures, self.checker = {}, default
                arrange()
                self.assertEqual(self.run_prepare(), 1)
                self.assertFalse((self.output / "run-plan.json").exists())

    def test_ambiguous_positive_output_is_rejected(self):
        self.failures["bun-small"] = (0, "REJECT\nACCEPT\n")
        self.assertEqual(self.run_prepare(), 0)
        self.assertEqual(self.record()["qualified_candidates"], {"bun": False, "node-worker": True})
        self.assertEqual(self.record()["disqualified"], ["bun"])

    def test_candidate_harness_error_stops_without_plan(self):
        # A harness error is not a failed check: it must not drop the endpoint and publish the other one.
        error = {"bun-small": OSError("spawn failed")}
        for label, extra in (("other qualifies", {}),
                             ("other deadline", {"node-worker-small": subprocess.TimeoutExpired(["node"], 600)})):
            with self.subTest(case=label):
                self.output = self.root / f"harness-{label.replace(' ', '-')}"
                self.failures = {**error, **extra}
                self.assertEqual(self.run_prepare(), 1)
                self.assertEqual(self.record()["status"], "ERROR")
                self.assertIn("bun-small", self.record()["reason"])
                self.assertFalse((self.output / "run-plan.json").exists())
                self.assertFalse((self.output / "run-plan.pending.json").exists())

    def test_native_harness_error_is_informational(self):
        self.failures["native-small"] = OSError("spawn failed")
        self.assertEqual(self.run_prepare(), 0)
        self.assertFalse(self.record()["native_qualified"])

    def test_candidate_deadline_is_unmet(self):
        self.failures["bun-small"] = subprocess.TimeoutExpired(["bun"], 600)
        self.assertEqual(self.run_prepare(), 3)
        self.assertEqual(self.record()["status"], "UNMET")

    def test_candidate_failure_outranks_deadline(self):
        deadline = {"bun-small": subprocess.TimeoutExpired(["bun"], 600)}
        # A failure outranks a deadline on the same endpoint only; the other endpoint decides the outcome.
        for failed, code, status in ((["bun-reject-lex"], 0, "READY"),
                                     (["node-worker-reject-type"], 3, "UNMET"),
                                     (["bun-reject-lex", "node-worker-reject-type"], 1, "ERROR")):
            with self.subTest(failed=failed):
                self.output = self.root / f"outranks-{'-'.join(failed)}"
                self.failures = {**deadline, **{name: (0, "ACCEPT\n") for name in failed}}
                self.assertEqual(self.run_prepare(), code)
                self.assertEqual(self.record()["status"], status)
                self.assertEqual((self.output / "run-plan.json").exists(), code == 0)
                if code == 1:
                    self.assertTrue(all(name in self.record()["reason"] for name in failed))
                if code == 0:
                    self.assertEqual(self.record()["disqualified"], ["bun"])

    def test_one_failed_candidate_is_recorded_and_dropped(self):
        self.failures["node-worker-reject-type"] = (0, "ACCEPT\n")
        self.assertEqual(self.run_prepare(), 0)
        record = self.record()
        self.assertEqual(record["status"], "READY")
        self.assertEqual(record["qualified_candidates"], {"bun": True, "node-worker": False})
        self.assertEqual(record["disqualified"], ["node-worker"])
        plan = json.loads((self.output / "run-plan.json").read_text())
        self.assertEqual(plan["disqualified"], ["node-worker"])
        self.assertEqual(self.collector().manifest["disqualified"], ["node-worker"])

    def test_startup_bundle_is_copied_before_checking(self):
        def inspect(name):
            if name == "startup-bun":
                copy_path = self.output / "startup/attest.js"
                self.assertEqual(copy_path.read_bytes(), Path(self.plan["startup"]["bun"]["origin"]).read_bytes())
        self.after = inspect
        self.assertEqual(self.run_prepare(), 0)

    def test_startup_failure_stops_publication(self):
        self.failures["startup-node-worker"] = (1, "bad startup\n")
        self.assertEqual(self.run_prepare(), 1)
        self.assertFalse((self.output / "run-plan.json").exists())

    def test_source_change_during_build_stops_publication(self):
        self.after = lambda name: self.entry.write_text("changed\n") if name == "build-javascript" else None
        self.assertEqual(self.run_prepare(), 1)
        self.assertNotIn("build-native", self.calls)

    def test_transitive_foreign_import_is_bound(self):
        imported, foreign = self.root / "helper.bend", self.root / "helper.js"
        self.entry.write_text("import helper.bend as Helper\n")
        imported.write_text('import "helper.js"\n')
        foreign.write_text("original\n")
        self.after = lambda name: foreign.write_text("changed\n") if name == "build-javascript" else None
        self.assertEqual(self.run_prepare(), 1)
        self.assertNotIn("build-native", self.calls)

    def test_invalid_inputs_fail_without_creating_output(self):
        original = copy.deepcopy(self.plan)
        mutations = [lambda p: p.update(schema=True),
                     lambda p: p["probe"].update(entry=p["empty"]),
                     lambda p: p["workloads"].update(conversion=p["workloads"]["small"]),
                     lambda p: p.update(empty=p["workloads"]["small"]["sole"]),
                     lambda p: p["rejects"].update(type=p["rejects"]["lex"]),
                     lambda p: p["rejects"].pop("conversion"),
                     lambda p: p["startup"]["bun"].update(origin=p["probe"]["entry"]),
                     lambda p: p["startup"]["bun"].update(accept_line="ACCEPT\nextra")]
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                self.plan = copy.deepcopy(original)
                mutate(self.plan)
                self.assertEqual(self.run_prepare(), 1)
                self.assertFalse(self.output.exists())
                self.assertEqual(self.calls, [])

    def test_existing_output_is_never_overwritten(self):
        self.output.mkdir()
        sentinel = self.output / "keep"
        sentinel.write_text("keep")
        self.assertEqual(self.run_prepare(), 1)
        self.assertEqual(sentinel.read_text(), "keep")
        self.assertEqual(self.calls, [])

    def test_protected_output_is_refused(self):
        for root in (prepare.ROOT, self.root / "attest", self.root / "assay"):
            with self.subTest(root=root):
                self.output = root / "forbidden-preparation-output"
                self.assertEqual(self.run_prepare(), 1)
                self.assertFalse(self.output.exists())
                self.assertEqual(self.calls, [])

    def test_signal_cancellation_keeps_journal_without_ready_plan(self):
        for signum in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            with self.subTest(signum=signum):
                self.output = self.root / f"cancel-{signum}"
                self.failures["build-javascript"] = prepare.Interrupted(signum)
                self.assertEqual(self.run_prepare(), 128 + signum)
                self.assertEqual(self.record()["status"], "CANCELLED")
                self.assertEqual(self.record()["steps"][-1]["status"], "CANCELLED")
                self.assertFalse((self.output / "run-plan.json").exists())

    def test_collector_rejects_source_changed_since_build(self):
        self.assertEqual(self.run_prepare(), 0)
        self.entry.write_text("changed after preparation\n")
        with self.assertRaisesRegex(ValueError, "prepared file changed"):
            self.collector()
        self.assertFalse((self.root / "measurement").exists())

    def test_collector_rejects_rejection_case_changed_since_qualification(self):
        self.assertEqual(self.run_prepare(), 0)
        Path(self.plan["rejects"]["conversion"]).write_text("changed negative\n")
        with self.assertRaisesRegex(ValueError, "prepared file changed"):
            self.collector()

    def test_contract_check_plan_is_not_loadable(self):
        def refused():
            candidate = self.output / "candidate-plan.json"
            self.assertTrue(candidate.is_file())
            with self.assertRaises(ValueError):
                prepare.runner.Collection(candidate, self.pins_path, self.root / "m2").prepare()
            self.assertFalse((self.root / "m2").exists())

        self.assertEqual(self.run_prepare(), 0)
        refused()
        self.output = self.root / "prepared-late-error"
        original = prepare.runner.Collection.prepare

        def late(collection):
            original(collection)
            raise ValueError("late")
        with patch.object(prepare.runner.Collection, "prepare", late):
            self.assertEqual(self.run_prepare(), 1)
        self.assertEqual(self.record()["status"], "ERROR")
        refused()
        self.assertFalse((self.output / "run-plan.json").exists())

    def test_collector_rejects_edited_plan(self):
        self.assertEqual(self.run_prepare(), 0)
        path = self.output / "run-plan.json"
        path.write_text(path.read_text() + "\n")
        with self.assertRaisesRegex(ValueError, "does not bind this run plan"):
            self.collector()

    def test_collector_rejects_incomplete_or_failed_preparation(self):
        self.assertEqual(self.run_prepare(), 0)
        path = self.output / "preparation.json"
        original = self.record()
        mutations = [lambda r: r.update(status="ERROR"),
                     lambda r: r.update(schema=True),
                     lambda r: r["qualified_candidates"].update(bun=False),
                     lambda r: r["qualified_candidates"].update(bun=False, **{"node-worker": False}),
                     lambda r: r["qualified_candidates"].update(bun=1),
                     lambda r: r.update(disqualified=["bun"]),
                     lambda r: r.pop("disqualified"),
                     lambda r: r.update(files=[]),
                     lambda r: r.update(files=[f for f in r["files"] if f["path"] != str(self.entry)]),
                     lambda r: r["files"].append(r["files"][0])]
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                record = copy.deepcopy(original)
                mutate(record)
                path.write_text(json.dumps(record))
                with self.assertRaises(ValueError):
                    self.collector()
                self.assertFalse((self.root / "measurement").exists())


class ProcessTests(unittest.TestCase):
    def preparation(self, root):
        result = prepare.Preparation.__new__(prepare.Preparation)
        result.tracker = SimpleNamespace(verify=lambda: None, reference=lambda path: {"path": str(path)})
        result.output, result.rows, result.record, result.env = root, [], None, os.environ.copy()
        (root / "logs").mkdir()
        return result

    def test_eperm_from_zombie_group_keeps_timeout_and_cancel(self):
        real = os.killpg

        def eperm(pid, sig):
            try:
                real(pid, sig)
            except ProcessLookupError:
                pass
            raise PermissionError(errno.EPERM, "Operation not permitted")

        command = [sys.executable, "-c", "import time; time.sleep(20)"]
        with tempfile.TemporaryDirectory() as directory, patch.object(prepare.os, "killpg", eperm):
            prep = self.preparation(Path(directory))
            self.assertEqual(prep.run("deadline", command, 0.3)["status"], "UNMET")
            timer = threading.Timer(0.3, lambda: os.kill(os.getpid(), signal.SIGINT))
            with prepare.cancellation(), self.assertRaises(prepare.Interrupted):
                timer.start()
                try:
                    prep.run("cancel", command, 10)
                finally:
                    timer.cancel()
            self.assertEqual(prep.rows[-1]["status"], "CANCELLED")

    def test_signal_during_deadline_cleanup_cancels(self):
        real = os.killpg

        def signalled(pid, sig):
            os.kill(os.getpid(), signal.SIGTERM)
            real(pid, sig)

        with tempfile.TemporaryDirectory() as directory, prepare.cancellation():
            root = Path(directory)
            handler = signal.getsignal(signal.SIGINT)
            with patch.object(prepare.os, "killpg", signalled), self.assertRaises(prepare.Interrupted) as caught:
                prepare.execute(["sleep", "30"], root, os.environ.copy(), 0.3, root / "out", root / "err")
            self.assertEqual(caught.exception.signum, signal.SIGTERM)
            self.assertEqual(signal.getsignal(signal.SIGINT), handler)

    def test_signal_at_spawn_still_kills_group(self):
        base, pids = subprocess.Popen, []

        class Signalled(base):
            def __init__(self, *args, **kwargs):
                super().__init__(*args, **kwargs)
                pids.append(self.pid)
                os.kill(os.getpid(), signal.SIGTERM)

        def gone(pid):
            try:
                os.killpg(pid, 0)
            except ProcessLookupError:
                return True
            return False

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            try:
                with prepare.cancellation(), patch.object(prepare.subprocess, "Popen", Signalled), \
                        self.assertRaises(prepare.Interrupted):
                    prepare.execute(["sleep", "30"], root, os.environ.copy(), 10, root / "out", root / "err")
                deadline = time.monotonic() + 2
                while time.monotonic() < deadline and not gone(pids[0]):
                    time.sleep(0.02)
                self.assertTrue(gone(pids[0]), "process group survived a signal at spawn")
            finally:
                for pid in pids:
                    with contextlib.suppress(ProcessLookupError, PermissionError):
                        os.killpg(pid, signal.SIGKILL)
                    with contextlib.suppress(ChildProcessError):
                        os.waitpid(pid, 0)

    def test_signals_reach_cleanup_and_restore_handlers(self):
        worker = '''import importlib.util, pathlib, signal, sys
spec = importlib.util.spec_from_file_location("prep", sys.argv[1])
prep = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prep)
root = pathlib.Path(sys.argv[2])
code = "import os,time; print(os.getpid(),flush=True); time.sleep(60)"
before = {s: signal.getsignal(s) for s in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}
try:
    with prep.cancellation():
        prep.execute([sys.executable, "-c", code], root, {}, 30, root / "out", root / "err")
except prep.Interrupted as error:
    assert all(signal.getsignal(s) == handler for s, handler in before.items())
    sys.exit(128 + error.signum)
sys.exit(99)
'''
        for signum in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            with self.subTest(signum=signum), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                child_pid = None
                with subprocess.Popen([sys.executable, "-c", worker, str(Path(prepare.__file__)), directory],
                                      stdout=subprocess.DEVNULL, stderr=subprocess.PIPE) as child:
                    try:
                        deadline = time.monotonic() + 10
                        while time.monotonic() < deadline:
                            out = root / "out"
                            if out.exists() and out.read_text().strip():
                                child_pid = int(out.read_text())
                                break
                            if child.poll() is not None:
                                self.fail(child.stderr.read().decode())
                            time.sleep(0.02)
                        self.assertIsNotNone(child_pid, "child did not become ready")
                        child.send_signal(signum)
                        self.assertEqual(child.wait(timeout=5), 128 + signum)
                        with self.assertRaises(ProcessLookupError):
                            os.kill(child_pid, 0)
                    finally:
                        if child_pid is not None:
                            with contextlib.suppress(ProcessLookupError):
                                os.killpg(child_pid, signal.SIGKILL)
                        if child.poll() is None:
                            child.kill()
                        child.wait()

    def test_deadline_kills_process_group_and_preserves_logs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            heartbeat = root / "heartbeat"
            descendant = ("from pathlib import Path; import sys,time; "
                          "p=Path(sys.argv[1]); "
                          "exec('while True:\\n p.write_text(str(time.monotonic_ns()))\\n time.sleep(0.02)')")
            code = ("import os,signal,subprocess,sys,time; "
                    "signal.signal(signal.SIGTERM,signal.SIG_IGN); "
                    "child=subprocess.Popen([sys.executable,'-c',sys.argv[1],sys.argv[2]]); "
                    "print(str(os.getpid())+' '+str(child.pid),flush=True); time.sleep(60)")
            stdout, stderr = root / "out", root / "err"
            start = time.monotonic()
            with self.assertRaises(subprocess.TimeoutExpired):
                prepare.execute([sys.executable, "-c", code, descendant, str(heartbeat)],
                                root, os.environ.copy(), 1, stdout, stderr)
            self.assertLess(time.monotonic() - start, 10)
            parent, _child = map(int, stdout.read_text().split())
            with self.assertRaises(ProcessLookupError):
                os.kill(parent, 0)
            # A heartbeat checks descendant termination without ps permission,
            # and does not mistake a terminated, unreaped zombie for a live child.
            try:
                before = heartbeat.read_text()
                time.sleep(0.15)
                self.assertEqual(heartbeat.read_text(), before, "descendant survived process-group cleanup")
            finally:
                with contextlib.suppress(ProcessLookupError):
                    os.killpg(parent, signal.SIGKILL)


if __name__ == "__main__":
    unittest.main()
