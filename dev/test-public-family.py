#!/usr/bin/env python3
"""Check native families against frozen Kanon output and isolated sugar mutations."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/public-family"
SOURCE = "examples/families.sole-comb"
ORACLE_SOURCE = "test/public-families.kan"
PROBE = "test/public-family-order.sole-comb"
# Derived by hand from the erasure rules; the probe has no Kanon oracle.
PROBE_EXPECTED = "\n".join((
    f"CHECK {PROBE} defs=5 ok",
    "rec [mu<B>; leg<mu<B>,0>; leg<mu<B>,1>]",
    "fun yes () : union mu<B> := KTag mu<B> 0 []",
    "rec [mu<B>; leg<mu<B>,0>; leg<mu<B>,1>]",
    "fun no () : union mu<B> := KTag mu<B> 1 []",
    "rec [mu<B>; nat; leg<mu<B>,0>; leg<mu<B>,1>]",
    "fun pick (union mu<B>) : union nat := KCase mu<B> (KVar 0) [{0 0 (KLit 1)}; {1 0 (KLit 2)}]",
    "rec [mu<P>; leg<mu<P>,0,union nat,union nat>]",
    "fun made () : union mu<P> := KTag mu<P> 0 [KLit 3; KLit 4]",
    "rec [mu<P>; nat; leg<mu<P>,0,union nat,union nat>]",
    "fun fst (union mu<P>) : union nat := KCase mu<P> (KVar 0) [{0 2 (KVar 1)}]",
)) + "\n"
GOLDENS = (
    "fun first () : union mu<N> := KTag mu<N> 0 []",
    "fun second () : union mu<N> := KTag mu<N> 1 [KTag mu<N> 0 []]",
    "fun read (union mu<N>) : union nat := KCase mu<N> (KVar 0) [{0 0 (KLit 1)}; {1 1 (KLit 2)}]",
    "fun ghostValue () : union mu<Ghost> := KTag mu<Ghost> 0 [KLit 7]",
    "fun ghostRead (union mu<Ghost>) : union nat := KCase mu<Ghost> (KVar 0) [{0 1 (KVar 0)}]",
    "fun copy (union mu<V>) : union mu<V> := KCase mu<V> (KVar 0) [{0 0 (KTag mu<V> 0 [])}; {1 2 (KTag mu<V> 1 [KVar 1; KVar 0])}]",
    "fun pu () : union mu<Pack> := KTag mu<Pack> 0 [KErased; KLit 9]",
    "fun last (union mu<Pack>) : union nat := KCase mu<Pack> (KVar 0) [{0 2 (KVar 0)}]",
    "fun emptyRead () : union nat := KCase mu<Empty> (KGlobal empty) []",
    "rec [mu<Left>; mu<Right>; leg<mu<Left>,0>; leg<mu<Left>,1,union mu<Right>>; leg<mu<Right>,0,union mu<Left>>]",
    "fun capture$0 (union nat, union nat, union nat) : union nat := KTail (KGlobal natAdd) [KVar 2; KApp (KGlobal natAdd) [KVar 1; KVar 0]]",
    "fun capture (union nat, union mu<Ghost>, union nat) : union nat := KTail (KCase mu<Ghost> (KVar 1) [{0 1 (KClos capture$0 1 [KVar 3; KVar 0])}]) [KVar 0]",
    "fun nullFunction (union mu<N>, union nat) : union nat := KTail (KCase mu<N> (KVar 1) [{0 0 (KClos nullFunction$0 1 [])}; {1 1 (KClos nullFunction$1 1 [])}]) [KVar 0]",
    "fun boxRead (union mu<Box>) : union nat := KCase mu<Box> (KVar 0) [{0 1 (KLit 5)}]",
    "fun mutualValue () : union mu<Left> := KTag mu<Left> 1 [KTag mu<Right> 0 [KTag mu<Left> 0 []]]",
    "fun rightRead (union mu<Right>) : union mu<Left> := KCase mu<Right> (KVar 0) [{0 1 (KVar 0)}]",
)
# Each mutant names the observations that kill it. A refusal of the well-typed source is the
# expected observation for result-lambda: the checker sees the arm result at the wrong type.
MUTANTS = (
    ("constructor-order", "L.reverse(S.fam_ctor, ctors, [])", "ctors", PROBE, ("output-changed",)),
    ("field-order", "L.reverse(S.field, acc, [])", "acc", PROBE, ("output-changed",)),
    ("result-lambda", "body(bs, value)", "value", SOURCE, ("output-changed", "refused")),
    ("field-annotation", "S.Field{q, name, Some{ty}}", "S.Field{q, name, None{}}", "corpus/refuse/family-arm-domain.sole-comb", ("admitted",)),
)


def kill_kind(result, path, expected):
    """Classify one mutant observation; None means the mutant survived."""
    out, err = result.stdout.decode(), result.stderr.decode()
    if result.returncode == 0 and not err and path in expected and out != expected[path]:
        return "output-changed"
    if result.returncode == 1 and not out and err.startswith(f"CHECK {path} FAIL "):
        return "refused"
    if result.returncode == 0 and not err and path not in expected and out.startswith(f"CHECK {path} defs=1 ok\n"):
        return "admitted"
    return None


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sources():
    paths = {ROOT / path for path in (
        SOURCE, ORACLE_SOURCE, PROBE, "Makefile", "dev/test-public-family.py", "dev/test-cli.py",
        "dev/house-bend.py", "dev/bend-policy.json", "dev/build.py", "dev/cli.py", "sole-comb",
        "dev/toolchain.json", "examples/EXPECTATIONS.json", "dev/reference-fixtures.py",
        "dev/reference-fixtures/manifest.json", "dev/reference-fixtures/public-family.json",
        "dev/validation/stage-a-family-erasure-reference.json")}
    for directory in ("bin", "lib", "surface", "erase"):
        paths.update((ROOT / directory).glob("*.bend"))
    paths.update((ROOT / "corpus/refuse").glob("*.sole-comb"))
    return {str(p.relative_to(ROOT)): digest(p) for p in sorted(paths)}


def run(name, argv, cwd=ROOT, expected=0):
    result = subprocess.run(list(map(str, argv)), cwd=cwd, capture_output=True, timeout=900)
    (WORK / f"{name}.stdout").write_bytes(result.stdout)
    (WORK / f"{name}.stderr").write_bytes(result.stderr)
    if expected is not None and result.returncode != expected:
        raise RuntimeError(f"{name}: expected exit {expected}, got {result.returncode}\n{result.stderr.decode()}")
    return result


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
    before = sources()
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    spec = importlib.util.spec_from_file_location("public_family_reference", ROOT / "dev/reference-fixtures.py")
    reference = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reference)
    oracle, provenance = reference.load(pins, "public-family", [[SOURCE, digest(ROOT / SOURCE), digest(ROOT / ORACLE_SOURCE)]])
    if not isinstance(oracle, str) or not oracle or any(line not in oracle.splitlines() for line in GOLDENS):
        raise RuntimeError("public family reference differs from independent literal expectations")
    expected = f"CHECK {SOURCE} defs=23 ok\n{oracle}"
    observations = []
    for host in hosts:
        result = run(host, [ROOT / "sole-comb", "check", "--host", host, "--erased", SOURCE])
        if result.stderr or result.stdout.decode() != expected:
            raise RuntimeError(f"{host}: public family erasure differs from pinned reference")
        observations.append({"host": host, "path": SOURCE, "exit": result.returncode, "stdout": result.stdout.decode(), "stderr": ""})
        probe = run(f"{host}-probe", [ROOT / "sole-comb", "check", "--host", host, "--erased", PROBE])
        if probe.stderr or probe.stdout.decode() != PROBE_EXPECTED:
            raise RuntimeError(f"{host}: constructor and field order probe differs from its literal expectation")
        observations.append({"host": host, "path": PROBE, "exit": probe.returncode, "stdout": probe.stdout.decode(), "stderr": ""})
        print(f"PASS public families {host}: exact erased output, {len(GOLDENS)} literal goldens, exact order probe", flush=True)
    mutations = []
    if args.mutations:
        outputs = {SOURCE: expected, PROBE: PROBE_EXPECTED}
        for name, old, new, path, kills in MUTANTS:
            dest = WORK / f"mutant-{name}"
            if dest.exists():
                shutil.rmtree(dest)
            shutil.copytree(ROOT, dest, ignore=shutil.ignore_patterns(".git", "_build", ".gatework", ".kanon*", "__pycache__"))
            source = dest / "surface/family.bend"
            text = source.read_text()
            if text.count(old) != 1:
                raise RuntimeError(f"{name}: mutation anchor is not unique")
            source.write_text(text.replace(old, new))
            # A source or host compile failure never counts as a semantic kill.
            run(f"{name}-build", [sys.executable, "-P", "dev/build.py", "--backend", "js"], cwd=dest)
            result = run(name, [dest / "sole-comb", "check", "--host", "bun", "--erased", path], cwd=dest, expected=None)
            kill = kill_kind(result, path, outputs)
            if kill not in kills:
                raise RuntimeError(f"{name}: mutant survived or failed outside an accepted observation ({kill})")
            mutations.append({"name": name, "path": path, "compile_exit": 0, "exit": result.returncode,
                              "stdout": result.stdout.decode(), "stderr": result.stderr.decode(), "kill": kill})
            print(f"PASS public family mutant: {name} ({kill})", flush=True)
    if sources() != before:
        raise RuntimeError("public family sources changed during validation")
    record = {"schema": 1, "revision": pins["kanon"]["revision"], "hosts": hosts, "sources": before, "probe": PROBE,
              "oracle": provenance, "observations": observations, "independent_goldens": len(GOLDENS), "mutations": mutations}
    (WORK / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"PUBLIC-FAMILY PASS hosts={','.join(hosts)} goldens={len(GOLDENS)} mutants={len(mutations)}", flush=True)


if __name__ == "__main__":
    main()
