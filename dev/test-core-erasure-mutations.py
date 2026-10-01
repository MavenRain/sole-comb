#!/usr/bin/env python3
"""Require ordinary erasure tests to reject isolated semantic mutants."""
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/core-erasure-mutations"
MUTANTS = (
    ("zero-kept", "erase/type.bend", "Q.equal(q, Q.Zero{})", "False{}"),
    ("proof-kept", "erase/type.bend", "Bool.not(L.equal(l, L.zero))", "True{}"),
    ("index-shift", "erase/core.bend", "K.KVar{i}", "K.KVar{F.Int63.succ(i)}"),
    ("capture-not-pruned", "erase/core.bend", "RT.prune_captures(ps, args, count(K.repr, params), body)",
     "RT.Done{RT.Pruned{ps, args, body}}"),
)


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    rows = []
    for name, path, old, new in MUTANTS:
        dest = WORK / name
        if dest.exists():
            shutil.rmtree(dest)
        shutil.copytree(ROOT, dest, ignore=shutil.ignore_patterns(".git", "_build", ".gatework", ".kanon*", "__pycache__"))
        source = dest / path
        text = source.read_text()
        if text.count(old) != 1:
            raise RuntimeError(f"{name}: mutation anchor is not unique")
        source.write_text(text.replace(old, new))
        result = subprocess.run([sys.executable, "-P", str(dest / "dev/test-core-erasure.py"), "--hosts", "bun"],
                                cwd=dest, capture_output=True, timeout=900)
        (WORK / f"{name}.stdout").write_bytes(result.stdout)
        (WORK / f"{name}.stderr").write_bytes(result.stderr)
        error = result.stderr.decode(errors="replace")
        if result.returncode == 0 or not any(marker in error for marker in (
                "erased public output differs from the pinned oracle", "erasure contracts failed")):
            raise RuntimeError(f"{name}: mutant did not fail a semantic comparison\n{error}")
        rows.append({"name": name, "path": path, "exit": result.returncode, "semantic_rejection": True})
        print(f"PASS ordinary erasure mutant: {name}", flush=True)
    (WORK / "result.json").write_text(json.dumps({"mutants": rows}, indent=2) + "\n")


if __name__ == "__main__":
    main()
