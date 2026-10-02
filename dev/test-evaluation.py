#!/usr/bin/env python3
"""Compare evaluation behavior with immutable pinned Kanon fixtures."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/evaluation"
EXPECTED_CASES = 251


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def run(name, argv, env, cwd=ROOT, timeout=120):
    completed = subprocess.run(argv, cwd=cwd, env=env, capture_output=True, text=True, timeout=timeout)
    (WORK / f"{name}.log").write_text(completed.stdout + completed.stderr)
    if completed.returncode:
        raise RuntimeError(f"{name} failed ({completed.returncode}):\n{(completed.stdout + completed.stderr)[-5000:]}")
    return completed.stdout.strip()


def oracle(pins, checks, env):
    reference = module("sole_reference", ROOT / "dev/reference-fixtures.py")
    expected, provenance = reference.expressions(pins, "evaluation", checks)
    (WORK / "oracle-results.log").write_text("".join(f"{name}\t{value}\n" for (name, _, _), value in zip(checks, expected)))
    return expected, provenance


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    build = module("sole_build", ROOT / "dev/build.py")
    binary, _, _ = build.compiler(pins)
    checks = module("sole_evaluation_cases", ROOT / "dev/evaluation-cases.py").cases()
    if len(checks) != EXPECTED_CASES or len({name for name, _, _ in checks}) != len(checks):
        raise RuntimeError(f"expected {EXPECTED_CASES} uniquely named cases, found {len(checks)}")
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    (ROOT / "_build/bend-cache").mkdir(parents=True, exist_ok=True)
    expected, provenance = oracle(pins, checks, env)
    print(f"PASS A.3 pinned reference fixtures: {len(checks)} cases", flush=True)
    imports = ["import Base", "import ../../test/evaluation.bend as C", "import ../../test/foundation.bend as FT"]
    for filename, alias in [("foundation", "F"), ("kernel_error", "E"), ("kernel_level", "L"), ("kernel_literal", "Lit"),
                            ("kernel_quantity", "Q"), ("kernel_shape", "S"), ("kernel_term", "T"), ("kernel_value", "V"),
                            ("kernel_positivity", "Pos"), ("kernel_prim", "P"), ("kernel_rules", "R"), ("kernel_global", "G"),
                            ("kernel_eval", "EV"), ("kernel_conv", "CV")]:
        imports.append(f"import ../../lib/{filename}.bend as {alias}")
    definitions = ['def check(+name: String, +expected: String, +actual: String) -> String:\n  F.choose(String, String.eq(expected, actual), "", String.append(name, String.append(" expected=", String.append(expected, String.append(" actual=", String.append(actual, "\\n"))))))']
    definitions += [f'def case_{i}() -> String:\n  check({json.dumps(name)}, {json.dumps(want)}, {expr.bend})' for i, ((name, expr, _), want) in enumerate(zip(checks, expected))]
    groups = []
    for start in range(0, len(checks), 24):
        body = '""'
        for i in reversed(range(start, min(start + 24, len(checks)))):
            body = f"String.append(case_{i}, {body})"
        groups.append(f"group_{start}")
        definitions.append(f"def group_{start}() -> String:\n  {body}")
    body = '"PASS"'
    for group in reversed(groups):
        body = f"String.append({group}, {body})"
    source = WORK / "checks.bend"
    source.write_text("\n\n".join(imports + definitions + [f"def main() -> String:\n  {body}\n"]))
    run("check", [str(binary), str(source), "--check-only"], env)
    run("compile-js", [str(binary), str(source), "-o", str(WORK / "checks.js")], env)
    endpoints = {"bun": [pins["tools"]["bun"]["path"], str(WORK / "checks.js")],
                 "node-worker": [pins["tools"]["node"]["path"], "-e", build.node_worker_script(pins), str(WORK / "checks.js")]}
    for endpoint, argv in endpoints.items():
        output = run(endpoint, argv, env)
        if output not in ('"PASS"', "PASS"):
            raise RuntimeError(f"{endpoint} failed evaluation cases:\n{output}")
        print(f"PASS A.3 {endpoint}: {len(checks)} cases", flush=True)
    run("compile-native", [str(binary), str(source), "-o", str(WORK / "checks.exe")], env, timeout=1800)
    output = run("native-info", [str(WORK / "checks.exe")], env)
    if output not in ('"PASS"', "PASS"):
        raise RuntimeError(f"native INFO failed evaluation cases:\n{output}")
    print(f"PASS A.3 native INFO: {len(checks)} cases", flush=True)
    inputs = sorted((ROOT / "lib").glob("*.bend")) + [ROOT / name for name in ("test/foundation.bend",
             "test/representation.bend",
             "test/evaluation.bend",
             "dev/evaluation-cases.py",
             "dev/test-evaluation.py",
             "dev/build.py",
             "dev/pin-check.py",
             "dev/toolchain.json",
             "dev/house-bend.py",
             "dev/test-house.py",
             "dev/bend-policy.json",
             "Makefile",
             "dev/reference-fixtures.py",
             "dev/reference-fixtures/manifest.json",
             "dev/reference-fixtures/evaluation.json")]
    report = {"milestone": "A.3", "cases_per_endpoint": len(checks), "endpoints": list(endpoints),
              "informational_endpoints": ["native"], "sources": {str(p.relative_to(ROOT)): digest(p) for p in inputs},
              "harness_sha256": digest(source), "kanon_revision": pins["kanon"]["revision"], "oracle": provenance,
              "observations_sha256": digest(WORK / "oracle-results.log"), "case_names": [name for name, _, _ in checks],
              "independent_expectations": sum(fixed is not None for _, _, fixed in checks),
              "r2_qualification": "pending", "stage_a_full_corpus": "pending"}
    (WORK / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
