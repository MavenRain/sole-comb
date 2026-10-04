#!/usr/bin/env python3
"""Compare field-directed constructor inference with explicit annotations."""
import argparse
import hashlib
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "_build/constructor-inference"
spec = importlib.util.spec_from_file_location("parameters", ROOT / "dev/test-constructor-parameters.py")
parameters = importlib.util.module_from_spec(spec)
spec.loader.exec_module(parameters)
BOX, BOTH, LIST = parameters.BOX, parameters.BOTH, parameters.LIST
IX = "mu Ix : (0 n : Nat) -> Type 0 { ix : (0 n : Nat) -> Ix n }\n"


def elim(term, family, result, arms):
    return f"elim ({term}) as x in {family} return {result} {{ {arms} }}"


# name, declarations, inferred scrutinee, explicit type, result type, arms
PAIRS = [
    ("box", BOX, "box 7", "Box Nat", "Nat", "fun (n : Nat) => n"),
    ("nested", BOX, "box (box 7)", "Box (Box Nat)", "Box Nat", "fun (b : Box Nat) => b"),
    ("two-parameters", BOTH, "both 7 (8 : Nat)", "Both Nat Nat", "Nat", "fun (a : Nat) (b : Nat) => natAdd a b"),
    ("distinct-parameters", BOTH, "both 7 (tuple () : prod ())", "Both Nat (prod ())", "Nat", "fun (a : Nat) (b : prod ()) => a"),
    ("dependent-field", parameters.DEP, "depBox Nat 3 7", "DepBox Nat", "Nat", "fun (0 B : Type 0) (b : B) (n : Nat) => n"),
    ("recursive-tail", LIST, "cons 1 nil", "List Nat", "Nat", "0; fun (n : Nat) (xs : List Nat) => n"),
    ("recursive-nested", LIST, "cons 1 (cons 2 nil)", "List Nat", "Nat", "0; fun (n : Nat) (xs : List Nat) => n"),
    ("nominal-field", BOX + "mu Wrap (0 A : Type 0) : Type 0 { wrap : Box A -> Wrap A }\n", "wrap (box 7)", "Wrap Nat", "Box Nat", "fun (b : Box Nat) => b"),
    ("nominal-nested", BOX + "mu Wrap (0 A : Type 0) : Type 0 { wrap : Box (Box A) -> Wrap A }\n", "wrap (box (box 7))", "Wrap Nat", "Box (Box Nat)", "fun (b : Box (Box Nat)) => b"),
    ("nominal-repeated", BOTH + "mu Twin (0 A : Type 0) : Type 0 { twin : Both A A -> Twin A }\n", "twin (both 7 8)", "Twin Nat", "Both Nat Nat", "fun (b : Both Nat Nat) => b"),
    ("alias", BOX + "def N : Type 0 := Nat\ndef n : N := 7\n", "box n", "Box N", "Nat", "fun (n : Nat) => n"),
    ("parameterless", "mu Token : Type 0 { token : Token }\n", "token", "Token", "Nat", "7"),
    ("index", "mu Ix : (0 n : Nat) -> Type 0 { ix : (0 n : Nat) -> Ix n }\n", "ix 7", "Ix 7", "Nat", "fun (0 n : Nat) => 7"),
    ("family-index", "mu At (0 A : Type 0) : (0 n : Nat) -> Type 0 { at : (0 n : Nat) -> A -> At A n }\n", "at 7 8", "At Nat 7", "Nat", "fun (0 n : Nat) (a : Nat) => a"),
    ("value-parameter", "mu Ix : (0 n : Nat) -> Type 0 { ix : (0 n : Nat) -> Ix n }\nmu Wrap (0 n : Nat) : Type 0 { wrap : Ix n -> Wrap n }\n", "wrap (ix 7)", "Wrap 7", "Nat", "fun (v : Ix 7) => 7"),
    ("solved-slot", BOTH + "mu W2 (0 A : Type 0) : Type 0 { w2 : Both A (Nat -> A) -> W2 A }\ndef g4 : Nat -> Nat := fun (n : Nat) => n\n", "w2 (both 7 g4)", "W2 Nat", "Nat", "fun (b : Both Nat (Nat -> Nat)) => 0"),
    ("pi-before-slot", BOTH + "mu W3 (0 A : Type 0) : Type 0 { w3 : Both (Nat -> A) A -> W3 A }\ndef g4 : Nat -> Nat := fun (n : Nat) => n\n", "w3 (both g4 7)", "W3 Nat", "Nat", "fun (b : Both (Nat -> Nat) Nat) => 0"),
    ("parameter-index", IX + "mu Pt (0 A : Type 0) (0 n : Nat) : (0 m : Nat) -> Type 0 { pt : A -> Ix n -> Pt A n n }\n", "pt 5 (ix 7)", "Pt Nat 7 7", "Nat", "fun (a : Nat) (v : Ix 7) => a"),
    ("pi-field", "mu R (0 A : Type 0) : Type 0 { r : (Nat -> A) -> A -> R A }\ndef g : Nat -> Nat := fun (n : Nat) => n\n", "r g 7", "R Nat", "Nat", "fun (h : Nat -> Nat) (a : Nat) => a"),
    ("nested-section", BOX + "mu WN (0 A : Type 0) : Type 0 { wn : Box (Nat -> A) -> A -> WN A }\ndef g4 : Nat -> Nat := fun (n : Nat) => n\n", "wn (box g4) 7", "WN Nat", "Nat", "fun (b : Box (Nat -> Nat)) (a : Nat) => a"),
]
OPEN = [
    ("open-type", BOX, "(0 A : Type 0) -> A -> A", "fun (0 A : Type 0) (a : A) => ", "box a", "Box A", "A", "fun (b : A) => b"),
    ("open-value", BOX, "Nat -> Nat", "fun (a : Nat) => ", "box a", "Box Nat", "Nat", "fun (b : Nat) => b"),
]
# A pair has equal checked output for the inferred and the explicit term, or a golden of the inferred `def value` line.
INNER = "the nested constructor argument is inferred, so it keeps its checked annotation"
ALIAS = "the explicit type names the alias N; inference gives the unfolded type Nat"
GOLDENS = {
    "nested": (INNER, "def value : (Lan SMu Box [] (Sec SColl 1 [ => Nat])) := (Elim SMu Box [] ((In SMu Box [] (ACtor box) [((In SMu Box [] (ACtor box) [7]) : (Lan SMu Box [] (Sec SColl 1 [ => Nat])))]) : (Lan SMu Box [] (Sec SColl 1 [ => (Lan SMu Box [] (Sec SColl 1 [ => Nat]))]))) as x return (Lan SMu Box [] (Sec SColl 1 [ => Nat])) with | (ACtor box) b => b)"),
    "nominal-field": (INNER, "def value : (Lan SMu Box [] (Sec SColl 1 [ => Nat])) := (Elim SMu Wrap [] ((In SMu Wrap [] (ACtor wrap) [((In SMu Box [] (ACtor box) [7]) : (Lan SMu Box [] (Sec SColl 1 [ => Nat])))]) : (Lan SMu Wrap [] (Sec SColl 1 [ => Nat]))) as x return (Lan SMu Box [] (Sec SColl 1 [ => Nat])) with | (ACtor wrap) b => b)"),
    "nominal-nested": (INNER, "def value : (Lan SMu Box [] (Sec SColl 1 [ => (Lan SMu Box [] (Sec SColl 1 [ => Nat]))])) := (Elim SMu Wrap [] ((In SMu Wrap [] (ACtor wrap) [((In SMu Box [] (ACtor box) [((In SMu Box [] (ACtor box) [7]) : (Lan SMu Box [] (Sec SColl 1 [ => Nat])))]) : (Lan SMu Box [] (Sec SColl 1 [ => (Lan SMu Box [] (Sec SColl 1 [ => Nat]))])))]) : (Lan SMu Wrap [] (Sec SColl 1 [ => Nat]))) as x return (Lan SMu Box [] (Sec SColl 1 [ => (Lan SMu Box [] (Sec SColl 1 [ => Nat]))])) with | (ACtor wrap) b => b)"),
    "nominal-repeated": (INNER, "def value : (Lan SMu Both [] (Sec SColl 2 [ => Nat;  => Nat])) := (Elim SMu Twin [] ((In SMu Twin [] (ACtor twin) [((In SMu Both [] (ACtor both) [7; 8]) : (Lan SMu Both [] (Sec SColl 2 [ => Nat;  => Nat])))]) : (Lan SMu Twin [] (Sec SColl 1 [ => Nat]))) as x return (Lan SMu Both [] (Sec SColl 2 [ => Nat;  => Nat])) with | (ACtor twin) b => b)"),
    "alias": (ALIAS, "def value : Nat := (Elim SMu Box [] ((In SMu Box [] (ACtor box) [n]) : (Lan SMu Box [] (Sec SColl 1 [ => Nat]))) as x return Nat with | (ACtor box) n => n)"),
    "value-parameter": (INNER, "def value : Nat := (Elim SMu Wrap [] ((In SMu Wrap [] (ACtor wrap) [((In SMu Ix [7] (ACtor ix) [7]) : (Lan SMu Ix [7] (Sec SColl 0 [])))]) : (Lan SMu Wrap [] (Sec SColl 1 [ => 7]))) as x return Nat with | (ACtor wrap) v => 7)"),
    "solved-slot": (INNER, "def value : Nat := (Elim SMu W2 [] ((In SMu W2 [] (ACtor w2) [((In SMu Both [] (ACtor both) [7; g4]) : (Lan SMu Both [] (Sec SColl 2 [ => Nat;  => (Ran SPi w _ Nat Nat)])))]) : (Lan SMu W2 [] (Sec SColl 1 [ => Nat]))) as x return Nat with | (ACtor w2) b => 0)"),
    "pi-before-slot": (INNER, "def value : Nat := (Elim SMu W3 [] ((In SMu W3 [] (ACtor w3) [((In SMu Both [] (ACtor both) [g4; 7]) : (Lan SMu Both [] (Sec SColl 2 [ => (Ran SPi w _ Nat Nat);  => Nat])))]) : (Lan SMu W3 [] (Sec SColl 1 [ => Nat]))) as x return Nat with | (ACtor w3) b => 0)"),
    "parameter-index": (INNER, "def value : Nat := (Elim SMu Pt [7] ((In SMu Pt [7] (ACtor pt) [5; ((In SMu Ix [7] (ACtor ix) [7]) : (Lan SMu Ix [7] (Sec SColl 0 [])))]) : (Lan SMu Pt [7] (Sec SColl 2 [ => Nat;  => 7]))) as x return Nat with | (ACtor pt) a v => a)"),
    "nested-section": (INNER, "def value : Nat := (Elim SMu WN [] ((In SMu WN [] (ACtor wn) [((In SMu Box [] (ACtor box) [g4]) : (Lan SMu Box [] (Sec SColl 1 [ => (Ran SPi w _ Nat Nat)]))); 7]) : (Lan SMu WN [] (Sec SColl 1 [ => Nat]))) as x return Nat with | (ACtor wn) b a => a)"),
}
REFUSALS = [
    ("nullary", LIST + "def value : Nat := " + elim("nil", "List", "Nat", "0; fun (n : Nat) (xs : List Nat) => n") + "\n", "the constructor nil needs an expected type or an inferable field for the parameter A"),
    ("phantom", "mu P (0 A : Type 0) : Type 0 { p : Nat -> P A }\ndef value : Nat := " + elim("p 7", "P", "Nat", "fun (n : Nat) => n") + "\n", "the constructor p needs an expected type or an inferable field for the parameter A"),
    ("conflict", BOTH + "mu Twin (0 A : Type 0) : Type 0 { twin : Both A A -> Twin A }\ndef bad : Both Nat (prod ()) := both 7 (tuple ())\ndef value : Nat := " + elim("twin bad", "Twin", "Nat", "fun (b : Both Nat Nat) => 0") + "\n", "field 1 of the constructor twin gives inconsistent parameters: the argument gives (Ran SColl 0 (Sec SColl 0 [])) where Nat is required"),
    ("missing", BOX + "def value : Nat := " + elim("box", "Box", "Nat", "fun (n : Nat) => n") + "\n", "takes 1 arguments"),
    ("excess", BOX + "def value : Nat := " + elim("box 7 8", "Box", "Nat", "fun (n : Nat) => n") + "\n", "takes 1 arguments"),
    ("wrong-dependent-field", parameters.DEP + "def value : Nat := " + elim("depBox Nat (tuple ()) 7", "DepBox", "Nat", "fun (0 B : Type 0) (b : B) (n : Nat) => n") + "\n", "a tuple needs a right former as its expected type"),
    ("usage", BOTH + "def value : (1 n : Nat) -> Nat := fun (1 n : Nat) => " + elim("both n n", "Both", "Nat", "fun (a : Nat) (b : Nat) => natAdd a b") + "\n", "quantity"),
    ("section-field", "mu R (0 A : Type 0) : Type 0 { r : (Nat -> A) -> A -> R A }\ndef value : Nat := " + elim("r (fun (n : Nat) => n) 7", "R", "Nat", "fun (h : Nat -> Nat) (a : Nat) => a") + "\n", "the constructor r needs an expected type or an inferable field: field 1 has no inferable type (a section has no type of its own"),
]
MUTANTS = [
    ("infer-slot", "V.var(F.Int63.add(base, i))", "V.var(F.Int63.add(base, F.Int63.zero))", "distinct-parameters", "inconsistent parameters"),
    ("infer-fields", "solve(c, base, np, [F.Pair2{dom, actual}], ps, field_site(name, k))", "M.Pure{ps}", "box", "the constructor box needs an expected type or an inferable field for the parameter A"),
    ("infer-nominal", "zip(site, append(xs, ix), append(ys, jx))", "zip(site, ix, jx)", "nominal-field", "the constructor wrap needs an expected type or an inferable field for the parameter A"),
    ("infer-indices", "zip(site, append(xs, ix), append(ys, jx))", "zip(site, xs, ys)", "value-parameter", "the constructor wrap needs an expected type or an inferable field for the parameter n"),
    ("infer-annotation", 'T.Ann{T.In{S.SMu{family, it}, T.ACtor{name}, terms(ts, [])},\n              T.Lan{S.SMu{family, it}, T.Sec{S.SColl{np}, legs(pt)}}}', 'T.In{S.SMu{family, it}, T.ACtor{name}, terms(ts, [])}', "box", "an injection has no type of its own"),
    ("drop-reverse", "append(vs, C.reverse(pv, []))", "append(vs, pv)", "parameter-index", "mismatch: the term has type Type 1 and the expected type is Nat"),
    ("occurs-scan", "open_term(pending(T.scan_term(False{}, [], tm, rest)))", "open_term(rest)", "solved-slot", "unbound: de Bruijn index -4294967297 is outside the environment"),
]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def source_hashes():
    paths = [ROOT / "dev/test-constructor-inference.py", ROOT / "dev/test-constructor-parameters.py", ROOT / "dev/bend-policy.json", ROOT / "dev/house-bend.py", ROOT / "dev/toolchain.json", ROOT / "Makefile", ROOT / "examples/constructor-inference.sole-comb", ROOT / "examples/EXPECTATIONS.json"]
    for directory in ("bin", "lib", "surface", "erase"):
        paths.extend((ROOT / directory).glob("*.bend"))
    return {str(p.relative_to(ROOT)): digest(p.read_bytes()) for p in sorted(paths)}


