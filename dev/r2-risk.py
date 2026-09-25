#!/usr/bin/env python3
"""Assemble S0-5 R2 evidence without running benchmarks or changing pins."""

import argparse
from datetime import datetime, timezone
from functools import reduce
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
CANDIDATES = ("bun", "node-worker")
WORKLOADS = ("small", "conversion")
PROBES = (*WORKLOADS, "empty")
CHILD_FAILURE = "child exit or acceptance line mismatch"
SPEC = importlib.util.spec_from_file_location("sole_comb_build", ROOT / "dev/build.py")
build = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(build)


class Incomplete(ValueError):
    """Evidence is well formed, but a measurement or decision is missing."""


def digest(data):
    return hashlib.sha256(data).hexdigest()


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def read_json(data):
    def invalid(value):
        raise ValueError(f"non-finite JSON number: {value}")
    def finite_float(value):
        parsed = float(value)
        return parsed if math.isfinite(parsed) else invalid(value)
    return json.loads(data, object_pairs_hook=unique_object, parse_constant=invalid, parse_float=finite_float)


def fields(value, expected, label):
    if not isinstance(value, dict) or set(value) != set(expected):
        raise ValueError(f"{label}: expected fields {sorted(expected)}")


def number(value, label, positive=False):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{label}: expected a finite number")
    try:
        finite = math.isfinite(value)
    except OverflowError:
        finite = False
    if not finite or value < 0 or (positive and value == 0):
        raise ValueError(f"{label}: number out of range")
    return value


class Artifacts:
    def __init__(self, base):
        self.base = base
        self.records = {}
        self.intervals = {}

    def read(self, ref):
        fields(ref, ("path", "sha256"), "artifact")
        if not isinstance(ref["path"], str) or not ref["path"]:
            raise ValueError("artifact path must be nonempty")
        path = (self.base / ref["path"]).resolve()
        data = path.read_bytes()
        sha = digest(data)
        if ref["sha256"] != sha:
            raise ValueError(f"artifact hash mismatch: {path}")
        record = {"path": str(path), "sha256": sha, "bytes": len(data)}
        if str(path) in self.records and self.records[str(path)] != record:
            raise ValueError(f"artifact changed during assembly: {path}")
        self.records[str(path)] = record
        return path, data

    def file(self, ref):
        return str(self.read(ref)[0])

    def verify(self):
        for path, record in self.records.items():
            if digest(Path(path).read_bytes()) != record["sha256"]:
                raise ValueError(f"artifact changed during assembly: {path}")


def command(pins, endpoint, bundle, arguments):
    if endpoint == "bun":
        return [pins["tools"]["bun"]["path"], bundle, *arguments]
    if endpoint == "node-worker":
        return [pins["tools"]["node"]["path"], "-e", build.node_worker_script(pins), bundle, *arguments]
    if endpoint == "native":
        return [bundle, *arguments]
    raise ValueError(f"unknown endpoint: {endpoint}")


