#!/usr/bin/env python3
"""Check recursor branch bodies against their branch plans on the pinned Bend hosts."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/recursor-body"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


BASE = module("recursor_layout_cases", ROOT / "dev/test-recursor-layout.py")
ROWS = {row[0]: row for row in BASE.UNIFORM_CASES}
N, NT = BASE.N, BASE.NT
AFFINE = "FAIL\nnot yet: recursor hypotheses for affine recursive fields are not supported"
OUT_N = N + "mu Out : Type 0 with | stop : Out | both : (field : N) -> (previous : Out) -> Out"
AT = N + "mu At : (0 n : N) -> Type 0 with | at : (0 n : N) -> At n | next : (0 n : N) -> (previous : At n) -> At (succ n)"
LIST = ROWS["recursive-parameter"][1]
PAIR = ROWS["parameters"][1]
VEC = ROWS["indexed-child"][1] + "\nmu Tag : (0 i : N) -> Type 0 with | tag : (0 i : N) -> Tag i | grow : (0 i : N) -> (previous : Tag i) -> Tag (succ i)"
TREE = "mu T : Type 0 with | leaf : T | fork : (left : T) -> (0 ghost : T) -> (label : Nat) -> (right : T) -> T"
ZHIDE = "mu Z : Type 0 with | base : Z | step : (0 child : Z) -> Z\nmu Out : Type 0 with | stop : Out | hide : (0 previous : Out) -> Out"
KEEP = "mu Z : Type 0 with | base : Z | keep : (0 ghost : Nat) -> (child : Z) -> Z"
BOX = ("mu B : Type 0 with | box : (1 value : Nat) -> B\n"
       "mu Out : Type 0 with | stop : Out | one : (1 only : Nat) -> Out | two : (1 left : Nat) -> (1 right : Nat) -> Out")
WRAP = "mu Wrap (x : Nat) : Type 0 with | wrap : Wrap x"


def former(o, args="", count=0, params=""):
    return f"(Lan SMu {o} [{args}] (Sec SColl {count} [{params}]))"


def value(o, k, fields=""):
    return f"(In SMu {o} [] (ACtor {k}) [{fields}])"


def accept(params=0, used=""):
    return f"PASS origin={params} used={used}"


def arity(key, got, want):
    return f"FAIL\nmissing branch: the recursor branch at {key} binds {got} binders and {key} takes {want} fields and hypotheses"


def marked(name, got, source, want):
    return f"FAIL\nquantity: the recursor branch binder {name} is marked {got} and the {source} marks it {want}"


def mismatch(got, want):
    return f"FAIL\nmismatch: the term has type {got} and the expected type is {want}"


def erased(name):
    return f"FAIL\nquantity: the erased binder {name} is read in a runtime position"


def linear(name):
    return f"FAIL\nquantity: the linear binder {name} must be used exactly once on every runtime path"


# A row is name, source, family, constructor, motive, checking mode, body, leg binders, expected output.
# Leg binders are quantity and name pairs. Body index 0 is the last leg binder. Hypotheses follow fields.
SUCC = "w child w ih"
CONS = "w h w t w ih"
VS = "0 i w value w tail w ih"
FORK = "w l 0 g w n w r w li 0 gi w ri"
STEP = "0 child 0 ih"
KEEPS = "0 ghost w child w ih"
CASES = (
    ("nat-zero", OUT_N, "N", "zero", "result", "w", "stop", "", accept()),
    ("nat-succ", OUT_N, "N", "succ", "result", "w", "both", "w n w ih", accept()),
    ("nat-hypothesis", N, "N", "succ", "constant", "w", "v0", SUCC, accept()),
    ("self-zero", AT, "N", "zero", "self", "w", "at-zero", "", accept()),
    ("self-succ", AT, "N", "succ", "self", "w", "next", "w n w ih", accept()),
    ("list-head", LIST, "List", "cons", "parameter", "w", "v2", CONS, accept(1)),
    ("list-hypothesis", LIST, "List", "cons", "parameter", "w", "v0", CONS, accept(1)),
    ("pair-right", PAIR, "Pair", "pair", "parameter", "w", "v0", "w l w r", accept(2)),
    ("indexed-nil", VEC, "V", "vz", "index", "w", "tag-zero", "", accept()),
    ("indexed-cons", VEC, "V", "vs", "index", "w", "grow", VS, accept()),
    ("indexed-constant", VEC, "V", "vs", "constant", "w", "v2", VS, accept()),
    ("tree-label", TREE, "T", "fork", "constant", "w", "v4", FORK, accept()),
    ("tree-left-hypothesis", TREE, "T", "fork", "constant", "w", "v2", FORK, accept()),
    ("zero-hypothesis-erased-use", ZHIDE, "Z", "step", "result", "w", "hide", STEP, accept()),
    ("linear-direct", BOX, "B", "box", "constant", "w", "v0", "1 value", accept()),
    ("linear-once", BOX, "B", "box", "result", "w", "one", "1 value", accept()),
    ("erased-mode", KEEP, "Z", "keep", "constant", "0", "v2", KEEPS, accept()),
    ("parameter-use", WRAP, "Wrap", "wrap", "constant", "w", "v0", "", accept(1, "x")),
    ("too-few", N, "N", "succ", "constant", "w", "v0", "w child", arity("succ", 1, 2)),
    ("too-many", N, "N", "succ", "constant", "w", "v0", "w a w b w c", arity("succ", 3, 2)),
    ("nullary-extra", N, "N", "zero", "constant", "w", "v0", "w x", arity("zero", 1, 0)),
    ("fields-only", TREE, "T", "fork", "constant", "w", "v0", "w l 0 g w n w r", arity("fork", 4, 7)),
    ("field-quantity", N, "N", "succ", "constant", "w", "v0", "0 child w ih", marked("child", "0", "field", "w")),
    ("hypothesis-quantity", N, "N", "succ", "constant", "w", "v0", "w child 0 ih", marked("ih", "0", "hypothesis", "w")),
    ("zero-hypothesis-many", ZHIDE, "Z", "step", "result", "w", "hide", "0 child w ih", marked("ih", "w", "hypothesis", "0")),
    ("erased-field-many", VEC, "V", "vs", "constant", "w", "v2", "w i w value w tail w ih", marked("i", "w", "field", "0")),
    ("linear-field-many", BOX, "B", "box", "constant", "w", "v0", "w value", marked("value", "w", "field", "1")),
    ("first-quantity-wins", N, "N", "succ", "constant", "w", "v0", "0 child 0 ih", marked("child", "0", "field", "w")),
    ("wrong-type", AT, "N", "succ", "self", "w", "v0", "w m w ih", mismatch(former("At", "m"), former("At", value("N", "succ", "m")))),
    ("wrong-parameter", PAIR, "Pair", "pair", "parameter", "w", "v1", "w l w r", mismatch("A", "B")),
    ("wrong-index", VEC, "V", "vs", "index", "w", "v0", "0 k w value w tail w ih", mismatch(former("Tag", "k"), former("Tag", value("N", "succ", "k")))),
    ("list-tail", LIST, "List", "cons", "parameter", "w", "v1", CONS, mismatch(former("List", count=1, params=" => A"), "A")),
    ("zero-field-runtime", KEEP, "Z", "keep", "constant", "w", "v2", KEEPS, erased("ghost")),
    ("zero-hypothesis-runtime", ZHIDE, "Z", "step", "result", "w", "v0", STEP, erased("ih")),
    ("tree-ghost-hypothesis", TREE, "T", "fork", "constant", "w", "v1", FORK, erased("gi")),
    ("linear-twice", BOX, "B", "box", "result", "w", "two", "1 value", linear("value")),
    ("linear-dropped", BOX, "B", "box", "result", "w", "stop", "1 value", linear("value")),
    ("ambient-not-captured", N, "N", "succ", "constant", "w", "v2", SUCC, "FAIL\nunbound: de Bruijn index 2 is outside the context"),
    ("affine-child", ROWS["quantities-and-order"][1], "T", "fork", "constant", "w", "v0", "", AFFINE),
    ("unknown-constructor", N, "N", "absent", "constant", "w", "v0", "", "FAIL\nunbound: absent is not a constructor of N"),
    ("missing-family", N, "Missing", "zero", "constant", "w", "v0", "", "FAIL\nunbound: the family Missing is not declared"),
    ("wrong-family", N, "N", "succ", "wrong-family", "w", "v0", SUCC, "FAIL\nmismatch: the motive is built for Wrong and the scrutinee is at N"),
    ("missing-motive-family", N, "N", "zero", "missing-family", "w", "v0", "", "FAIL\nmismatch: the motive of an elimination at N names no family"),
    ("wrong-indices", ROWS["indexed-child"][1], "V", "vs", "wrong-indices", "w", "v0", VS, "FAIL\nmismatch: the motive of V binds 0 indices and the family has 1"),
    ("invalid-motive", N, "N", "succ", "invalid", "w", "v0", SUCC, "FAIL\nuniverse: a term used as a type is not a universe:  (" + NT + " : Type 1)"),
    ("unknown-mode", N, "N", "zero", "bogus", "w", "v0", "", "FAIL\nexpected a known motive mode"),
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
    if len(CASES) != 46 or len({row[0] for row in CASES}) != len(CASES):
        raise RuntimeError(f"body case count or names changed: {len(CASES)}")
    WORK.mkdir(parents=True, exist_ok=True)
    for name in ("result.json", "failures.json"):
        (WORK / name).unlink(missing_ok=True)
    core = module("body_core", ROOT / "dev/test-core-erasure.py")
    build = module("body_build", ROOT / "dev/build.py")
    core.WORK = WORK
    source = ROOT / "test/recursor-body.bend"

    def snapshot():
        paths = set(core.sources()) | {"dev/test-recursor-body.py", "dev/test-recursor-layout.py"}
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
        for name, text, family, ctor, mode, grade, body, marks, want in CASES:
            result = core.run(f"{host}-{name}", [*argv, text, family, ctor, mode, grade, body, *marks.split()], env)
            if result["stderr"] or result["stdout"] != want + "\n":
                failures.append({"host": host, "case": name, "expected": want + "\n", "observed": result})
            results[name] = result
        observations[host] = results
    if failures:
        (WORK / "failures.json").write_text(json.dumps(failures, indent=2) + "\n")
        raise RuntimeError(f"{len(failures)} body mismatches; see {WORK / 'failures.json'}")
    mutations = []
    if args.mutations:
        mutation_path = "lib/kernel_recursor_body.bend"
        original = (ROOT / mutation_path).read_text()
        candidates = (
            ("drop-arity-guard", "F.Int63.equal(got, want)", "True{}", "too-few"),
            ("skip-quantity", "name, marked, declared)", "name, marked, marked)", "field-quantity"),
            ("source-swap", 'case Branch.FieldBinder{}: "field"', 'case Branch.FieldBinder{}: "hypothesis"', "field-quantity"),
            ("skip-close", "R.c_close(C.ctx, C.ops, inner, C.size(origin), mode, uses)", "R.Pure{uses}", "linear-dropped"),
            ("check-erased-mode", "Q.runtime(mode)", "Q.Zero{}", "zero-field-runtime"),
            ("check-outer-mode", "Q.runtime(mode)", "mode", "linear-direct"),
            ("close-erased-mode", "C.size(origin), mode, uses)", "C.size(origin), Q.Zero{}, uses)", "linear-twice"),
            ("close-size", "inner, C.size(origin),", "inner, C.size(inner),", "linear-twice"),
            ("ambient-origin", "C.make(C.globals(c), C.budget(c))", "c", "ambient-not-captured"),
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
            mutant = tree / "test/recursor-body.bend"
            output = tree / "checks.js"
            core.run(f"mutant-{name}-check", [binary, mutant, "--check-only"], env)
            core.run(f"mutant-{name}-compile", [binary, mutant, "-o", output], env)
            _, text, family, ctor, mode, grade, body, marks, want = next(row for row in CASES if row[0] == witness)
            result = core.run(f"mutant-{name}", [pins["tools"]["bun"]["path"], output, text, family, ctor, mode, grade, body, *marks.split()], env)
            if result["stderr"] or result["stdout"] == want + "\n":
                raise RuntimeError(f"mutant {name} did not fail its behavioral witness")
            mutations.append({"name": name, "case": witness, "killed": True, "observation": result})
    if snapshot() != hashes:
        raise RuntimeError("source changed during body validation")
    record = {"schema": 1, "cases": len(CASES), "sources": hashes, "hosts": observations, "mutations": mutations,
              "scope": "Checked recursor branch bodies against branch plans; constructor coverage, ambient captures, public elim hypotheses, delayed evaluation, erasure, mutual and nondirect recursion remain pending."}
    (WORK / "result.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"RECURSOR-BODY PASS cases={len(CASES)} hosts={','.join(hosts)} mutants={len(mutations)}")


if __name__ == "__main__":
    main()