def run(path, host, mode, marker=None):
    result = parameters.run(ROOT, path, host, mode)
    stem = WORK / f"{path.stem}-{host}-{mode}"
    stem.with_suffix(".stdout").write_bytes(result.stdout)
    stem.with_suffix(".stderr").write_bytes(result.stderr)
    if marker:
        if not parameters.refused(result, path, marker):
            raise RuntimeError(f"{path.name}/{host}: invalid refusal: {result.stderr.decode()}")
    elif result.returncode or result.stderr or not result.stdout.startswith(f"CHECK {path} defs=".encode()):
        raise RuntimeError(f"{path.name}/{host}/{mode}: exit={result.returncode}: {result.stderr.decode()}")
    return result.stdout.partition(b"\n")[2], {"case": path.stem, "host": host, "mode": mode, "exit": result.returncode, "stdout_sha256": digest(result.stdout), "stderr_sha256": digest(result.stderr)}


def value_line(inferred, explicit):
    """Return the inferred `def value` line if it is the only line that differs from the explicit output."""
    a, b = inferred.decode().split("\n"), explicit.decode().split("\n")
    lines = [x for x, y in zip(a, b) if x != y]
    return lines[0] if len(a) == len(b) and len(lines) == 1 and lines[0].startswith("def value : ") else None


