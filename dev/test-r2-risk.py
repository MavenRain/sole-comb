#!/usr/bin/env python3
"""Exercise R2 decisions and refusal paths using synthetic benchmark evidence."""

import contextlib
import copy
from datetime import datetime, timedelta, timezone
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location("sole_comb_r2", Path(__file__).with_name("r2-risk.py"))
r2 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(r2)
# Literal node worker script for stack_kib 65500 (65500 / 1024 = 63.96484375), never built by r2.command.
WORKER = ("const { Worker } = require('node:worker_threads'); "
          "const worker = new Worker(require('node:path').resolve(process.argv[1]), { argv: process.argv.slice(2), execArgv: [], "
          "resourceLimits: { stackSizeMb: 63.96484375 } }); "
          "worker.on('error', error => { process.stderr.write(String(error.stack || error) + '\\n'); process.exitCode = 1; }); "
          "worker.on('exit', code => { process.exitCode = code || process.exitCode || 0; });")


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        tools = tempfile.TemporaryDirectory()
        self.addCleanup(tools.cleanup)
        binaries = {}
        for name in ("bend", "bun", "node"):
            path = Path(tools.name).resolve() / name
            path.write_text("pinned " + name)
            binaries[name] = {"path": str(path), "realpath": str(path), "version": name + " 1",
                              "sha256": r2.digest(path.read_bytes())}
        self.pins = {"bend": {"revision": "f" * 40, "version": "2.0.25", "binary": binaries["bend"]["path"],
                              "binary_sha256": binaries["bend"]["sha256"]},
                     "endpoint": None, "stack_kib": 65500,
                     "tools": {"bun": binaries["bun"], "node": binaries["node"]},
                     "prior_art": {p: {"checkout": str(self.root / p)} for p in ("attest", "assay")},
                     "load_ceiling": 8.0, "load_wait_max_s": 3600, "leg_deadline_s": 600}
        self.pins_path = self.root / "toolchain.json"
        self.pin()
        self.serial = 0
        self.manifest = {"schema": 1,
                         "probe": {"sources": [self.artifact("probe.bend", "def main() -> U32: 0")],
                                   "javascript": self.artifact("probe.js", "probe"),
                                   "build_log": self.artifact("build.log", "synthetic build evidence")},
                         "workloads": {w: {"bend": self.artifact(w + ".bend", w + " bend"),
                                           "sole": self.artifact(w + ".sole", w + " sole"),
                                           "bend_imports": []} for w in r2.WORKLOADS},
                         "empty": self.artifact("empty.sole", ""), "startup": {},
                         "native": {"unavailable": self.artifact("native.log", "synthetic native failure")},
                         "rounds": []}
        self.add_startup()
        self.add_round()

    def pin(self, **changes):
        self.pins.update(changes)
        self.pins_path.write_text(json.dumps(self.pins))
        self.pins_sha = r2.digest(self.pins_path.read_bytes())

    def expected(self, endpoint, bundle, arguments):
        tools = self.pins["tools"]
        return {"bun": [tools["bun"]["path"], bundle, *arguments],
                "node-worker": [tools["node"]["path"], "-e", WORKER, bundle, *arguments]}[endpoint]

    def add_startup(self, cpus=(5, 8)):
        for endpoint, project, cpu in zip(r2.CANDIDATES, ("attest", "assay"), cpus):
            origin = self.artifact(f"{project}/{project}.js", "prebuilt " + project)
            bundle = self.artifact(f"startup/{project}.js", "prebuilt " + project)
            source = self.artifact(f"startup/{project}.input", "trivial")
            argv = self.expected(endpoint, bundle["path"], ["check", source["path"]])
            self.manifest["startup"][endpoint] = {
                "origin": origin, "bundle": bundle, "input": source, "arguments": ["check"],
                "accept_line": "ACCEPT", "measurement": self.leg(argv, cpu, [bundle["path"], source["path"]], cache="cold")}

    def artifact(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(data)
        return {"path": str(path), "sha256": r2.digest(path.read_bytes())}

    def probe_inputs(self, bundle, source):
        return [bundle, source, *(s["path"] for s in self.manifest["probe"]["sources"])]

    def snapshot_of(self, ref):
        return [row["path"] for row in json.loads(Path(ref["path"]).read_text())["inputs"]]

    def leg(self, argv, cpu, inputs, accept="ACCEPT", cache="unknown", status="PASS"):
        self.serial += 1
        timed = tuple(cpu) if isinstance(cpu, tuple) else (cpu,) * 5
        samples = []
        for label, value in zip(["warm", 1, 2, 3, 4, 5], (9000, *timed)):
            # Wall and user CPU are not proportional to child CPU, so a median of either gives other ratios.
            system = min(value / 2, 2.5)
            samples.append({"run": label, "accepted": True, "exit_code": 0, "timed_out": False,
                            "accept_line_found": True, "threads_peak": 2,
                            "load1_start": 1.0, "load1_peak": 1.0, "load1_end": 1.0,
                            "cpu_ms": value, "user_cpu_ms": value - system, "system_cpu_ms": system,
                            "wall_ms": value * 3 + 11, "cache_state": cache})
        if status == "ERROR":
            samples[0].update(accepted=False, exit_code=1, accept_line_found=False)
            samples = samples[:1]
        if status == "UNMET":
            samples = []
        report = {"schema": 1, "name": f"synthetic-{self.serial}", "phase": "measurement", "launcher": "direct",
                  "command": argv, "toolchain_sha256": self.pins_sha, "accept_line": accept,
                  "status": status, "pin_check": {"exit_code": 0},
                  "limits": {k: self.pins[k] for k in ("load_ceiling", "load_wait_max_s", "leg_deadline_s")},
                  "samples": samples, "compile_cache_state": cache,
                  "summary": {"cpu_ms": {"median": -999}, "runs": 100}}
        if status == "ERROR":
            report["reason"] = r2.CHILD_FAILURE
        start = datetime(2026, 9, 25, tzinfo=timezone.utc) + timedelta(minutes=self.serial * 2)
        report.update(recorded_at=start.isoformat(), completed_at=(start + timedelta(minutes=1)).isoformat())
        # Snapshot exactly the leg's required inputs, as bench.py input_snapshot(args.input) does.
        sources = [Path(p) for p in inputs]
        report["inputs"] = [{"path": str(p), "sha256": r2.digest(p.read_bytes()), "bytes": p.stat().st_size} for p in sources]
        return self.artifact(f"leg-{self.serial}.json", json.dumps(report))

    def add_round(self, bun=(60, 70), node=(80, 90), denominator=100, empty=(5, 5)):
        result = {"denominator": {}, "endpoints": {}}
        denominators = denominator if isinstance(denominator, dict) else {w: denominator for w in r2.WORKLOADS}
        for w in r2.WORKLOADS:
            source = self.manifest["workloads"][w]["bend"]["path"]
            imports = [ref["path"] for ref in self.manifest["workloads"][w]["bend_imports"]]
            result["denominator"][w] = self.leg([self.pins["bend"]["binary"], source, "--check-only"],
                                                denominators[w], [source, *imports], "All terms check.")
        bundle = self.manifest["probe"]["javascript"]["path"]
        for endpoint, cpus, empty_cpu in zip(r2.CANDIDATES, (bun, node), empty):
            legs = {}
            for w, cpu in zip(r2.PROBES, (*cpus, empty_cpu)):
                source = self.manifest["empty"] if w == "empty" else self.manifest["workloads"][w]["sole"]
                legs[w] = self.leg(self.expected(endpoint, bundle, [source["path"]]), cpu,
                                   self.probe_inputs(bundle, source["path"]))
            result["endpoints"][endpoint] = legs
        self.manifest["rounds"].append(result)

    def amend(self, ref, change):
        data = json.loads(Path(ref["path"]).read_text())
        change(data)
        new = self.artifact(Path(ref["path"]).name, json.dumps(data))
        ref.update(new)

    def selected(self):
        return self.manifest["rounds"][0]["endpoints"]["bun"]["small"]

    def assemble(self):
        report = {}
        artifacts = r2.Artifacts(self.root)
        r2.assemble(self.manifest, artifacts, self.pins, self.pins_sha, report)
        artifacts.verify()
        return report

    def cli(self, output=None, pin_code=0):
        manifest_path = self.root / "manifest.json"
        manifest_path.write_text(json.dumps(self.manifest))
        output = output or self.root / "result.json"
        with patch.object(r2.subprocess, "run", return_value=subprocess.CompletedProcess([], pin_code, "pins", "")), contextlib.redirect_stdout(io.StringIO()) as printed:
            code = r2.main(["--manifest", str(manifest_path), "--toolchain", str(self.pins_path), "--json", str(output)])
        self.printed = printed.getvalue()
        return code, output

    def test_amber_recomputes_cpu_and_startup_share(self):
        report = self.assemble()
        self.assertEqual((report["endpoint"], report["verdict"]), ("bun", "AMBER"))
        self.assertEqual(report["rounds"][0]["decision"]["ratios"], {"bun": {"small": 0.6, "conversion": 0.7},
                                                                     "node-worker": {"small": 0.8, "conversion": 0.9}})
        self.assertEqual(report["rounds"][0]["startup_share"], {"bun": {"small": 0.05, "conversion": 0.05},
                                                                "node-worker": {"small": 0.08, "conversion": 0.08}})
        self.assertEqual(report["rounds"][0]["probe_startup_share"], {e: {"small": 0.05, "conversion": 0.05}
                                                                      for e in r2.CANDIDATES})
        self.assertTrue(report["native"]["info_only"])

    def test_ratios_bind_median_of_timed_child_cpu(self):
        # Median 60; mean 115; median with warm-up 130; wall and user CPU give other ratios.
        self.manifest["rounds"] = []
        self.add_round(bun=((50, 55, 60, 200, 210), 70), denominator={"small": (80, 90, 100, 400, 500), "conversion": 200})
        report = self.assemble()
        self.assertEqual(report["rounds"][0]["decision"]["ratios"]["bun"], {"small": 0.6, "conversion": 0.35})

    def test_startup_shares_use_own_legs(self):
        self.manifest["rounds"] = []
        self.add_round(denominator={"small": 100, "conversion": 200}, empty=(3, 4))
        result = self.assemble()["rounds"][0]
        self.assertEqual(result["startup_share"]["bun"], {"small": 0.05, "conversion": 0.025})
        self.assertEqual(result["startup_share"]["node-worker"], {"small": 0.08, "conversion": 0.04})
        self.assertEqual(result["probe_startup_share"]["bun"], {"small": 0.03, "conversion": 0.015})
        self.assertEqual(result["probe_startup_share"]["node-worker"], {"small": 0.04, "conversion": 0.02})

    def test_node_worker_command_is_literal(self):
        ref = self.manifest["rounds"][0]["endpoints"]["node-worker"]["small"]
        report = json.loads(Path(ref["path"]).read_text())
        bundle = self.manifest["probe"]["javascript"]["path"]
        source = self.manifest["workloads"]["small"]["sole"]["path"]
        self.assertEqual(report["command"], [self.pins["tools"]["node"]["path"], "-e", WORKER, bundle, source])
        self.assertIn("stackSizeMb: 63.96484375", WORKER)
        original = Path(ref["path"]).read_text()
        commands = [[self.pins["tools"]["node"]["path"], bundle, source],
                    [self.pins["tools"]["node"]["path"], "-e", WORKER.replace("63.96484375", "1"), bundle, source]]
        for command in commands:
            with self.subTest(command=commands.index(command)):
                ref.update(self.artifact(Path(ref["path"]).name, original))
                self.amend(ref, lambda x: x.update(command=command))
                with self.assertRaisesRegex(ValueError, "command mismatch"):
                    self.assemble()

    def test_threshold_boundaries(self):
        self.assertEqual([r2.classify(x) for x in (0.45, 0.450001, 1.0, 1.000001)], ["GREEN", "AMBER", "AMBER", "FAIL"])

    def test_green_requires_independent_confirmation(self):
        self.manifest["rounds"] = []
        self.add_round(bun=(40, 45), node=(50, 55))
        code, output = self.cli()
        self.assertEqual(code, 3)
        self.assertIsNone(json.loads(output.read_text())["endpoint"])
        self.add_round(bun=(42, 44), node=(51, 52))
        self.assertEqual(self.assemble()["verdict"], "GREEN")

    def test_confirmation_uses_conservative_verdict(self):
        self.manifest["rounds"] = []
        self.add_round(bun=(40, 40))
        self.add_round(bun=(50, 60), denominator=80)
        report = self.assemble()
        self.assertEqual(report["verdict"], "AMBER")
        # Each round divides by its own denominator legs.
        self.assertEqual(report["rounds"][1]["decision"]["ratios"]["bun"], {"small": 0.625, "conversion": 0.75})

    def test_confirmation_cannot_reuse_or_copy_reports(self):
        self.manifest["rounds"] = []
        self.add_round(bun=(40, 40))
        original = copy.deepcopy(self.manifest["rounds"][0])
        self.manifest["rounds"].append(copy.deepcopy(original))
        with self.assertRaisesRegex(ValueError, "new benchmark reports"):
            self.assemble()
        for mapping in [original["denominator"], *original["endpoints"].values()]:
            for key, ref in mapping.items():
                mapping[key] = self.artifact("copy-" + Path(ref["path"]).name, Path(ref["path"]).read_text())
        self.manifest["rounds"][1] = original
        with self.assertRaisesRegex(ValueError, "reuses"):
            self.assemble()

    def test_crossing_winners_and_confirmation_switch_require_ruling(self):
        self.manifest["rounds"] = []
        self.add_round(bun=(50, 90), node=(80, 60))
        with self.assertRaisesRegex(r2.Incomplete, "different workloads"):
            self.assemble()
        self.manifest["rounds"] = []
        self.add_round(bun=(40, 40))
        self.add_round(bun=(70, 70), node=(60, 60))
        with self.assertRaisesRegex(r2.Incomplete, "changed on confirmation"):
            self.assemble()

    def test_fail_is_complete_but_returns_exit_four(self):
        self.manifest["rounds"] = []
        self.add_round(bun=(101, 110), node=(120, 130))
        code, output = self.cli()
        self.assertEqual(code, 4)
        report = json.loads(output.read_text())
        self.assertEqual((report["status"], report["verdict"]), ("COMPLETE", "FAIL"))
        self.assertEqual(json.loads(self.pins_path.read_text())["endpoint"], None)

    def test_zero_denominator_rejected(self):
        self.manifest["rounds"] = []
        self.add_round(denominator=0)
        with self.assertRaisesRegex(ValueError, "denominator median"):
            self.assemble()

    def test_failed_endpoint_dropped_but_unmet_is_not(self):
        ref = self.selected()
        argv, inputs = json.loads(Path(ref["path"]).read_text())["command"], self.snapshot_of(ref)
        ref.update(self.leg(argv, 50, inputs, status="ERROR"))
        self.assertEqual(self.assemble()["endpoint"], "node-worker")
        ref.update(self.leg(argv, 50, inputs, status="UNMET"))
        with self.assertRaisesRegex(r2.Incomplete, "conditions unmet"):
            self.assemble()

    def test_no_eligible_endpoint_is_unmet(self):
        for legs in self.manifest["rounds"][0]["endpoints"].values():
            ref = legs["small"]
            argv = json.loads(Path(ref["path"]).read_text())["command"]
            ref.update(self.leg(argv, 50, self.snapshot_of(ref), status="ERROR"))
        with self.assertRaisesRegex(r2.Incomplete, "no JavaScript endpoint"):
            self.assemble()

    def test_native_never_enters_selection(self):
        binary = self.artifact("native", "native binary")
        measurements = {}
        for w in r2.PROBES:
            source = self.manifest["empty"] if w == "empty" else self.manifest["workloads"][w]["sole"]
            measurements[w] = self.leg([binary["path"], source["path"]], 0.01, self.probe_inputs(binary["path"], source["path"]))
        self.manifest["native"] = {"binary": binary, "build_log": self.artifact("native-ok.log", "built"), "measurements": measurements}
        self.assertEqual(self.assemble()["endpoint"], "bun")

    def test_incomplete_or_invalid_sample_is_rejected(self):
        changes = [lambda x: x["samples"].pop(), lambda x: x["samples"].append(x["samples"][-1]),
                   lambda x: x["samples"][1].update(run=True),
                   lambda x: x["samples"][1].update(threads_peak=None),
                   lambda x: x["samples"][1].update(threads_peak=True),
                   lambda x: x["samples"][1].update(load1_peak=9),
                   lambda x: x["samples"][1].update(load1_end=2),
                   lambda x: x["samples"][1].update(cpu_ms=-1),
                   lambda x: x["samples"][1].update(user_cpu_ms=1000),
                   lambda x: x["samples"][1].update(timed_out=True),
                   lambda x: x["samples"][1].update(accept_line_found=False),
                   lambda x: x["samples"].append({"accepted": False, "load1_peak": 1}),
                   lambda x: x.update(pin_check=None), lambda x: x.update(samples=[None])]
        ref = self.selected()
        original = Path(ref["path"]).read_text()
        for change in changes:
            with self.subTest(change=changes.index(change)):
                ref.update(self.artifact(Path(ref["path"]).name, original))
                self.amend(ref, change)
                with self.assertRaises(ValueError):
                    self.assemble()

    def test_discarded_load_attempt_does_not_enter_median(self):
        def discard(report):
            row = copy.deepcopy(report["samples"][1])
            row.update(accepted=False, discard_reason="load above ceiling", load1_peak=9, cpu_ms=90000)
            report["samples"].insert(1, row)
        self.manifest["rounds"] = []
        self.add_round(bun=((50, 55, 60, 65, 70), 70))
        self.amend(self.selected(), discard)
        # Including the discarded row would give a median of 62.5.
        self.assertEqual(self.assemble()["rounds"][0]["decision"]["ratios"]["bun"]["small"], 0.6)

    def test_command_pins_and_acceptance_are_bound(self):
        changes = [lambda x: x.update(launcher="/bin/zsh -f -c"), lambda x: x.update(phase="preflight"),
                   lambda x: x.update(command=["different"]), lambda x: x.update(toolchain_sha256="0" * 64),
                   lambda x: x.update(accept_line="Anything"), lambda x: x["limits"].update(load_ceiling=100),
                   lambda x: x["pin_check"].update(exit_code=1)]
        ref = self.selected()
        original = Path(ref["path"]).read_text()
        for change in changes:
            with self.subTest(change=changes.index(change)):
                ref.update(self.artifact(Path(ref["path"]).name, original))
                self.amend(ref, change)
                with self.assertRaises(ValueError):
                    self.assemble()

    def test_startup_needs_known_cache_and_exact_bundle_copy(self):
        ref = self.manifest["startup"]["bun"]["measurement"]
        original = Path(ref["path"]).read_text()
        self.amend(ref, lambda x: x["samples"][1].update(cache_state="unknown"))
        with self.assertRaisesRegex(ValueError, "compile-cache"):
            self.assemble()
        ref.update(self.artifact(Path(ref["path"]).name, original))
        self.manifest["startup"]["bun"]["bundle"] = self.artifact("wrong.js", "unrelated")
        with self.assertRaisesRegex(ValueError, "differs from its origin"):
            self.assemble()

    def test_artifact_tampering_is_rejected(self):
        Path(self.selected()["path"]).write_text("changed")
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            self.assemble()

    def test_manifest_cannot_rebind_old_measurements_to_changed_sources(self):
        self.manifest["probe"]["javascript"] = self.artifact("probe.js", "different compiler")
        with self.assertRaisesRegex(ValueError, "artifact hash mismatch"):
            self.assemble()

    def test_measured_inputs_must_include_the_probe_bundle(self):
        self.amend(self.selected(), lambda x: x.update(inputs=[r for r in x["inputs"] if Path(r["path"]).name != "probe.js"]))
        with self.assertRaisesRegex(ValueError, "measured input snapshot mismatch: .*probe\\.js"):
            self.assemble()

    def test_measured_inputs_must_include_the_probe_sources(self):
        self.amend(self.selected(), lambda x: x.update(inputs=[r for r in x["inputs"] if Path(r["path"]).name != "probe.bend"]))
        with self.assertRaisesRegex(ValueError, "measured input snapshot mismatch: .*probe\\.bend"):
            self.assemble()

    def test_measured_inputs_must_bind_the_workload_exactly_once(self):
        self.amend(self.selected(), lambda x: x["inputs"].extend([r for r in x["inputs"] if Path(r["path"]).name == "small.sole"]))
        with self.assertRaisesRegex(ValueError, "measured input snapshot mismatch: .*small\\.sole"):
            self.assemble()

    def test_legs_that_snapshot_exactly_their_inputs_assemble(self):
        bundle = self.manifest["probe"]["javascript"]["path"]
        source = self.manifest["workloads"]["small"]["sole"]["path"]
        self.assertEqual(self.snapshot_of(self.selected()), self.probe_inputs(bundle, source))
        self.assertEqual(self.snapshot_of(self.manifest["rounds"][0]["denominator"]["small"]),
                         [self.manifest["workloads"]["small"]["bend"]["path"]])
        self.assertEqual(self.assemble()["status"], "COMPLETE")

    def test_denominator_requires_bend_twin_imports(self):
        self.manifest["workloads"]["small"]["bend"] = self.artifact("small.bend", "import ./dep.bend as D\n")
        dependency = self.artifact("dep.bend", "dependency")
        self.manifest["workloads"]["small"]["bend_imports"] = [dependency]
        self.manifest["rounds"] = []
        self.add_round()
        self.manifest["workloads"]["small"]["bend_imports"] = []
        with self.assertRaisesRegex(ValueError, "must be listed in bend_imports: .*dep\\.bend"):
            self.assemble()
        self.manifest["workloads"]["small"]["bend_imports"] = [dependency]
        self.assertEqual(self.assemble()["workloads"]["small"]["bend_imports"], [dependency["path"]])
        self.amend(self.manifest["rounds"][0]["denominator"]["small"],
                   lambda x: x.update(inputs=[r for r in x["inputs"] if Path(r["path"]).name != "dep.bend"]))
        with self.assertRaisesRegex(ValueError, "measured input snapshot mismatch: .*dep\\.bend"):
            self.assemble()

    def test_denominator_rejects_changed_bend_twin_import(self):
        self.manifest["workloads"]["small"]["bend"] = self.artifact("small.bend", "import ./dep.bend as D\n")
        stale = self.artifact("dep.bend", "dependency")
        self.manifest["workloads"]["small"]["bend_imports"] = [self.artifact("dep.bend", "changed before the legs")]
        self.manifest["rounds"] = []
        self.add_round()
        self.assertEqual(self.assemble()["status"], "COMPLETE")
        # The legs measured the current import; only the manifest's bend_imports ref is stale.
        self.manifest["workloads"]["small"]["bend_imports"] = [stale]
        with self.assertRaisesRegex(ValueError, "hash mismatch: .*dep\\.bend"):
            self.assemble()

    def test_legacy_report_without_input_snapshots_is_rejected(self):
        self.amend(self.selected(), lambda x: x.pop("inputs"))
        with self.assertRaisesRegex(ValueError, "input snapshots missing"):
            self.assemble()

    def test_extra_measured_dependencies_are_bound_and_protected(self):
        dependency = self.artifact("import.bend", "imported content")
        path = Path(dependency["path"])
        self.amend(self.selected(), lambda x: x["inputs"].append({**dependency, "bytes": path.stat().st_size}))
        before = path.read_bytes()
        code, _ = self.cli(output=path)
        self.assertEqual(code, 1)
        self.assertEqual(path.read_bytes(), before)
        path.write_text("changed dependency")
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            self.assemble()

    def test_duplicate_keys_and_nonfinite_numbers_rejected(self):
        for text in ('{"a":1,"a":2}', '{"x":NaN}', '{"x":Infinity}', '{"x":1e999}'):
            with self.assertRaises(ValueError):
                r2.read_json(text)

    def test_cli_preserves_inputs_and_reports_pin_failure(self):
        before = self.pins_path.read_bytes()
        code, _ = self.cli(output=self.pins_path)
        self.assertEqual(code, 1)
        self.assertEqual(self.pins_path.read_bytes(), before)
        evidence = Path(self.selected()["path"])
        before = evidence.read_bytes()
        code, _ = self.cli(output=evidence)
        self.assertEqual(code, 1)
        self.assertEqual(evidence.read_bytes(), before)
        code, output = self.cli(pin_code=1)
        self.assertEqual(code, 1)
        self.assertEqual(json.loads(output.read_text())["status"], "ERROR")

    def test_existing_endpoint_cannot_silently_switch(self):
        # Legs measured after the pin carry the new toolchain sha, as the CLI requires.
        self.pin(endpoint="node-worker")
        self.add_startup()
        self.manifest["rounds"] = []
        self.add_round()
        with self.assertRaisesRegex(r2.Incomplete, "switch needs a user ruling"):
            self.assemble()

    def test_report_records_pinned_binary_shas(self):
        self.add_round()
        code, output = self.cli()
        rep = json.loads(output.read_text())
        self.assertEqual((code, rep["status"]), (0, "COMPLETE"))
        self.assertEqual(rep["pins"]["bend"]["binary_sha256"], self.pins["bend"]["binary_sha256"])
        self.assertEqual(rep["pins"]["bun"]["sha256"], self.pins["tools"]["bun"]["sha256"])
        self.assertEqual(rep["pins"]["node"]["sha256"], self.pins["tools"]["node"]["sha256"])
        self.assertEqual(rep["pins"]["observed"], {"bend": self.pins["bend"]["binary_sha256"],
                                                   "bun": self.pins["tools"]["bun"]["sha256"],
                                                   "node": self.pins["tools"]["node"]["sha256"]})
        Path(self.pins["tools"]["bun"]["path"]).write_text("replaced runtime")
        code, output = self.cli(output=self.root / "second.json")
        rep = json.loads(output.read_text())
        self.assertEqual((code, rep["status"]), (1, "ERROR"))
        self.assertIn("bun: observed binary sha256 differs from its pin", rep["reason"])

    def test_failed_rerun_keeps_complete_report(self):
        self.add_round()
        code, output = self.cli()
        self.assertEqual((code, json.loads(output.read_text())["status"]), (0, "COMPLETE"))
        self.pins["endpoint"] = "bun"
        self.pins_path.write_text(json.dumps(self.pins))
        code2, _ = self.cli(output=output)
        self.assertEqual(code2, 1)
        self.assertEqual(json.loads(output.read_text())["status"], "COMPLETE")
        # The sibling name follows the output stem, so two outputs in one directory never share it.
        failed = json.loads(output.with_suffix(".failed.json").read_text())
        self.assertEqual(failed["status"], "ERROR")
        self.assertIn("toolchain_sha256 mismatch", failed["reason"])
        self.assertFalse(output.with_name("r2-risk.failed.json").exists())

    def test_failed_rerun_never_overwrites_an_input_sibling(self):
        code, output = self.cli()
        self.assertEqual(code, 0)
        sibling = self.artifact(output.with_suffix(".failed.json").name, "saved native log")
        self.manifest["native"] = {"unavailable": sibling}
        before = Path(sibling["path"]).read_bytes()
        code, _ = self.cli(output=output, pin_code=1)
        self.assertEqual(code, 1)
        self.assertEqual(Path(sibling["path"]).read_bytes(), before)
        self.assertEqual(json.loads(output.read_text())["status"], "COMPLETE")
        self.assertIn("failed-run report would overwrite an input", self.printed)

    def test_error_without_child_failure_is_refused(self):
        def overwritten(x):
            x.update(status="ERROR", reason="ValueError: measured inputs changed during the benchmark leg")
            x.pop("summary")
            x["samples"] = x["samples"][:3]
            x["samples"][2].update(accepted=False, timed_out=True, exit_code=-9, accept_line_found=False)
        def mislabeled(x):
            # Valid child-failure samples, but bench.py later overwrote the reason with an input change.
            x.update(status="ERROR", reason="ValueError: measured inputs changed during the benchmark leg",
                     samples=x["samples"][:1])
            x["samples"][0].update(accepted=False, exit_code=1, accept_line_found=False)
        changes = [overwritten, lambda x: x.update(status="ERROR"), lambda x: x.update(status="ERROR", samples=[]),
                   mislabeled]
        ref = self.selected()
        original = Path(ref["path"]).read_text()
        for change in changes:
            with self.subTest(change=changes.index(change)):
                ref.update(self.artifact(Path(ref["path"]).name, original))
                self.amend(ref, change)
                with self.assertRaisesRegex(ValueError, "no child failure evidence"):
                    self.assemble()

    def test_exact_ties_prefer_bun(self):
        self.manifest["rounds"] = []
        self.add_round(bun=(60, 70), node=(60, 70))
        self.assertEqual(self.assemble()["endpoint"], "bun")

    def test_overlapping_measurements_are_rejected(self):
        other = json.loads(Path(self.manifest["rounds"][0]["denominator"]["small"]["path"]).read_text())
        self.amend(self.selected(), lambda x: x.update(recorded_at=other["recorded_at"], completed_at=other["completed_at"]))
        with self.assertRaisesRegex(ValueError, "overlap"):
            self.assemble()

    def test_confirmation_cannot_precede_first_round(self):
        self.manifest["rounds"] = []
        self.add_round(bun=(40, 40))
        self.add_round(bun=(42, 42))
        self.manifest["rounds"].reverse()
        with self.assertRaisesRegex(ValueError, "after the first round"):
            self.assemble()

    def test_measurement_timestamps_must_have_timezones(self):
        self.amend(self.selected(), lambda x: x.update(recorded_at="2026-09-25T12:00:00"))
        with self.assertRaisesRegex(ValueError, "timezone-aware"):
            self.assemble()


if __name__ == "__main__":
    unittest.main(verbosity=2)
