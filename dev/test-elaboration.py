#!/usr/bin/env python3
"""A.5b.1 declaration differential, preceding the full program KANON-DIFF."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/elaboration"
REFERENCE = ROOT / "dev/validation/stage-a-elaboration-reference.json"
EXPECTED_PROBES = 75
EXPECTED_FIXED = 65
SCOPE = "A.5b.1 expressions and declarations; recursive program groups and full KANON-DIFF pending"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


pf = module("elaboration_pinfront", ROOT / "dev/test-pinfront.py")
build = module("elaboration_build", ROOT / "dev/build.py")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(name, argv, env, cwd=ROOT, timeout=300):
    dest = WORK / name
    dest.parent.mkdir(parents=True, exist_ok=True)
    try:
        result = subprocess.run(argv, cwd=cwd, env=env, capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        dest.with_suffix(".stdout.log").write_bytes(error.stdout or b"")
        dest.with_suffix(".stderr.log").write_bytes(error.stderr or b"")
        raise RuntimeError(f"{name} exceeded {timeout}s; see {dest}.stderr.log") from error
    dest.with_suffix(".stdout.log").write_bytes(result.stdout)
    dest.with_suffix(".stderr.log").write_bytes(result.stderr)
    if result.returncode:
        raise RuntimeError(f"{name} failed ({result.returncode}); see {dest}.stderr.log\n" + result.stderr[-2500:].decode(errors="replace"))
    return result.stdout


def cases(pins, verify_upstream=True):
    corpus = pf.corpus(pins, verify_upstream)
    probes = module("elaboration_cases", ROOT / "dev/elaboration-cases.py").cases()
    if len(corpus) != 146 or len(probes) != EXPECTED_PROBES:
        raise RuntimeError("elaboration case counts changed")
    if sum("expected" in r or "prefix" in r for r in probes) != EXPECTED_FIXED:
        raise RuntimeError("independent elaboration expectation count changed")
    rows = corpus + probes
    if len({r["name"] for r in rows}) != len(rows):
        raise RuntimeError("duplicate elaboration cases")
    return rows


def inputs():
    names = ["dev/test-elaboration.py", "dev/elaboration-cases.py", "test/elaboration-oracle.ml",
             "dev/test-pinfront.py", "test/kanon/SOURCE.json", "test/kanon/SURVIVORS.tsv"]
    return {name: digest(ROOT / name) for name in names}


def source_hashes():
    names = list(inputs()) + ["Makefile", "dev/build.py", "dev/bend-policy.json", "dev/house-bend.py", "dev/test-elaboration-mutations.py",
                             "test/elaboration.bend", "test/elaboration-driver.bend", "dev/toolchain.json"]
    paths = [ROOT / n for n in names]
    paths += sorted((ROOT / "lib").glob("*.bend")) + sorted((ROOT / "test/pinfront").glob("*.bend"))
    return {p.relative_to(ROOT).as_posix(): digest(p) for p in paths}


def identity(checks):
    return [{"name": r["name"], "source_sha256": hashlib.sha256(r["source"]).hexdigest()} for r in checks]


def oracle(pins, checks, env):
    upstream, revision = pf.pin_state(pins)
    files = sorted((upstream / "lib").glob("*.ml")) + sorted((upstream / "lib").glob("*.mli"))
    files += [upstream / "surface" / f"{name}.ml" for name in ("token", "syntax", "lexer", "parser", "elab")]
    dest = WORK / "oracle"
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    hashes = {}
    for path in files:
        name = path.relative_to(upstream).as_posix()
        content = path.read_bytes()
        if content != pf.git(upstream, "show", f"{revision}:{name}"):
            raise RuntimeError(f"oracle source differs from pin: {name}")
        (dest / path.name).write_bytes(content)
        hashes[name] = hashlib.sha256(content).hexdigest()
    names = sorted(p.stem.capitalize() for p in files if p.parent.name == "lib" and p.suffix == ".ml")
    (dest / "kanon_kernel.ml").write_text("\n".join(f"module {name} = {name}" for name in names) + "\n")
    calls = [f"let () = emit {pf.ocaml_string(r['name'].encode())} {pf.ocaml_string(r['source'])}" for r in checks]
    (dest / "oracle_cases.ml").write_text((ROOT / "test/elaboration-oracle.ml").read_text() + "\n" + "\n".join(calls) + "\n")
    order = run("oracle-order", ["ocamldep", "-sort", *[p.name for p in files], "kanon_kernel.ml"], env, dest).decode().split()
    run("oracle-compile", ["ocamlfind", "ocamlc", "-package", "zarith", "-linkpkg", *order, "oracle_cases.ml", "-o", "oracle.exe"], env, dest)
    observations = pf.parse_rows(run("oracle-results", [str(dest / "oracle.exe")], env), checks)
    pf.pin_state(pins)
    record = {"schema": 1, "scope": SCOPE, "revision": revision, "inputs": inputs(),
              "oracle_sources": hashes, "cases": [dict(row, observation=value.hex()) for row, value in zip(identity(checks), observations)]}
    (WORK / "reference.json").write_text(json.dumps(record, indent=2) + "\n")
    compare_reference(record)
    return observations, record


def compare_reference(record, path=REFERENCE):
    # Input hashes change with the harness, so only the per-case observations must stay equal.
    fields = ("name", "source_sha256", "observation")
    try:
        saved = [[case[key] for key in fields] for case in json.loads(path.read_text())["cases"]]
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise RuntimeError(f"saved reference is missing or malformed: {path}") from error
    fresh = [[case[key] for key in fields] for case in record["cases"]]
    changed = [new[0] for new, old in zip(fresh, saved) if new != old]
    if changed or len(fresh) != len(saved):
        where = changed[0] if changed else "the case count"
        raise RuntimeError(f"fresh oracle cases differ from the saved reference at {where}; "
                           f"review {WORK / 'reference.json'} and copy it to {path}")


def replay(pins, checks, path=REFERENCE):
    record = json.loads(path.read_text())
    if record.get("schema") != 1 or record.get("scope") != SCOPE or record.get("revision") != pins["kanon"]["revision"] or record.get("inputs") != inputs():
        raise RuntimeError("elaboration reference metadata or input hashes changed")
    rows = record.get("cases", [])
    if [{k: row.get(k) for k in ("name", "source_sha256")} for row in rows] != identity(checks):
        raise RuntimeError("elaboration reference cases changed")
    values = []
    for row in rows:
        encoded = row.get("observation")
        if not isinstance(encoded, str) or re.fullmatch(r"(?:[0-9a-f]{2})*", encoded) is None:
            raise RuntimeError("malformed elaboration reference observation")
        values.append(bytes.fromhex(encoded))
    return values, record


def independent(checks, observations):
    for row, observation in zip(checks, observations):
        if "expected" in row and observation != row["expected"].encode():
            raise RuntimeError(f"fixed elaboration expectation failed: {row['name']}: {observation!r}")
        if "prefix" in row and not observation.startswith(row["prefix"].encode()):
            raise RuntimeError(f"fixed elaboration verdict failed: {row['name']}: {observation!r}")


def compare(name, actual, observations, checks):
    failures = [{"name": row["name"], "expected": expected.decode("utf8", errors="replace"), "actual": value.decode("utf8", errors="replace")}
                for row, expected, value in zip(checks, observations, actual) if expected != value]
    if len(actual) != len(checks):
        raise RuntimeError(f"{name} elaboration row count differs")
    if failures:
        (WORK / f"{name}-failures.json").write_text(json.dumps(failures, indent=2) + "\n")
        raise RuntimeError(f"{name} elaboration differs in {len(failures)} cases; see {WORK / (name + '-failures.json')}")
    print(f"PASS A.5b.1 {name}: {len(checks)} cases", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--replay", action="store_true")
    parser.add_argument("--backend", choices=("all", "bun"), default="all")
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    before = source_hashes()
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    binary, _, _ = build.compiler(pins)
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    checks = cases(pins, not args.replay)
    observations, _ = replay(pins, checks) if args.replay else oracle(pins, checks, env)
    independent(checks, observations)
    print(f"PASS A.5b.1 oracle: {len(checks)} cases, {EXPECTED_FIXED} independent expectations", flush=True)
    source = ROOT / "test/elaboration-driver.bend"
    run("check", [str(binary), str(source), "--check-only"], env)
    run("compile-js", [str(binary), str(source), "-o", str(WORK / "checks.js")], env)
    endpoints = {"bun": [pins["tools"]["bun"]["path"], str(WORK / "checks.js")]}
    if args.backend == "all":
        endpoints["node-worker"] = [pins["tools"]["node"]["path"], "-e", build.node_worker_script(pins), str(WORK / "checks.js")]
    for name, argv in endpoints.items():
        actual = [pf.parse_native(run(f"{name}/{i:04d}", [*argv, r["name"], *pf.native_args(r["source"])], env), [r])[0] for i, r in enumerate(checks)]
        compare(name, actual, observations, checks)
    if args.backend == "all":
        native = WORK / "checks-native.c"
        run("emit-native", [str(binary), str(source), "-o", str(native)], env)
        cc = shutil.which(os.environ.get("SOLE_COMB_CC", os.environ.get("CC", "clang")))
        if cc is None:
            raise RuntimeError("native elaboration checks require clang 14 or newer")
        cc_version = run("cc-version", [cc, "--version"], env).decode().strip()
        version = re.match(r"(?:Apple )?(?:\w+ )?clang version (\d+)", cc_version)
        if version is None or int(version[1]) < 14:
            raise RuntimeError("native elaboration checks require clang 14 or newer")
        executable = WORK / "checks-native.exe"
        run("compile-native", [cc, "-std=c11", "-O3", str(native), "-lpthread", "-lm", "-o", str(executable)], env, timeout=900)
        actual = [pf.parse_native(run(f"native/{i:04d}", [str(executable), "--", r["name"], *pf.native_args(r["source"])], env), [r])[0] for i, r in enumerate(checks)]
        compare("native INFO", actual, observations, checks)
    if before != source_hashes():
        raise RuntimeError("elaboration sources changed during validation")
    result = {"schema": 1, "scope": SCOPE, "cases": len(checks), "corpus": 146, "probes": EXPECTED_PROBES,
              "independent": EXPECTED_FIXED, "backend": args.backend, "source_sha256": before,
              "reference_sha256": digest(REFERENCE if args.replay else WORK / "reference.json"),
              "agree": len(checks), "divergences": []}
    destination = ROOT / "dev/validation/stage-a-elaboration.json" if args.backend == "all" else WORK / "bun-result.json"
    destination.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"FAIL A.5b.1: {error}", file=sys.stderr)
        sys.exit(1)
