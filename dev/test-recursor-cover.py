#!/usr/bin/env python3
"""Check recursor branch coverage and the joined branch usage on the pinned Bend hosts."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/recursor-cover"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


BODY = module("recursor_body_cases", ROOT / "dev/test-recursor-body.py")
N, NT, OUT_N, ZHIDE = BODY.N, BODY.NT, BODY.OUT_N, BODY.ZHIDE
OUT = "\nmu Out : Type 0 with | stop : Out | both : (field : N) -> (previous : Out) -> Out"
OUT_T = BODY.former("Out")
LIST = OUT_N + "\n" + BODY.LIST
VEC = BODY.ROWS["indexed-child"][1] + OUT
T3 = OUT_N + "\nmu T3 : Type 0 with | leaf : T3 | one : (child : T3) -> T3 | fork : (left : T3) -> (right : T3) -> T3"
JOIN = OUT_N + "\nmu W (x : Out) : Type 0 with | a : W x | b : W x"
AFF = OUT_N + "\n" + BODY.ROWS["quantities-and-order"][1]
EMPTY = OUT_N + "\nmu E : Type 0 with"
INDEX_0 = "FAIL\nunbound: de Bruijn index 0 is outside the context"
ABSENT = "FAIL\nunbound: absent is not a constructor of N"
WRONG = "FAIL\nmismatch: the motive is built for Wrong and the scrutinee is at N"


def accept(params=0, uses=""):
    return f"PASS origin={params} uses={uses}"


def missing(owner, key):
    return f"FAIL\nmissing branch: the elimination of {owner} has no branch at {key}"


def repeated(owner, key):
    return f"FAIL\nwrong leg: the elimination of {owner} repeats the branch at {key}"


# A row is name, source, family, motive, checking mode, branch groups, expected output.
# A group is a key, a body, then quantity and name pairs. Body index 0 is the last leg binder.
Z, S = "zero stop", "succ both w n w ih"
BAD_Z, BAD_S = "zero v0", "succ v1 w n w ih"
VS = "0 i w value w tail w ih"
LEAF, ONE, FORK = "leaf stop", "one v0 w c w ih", "fork v0 w l w r w li w ri"
STEP = "0 child 0 ih"
CASES = (
    ("nat", OUT_N, "N", "result", "w", (Z, S), accept()),
    ("nat-reversed", OUT_N, "N", "result", "w", (S, Z), accept()),
    ("nat-self", BODY.AT, "N", "self", "w", ("zero at-zero", "succ next w n w ih"), accept()),
    ("list", LIST, "List", "result", "w", ("nil stop", "cons v0 w h w t w ih"), accept(1)),
    ("vector", VEC, "V", "result", "w", ("vz stop", "vs v0 " + VS), accept()),
    ("vector-index", BODY.VEC, "V", "index", "w", ("vz tag-zero", "vs grow " + VS), accept()),
    ("three-constructors", T3, "T3", "result", "w", (LEAF, ONE, "fork v1 w l w r w li w ri"), accept()),
    ("three-shuffled", T3, "T3", "result", "w", (FORK, LEAF, ONE), accept()),
    ("zero-child", ZHIDE, "Z", "result", "w", ("base stop", "step hide " + STEP), accept()),
    ("single-constructor", BODY.BOX, "B", "result", "w", ("box one 1 value",), accept()),
    ("single-parameter", BODY.WRAP, "Wrap", "constant", "w", ("wrap v0",), accept(1, "x:1..1")),
    ("join-every-branch", JOIN, "W", "result", "w", ("a v0", "b v0"), accept(1, "x:1..1")),
    ("join-first-branch", JOIN, "W", "result", "w", ("a v0", "b stop"), accept(1, "x:0..1")),
    ("join-last-branch", JOIN, "W", "result", "w", ("a stop", "b v0"), accept(1, "x:0..1")),
    ("join-no-branch", JOIN, "W", "result", "w", ("a stop", "b stop"), accept(1)),
    ("join-linear-mode", JOIN, "W", "result", "1", ("a v0", "b v0"), accept(1, "x:1..1")),
    ("join-erased-mode", JOIN, "W", "result", "0", ("a v0", "b v0"), accept(1)),
    ("erased-mode", ZHIDE, "Z", "result", "0", ("base stop", "step v0 " + STEP), accept()),
    ("missing-first", OUT_N, "N", "result", "w", (S,), missing("N", "zero")),
    ("missing-last", OUT_N, "N", "result", "w", (Z,), missing("N", "succ")),
    ("no-branches", OUT_N, "N", "result", "w", (), missing("N", "zero")),
    ("three-missing-middle", T3, "T3", "result", "w", (LEAF, FORK), missing("T3", "one")),
    ("repeated-first", OUT_N, "N", "result", "w", (Z, Z, S), repeated("N", "zero")),
    ("repeated-last", OUT_N, "N", "result", "w", (Z, S, S), repeated("N", "succ")),
    ("unknown-key", OUT_N, "N", "result", "w", (Z, "absent stop"), missing("N", "succ")),
    ("extra-key", OUT_N, "N", "result", "w", (Z, S, "absent stop"), ABSENT),
    ("address-key", OUT_N, "N", "result", "w", ("@leg stop", Z, S), "FAIL\nwrong leg: a branch of N takes the constructor address"),
    ("wrong-first-body", OUT_N, "N", "result", "w", (BAD_Z, S), INDEX_0),
    ("wrong-last-body", OUT_N, "N", "result", "w", (Z, BAD_S), BODY.mismatch(NT, OUT_T)),
    ("both-wrong", OUT_N, "N", "result", "w", (BAD_Z, BAD_S), INDEX_0),
    ("both-wrong-reversed", OUT_N, "N", "result", "w", (BAD_S, BAD_Z), INDEX_0),
    ("three-wrong-middle", T3, "T3", "result", "w", (LEAF, "one v1 w c w ih", FORK), BODY.mismatch(BODY.former("T3"), OUT_T)),
    ("wrong-body-and-missing", OUT_N, "N", "result", "w", (BAD_Z,), missing("N", "succ")),
    ("wrong-body-and-extra", OUT_N, "N", "result", "w", (BAD_Z, S, "absent stop"), ABSENT),
    ("arity-one-branch", OUT_N, "N", "result", "w", (Z, "succ both w n"), BODY.arity("succ", 1, 2)),
    ("nullary-extra-binder", OUT_N, "N", "result", "w", ("zero stop w x", S), BODY.arity("zero", 1, 0)),
    ("field-quantity", OUT_N, "N", "result", "w", (Z, "succ both 0 n w ih"), BODY.marked("n", "0", "field", "w")),
    ("hypothesis-quantity", OUT_N, "N", "result", "w", (Z, "succ both w n 0 ih"), BODY.marked("ih", "0", "hypothesis", "w")),
    ("zero-hypothesis-runtime", ZHIDE, "Z", "result", "w", ("base stop", "step v0 " + STEP), BODY.erased("ih")),
    ("affine-child", AFF, "T", "result", "w", ("leaf stop", "fork stop"), BODY.AFFINE),
    ("wrong-motive", OUT_N, "N", "wrong-family", "w", (Z, S), WRONG),
    ("wrong-motive-and-missing", OUT_N, "N", "wrong-family", "w", (Z,), missing("N", "succ")),
    ("missing-family", OUT_N, "Missing", "result", "w", (Z,), "FAIL\nunbound: the family Missing is not declared"),
    ("empty-family", EMPTY, "E", "result", "w", (), "FAIL\nnot yet: recursor coverage for a family without constructors is not supported"),
    ("unknown-mode", OUT_N, "N", "bogus", "w", (Z, S), "FAIL\nexpected a known motive mode"),
)
WALK = "walk(names, c, scope, mode, owner, mo, branches)"
COVER = "Rec.rules(Unit, c, R.mu_cover(C.ctx, owner, names, branches))"
MUTANTS = (
    ("drop-coverage", COVER, "Done{Unit{}}", "extra-key"),
    ("coverage-after-bodies", f"R.bind(Unit, Q.usage, {COVER}, _ => {WALK})",
     f"R.bind(Q.usage, Q.usage, {WALK}, uses => R.map_result(Unit, Q.usage, _ => uses, {COVER}))", "wrong-body-and-missing"),
    ("walk-branch-order", f"_ => {WALK}",
     "_ => R.bind(List<&2, String>, Q.usage, Rec.rules(List<&2, String>, c, R.mu_keys(C.ctx, owner, branches)), keys => walk(keys, c, scope, mode, owner, mo, branches))", "both-wrong-reversed"),
    ("first-constructor-only", "walk(rest, c, scope, mode, owner, mo, branches)",
     "R.map_result(F.Int63.t, Q.usage, _ => Q.unreachable, Done{R.length(String, rest)})", "wrong-last-body"),
    ("fixed-key", "R.branch_leg(V.VACtor{key}, branches)", 'R.branch_leg(V.VACtor{"zero"}, branches)', "nat"),
    ("sequence-join", "Q.alternative(uses, tail)", "Q.sequence(uses, tail)", "join-every-branch"),
    ("empty-base", "case []: Done{Q.unreachable}", "case []: Done{Q.empty}", "join-every-branch"),
    ("drop-empty-guard", "inhabited(all)", "Done{all}", "empty-family"),
    ("erased-bodies", "Body.check(c, scope, mode, owner, key, mo, lg)", "Body.check(c, scope, Q.Zero{}, owner, key, mo, lg)", "zero-hypothesis-runtime"),
)


def argv_of(row):
    _, text, family, mode, grade, groups, _ = row
    return [text, family, mode, grade, *" / ".join(groups).split()]


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
    if len(CASES) != 45 or len({row[0] for row in CASES}) != len(CASES):
        raise RuntimeError(f"cover case count or names changed: {len(CASES)}")
    WORK.mkdir(parents=True, exist_ok=True)
    for name in ("result.json", "failures.json"):
        (WORK / name).unlink(missing_ok=True)
    core = module("cover_core", ROOT / "dev/test-core-erasure.py")
    build = module("cover_build", ROOT / "dev/build.py")
    core.WORK = WORK
    source = ROOT / "test/recursor-cover.bend"

    def snapshot():
        paths = set(core.sources()) | {"dev/test-recursor-cover.py", "dev/test-recursor-body.py", "dev/test-recursor-layout.py"}
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
        raise RuntimeError(f"{len(failures)} cover mismatches; see {WORK / 'failures.json'}")
    mutations = []
    if args.mutations:
        mutation_path = "lib/kernel_recursor_cover.bend"
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
            mutant = tree / "test/recursor-cover.bend"
            output = tree / "checks.js"
            core.run(f"mutant-{name}-check", [binary, mutant, "--check-only"], env)
            core.run(f"mutant-{name}-compile", [binary, mutant, "-o", output], env)
            row = next(row for row in CASES if row[0] == witness)
            result = core.run(f"mutant-{name}", [pins["tools"]["bun"]["path"], output, *argv_of(row)], env)
            if result["stderr"] or result["stdout"] == row[-1] + "\n":
                raise RuntimeError(f"mutant {name} did not fail its behavioral witness")
            mutations.append({"name": name, "case": witness, "killed": True, "observation": result})
    if snapshot() != hashes:
        raise RuntimeError("source changed during cover validation")
    record = {"schema": 1, "cases": len(CASES), "sources": hashes, "hosts": observations, "mutations": mutations,
              "scope": "Checked recursor branch coverage and joined branch usage; ambient captures, scrutinee typing and the elimination result type, public elim hypotheses, delayed evaluation, erasure, families without constructors, mutual and nondirect recursion remain pending."}
    (WORK / "result.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"RECURSOR-COVER PASS cases={len(CASES)} hosts={','.join(hosts)} mutants={len(mutations)}")


if __name__ == "__main__":
    main()
