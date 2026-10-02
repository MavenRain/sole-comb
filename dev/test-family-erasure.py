#!/usr/bin/env python3
"""Compare recursive-family erasure with frozen pinned Kanon CLI observations."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/family-erasure"
FIXTURES = (
    "test/family-erasure.kan",
    "test/family-erasure-runtime.kan",
    "test/kanon/test/fixtures/mu-dependent-layout.kan",
    "test/kanon/test/fixtures/mu-parameter-layout.kan",
    "test/kanon/test/fixtures/mu-indexed.kan",
    "test/kanon/test/fixtures/mu-empty-large-elim.kan",
)
# Each golden is checked only against the recorded output of its own fixture.
GOLDENS = {
    "test/family-erasure.kan": (
        "fun first () : union mu<N> := KTag mu<N> 0 []",
        "fun second () : union mu<N> := KTag mu<N> 1 [KTag mu<N> 0 []]",
        "fun reverse (union mu<N>) : union nat := KCase mu<N> (KVar 0) [{0 0 (KLit 1)}; {1 1 (KLit 2)}]",
        "fun ghostValue () : union mu<Ghost> := KTag mu<Ghost> 0 [KLit 7]",
        "fun ghostRead (union mu<Ghost>) : union nat := KCase mu<Ghost> (KVar 0) [{0 1 (KVar 0)}]",
        "fun afterGhost (union nat, union mu<Ghost>) : union nat := KCase mu<Ghost> (KVar 0) [{0 1 (KTail (KGlobal natAdd) [KVar 2; KVar 0])}]",
        "fun capture$0 (union nat, union nat, union nat) : union nat := KTail (KGlobal natAdd) [KVar 2; KApp (KGlobal natAdd) [KVar 1; KVar 0]]",
        "fun capture (union nat, union mu<Ghost>, union nat) : union nat := KTail (KCase mu<Ghost> (KVar 1) [{0 1 (KClos capture$0 1 [KVar 3; KVar 0])}]) [KVar 0]",
        "fun boxRead (union mu<Box>) : union nat := KCase mu<Box> (KVar 0) [{0 1 (KLit 5)}]",
        "fun head (union mu<V>) : union nat := KCase mu<V> (KVar 0) [{0 0 (KLit 0)}; {1 2 (KVar 1)}]",
        "fun copy (union mu<V>) : union mu<V> := KCase mu<V> (KVar 0) [{0 0 (KTag mu<V> 0 [])}; {1 2 (KTag mu<V> 1 [KVar 1; KVar 0])}]",
        "rec [mu<Left>; mu<Right>; leg<mu<Left>,0>; leg<mu<Left>,1,union mu<Right>>; leg<mu<Right>,0,union mu<Left>>]",
        "fun mutualValue () : union mu<Left> := KTag mu<Left> 1 [KTag mu<Right> 0 [KTag mu<Left> 0 []]]",
        "fun rightRead (union mu<Right>) : union mu<Left> := KCase mu<Right> (KVar 0) [{0 1 (KVar 0)}]",
        "axiom empty : union mu<Empty>",
        "fun emptyRead () : union nat := KCase mu<Empty> (KGlobal empty) []",
        "rec [mu<N>; leg<mu<N>,0>; leg<mu<N>,1,union mu<N>>]",
    ),
    "test/family-erasure-runtime.kan": (
        "fun rpRead (union mu<RP>) : union nat := KCase mu<RP> (KVar 0) [{0 1 (KVar 0)}]",
        "fun scrutApp (union nat) : union nat := KCase mu<N> (KApp (KGlobal mkN) [KVar 0]) [{0 0 (KLit 0)}; {1 1 (KLit 1)}]",
        "fun nonTail (union mu<N>) : union nat := KTail (KGlobal natAdd) [KLit 1; KCase mu<N> (KVar 0) [{0 0 (KApp (KGlobal natAdd) [KLit 2; KLit 3])}; {1 1 (KLit 4)}]]",
        "rec [mu<N>; mu<Ghost>; leg<mu<N>,0>; leg<mu<N>,1,union mu<N>>; leg<mu<Ghost>,0,union nat>]",
        "fun tagClosure$0 (union mu<N>) : union mu<N> := KCase mu<N> (KVar 0) [{0 0 (KTag mu<N> 1 [KTag mu<N> 0 []])}; {1 1 (KVar 0)}]",
        "fun tagClosure (union mu<Ghost>, union mu<N>) : union mu<N> := KTail (KCase mu<Ghost> (KVar 1) [{0 1 (KClos tagClosure$0 1 [])}]) [KVar 0]",
        "fun firstOf (union mu<Two>) : union nat := KCase mu<Two> (KVar 0) [{0 2 (KVar 1)}]",
    ),
    "test/kanon/test/fixtures/mu-dependent-layout.kan": (
        "fun pn () : union mu<Pack> := KTag mu<Pack> 0 [KLit 7; KLit 8]",
        "fun pu () : union mu<Pack> := KTag mu<Pack> 0 [KErased; KLit 9]",
        "fun pf () : union mu<Pack> := KTag mu<Pack> 0 [KClos pf$0 1 []; KLit 10]",
        "fun last (union mu<Pack>) : union nat := KCase mu<Pack> (KVar 0) [{0 2 (KVar 0)}]",
    ),
}
RUNTIME = "test/family-erasure-runtime.kan"
MUTANTS = (
    ("constructor-tag", "erase/mu.bend", "position(key, names, F.Int63.zero)", "position(key, names, F.Int63.one)", FIXTURES[0]),
    # This site is in erase/family.bend, which this increment does not change; it still guards the shared layout.
    ("ghost-field", "erase/family.bend", "field_repr(c, tyv, keep)", "field_repr(c, tyv, True{})", FIXTURES[0]),
    # Kept and erased binders of `last` are not symmetric, so a reversed binder list changes its KVar.
    ("binder-order", "erase/mu.bend", "Binder{q, x, dom, keep} <> bs1}", "List.append(&2, binder, bs1, [Binder{q, x, dom, keep}])}", FIXTURES[2]),
    ("branch-tag", "erase/core.bend", "names, F.Int63.zero, branches, []", "names, F.Int63.one, branches, []", FIXTURES[0]),
    ("constructor-group", "erase/core.bend", "with_groups(s1, group)", "s1", FIXTURES[0]),
    ("lifted-groups", "erase/core.bend", "ret, body}]), groups, b}", "ret, body}]), [], b}", RUNTIME),
    ("scrutinee-tail", "erase/core.bend", "term_request(c, s, False{}, Some{sty}, scrut), value =>\n        with_answer(computation<answer>, value, +kscrut => s1 =>", "term_request(c, s, tail, Some{sty}, scrut), value =>\n        with_answer(computation<answer>, value, +kscrut => s1 =>", RUNTIME),
    ("branch-tail", "erase/core.bend", "mu_body(c, s, tail, index, plan)", "mu_body(c, s, True{}, index, plan)", RUNTIME),
)


def main():
    import importlib.util
    spec = importlib.util.spec_from_file_location("family_core", ROOT / "dev/test-core-erasure.py")
    core = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(core)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hosts", default="bun,node-worker,native")
    parser.add_argument("--mutations", action="store_true")
    args = parser.parse_args()
    hosts = args.hosts.split(",")
    if not hosts or len(hosts) != len(set(hosts)) or any(h not in ("bun", "node-worker", "native") for h in hosts):
        parser.error("hosts must be a unique list of bun,node-worker,native")
    if args.mutations and "bun" not in hosts:
        parser.error("mutation checks require the bun host")
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    core.WORK = WORK
    build = core.module("family_build", ROOT / "dev/build.py")
    reference = core.module("family_reference", ROOT / "dev/reference-fixtures.py")
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    inputs = [[path, core.digest(ROOT / path)] for path in FIXTURES]
    expected, provenance = reference.load(pins, "family-erasure", inputs)
    if not isinstance(expected, dict) or set(expected) != set(FIXTURES) or any(not isinstance(v, str) or not v for v in expected.values()):
        raise RuntimeError("malformed family erasure observations")
    if any(line not in expected[path].splitlines() for path, lines in GOLDENS.items() for line in lines):
        raise RuntimeError("recorded Kanon observations differ from independent family erasure goldens")
    source = ROOT / "test/family-erasure.bend"
    paths = set(build.dependencies(source)) | {ROOT / p for p in FIXTURES}
    paths |= {ROOT / p for p in ("Makefile", "dev/build.py", "dev/toolchain.json", "dev/test-core-erasure.py",
                                "dev/test-family-erasure.py", "dev/bend-policy.json", "dev/house-bend.py",
                                *reference.source_files("family-erasure"))}
    before = {str(p.relative_to(ROOT)): core.digest(p) for p in sorted(paths)}
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    binary, _, _ = build.compiler(pins)
    core.run("family-check", [binary, source, "--check-only"], env)
    if set(hosts) & {"bun", "node-worker"}:
        core.run("family-js", [binary, source, "-o", WORK / "families.js"], env)
    results = {}
    for host in hosts:
        if host == "native":
            core.run("family-native-build", [binary, source, "-o", WORK / "families.exe"], env)
            argv = [WORK / "families.exe"]
        elif host == "bun":
            argv = [pins["tools"]["bun"]["path"], WORK / "families.js"]
        else:
            argv = [pins["tools"]["node"]["path"], "-e", build.node_worker_script(pins), WORK / "families.js"]
        observations = {}
        for i, path in enumerate(FIXTURES):
            result = core.run(f"family-{host}-{i}", [*argv, (ROOT / path).read_text()], env)
            # IO.print adds one newline after the erased program; the recorded Kanon text has none.
            if result["stderr"] or result["stdout"] != expected[path] + "\n":
                raise RuntimeError(f"{host} {path}: family erasure differs from recorded Kanon observations; see {WORK}")
            observations[path] = result
            print(f"PASS family erasure {host}: {path}", flush=True)
        results[host] = observations
    mutations = []
    if args.mutations:
        for label, path, old, new, fixture in MUTANTS:
            dest = WORK / "mutants" / label
            if dest.exists():
                shutil.rmtree(dest)
            shutil.copytree(ROOT, dest, ignore=shutil.ignore_patterns(".git", "_build", ".gatework*", ".kanon*"))
            target = dest / path
            text = target.read_text()
            if text.count(old) != 1:
                raise RuntimeError(f"{label}: mutation site count changed")
            target.write_text(text.replace(old, new))
            out = dest / "families.js"
            core.run(f"mutant-{label}-build", [binary, dest / "test/family-erasure.bend", "-o", out], env)
            result = core.run(f"mutant-{label}", [pins["tools"]["bun"]["path"], out, (ROOT / fixture).read_text()], env)
            if result["stderr"] or result["stdout"].startswith("FAIL") or result["stdout"] == expected[fixture] + "\n":
                raise RuntimeError(f"{label}: mutation survived or failed outside the semantic comparison")
            mutations.append({"name": label, "kind": "semantic-rejection", "fixture": fixture, "observation": result})
            print(f"PASS family erasure mutation: {label}", flush=True)
    after = {str(p.relative_to(ROOT)): core.digest(p) for p in sorted(paths)}
    if before != after:
        raise RuntimeError("family erasure inputs changed during validation")
    record = {"schema": 1, "stage": "A.5b.3.2e.2", "oracle": provenance, "source_sha256": before,
              "fixtures": len(FIXTURES), "independent_goldens": sum(len(lines) for lines in GOLDENS.values()), "hosts": results,
              "mutations": mutations, "full_stage_a": "pending"}
    (WORK / "result.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"FAMILY-ERASURE PASS fixtures={len(FIXTURES)} goldens={record['independent_goldens']} hosts={','.join(hosts)} mutants={len(mutations)}", flush=True)


if __name__ == "__main__":
    main()
