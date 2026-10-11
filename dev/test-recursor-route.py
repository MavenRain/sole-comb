#!/usr/bin/env python3
"""Check the routing of the kernel Elim term on the pinned Bend hosts: a leg that binds hypotheses routes the term to the checked recursor."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/recursor-route"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


AMBIENT = module("recursor_ambient_cases", ROOT / "dev/test-recursor-ambient.py")
ELIM = AMBIENT.ELIM
BODY = ELIM.BODY
accept, other, unbound = ELIM.accept, ELIM.other, AMBIENT.unbound
OUT_T, NT, LIST, P, U, LIN, TREE = ELIM.OUT_T, ELIM.NT, ELIM.LIST, ELIM.P, ELIM.U, ELIM.LIN, AMBIENT.TREE
TYPE_1, UNKNOWN_CONTEXT = AMBIENT.TYPE_1, AMBIENT.UNKNOWN_CONTEXT
LIST_A = BODY.former("List", "", 1, " => A")
LIST_N = BODY.former("List", "", 1, " => " + NT)
NOT_UNIFORM = "FAIL\nnot yet: recursor layout requires uniform recursive parameters"
UNKNOWN_SCRUTINEE = "FAIL\nmismatch: expected a known scrutinee"
LEG_0 = BODY.mismatch("Type 0", TYPE_1)

# A row is name, source, family, motive, checking mode, scrutinee quantity, scrutinee, expected type, context, branch
# groups, expected output. The expected type "-" infers through the public infer drive, "eval" evaluates through the
# public evaluation entry at the given locals, and "out" or "n" checks at that closed type (the check_node site).
# A leg at a constructor with a recursive field that binds more binders than the constructor takes fields routes the
# term to the checked recursor: there the body indices are the leg binders, then the declaration parameters, then the
# given locals. A term without such a leg stays on the kernel case analysis: there the body indices are the leg
# binders, then the given locals. The origin count is the given locals plus the declaration parameters; a parameter
# row never prints (the result usage is closed down to the given locals). A usage row lists the innermost variable
# first.
OUT_N, VEC, AFF, EMPTY = ELIM.OUT_N, ELIM.VEC, ELIM.AFF, AMBIENT.EMPTY
LIN_N = BODY.N + "mu Out : Type 0 with | stop : Out | one : (1 only : Nat) -> Out | both : (1 field : Nat) -> (previous : Out) -> Out"
NILS = ("nil stop", "cons v0 w h w t w ih")
HEAD = ("nil v2", "cons v2 w h w t w ih")
HEAD_N = ("nil v1", "cons v2 w h w t w ih")
FORK = ("leaf stop", "fork stop w left w right w ihl w ihr")
CASE_N = ("zero stop", "succ stop w c")
FIELD_N = ("zero stop", "succ v0 w c")
HYP_N = ("zero stop", "succ v0 w c w ih")
CASE_LIST = ("nil stop", "cons stop w h w t")
KERNEL_ARITY = "FAIL\nmissing branch: the branch at succ binds 0 fields and succ takes 1"
KERNEL_EXTRA = "FAIL\nmissing branch: the branch at p binds 1 fields and p takes 0"
NO_MOTIVE = "FAIL\ncannot infer: an elimination at a mu shape needs a motive"
UNKNOWN_KEY = "FAIL\nunbound: bogus is not a constructor of N"
DELAYED = "FAIL\nnot yet: delayed recursive evaluation of a recursor branch is not supported"
CASES = (
    # Accept, kernel path unchanged (no leg binds hypotheses).
    ("kernel-zero", OUT_N, "N", "result", "w", "w", "zero", "-", "none", CASE_N, accept(0, OUT_T)),
    ("kernel-succ", OUT_N, "N", "result", "w", "w", "one", "-", "none", CASE_N, accept(0, OUT_T)),
    ("kernel-nil", LIST, "List", "result", "w", "w", "nil-n", "-", "none", CASE_LIST, accept(1, OUT_T)),
    ("kernel-cons", LIST, "List", "result", "w", "w", "cons-n", "-", "none", CASE_LIST, accept(1, OUT_T)),
    ("kernel-pair", P, "P", "result", "w", "w", "p-nn", "-", "none", ("p stop",), accept(2, OUT_T)),
    ("kernel-local-uses", OUT_N, "N", "result", "1", "1", "zero", "-", "n", ("zero stop", "succ near w c"), accept(1, OUT_T, "n:0..w")),
    # No branches: the path set is unreachable, so the scrutinee read reports a lower bound of 1.
    ("kernel-empty", EMPTY, "E", "result", "w", "w", "p0", "-", "e", (), accept(1, OUT_T, "e:1..w")),
    ("kernel-check", OUT_N, "N", "result", "w", "w", "one", "out", "none", CASE_N, accept(0, OUT_T)),
    # Accept, routed to the checked recursor.
    ("routed-zero", OUT_N, "N", "result", "w", "w", "zero", "-", "none", ELIM.NAT, accept(0, OUT_T)),
    ("routed-succ", OUT_N, "N", "result", "w", "w", "one", "-", "none", ELIM.NAT, accept(0, OUT_T)),
    ("routed-hypothesis", OUT_N, "N", "result", "w", "w", "one", "-", "none", HYP_N, accept(0, OUT_T)),
    ("routed-cons", LIST, "List", "result", "w", "w", "cons-n", "-", "none", NILS, accept(1, OUT_T)),
    ("routed-closed-parameters", LIST, "List", "result", "1", "1", "p0", "-", "la1", NILS, accept(3, OUT_T, "xs:1..1")),
    ("routed-local-list-linear", LIST, "List", "result", "1", "1", "p0", "-", "ln", NILS, accept(2, OUT_T, "xs:1..1")),
    ("routed-parameter-value", LIST, "List", "parameter", "w", "w", "nil-n", "-", "n", HEAD_N, accept(2, NT, "n:0..w")),
    ("routed-head-linear", LIST, "List", "parameter", "1", "1", "p0", "-", "nln", HEAD, accept(3, NT, "xs:1..1;n:0..1")),
    ("routed-linear-body", LIN_N, "N", "result", "1", "1", "zero", "-", "m1", ("zero one", "succ far w c w ih"), accept(1, OUT_T, "m:1..1")),
    ("routed-indexed", VEC, "V", "result", "w", "w", "vz", "-", "none", ("vz stop", "vs stop " + ELIM.VS), accept(0, OUT_T)),
    ("routed-uniform-tree", TREE, "T", "result", "w", "w", "leaf-nat", "-", "none", FORK, accept(1, OUT_T)),
    ("routed-check", OUT_N, "N", "result", "w", "w", "one", "out", "none", ELIM.NAT, accept(0, OUT_T)),
    # Refuse, routed: the recursor texts.
    ("routed-three-binders", OUT_N, "N", "result", "w", "w", "one", "-", "none", ("zero stop", "succ v0 w c w ih w extra"), BODY.arity("succ", 3, 2)),
    ("routed-mixed-legs", OUT_N, "N", "result", "w", "w", "one", "-", "none", ("zero stop w x", "succ both w n w ih"), BODY.arity("zero", 1, 0)),
    ("routed-hypothesis-quantity", OUT_N, "N", "result", "w", "w", "one", "-", "none", ("zero stop", "succ both w n 0 ih"), BODY.marked("ih", "0", "hypothesis", "w")),
    ("routed-field-quantity", OUT_N, "N", "result", "w", "w", "one", "-", "none", ("zero stop", "succ both 0 n w ih"), BODY.marked("n", "0", "field", "w")),
    ("routed-parameter-runtime", LIST, "List", "result", "w", "w", "nil-n", "-", "none", ("nil v0", "cons v0 w h w t w ih"), BODY.erased("A")),
    ("routed-no-motive", OUT_N, "N", "none", "w", "w", "one", "-", "none", ELIM.NAT, NO_MOTIVE),
    ("routed-other-family", LIST, "Out", "result", "w", "w", "nil-n", "-", "none", ("stop stop", "both stop w f w p w ih"), other("List", "Out")),
    ("routed-affine", AFF, "T", "result", "w", "w", "leaf", "-", "none", ("leaf stop", "fork stop w l 0 g w n w r w li 0 gi w ri"), BODY.AFFINE),
    ("routed-check-wrong", OUT_N, "N", "result", "w", "w", "one", "n", "none", ELIM.NAT, BODY.mismatch(OUT_T, NT)),
    ("routed-missing-branch", OUT_N, "N", "result", "w", "w", "one", "-", "none", ("succ both w n w ih",), ELIM.missing("N", "zero")),
    # Refuse, kernel path unchanged: the kernel texts.
    ("kernel-zero-binders", OUT_N, "N", "result", "w", "w", "one", "-", "none", ("zero stop", "succ stop"), KERNEL_ARITY),
    # p has no recursive field: an extra binder at p stays on the kernel arity check.
    ("kernel-nonrecursive-extra", P, "P", "result", "w", "w", "p-nn", "-", "none", ("p stop w x",), KERNEL_EXTRA),
    ("kernel-missing-branch", OUT_N, "N", "result", "w", "w", "one", "-", "none", ("zero stop",), ELIM.missing("N", "succ")),
    ("kernel-unknown-key", OUT_N, "N", "result", "w", "w", "one", "-", "none", ("zero stop", "succ stop w c", "bogus stop w x"), UNKNOWN_KEY),
    ("kernel-no-motive", OUT_N, "N", "none", "w", "w", "one", "-", "none", CASE_N, NO_MOTIVE),
    ("kernel-check-wrong", OUT_N, "N", "result", "w", "w", "one", "n", "none", CASE_N, BODY.mismatch(OUT_T, NT)),
    ("kernel-unknown-scrutinee", OUT_N, "N", "result", "w", "w", "bogus", "-", "none", CASE_N, UNKNOWN_SCRUTINEE),
    # Evaluate through the public evaluation entry.
    ("eval-routed-succ", OUT_N, "N", "result", "w", "w", "one", "eval", "none", HYP_N, DELAYED),
    ("eval-routed-zero", OUT_N, "N", "result", "w", "w", "zero", "eval", "none", HYP_N, "PASS value=" + BODY.value("Out", "stop")),
    ("eval-fields-succ", OUT_N, "N", "result", "w", "w", "one", "eval", "none", FIELD_N, "PASS value=" + ELIM.ZERO_V),
    ("eval-fields-zero", OUT_N, "N", "result", "w", "w", "zero", "eval", "none", FIELD_N, "PASS value=" + BODY.value("Out", "stop")),
)
# Each mutant breaks one routing site. The harness names the file of each mutant: the dispatch signal in the route
# module, the dispatch at both kernel Elim sites in the check module and the evaluator guard in the rules module. The
# parameter close in the elim module has no mutant: a parameter binds at quantity 0, so a parameter row that is not
# closed reads 0..0 and never prints.
ROUTE, CHECK, RULES = "lib/kernel_recursor_route.bend", "lib/kernel_check.bend", "lib/kernel_rules.bend"
FIELDS = "R.less_than(F.Int63.compare(R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, args), binders(lg)))"
FEWER = "R.less_than(F.Int63.compare(binders(lg), R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, args)))"
GUARD = "F.Int63.equal(advance(F.Int63.zero, List.length(&2, F.Pair2<Q.t, String>, bs)), advance(F.Int63.zero, List.length(&2, V.t, args)))"
MUTANTS = (
    ("routes-never", ROUTE, "case Some{f}: legs(f, branches)", "case Some{f}: Bool.and(False{}, legs(f, branches))", "routed-succ"),
    ("routes-always-at-mu", ROUTE, "case Some{f}: legs(f, branches)", "case Some{f}: Bool.or(True{}, legs(f, branches))", "kernel-succ"),
    ("routes-on-equal", ROUTE, FIELDS, "Bool.not(" + FEWER + ")", "kernel-succ"),
    ("routes-on-fewer", ROUTE, FIELDS, FEWER, "routed-succ"),
    ("routes-nonrecursive", ROUTE, "Bool.and(self, ", "Bool.and(Bool.or(True{}, self), ", "kernel-nonrecursive-extra"),
    ("routes-first-leg", ROUTE, "legs(fam, rest))", "Bool.and(False{}, legs(fam, rest)))", "routed-hypothesis"),
    ("infer-site-unrouted", CHECK, "route(c, mode, e, None{})", "R.elim_elim(ctx, ops, c, mode, e, None{})", "routed-succ"),
    ("check-site-unrouted", CHECK, "route(c, mode, e, Some{expected})", "R.elim_elim(ctx, ops, c, mode, e, Some{expected})", "routed-check"),
    ("eval-guard-dropped", RULES, GUARD, "Bool.or(True{}, " + GUARD + ")", "eval-routed-succ"),
    ("eval-guard-inverted", RULES, GUARD, "Bool.not(" + GUARD + ")", "eval-fields-succ"),
)


def argv_of(row):
    _, text, family, motive, mode, quantity, scrutinee, expect, given, groups, _ = row
    return [text, family, motive, mode, quantity, scrutinee, expect, given, *" / ".join(groups).split()]


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
    if len(CASES) != 41 or len({row[0] for row in CASES}) != len(CASES):
        raise RuntimeError(f"route case count or names changed: {len(CASES)}")
    WORK.mkdir(parents=True, exist_ok=True)
    for name in ("result.json", "failures.json"):
        (WORK / name).unlink(missing_ok=True)
    core = module("route_core", ROOT / "dev/test-core-erasure.py")
    build = module("route_build", ROOT / "dev/build.py")
    core.WORK = WORK
    source = ROOT / "test/recursor-route.bend"

    def snapshot():
        paths = set(core.sources()) | {"dev/test-recursor-route.py", "dev/test-recursor-ambient.py", "dev/test-recursor-elim.py", "dev/test-recursor-body.py", "dev/test-recursor-layout.py"}
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
        for row in CASES:
            result = core.run(f"{host}-{row[0]}", [*argv, *argv_of(row)], env)
            if result["stderr"] or result["stdout"] != row[-1] + "\n":
                failures.append({"host": host, "case": row[0], "expected": row[-1] + "\n", "observed": result})
            results[row[0]] = result
        observations[host] = results
    if failures:
        (WORK / "failures.json").write_text(json.dumps(failures, indent=2) + "\n")
        raise RuntimeError(f"{len(failures)} route mismatches; see {WORK / 'failures.json'}")
    mutations = []
    if args.mutations:
        for name, mutation_path, before, after, witness in MUTANTS:
            original = (ROOT / mutation_path).read_text()
            if original.count(before) != 1:
                raise RuntimeError(f"mutant {name} site changed")
            tree = WORK / f"mutant-{name}"
            for path in build.dependencies(source):
                dest = tree / path.relative_to(ROOT)
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, dest)
            (tree / mutation_path).write_text(original.replace(before, after))
            mutant = tree / "test/recursor-route.bend"
            output = tree / "checks.js"
            core.run(f"mutant-{name}-check", [binary, mutant, "--check-only"], env)
            core.run(f"mutant-{name}-compile", [binary, mutant, "-o", output], env)
            row = next(row for row in CASES if row[0] == witness)
            result = core.run(f"mutant-{name}", [pins["tools"]["bun"]["path"], output, *argv_of(row)], env)
            if result["stderr"] or result["stdout"] == row[-1] + "\n":
                raise RuntimeError(f"mutant {name} did not fail its behavioral witness")
            mutations.append({"name": name, "path": mutation_path, "case": witness, "killed": True, "observation": result})
    if snapshot() != hashes:
        raise RuntimeError("source changed during route validation")
    record = {"schema": 1, "cases": len(CASES), "sources": hashes, "hosts": observations, "mutations": mutations,
              "scope": "Routing of the kernel Elim term: a leg at a constructor that binds more binders than the constructor takes fields routes the term to the checked recursor at both kernel Elim sites, the parameter rows are closed out of the result usage, the evaluator refuses a leg whose binder count differs from the constructor arguments, and every other term stays on the kernel case analysis; public elim hypotheses, delayed recursive evaluation, erasure, families without constructors in the recursor, mutual and nondirect recursion remain pending."}
    (WORK / "result.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"RECURSOR-ROUTE PASS cases={len(CASES)} hosts={','.join(hosts)} mutants={len(mutations)}")


if __name__ == "__main__":
    main()
