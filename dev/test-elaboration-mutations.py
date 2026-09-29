#!/usr/bin/env python3
"""Require executable elaborator mutations to disagree with the pinned oracle."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("elaboration", ROOT / "dev/test-elaboration.py")
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)

MUTANTS = [
    ("local-lookup", "String.eq(x, y)", "False{}", "probe/local-shadow"),
    ("second-projection", "R.proj_branch(q, second)", "R.proj_branch(q, False{})", "probe/pair-projection-2"),
    ("field-annotation", "K.conv_type(c, actual, expected)", "Done{True{}}", "probe/field-annotation-refused"),
]


def main():
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    binary, _, _ = h.build.compiler(pins)
    checks = h.cases(pins, False)
    observations, _ = h.replay(pins, checks, h.WORK / "reference.json")
    h.independent(checks, observations)
    before = h.source_hashes()
    baseline = json.loads((ROOT / "dev/validation/stage-a-elaboration.json").read_text())
    if (baseline.get("source_sha256") != before or baseline.get("backend") != "all"
            or baseline.get("agree") != len(checks) or baseline.get("divergences") != []
            or baseline.get("reference_sha256") != h.digest(h.WORK / "reference.json")):
        raise RuntimeError("run the complete elaboration gate on these sources before mutations")
    directory = ROOT / "_build/elaboration-mutations"
    directory.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    records = []
    for name, original, replacement, case in MUTANTS:
        selected = [(row, value) for row, value in zip(checks, observations) if row["name"] == case]
        if len(selected) != 1:
            raise RuntimeError(f"{name}: killing case is missing or duplicated")
        row, expected = selected[0]
        work = Path(tempfile.mkdtemp(prefix=name + "-", dir=directory))
        shutil.copytree(ROOT / "lib", work / "lib")
        shutil.copytree(ROOT / "test/pinfront", work / "test/pinfront")
        for filename in ("elaboration.bend", "elaboration-driver.bend"):
            shutil.copyfile(ROOT / "test" / filename, work / "test" / filename)
        path = work / "test/pinfront/elab.bend"
        source = path.read_text()
        if source.count(original) != 1:
            raise RuntimeError(f"{name}: mutation no longer selects exactly one site")
        path.write_text(source.replace(original, replacement))
        output = work / "checks.js"
        compile_command = [str(binary), str(work / "test/elaboration-driver.bend"), "-o", str(output)]
        h.run(f"mutations/{name}-compile", compile_command, env, work)
        command = [pins["tools"]["bun"]["path"], str(output), case, *h.pf.native_args(row["source"])]
        actual = h.pf.parse_native(h.run(f"mutations/{name}-run", command, env, work), [row])[0]
        if actual == expected:
            raise RuntimeError(f"{name}: executable mutation survived {case}")
        records.append({"name": name, "case": case, "caught": True,
                        "source_sha256": h.digest(path), "expected_hex": expected.hex(), "actual_hex": actual.hex()})
        print(f"PASS A.5b.1 mutation {name}: {case}", flush=True)
    if before != h.source_hashes():
        raise RuntimeError("baseline elaboration sources changed during mutation testing")
    result = {"schema": 1, "scope": h.SCOPE, "source_sha256": before,
              "reference_sha256": h.digest(h.WORK / "reference.json"), "backend": "bun",
              "mutations": records, "caught": len(records)}
    (ROOT / "dev/validation/stage-a-elaboration-mutations.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"FAIL A.5b.1 mutations: {error}", file=sys.stderr)
        sys.exit(1)
