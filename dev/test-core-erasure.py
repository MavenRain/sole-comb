#!/usr/bin/env python3
"""Compare public ordinary, product, sum and pair erasure with fresh pinned Kanon and goldens."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/core-erasure"
POSITIVE = ("examples/identity.sole-comb", "examples/arithmetic.sole-comb", "examples/erasure.sole-comb",
            "examples/product-erasure.sole-comb", "examples/finite-elim.sole-comb",
            "examples/sum-erasure.sole-comb", "examples/records.sole-comb",
            "examples/pair-erasure.sole-comb")
COUNTS = dict(zip(POSITIVE, (3, 3, 19, 40, 6, 29, 32, 33), strict=True))
ORACLE_SOURCES = {"examples/product-erasure.sole-comb": "test/product-erasure.kan",
                  "examples/finite-elim.sole-comb": "test/finite-elim-erasure.kan",
                  "examples/sum-erasure.sole-comb": "test/sum-erasure.kan",
                  "examples/records.sole-comb": "test/records-erasure.kan",
                  "examples/pair-erasure.sole-comb": "test/pair-erasure.kan"}
ORACLE_NAMES = {f"{record}_{field}": f"{record}.{field}"
                for record, fields in (("Pair", ("first", "second")),
                    ("Mixed", ("kind", "witness", "first", "second", "callback", "nested", "trailing")),
                    ("AllErased", ("kind", "witness")), ("Outer", ("erased", "kept")),
                    ("Other", ("first", "zero")), ("Packet", ("kind", "payload", "handler", "count")),
                    ("Capture", ("item",)))
                for field in fields}
ORACLE_NAME_PATTERN = re.compile(r"(?<![\w'.$])(?:" + "|".join(map(re.escape, ORACLE_NAMES)) + r")(?![\w'])")
PRODUCT_LAYOUT = "tuple<union nat,union nat,func fn<1>,struct tuple<union nat,union nat>>"
PRODUCT_GOLDENS = (
    "erased proof", "erased Mixed.witness",
    f"fun Mixed.first (struct {PRODUCT_LAYOUT}) : union nat := KProj {PRODUCT_LAYOUT} 0 (KVar 0)",
    f"fun Mixed.second (struct {PRODUCT_LAYOUT}) : union nat := KProj {PRODUCT_LAYOUT} 1 (KVar 0)",
    f"fun Mixed.nested (struct {PRODUCT_LAYOUT}) : struct tuple<union nat,union nat> := KProj {PRODUCT_LAYOUT} 3 (KVar 0)",
    "fun allErased () : struct tuple<> := KErased",
    f"rec [nat; {PRODUCT_LAYOUT}; tuple<union nat,union nat>]",
)
SUM_GOLDENS = (
    "fun first () : union sum<unit|union nat|unit> := KTag sum<unit|union nat|unit> 0 []",
    "fun middle () : union sum<unit|union nat|unit> := KTag sum<unit|union nat|unit> 1 [KLit 42]",
    "fun last () : union sum<unit|union nat|unit> := KTag sum<unit|union nat|unit> 2 []",
    "fun unwrap (union sum<unit|union nat|unit>) : union nat := KCase sum<unit|union nat|unit> (KVar 0) [{0 0 (KLit 1)}; {1 1 (KVar 0)}; {2 0 (KLit 3)}]",
    "fun typeTag () : union sum<unit|union nat> := KTag sum<unit|union nat> 0 []",
    "fun typeUnwrap (union sum<unit|union nat>) : union nat := KCase sum<unit|union nat> (KVar 0) [{0 0 (KLit 9)}; {1 1 (KVar 0)}]",
    "rec [nat; sum<func fn<1>|unit>]",
    "fun functionTag () : union sum<func fn<1>|unit> := KTag sum<func fn<1>|unit> 0 [KClos functionTag$0 1 []]",
    "fun applyTag (union sum<func fn<1>|unit>) : union nat := KCase sum<func fn<1>|unit> (KVar 0) [{0 1 (KTail (KVar 0) [KLit 5])}; {1 0 (KLit 0)}]",
    "erased impossible",
    "fun emptyCase () : union nat := KCase any (KErased) []",
    "fun erasedBinder (union sum<union nat|union nat>) : union nat := KCase sum<union nat|union nat> (KVar 0) [{0 0 (KLit 10)}; {1 0 (KLit 20)}]",
)
APPLIED = WORK / "applied-erased-arity.sole-comb"
PAIR_GOLDENS = (
    "fun both () : struct pair<union nat,union nat> := KStruct pair<union nat,union nat> [KLit 11; KLit 6]",
    "fun pointOnly () : struct pair<union nat> := KStruct pair<union nat> [KLit 3]",
    "fun fibreOnly () : struct pair<union nat> := KStruct pair<union nat> [KLit 20]",
    "fun neither () : struct pair<> := KErased",
    "fun dependent () : struct pair<union any> := KStruct pair<union any> [KLit 7]",
    "erased droppedProof", "erased droppedType",
)
APPLIED_SOURCE = """axiom Nat : Type 0
def g : (0 A : Type 0) -> (n : Nat) -> (0 B : Type 0) -> Nat := fun (0 A : Type 0) (n : Nat) (0 B : Type 0) => n
def h : (0 B : Type 0) -> Nat := g Nat 1
def r : Nat := h Nat
"""
APPLIED_REFUSAL = "applications whose remaining parameters all erase in type-directed erasure"
PAIR_PROBES = WORK / "pair-probes.sole-comb"
PAIR_PROBE_SOURCE = """axiom Nat : Type 0
def Both : Type 0 := (x : Nat) * Nat
def capture : Nat -> Both -> ((Nat -> Nat) * Nat) := fun (outer : Nat) (p : Both) => elim p { fun (x : Nat) (y : Nat) => (fun (n : Nat) => natAdd outer (natAdd x (natAdd y n)), y) }
def Dependent : Type 1 := (0 A : Type 0) * A
def repack : Dependent -> Dependent := fun (p : Dependent) => elim p { fun (0 A : Type 0) (y : A) => (A, y) }
"""
PAIR_PROBE_GOLDENS = (
    "fun capture$0 (union nat, union nat, union nat, union nat) : union nat := KTail (KGlobal natAdd) [KVar 3; KApp (KGlobal natAdd) [KVar 2; KApp (KGlobal natAdd) [KVar 1; KVar 0]]]",
    "fun capture (union nat, struct pair<union nat,union nat>) : struct pair<func fn<1>,union nat> := KLet scrut (KVar 0) (KLet x (KProj pair<union nat,union nat> 0 (KVar 0)) (KLet y (KProj pair<union nat,union nat> 1 (KVar 1)) (KStruct pair<func fn<1>,union nat> [KClos capture$0 1 [KVar 4; KVar 1; KVar 0]; KVar 0])))",
    "fun repack (struct pair<union any>) : struct pair<union any> := KLet scrut (KVar 0) (KLet y (KProj pair<union any> 0 (KVar 0)) (KStruct pair<union any> [KVar 0]))",
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def run(name, argv, env, cwd=ROOT, expected=0):
    result = subprocess.run(list(map(str, argv)), cwd=cwd, env=env, capture_output=True, timeout=900)
    (WORK / f"{name}.stdout").write_bytes(result.stdout)
    (WORK / f"{name}.stderr").write_bytes(result.stderr)
    if result.returncode != expected:
        raise RuntimeError(f"{name}: expected exit {expected}, got {result.returncode}\n"
                           + (result.stdout + result.stderr).decode(errors="replace"))
    return {"exit": result.returncode, "stdout": result.stdout.decode(), "stderr": result.stderr.decode()}


def sources():
    paths = [ROOT / name for name in ("sole-comb", "Makefile", "dev/cli.py", "dev/build.py",
             "dev/test-core-erasure.py", "dev/test-core-erasure-mutations.py", "dev/bend-policy.json",
             "dev/house-bend.py", "dev/toolchain.json", "test/core-erasure.bend", "test/core-erasure-oracle.ml")]
    for directory in ("lib", "surface", "erase", "bin"):
        paths.extend((ROOT / directory).glob("*.bend"))
    paths.extend(ROOT / name for name in POSITIVE)
    paths.extend(ROOT / name for name in ORACLE_SOURCES.values())
    return {str(p.relative_to(ROOT)): digest(p) for p in sorted(set(paths))}


def oracle(pins, env):
    upstream = Path(pins["kanon"]["checkout"])
    revision = pins["kanon"]["revision"]

    def git(*args):
        return subprocess.run(["git", *args], cwd=upstream, capture_output=True, check=True).stdout

    def pin():
        if git("rev-parse", "HEAD").decode().strip() != revision or git("status", "--porcelain"):
            raise RuntimeError("Kanon oracle checkout differs from the clean configured pin")

    pin()
    files = sorted((upstream / "lib").glob("*.ml")) + sorted((upstream / "lib").glob("*.mli"))
    files += [upstream / "surface" / f"{name}.ml" for name in ("token", "syntax", "lexer", "parser", "elab")]
    dest = WORK / "oracle"
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir()
    hashes = {}
    for path in files:
        relative = path.relative_to(upstream).as_posix()
        content = path.read_bytes()
        if content != git("show", f"{revision}:{relative}"):
            raise RuntimeError(f"oracle source differs from pin: {relative}")
        hashes[relative] = digest(path)
        (dest / path.name).write_bytes(content)
    names = sorted(p.stem.capitalize() for p in files if p.parent.name == "lib" and p.suffix == ".ml")
    (dest / "kanon_kernel.ml").write_text("\n".join(f"module {name} = {name}" for name in names) + "\n")
    shutil.copyfile(ROOT / "test/core-erasure-oracle.ml", dest / "oracle_adapter.ml")
    order = run("oracle-order", ["ocamldep", "-sort", *[p.name for p in files], "kanon_kernel.ml"], env, dest)["stdout"].split()
    run("oracle-compile", ["ocamlfind", "ocamlc", "-package", "zarith", "-linkpkg", *order,
                          "oracle_adapter.ml", "-o", "oracle.exe"], env, dest)
    expected = {}
    for i, path in enumerate(POSITIVE):
        result = run(f"oracle-{i}", [dest / "oracle.exe", ROOT / ORACLE_SOURCES.get(path, path)], env)
        if result["stderr"] or not result["stdout"]:
            raise RuntimeError(f"oracle produced malformed success for {path}")
        output = result["stdout"]
        if path in ORACLE_SOURCES:
            # The pin has no qualified surface names. Restore this fixture's namespaces.
            # Rename whole identifiers only, so no key rewrites part of a longer name.
            output = ORACLE_NAME_PATTERN.sub(lambda match: ORACLE_NAMES[match.group(0)], output)
        if path == "examples/product-erasure.sole-comb" and any(line not in output.splitlines() for line in PRODUCT_GOLDENS):
            raise RuntimeError("pinned oracle differs from independent product erasure goldens")
        if path == "examples/sum-erasure.sole-comb" and any(line not in output.splitlines() for line in SUM_GOLDENS):
            raise RuntimeError("pinned oracle differs from independent sum erasure goldens")
        if path == "examples/pair-erasure.sole-comb" and any(line not in output.splitlines() for line in PAIR_GOLDENS):
            raise RuntimeError("pinned oracle differs from independent pair erasure goldens")
        expected[path] = output
    pin()
    return expected, {"revision": revision, "sources": hashes, "adapter_sha256": digest(dest / "oracle_adapter.ml"),
                      "compiler": run("oracle-version", ["ocamlc", "-version"], env)["stdout"].strip(),
                      "source_overrides": ORACLE_SOURCES, "name_overrides": ORACLE_NAMES}


def contracts(pins, hosts, env):
    build = module("core_erasure_build", ROOT / "dev/build.py")
    binary, _, _ = build.compiler(pins)
    source = ROOT / "test/core-erasure.bend"
    run("contract-check", [binary, source, "--check-only"], env)
    results = {}
    if any(host in hosts for host in ("bun", "node-worker")):
        run("contract-js", [binary, source, "-o", WORK / "contracts.js"], env)
    for host in hosts:
        if host == "native":
            run("contract-native-build", [binary, source, "-o", WORK / "contracts.exe"], env)
            argv = [WORK / "contracts.exe"]
        elif host == "bun":
            argv = [pins["tools"]["bun"]["path"], WORK / "contracts.js"]
        else:
            argv = [pins["tools"]["node"]["path"], "-e", build.node_worker_script(pins), WORK / "contracts.js"]
        result = run(f"contract-{host}", argv, env)
        if result["stderr"] or result["stdout"].strip() not in ("PASS", '"PASS"'):
            raise RuntimeError(f"{host}: erasure contracts failed: {result}")
        results[host] = result
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hosts", default="bun,node-worker,native")
    args = parser.parse_args()
    hosts = args.hosts.split(",")
    if not hosts or len(hosts) != len(set(hosts)) or any(h not in ("bun", "node-worker", "native") for h in hosts):
        parser.error("hosts must be unique bun,node-worker,native values")
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    APPLIED.write_text(APPLIED_SOURCE)
    PAIR_PROBES.write_text(PAIR_PROBE_SOURCE)
    before = sources()
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    expected, provenance = oracle(pins, env)
    observations = []
    for host in hosts:
        for i, path in enumerate(POSITIVE):
            result = run(f"public-{host}-{i}", [ROOT / "sole-comb", "check", "--erased", "--host", host, path], env)
            actual = result["stdout"].partition("\n")[2]
            count = COUNTS[path]
            if result["stderr"] or result["stdout"].splitlines()[0] != f"CHECK {path} defs={count} ok" or actual != expected[path]:
                raise RuntimeError(f"{host} {path}: erased public output differs from the pinned oracle\n{actual}")
            observations.append({"host": host, "file": path, **result})
        # An application whose remaining parameters all erase must refuse, not saturate.
        checked = run(f"checked-{host}-applied", [ROOT / "sole-comb", "check", "--host", host, APPLIED], env)
        if checked["stderr"]:
            raise RuntimeError(f"{host} {APPLIED}: malformed checking success")
        result = run(f"refused-{host}-applied", [ROOT / "sole-comb", "check", "--erased", "--host", host, APPLIED], env, expected=1)
        if result["stdout"] or "not yet:" not in result["stderr"] or APPLIED_REFUSAL not in result["stderr"]:
            raise RuntimeError(f"{host} {APPLIED}: missing erasure refusal: {result}")
        observations.append({"host": host, "file": "applied-erased-arity", **result})
        probes = run(f"pair-probes-{host}", [ROOT / "sole-comb", "check", "--erased", "--host", host, PAIR_PROBES], env)
        if probes["stderr"] or probes["stdout"].splitlines()[0] != f"CHECK {PAIR_PROBES} defs=5 ok" or any(line not in probes["stdout"].splitlines() for line in PAIR_PROBE_GOLDENS):
            raise RuntimeError(f"{host}: erasure contracts failed for native pair probes: {probes}")
        observations.append({"host": host, "file": "pair-probes", **probes})
        print(f"PASS public erasure {host}: {len(POSITIVE)} oracle comparisons, {len(PAIR_PROBE_GOLDENS)} pair goldens, 1 refusal", flush=True)
    contract_results = contracts(pins, hosts, env)
    if sources() != before:
        raise RuntimeError("erasure sources changed during validation")
    record = {"milestone": "A.5b.3.2d", "scope": "ordinary definitions, function closures, Nat, collection products and sums, dependent pairs, erased positions",
              "hosts": hosts, "sources": before, "oracle": provenance, "oracle_results": expected,
              "public_observations": observations, "contracts": contract_results,
              "independent_goldens": {"product": len(PRODUCT_GOLDENS), "sum": len(SUM_GOLDENS), "pair": len(PAIR_GOLDENS), "pair_probes": len(PAIR_PROBE_GOLDENS)},
              "structural_erasure": "finite collections and dependent pairs supported; recursive families pending", "full_erased_corpus": "pending", "wasm": "pending"}
    (WORK / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    print("PASS ordinary, product, sum and pair erasure: public observations, direct contracts, source hashes stable", flush=True)


if __name__ == "__main__":
    main()