def measurement(artifacts, ref, pins, pins_sha, expected_command, accept_line, inputs, cache=False):
    path, data = artifacts.read(ref)
    leg = read_json(data)
    if not isinstance(leg, dict) or type(leg.get("schema")) is not int or leg["schema"] != 1:
        raise ValueError(f"{path}: unsupported benchmark schema")
    timestamps = [datetime.fromisoformat(leg[k]) for k in ("recorded_at", "completed_at")]
    if any(t.tzinfo is None or t.utcoffset() is None for t in timestamps) or timestamps[1] < timestamps[0]:
        raise ValueError(f"{path}: benchmark timestamps must be ordered and timezone-aware")
    artifacts.intervals[str(path)] = timestamps
    snapshots = leg.get("inputs")
    if not isinstance(snapshots, list) or any(not isinstance(row, dict) for row in snapshots):
        raise ValueError(f"{path}: measured input snapshots missing")
    for row in snapshots:
        fields(row, ("path", "sha256", "bytes"), "input snapshot")
        source = artifacts.file({"path": row["path"], "sha256": row["sha256"]})
        if type(row["bytes"]) is not int or row != artifacts.records[source]:
            raise ValueError(f"{path}: input snapshot identity mismatch")
    for source in inputs:
        if sum(row == artifacts.records[source] for row in snapshots) != 1:
            raise ValueError(f"{path}: measured input snapshot mismatch: {source}")
    expected = {"phase": "measurement", "launcher": "direct", "command": expected_command,
                "toolchain_sha256": pins_sha, "accept_line": accept_line}
    for key, value in expected.items():
        if leg.get(key) != value:
            raise ValueError(f"{path}: {key} mismatch")
    check = leg.get("pin_check")
    if not isinstance(check, dict) or type(check.get("exit_code")) is not int or check["exit_code"] != 0:
        raise ValueError(f"{path}: benchmark pin check did not pass")
    for key in ("load_ceiling", "load_wait_max_s", "leg_deadline_s"):
        if number(leg["limits"][key], key, positive=True) != number(pins[key], key, positive=True):
            raise ValueError(f"{path}: {key} differs from the pins")
    samples = leg.get("samples")
    if not isinstance(samples, list) or any(not isinstance(s, dict) for s in samples):
        raise ValueError(f"{path}: samples must be a list of objects")
    status = leg.get("status")
    if status == "ERROR":
        # Only bench.py's child-failure outcome drops a runtime; input changes, harness errors
        # and timeouts need a new measurement.
        last = samples[-1] if samples else {}
        if (leg.get("reason") != CHILD_FAILURE or not samples or any(s.get("timed_out") is not False for s in samples)
                or type(last.get("exit_code")) is not int
                or (last["exit_code"] == 0 and last.get("accept_line_found") is not False)):
            raise ValueError(f"{path}: endpoint rejection has no child failure evidence")
    elif status in ("UNMET", "INTERRUPTED"):
        pass
    elif status == "PASS":
        for s in samples:
            if type(s.get("accepted")) is not bool:
                raise ValueError(f"{path}: sample acceptance must be explicit")
            if not s["accepted"] and (s.get("discard_reason") != "load above ceiling" or
                                      number(s["load1_peak"], "discarded load") <= pins["load_ceiling"]):
                raise ValueError(f"{path}: discarded sample lacks a load rejection")
        accepted = [s for s in samples if s.get("accepted") is True]
        labels = [s.get("run") for s in accepted]
        if labels != ["warm", 1, 2, 3, 4, 5] or any(type(x) is not int for x in labels[1:]):
            raise ValueError(f"{path}: need one warm-up and exactly five timed samples")
        for s in accepted:
            if type(s.get("exit_code")) is not int or s["exit_code"] != 0 or s.get("timed_out") is not False:
                raise ValueError(f"{path}: failed child marked accepted")
            if s.get("accept_line_found") is not True or s.get("load_observation_error"):
                raise ValueError(f"{path}: acceptance or load observation missing")
            if type(s.get("threads_peak")) is not int or s["threads_peak"] < 1:
                raise ValueError(f"{path}: child thread observation missing")
            loads = [number(s[k], k) for k in ("load1_start", "load1_peak", "load1_end")]
            if max(loads) > pins["load_ceiling"] or loads[1] < max(loads[0], loads[2]):
                raise ValueError(f"{path}: accepted sample has invalid load")
            cpu = number(s["cpu_ms"], "cpu_ms")
            total = number(s["user_cpu_ms"], "user_cpu_ms") + number(s["system_cpu_ms"], "system_cpu_ms")
            number(s["wall_ms"], "wall_ms")
            if not math.isclose(cpu, total, rel_tol=1e-9, abs_tol=1e-6):
                raise ValueError(f"{path}: child CPU is not user plus system CPU")
            if cache and (s.get("cache_state") not in ("cold", "warm") or
                          s["cache_state"] != leg.get("compile_cache_state")):
                raise ValueError(f"{path}: startup compile-cache state must be declared cold or warm")
        # Recompute from raw samples, never trust a cached summary or include warm-up.
        median = statistics.median(s["cpu_ms"] for s in accepted[1:])
        return {"status": status, "cpu_ms": median, "evidence": leg, "path": str(path)}
    else:
        raise ValueError(f"{path}: incomplete or unknown benchmark status {status!r}")
    return {"status": status, "cpu_ms": None, "evidence": leg, "path": str(path)}


