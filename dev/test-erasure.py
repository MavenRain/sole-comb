#!/usr/bin/env python3
"""Compare erasure behavior with immutable pinned Kanon fixtures."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/erasure"
REFERENCE = ROOT / "dev/validation/stage-a-erasure-observations.tsv"
EXPECTED_CASES = 221


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def run(name, argv, env, cwd=ROOT):
    completed = subprocess.run(argv, cwd=cwd, env=env, capture_output=True, text=True, timeout=300)
    (WORK / f"{name}.log").write_text(completed.stdout + completed.stderr)
    if completed.returncode:
        raise RuntimeError(f"{name} failed ({completed.returncode}):\n{(completed.stdout + completed.stderr)[-5000:]}")
    return completed.stdout.strip()


def compare_reference(path=REFERENCE):
    # Input hashes change with the harness, so only the oracle observations must stay equal.
    fresh = (WORK / "oracle-results.log").read_bytes()
    try:
        saved = path.read_bytes()
    except OSError as error:
        raise RuntimeError(f"saved observations are missing: {path}") from error
    if fresh != saved:
        pairs = zip(fresh.decode().splitlines(), saved.decode().splitlines())
        where = next((new.split("\t", 1)[0] for new, old in pairs if new != old), "the case count")
        raise RuntimeError(f"reference observations differ from the saved observations at {where}; "
                           f"review {WORK / 'oracle-results.log'} and copy it to {path}")


def oracle(pins, checks, env):
    reference = module("sole_reference", ROOT / "dev/reference-fixtures.py")
    expected, provenance = reference.expressions(pins, "erasure", checks)
    (WORK / "oracle-results.log").write_text("".join(f"{name}\t{value}\n" for (name, _, _), value in zip(checks, expected)))
    compare_reference()
    return expected, provenance


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    build = module("sole_build", ROOT / "dev/build.py")
    binary, _, _ = build.compiler(pins)
    case_module = module("sole_erasure_cases", ROOT / "dev/erasure-cases.py")
    checks = case_module.cases()
    if len(checks) != EXPECTED_CASES or len({name for name, _, _ in checks}) != len(checks):
        raise RuntimeError(f"expected {EXPECTED_CASES} uniquely named cases, found {len(checks)}")
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    (ROOT / "_build/bend-cache").mkdir(parents=True, exist_ok=True)
    expected, provenance = oracle(pins, checks, env)
    print(f"PASS A.5b.3.1 pinned reference fixtures: {len(checks)} cases", flush=True)
    imports = ["import Base", "import ../../test/erasure.bend as C", "import ../../lib/foundation.bend as F",
               "import ../../lib/kernel_literal.bend as Lit", "import ../../erase/eterm.bend as K",
               "import ../../erase/runtime.bend as R"]
    definitions = ['def check(+name: String, +expected: String, +actual: String) -> String:\n  F.choose(String, String.eq(expected, actual), "", String.append(name, String.append(" expected=", String.append(expected, String.append(" actual=", String.append(actual, "\\n"))))))']
    definitions += [f'def case_{i}() -> String:\n  check({json.dumps(name)}, {case_module.text(want).bend}, {expr.bend})' for i, ((name, expr, _), want) in enumerate(zip(checks, expected))]
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
            raise RuntimeError(f"{endpoint} failed erasure cases:\n{output}")
        print(f"PASS A.5b.3.1 {endpoint}: {len(checks)} cases", flush=True)
    run("compile-native", [str(binary), str(source), "-o", str(WORK / "checks.exe")], env)
    output = run("native-info", [str(WORK / "checks.exe")], env)
    if output not in ('"PASS"', "PASS"):
        raise RuntimeError(f"native INFO failed erasure cases:\n{output}")
    print(f"PASS A.5b.3.1 native INFO: {len(checks)} cases", flush=True)
    inputs = sorted(build.dependencies(ROOT / "test/erasure.bend")) + [ROOT / name for name in ("dev/erasure-cases.py",
             "dev/test-erasure.py",
             "dev/test-erasure-mutations.py",
             "dev/build.py",
             "dev/pin-check.py",
             "dev/toolchain.json",
             "dev/house-bend.py",
             "dev/test-house.py",
             "dev/bend-policy.json",
             "Makefile",
             "dev/reference-fixtures.py",
             "dev/reference-fixtures/manifest.json",
             "dev/reference-fixtures/erasure.json")]
    report = {"milestone": "A.5b.3.1", "cases_per_endpoint": len(checks), "endpoints": list(endpoints),
              "informational_endpoints": ["native"], "sources": {str(p.relative_to(ROOT)): digest(p) for p in inputs},
              "harness_sha256": digest(source), "kanon_revision": pins["kanon"]["revision"], "oracle": provenance,
              "observations_sha256": digest(WORK / "oracle-results.log"), "case_names": [name for name, _, _ in checks],
              "independent_expectations": sum(fixed is not None for _, _, fixed in checks),
              "r2_qualification": "pending", "stage_a_full_corpus": "pending",
              "type_directed_erasure": "pending", "scope": "erased representation, printer, runtime scope and capture pruning"}
    (WORK / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
