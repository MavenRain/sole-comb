#!/usr/bin/env python3
"""Check recursor branch binder and target types on the pinned Bend hosts."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/recursor-branch"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


BASE = module("recursor_layout_cases", ROOT / "dev/test-recursor-layout.py")
ROWS = {row[0]: row for row in BASE.UNIFORM_CASES}
N, NT = BASE.N, BASE.NT
AFFINE = "FAIL\nnot yet: recursor hypotheses for affine recursive fields are not supported"
UNIVERSE = "FAIL\nuniverse: a term used as a type is not a universe:  "
AT_N = "mu At : (0 n : N) -> Type 0 with | at : (0 n : N) -> At n"
AT_T = "mu At : (0 t : T) -> Type 0 with | at : (0 t : T) -> At t"
AT_LIST = "mu At (0 A : Type 0) : (0 xs : List A) -> Type 0 with | at : (0 xs : List A) -> At A xs"
AT_DUO = "mu At (0 A : Type 0) (0 B : Type 0) : (0 xs : Duo A B) -> Type 0 with | at : (0 xs : Duo A B) -> At A B xs"
TAG_N = "mu Tag : (0 i : N) -> Type 0 with | tag : (0 i : N) -> Tag i"
TREE = "mu T : Type 0 with | leaf : T | fork : (left : T) -> (0 ghost : T) -> (label : Nat) -> (right : T) -> T"
TWO = ("mu D : (0 i : Nat) -> (0 j : Nat) -> Type 0 with | step : (0 i : Nat) -> (0 j : Nat) -> (child : D j i) -> D i j\n"
       "mu Tag : (0 i : Nat) -> (0 j : Nat) -> Type 0 with | tag : (0 i : Nat) -> (0 j : Nat) -> Tag i j")


def expected(context, binders, target, level=1):
    return f"context={context} level={level}\nbinders={';'.join(binders)}\ntarget={target}\nordinary=same"


def former(o, args="", count=0, params=""):
    return f"(Lan SMu {o} [{args}] (Sec SColl {count} [{params}]))"


def value(o, k, fields=""):
    return f"(In SMu {o} [] (ACtor {k}) [{fields}])"


def case(name, base, want, mode="constant", key=None, extra=""):
    _, source, owner, ctor, _ = ROWS[base]
    return name, source + "\n" + extra, owner, key or ctor, mode, want


TREE_T = former("T")
LIST_A = former("List", count=1, params=" => A")
DUO = former("Duo", count=2, params=" => A;  => B")
TREE_FIELDS = [f"0:w:left:{TREE_T}", f"1:0:ghost:{TREE_T}", "2:w:label:Nat", f"3:w:right:{TREE_T}"]
INDEXED_FIELDS = [f"0:0:i:{NT}", "1:w:value:Nat", "2:w:tail:" + former("V", "i")]
CASES = (
    case("nullary", "nullary", expected(0, [], "Nat")),
    case("direct", "direct", expected(2, [f"0:w:child:{NT}", "1:w:child:Nat"], "Nat")),
    case("self-nullary", "nullary", expected(0, [], former("At", value("N", "zero"))), "self", extra=AT_N),
    case("self-direct", "direct", expected(2, [f"0:w:child:{NT}", "1:w:child:" + former("At", "child")], former("At", value("N", "succ", "child"))), "self", extra=AT_N),
    ("multiple-children", TREE, "T", "fork", "constant", expected(7, [*TREE_FIELDS, "4:w:left:Nat", "5:0:ghost:Nat", "6:w:right:Nat"], "Nat")),
    ("multiple-self", TREE + "\n" + AT_T, "T", "fork", "self",
     expected(7, [*TREE_FIELDS, "4:w:left:" + former("At", "left"), "5:0:ghost:" + former("At", "ghost"), "6:w:right:" + former("At", "right")],
              former("At", value("T", "fork", "left; ghost; label; right")))),
    ("leaf-self", TREE + "\n" + AT_T, "T", "leaf", "self", expected(0, [], former("At", value("T", "leaf")))),
    ("zero-child", "mu Z : Type 0 with | base : Z | step : (0 child : Z) -> Z", "Z", "step", "constant",
     expected(2, ["0:0:child:" + former("Z"), "1:0:child:Nat"], "Nat")),
    case("affine-child", "quantities-and-order", AFFINE),
    ("affine-later-child", "mu T : Type 0 with | leaf : T | fork : (left : T) -> (1 right : T) -> T", "T", "fork", "constant", AFFINE),
    case("affine-sibling", "quantities-and-order", expected(0, [], "Nat"), key="leaf"),
    ("affine-ordinary-field", "mu B : Type 0 with | box : (1 value : Nat) -> B", "B", "box", "constant", expected(1, ["0:1:value:Nat"], "Nat")),
    case("later-field", "later-field", expected(3, ["0:w:label:Nat", "1:w:child:" + former("End"), "2:w:child:Nat"], "Nat")),
    case("ordinary-parameters", "parameters", expected(4, ["0:w:left:A", "1:w:right:B"], "B"), "parameter"),
    case("list-nil", "recursive-parameter", expected(1, [], "A"), "parameter", key="nil"),
    case("list-cons", "recursive-parameter", expected(4, ["0:w:head:A", f"1:w:tail:{LIST_A}", "2:w:tail:A"], "A"), "parameter"),
    case("list-nil-self", "recursive-parameter", expected(1, [], former("At", value("List", "nil"), 1, " => A")), "parameter-self", key="nil", extra=AT_LIST),
    case("list-cons-self", "recursive-parameter",
         expected(4, ["0:w:head:A", f"1:w:tail:{LIST_A}", "2:w:tail:" + former("At", "tail", 1, " => A")], former("At", value("List", "cons", "head; tail"), 1, " => A")),
         "parameter-self", extra=AT_LIST),
    case("parameter-order", "two-uniform-parameters", expected(4, [f"0:w:child:{DUO}", "1:w:child:A"], "A"), "earlier-parameter"),
    case("dependent-parameter", "dependent-uniform-parameters", expected(4, ["0:w:child:" + former("D", count=2, params=" => A;  => x"), "1:w:child:A"], "A"), "earlier-parameter"),
    case("parameters-self", "two-uniform-parameters",
         expected(4, [f"0:w:child:{DUO}", "1:w:child:" + former("At", "child", 2, " => A;  => B")], former("At", value("Duo", "duo", "child"), 2, " => A;  => B")),
         "parameters-self", extra=AT_DUO),
    case("alias-parameter", "alias-uniform-parameter", expected(4, ["0:w:head:A", f"1:w:tail:{LIST_A}", "2:w:tail:A"], "A"), "parameter"),
    case("indexed-nil", "indexed-child", expected(0, [], former("Tag", value("N", "zero"))), "index", key="vz", extra=TAG_N),
    case("indexed-cons", "indexed-child", expected(4, [*INDEXED_FIELDS, "3:w:tail:" + former("Tag", "i")], former("Tag", value("N", "succ", "i"))), "index", extra=TAG_N),
    case("indexed-constant", "indexed-child", expected(4, [*INDEXED_FIELDS, "3:w:tail:Nat"], "Nat")),
    case("indexed-parameter", "indexed-uniform-parameter",
         expected(5, [f"0:0:i:{NT}", "1:w:head:A", "2:w:tail:" + former("V", "i", 1, " => A"), "3:w:tail:A"], "A"), "parameter"),
    ("two-indices", TWO, "D", "step", "indices",
     expected(4, ["0:0:i:Nat", "1:0:j:Nat", "2:w:child:" + former("D", "j; i"), "3:w:child:" + former("Tag", "j; i")], former("Tag", "i; j"))),
    case("dependent-indices", "dependent-indices", expected(4, ["0:0:A:Type 1", "1:0:x:A", "2:w:child:" + former("D", "A; x"), "3:w:child:A"], "A"), "index-type"),
    ("small-proposition", "mu Truth : Prop with | truth : Truth\nmu P : Prop with | p : P | q : P", "P", "p", "prop", expected(0, [], former("Truth"), 0)),
    ("large-proposition", "mu P : Prop with | p : P | q : P", "P", "p", "constant",
     "FAIL\nuniverse: a large elimination out of a proposition needs a subsingleton family at P"),
    ("large-subsingleton", "mu P : Prop with | p : P", "P", "p", "constant", expected(0, [], "Nat")),
    ("wrong-family", N, "N", "succ", "wrong-family", "FAIL\nmismatch: the motive is built for Wrong and the scrutinee is at N"),
    ("missing-motive-family", N, "N", "zero", "missing-family", "FAIL\nmismatch: the motive of an elimination at N names no family"),
    ("wrong-indices", ROWS["indexed-child"][1], "V", "vs", "wrong-indices", "FAIL\nmismatch: the motive of V binds 0 indices and the family has 1"),
    ("invalid-motive", N, "N", "succ", "invalid", "FAIL\nuniverse: a term used as a type is not a universe:  (" + NT + " : Type 1)"),
    ("unknown-constructor", N, "N", "absent", "constant", "FAIL\nunbound: absent is not a constructor of N"),
    ("missing-family", N, "Missing", "zero", "constant", "FAIL\nunbound: the family Missing is not declared"),
    case("nonuniform", "swapped-parameters", BASE.NONUNIFORM),
    case("nonuniform-zero-child", "zero-changed-parameter", BASE.NONUNIFORM),
    case("mutual", "mutual-sibling", BASE.MUTUAL),
    case("nondirect", "functional-recursion", "FAIL\nnot yet: recursor layout supports direct recursive fields only"),
    ("unknown-mode", N, "N", "zero", "bogus", "FAIL\nexpected a known motive mode"),
    ("ill-formed-target", N, "N", "succ", "ill-formed-target", UNIVERSE + NT),
    ("ill-formed-binder", N, "N", "succ", "ill-formed-binder", UNIVERSE + NT),
)


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
    if len(CASES) != 44 or len({row[0] for row in CASES}) != len(CASES):
        raise RuntimeError(f"branch case count or names changed: {len(CASES)}")
    WORK.mkdir(parents=True, exist_ok=True)
    for name in ("result.json", "failures.json"):
        (WORK / name).unlink(missing_ok=True)
    core = module("branch_core", ROOT / "dev/test-core-erasure.py")
    build = module("branch_build", ROOT / "dev/build.py")
    core.WORK = WORK
    source = ROOT / "test/recursor-branch.bend"

    def snapshot():
        paths = set(core.sources()) | {"dev/test-recursor-branch.py", "dev/test-recursor-layout.py"}
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
        raise RuntimeError(f"{len(failures)} branch mismatches; see {WORK / 'failures.json'}")
    mutations = []
    if args.mutations:
        mutation_path = "lib/kernel_recursor_branch.bend"
        original = (ROOT / mutation_path).read_text()
        candidates = (
            ("hypotheses-first", "List.append(&2, binder, fields(fs), ihs)", "List.append(&2, binder, ihs, fields(fs))", "direct"),
            ("zero-as-many", "case Q.Zero{}: Done{Q.Zero{}}", "case Q.Zero{}: Done{Q.Many{}}", "zero-child"),
            ("admit-affine", 'Fail{E.Not_yet{"recursor hypotheses for affine recursive fields are not supported"}}', "Done{Q.One{}}", "affine-child"),
            ("index-order", "R.mu_result(C, op, origin, mo, idx,", "R.mu_result(C, op, origin, mo, R.rev_append(V.t, idx, []),", "two-indices"),
            ("drop-constructor-fields", "V.VACtor{key}, values(fs)}", "V.VACtor{key}, []}", "self-direct"),
            ("skip-formation", "R.c_univ(C, op, c, tm)", "R.Pure{L.one}", "ill-formed-target"),
            ("hypothesis-position", "hypotheses(hs, fs, R.length(Rec.field, fs))", "hypotheses(hs, fs, F.Int63.succ(R.length(Rec.field, fs)))", "direct"),
            ("child-lookup", "F.Int63.equal(p, position)", "F.Int63.equal(p, F.Int63.succ(position))", "multiple-children"),
            ("drop-later-hypotheses", "hypotheses(rest, fs, F.Int63.succ(count))))", "Done{[]}))", "multiple-children"),
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
            mutant = tree / "test/recursor-branch.bend"
            output = tree / "checks.js"
            core.run(f"mutant-{name}-check", [binary, mutant, "--check-only"], env)
            core.run(f"mutant-{name}-compile", [binary, mutant, "-o", output], env)
            _, text, family, ctor, mode, want = next(row for row in CASES if row[0] == witness)
            result = core.run(f"mutant-{name}", [pins["tools"]["bun"]["path"], output, text, family, ctor, mode], env)
            if result["stderr"] or result["stdout"] == want + "\n":
                raise RuntimeError(f"mutant {name} did not fail its behavioral witness")
            mutations.append({"name": name, "case": witness, "killed": True, "observation": result})
    if snapshot() != hashes:
        raise RuntimeError("source changed during branch validation")
    record = {"schema": 1, "cases": len(CASES), "sources": hashes, "hosts": observations, "mutations": mutations,
              "scope": "Checked recursor branch binder and target types; branch bodies, public elim hypotheses, delayed evaluation, erasure, mutual and nondirect recursion remain pending."}
    (WORK / "result.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"RECURSOR-BRANCH PASS cases={len(CASES)} hosts={','.join(hosts)} mutants={len(mutations)}")


if __name__ == "__main__":
    main()