def classify(ratio):
    return "GREEN" if ratio <= 0.45 else "AMBER" if ratio <= 1.0 else "FAIL"


def decide(denominators, endpoints, disqualified=()):
    if any(leg["status"] != "PASS" for leg in denominators.values()):
        raise Incomplete("both denominator measurements must pass")
    for leg in denominators.values():
        number(leg["cpu_ms"], "denominator median", positive=True)
    eligible = {}
    dropped = []
    for endpoint, legs in endpoints.items():
        states = [leg["status"] for leg in legs.values()]
        # An endpoint that failed correctness qualification never enters selection, however fast it is.
        if endpoint in disqualified:
            dropped.append(endpoint)
        elif "UNMET" in states or "INTERRUPTED" in states:
            raise Incomplete(f"{endpoint}: measurement conditions unmet")
        elif "ERROR" in states:
            dropped.append(endpoint)
        else:
            eligible[endpoint] = {w: number(legs[w]["cpu_ms"] / denominators[w]["cpu_ms"], "CPU ratio") for w in WORKLOADS}
    if not eligible:
        raise Incomplete("no JavaScript endpoint gave the required verdict on every probe")
    winners = [e for e in CANDIDATES if e in eligible and all(
        eligible[e][w] <= eligible[other][w] for other in eligible for w in WORKLOADS)]
    if not winners:
        raise Incomplete("different endpoints are faster on different workloads; a selection rule needs a user ruling")
    endpoint = winners[0]  # Exact ties follow the documented candidate order.
    return {"endpoint": endpoint, "ratios": eligible, "dropped": dropped,
            "verdict": classify(max(eligible[endpoint].values()))}


def bend_closure(source, seen=frozenset()):
    """Bend .bend imports recursively, plus foreign .c/.js leaves, as build.dependencies() without its ROOT check."""
    source = Path(source).resolve()
    if source in seen:
        return seen
    text = source.read_text()
    foreign = frozenset((source.parent / name).resolve() for name in build.FOREIGN.findall(text))
    return reduce(lambda acc, imported: bend_closure(source.parent / imported, acc),
                  [name for name in build.IMPORT.findall(text) if not build.CONTENT_ADDRESSED.match(name)],
                  seen | {source} | foreign)


def pinned_binaries(pins):
    return {"bend": {key: pins["bend"][key] for key in ("revision", "version", "binary", "binary_sha256")},
            **{name: {key: pins["tools"][name][key] for key in ("path", "realpath", "version", "sha256")}
               for name in ("bun", "node")}}


def observe_binaries(block):
    observed = {name: digest(Path(block[name]["binary" if name == "bend" else "path"]).read_bytes())
                for name in ("bend", "bun", "node")}
    for name, sha in observed.items():
        if sha != block[name]["binary_sha256" if name == "bend" else "sha256"]:
            raise ValueError(f"{name}: observed binary sha256 differs from its pin")
    return observed


def completed(path):
    try:
        existing = read_json(path.read_bytes())
    except (OSError, ValueError):
        return False
    return isinstance(existing, dict) and existing.get("status") == "COMPLETE"


