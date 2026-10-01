#!/usr/bin/env python3
"""Compare public ordinary erasure with fresh pinned Kanon and binding goldens."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/core-erasure"
POSITIVE = ("examples/identity.sole-comb", "examples/arithmetic.sole-comb", "examples/erasure.sole-comb")
REFUSED = ("examples/finite-elim.sole-comb", "examples/records.sole-comb")
APPLIED = WORK / "applied-erased-arity.sole-comb"
APPLIED_SOURCE = """axiom Nat : Type 0
def g : (0 A : Type 0) -> (n : Nat) -> (0 B : Type 0) -> Nat := fun (0 A : Type 0) (n : Nat) (0 B : Type 0) => n
def h : (0 B : Type 0) -> Nat := g Nat 1
def r : Nat := h Nat
"""
APPLIED_REFUSAL = "applications whose remaining parameters all erase in type-directed erasure"


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
    paths.extend(ROOT / name for name in POSITIVE + REFUSED)
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
        result = run(f"oracle-{i}", [dest / "oracle.exe", ROOT / path], env)
        if result["stderr"] or not result["stdout"]:
            raise RuntimeError(f"oracle produced malformed success for {path}")
        expected[path] = result["stdout"]
    pin()
    return expected, {"revision": revision, "sources": hashes, "adapter_sha256": digest(dest / "oracle_adapter.ml"),
                      "compiler": run("oracle-version", ["ocamlc", "-version"], env)["stdout"].strip()}


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
    before = sources()
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    expected, provenance = oracle(pins, env)
    observations = []
    for host in hosts:
        for i, path in enumerate(POSITIVE):
            result = run(f"public-{host}-{i}", [ROOT / "sole-comb", "check", "--erased", "--host", host, path], env)
            actual = result["stdout"].partition("\n")[2]
            count = {POSITIVE[0]: 3, POSITIVE[1]: 3, POSITIVE[2]: 19}[path]
            if result["stderr"] or result["stdout"].splitlines()[0] != f"CHECK {path} defs={count} ok" or actual != expected[path]:
                raise RuntimeError(f"{host} {path}: erased public output differs from the pinned oracle\n{actual}")
            observations.append({"host": host, "file": path, **result})
        for i, path in enumerate(REFUSED):
            # A successfully checked structural program must refuse erasure explicitly.
            checked = run(f"checked-{host}-{i}", [ROOT / "sole-comb", "check", "--host", host, path], env)
            if checked["stderr"]:
                raise RuntimeError(f"{host} {path}: malformed checking success")
            result = run(f"refused-{host}-{i}", [ROOT / "sole-comb", "check", "--erased", "--host", host, path], env, expected=1)
            if result["stdout"] or "not yet:" not in result["stderr"] or "in type-directed erasure" not in result["stderr"]:
                raise RuntimeError(f"{host} {path}: missing erasure refusal: {result}")
            observations.append({"host": host, "file": path, **result})
        # An application whose remaining parameters all erase must refuse, not saturate.
        checked = run(f"checked-{host}-applied", [ROOT / "sole-comb", "check", "--host", host, APPLIED], env)
        if checked["stderr"]:
            raise RuntimeError(f"{host} {APPLIED}: malformed checking success")
        result = run(f"refused-{host}-applied", [ROOT / "sole-comb", "check", "--erased", "--host", host, APPLIED], env, expected=1)
        if result["stdout"] or "not yet:" not in result["stderr"] or APPLIED_REFUSAL not in result["stderr"]:
            raise RuntimeError(f"{host} {APPLIED}: missing erasure refusal: {result}")
        observations.append({"host": host, "file": "applied-erased-arity", **result})
        print(f"PASS ordinary erasure {host}: {len(POSITIVE)} oracle comparisons, {len(REFUSED) + 1} refusals", flush=True)
    contract_results = contracts(pins, hosts, env)
    if sources() != before:
        raise RuntimeError("erasure sources changed during validation")
    record = {"milestone": "A.5b.3.2a", "scope": "ordinary definitions, function closures, Nat and erased positions",
              "hosts": hosts, "sources": before, "oracle": provenance, "oracle_results": expected,
              "public_observations": observations, "contracts": contract_results,
              "structural_erasure": "pending", "full_erased_corpus": "pending", "wasm": "pending"}
    (WORK / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    print("PASS ordinary erasure: public observations, six contracts, source hashes stable", flush=True)


if __name__ == "__main__":
    main()
