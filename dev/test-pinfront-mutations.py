#!/usr/bin/env python3
"""Five behavioral mutations for A.5a parsing, separate from A.close kernel mutants."""
import json
import os
from pathlib import Path
import shutil
import tempfile

import importlib.util

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("pinfront", ROOT / "dev/test-pinfront.py")
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)

MUTANTS = [
    ("match-kind", "parser.bend", "ParseCaseTerm{FiberedMatch{}, rest}", "ParseCaseTerm{LegacyCase{}, rest}", "match-empty"),
    ("numeric-bound", "parser.bend", "> 18n", "> 19n", "host-bound-overflow"),
    ("byte-position", "token.bend", "(col + n : Nat)", "(col + 1n : Nat)", "identity"),
    ("constructor-list", "parser.bend", "L.reverse(S.fam_ctor, acc, [])", "[]", "second-constructor"),
    ("utf8-byte", "lexer.bend", "(192 + c / 64 : U32)", "(193 + c / 64 : U32)", "bytes-utf8"),
]


def main():
    result = h.WORK / "mutations.json"
    result.unlink(missing_ok=True)
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    build = h.module("sole_build", ROOT / "dev/build.py")
    binary, _, _ = build.compiler(pins)
    checks = h.cases(pins, False)
    observations, _ = h.replay(pins, checks, h.WORK / "reference.json")
    h.independent(checks, observations)
    original = h.source_hashes()
    directory = ROOT / ".gatework/pinfront-mutants"
    directory.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    records = []
    for name, filename, before, after, case in MUTANTS:
        work = Path(tempfile.mkdtemp(prefix=name + "-", dir=directory))
        shutil.copytree(ROOT / "lib", work / "lib")
        shutil.copytree(ROOT / "test/pinfront", work / "test/pinfront")
        path = work / "test/pinfront" / filename
        source = path.read_text()
        if source.count(before) != 1:
            raise RuntimeError(f"{name}: mutation no longer selects exactly one site")
        path.write_text(source.replace(before, after))
        selected = [(row, value) for row, value in zip(checks, observations) if row["name"] == case]
        if len(selected) != 1:
            raise RuntimeError(f"{name}: killing case is missing or duplicated")
        output = work / "_build/pinfront"
        output.mkdir(parents=True)
        harness = output / "checks.bend"
        h.harness([selected[0][0]], harness)
        h.run(name + "-compile", [str(binary), str(harness), "-o", str(output / "checks.js")], env, work)
        observed = h.run(name + "-run", [pins["tools"]["bun"]["path"], str(output / "checks.js")], env, work)
        if h.mismatches(observed, [selected[0][0]], [selected[0][1]]) != [case]:
            raise RuntimeError(f"{name}: expected a behavioral mismatch in {case}")
        records.append({"name": name, "file": filename, "before": before, "after": after,
                        "case": case, "status": "KILLED", "source_sha256": h.digest(path),
                        "harness_sha256": h.digest(harness),
                        "output_sha256": h.digest(h.WORK / f"{name}-run.stdout.log")})
        print(f"PASS A.5a mutation {name}: killed by {case}", flush=True)
    if original != h.source_hashes():
        raise RuntimeError("production sources changed during mutations")
    result.write_text(json.dumps({"schema": 1, "sources": original, "mutants": records}, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
