#!/usr/bin/env python3
"""Compare program behavior with immutable pinned Kanon fixtures."""
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
WORK = ROOT / "_build/program"
REFERENCE = ROOT / "dev/validation/stage-a-program-reference.json"
MODES = ("check", "print")
EXPECTED_PROBES = 99
SCOPE = "A.5b.2 recursive programs and check/print observations; erased mode and full KANON-DIFF pending"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


pf = module("program_pinfront", ROOT / "dev/test-pinfront.py")
build = module("program_build", ROOT / "dev/build.py")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(name, argv, env, cwd=ROOT, timeout=300, allow_refusal=False):
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
    if result.returncode not in ((0, 1) if allow_refusal else (0,)):
        raise RuntimeError(f"{name} failed ({result.returncode}); see {dest}.stderr.log\n" + result.stderr[-2500:].decode(errors="replace"))
    return result


def cases(pins, verify_upstream=True):
    corpus = pf.corpus(pins, verify_upstream)
    legacy = module("program_legacy_cases", ROOT / "dev/elaboration-cases.py").cases()
    probes = []
    for old in legacy:
        row = {k: old[k] for k in ("name", "source")}
        expected = old.get("expected", old.get("prefix", ""))
        if expected.startswith(("ok:", "error:")):
            row["code"] = int(expected.startswith("error:"))
        if "expected" in old and expected.startswith("error:"):
            row["stderr"] = expected[6:] + "\n"
        if row["name"] == "probe/recursive-declaration":
            row["code"] = 0
            row.pop("stderr", None)
        probes.append(row)
    probes += module("program_cases", ROOT / "dev/program-cases.py").cases()
    rows = corpus + probes
    if len(corpus) != 146 or len(legacy) != 75 or len(probes) != EXPECTED_PROBES or len({r["name"] for r in rows}) != len(rows):
        raise RuntimeError("program corpus/probe count or unique names changed")
    return rows


def inputs():
    names = ["dev/test-program.py",
             "dev/program-cases.py",
             "dev/elaboration-cases.py",
             "dev/test-pinfront.py",
             "test/kanon/SOURCE.json",
             "test/kanon/SURVIVORS.tsv",
             "dev/reference-fixtures.py",
             "dev/reference-fixtures/manifest.json",
             "dev/reference-fixtures/program.json",
             "dev/validation/stage-a-program-reference.json"]
    return {name: digest(ROOT / name) for name in names}


def source_hashes():
    names = list(inputs()) + ["Makefile", "dev/build.py", "dev/toolchain.json", "dev/bend-policy.json", "dev/house-bend.py",
                             "dev/test-program-mutations.py", "dev/test-program-harness.py", "test/program.bend",
                             "test/program-driver.bend", "test/program-contract.bend", "test/elaboration-driver.bend", "test/elaboration.bend"]
    paths = [ROOT / n for n in names]
    paths += sorted((ROOT / "lib").glob("*.bend")) + sorted((ROOT / "test/pinfront").glob("*.bend"))
    paths += sorted((ROOT / "test/kanon").rglob("*.kan"))
    return {p.relative_to(ROOT).as_posix(): digest(p) for p in paths}


def identity(checks):
    return [{"name": r["name"], "source_sha256": hashlib.sha256(r["source"]).hexdigest()} for r in checks]


def triple(code, stdout, stderr):
    if type(code) is not int or code not in (0, 1) or (code == 0 and stderr) or (code == 1 and stdout):
        raise RuntimeError("invalid program result channels")
    return {"exit": code, "stdout": stdout.hex(), "stderr": stderr.hex()}


def decode(value):
    text = value.decode("utf8")
    fields = text.split(":", 2)
    if len(fields) != 3 or fields[0] not in ("0", "1") or re.fullmatch(r"0|[1-9][0-9]*", fields[1]) is None:
        raise RuntimeError("malformed program observation")
    count = int(fields[1])
    if count > len(fields[2]):
        raise RuntimeError("truncated program observation")
    return triple(int(fields[0]), fields[2][:count].encode(), fields[2][count:].encode())


def decode_both(value):
    text = value.decode("utf8")
    fields = text.split(":", 1)
    if len(fields) != 2 or re.fullmatch(r"0|[1-9][0-9]*", fields[0]) is None:
        raise RuntimeError("malformed program mode pair")
    count = int(fields[0])
    if count > len(fields[1]):
        raise RuntimeError("truncated program mode pair")
    return {"check": decode(fields[1][:count].encode()), "print": decode(fields[1][count:].encode())}


def validate_record(record, pins, checks, replay=False):
    if (record.get("schema"), record.get("scope"), record.get("revision"), record.get("modes")) != (1, SCOPE, pins["kanon"]["revision"], list(MODES)):
        raise RuntimeError("program reference metadata changed")
    if replay and record.get("inputs") != inputs():
        raise RuntimeError("program replay input hashes changed")
    rows = record.get("cases", [])
    if [{k: r.get(k) for k in ("name", "source_sha256")} for r in rows] != identity(checks):
        raise RuntimeError("program reference case identities changed")
    for row in rows:
        if set(row.get("observations", {})) != set(MODES):
            raise RuntimeError("program reference modes changed")
        for value in row["observations"].values():
            if not isinstance(value, dict) or set(value) != {"exit", "stdout", "stderr"}:
                raise RuntimeError("malformed program reference result")
            for key in ("stdout", "stderr"):
                if not isinstance(value[key], str) or re.fullmatch(r"(?:[0-9a-f]{2})*", value[key]) is None:
                    raise RuntimeError("malformed program reference bytes")
            triple(value["exit"], bytes.fromhex(value["stdout"]), bytes.fromhex(value["stderr"]))


