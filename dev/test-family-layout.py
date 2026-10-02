#!/usr/bin/env python3
"""Compare nominal recursive-family layouts with freshly compiled pinned Kanon."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/family-layout"
FIXTURES = {
    "test/family-layouts.kan": ("N", "Box", "V", "Pair", "Dep", "Packet", "Exists", "Left", "Right", "W"),
    "test/kanon/test/fixtures/mu-parameter-layout.kan": ("Box",),
    "test/kanon/test/fixtures/mu-constructor-scopes.kan": ("Pair", "Ix", "Dep", "Large"),
    "test/kanon/test/fixtures/mu-indexed.kan": ("N", "V"),
    "test/kanon/test/fixtures/one-recursive.kan": ("Chain",),
    "test/kanon/test/fixtures/mu-rec-mutual.kan": ("N", "A", "B"),
}
GOLDENS = (
    "N [leg<mu<N>,0>;leg<mu<N>,1,union mu<N>>]",
    "Box [leg<mu<Box>,0,union any>]",
    "V [leg<mu<V>,0>;leg<mu<V>,1,union nat,union mu<V>>]",
    "vs [drop;union nat;union mu<V>]",
    "Pair [leg<mu<Pair>,0,union any,union any>]",
    "Dep [leg<mu<Dep>,0>]",
    "packet [drop;drop;drop;func fn<1>;struct tuple<union nat,union nat>]",
    "pack [drop;union any]",
    "Left [leg<mu<Left>,0>;leg<mu<Left>,1,union mu<Right>>]",
    "Right [leg<mu<Right>,0,union mu<Left>>]",
    "w [drop;union any;drop;drop;union nat]",
)
# The test-side import closure of test/family-layout.bend; core.sources() covers lib/, surface/, erase/ and bin/.
TEST_SOURCES = ("test/family-layout.bend", "test/program.bend", *(f"test/pinfront/{name}.bend" for name in (
    "elab", "elab_program", "lexer", "order", "parser", "program", "syntax", "token", "totality")))


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hosts", default="bun,node-worker,native")
    parser.add_argument("--mutations", action="store_true")
    args = parser.parse_args()
    hosts = args.hosts.split(",")
    if not hosts or len(set(hosts)) != len(hosts) or set(hosts) - {"bun", "node-worker", "native"}:
        parser.error("hosts must be a nonempty unique subset of bun,node-worker,native")
    if args.mutations and "bun" not in hosts:
        parser.error("--mutations runs the mutants on Bun only, so --hosts must include bun")
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    core = module("family_layout_core", ROOT / "dev/test-core-erasure.py")
    build = module("family_layout_build", ROOT / "dev/build.py")
    core.WORK = WORK

    def snapshot():
        paths = list(core.sources()) + ["dev/test-family-layout.py", *TEST_SOURCES, *FIXTURES]
        return {path: core.digest(ROOT / path) for path in sorted(set(paths))}

    hashes = snapshot()
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    (ROOT / "_build/bend-cache").mkdir(parents=True, exist_ok=True)
    _, provenance = core.oracle(pins, env)
    expected = {}
    for i, (path, names) in enumerate(FIXTURES.items()):
        result = core.run(f"layout-oracle-{i}", [WORK / "oracle/oracle.exe", "--layouts", ROOT / path, *names], env)
        if result["stderr"] or not result["stdout"]:
            raise RuntimeError(f"malformed oracle result for {path}")
        expected[path] = result["stdout"]
    if any(line not in expected["test/family-layouts.kan"].splitlines() for line in GOLDENS):
        raise RuntimeError("pinned family layout output differs from independent goldens")
    binary, _, _ = build.compiler(pins)
    source = ROOT / "test/family-layout.bend"
    core.run("layout-check", [binary, source, "--check-only"], env)
    if set(hosts) & {"bun", "node-worker"}:
        core.run("layout-js", [binary, source, "-o", WORK / "layouts.js"], env)
    results = {}
    for host in hosts:
        if host == "native":
            core.run("layout-native-build", [binary, source, "-o", WORK / "layouts.exe"], env)
            argv = [WORK / "layouts.exe"]
        elif host == "bun":
            argv = [pins["tools"]["bun"]["path"], WORK / "layouts.js"]
        else:
            argv = [pins["tools"]["node"]["path"], "-e", build.node_worker_script(pins), WORK / "layouts.js"]
        observations = {}
        for i, (path, names) in enumerate(FIXTURES.items()):
            result = core.run(f"layout-{host}-{i}", [*argv, (ROOT / path).read_text(), *names], env)
            # IO.print contributes one transport newline after the exact layout text.
            if result["stderr"] or result["stdout"] != expected[path] + "\n":
                raise RuntimeError(f"{host} {path}: layout output differs from the pinned oracle: {result}")
            observations[path] = result
        results[host] = observations
    mutations = []
    if args.mutations:
        candidates = (
            ("ordinal", "erase/family.bend", "group_loop(xs, c, name, fam, F.Int63.zero)", "group_loop(xs, c, name, fam, F.Int63.one)"),
            ("ghost-field", "erase/family.bend", "field_repr(c, tyv, keep)", "field_repr(c, tyv, True{})"),
            ("nominal-name", "erase/type.bend", 'String.concat(["mu<", name, ">"])', 'String.concat(["mu<", "wrong", ">"])'),
            ("env-order", "erase/family.bend", "V.var(C.size(c)) <> env", "V.var(F.Int63.zero) <> env"),
        )
        for label, path, before, after in candidates:
            dest = WORK / "mutants" / label
            if dest.exists():
                shutil.rmtree(dest)
            for directory in ("lib", "surface", "erase", "test"):
                shutil.copytree(ROOT / directory, dest / directory, ignore=shutil.ignore_patterns("*.json", "*.log", "*.tsv", "kanon"))
            target = dest / path
            text = target.read_text()
            if text.count(before) != 1:
                raise RuntimeError(f"mutation {label}: source candidate drifted")
            target.write_text(text.replace(before, after))
            core.run(f"mutation-{label}-check", [binary, dest / "test/family-layout.bend", "--check-only"], env)
            core.run(f"mutation-{label}-build", [binary, dest / "test/family-layout.bend", "-o", dest / "layouts.js"], env)
            path = "test/family-layouts.kan"
            result = core.run(f"mutation-{label}-run", [pins["tools"]["bun"]["path"], dest / "layouts.js", (ROOT / path).read_text(), *FIXTURES[path]], env)
            if result["stderr"] or result["stdout"].startswith("FAIL") or result["stdout"] == expected[path] + "\n":
                raise RuntimeError(f"mutation {label}: was not rejected by a semantic layout comparison")
            mutations.append({"name": label, "reason": "layout output differs from pinned oracle", "observation": result})
    if snapshot() != hashes:
        raise RuntimeError("family layout sources changed during validation")
    record = {"schema": 1, "stage": "A.5b.3.2e.1 family layouts", "oracle": provenance,
              "sources": hashes,
              "families": sum(map(len, FIXTURES.values())), "fixtures": len(FIXTURES),
              "goldens": len(GOLDENS), "expected": expected, "hosts": results, "mutations": mutations}
    (WORK / "result.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"FAMILY LAYOUT PASS {record['fixtures']} fixtures, {record['families']} families, {len(GOLDENS)} goldens on {','.join(hosts)}; {len(mutations)} semantic mutations")


if __name__ == "__main__":
    main()