def assemble(manifest, artifacts, pins, pins_sha, report):
    expected = ("schema", "probe", "workloads", "empty", "startup", "native", "rounds")
    fields(manifest, (*expected, "disqualified") if "disqualified" in manifest else expected, "manifest")
    if type(manifest["schema"]) is not int or manifest["schema"] != 1:
        raise ValueError("unsupported manifest schema")
    disqualified = manifest.get("disqualified", [])
    if (not isinstance(disqualified, list) or disqualified != [e for e in CANDIDATES if e in disqualified] or
            len(disqualified) >= len(CANDIDATES)):
        raise ValueError("disqualified must list a strict subset of the candidate endpoints in candidate order")
    probe = manifest["probe"]
    fields(probe, ("sources", "javascript", "build_log"), "probe")
    if not isinstance(probe["sources"], list) or not probe["sources"]:
        raise ValueError("probe sources must be a nonempty list")
    probe_sources = [artifacts.file(source) for source in probe["sources"]]
    artifacts.file(probe["build_log"])
    javascript = artifacts.file(probe["javascript"])
    fields(manifest["workloads"], WORKLOADS, "workloads")
    twins = {}
    imports = {}
    for name, pair in manifest["workloads"].items():
        fields(pair, ("bend", "sole", "bend_imports"), f"workload {name}")
        twins[name] = {language: artifacts.file(pair[language]) for language in ("bend", "sole")}
        if not isinstance(pair["bend_imports"], list):
            raise ValueError(f"workload {name}: bend_imports must be a list")
        imports[name] = [artifacts.file(ref) for ref in pair["bend_imports"]]
        closure = {str(path) for path in bend_closure(twins[name]["bend"])} - {twins[name]["bend"]}
        if closure != set(imports[name]) or len(imports[name]) != len(closure):
            raise ValueError(f"workload {name}: Bend twin imports must be listed in bend_imports: {sorted(closure)}")
    report["workloads"] = {w: {**twins[w], "bend_imports": imports[w]} for w in WORKLOADS}
    inputs = {w: twins[w]["sole"] for w in WORKLOADS}
    inputs["empty"] = artifacts.file(manifest["empty"])
    for language in ("bend", "sole"):
        if artifacts.records[twins["small"][language]]["sha256"] == artifacts.records[twins["conversion"][language]]["sha256"]:
            raise ValueError("small and conversion workloads must be distinct")

    def read_leg(ref, argv, inputs, accept="ACCEPT", cache=False):
        return measurement(artifacts, ref, pins, pins_sha, argv, accept, inputs, cache)

    def probe_legs(refs, endpoint, bundle):
        fields(refs, PROBES, f"{endpoint} probes")
        return {w: read_leg(refs[w], command(pins, endpoint, bundle, [inputs[w]]),
                            [bundle, inputs[w], *probe_sources]) for w in PROBES}

    fields(manifest["startup"], CANDIDATES, "startup")
    report["startup"] = {}
    for endpoint, entry in manifest["startup"].items():
        fields(entry, ("origin", "bundle", "input", "arguments", "accept_line", "measurement"), f"startup {endpoint}")
        origin = artifacts.file(entry["origin"])
        bundle = artifacts.file(entry["bundle"])
        project = "attest" if endpoint == "bun" else "assay"
        checkout = Path(pins["prior_art"][project]["checkout"]).resolve()
        if not Path(origin).is_relative_to(checkout) or Path(origin).name != f"{project}.js":
            raise ValueError(f"{endpoint}: startup origin must be the prebuilt {project}.js")
        if origin == bundle or Path(bundle).is_relative_to(checkout):
            raise ValueError(f"{endpoint}: measure a scratch copy of the startup bundle")
        if artifacts.records[origin]["sha256"] != artifacts.records[bundle]["sha256"]:
            raise ValueError(f"{endpoint}: startup copy differs from its origin")
        source = artifacts.file(entry["input"])
        args = entry["arguments"]
        if not isinstance(args, list) or any(not isinstance(arg, str) for arg in args):
            raise ValueError("startup arguments must be a list of strings, preceding the input path")
        accept = entry["accept_line"]
        if not isinstance(accept, str) or not accept.strip() or "\n" in accept or "\r" in accept:
            raise ValueError("startup acceptance must be one nonempty stdout line")
        leg = read_leg(entry["measurement"], command(pins, endpoint, bundle, [*args, source]), [bundle, source], accept, True)
        report["startup"][endpoint] = {"bundle_bytes": artifacts.records[bundle]["bytes"], **leg}

    native = manifest["native"]
    if isinstance(native, dict) and set(native) == {"unavailable"}:
        artifacts.file(native["unavailable"])
        report["native"] = {"status": "UNAVAILABLE", "info_only": True, "build_log": native["unavailable"]}
    else:
        fields(native, ("binary", "build_log", "measurements"), "native")
        binary = artifacts.file(native["binary"])
        artifacts.file(native["build_log"])
        report["native"] = {"info_only": True, "measurements": probe_legs(native["measurements"], "native", binary)}

    rounds = manifest["rounds"]
    if not isinstance(rounds, list) or not 1 <= len(rounds) <= 2:
        raise ValueError("need one measurement round and at most one confirmation round")
    report["rounds"] = []
    for entry in rounds:
        fields(entry, ("denominator", "endpoints"), "round")
        fields(entry["denominator"], WORKLOADS, "denominators")
        fields(entry["endpoints"], CANDIDATES, "endpoints")
        result = {"denominator": {}, "endpoints": {}}
        report["rounds"].append(result)
        for w in WORKLOADS:
            argv = [pins["bend"]["binary"], twins[w]["bend"], "--check-only"]
            result["denominator"][w] = read_leg(entry["denominator"][w], argv, [twins[w]["bend"], *imports[w]], "All terms check.")
        for endpoint in CANDIDATES:
            result["endpoints"][endpoint] = probe_legs(entry["endpoints"][endpoint], endpoint, javascript)
        result["decision"] = decide(result["denominator"], result["endpoints"], disqualified)
        for endpoint in CANDIDATES:
            startup = report["startup"][endpoint]
            if startup["status"] != "PASS":
                raise Incomplete(f"{endpoint}: prebuilt-bundle startup measurement did not pass")
            result.setdefault("startup_share", {})[endpoint] = {
                w: number(startup["cpu_ms"] / result["denominator"][w]["cpu_ms"], "startup ratio") for w in WORKLOADS}
            empty = result["endpoints"][endpoint]["empty"]
            result.setdefault("probe_startup_share", {})[endpoint] = {
                w: number(empty["cpu_ms"] / result["denominator"][w]["cpu_ms"], "empty ratio") if empty["status"] == "PASS" else None
                for w in WORKLOADS}
    first = report["rounds"][0]["decision"]
    if first["verdict"] == "GREEN" and len(rounds) != 2:
        raise Incomplete("GREEN contradicts the prior; an independent confirmation round is required")
    chosen = first["endpoint"]
    if any(r["decision"]["endpoint"] != chosen for r in report["rounds"]):
        raise Incomplete("the fastest endpoint changed on confirmation; selection needs a user ruling")
    if len(rounds) == 2:
        evidence = [{leg["path"] for leg in r["denominator"].values()} |
                    {leg["path"] for legs in r["endpoints"].values() for leg in legs.values()}
                    for r in report["rounds"]]
        if evidence[0] & evidence[1]:
            raise ValueError("confirmation must use new benchmark reports")
        first_hashes = {artifacts.records[p]["sha256"] for p in evidence[0]}
        if any(artifacts.records[p]["sha256"] in first_hashes for p in evidence[1]):
            raise ValueError("confirmation reuses a first-round benchmark report")
        if min(artifacts.intervals[p][0] for p in evidence[1]) < max(artifacts.intervals[p][1] for p in evidence[0]):
            raise ValueError("confirmation must start after the first round finishes")
    intervals = sorted((start, end, path) for path, (start, end) in artifacts.intervals.items())
    for previous, current in zip(intervals, intervals[1:]):
        if current[0] < previous[1]:
            raise ValueError(f"benchmark legs overlap: {previous[2]} and {current[2]}")
    worst = max(r["decision"]["ratios"][chosen][w] for r in report["rounds"] for w in WORKLOADS)
    if pins["endpoint"] is not None and pins["endpoint"] != chosen:
        raise Incomplete("selection differs from the pinned endpoint; a switch needs a user ruling")
    report.update(status="COMPLETE", endpoint=chosen, verdict=classify(worst), worst_ratio=worst,
                  confirmation_required=first["verdict"] == "GREEN")


