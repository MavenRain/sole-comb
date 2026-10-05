#!/usr/bin/env python3
"""Check motive admission and direct recursive-child hypothesis types."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/recursor-motive"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


BASE = module("recursor_layout_cases", ROOT / "dev/test-recursor-layout.py")
ROWS = {row[0]: row for row in BASE.UNIFORM_CASES}


def expected(raw, types, level=1):
    return raw + f"\nlevel={level}\ntypes=" + ";".join(types)


def case(name, base, mode="constant", types=(), extra="", level=1):
    _, source, owner, key, raw = ROWS[base]
    return name, source + "\n" + extra, owner, key, mode, expected(raw, types, level)


CASES = (
    case("nullary", "nullary"),
    case("direct", "direct", types=("0:child:Nat",)),
    case("multiple-quantities", "quantities-and-order", types=("0:left:Nat", "1:ghost:Nat", "3:right:Nat")),
    case("nonrecursive", "parameters", "parameter"),
    case("parameter-scope", "recursive-parameter", "parameter", ("1:tail:A",)),
    case("parameter-order", "two-uniform-parameters", "earlier-parameter", ("0:child:A",)),
    case("dependent-parameter", "dependent-uniform-parameters", "earlier-parameter", ("0:child:A",)),
    case("alias-parameter", "alias-uniform-parameter", "parameter", ("1:tail:A",)),
    case("indexed-constant", "indexed-child", types=("2:tail:Nat",)),
    case("indexed-parameter", "indexed-uniform-parameter", "parameter", ("2:tail:A",)),
    case("indexed-domain", "indexed-child", "index", ("2:tail:(Lan SMu Tag [i] (Sec SColl 0 []))",),
         "mu Tag : (0 i : N) -> Type 0 with | tag : (0 i : N) -> Tag i"),
    case("self-domain", "direct", "self", ("0:child:(Lan SMu At [child] (Sec SColl 0 []))",),
         "mu At : (0 n : N) -> Type 0 with | at : (0 n : N) -> At n"),
    case("parameter-self", "recursive-parameter", "parameter-self", ("1:tail:(Lan SMu At [tail] (Sec SColl 1 [ => A]))",),
         "mu At (0 A : Type 0) : (0 xs : List A) -> Type 0 with | at : (0 xs : List A) -> At A xs"),
    case("parameters-self", "two-uniform-parameters", "parameters-self", ("0:child:(Lan SMu At [child] (Sec SColl 2 [ => A;  => B]))",),
         "mu At (0 A : Type 0) (0 B : Type 0) : (0 xs : Duo A B) -> Type 0 with | at : (0 xs : Duo A B) -> At A B xs"),
    case("multiple-self-domains", "quantities-and-order", "self",
         ("0:left:(Lan SMu At [left] (Sec SColl 0 []))", "1:ghost:(Lan SMu At [ghost] (Sec SColl 0 []))", "3:right:(Lan SMu At [right] (Sec SColl 0 []))"),
         "mu At : (0 t : T) -> Type 0 with | at : (0 t : T) -> At t"),
    ("wrong-family", BASE.N, "N", "succ", "wrong-family", "FAIL\nmismatch: the motive is built for Wrong and the scrutinee is at N"),
    ("missing-family", BASE.N, "N", "zero", "missing-family", "FAIL\nmismatch: the motive of an elimination at N names no family"),
    ("unknown-mode", BASE.N, "N", "zero", "bogus", "FAIL\nexpected a known motive mode"),
    ("wrong-indices", ROWS["indexed-child"][1], "V", "vs", "wrong-indices", "FAIL\nmismatch: the motive of V binds 0 indices and the family has 1"),
    ("invalid-motive", BASE.N, "N", "succ", "invalid", "FAIL\nuniverse: a term used as a type is not a universe:  (" + BASE.NT + " : Type 1)"),
    ("invalid-nullary-motive", BASE.N, "N", "zero", "invalid", "FAIL\nuniverse: a term used as a type is not a universe:  (" + BASE.NT + " : Type 1)"),
    ("large-proposition", "mu P : Prop with | p : P | q : P", "P", "p", "constant",
     "FAIL\nuniverse: a large elimination out of a proposition needs a subsingleton family at P"),
    ("small-proposition", "mu Truth : Prop with | truth : Truth\nmu P : Prop with | p : P | q : P", "P", "p", "prop",
     "arity=0 context=0\nfields=\nih=\nlevel=0\ntypes="),
    ("large-subsingleton", "mu P : Prop with | p : P", "P", "p", "constant",
     "arity=0 context=0\nfields=\nih=\nlevel=1\ntypes="),
    ("two-indices", "mu D : (0 i : Nat) -> (0 j : Nat) -> Type 0 with | step : (0 i : Nat) -> (0 j : Nat) -> (child : D j i) -> D i j\nmu Tag : (0 i : Nat) -> (0 j : Nat) -> Type 0 with | tag : (0 i : Nat) -> (0 j : Nat) -> Tag i j", "D", "step", "indices",
     expected(BASE.golden(4, 3, ["0:0:i:Nat:i:ordinary", "1:0:j:Nat:j:ordinary", "2:w:child:(Lan SMu D [j; i] (Sec SColl 0 [])):child:recursive[j,i]"], ["2:w:child:(Lan SMu D [j; i] (Sec SColl 0 [])):child:recursive[j,i]"]), ["2:child:(Lan SMu Tag [j; i] (Sec SColl 0 []))"])),
    ("dependent-indices", "mu D : (0 A : Type 0) -> (0 x : A) -> Type 1 with | step : (0 A : Type 0) -> (0 x : A) -> (child : D A x) -> D A x", "D", "step", "index-type",
     expected(BASE.golden(4, 3, ["0:0:A:Type 1:A:ordinary", "1:0:x:A:x:ordinary", "2:w:child:(Lan SMu D [A; x] (Sec SColl 0 [])):child:recursive[A,x]"], ["2:w:child:(Lan SMu D [A; x] (Sec SColl 0 [])):child:recursive[A,x]"]), ["2:child:A"])),
) + tuple(("layout-" + name, source, owner, key, "constant", raw)
          for name, source, owner, key, raw in BASE.UNIFORM_CASES
          if raw.startswith("FAIL") and not owner.startswith("@"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hosts", default="bun,node-worker,native")
    parser.add_argument("--mutations", action="store_true")
    args = parser.parse_args()
    hosts = args.hosts.split(",")
    if not hosts or len(set(hosts)) != len(hosts) or set(hosts) - {"bun", "node-worker", "native"}:
        parser.error("hosts must be a nonempty unique subset of bun,node-worker,native")
    if args.mutations and "bun" not in hosts:
        parser.error("mutations require the Bun host")
    if len(CASES) != 43 or len({row[0] for row in CASES}) != len(CASES):
        raise RuntimeError(f"motive case count or names changed: {len(CASES)}")
    WORK.mkdir(parents=True, exist_ok=True)
    for name in ("result.json", "failures.json"):
        (WORK / name).unlink(missing_ok=True)
    core = module("motive_core", ROOT / "dev/test-core-erasure.py")
    build = module("motive_build", ROOT / "dev/build.py")
    core.WORK = WORK
    source = ROOT / "test/recursor-motive.bend"

    def snapshot():
        paths = set(core.sources()) | {"dev/test-recursor-motive.py", "dev/test-recursor-layout.py"}
        paths.update(str(path.relative_to(ROOT)) for path in build.dependencies(source))
        return {path: core.digest(ROOT / path) for path in sorted(paths)}

    hashes = snapshot()
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    (ROOT / "_build/bend-cache").mkdir(parents=True, exist_ok=True)
    binary, _, _ = build.compiler(pins)
    core.run("check", [binary, source, "--check-only"], env)
    if set(hosts) & {"bun", "node-worker"}:
        core.run("javascript", [binary, source, "-o", WORK / "checks.js"], env)
    observations, failures = {}, []
    for host in hosts:
        if host == "native":
            core.run("native-build", [binary, source, "-o", WORK / "checks.exe"], env)
            argv = [WORK / "checks.exe"]
        elif host == "bun":
            argv = [pins["tools"]["bun"]["path"], WORK / "checks.js"]
        else:
            argv = [pins["tools"]["node"]["path"], "-e", build.node_worker_script(pins), WORK / "checks.js"]
        results = {}
        for name, text, family, ctor, mode, want in CASES:
            result = core.run(f"{host}-{name}", [*argv, text, family, ctor, mode], env)
            if result["stderr"] or result["stdout"] != want + "\n":
                failures.append({"host": host, "case": name, "expected": want + "\n", "observed": result})
            results[name] = result
        observations[host] = results
    if failures:
        (WORK / "failures.json").write_text(json.dumps(failures, indent=2) + "\n")
        raise RuntimeError(f"{len(failures)} motive mismatches; see {WORK / 'failures.json'}")
    mutations = []
    if args.mutations:
        mutation_path = "lib/kernel_recursor_motive.bend"
        original = (ROOT / mutation_path).read_text()
        candidates = (
            ("skip-motive-type", "R.mu_motive_lvl(C.ctx, C.ops, origin, owner, fam, valid, diagram(R.family_params(fam), origin), Some{R.family_level(fam)}, C.env(origin))", "R.Pure{L.one}", "invalid-nullary-motive"),
            ("skip-family", "R.mu_motive_view(C.ctx, owner, count, count, mo, mo)", "R.Pure{mo}", "wrong-family"),
            ("skip-large", "R.mu_large(C.ctx, owner, fam, lvl)", "R.Pure{Unit{}}", "large-proposition"),
            ("parameter-diagram", "T.Var{R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, rest)}", "T.Var{F.Int63.zero}", "parameters-self"),
            ("index-order", "mo, indices, value)", "mo, R.rev_append(V.t, indices, []), value)", "two-indices"),
            ("wrong-self", "mo, indices, value)", "mo, indices, V.var(F.Int63.zero))", "multiple-self-domains"),
            ("parameter-environment", "hypotheses(origin, mo, fs)", "hypotheses(C.make(C.globals(origin), C.budget(origin)), mo, fs)", "parameter-scope"),
            ("drop-later-children", "hypotheses(origin, mo, rest)))", "Done{[]}))", "multiple-quantities"),
        )
        for name, before, after, witness in candidates:
            if original.count(before) != 1:
                raise RuntimeError(f"mutant {name} site changed")
            tree = WORK / f"mutant-{name}"
            for path in build.dependencies(source):
                dest = tree / path.relative_to(ROOT)
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, dest)
            (tree / mutation_path).write_text(original.replace(before, after))
            mutant = tree / "test/recursor-motive.bend"
            output = tree / "checks.js"
            core.run(f"mutant-{name}-check", [binary, mutant, "--check-only"], env)
            core.run(f"mutant-{name}-compile", [binary, mutant, "-o", output], env)
            _, text, family, ctor, mode, want = next(row for row in CASES if row[0] == witness)
            result = core.run(f"mutant-{name}", [pins["tools"]["bun"]["path"], output, text, family, ctor, mode], env)
            if result["stderr"] or result["stdout"] == want + "\n":
                raise RuntimeError(f"mutant {name} did not fail its behavioral witness")
            mutations.append({"name": name, "case": witness, "killed": True, "observation": result})
    if snapshot() != hashes:
        raise RuntimeError("source changed during motive validation")
    record = {"schema": 1, "cases": len(CASES), "sources": hashes, "hosts": observations, "mutations": mutations,
              "scope": "Checked recursor motives and direct-child hypothesis types; public branch binders, delayed evaluation and erasure remain pending."}
    (WORK / "result.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"RECURSOR-MOTIVE PASS cases={len(CASES)} hosts={','.join(hosts)} mutants={len(mutations)}")


if __name__ == "__main__":
    main()
