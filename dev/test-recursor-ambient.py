#!/usr/bin/env python3
"""Check the recursor under given locals on the pinned Bend hosts: scrutinee, motive and body reads of the given context."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/recursor-ambient"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


ELIM = module("recursor_elim_cases", ROOT / "dev/test-recursor-elim.py")
BODY = ELIM.BODY
accept, other = ELIM.accept, ELIM.other
OUT_N, OUT_T, NAT, VS, NAT_T = ELIM.OUT_N, ELIM.OUT_T, ELIM.NAT, ELIM.VS, ELIM.NT
EMPTY = OUT_N + "\nmu E : Type 0 with"
TREE = OUT_N + "\nmu T (0 A : Type 0) : Type 0 with | leaf : T A | fork : (left : T A) -> (right : T Nat) -> T A"
NO_CONSTRUCTORS = "FAIL\nnot yet: recursor coverage for a family without constructors is not supported"
TYPE_1 = "Type 1"
UNKNOWN_CONTEXT = "FAIL\nmismatch: expected a known context"


def unbound(index):
    return f"FAIL\nunbound: de Bruijn index {index} is outside the context"


def at_none(row):
    name, *middle, groups, expected = row
    return (f"none-{name}", *middle, "none", groups, expected)


# A row is name, source, family, motive, checking mode, scrutinee quantity, scrutinee, context, branch groups, expected output.
# A context names the given locals, outermost first: none; n (n : N); n0 (0 n : N); m (m : Nat); m0 (0 m : Nat);
# m1 (1 m : Nat); m1-closed (1 m : Nat, with the binder check on the result usage); nk (n : N, k : N); e (e : E);
# iv (0 i : N, v : V i). A term indexes the leg binders first, then the declaration parameters, then the given locals.
# The origin count is the given locals plus the declaration parameters. A usage row lists the innermost variable first.
# The elim rows run again without a given local. Their expected outputs do not change.
# A scrutinee type at a declaration parameter reads the given locals: a local of type N is not a parameter of type Type 0.
# The kernel checks such a leg against Type 1, the universe of the parameter domain; the mismatch names that universe.
RETURN = ("zero v0", "succ v2 w c w ih")
ONCE = ("zero v0", "succ v0 w c w ih")
BOXED = ("box pair 1 value",)
VECTOR = ("vz tag-zero", "vs grow " + VS)
NONE = tuple(at_none(row) for row in ELIM.CASES)
LOCAL = (
    ("local-scrutinee", OUT_N, "N", "result", "w", "w", "p0", "n", NAT, accept(1, OUT_T, "n:w..w")),
    ("local-motive", BODY.AT, "N", "local", "w", "w", "zero", "n", ("zero at-local", "succ v0 w c w ih"), accept(1, BODY.former("At", "n"))),
    ("local-body", OUT_N, "N", "constant", "w", "w", "zero", "m", RETURN, accept(1, "Nat", "m:w..w")),
    ("linear-every-branch", OUT_N, "N", "constant", "1", "1", "zero", "m1", RETURN, accept(1, "Nat", "m:1..1")),
    ("linear-every-branch-closed", OUT_N, "N", "constant", "1", "1", "zero", "m1-closed", RETURN, accept(1, "Nat", "m:1..1")),
    ("linear-one-branch", OUT_N, "N", "constant", "1", "1", "zero", "m1", ONCE, accept(1, "Nat", "m:0..1")),
    ("linear-one-branch-closed", OUT_N, "N", "constant", "1", "1", "zero", "m1-closed", ONCE, BODY.linear("m")),
    ("linear-scrutinee-and-branch", BODY.BOX, "B", "result", "1", "1", "box-local", "m1", BOXED, accept(1, OUT_T, "m:w..w")),
    ("linear-twice-closed", BODY.BOX, "B", "result", "1", "1", "box-local", "m1-closed", BOXED, BODY.linear("m")),
    ("vector-local-index", BODY.VEC, "V", "index", "w", "w", "p0", "iv", VECTOR, accept(2, BODY.former("Tag", "i"), "v:w..w")),
    ("list-under-local", ELIM.LIST, "List", "result", "w", "w", "nil", "n", ("nil near", "cons deep w h w t w ih"), BODY.mismatch(NAT_T, TYPE_1)),
    ("two-locals", OUT_N, "N", "result", "w", "w", "p1", "nk", ("zero stop", "succ far w c w ih"), accept(2, OUT_T, "k:0..w;n:w..w")),
    ("self-under-local", BODY.AT, "N", "self", "w", "w", "one", "n", ELIM.SELF, accept(1, BODY.former("At", ELIM.ONE_V))),
    ("vector-index-under-local", BODY.VEC, "V", "index", "w", "w", "vz", "n", VECTOR, accept(1, BODY.former("Tag", ELIM.ZERO_V))),
    ("pair-parameters-under-local", ELIM.P, "P", "result", "w", "w", "pair", "n", ("p stop",), unbound(1)),
    ("parameters-under-local", ELIM.U, "U", "constant", "1", "1", "keep-y", "n", ELIM.KEEP_X, unbound(1)),
    ("erased-mode-local", OUT_N, "N", "result", "0", "0", "p0", "n0", NAT, accept(1, OUT_T)),
    ("erased-local-scrutinee", OUT_N, "N", "result", "w", "w", "p0", "n0", NAT, BODY.erased("n")),
    ("erased-local-quantity", OUT_N, "N", "result", "w", "0", "p0", "n0", NAT, ELIM.ERASED),
    ("erased-local-body", OUT_N, "N", "constant", "w", "w", "zero", "m0", ONCE, BODY.erased("m")),
    ("empty-family-local", EMPTY, "E", "result", "w", "w", "p0", "e", (), NO_CONSTRUCTORS),
    ("local-other-family", OUT_N, "Out", "result", "w", "w", "p0", "n", ("stop stop",), other("N", "Out")),
    ("scrutinee-above-context", OUT_N, "N", "result", "w", "w", "p1", "n", NAT, unbound(1)),
    ("body-above-context", OUT_N, "N", "constant", "w", "w", "zero", "m", ("zero v1", "succ v0 w c w ih"), unbound(1)),
    ("non-uniform-under-local", TREE, "T", "result", "w", "w", "leaf-a", "n", ("leaf stop", "fork stop w left w right w ihl w ihr"), BODY.mismatch(NAT_T, TYPE_1)),
    ("unknown-context", OUT_N, "N", "result", "w", "w", "zero", "bogus", NAT, UNKNOWN_CONTEXT),
)
CASES = NONE + LOCAL
# Each mutant puts one scope site back on a context without the given locals. The uniform site needs a given local
# of type Type 0: its mutant lives in the instance suite.
SCOPE = "Rec.open(scope, R.family_params(fam), c)"
FRESH = "C.make(C.globals(c), C.budget(c))"
DROPPED = f"Rec.open(scope, R.family_params(fam), {FRESH})"
MUTANTS = (
    ("elim-drops-locals", "lib/kernel_recursor_elim.bend", SCOPE, DROPPED, "local-motive"),
    ("cover-drops-locals", "lib/kernel_recursor_elim.bend", "Cover.check(c, scope, mode, owner, mo, branches)", f"Cover.check({FRESH}, scope, mode, owner, mo, branches)", "local-body"),
    ("body-drops-locals", "lib/kernel_recursor_body.bend", SCOPE, DROPPED, "local-body"),
    ("motive-drops-locals", "lib/kernel_recursor_motive.bend", SCOPE, DROPPED, "local-motive"),
    ("branch-drops-locals", "lib/kernel_recursor_branch.bend", SCOPE, DROPPED, "self-under-local"),
    ("constructor-drops-locals", "lib/kernel_recursor.bend", "open(scope, params, c)", f"open(scope, params, {FRESH})", "vector-index-under-local"),
)


def argv_of(row):
    _, text, family, motive, mode, quantity, scrutinee, given, groups, _ = row
    return [text, family, motive, mode, quantity, scrutinee, given, *" / ".join(groups).split()]


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
    if len(CASES) != 69 or len({row[0] for row in CASES}) != len(CASES):
        raise RuntimeError(f"ambient case count or names changed: {len(CASES)}")
    WORK.mkdir(parents=True, exist_ok=True)
    for name in ("result.json", "failures.json"):
        (WORK / name).unlink(missing_ok=True)
    core = module("ambient_core", ROOT / "dev/test-core-erasure.py")
    build = module("ambient_build", ROOT / "dev/build.py")
    core.WORK = WORK
    source = ROOT / "test/recursor-ambient.bend"

    def snapshot():
        paths = set(core.sources()) | {"dev/test-recursor-ambient.py", "dev/test-recursor-elim.py", "dev/test-recursor-body.py", "dev/test-recursor-layout.py"}
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
        raise RuntimeError(f"{len(failures)} ambient mismatches; see {WORK / 'failures.json'}")
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
            mutant = tree / "test/recursor-ambient.bend"
            output = tree / "checks.js"
            core.run(f"mutant-{name}-check", [binary, mutant, "--check-only"], env)
            core.run(f"mutant-{name}-compile", [binary, mutant, "-o", output], env)
            row = next(row for row in CASES if row[0] == witness)
            result = core.run(f"mutant-{name}", [pins["tools"]["bun"]["path"], output, *argv_of(row)], env)
            if result["stderr"] or result["stdout"] == row[-1] + "\n":
                raise RuntimeError(f"mutant {name} did not fail its behavioral witness")
            mutations.append({"name": name, "path": mutation_path, "case": witness, "killed": True, "observation": result})
    if snapshot() != hashes:
        raise RuntimeError("source changed during ambient validation")
    record = {"schema": 1, "cases": len(CASES), "sources": hashes, "hosts": observations, "mutations": mutations,
              "scope": "Checked recursor under given locals: the scope is the given context, then the declaration parameters defined at the scrutinee values; the scrutinee is inferred in the given context, and the motive and the bodies can read the given locals; routing of the kernel Elim term, public elim hypotheses, delayed recursive evaluation, erasure, families without constructors, mutual and nondirect recursion remain pending."}
    (WORK / "result.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"RECURSOR-AMBIENT PASS cases={len(CASES)} hosts={','.join(hosts)} mutants={len(mutations)}")


if __name__ == "__main__":
    main()
