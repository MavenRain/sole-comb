#!/usr/bin/env python3
"""Require focused A.4 checks to reject three isolated semantic mutations."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/checking-mutations"
MUTANTS = [
    ("linear-close", "lib/kernel_check.bend",
     "Bool.not(F.choose(Bool, affine, Q.at_most_once(level, uses), Q.exactly_once(level, uses)))", "False{}",
     ["check-constant-1", "check-linear-product-duplicate", "check-linear-alias-duplicate"]),
    ("budget-refund", "lib/kernel_check.bend",
     "answer => next => drive(A, resume(answer), next)", "answer => _ => drive(A, resume(answer), remaining)",
     ["infer-budget-let-1", "infer-budget-let-4", "infer-budget-let-8"]),
    ("negative-field", "lib/kernel_positivity.bend",
     "F.choose(result, occurs(names, term), Fail{E.Not_yet{nonpositive_word}}, Done{Unit{}})", "Done{Unit{}}",
     ["positive-negative-arrow", "ctors-negative"]),
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
    result = subprocess.run(argv, cwd=dest, env=env, capture_output=True, text=True, timeout=120)
    (dest / (label + ".log")).write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f"{dest.name} {label} failed; see {dest / (label + '.log')}")
    return result.stdout.strip()


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    baseline = json.loads((ROOT / "_build/checking/result.json").read_text())
    for name, expected in baseline["sources"].items():
        if digest(ROOT / name) != expected:
            raise RuntimeError(f"stale checking evidence for {name}; run make test-checking")
    oracle = ROOT / "_build/checking/oracle-results.log"
    if digest(oracle) != baseline["observations_sha256"]:
        raise RuntimeError("checking observations changed")
    expected = dict(line.split("\t", 1) for line in oracle.read_text().splitlines())
    cases = {name: expr for name, expr, _ in load("sole_mutant_cases", ROOT / "dev/checking-cases.py").cases()}
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    binary, _, _ = load("sole_mutant_build", ROOT / "dev/build.py").compiler(pins)
    generated = ROOT / "_build/checking/checks.bend"
    if digest(generated) != baseline["harness_sha256"]:
        raise RuntimeError("checking harness changed")
    header = generated.read_text().split("def case_0", 1)[0].replace("../../", "./")
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    evidence = []
    for name, path, before, after, selected in MUTANTS:
        dest = WORK / name
        for directory in ("lib", "test"):
            shutil.copytree(ROOT / directory, dest / directory, dirs_exist_ok=True)
        body = '"PASS"'
        for case in reversed(selected):
            body = f"String.append(check({json.dumps(case)}, {json.dumps(expected[case])}, {cases[case].bend}), {body})"
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
        print(f"PASS A.4 mutation {name}: rejected by {', '.join(killed)}", flush=True)
    report = {"milestone": "A.4", "endpoint": "bun", "mutants": evidence,
              "checking_evidence_sha256": digest(ROOT / "_build/checking/result.json"),
              "runner_sha256": digest(Path(__file__)), "source_hashes": baseline["sources"]}
    (WORK / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
