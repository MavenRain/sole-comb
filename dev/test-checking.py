#!/usr/bin/env python3
"""A.4 differential cases against freshly compiled, verified pinned OCaml sources."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/checking"
EXPECTED_CASES = 291
EXPECTED_INDEPENDENT = 125


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def run(name, argv, env, cwd=ROOT):
    completed = subprocess.run(argv, cwd=cwd, env=env, capture_output=True, text=True, timeout=120)
    (WORK / f"{name}.log").write_text(completed.stdout + completed.stderr)
    if completed.returncode:
        raise RuntimeError(f"{name} failed ({completed.returncode}):\n{(completed.stdout + completed.stderr)[-5000:]}")
    return completed.stdout.strip()


def oracle(pins, checks, env):
    upstream = Path(pins["kanon"]["checkout"])
    pin = pins["kanon"]["revision"]
    if run("oracle-head", ["git", "rev-parse", "HEAD"], env, upstream) != pin:
        raise RuntimeError("Kanon HEAD differs from the configured pin")
    sources = {p.stem.capitalize(): p for p in (upstream / "lib").glob("*.ml")}
    dependencies = run("oracle-dependencies", ["ocamldep", "-modules", *map(str, sources.values())], env)
    graph = {Path(line.split(":", 1)[0]).stem.capitalize(): line.split(":", 1)[1].split() for line in dependencies.splitlines()}
    needed = {"Eval", "Conv", "Global", "Term", "Value", "Shape", "Quantity", "Level", "Literal", "Positivity", "Prim", "Bignum", "Error", "Rules", "Check", "Pp", "Spec_count", "Budget"}
    while True:
        expanded = needed | {dep for key in needed for dep in graph[key] if dep in sources}
        if needed == expanded:
            break
        needed = expanded
    dest = WORK / "oracle"
    dest.mkdir(parents=True, exist_ok=True)
    interfaces = {p.stem.capitalize(): p for p in (upstream / "lib").glob("*.mli")}
    files = [path for key in sorted(needed) for path in (interfaces.get(key), sources[key]) if path is not None]
    reference = {}
    for source in files:
        content = source.read_bytes()
        pinned = subprocess.run(["git", "show", f"{pin}:lib/{source.name}"], cwd=upstream, capture_output=True, check=True).stdout
        if content != pinned:
            raise RuntimeError(f"oracle source differs from pin: {source.name}")
        (dest / source.name).write_bytes(content)
        reference[source.name] = digest(source)
    adapter = "\n".join((ROOT / "test/evaluation-oracle.ml").read_text().splitlines()[:14]) + "\n" + (ROOT / "test/checking-oracle.ml").read_text()
    calls = [f"let () = Printf.printf \"%s\\t%s\\n\" {json.dumps(name)} ({expr.ocaml})" for name, expr, _ in checks]
    (dest / "oracle_cases.ml").write_text(adapter + "\n" + "\n".join(calls) + "\n")
    order = run("oracle-order", ["ocamldep", "-sort", *[source.name for source in files]], env, dest).split()
    run("oracle-compile", ["ocamlfind", "ocamlc", "-package", "zarith", "-linkpkg", *order, "oracle_cases.ml", "-o", "oracle.exe"], env, dest)
    rows = run("oracle-results", [str(dest / "oracle.exe")], env).splitlines()
    parsed = [row.split("\t", 1) for row in rows]
    if len(parsed) != len(checks) or any(len(row) != 2 or row[0] != case[0] for row, case in zip(parsed, checks)):
        raise RuntimeError("oracle output has missing, duplicate, or reordered cases")
    expected = []
    for (_, observed), (name, _, fixed) in zip(parsed, checks):
        if fixed is not None and fixed != observed:
            raise RuntimeError(f"independent expectation failed: {name}: expected {fixed!r}, oracle returned {observed!r}")
        expected.append(observed)
    provenance = {"sources": reference, "generated_adapter_sha256": digest(dest / "oracle_cases.ml"),
                  "executable_sha256": digest(dest / "oracle.exe"), "compiler_path": shutil.which("ocamlc"),
                  "compiler_version": run("oracle-version", ["ocamlc", "-version"], env),
                  "zarith_version": run("oracle-zarith", ["ocamlfind", "query", "-format", "%v", "zarith"], env),
                  "build": "fresh compilation of byte-identical sources at the configured revision"}
    return expected, provenance


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    build = module("sole_build", ROOT / "dev/build.py")
    binary, _, _ = build.compiler(pins)
    checks = module("sole_checking_cases", ROOT / "dev/checking-cases.py").cases()
    if len(checks) != EXPECTED_CASES or len({name for name, _, _ in checks}) != len(checks):
        raise RuntimeError(f"expected {EXPECTED_CASES} uniquely named cases, found {len(checks)}")
    independent = sum(fixed is not None for _, _, fixed in checks)
    if independent != EXPECTED_INDEPENDENT:
        raise RuntimeError(f"expected {EXPECTED_INDEPENDENT} fixed expectations, found {independent}")
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    (ROOT / "_build/bend-cache").mkdir(parents=True, exist_ok=True)
    expected, provenance = oracle(pins, checks, env)
    print(f"PASS A.4 pinned source oracle: {len(checks)} cases", flush=True)
    imports = ["import Base", "import ../../test/checking.bend as C", "import ../../test/foundation.bend as FT"]
    for filename, alias in [("foundation", "F"), ("kernel_error", "E"), ("kernel_level", "L"), ("kernel_literal", "Lit"),
                            ("kernel_quantity", "Q"), ("kernel_shape", "S"), ("kernel_term", "T"), ("kernel_value", "V"),
                            ("kernel_positivity", "Pos"), ("kernel_prim", "P"), ("kernel_rules", "R"), ("kernel_global", "G"),
                            ("kernel_eval", "EV"), ("kernel_conv", "CV"), ("kernel_check", "K"), ("kernel_pp", "PP"), ("kernel_spec_count", "SC"), ("kernel_budget", "B")]:
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
            raise RuntimeError(f"{endpoint} failed checking cases:\n{output}")
        print(f"PASS A.4 {endpoint}: {len(checks)} cases", flush=True)
    run("compile-native", [str(binary), str(source), "-o", str(WORK / "checks.exe")], env)
    output = run("native-info", [str(WORK / "checks.exe")], env)
    if output not in ('"PASS"', "PASS"):
        raise RuntimeError(f"native INFO failed checking cases:\n{output}")
    print(f"PASS A.4 native INFO: {len(checks)} cases", flush=True)
    inputs = sorted((ROOT / "lib").glob("*.bend")) + [ROOT / name for name in (
        "test/foundation.bend", "test/representation.bend", "test/checking.bend", "test/checking-oracle.ml",
        "dev/checking-cases.py", "dev/evaluation-cases.py", "test/evaluation-oracle.ml", "dev/test-checking.py", "dev/test-checking-mutations.py", "dev/build.py", "dev/pin-check.py", "dev/toolchain.json",
        "dev/house-bend.py", "dev/test-house.py", "dev/bend-policy.json", "Makefile")]
    report = {"milestone": "A.4", "cases_per_endpoint": len(checks), "endpoints": list(endpoints),
              "informational_endpoints": ["native"], "sources": {str(p.relative_to(ROOT)): digest(p) for p in inputs},
              "harness_sha256": digest(source), "kanon_revision": pins["kanon"]["revision"], "oracle": provenance,
              "observations_sha256": digest(WORK / "oracle-results.log"), "case_names": [name for name, _, _ in checks],
              "independent_expectations": sum(fixed is not None for _, _, fixed in checks),
              "r2_qualification": "pending", "stage_a_full_corpus": "pending"}
    (WORK / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