def compare(name, host, bodies):
    checked = bodies["inferred", host, "checked"], bodies["explicit", host, "checked"]
    erased = bodies["inferred", host, "erased"], bodies["explicit", host, "erased"]
    if erased[0] != erased[1]:
        raise RuntimeError(f"{name}/{host}: inference changed erasure")
    reason, golden = GOLDENS.get(name, (None, None))
    if golden is None and checked[0] != checked[1]:
        raise RuntimeError(f"{name}/{host}: inferred checked output differs and the pair has no golden")
    if golden is not None and value_line(*checked) != golden:
        raise RuntimeError(f"{name}/{host}: inferred checked output does not match its golden")
    return {"case": name, "host": host, "checked_equal": checked[0] == checked[1], "golden_reason": reason,
            "inferred_checked_sha256": digest(checked[0]), "explicit_checked_sha256": digest(checked[1]),
            "erasure_equal": erased[0] == erased[1], "inferred_erased_sha256": digest(erased[0]), "explicit_erased_sha256": digest(erased[1])}


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
    before = source_hashes()
    observations, comparisons, outputs = [], [], {}
    cases = [(name, declarations, result, "", term, ty, result, arms) for name, declarations, term, ty, result, arms in PAIRS] + OPEN
    if not set(GOLDENS) <= {case[0] for case in cases}:
        raise RuntimeError(f"goldens name no pair: {sorted(set(GOLDENS) - {case[0] for case in cases})}")
    for name, declarations, ty, prefix, term, explicit, result, arms in cases:
        bodies = {}
        for variant, scrutinee in (("inferred", term), ("explicit", f"({term} : {explicit})")):
            path = WORK / f"{name}-{variant}.sole-comb"
            family = explicit.split()[0] + (" i" if name in ("index", "family-index", "parameter-index") else "")
            path.write_text(declarations + f"def value : {ty} := {prefix}" + elim(scrutinee, family, result, arms) + "\n")
            for host in hosts:
                for mode in ("checked", "erased"):
                    body, observation = run(path, host, mode)
                    observations.append(observation)
                    key = (name, variant, mode)
                    if key in outputs and outputs[key] != body:
                        raise RuntimeError(f"{name}/{variant}/{mode}: hosts disagree")
                    outputs[key] = body
                    bodies[variant, host, mode] = body
        comparisons.extend(compare(name, host, bodies) for host in hosts)
    for name, source, marker in REFUSALS:
        path = WORK / f"{name}.sole-comb"
        path.write_text(source)
        for host in hosts:
            _, observation = run(path, host, "checked", marker)
            observations.append(observation)
    for host in hosts:
        for mode in ("checked", "erased"):
            _, observation = run(ROOT / "examples/constructor-inference.sole-comb", host, mode)
            observations.append(observation)
    mutants = []
    if args.mutations:
        parameters.WORK = WORK
        for name, old, new, case, marker in MUTANTS:
            mutant = parameters.mutate(name, "surface/constructor_infer.bend", old, new, case, marker, WORK / f"{case}-inferred.sole-comb")
            if mutant["kill"] != "refused":
                raise RuntimeError(f"{name}: semantic mutant survived")
            mutants.append(mutant)
    if before != source_hashes():
        raise RuntimeError("sources changed during constructor inference checks")
    record = {"schema": 1, "sources": before, "hosts": hosts, "pairs": len(cases), "refusals": len(REFUSALS), "comparisons": comparisons, "observations": observations, "mutants": mutants}
    (WORK / "record.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"CONSTRUCTOR INFERENCE PASS pairs={len(cases)} refusals={len(REFUSALS)} hosts={len(hosts)} observations={len(observations)} mutants={len(mutants)}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"CONSTRUCTOR INFERENCE FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