def oracle(pins, checks, env):
    reference = module("sole_reference", ROOT / "dev/reference-fixtures.py")
    observations, provenance = reference.observations(pins, "program", checks)
    record = {"schema": 1, "scope": SCOPE, "revision": pins["kanon"]["revision"], "modes": list(MODES),
              "inputs": inputs(), "fixture": provenance,
              "cases": [dict(row, observations=value) for row, value in zip(identity(checks), observations)]}
    validate_record(record, pins, checks, True)
    (WORK / "reference.json").write_text(json.dumps(record, indent=2) + "\n")
    return record


def independent(checks, record):
    for row, saved in zip(checks, record["cases"]):
        for mode in MODES:
            value = saved["observations"][mode]
            fields = {"exit": row["code"]} if "code" in row else {}
            if "stderr" in row:
                fields["stderr"] = row["stderr"].encode().hex()
            if mode == "print" and "print_stdout" in row:
                fields["stdout"] = row["print_stdout"].encode().hex()
            if any(value[k] != v for k, v in fields.items()):
                raise RuntimeError(f"independent expectation failed: {row['name']} {mode}: {value}")


def host_observation(name, argv, row, mode, env):
    timeout = 1800 if len(row["source"]) > 65536 else 300
    transport = run(name, [*argv, row["name"], mode, *pf.native_args(row["source"])], env, timeout=timeout)
    if transport.stderr:
        raise RuntimeError(f"unexpected host transport stderr: {name}")
    value = pf.parse_native(transport.stdout, [row])[0]
    return decode_both(value) if mode == "both" else decode(value)


def compare_host(name, argv, checks, record, env):
    contract = run(f"{name}/contracts", [*argv, "contracts"], env)
    if contract.stdout != b"PASS program contracts: 16\n" or contract.stderr:
        raise RuntimeError(f"{name} direct contracts failed: {contract.stdout!r} {contract.stderr!r}")
    failures = []
    for i, (row, saved) in enumerate(zip(checks, record["cases"])):
        observed = host_observation(f"{name}/{i:04d}-both", argv, row, "both", env)
        for mode in MODES:
            actual = observed[mode]
            expected = saved["observations"][mode]
            if actual != expected:
                failures.append(dict(name=row["name"], mode=mode, expected=expected, actual=actual))
        if len(row["source"]) > 65536:
            print(f"CHECKED A.5b.2 {name} large input {row['name']}", flush=True)
    if failures:
        path = WORK / f"{name}-failures.json"
        path.write_text(json.dumps(failures, indent=2) + "\n")
        raise RuntimeError(f"{name} differs in {len(failures)} observations; see {path}")
    print(f"PASS A.5b.2 {name}: {len(checks)} files, {len(MODES)} modes", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--replay", action="store_true", help="compatibility flag; recorded pinned fixtures are always used")
    parser.add_argument("--backend", choices=("all", "bun"), default="all")
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    before = source_hashes()
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    binary, _, _ = build.compiler(pins)
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    checks = cases(pins, False)
    record = oracle(pins, checks, env)
    validate_record(record, pins, checks, True)
    independent(checks, record)
    print(f"PASS A.5b.2 reference fixtures: {len(checks)} files, {len(MODES)} modes", flush=True)
    source = ROOT / "test/program-driver.bend"
    run("check", [str(binary), str(source), "--check-only"], env)
    run("compile-js", [str(binary), str(source), "-o", str(WORK / "checks.js")], env)
    endpoints = {"bun": [pins["tools"]["bun"]["path"], str(WORK / "checks.js")]}
    if args.backend == "all":
        endpoints["node-worker"] = [pins["tools"]["node"]["path"], "-e", build.node_worker_script(pins), str(WORK / "checks.js")]
    for name, argv in endpoints.items():
        compare_host(name, argv, checks, record, env)
    if args.backend == "all":
        native = WORK / "checks-native.c"
        run("emit-native", [str(binary), str(source), "-o", str(native)], env, timeout=1800)
        cc = shutil.which(os.environ.get("SOLE_COMB_CC", os.environ.get("CC", "clang")))
        if cc is None:
            raise RuntimeError("native program checks require clang 14 or newer")
        version = run("cc-version", [cc, "--version"], env).stdout.decode()
        parsed = re.match(r"(?:Apple )?(?:\w+ )?clang version (\d+)", version)
        if parsed is None or int(parsed[1]) < 14:
            raise RuntimeError("native program checks require clang 14 or newer")
        executable = WORK / "checks-native.exe"
        run("compile-native", [cc, "-std=c11", "-O3", str(native), "-lpthread", "-lm", "-o", str(executable)], env, timeout=900)
        compare_host("native", [str(executable), "--"], checks, record, env)
    if before != source_hashes() or identity(checks) != identity(cases(pins, False)):
        raise RuntimeError("program sources changed during validation")
    result = {"schema": 1, "scope": SCOPE, "cases": len(checks), "corpus": 146, "probes": EXPECTED_PROBES,
              "independent": sum("code" in r for r in checks), "modes": list(MODES), "backend": args.backend,
              "source_sha256": before, "reference_sha256": digest(WORK / "reference.json"),
              "agree": len(checks) * len(MODES), "divergences": []}
    dest = ROOT / "dev/validation/stage-a-program.json" if args.backend == "all" else WORK / "bun-result.json"
    dest.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f"FAIL A.5b.2: {error}", file=sys.stderr)
        sys.exit(1)
