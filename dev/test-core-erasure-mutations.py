#!/usr/bin/env python3
"""Require ordinary, product and sum erasure tests to reject isolated semantic mutants."""
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
    ("tuple-field-order", "erase/core.bend", "K.KStruct{tid, fields}",
     "K.KStruct{tid, R.rev_append(K.ktm, fields, [])}"),
    ("projection-unfiltered", "erase/core.bend", "projection_offset(checker(c), tys, index, F.Int63.zero)",
     "Done{index}"),
    ("tuple-layout", "erase/type.bend", 'String.concat(["tuple<", K.join(",", texts), ">"])',
     'String.concat(["tuple<", K.join(";", texts), ">"])'),
    ("body-layout-dependencies", "erase/core.bend", "Dep.term(body)", "[]"),
    ("sum-layout", "erase/type.bend", 'K.join("|", texts)', 'K.join(",", texts)'),
    ("sum-tag-number", "erase/core.bend", "K.KTag{tid, index, [field]}", "K.KTag{tid, F.Int63.succ(index), [field]}"),
    ("sum-erased-payload", "erase/sum.bend", "F.choose(Maybe<&2, payload>, keep, Some{Payload{lty, body}}, None{})", "Some{Payload{lty, body}}"),
    ("case-payload-arity", "erase/core.bend", "F.choose(F.Int63.t, keep, F.Int63.one, F.Int63.zero)", "F.Int63.one"),
    ("case-leg-selection", "erase/sum.bend", "F.Int63.equal(k, index)",
     "Bool.or(F.Int63.equal(k, index), Bool.and(F.Int63.equal(k, F.Int63.zero), F.Int63.equal(index, F.Int63.succ(F.Int63.one))))"),
    ("case-tail-lost", "erase/core.bend", "term_request(added(c, q, x, dom, keep), s, tail, Some{target}, body)",
     "term_request(added(c, q, x, dom, keep), s, False{}, Some{target}, body)"),
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
        print(f"PASS erasure mutant: {name}", flush=True)
    (WORK / "result.json").write_text(json.dumps({"mutants": rows}, indent=2) + "\n")


if __name__ == "__main__":
    main()
