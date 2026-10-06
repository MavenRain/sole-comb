#!/usr/bin/env python3
"""Check the recursor at instantiated parameters on the pinned Bend hosts: scrutinee types whose declaration parameters are given values."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/recursor-instance"


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

# A row is name, source, family, motive, checking mode, scrutinee quantity, scrutinee, context, branch groups, expected output.
# The scrutinee type gives the declaration parameters their values: nil-n is nil at List N, p-an is p at P A N under a
# given local A, keep-m is keep m at U m m. The origin count is the given locals plus the declaration parameters.
# A defined parameter is an erased alias: a body that reads its slot at a runtime mode is refused, and the motive
# modes parameter and earlier-parameter print the value, not the parameter name. A usage row lists the innermost
# variable first. A context named la or la0 binds xs at List A for a given type A; ln and nln bind xs at List N.
# A given local at Type 0 is refused as a parameter leg: the kernel checks the leg against Type 1, the universe of the
# parameter domain (the ambient observation). The contexts a1, na1 and la1 bind A at Type 1 for the rows that need a
# given local as a parameter value.
NILS = ("nil stop", "cons v0 w h w t w ih")
HEAD = ("nil v2", "cons v2 w h w t w ih")
HEAD_N = ("nil v1", "cons v2 w h w t w ih")
FORK = ("leaf stop", "fork stop w left w right w ihl w ihr")
CASES = (
    ("zero-valid-shape", ELIM.OUT_N, "N", "result", "w", "w", "zero", "none", ELIM.NAT, accept(0, OUT_T)),
    ("zero-bad-shape", ELIM.OUT_N, "N", "result", "w", "w", "zero-bad-shape", "none", ELIM.NAT, "FAIL\nmismatch: the diagram is not a section of the declared width"),
    ("zero-noncollection", ELIM.OUT_N, "N", "result", "w", "w", "zero-noncollection", "none", ELIM.NAT, "FAIL\nmismatch: the diagram is not a section of the declared width"),
    ("nil-bad-shape", LIST, "List", "result", "w", "w", "nil-bad-shape", "none", NILS, "FAIL\nmismatch: the diagram is not a section of the declared width"),
    ("nil-bound-parameter", LIST, "List", "result", "w", "w", "nil-bound-parameter", "none", NILS, "FAIL\nmismatch: the diagram is not a section of the declared width"),
    ("nil-at-n", LIST, "List", "result", "w", "w", "nil-n", "none", NILS, accept(1, OUT_T)),
    ("cons-at-n", LIST, "List", "result", "w", "w", "cons-n", "none", NILS, accept(1, OUT_T)),
    ("nil-at-nat", LIST, "List", "result", "w", "w", "nil-nat", "none", NILS, accept(1, OUT_T)),
    ("nil-linear", LIST, "List", "result", "1", "1", "nil-n", "none", NILS, accept(1, OUT_T)),
    ("nil-erased-mode", LIST, "List", "result", "0", "0", "nil-n", "none", NILS, accept(1, OUT_T)),
    ("pair-at-n-n", P, "P", "result", "w", "w", "p-nn", "none", ("p stop",), accept(2, OUT_T)),
    ("pair-at-local-n", P, "P", "result", "w", "w", "p-an", "a", ("p stop",), LEG_0),
    ("parameter-value", LIST, "List", "parameter", "w", "w", "nil-n", "n", HEAD_N, accept(2, NT, "n:0..w")),
    ("parameter-head", LIST, "List", "parameter", "w", "w", "cons-n", "n", HEAD_N, accept(2, NT, "n:0..w")),
    ("parameter-second", P, "P", "parameter", "w", "w", "p-an", "na1", ("p v3",), accept(4, NT, "n:w..w")),
    ("earlier-parameter-local", P, "P", "earlier-parameter", "w", "w", "p-an", "na1", ("p v3",), BODY.mismatch(NT, "A")),
    ("local-list-linear", LIST, "List", "result", "1", "1", "p0", "ln", NILS, accept(2, OUT_T, "xs:1..1")),
    ("local-list-many", LIST, "List", "result", "w", "w", "p0", "ln", NILS, accept(2, OUT_T, "xs:w..w")),
    ("local-list-head", LIST, "List", "parameter", "w", "w", "p0", "nln", HEAD, accept(3, NT, "xs:w..w;n:0..w")),
    ("local-list-head-linear", LIST, "List", "parameter", "1", "1", "p0", "nln", HEAD, accept(3, NT, "xs:1..1;n:0..1")),
    ("local-list-abstract", LIST, "List", "result", "1", "1", "p0", "la", NILS, LEG_0),
    ("local-list-abstract-1", LIST, "List", "result", "1", "1", "p0", "la1", NILS, accept(3, OUT_T, "xs:1..1")),
    ("local-list-abstract-type", LIST, "List", "parameter", "w", "w", "p0", "la", HEAD_N, BODY.mismatch(LIST_A, "A")),
    ("local-list-erased", LIST, "List", "result", "w", "w", "p0", "la0", NILS, BODY.erased("xs")),
    ("local-list-erased-mode", LIST, "List", "result", "0", "0", "p0", "la1", NILS, accept(3, OUT_T)),
    ("local-list-erased-quantity", LIST, "List", "result", "w", "0", "p0", "ln", NILS, ELIM.ERASED),
    ("keep-at-local", U, "U", "constant", "w", "w", "keep-m", "m", ("keep v3 w value",), accept(3, "Nat", "m:w..w")),
    ("keep-value", U, "U", "constant", "w", "w", "keep-m", "m", ("keep v0 w value",), accept(3, "Nat", "m:w..w")),
    ("parameter-slot-read", U, "U", "constant", "w", "w", "keep-m", "m", ("keep v2 w value",), BODY.erased("x")),
    ("parameter-slot-read-y", U, "U", "constant", "w", "w", "keep-m", "m", ("keep v1 w value",), BODY.erased("y")),
    ("parameter-slot-erased-mode", U, "U", "constant", "0", "0", "keep-m", "m", ("keep v2 w value",), accept(3, "Nat")),
    ("hold-linear", LIN, "L", "result", "1", "1", "hold-m", "m1", ("hold one 1 value",), accept(3, OUT_T, "m:1..1")),
    ("hold-linear-closed", LIN, "L", "result", "1", "1", "hold-m", "m1-closed", ("hold one 1 value",), accept(3, OUT_T, "m:1..1")),
    ("hold-both", LIN, "L", "result", "1", "1", "hold-m", "mk", ("hold deep 1 value",), accept(4, OUT_T, "k:1..1;m:1..1")),
    ("hold-slot-read", LIN, "L", "result", "1", "1", "hold-m", "m1", ("hold far 1 value",), BODY.erased("x")),
    ("box-linear-twice-closed", BODY.BOX, "B", "result", "1", "1", "box-local", "m1-closed", ("box pair 1 value",), BODY.linear("m")),
    ("uniform-at-nat", TREE, "T", "result", "w", "w", "leaf-nat", "none", FORK, accept(1, OUT_T)),
    ("non-uniform-at-n", TREE, "T", "result", "w", "w", "leaf-n", "none", FORK, NOT_UNIFORM),
    ("non-uniform-at-local", TREE, "T", "result", "w", "w", "leaf-a", "a", FORK, LEG_0),
    ("non-uniform-at-local-1", TREE, "T", "result", "w", "w", "leaf-a", "a1", FORK, NOT_UNIFORM),
    ("uniform-local-list", LIST, "List", "result", "w", "w", "p0", "la1", NILS, accept(3, OUT_T, "xs:w..w")),
    ("wrong-body-head", LIST, "List", "result", "w", "w", "nil-n", "none", ("nil stop", "cons v2 w h w t w ih"), BODY.mismatch(NT, OUT_T)),
    ("wrong-body-head-nat", LIST, "List", "result", "w", "w", "nil-nat", "none", ("nil stop", "cons v2 w h w t w ih"), BODY.mismatch("Nat", OUT_T)),
    ("cons-local-tail", LIST, "List", "result", "w", "w", "p0", "ln", ("nil stop", "cons near w h w t w ih"), BODY.mismatch(LIST_N, NT)),
    ("missing-cons", LIST, "List", "result", "w", "w", "nil-n", "none", ("nil stop",), ELIM.missing("List", "cons")),
    ("other-family", LIST, "Out", "result", "w", "w", "nil-n", "none", ("stop stop",), other("List", "Out")),
    ("local-not-former", LIST, "List", "result", "0", "0", "p0", "a", NILS, ELIM.NOT_FORMER),
    ("local-erased-type", LIST, "List", "result", "w", "w", "p0", "a", NILS, BODY.erased("A")),
    ("leg-not-a-type", LIST, "List", "result", "w", "w", "nil", "n", NILS, BODY.mismatch(NT, TYPE_1)),
    ("pair-swapped-at-locals", P, "P", "result", "w", "w", "pair-swapped", "na1", ("p stop",), BODY.mismatch(NT, TYPE_1)),
    ("scrutinee-above-context", P, "P", "result", "w", "w", "p-an", "none", ("p stop",), unbound(0)),
    ("unknown-context", LIST, "List", "result", "w", "w", "nil-n", "bogus", NILS, UNKNOWN_CONTEXT),
    ("unknown-scrutinee", LIST, "List", "result", "w", "w", "bogus", "none", NILS, UNKNOWN_SCRUTINEE),
)
# Each mutant breaks one instance site. The harness names the file of each mutant: the scope value and the value
# order and the result usage live in the elim module, the defined parameter in the recursor module, and the expected
# parameter list and its comparison in the uniform module.
USAGE = "Q.sequence(su, Q.scale(mode, bu))"
MUTANTS = (
    ("diagram-width-unchecked", "lib/kernel_recursor_instance.bend", "F.Int63.equal(actual, width)", "True{}", "zero-bad-shape"),
    ("parameter-binders-unchecked", "lib/kernel_recursor_instance.bend", "nullary(legs)", "True{}", "nil-bound-parameter"),
    ("instance-as-abstract", "lib/kernel_recursor_elim.bend", "Rec.Instance{vals}", "Rec.Abstract{}", "parameter-value"),
    ("drop-reverse", "lib/kernel_recursor_elim.bend", "R.rev_append(V.t, env, [])", "env", "parameter-second"),
    ("usage-drops-scrutinee", "lib/kernel_recursor_elim.bend", USAGE, "Q.scale(mode, bu)", "local-list-linear"),
    ("usage-drops-branches", "lib/kernel_recursor_elim.bend", USAGE, "su", "parameter-value"),
    ("scale-many", "lib/kernel_recursor_elim.bend", "Q.scale(mode, bu)", "Q.scale(Q.Many{}, bu)", "local-list-head-linear"),
    ("instance-keeps-quantity", "lib/kernel_recursor.bend", "instance(rest, tail, C.define(x, Q.Zero{}, tyv, v, c))", "instance(rest, tail, C.define(x, Q.Many{}, tyv, v, c))", "parameter-slot-read"),
    ("open-abstract-for-instance", "lib/kernel_recursor.bend", "case Instance{values}: instance(tele, values, c)", "case Instance{_}: abstract(tele, c)", "parameter-value"),
    ("targets-abstract-for-instance", "lib/kernel_recursor_uniform.bend", "case Rec.Instance{values}: values", "case Rec.Instance{_}: levels(tele, C.size(c))", "uniform-at-nat"),
    ("uniform-flips-compare", "lib/kernel_recursor_uniform.bend",
     "eq,\n                _ => arguments(c, scope, rest, tail, es, Rec.install(scope, x, q, tyv, e, prefix)),\n                _ => Fail{mismatch()})))",
     "eq,\n                _ => Fail{mismatch()},\n                _ => arguments(c, scope, rest, tail, es, Rec.install(scope, x, q, tyv, e, prefix)))))", "cons-at-n"),
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
    if len(CASES) != 53 or len({row[0] for row in CASES}) != len(CASES):
        raise RuntimeError(f"instance case count or names changed: {len(CASES)}")
    WORK.mkdir(parents=True, exist_ok=True)
    for name in ("result.json", "failures.json"):
        (WORK / name).unlink(missing_ok=True)
    core = module("instance_core", ROOT / "dev/test-core-erasure.py")
    build = module("instance_build", ROOT / "dev/build.py")
    core.WORK = WORK
    source = ROOT / "test/recursor-instance.bend"

    def snapshot():
        paths = set(core.sources()) | {"dev/test-recursor-instance.py", "dev/test-recursor-ambient.py", "dev/test-recursor-elim.py", "dev/test-recursor-body.py", "dev/test-recursor-layout.py"}
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
        raise RuntimeError(f"{len(failures)} instance mismatches; see {WORK / 'failures.json'}")
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
            mutant = tree / "test/recursor-instance.bend"
            output = tree / "checks.js"
            core.run(f"mutant-{name}-check", [binary, mutant, "--check-only"], env)
            core.run(f"mutant-{name}-compile", [binary, mutant, "-o", output], env)
            row = next(row for row in CASES if row[0] == witness)
            result = core.run(f"mutant-{name}", [pins["tools"]["bun"]["path"], output, *argv_of(row)], env)
            if result["stderr"] or result["stdout"] == row[-1] + "\n":
                raise RuntimeError(f"mutant {name} did not fail its behavioral witness")
            mutations.append({"name": name, "path": mutation_path, "case": witness, "killed": True, "observation": result})
    if snapshot() != hashes:
        raise RuntimeError("source changed during instance validation")
    record = {"schema": 1, "cases": len(CASES), "sources": hashes, "hosts": observations, "mutations": mutations,
              "scope": "Checked recursor scrutinees at instantiated parameters: the scrutinee is inferred in the given context, its type gives the declaration parameters their values, the parameters open as erased aliases defined at those values, the recursive children are compared with the values, and the result type and the usage are reported over the given locals and the parameters; routing of the kernel Elim term, parameter rows in the result usage, public elim hypotheses, delayed recursive evaluation, erasure, families without constructors, mutual and nondirect recursion remain pending."}
    (WORK / "result.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"RECURSOR-INSTANCE PASS cases={len(CASES)} hosts={','.join(hosts)} mutants={len(mutations)}")


if __name__ == "__main__":
    main()
