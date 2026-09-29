#!/usr/bin/env python3
"""Require isolated recursive-program mutations to change their observations."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("program_mutations", ROOT / "dev/test-program.py")
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
MUTANTS = [
    ("nondecreasing", "test/pinfront/order.bend", "case Some{Principal{}}: False{}", "case Some{Principal{}}: True{}", "program/unguarded"),
    ("declaration-order", "test/pinfront/program.bend", "List.append(&2, F.Pair2<String, G.entry>, rows, out)", "List.append(&2, F.Pair2<String, G.entry>, out, rows)", "program/ordinary-after-group"),
    ("missing-motive", "test/pinfront/order.bend", "case None{}: Fail{E.Missing_branch{motive_msg}}", "case None{}: Done{Unit{}}", "contracts"),
]


def main():
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    binary, _, _ = h.build.compiler(pins)
    before = h.source_hashes()
    baseline = json.loads((ROOT / "dev/validation/stage-a-program.json").read_text())
    if baseline.get("source_sha256") != before or baseline.get("backend") != "all":
        raise RuntimeError("program mutations require a fresh complete baseline")
    if baseline.get("reference_sha256") != h.digest(h.WORK / "reference.json"):
        raise RuntimeError("program mutations require the exact reference used by the baseline")
    checks = h.cases(pins, False)
    reference = json.loads((h.WORK / "reference.json").read_text())
    h.validate_record(reference, pins, checks, replay=True)
    h.independent(checks, reference)
    rows = []
    for name, relative, old, new, target in MUTANTS:
        baseline_argv = [pins["tools"]["bun"]["path"], str(h.WORK / "checks.js")]
        baseline_env = dict(os.environ, BEND_NO_TELEMETRY="1")
        if target == "contracts":
            if h.run(f"baseline-{name}", [*baseline_argv, "contracts"], baseline_env).stdout != b"PASS program contracts: 16\n":
                raise RuntimeError(f"{name}: direct contract baseline failed")
        else:
            index = next(i for i, row in enumerate(checks) if row["name"] == target)
            observed = h.host_observation(f"baseline-{name}", baseline_argv, checks[index], "print", baseline_env)
            if observed != reference["cases"][index]["observations"]["print"]:
                raise RuntimeError(f"{name}: single-mode baseline failed")
        with tempfile.TemporaryDirectory(prefix="sole-comb-program-mutant-") as temporary:
            root = Path(temporary)
            for directory in ("lib", "test/pinfront"):
                shutil.copytree(ROOT / directory, root / directory)
            for source in (ROOT / "test").glob("*.bend"):
                shutil.copyfile(source, root / "test" / source.name)
            path = root / relative
            text = path.read_text()
            if text.count(old) != 1:
                raise RuntimeError(f"{name}: mutation target is not unique")
            path.write_text(text.replace(old, new))
            env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(root / "cache"))
            output = root / "mutant.js"
            h.run(f"mutant-{name}-check", [str(binary), str(root / "test/program-driver.bend"), "--check-only"], env)
            h.run(f"mutant-{name}-compile", [str(binary), str(root / "test/program-driver.bend"), "-o", str(output)], env)
            argv = [pins["tools"]["bun"]["path"], str(output)]
            if target == "contracts":
                actual = h.run(f"mutant-{name}-observe", [*argv, "contracts"], env).stdout
                if b"FAIL program contracts:\nmissing-motive\n\n" != actual:
                    raise RuntimeError(f"{name}: expected only the missing-motive contract failure, got {actual!r}")
            else:
                index = next(i for i, row in enumerate(checks) if row["name"] == target)
                actual = h.host_observation(f"mutant-{name}-observe", argv, checks[index], "print", env)
                if actual == reference["cases"][index]["observations"]["print"]:
                    raise RuntimeError(f"{name}: executable mutation survived")
            rows.append(dict(name=name, target=target, killed=True))
            print(f"PASS program mutation {name}: {target}", flush=True)
    if before != h.source_hashes():
        raise RuntimeError("program baseline changed during mutation checks")
    (ROOT / "dev/validation/stage-a-program-mutations.json").write_text(json.dumps({"schema": 1, "source_sha256": before, "mutants": rows}, indent=2) + "\n")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyError, TypeError, StopIteration, subprocess.SubprocessError) as error:
        print(f"FAIL A.5b.2 mutations: {error}", file=sys.stderr)
        sys.exit(1)
