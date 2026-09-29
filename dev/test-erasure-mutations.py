#!/usr/bin/env python3
"""Require erasure support checks to reject three isolated semantic mutations."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/erasure-mutations"
MUTANTS = [
    ("branch-depth", "erase/runtime.bend",
     "F.Pair2{F.Int63.add(depth, n), b}", "F.Pair2{depth, b}",
     ["shift/case", "reindex/case"]),
    ("capture-order", "erase/runtime.bend",
     "ps, F.Int63.pred(count))", "ps, F.Int63.zero)",
     ["prune/first", "prune/gaps"]),
    ("nested-closure", "erase/runtime.bend",
     "case K.KClos{_, _, cs}: scope_terms(depth, cs)", "case K.KClos{_, _, _}: []",
     ["vars/closure", "prune/nested-closure"]),
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def run(dest, label, argv, env):
    result = subprocess.run(argv, cwd=dest, env=env, capture_output=True, text=True, timeout=300)
    (dest / (label + ".log")).write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f"{dest.name} {label} failed; see {dest / (label + '.log')}")
    return result.stdout.strip()


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    baseline = json.loads((ROOT / "_build/erasure/result.json").read_text())
    for name, expected in baseline["sources"].items():
        if digest(ROOT / name) != expected:
            raise RuntimeError(f"stale erasure evidence for {name}; run make test-erasure")
    oracle = ROOT / "_build/erasure/oracle-results.log"
    if digest(oracle) != baseline["observations_sha256"]:
        raise RuntimeError("erasure observations changed")
    expected = dict(line.split("\t", 1) for line in oracle.read_text().splitlines())
    case_module = load("sole_erasure_mutant_cases", ROOT / "dev/erasure-cases.py")
    cases = {name: expr for name, expr, _ in case_module.cases()}
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    binary, _, _ = load("sole_mutant_build", ROOT / "dev/build.py").compiler(pins)
    generated = ROOT / "_build/erasure/checks.bend"
    if digest(generated) != baseline["harness_sha256"]:
        raise RuntimeError("erasure harness changed")
    header = generated.read_text().split("def case_0", 1)[0].replace("../../", "./")
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    evidence = []
    for name, path, before, after, selected in MUTANTS:
        dest = WORK / name
        if dest.exists():
            shutil.rmtree(dest)
        for directory in ("lib", "test", "erase"):
            shutil.copytree(ROOT / directory, dest / directory, dirs_exist_ok=True)
        body = '"PASS"'
        for case in reversed(selected):
            body = f"String.append(check({json.dumps(case)}, {case_module.text(expected[case]).bend}, {cases[case].bend}), {body})"
        source = dest / "cases.bend"
        source.write_text(header + "def main() -> String:\n  " + body + "\n")
        target = dest / path
        original = target.read_text()
        if original.count(before) != 1:
            raise RuntimeError(f"mutation {name} must match exactly once")
        run(dest, "control-compile", [str(binary), str(source), "-o", str(dest / "control.js")], env)
        control = run(dest, "control", [pins["tools"]["bun"]["path"], str(dest / "control.js")], env)
        if control not in ('"PASS"', "PASS"):
            raise RuntimeError(f"control {name} failed")
        target.write_text(original.replace(before, after, 1))
        run(dest, "mutant-compile", [str(binary), str(source), "-o", str(dest / "mutant.js")], env)
        output = run(dest, "mutant", [pins["tools"]["bun"]["path"], str(dest / "mutant.js")], env)
        killed = [case for case in selected if case + " expected=" in output]
        if not killed:
            raise RuntimeError(f"mutation {name} survived")
        evidence.append({"name": name, "path": path, "before_sha256": digest(ROOT / path),
                         "mutated_sha256": digest(target), "harness_sha256": digest(source),
                         "selected_cases": selected, "killed_by": killed,
                         "control_sha256": digest(dest / "control.log"), "output_sha256": digest(dest / "mutant.log")})
        print(f"PASS A.5b.3.1 mutation {name}: rejected by {', '.join(killed)}", flush=True)
    report = {"milestone": "A.5b.3.1", "endpoint": "bun", "mutants": evidence,
              "erasure_evidence_sha256": digest(ROOT / "_build/erasure/result.json"),
              "runner_sha256": digest(Path(__file__)), "source_hashes": baseline["sources"]}
    (WORK / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
