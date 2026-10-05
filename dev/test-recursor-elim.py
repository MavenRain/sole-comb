#!/usr/bin/env python3
"""Check the recursor scrutinee, the result type and the joined usage on the pinned Bend hosts."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/recursor-elim"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


BODY = module("recursor_body_cases", ROOT / "dev/test-recursor-body.py")
NT, OUT_N = BODY.NT, BODY.OUT_N
OUT_T = BODY.former("Out")
LIST = OUT_N + "\n" + BODY.LIST
VEC = BODY.ROWS["indexed-child"][1] + "\nmu Out : Type 0 with | stop : Out | both : (field : N) -> (previous : Out) -> Out"
JOIN = OUT_N + "\nmu W (x : Out) : Type 0 with | a : W x | b : W x"
AFF = OUT_N + "\n" + BODY.ROWS["quantities-and-order"][1]
U = OUT_N + "\nmu U (x : Nat) (y : Nat) : Type 0 with | keep : (value : Nat) -> U x y"
P = OUT_N + "\nmu P (0 A : Type 0) (0 B : Type 0) : Type 0 with | p : P A B"
LIN = ("mu Out : Type 0 with | stop : Out | one : (1 only : Nat) -> Out | both : (1 field : Nat) -> (1 previous : Nat) -> Out\n"
       "mu L (x : Nat) (y : Nat) : Type 0 with | hold : (1 value : Nat) -> L x y")
INDEX_0 = "FAIL\nunbound: de Bruijn index 0 is outside the context"
BARE = "FAIL\ncannot infer: an injection has no type of its own;  it needs an expected type"
NOT_FORMER = "FAIL\nmismatch: the scrutinee is not a left former"
OTHER_PARAMETERS = "FAIL\nnot yet: recursor scrutinee parameters other than the declaration parameters are not supported"
ERASED = "FAIL\nquantity: an erased scrutinee cannot be eliminated at runtime"
WRONG = "FAIL\nmismatch: the motive is built for Wrong and the scrutinee is at N"


def accept(params, kind, uses=""):
    return f"PASS origin={params} type={kind} uses={uses}"


def missing(owner, key):
    return f"FAIL\nmissing branch: the elimination of {owner} has no branch at {key}"


def repeated(owner, key):
    return f"FAIL\nwrong leg: the elimination of {owner} repeats the branch at {key}"


def other(found, owner):
    return f"FAIL\nmismatch: the scrutinee is in the family {found} and the recursor eliminates {owner}"


# A row is name, source, family, motive, checking mode, scrutinee quantity, scrutinee, branch groups, expected output.
# A group is a key, a body, then quantity and name pairs. Body index 0 is the last leg binder.
# A variable read records its checking mode. A constructor argument is read at the mode times the field quantity.
# The result usage is the scrutinee usage in sequence with the branch usage scaled by the checking mode.
Z, S = "zero stop", "succ both w n w ih"
NAT = (Z, S)
SELF = ("zero at-zero", "succ next w n w ih")
VS = "0 i w value w tail w ih"
ZERO_V = BODY.value("N", "zero")
ONE_V = BODY.value("N", "succ", ZERO_V)
KEEP_V, KEEP_X = ("keep v0 w value",), ("keep v2 w value",)
HOLD_V, HOLD_X = ("hold one 1 value",), ("hold far 1 value",)
CASES = (
    ("nat-zero", OUT_N, "N", "result", "w", "w", "zero", NAT, accept(0, OUT_T)),
    ("nat-one", OUT_N, "N", "result", "w", "w", "one", NAT, accept(0, OUT_T)),
    ("nat-linear", OUT_N, "N", "result", "1", "1", "zero", NAT, accept(0, OUT_T)),
    ("self-zero", BODY.AT, "N", "self", "w", "w", "zero", SELF, accept(0, BODY.former("At", ZERO_V))),
    ("self-one", BODY.AT, "N", "self", "w", "w", "one", SELF, accept(0, BODY.former("At", ONE_V))),
    ("list-nil", LIST, "List", "result", "w", "w", "nil", ("nil stop", "cons v0 w h w t w ih"), accept(1, OUT_T)),
    ("vector-constant", VEC, "V", "result", "w", "w", "vz", ("vz stop", "vs v0 " + VS), accept(0, OUT_T)),
    ("vector-index", BODY.VEC, "V", "index", "w", "w", "vz", ("vz tag-zero", "vs grow " + VS), accept(0, BODY.former("Tag", ZERO_V))),
    ("pair-parameters", P, "P", "result", "w", "w", "pair", ("p stop",), accept(2, OUT_T)),
    ("erased-mode", OUT_N, "N", "result", "0", "0", "zero", NAT, accept(0, OUT_T)),
    ("erased-mode-many", OUT_N, "N", "result", "0", "w", "one", NAT, accept(0, OUT_T)),
    ("erased-mode-parameter", U, "U", "constant", "0", "0", "keep-x", KEEP_X, accept(2, "Nat")),
    ("field-many-scrutinee", U, "U", "constant", "w", "w", "keep-x", KEEP_V, accept(2, "Nat", "x:w..w")),
    ("field-many-branch", U, "U", "constant", "1", "1", "keep-y", KEEP_X, accept(2, "Nat", "y:w..w;x:1..1")),
    ("scrutinee-read-1-1", LIN, "L", "result", "1", "1", "hold-x", HOLD_V, accept(2, OUT_T, "x:1..1")),
    ("scrutinee-read-1-w", LIN, "L", "result", "1", "w", "hold-x", HOLD_V, accept(2, OUT_T, "x:w..w")),
    ("scrutinee-read-w-1", LIN, "L", "result", "w", "1", "hold-x", HOLD_V, accept(2, OUT_T, "x:w..w")),
    ("scrutinee-read-w-w", LIN, "L", "result", "w", "w", "hold-x", HOLD_V, accept(2, OUT_T, "x:w..w")),
    ("branch-read-1-1", LIN, "L", "result", "1", "1", "hold-y", HOLD_X, accept(2, OUT_T, "y:1..1;x:1..1")),
    ("branch-read-1-w", LIN, "L", "result", "1", "w", "hold-y", HOLD_X, accept(2, OUT_T, "y:w..w;x:1..1")),
    ("branch-read-w-1", LIN, "L", "result", "w", "1", "hold-y", HOLD_X, accept(2, OUT_T, "y:w..w;x:w..w")),
    ("branch-read-w-w", LIN, "L", "result", "w", "w", "hold-y", HOLD_X, accept(2, OUT_T, "y:w..w;x:w..w")),
    ("both-read-1-1", LIN, "L", "result", "1", "1", "hold-x", HOLD_X, accept(2, OUT_T, "x:w..w")),
    ("both-read-w-w", LIN, "L", "result", "w", "w", "hold-x", HOLD_X, accept(2, OUT_T, "x:w..w")),
    ("untypable", OUT_N, "N", "result", "w", "w", "p0", NAT, INDEX_0),
    ("bare-constructor", OUT_N, "N", "result", "w", "w", "bare", NAT, BARE),
    ("erased-parameter-read", P, "P", "result", "w", "w", "p1", ("p stop",), BODY.erased("A")),
    ("not-a-former", BODY.WRAP, "Wrap", "constant", "w", "w", "p0", ("wrap v0",), NOT_FORMER),
    ("universe-parameter", P, "P", "result", "0", "0", "p0", ("p stop",), NOT_FORMER),
    ("other-family", OUT_N, "Out", "result", "w", "w", "zero", ("stop stop",), other("N", "Out")),
    ("parameter-other-family", JOIN, "W", "result", "w", "w", "p0", ("a stop", "b stop"), other("Out", "W")),
    ("other-parameters", P, "P", "result", "w", "w", "pair-swapped", ("p stop",), OTHER_PARAMETERS),
    ("erased-scrutinee-many", OUT_N, "N", "result", "w", "0", "zero", NAT, ERASED),
    ("erased-scrutinee-linear", OUT_N, "N", "result", "1", "0", "zero", NAT, ERASED),
    ("missing-branch", OUT_N, "N", "result", "w", "w", "zero", (Z,), missing("N", "succ")),
    ("untypable-and-missing", OUT_N, "N", "result", "w", "w", "p0", (Z,), INDEX_0),
    ("other-family-and-missing", OUT_N, "Out", "result", "w", "w", "zero", (), other("N", "Out")),
    ("wrong-body", OUT_N, "N", "result", "w", "w", "zero", (Z, "succ v1 w n w ih"), BODY.mismatch(NT, OUT_T)),
    ("wrong-motive", OUT_N, "N", "wrong-family", "w", "w", "zero", NAT, WRONG),
    ("missing-family", OUT_N, "Missing", "result", "w", "w", "zero", (Z,), "FAIL\nunbound: the family Missing is not declared"),
    ("repeated-branch", OUT_N, "N", "result", "w", "w", "zero", (Z, Z, S), repeated("N", "zero")),
    ("affine-child", AFF, "T", "result", "w", "w", "leaf", ("leaf stop", "fork stop"), BODY.AFFINE),
    ("unknown-mode", OUT_N, "N", "bogus", "w", "w", "zero", NAT, "FAIL\nexpected a known motive mode"),
)
COVER = "Cover.check(c, mode, owner, mo, branches)"
INFER = "R.infer_scrutinee(C.ctx, C.ops, origin, mode, sq, scrut)"
TYPED = f"R.bind(R.inferred, R.inferred, Rec.rules(R.inferred, origin, {INFER}), inf => typed(c, mode, owner, scrut, mo, branches, fam, origin, inf))"
USAGE = "Q.sequence(su, Q.scale(mode, bu))"
MUTANTS = (
    ("drop-family-name", "named(owner, n)", "Done{Unit{}}", "other-family"),
    ("drop-parameter-check", "uniform(origin, owner, fam, w, ixv)", "Done{Unit{}}", "other-parameters"),
    ("coverage-before-scrutinee", TYPED, f"R.bind(Q.usage, R.inferred, {COVER}, _ => {TYPED})", "untypable-and-missing"),
    ("drop-coverage", COVER, "Done{Q.unreachable}", "missing-branch"),
    ("result-fixed-index", "R.mu_result(C.ctx, C.ops, origin, mo, ixv, sv)",
     'R.mu_result(C.ctx, C.ops, origin, mo, [V.VIn{S.SMu{"N", []}, V.VACtor{"succ"}, []}], sv)', "vector-index"),
    ("result-fixed-value", "R.c_eval(C.ctx, C.ops, origin, scrut)", 'R.c_eval(C.ctx, C.ops, origin, T.In{S.SMu{"N", []}, T.ACtor{"zero"}, []})', "self-one"),
    ("alternative-join", USAGE, "Q.alternative(su, Q.scale(mode, bu))", "both-read-1-1"),
    ("drop-scale", USAGE, "Q.sequence(su, bu)", "branch-read-w-1"),
    ("drop-scrutinee-usage", USAGE, "Q.scale(mode, bu)", "scrutinee-read-1-1"),
    ("fixed-scrutinee-quantity", INFER, "R.infer_scrutinee(C.ctx, C.ops, origin, mode, Q.Many{}, scrut)", "erased-scrutinee-many"),
)


def argv_of(row):
    _, text, family, motive, mode, quantity, scrutinee, groups, _ = row
    return [text, family, motive, mode, quantity, scrutinee, *" / ".join(groups).split()]



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
        raise RuntimeError(f"elim case count or names changed: {len(CASES)}")
    WORK.mkdir(parents=True, exist_ok=True)
    for name in ("result.json", "failures.json"):
        (WORK / name).unlink(missing_ok=True)
    core = module("elim_core", ROOT / "dev/test-core-erasure.py")
    build = module("elim_build", ROOT / "dev/build.py")
    core.WORK = WORK
    source = ROOT / "test/recursor-elim.bend"

    def snapshot():
        paths = set(core.sources()) | {"dev/test-recursor-elim.py", "dev/test-recursor-body.py", "dev/test-recursor-layout.py"}
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
        raise RuntimeError(f"{len(failures)} elim mismatches; see {WORK / 'failures.json'}")
    mutations = []
    if args.mutations:
        mutation_path = "lib/kernel_recursor_elim.bend"
        original = (ROOT / mutation_path).read_text()
        for name, before, after, witness in MUTANTS:
            if original.count(before) != 1:
                raise RuntimeError(f"mutant {name} site changed")
            tree = WORK / f"mutant-{name}"
            for path in build.dependencies(source):
                dest = tree / path.relative_to(ROOT)
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, dest)
            (tree / mutation_path).write_text(original.replace(before, after))
            mutant = tree / "test/recursor-elim.bend"
            output = tree / "checks.js"
            core.run(f"mutant-{name}-check", [binary, mutant, "--check-only"], env)
            core.run(f"mutant-{name}-compile", [binary, mutant, "-o", output], env)
            row = next(row for row in CASES if row[0] == witness)
            result = core.run(f"mutant-{name}", [pins["tools"]["bun"]["path"], output, *argv_of(row)], env)
            if result["stderr"] or result["stdout"] == row[-1] + "\n":
                raise RuntimeError(f"mutant {name} did not fail its behavioral witness")
            mutations.append({"name": name, "case": witness, "killed": True, "observation": result})
    if snapshot() != hashes:
        raise RuntimeError("source changed during elim validation")
    record = {"schema": 1, "cases": len(CASES), "sources": hashes, "hosts": observations, "mutations": mutations,
              "scope": "Checked recursor scrutinee typing and the elimination result type over the declaration parameters; ambient locals and captures, scrutinee types at instantiated parameters, routing of the kernel Elim term, public elim hypotheses, delayed recursive evaluation, erasure, families without constructors, mutual and nondirect recursion remain pending."}
    (WORK / "result.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"RECURSOR-ELIM PASS cases={len(CASES)} hosts={','.join(hosts)} mutants={len(mutations)}")


if __name__ == "__main__":
    main()
