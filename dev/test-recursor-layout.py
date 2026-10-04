#!/usr/bin/env python3
"""Check direct recursive-field layouts on the three pinned Bend hosts."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/recursor-layout"
N = "mu N : Type 0 with | zero : N | succ : (child : N) -> N\n"
NT = "(Lan SMu N [] (Sec SColl 0 []))"
ODD = " and Odd : Type 0 with | os : (e : Even) -> Odd"
MUTUAL = "FAIL\nnot yet: recursor layout does not support mutual families"


def golden(arity, context, fields, hypotheses):
    return f"arity={arity} context={context}\nfields={';'.join(fields)}\nih={';'.join(hypotheses)}"


CASES = (
    ("nullary", N, "N", "zero", "arity=0 context=0\nfields=\nih="),
    ("direct", N, "N", "succ", golden(2, 1, [f"0:w:child:{NT}:child:recursive[]"], [f"0:w:child:{NT}:child:recursive[]"])),
    ("quantities-and-order", "mu T : Type 0 with | leaf : T | fork : (1 left : T) -> (0 ghost : T) -> (label : Nat) -> (right : T) -> T", "T", "fork",
     golden(7, 4, ["0:1:left:(Lan SMu T [] (Sec SColl 0 [])):left:recursive[]", "1:0:ghost:(Lan SMu T [] (Sec SColl 0 [])):ghost:recursive[]", "2:w:label:Nat:label:ordinary", "3:w:right:(Lan SMu T [] (Sec SColl 0 [])):right:recursive[]"],
            ["0:1:left:(Lan SMu T [] (Sec SColl 0 [])):left:recursive[]", "1:0:ghost:(Lan SMu T [] (Sec SColl 0 [])):ghost:recursive[]", "3:w:right:(Lan SMu T [] (Sec SColl 0 [])):right:recursive[]"])),
    ("parameters", "mu Pair (0 A : Type 0) (0 B : Type 0) : Type 0 with | pair : (left : A) -> (right : B) -> Pair A B", "Pair", "pair",
     golden(2, 4, ["0:w:left:A:left:ordinary", "1:w:right:B:right:ordinary"], [])),
    ("dependent-field", "mu Pack : Type 1 with | pack : (0 A : Type 0) -> (value : A) -> Pack", "Pack", "pack",
     golden(2, 2, ["0:0:A:Type 1:A:ordinary", "1:w:value:A:value:ordinary"], [])),
    ("indexed-child", N + "mu V : (0 i : N) -> Type 0 with | vz : V zero | vs : (0 i : N) -> (value : Nat) -> (tail : V i) -> V (succ i)", "V", "vs",
     golden(4, 3, [f"0:0:i:{NT}:i:ordinary", "1:w:value:Nat:value:ordinary", "2:w:tail:(Lan SMu V [i] (Sec SColl 0 [])):tail:recursive[i]"], ["2:w:tail:(Lan SMu V [i] (Sec SColl 0 [])):tail:recursive[i]"])),
    ("recursive-parameter", "mu List (0 A : Type 0) : Type 0 with | nil : List A | cons : (head : A) -> (tail : List A) -> List A", "List", "cons",
     golden(3, 3, ["0:w:head:A:head:ordinary", "1:w:tail:(Lan SMu List [] (Sec SColl 1 [ => A])):tail:recursive[]"], ["1:w:tail:(Lan SMu List [] (Sec SColl 1 [ => A])):tail:recursive[]"])),
    ("foreign-family", N + "mu Box : Type 0 with | box : (value : N) -> Box", "Box", "box",
     golden(1, 1, [f"0:w:value:{NT}:value:ordinary"], [])),
    # The pin rejects an alias application containing self before metadata planning.
    ("source-alias-refusal", "def Id : Type 0 -> Type 0 := fun (A : Type 0) => A\nmu Alias : Type 0 with | az : Alias | astep : (child : Id Alias) -> Alias", "Alias", "astep", "FAIL\nnot yet: a family that is not strictly positive arrives at M2"),
    # A synthetic telescope field exercises normalization against a checked alias.
    ("alias-field", N + "def Alias : Type 0 := N", "@alias-field", "unused",
     golden(2, 1, [f"0:w:child:{NT}:child:recursive[]"], [f"0:w:child:{NT}:child:recursive[]"])),
    ("dependent-indices", "mu D : (0 A : Type 0) -> (0 x : A) -> Type 1 with | step : (0 A : Type 0) -> (0 x : A) -> (child : D A x) -> D A x", "D", "step",
     golden(4, 3, ["0:0:A:Type 1:A:ordinary", "1:0:x:A:x:ordinary", "2:w:child:(Lan SMu D [A; x] (Sec SColl 0 [])):child:recursive[A,x]"], ["2:w:child:(Lan SMu D [A; x] (Sec SColl 0 [])):child:recursive[A,x]"])),
    ("later-field", "mu End : Type 0 with | baseEnd : End | more : (label : Nat) -> (child : End) -> End", "End", "more",
     golden(3, 2, ["0:w:label:Nat:label:ordinary", "1:w:child:(Lan SMu End [] (Sec SColl 0 [])):child:recursive[]"], ["1:w:child:(Lan SMu End [] (Sec SColl 0 [])):child:recursive[]"])),
    ("functional-recursion", "mu W : Type 0 with | sup : (children : Nat -> W) -> W", "W", "sup", "FAIL\nnot yet: recursor layout supports direct recursive fields only"),
    ("mutual-sibling", "mu Even : Type 0 with | ez : Even | es : (o : Odd) -> Even" + ODD, "Even", "es", MUTUAL),
    ("mutual-mixed", "mu Even : Type 0 with | ez : Even | es : (x : Even) -> (o : Odd) -> Even" + ODD, "Even", "es", MUTUAL),
    ("mutual-three-cycle", "mu A : Type 0 with | a : (x : B) -> A and B : Type 0 with | b : (x : C) -> B and C : Type 0 with | c : (x : A) -> C | cz : C", "A", "a", MUTUAL),
    # A later family that uses N reaches N, but no field of N can mention it.
    ("later-user", N + "mu L : Type 0 with | lnil : L | lcons : (h : N) -> (t : L) -> L", "N", "succ",
     golden(2, 1, [f"0:w:child:{NT}:child:recursive[]"], [f"0:w:child:{NT}:child:recursive[]"])),
    ("missing-family", N, "Missing", "zero", "FAIL\nunbound: the family Missing is not declared"),
    ("wrong-constructor", N, "N", "absent", "FAIL\nunbound: absent is not a constructor of N"),
)
TEST_SOURCES = ("test/recursor-layout.bend", "test/program.bend", *(f"test/pinfront/{name}.bend" for name in (
    "elab", "elab_program", "lexer", "order", "parser", "program", "syntax", "token", "totality")))


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


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
    if len(CASES) != 19 or len({row[0] for row in CASES}) != len(CASES):
        raise RuntimeError("recursor layout case count or names changed")
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    (WORK / "failures.json").unlink(missing_ok=True)
    core = module("recursor_core", ROOT / "dev/test-core-erasure.py")
    build = module("recursor_build", ROOT / "dev/build.py")
    core.WORK = WORK

    def snapshot():
        paths = set(core.sources()) | {"dev/test-recursor-layout.py", *TEST_SOURCES}
        paths.update(str(path.relative_to(ROOT)) for path in build.dependencies(ROOT / "test/recursor-layout.bend"))
        return {path: core.digest(ROOT / path) for path in sorted(paths)}

    hashes = snapshot()
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    (ROOT / "_build/bend-cache").mkdir(parents=True, exist_ok=True)
    binary, _, _ = build.compiler(pins)
    source = ROOT / "test/recursor-layout.bend"
    core.run("check", [binary, source, "--check-only"], env)
    if set(hosts) & {"bun", "node-worker"}:
        core.run("javascript", [binary, source, "-o", WORK / "checks.js"], env)
    observations = {}
    failures = []
    for host in hosts:
        if host == "native":
            core.run("native-build", [binary, source, "-o", WORK / "checks.exe"], env)
            argv = [WORK / "checks.exe"]
        elif host == "bun":
            argv = [pins["tools"]["bun"]["path"], WORK / "checks.js"]
        else:
            argv = [pins["tools"]["node"]["path"], "-e", build.node_worker_script(pins), WORK / "checks.js"]
        results = {}
        for name, text, family, ctor, expected in CASES:
            result = core.run(f"{host}-{name}", [*argv, text, family, ctor], env)
            if result["stderr"] or result["stdout"] != expected + "\n":
                failures.append({"host": host, "case": name, "expected": expected + "\n", "observed": result})
            results[name] = result
        observations[host] = results
    if snapshot() != hashes:
        raise RuntimeError("source changed during recursor layout validation")
    if failures:
        (WORK / "failures.json").write_text(json.dumps(failures, indent=2) + "\n")
        raise RuntimeError(f"{len(failures)} layout mismatches; see {WORK / 'failures.json'}")
    mutations = []
    if args.mutations:
        candidates = (
            ("field-quantity", "Field{position, q, x, w,", "Field{position, Q.Many{}, x, w,", "quantities-and-order"),
            ("field-value", "V.var(C.size(c))", "V.var(F.Int63.zero)", "dependent-field"),
            ("ambient-context", "parameters(params, C.make(C.globals(c), C.budget(c)))", "parameters(params, c)", "nullary"),
            ("non-direct-refusal", "P.occurs(group, tm)", "False{}", "functional-recursion"),
            ("mutual-group", "reach(fams, fams, [owner])", "[owner]", "mutual-sibling"),
        )
        closure = build.dependencies(source)
        original = (ROOT / "lib/kernel_recursor.bend").read_text()
        for name, before, after, witness in candidates:
            if original.count(before) != 1:
                raise RuntimeError(f"mutation anchor changed: {name}")
            tree = WORK / f"mutant-{name}"
            for path in closure:
                dest = tree / path.relative_to(ROOT)
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, dest)
            (tree / "lib/kernel_recursor.bend").write_text(original.replace(before, after))
            mutant_source = tree / "test/recursor-layout.bend"
            output = tree / "checks.js"
            core.run(f"mutant-{name}-check", [binary, mutant_source, "--check-only"], env)
            core.run(f"mutant-{name}-compile", [binary, mutant_source, "-o", output], env)
            _, text, family, ctor, expected = next(row for row in CASES if row[0] == witness)
            result = core.run(f"mutant-{name}", [pins["tools"]["bun"]["path"], output, text, family, ctor], env)
            if result["stderr"] or result["stdout"] == expected + "\n":
                raise RuntimeError(f"mutation did not produce a clean behavioral mismatch: {name}")
            mutations.append({"name": name, "case": witness, "killed": True, "observation": result})
        if snapshot() != hashes:
            raise RuntimeError("source changed during recursor layout mutations")
    record = {"schema": 1, "cases": len(CASES), "sources": hashes, "hosts": observations,
              "mutations": mutations,
              "scope": "Direct recursive-field metadata; public recursors and delayed IH evaluation remain pending."}
    (WORK / "result.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"RECURSOR-LAYOUT PASS {len(CASES)} cases on {','.join(hosts)}")


if __name__ == "__main__":
    main()
