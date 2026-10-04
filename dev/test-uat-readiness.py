#!/usr/bin/env python3
"""Check the frozen UAT baseline and current public-source prerequisite boundary."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
BASELINE_SHA256 = "f4f4d0511b6ce5f7dc917f302dae8f40f4c927a6a90e4c6a22dd52ba083619ec"
EXPECTED_CASES = 12
HOSTS = ("bun", "node-worker", "native")
PARSE_AXIOMS = runpy.run_path(str(ROOT / "dev/capture-uat-baseline.py"))["parse_axioms"]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def load_baseline(root=ROOT):
    directory = root / "dev/uat-baseline"
    data = (directory / "uat-baseline.json").read_bytes()
    if digest(data) != BASELINE_SHA256:
        raise RuntimeError("UAT baseline pin differs; review a new baseline explicitly")
    record = json.loads(data)
    if record["schema"] != 1 or record["stage"] != "UAT.0":
        raise RuntimeError("unsupported UAT baseline schema")
    for path, key in (("uat-baseline.stdout", "stdout_sha256"),
                      ("uat-baseline-build.stdout", "build_stdout_sha256")):
        if digest((directory / path).read_bytes()) != record[key]:
            raise RuntimeError(f"UAT baseline bytes differ: {path}")
    script = (root / "test/uat-baseline.lean").read_bytes()
    if digest(script) != record["script_sha256"]:
        raise RuntimeError("UAT baseline inspection script differs")
    names = re.findall(r"^#check @(\S+)$", script.decode(), re.M)
    if len(names) != 21 or names != record["declarations"]:
        raise RuntimeError("UAT baseline declaration census differs")
    observed = PARSE_AXIOMS((directory / "uat-baseline.stdout").read_text(), names)
    if observed != record["axioms"]:
        raise RuntimeError("UAT baseline axiom inventory differs")
    return record


def load_cases(root=ROOT):
    manifest = json.loads((root / "dev/uat-readiness.json").read_bytes())
    cases = manifest["cases"]
    ids = [c["id"] for c in cases]
    paths = [c["path"] for c in cases]
    if (manifest["schema"] != 1 or manifest["stage"] != "UAT.0"
            or len(cases) != EXPECTED_CASES or len(set(ids)) != len(ids)
            or len(set(paths)) != len(paths)):
        raise RuntimeError("UAT readiness case census differs")
    expected = {str(p.relative_to(root)) for p in (root / "test/uat").rglob("*.sole-comb")}
    expected.add("examples/uat-foundation.sole-comb")
    if set(paths) != expected:
        raise RuntimeError("UAT readiness source census differs")
    for case in cases:
        source = (root / case["path"]).resolve()
        source.relative_to(root.resolve())
        if not source.is_file():
            raise RuntimeError("UAT readiness source is missing")
        if case["status"] == "landed":
            if case["path"] != "examples/uat-foundation.sole-comb" or case["definitions"] != 6:
                raise RuntimeError("UAT foundation definition census differs")
        elif case["status"] in ("blocked", "required-refusal"):
            if not case["diagnostic"]:
                raise RuntimeError("UAT refusal needs its complete diagnostic")
        else:
            raise RuntimeError("unknown UAT readiness disposition")
        if case["milestone"] not in ("UAT.0", "UAT.1", "UAT.2", "UAT.3") or not case["requirement"]:
            raise RuntimeError("UAT probe needs its requirement and milestone")
    return cases


def check_result(case, result):
    path = case["path"]
    if case["status"] == "landed":
        header = f"CHECK {path} defs={case['definitions']} ok\n"
        if result.returncode != 0 or result.stderr or not result.stdout.startswith(header):
            raise RuntimeError(f"UAT prerequisite {path} did not check\n{result.stdout}{result.stderr}")
        return result.stdout[len(header):]
    if (result.returncode != 1 or result.stdout
            or result.stderr != f"CHECK {path} FAIL {case['diagnostic']}\n"):
        raise RuntimeError(f"UAT boundary {path} lacks its source refusal\n{result.stdout}{result.stderr}")
    return result.stderr


def sources(cases, root=ROOT):
    selected = [root / c["path"] for c in cases]
    selected += [root / p for p in ("Makefile", "sole-comb", "dev/cli.py", "dev/build.py",
                 "dev/toolchain.json", "dev/uat-readiness.json", "dev/test-uat-readiness.py",
                 "dev/test-uat-harness.py", "dev/capture-uat-baseline.py", "test/uat-baseline.lean")]
    selected += list((root / "dev/uat-baseline").glob("*"))
    for directory in ("bin", "lib", "surface", "erase"):
        selected += list((root / directory).glob("*.bend"))
    return {str(p.relative_to(root)): digest(p.read_bytes()) for p in sorted(selected)}


def compare_hosts(first, key, output):
    if first.setdefault(key, output) != output:
        raise RuntimeError(f"UAT probe differs across hosts: {key}")


def assert_inputs_unchanged(cases, before, root=ROOT):
    if sources(cases, root) != before:
        raise RuntimeError("UAT readiness inputs changed during validation")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hosts", default=",".join(HOSTS))
    parser.add_argument("--baseline-checkout", type=Path,
                        help="also rebuild and compare the pinned local Lean baseline")
    args = parser.parse_args()
    hosts = args.hosts.split(",")
    if not hosts or len(set(hosts)) != len(hosts) or any(h not in HOSTS for h in hosts):
        parser.error("hosts must be a unique list of bun,node-worker,native")
    baseline = load_baseline()
    cases = load_cases()
    before = sources(cases)
    work = ROOT / "_build/uat-readiness"
    work.mkdir(parents=True, exist_ok=True)
    if args.baseline_checkout:
        subprocess.run([sys.executable, "-P", str(ROOT / "dev/capture-uat-baseline.py"),
                        "--checkout", str(args.baseline_checkout), "--revision",
                        baseline["baseline"]["revision"], "--output", str(work / "baseline")],
                       cwd=ROOT, check=True, timeout=1800)
        fresh = json.loads((work / "baseline/uat-baseline.json").read_bytes())
        if fresh != baseline:
            raise RuntimeError("live UAT baseline differs from the frozen inspection")
    observations = []
    first = {}
    for host in hosts:
        for case in cases:
            modes = ("print", "erased") if case["status"] == "landed" else ("print",)
            for mode in modes:
                result = subprocess.run(["./sole-comb", "check", case["path"], "--host", host,
                                         f"--{mode}"], cwd=ROOT, capture_output=True, text=True, timeout=900)
                compare_hosts(first, (case["id"], mode), check_result(case, result))
                observation = {"host": host, "id": case["id"], "path": case["path"], "mode": mode,
                               "exit": result.returncode, "stdout": result.stdout, "stderr": result.stderr}
                observations.append(observation)
                log = work / f"{host}-{case['id']}-{mode}"
                log.with_suffix(".stdout").write_text(result.stdout)
                log.with_suffix(".stderr").write_text(result.stderr)
        print(f"UAT READINESS PASS {host}: {len(cases)} probes", flush=True)
    assert_inputs_unchanged(cases, before)
    record = {"schema": 1, "stage": "UAT.0", "hosts": hosts, "sources": before,
              "baseline_sha256": BASELINE_SHA256, "cases": len(cases), "observations": observations}
    (work / "result.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"UAT READINESS PASS: {len(observations)} observations; {(work / 'result.json').relative_to(ROOT)}")


if __name__ == "__main__":
    main()
