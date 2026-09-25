#!/usr/bin/env python3
"""Collect serial S0-5 measurements from a prepared scratch probe and source twins."""

import argparse
from datetime import datetime, timezone
import importlib.util
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location("sole_comb_r2", ROOT / "dev/r2-risk.py")
r2 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(r2)


# bench.py reasons for a child-side UNMET that an INFO-only native leg records instead of stopping on.
INFO_UNMET = ("child thread count unavailable", "child deadline exceeded")


def now():
    return datetime.now(timezone.utc).isoformat()


def run_benchmark(args):
    # bench.py must receive SIGINT so its wait4 cleanup can reap the measured child.
    # subprocess.run kills the harness on interruption, bypassing that cleanup.
    with subprocess.Popen(args, start_new_session=True) as child:
        try:
            return subprocess.CompletedProcess(args, child.wait())
        except KeyboardInterrupt:
            if child.poll() is None:
                try:
                    child.send_signal(signal.SIGINT)
                except ProcessLookupError:
                    pass
            child.wait()
            raise


class Stopped(Exception):
    def __init__(self, status, reason, code):
        super().__init__(reason)
        self.status, self.code = status, code


class Collection:
    def __init__(self, plan_path, toolchain, output):
        self.plan_path = plan_path.resolve(strict=True)
        self.toolchain = toolchain.resolve(strict=True)
        self.output = output.resolve()
        self.files = {}
        self.legs = []
        self.caches = {}
        self.manifest = {}

    def reference(self, path):
        path = path.resolve(strict=True)
        ref = {"path": str(path), "sha256": r2.digest(path.read_bytes())}
        if str(path) in self.files and self.files[str(path)] != ref:
            raise ValueError(f"input changed: {path}")
        self.files[str(path)] = ref
        return ref

    def input(self, value):
        if not isinstance(value, str) or not value or "\0" in value:
            raise ValueError("input paths must be nonempty strings")
        return self.reference(self.plan_path.parent / value)

    def verify(self):
        for ref in self.files.values():
            if r2.digest(Path(ref["path"]).read_bytes()) != ref["sha256"]:
                raise ValueError(f"input changed: {ref['path']}")

    def verify_preparation(self, path, plan_ref, pins_ref, disqualified):
        required = set(self.files) - {plan_ref["path"]}
        ref = self.input(path)
        prepared = r2.read_json(Path(ref["path"]).read_bytes())
        if (not isinstance(prepared, dict) or type(prepared.get("schema")) is not int or prepared["schema"] != 1 or
                prepared.get("status") != "READY"):
            raise ValueError("preparation must have schema 1 and status READY")
        if prepared.get("run_plan") != plan_ref or prepared.get("toolchain") != pins_ref:
            raise ValueError("preparation does not bind this run plan and toolchain")
        candidates = prepared.get("qualified_candidates")
        if (not isinstance(candidates, dict) or set(candidates) != set(r2.CANDIDATES) or
                any(type(candidates[endpoint]) is not bool for endpoint in r2.CANDIDATES) or
                not any(candidates.values())):
            raise ValueError("preparation must qualify at least one candidate endpoint")
        if not prepared.get("disqualified") == disqualified == [e for e in r2.CANDIDATES if not candidates[e]]:
            raise ValueError("preparation must drop exactly the candidate endpoints that failed qualification")
        files = prepared.get("files")
        if not isinstance(files, list) or not files:
            raise ValueError("preparation must bind its files")
        seen = set()
        for expected in files:
            r2.fields(expected, ("path", "sha256"), "prepared file")
            actual = self.input(expected["path"])
            if actual != expected or actual["path"] in seen:
                raise ValueError("prepared file changed or is duplicated")
            seen.add(actual["path"])
        if not required <= seen:
            raise ValueError("preparation does not bind every run-plan input")

    def prepare(self):
        plan_ref = self.reference(self.plan_path)
        pins_ref = self.reference(self.toolchain)
        plan = r2.read_json(self.plan_path.read_bytes())
        self.pins = r2.read_json(self.toolchain.read_bytes())
        self.pins_sha = pins_ref["sha256"]
        self.verify()
        expected = ("schema", "probe", "workloads", "empty", "startup", "native")
        # Only a bound preparation can drop a candidate; a manual plan measures and selects both.
        r2.fields(plan, (*expected, "preparation", "disqualified") if "preparation" in plan else expected, "run plan")
        if type(plan["schema"]) is not int or plan["schema"] != 1:
            raise ValueError("unsupported run plan schema")
        # A fresh external directory prevents overwriting inputs, prior evidence or checkouts.
        protected = [ROOT, *(Path(p["checkout"]).resolve() for p in self.pins["prior_art"].values())]
        protected += [Path(self.pins[k]["checkout"]).resolve() for k in ("bend", "kanon")
                      if "checkout" in self.pins.get(k, {})]
        if self.output.exists() or any(self.output.is_relative_to(p) for p in protected):
            raise ValueError("output must be a new scratch directory outside project and tool checkouts")

        probe = plan["probe"]
        r2.fields(probe, ("sources", "javascript", "build_log"), "probe")
        if not isinstance(probe["sources"], list) or not probe["sources"]:
            raise ValueError("probe sources must be a nonempty list")
        sources = [self.input(path) for path in probe["sources"]]
        source_paths = {s["path"] for s in sources}
        if len(source_paths) != len(sources):
            raise ValueError("duplicate probe source")
        for source in sources:
            if Path(source["path"]).is_relative_to(ROOT):
                raise ValueError("scratch probe sources must stay outside the sole-comb tree")
            if source["path"].endswith(".bend"):
                closure = {str(p) for p in r2.bend_closure(source["path"])}
                if not closure <= source_paths:
                    raise ValueError("probe sources must include their transitive local imports")
        self.manifest = {"schema": 1,
                         "probe": {"sources": sources, "javascript": self.input(probe["javascript"]),
                                   "build_log": self.input(probe["build_log"])},
                         "workloads": {}, "empty": self.input(plan["empty"]),
                         "startup": {}, "native": {}, "rounds": []}
        if Path(self.manifest["empty"]["path"]).read_bytes().strip():
            raise ValueError("empty probe input must contain only whitespace")
        r2.fields(plan["workloads"], r2.WORKLOADS, "workloads")
        for name in r2.WORKLOADS:
            pair = plan["workloads"][name]
            r2.fields(pair, ("bend", "sole"), f"workload {name}")
            twin = {language: self.input(pair[language]) for language in ("bend", "sole")}
            twin["bend_imports"] = [self.reference(p) for p in sorted(r2.bend_closure(twin["bend"]["path"]))
                                    if str(p) != twin["bend"]["path"]]
            self.manifest["workloads"][name] = twin
        for language in ("bend", "sole"):
            if (self.manifest["workloads"]["small"][language]["sha256"] ==
                    self.manifest["workloads"]["conversion"][language]["sha256"]):
                raise ValueError("small and conversion workloads must be distinct")

        r2.fields(plan["startup"], r2.CANDIDATES, "startup")
        for endpoint, project in zip(r2.CANDIDATES, ("attest", "assay")):
            entry = plan["startup"][endpoint]
            r2.fields(entry, ("origin", "input", "arguments", "accept_line", "cache_state"), f"startup {endpoint}")
            origin = self.input(entry["origin"])
            checkout = Path(self.pins["prior_art"][project]["checkout"]).resolve()
            if not Path(origin["path"]).is_relative_to(checkout) or Path(origin["path"]).name != f"{project}.js":
                raise ValueError(f"{endpoint}: origin must be the prebuilt {project}.js")
            if (not isinstance(entry["arguments"], list) or
                    any(not isinstance(a, str) or "\0" in a for a in entry["arguments"])):
                raise ValueError("startup arguments must be a list of strings")
            accept = entry["accept_line"]
            if not isinstance(accept, str) or not accept.strip() or any(c in accept for c in "\n\r\0"):
                raise ValueError("startup acceptance must be one nonempty line")
            if entry["cache_state"] not in ("cold", "warm"):
                raise ValueError("startup cache state must be cold or warm")
            self.caches[endpoint] = entry["cache_state"]
            self.manifest["startup"][endpoint] = {
                "origin": origin, "input": self.input(entry["input"]),
                "arguments": entry["arguments"], "accept_line": accept}
        native = plan["native"]
        if isinstance(native, dict) and set(native) == {"unavailable"}:
            self.manifest["native"] = {"unavailable": self.input(native["unavailable"])}
        else:
            r2.fields(native, ("binary", "build_log"), "native")
            self.manifest["native"] = {k: self.input(native[k]) for k in ("binary", "build_log")}
        if "preparation" in plan:
            self.verify_preparation(plan["preparation"], plan_ref, pins_ref, plan["disqualified"])
            self.manifest["disqualified"] = plan["disqualified"]
        self.verify()
        self.output.mkdir(parents=True, exist_ok=False)
        self.journal = {"schema": 1, "recorded_at": now(), "status": "PREPARING",
                        "plan": plan_ref, "toolchain": pins_ref, "legs": self.legs,
                        "cache_states": self.caches, "endpoint": None, "verdict": None}
        (self.output / "measurements").mkdir()
        (self.output / "startup").mkdir()
        for entry in self.manifest["startup"].values():
            origin = Path(entry["origin"]["path"])
            target = self.output / "startup" / origin.name
            shutil.copyfile(origin, target)
            entry["bundle"] = self.reference(target)
            if entry["bundle"]["sha256"] != entry["origin"]["sha256"]:
                raise ValueError("startup origin changed while copying")
        self.verify()
        self.journal["status"] = "PREPARED"
        self.save()

    def save(self):
        r2.write_report(self.output / "manifest.json", self.manifest)
        r2.write_report(self.output / "collection.json", self.journal)

    def measure(self, name, argv, inputs, accept="ACCEPT", cache="unknown", allow_rejection=False,
                info_only=False):
        self.verify()
        path = self.output / "measurements" / f"{name}.json"
        if path.exists():
            raise ValueError(f"measurement already exists: {path}")
        args = [sys.executable, "-P", str(ROOT / "dev/bench.py"), name, "--argv", shlex.join(argv),
                "--toolchain", str(self.toolchain), "--runs", "5", "--require-threads",
                # Attached values keep an acceptance line that starts with '-' from reading as an option.
                f"--accept-line={accept}", f"--cache-state={cache}", "--json", str(path)]
        for source in inputs:
            args.extend(("--input", source))
        row = {"name": name, "status": "RUNNING", "command": args, "measurement": str(path)}
        self.legs.append(row)
        self.save()
        print(f"R2-COLLECT {name}", flush=True)
        process = run_benchmark(args)
        row["exit_code"] = process.returncode
        try:
            self.verify()
            ref = self.reference(path)
            artifacts = r2.Artifacts(self.output)
            leg = r2.measurement(artifacts, ref, self.pins, self.pins_sha, argv, accept, inputs,
                                 cache=cache != "unknown")
        except (OSError, ValueError, KeyError, TypeError) as error:
            # main saves the journal after this error, so the finished leg is never left RUNNING.
            row.update(status="ERROR", reason=f"{type(error).__name__}: {error}")
            raise
        row["status"] = leg["status"]
        self.save()
        expected_code = {"PASS": 0, "ERROR": 1, "UNMET": 3, "INTERRUPTED": 130}[leg["status"]]
        if process.returncode != expected_code:
            raise ValueError(f"{name}: benchmark exit code disagrees with its report")
        if leg["status"] == "INTERRUPTED":
            raise Stopped("INTERRUPTED", f"{name}: cancelled", 130)
        # An informational leg keeps a child-side miss as its recorded outcome; load and harness stops still apply.
        recorded = info_only and leg["evidence"].get("reason") in INFO_UNMET
        if (leg["status"] == "UNMET" and not recorded) or (leg["status"] == "ERROR" and not allow_rejection):
            raise Stopped("UNMET", f"{name}: measurement did not pass", 3)
        return ref, leg

    def probes(self, label, endpoint, bundle, info_only=False):
        refs, legs = {}, {}
        for name in r2.PROBES:
            source = (self.manifest["empty"] if name == "empty" else self.manifest["workloads"][name]["sole"])["path"]
            argv = r2.command(self.pins, endpoint, bundle, [source])
            inputs = [bundle, source, *(ref["path"] for ref in self.manifest["probe"]["sources"])]
            refs[name], legs[name] = self.measure(f"{label}-{name}", argv, inputs, allow_rejection=True,
                                                  info_only=info_only)
        return refs, legs

    def round(self, number):
        entry = {"denominator": {}, "endpoints": {}}
        denominators, endpoints = {}, {}
        self.manifest["rounds"].append(entry)
        for name, pair in self.manifest["workloads"].items():
            source = pair["bend"]["path"]
            inputs = [source, *(ref["path"] for ref in pair["bend_imports"])]
            entry["denominator"][name], denominators[name] = self.measure(
                f"round-{number}-denominator-{name}", [self.pins["bend"]["binary"], source, "--check-only"],
                inputs, accept="All terms check.")
        for endpoint in r2.CANDIDATES:
            entry["endpoints"][endpoint], endpoints[endpoint] = self.probes(
                f"round-{number}-{endpoint}", endpoint, self.manifest["probe"]["javascript"]["path"])
        self.save()
        return r2.decide(denominators, endpoints, self.manifest.get("disqualified", []))

    def collect(self):
        self.journal["status"] = "RUNNING"
        for endpoint, entry in self.manifest["startup"].items():
            argv = r2.command(self.pins, endpoint, entry["bundle"]["path"], [*entry["arguments"], entry["input"]["path"]])
            entry["measurement"], _ = self.measure(
                f"startup-{endpoint}", argv, [entry["bundle"]["path"], entry["input"]["path"]],
                entry["accept_line"], self.caches[endpoint])
        native = self.manifest["native"]
        if "binary" in native:
            native["measurements"], _ = self.probes("native", "native", native["binary"]["path"], info_only=True)
        first = self.round(1)
        if first["verdict"] == "GREEN":
            self.round(2)
        self.verify()
        self.save()
        report = self.output / "r2-risk.json"
        code = r2.main(["--manifest", str(self.output / "manifest.json"),
                        "--toolchain", str(self.toolchain), "--json", str(report)])
        result = r2.read_json(report.read_bytes())
        self.verify()
        self.journal.update(status=result["status"], endpoint=result["endpoint"], verdict=result["verdict"],
                            report=self.reference(report))
        if "reason" in result:
            self.journal["reason"] = result["reason"]
        return code


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--toolchain", type=Path, default=ROOT / "dev/toolchain.json")
    parser.add_argument("--output", type=Path, required=True, help="new scratch directory for this attempt")
    parser.add_argument("--prepare-only", action="store_true", help="bind inputs and copy startup bundles without measuring")
    args = parser.parse_args(argv)
    collection = None
    code = 1
    try:
        collection = Collection(args.plan, args.toolchain, args.output)
        collection.prepare()
        code = 0 if args.prepare_only else collection.collect()
    except Stopped as error:
        collection.journal.update(status=error.status, reason=str(error), endpoint=None, verdict=None)
        code = error.code
    except r2.Incomplete as error:
        collection.journal.update(status="UNMET", reason=str(error), endpoint=None, verdict=None)
        code = 3
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f"R2-COLLECT ERROR {type(error).__name__}: {error}", file=sys.stderr)
        if collection is not None and hasattr(collection, "journal"):
            collection.journal.update(status="ERROR", reason=str(error), endpoint=None, verdict=None)
    except KeyboardInterrupt:
        if collection is not None and hasattr(collection, "journal"):
            collection.journal.update(status="INTERRUPTED", reason="cancelled by operator", endpoint=None, verdict=None)
        code = 130
    if collection is not None and hasattr(collection, "journal"):
        collection.journal["completed_at"] = now()
        collection.journal["exit_code"] = code
        try:
            collection.save()
        except OSError as error:
            print(f"R2-COLLECT ERROR cannot save attempt: {error}", file=sys.stderr)
            return 1
        print(f"R2-COLLECT {collection.journal['status']} {collection.output}")
    return code


if __name__ == "__main__":
    sys.exit(main())
