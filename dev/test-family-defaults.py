#!/usr/bin/env python3
"""Compare family defaults with explicit checked and erased arms on every host."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/family-defaults"
SUGARED = "examples/family-default-arms.sole-comb"
EXPLICIT = "examples/family-default-arms-explicit.sole-comb"
PROBE = "test/family-default-order.sole-comb"
# Hand-derived from the three constructor addresses and the two literal arm bodies.
PROBE_EXPECTED = "\n".join((
    f"CHECK {PROBE} defs=2 ok",
    "rec [mu<B>; nat; leg<mu<B>,0>; leg<mu<B>,1>; leg<mu<B>,2>]",
    "fun all (union mu<B>) : union nat := KCase mu<B> (KVar 0) [{0 0 (KLit 7)}; {1 0 (KLit 7)}; {2 0 (KLit 7)}]",
    "rec [mu<B>; nat; leg<mu<B>,0>; leg<mu<B>,1>; leg<mu<B>,2>]",
    "fun suffix (union mu<B>) : union nat := KCase mu<B> (KVar 0) [{0 0 (KLit 1)}; {1 0 (KLit 2)}; {2 0 (KLit 2)}]",
)) + "\n"
REFUSALS = {
    "family-default": "a default elim arm must cover at least one remaining constructor",
    "family-default-empty": "a default elim arm must cover at least one remaining constructor",
    "family-default-fields": "the elim arm for succ must bind each of its fields",
    "family-default-domain": "mismatch: the annotation of constructor field n differs from its declared type",
    "family-default-result": "mismatch: a tuple needs a right former as its expected type",
    "family-default-linear": "quantity: the linear binder n must be used exactly once on every runtime path",
    "family-default-ghost": "quantity: the erased binder n is read in a runtime position",
    "family-default-excess": "family elim arm 1 of B has no constructor",
    "family-default-nonlast": "after the final default elim arm",
}

def sources():
    paths = {ROOT / path for path in (
        SUGARED, EXPLICIT, PROBE, "Makefile", "dev/test-family-defaults.py", "dev/test-cli.py",
        "bin/main.c", "dev/c.mk", "sole-comb",
        "examples/EXPECTATIONS.json")}
    for pattern in ("*.c", "*.h"):
        paths.update((ROOT / "lib").glob(pattern))
    paths.update(ROOT / f"corpus/refuse/{name}.sole-comb" for name in REFUSALS)
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}


def run(name, argv, cwd=ROOT):
    result = subprocess.run(list(map(str, argv)), cwd=cwd, capture_output=True, text=True, timeout=900)
    (WORK / f"{name}.stdout").write_text(result.stdout)
    (WORK / f"{name}.stderr").write_text(result.stderr)
    return result


def checked(result, path, count):
    if result.returncode != 0 or result.stderr or not result.stdout.startswith(f"CHECK {path} defs={count} ok\n"):
        raise RuntimeError(f"{path}: malformed success or wrong declaration count\n{result.stdout}{result.stderr}")
    return result.stdout.partition("\n")[2]


def refused(result, path, marker):
    if result.returncode != 1 or result.stdout or not result.stderr.startswith(f"CHECK {path} FAIL ") or marker not in result.stderr:
        raise RuntimeError(f"{path}: missing source refusal {marker!r}\n{result.stdout}{result.stderr}")


def observation(result, host, path, mode):
    return {"host": host, "path": path, "mode": mode, "exit": result.returncode,
            "stdout": result.stdout, "stderr": result.stderr}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    host = "c"
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    before = sources()
    observations = []
    for mode in ("--print", "--erased"):
        outputs = []
        for path in (SUGARED, EXPLICIT):
            result = run(f"{host}-{mode[2:]}-{Path(path).stem}", [ROOT / "sole-comb", "check", mode, path])
            outputs.append(checked(result, path, 11))
            observations.append(observation(result, host, path, mode))
        if outputs[0] != outputs[1]:
            raise RuntimeError(f"{host} {mode}: default arms differ from the independent explicit expansion")
        for name, marker in REFUSALS.items():
            path = f"corpus/refuse/{name}.sole-comb"
            result = run(f"{host}-{mode[2:]}-{name}", [ROOT / "sole-comb", "check", mode, path])
            refused(result, path, marker)
            observations.append(observation(result, host, path, mode))
    probe = run(f"{host}-order", [ROOT / "sole-comb", "check", "--erased", PROBE])
    checked(probe, PROBE, 2)
    if probe.stdout != PROBE_EXPECTED:
        raise RuntimeError(f"{host}: constructor address or literal body differs from the hand-derived probe\n{probe.stdout}")
    observations.append(observation(probe, host, PROBE, "--erased"))
    print(f"PASS family defaults {host}: checked/erased expansion, {len(REFUSALS)} refusals, literal order probe", flush=True)
    if sources() != before:
        raise RuntimeError("family default sources changed during validation")
    record = {"schema": 1, "scope": "native family defaults through existing checked constructor arms",
              "sources": before, "definitions": 11, "refusals": len(REFUSALS),
              "observations": observations,
              "oracle": "independent explicit expansions and hand-derived erased order probe"}
    (WORK / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"FAMILY-DEFAULTS PASS observations={len(observations)}", flush=True)


if __name__ == "__main__":
    main()