def referenced_paths(value, base):
    if isinstance(value, dict):
        if isinstance(value.get("path"), str):
            yield (base / value["path"]).resolve()
        for child in value.values():
            yield from referenced_paths(child, base)
    elif isinstance(value, list):
        for child in value:
            yield from referenced_paths(child, base)


def write_report(path, report):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, delete=False) as handle:
            temporary = Path(handle.name)
            json.dump(report, handle, indent=2, allow_nan=False)
            handle.write("\n")
        temporary.replace(path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--toolchain", type=Path, default=ROOT / "dev/toolchain.json")
    parser.add_argument("--json", type=Path, default=ROOT / "dev/r2-risk.json")
    args = parser.parse_args(argv)
    output = args.json.resolve()
    manifest_path, pins_path = args.manifest.resolve(), args.toolchain.resolve()
    failed = output.with_suffix(".failed.json")
    forbidden = {manifest_path, pins_path, Path(__file__).resolve(), ROOT / "dev/build.py", ROOT / "dev/pin-check.py"}
    artifacts = Artifacts(manifest_path.parent)
    report = {"schema": 1, "recorded_at": datetime.now(timezone.utc).isoformat(),
              "status": "ERROR", "endpoint": None, "verdict": None, "rounds": [],
              "prior": {"project": "assay", "ratio": 1.346,
                        "denominator": "bend FILE -o", "comparable_denominator": False},
              "selection_rule": "lowest median child CPU on both workloads; exact ties prefer bun",
              "thresholds": {"green_max": 0.45, "amber_max": 1.0, "status": "plan-derived, not user-ratified"}}
    code = 1
    try:
        manifest_data, pins_data = manifest_path.read_bytes(), pins_path.read_bytes()
        manifest, pins = read_json(manifest_data), read_json(pins_data)
        forbidden.update(referenced_paths(manifest, manifest_path.parent))
        if output in forbidden:
            raise ValueError("report output would overwrite an input")
        report.update(manifest={"path": str(manifest_path), "sha256": digest(manifest_data)},
                      toolchain={"path": str(pins_path), "sha256": digest(pins_data)},
                      pins=pinned_binaries(pins))
        check = subprocess.run([sys.executable, "-P", str(ROOT / "dev/pin-check.py"),
                                "--toolchain", str(pins_path)], capture_output=True, text=True, timeout=180)
        report["pin_check"] = {"exit_code": check.returncode, "stdout": check.stdout, "stderr": check.stderr}
        if check.returncode:
            raise ValueError("current toolchain pin check failed")
        report["pins"]["observed"] = observe_binaries(report["pins"])
        assemble(manifest, artifacts, pins, digest(pins_data), report)
        artifacts.verify()
        if manifest_path.read_bytes() != manifest_data or pins_path.read_bytes() != pins_data:
            raise ValueError("manifest or pins changed during assembly")
        code = 4 if report["verdict"] == "FAIL" else 0
    except Incomplete as error:
        report.update(status="UNMET", reason=str(error), endpoint=None, verdict=None)
        code = 3
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        report.update(status="ERROR", reason=f"{type(error).__name__}: {error}", endpoint=None, verdict=None)
    except KeyboardInterrupt:
        report.update(status="INTERRUPTED", reason="cancelled by operator", endpoint=None, verdict=None)
        code = 130
    report["artifacts"] = list(artifacts.records.values())
    forbidden.update(Path(path) for path in artifacts.records)
    # A failed rerun never replaces a COMPLETE report; it goes to a sibling file.
    target = failed if report["status"] != "COMPLETE" and completed(output) else output
    if target in forbidden:
        name = "report output" if target == output else "failed-run report"
        report.update(status="ERROR", reason=f"{name} would overwrite an input", endpoint=None, verdict=None)
        code = 1
    else:
        write_report(target, report)
        if target != output:
            print(f"R2-RISK kept the COMPLETE report {output}; this run wrote {target}")
    if report["status"] == "COMPLETE":
        for index, result in enumerate(report["rounds"], 1):
            for endpoint, ratios in result["decision"]["ratios"].items():
                for w, ratio in ratios.items():
                    print(f"R2-RISK endpoint={endpoint} numer_cpu_ms={result['endpoints'][endpoint][w]['cpu_ms']:.6f} "
                          f"denom_cpu_ms={result['denominator'][w]['cpu_ms']:.6f} ratio={ratio:.6f} workload={w} round={index}")
        print(f"R2-RISK {report['verdict']} endpoint={report['endpoint']}")
    else:
        print(f"R2-RISK {report['status']} {report.get('reason', '')}")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
