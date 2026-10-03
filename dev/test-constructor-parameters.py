#!/usr/bin/env python3
"""Public parameter inference, dependent fields, refusal and host regressions."""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "_build/constructor-parameters"
BOX = "mu Box (0 A : Type 0) : Type 0 { box : A -> Box A }\n"
BOTH = "mu Both (0 A : Type 0) (0 B : Type 0) : Type 0 { both : A -> B -> Both A B }\n"
DEP = "mu DepBox (0 A : Type 0) : Type 1 { depBox : (0 B : Type 0) -> B -> A -> DepBox A }\n"
AT = "mu At (0 x : Nat) : (0 y : Nat) -> Type 0 { at : At x x }\n"
LIST = "mu List (0 A : Type 0) : Type 0 { nil : List A; cons : A -> List A -> List A }\n"
MUTUAL = "mu Ev (0 A : Type 0) : Type 0 { enil : Ev A; econs : A -> Od A -> Ev A } and Od (0 A : Type 0) : Type 0 { ocons : A -> Ev A -> Od A }\n"
LONG = 300
# The earlier per-field and whole-term kernel checks made this literal exceed the 120 s host timeout.
LONG_LIMIT_S = 60
LONG_LIST = LIST + "def value : List Nat := " + "".join(f"cons {k} (" for k in range(LONG, 0, -1)) + "nil" + ")" * LONG + "\n"
CASES = [
    ("annotation", BOX + "def value : Box Nat := box 7\n", None),
    ("nested", BOX + "def value : Box (Box Nat) := box (box 7)\n", None),
    ("generic", BOX + "def value : (0 A : Type 0) -> A -> Box A := fun (0 A : Type 0) (a : A) => box a\n", None),
    ("alias", BOX + "def Alias : Type 0 := Box Nat\ndef value : Alias := box 7\n", None),
    ("argument", BOX + "def read : Box Nat -> Nat := fun (b : Box Nat) => elim b as x in Box return Nat { fun (n : Nat) => n }\ndef value : Nat := read (box 7)\n", None),
    ("nullary", "mu Maybe (0 A : Type 0) : Type 0 { none : Maybe A; some : A -> Maybe A }\ndef value : Maybe Nat := none\n", None),
    ("parameter-order", BOX + BOTH + "def value : Both (Box Nat) (prod ()) := both (box 7) (tuple ())\n", None),
    ("dependent-fields", DEP + "def value : DepBox Nat := depBox (prod ()) (tuple ()) 7\n", None),
    ("indexed", AT + "def value : At 7 7 := at\n", None),
    ("open-index", AT + "def value : (0 x : Nat) -> At x x := fun (0 x : Nat) => at\n", None),
    ("shadowed", BOX + "def value : (box : Nat -> Box Nat) -> Box Nat := fun (box : Nat -> Box Nat) => box 7\n", None),
    ("erased-argument", BOX + "def ignore : (0 b : Box Nat) -> Nat := fun (0 b : Box Nat) => 0\ndef value : (0 n : Nat) -> Nat := fun (0 n : Nat) => ignore (box n)\n", None),
    ("list", LIST + "def value : List Nat := cons 1 (cons 2 nil)\n", None),
    ("mutual", MUTUAL + "def value : Ev Nat := econs 1 (ocons 2 enil)\n", None),
    ("annotated-scrutinee", BOX + "def value : Nat := elim (box 7 : Box Nat) as x in Box return Nat { fun (n : Nat) => n }\n", None),
    ("plain-wrapper", BOX + "mu W : Type 0 { w : Box Nat -> W }\ndef value : W := w (box 7)\n", None),
    ("plain-dependent", BOX + "mu S : Type 1 { s : (0 B : Type 0) -> B -> S }\ndef value : S := s (Box Nat) (box 7)\n", None),
    ("long-list", LONG_LIST, None),
    ("wrong-field", BOX + "def value : Box (prod ()) := box 7\n", "mismatch: the term has type Nat and the expected type is (Ran SColl 0 (Sec SColl 0 []))"),
    ("missing-field", BOX + "def value : Box Nat := box\n", "takes 1 arguments"),
    ("excess-field", BOX + "def value : Box Nat := box 7 8\n", "takes 1 arguments"),
    ("wrong-family", BOX + "mu Other (0 A : Type 0) : Type 0 { other : A -> Other A }\ndef value : Box Nat := other 7\n", "the constructor other of Other cannot have the expected family Box"),
    ("wrong-index", AT + "def value : At 0 1 := at\n", "mismatch: the constructor at of At gives the index 0 and the type asks for 1"),
    ("wrong-dependent-field", DEP + "def value : DepBox Nat := depBox (prod ()) 7 8\n", "mismatch: the term has type Nat and the expected type is (Ran SColl 0 (Sec SColl 0 []))"),
    ("wrong-parameter-order", BOTH + "def value : Both Nat (prod ()) := both (tuple ()) 7\n", "mismatch: a tuple needs a right former as its expected type"),
    ("unconstrained", BOX + "def value : Nat := elim (box 7) as x in Box return Nat { fun (n : Nat) => n }\n", "needs an expected type"),
    # Pending: a family argument gets no expected type, so a parameterized constructor there is refused.
    ("family-argument", BOX + "mu Ib2 : (0 b : Box Nat) -> Type 0 { ib2 : (0 b : Box Nat) -> Ib2 b }\ndef value : Ib2 (box 1) := ib2 (box 1)\n", "cannot infer: the constructor box needs an expected type"),
    ("erased-runtime-use", BOX + "def value : (0 n : Nat) -> Box Nat := fun (0 n : Nat) => box n\n", "quantity: the erased binder n is read in a runtime position"),
    ("uat-prop-equality", "mu EqNat (0 x : Nat) : (0 y : Nat) -> Prop { reflNat : EqNat x x }\n", "index above universe"),
    ("uat-dependent-record", "record Cat : Type 1 { Obj : Type 0; Hom : Obj -> Obj -> Type 0; }\n", "unbound: Obj"),
]
# Each positive case: the definition count, one literal substring of its checked `--print` output
# and one literal substring of its `--erased` output.
GOLDENS = {
    'alias': (2, 'def value : Alias := (In SMu Box [] (ACtor box) [7])', 'fun value () : union mu<Box> := KTag mu<Box> 0 [KLit 7]'),
    'annotated-scrutinee': (1, '((In SMu Box [] (ACtor box) [7]) : (Lan SMu Box [] (Sec SColl 1 [ => Nat])))', 'KCase mu<Box> (KTag mu<Box> 0 [KLit 7]) [{0 1 (KVar 0)}]'),
    'annotation': (1, 'def value : (Lan SMu Box [] (Sec SColl 1 [ => Nat])) := (In SMu Box [] (ACtor box) [7])', 'fun value () : union mu<Box> := KTag mu<Box> 0 [KLit 7]'),
    'argument': (2, '(APt w (In SMu Box [] (ACtor box) [7]))', 'fun value () : union nat := KTail (KGlobal read) [KTag mu<Box> 0 [KLit 7]]'),
    'dependent-fields': (1, '(In SMu DepBox [] (ACtor depBox) [(Ran SColl 0 (Sec SColl 0 [])); (Sec SColl 0 []); 7])', 'fun value () : union mu<DepBox> := KTag mu<DepBox> 0 [KErased; KLit 7]'),
    'erased-argument': (2, '(APt 0 (In SMu Box [] (ACtor box) [n]))', 'fun value () : union nat := KTail (KGlobal ignore) []'),
    'generic': (1, '[a => (In SMu Box [] (ACtor box) [a])]', 'fun value (union any) : union mu<Box> := KTag mu<Box> 0 [KVar 0]'),
    'indexed': (1, '(In SMu At [7] (ACtor at) [])', 'fun value () : union mu<At> := KTag mu<At> 0 []'),
    'list': (1, '(In SMu List [] (ACtor cons) [1; (In SMu List [] (ACtor cons) [2; (In SMu List [] (ACtor nil) [])])])', 'fun value () : union mu<List> := KTag mu<List> 1 [KLit 1; KTag mu<List> 1 [KLit 2; KTag mu<List> 0 []]]'),
    'long-list': (1, '(In SMu List [] (ACtor cons) [1; (In SMu List [] (ACtor nil) [])])', 'KTag mu<List> 1 [KLit 1; KTag mu<List> 0 []]'),
    'mutual': (1, '(In SMu Ev [] (ACtor econs) [1; (In SMu Od [] (ACtor ocons) [2; (In SMu Ev [] (ACtor enil) [])])])', 'fun value () : union mu<Ev> := KTag mu<Ev> 1 [KLit 1; KTag mu<Od> 0 [KLit 2; KTag mu<Ev> 0 []]]'),
    'nested': (1, '(In SMu Box [] (ACtor box) [(In SMu Box [] (ACtor box) [7])])', 'fun value () : union mu<Box> := KTag mu<Box> 0 [KTag mu<Box> 0 [KLit 7]]'),
    'nullary': (1, '(In SMu Maybe [] (ACtor none) [])', 'fun value () : union mu<Maybe> := KTag mu<Maybe> 0 []'),
    'open-index': (1, '[x => (In SMu At [x] (ACtor at) [])]', 'fun value () : union mu<At> := KTag mu<At> 0 []'),
    'parameter-order': (1, '(In SMu Both [] (ACtor both) [(In SMu Box [] (ACtor box) [7]); (Sec SColl 0 [])])', 'fun value () : union mu<Both> := KTag mu<Both> 0 [KTag mu<Box> 0 [KLit 7]; KErased]'),
    'plain-dependent': (1, '(In SMu S [] (ACtor s) [(Lan SMu Box [] (Sec SColl 1 [ => Nat])); (In SMu Box [] (ACtor box) [7])])', 'fun value () : union mu<S> := KTag mu<S> 0 [KTag mu<Box> 0 [KLit 7]]'),
    'plain-wrapper': (1, '(In SMu W [] (ACtor w) [(In SMu Box [] (ACtor box) [7])])', 'fun value () : union mu<W> := KTag mu<W> 0 [KTag mu<Box> 0 [KLit 7]]'),
    'shadowed': (1, '[box => (Out SPi w _ Nat (APt w 7) box)]', 'fun value (func fn<1>) : union mu<Box> := KTail (KVar 0) [KLit 7]'),
}
# name, file, anchor, replacement, case, expected stderr marker of the refusal that kills the mutant.
MUTANTS = [
    ("expected-type", "surface/elab.bend", "Constructor.expected(go, c, f, ct, args, ty)", "elab_ctor_ref(go, c, f, ct, args)", "annotation", "the constructor box needs an expected type"),
    ("parameter-order", "surface/constructor.bend", "reverse(vs, [])", "vs", "parameter-order", "the constructor box of Box needs the family Box as its expected type"),
    ("dependent-environment", "surface/constructor.bend", "value <> env", "env", "dependent-fields", "unbound: de Bruijn index 2 is outside the environment"),
    ("plain-fields", "surface/elab.bend", "Constructor.arguments(go, fields, c, [], args)", "collect(Syn.t, T.t, K.ctx, ctx => s => go(ctx, None{}, s), c, args)", "plain-wrapper", "the constructor box needs an expected type"),
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def sources():
    paths = {ROOT / p for p in ("Makefile", "dev/test-constructor-parameters.py", "dev/test-cli.py", "dev/house-bend.py", "dev/bend-policy.json", "dev/build.py", "dev/cli.py", "dev/toolchain.json", "sole-comb", "examples/constructor-parameters.sole-comb", "examples/EXPECTATIONS.json", "corpus/refuse/family-parameter-ctor.sole-comb")}
    for directory in ("bin", "lib", "surface", "erase"):
        paths.update((ROOT / directory).glob("*.bend"))
    return {str(p.relative_to(ROOT)): digest(p) for p in sorted(paths)}


def run(root, path, host, mode):
    args = [str(root / "sole-comb"), "check", "--host", host, "--print" if mode == "checked" else "--erased", str(path)]
    return subprocess.run(args, cwd=root, capture_output=True, timeout=900)


def refused(result, path, marker):
    err = result.stderr.decode()
    return result.returncode == 1 and not result.stdout and err.startswith(f"CHECK {path} FAIL ") and marker in err


def first_line(data):
    return data.decode().split("\n", 1)[0]


def mutate(name, file, old, new, case, marker, path):
    target = WORK / "mutants" / name
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(ROOT, target, ignore=shutil.ignore_patterns("_build", ".git", ".gatework", ".kanon-*", "__pycache__"))
    changed = target / file
    text = changed.read_text()
    if text.count(old) != 1:
        raise RuntimeError(f"{name}: mutation site is not unique")
    changed.write_text(text.replace(old, new, 1))
    # A source or host compile failure never counts as a semantic kill.
    built = subprocess.run([sys.executable, "-P", "dev/build.py", "--backend", "js"], cwd=target, capture_output=True, timeout=1800)
    (WORK / f"mutant-{name}-build.stdout").write_bytes(built.stdout)
    (WORK / f"mutant-{name}-build.stderr").write_bytes(built.stderr)
    if built.returncode:
        raise RuntimeError(f"{name}: mutant failed to build: {built.stderr.decode()}")
    result = run(target, path, "bun", "checked")
    (WORK / f"mutant-{name}.stdout").write_bytes(result.stdout)
    (WORK / f"mutant-{name}.stderr").write_bytes(result.stderr)
    kill = "refused" if refused(result, path, marker) else None
    return {"name": name, "file": file, "case": case, "marker": marker, "compile_exit": built.returncode, "exit": result.returncode,
            "stderr_first_line": first_line(result.stderr), "stdout_sha256": sha(result.stdout), "stderr_sha256": sha(result.stderr), "kill": kill}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hosts", default="bun,node-worker,native")
    parser.add_argument("--mutations", action="store_true")
    args = parser.parse_args()
    hosts = args.hosts.split(",")
    if not hosts or len(set(hosts)) != len(hosts) or any(h not in ("bun", "node-worker", "native") for h in hosts):
        parser.error("hosts must be a unique list of bun,node-worker,native")
    if args.mutations and "bun" not in hosts:
        parser.error("mutation checks require bun")
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    if sorted(GOLDENS) != sorted(name for name, _, marker in CASES if not marker):
        raise RuntimeError("every positive case needs exactly one golden")
    before = sources()
    observations, outputs, paths = [], {}, {}
    for name, source, marker in CASES:
        path = WORK / f"{name}.sole-comb"
        path.write_text(source)
        paths[name] = path
        for host in hosts:
            for mode in ("checked",) if marker else ("checked", "erased"):
                start = time.monotonic()
                result = run(ROOT, path, host, mode)
                seconds = round(time.monotonic() - start, 3)
                stem = WORK / f"{name}-{host}-{mode}"
                stem.with_suffix(".stdout").write_bytes(result.stdout)
                stem.with_suffix(".stderr").write_bytes(result.stderr)
                if marker and not refused(result, path, marker):
                    raise RuntimeError(f"{name}/{host}/{mode}: exit={result.returncode}: {result.stderr.decode()}")
                if not marker:
                    defs, golden, erased = GOLDENS[name]
                    text = result.stdout.decode()
                    if result.returncode or result.stderr or not text.endswith("\n") or not text.startswith(f"CHECK {path} defs={defs} ok\n"):
                        raise RuntimeError(f"{name}/{host}/{mode}: exit={result.returncode}: malformed successful output: {result.stderr.decode()}")
                    if mode == "checked" and golden not in text:
                        raise RuntimeError(f"{name}/{host}: checked output lacks its golden {golden!r}")
                    if mode == "erased" and erased not in text:
                        raise RuntimeError(f"{name}/{host}: erased output lacks its golden {erased!r}")
                    if name == "long-list" and seconds >= LONG_LIMIT_S:
                        raise RuntimeError(f"{name}/{host}/{mode}: {seconds} s is not under {LONG_LIMIT_S} s")
                    key = name, mode
                    if key in outputs and outputs[key] != result.stdout:
                        raise RuntimeError(f"{name}/{mode}: host output differs")
                    outputs[key] = result.stdout
                observations.append({"case": name, "host": host, "mode": mode, "exit": result.returncode, "marker": marker, "seconds": seconds, "stdout_sha256": sha(result.stdout), "stderr_sha256": sha(result.stderr)})
    mutants = []
    if args.mutations:
        mutants = [mutate(name, file, old, new, case, marker, paths[case]) for name, file, old, new, case, marker in MUTANTS]
        survivors = [m for m in mutants if m["kill"] is None]
        if survivors:
            raise RuntimeError("semantic mutants survived: " + "; ".join(f"{m['name']}: exit={m['exit']} {m['stderr_first_line']}" for m in survivors))
    if sources() != before:
        raise RuntimeError("validation sources changed during the run")
    goldens = {name: {"defs": defs, "checked": checked, "erased": erased} for name, (defs, checked, erased) in GOLDENS.items()}
    long_list = {f"{o['host']}/{o['mode']}": o["seconds"] for o in observations if o["case"] == "long-list"}
    record = {"schema": 3, "scope": "expected-type constructor parameters", "hosts": hosts, "cases": len(CASES), "goldens": goldens,
              "long_list": {"cells": LONG, "limit_s": LONG_LIMIT_S, "seconds": long_list},
              "observations": observations, "mutants": mutants, "sources": before, "result": "pass"}
    (WORK / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"CONSTRUCTOR PARAMETERS PASS cases={len(CASES)} hosts={len(hosts)} observations={len(observations)} goldens={len(GOLDENS)} mutants={len(mutants)}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"CONSTRUCTOR PARAMETERS FAIL: {error}", file=sys.stderr)
        sys.exit(1)
