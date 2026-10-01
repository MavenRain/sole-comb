#!/usr/bin/env python3
"""Exercise checked-in native examples through the actual public command."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def inputs():
    paths = [ROOT / "sole-comb", ROOT / "dev/cli.py", ROOT / "dev/build.py",
             ROOT / "examples/EXPECTATIONS.json", ROOT / "dev/test-cli.py",
             ROOT / "dev/toolchain.json", ROOT / "dev/bend-policy.json",
             ROOT / "examples/records.kernel"]
    for directory, pattern in (("bin", "*.bend"), ("lib", "*.bend"), ("surface", "*.bend"), ("erase", "*.bend"),
                               ("examples", "*.sole-comb"), ("corpus/refuse", "*.sole-comb")):
        paths.extend((ROOT / directory).rglob(pattern))
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hosts", default="bun,node-worker,native")
    args = parser.parse_args()
    hosts = args.hosts.split(",")
    if not hosts or len(set(hosts)) != len(hosts) or any(h not in ("bun", "node-worker", "native") for h in hosts):
        parser.error("hosts must be a unique list of bun,node-worker,native")
    manifest = json.loads((ROOT / "examples/EXPECTATIONS.json").read_text())
    selected = set(manifest["positive"]) | set(manifest["negative"])
    found = {str(p.relative_to(ROOT)) for directory in ("examples", "corpus/refuse")
             for p in (ROOT / directory).rglob("*.sole-comb")}
    if manifest["schema"] != 1 or selected != found or len(selected) != 55 or not all(manifest["negative"].values()):
        raise RuntimeError("native example census changed; update and review the independent expectations")
    equivalents = manifest["equivalent"]
    if equivalents != {"examples/default-arms.sole-comb": "examples/default-arms-explicit.sole-comb",
                       "examples/record-expansion.sole-comb": "examples/record-expansion-explicit.sole-comb"}:
        raise RuntimeError("native sugar equivalences changed; review the independent explicit expansions")
    if not all(a in manifest["positive"] and b in manifest["positive"] for a, b in equivalents.items()):
        raise RuntimeError("native sugar equivalences must refer to checked positive examples")
    before = inputs()
    observations = []
    printed = {}

    def run(argv, expected, host, cwd=ROOT, marker=""):
        result = subprocess.run([str(ROOT / "sole-comb"), *argv], cwd=cwd,
                                capture_output=True, text=True, timeout=900)
        if result.returncode != expected:
            raise RuntimeError(f"{host} {argv}: expected exit {expected}, got {result.returncode}\n{result.stdout}{result.stderr}")
        if marker and marker not in result.stderr:
            raise RuntimeError(f"{host} {argv}: missing diagnostic {marker!r}: {result.stderr}")
        observations.append({"host": host, "args": list(map(str, argv)), "exit": result.returncode,
                             "stdout": result.stdout, "stderr": result.stderr})
        return result

    for host in hosts:
        for path, count in manifest["positive"].items():
            result = run(["check", "--host", host, "--print", path], 0, host)
            if result.stderr or not result.stdout.startswith(f"CHECK {path} defs={count} ok\n"):
                raise RuntimeError(f"{host} {path}: malformed success or incorrect declaration count")
            if printed.setdefault(path, result.stdout) != result.stdout:
                raise RuntimeError(f"{host} {path}: printed kernel definitions differ from host {hosts[0]}")
            print(f"PASS public {host}: {path}", flush=True)
        for sugared, explicit in equivalents.items():
            expanded = printed[sugared].partition("\n")[2]
            if sugared == "examples/record-expansion.sole-comb":
                expanded = expanded.replace("#record", "recordArg")
            if expanded != printed[explicit].partition("\n")[2]:
                raise RuntimeError(f"{host}: {sugared} differs from its independent explicit kernel expansion")
            print(f"PASS public {host}: {sugared} equals explicit kernel expansion", flush=True)
        if printed["examples/records.sole-comb"].partition("\n")[2] != (ROOT / "examples/records.kernel").read_text():
            raise RuntimeError(f"{host}: records differ from the reviewed product and projection expectations")
        print(f"PASS public {host}: record field types, indices, namespaces and binder hygiene", flush=True)
        for path, marker in manifest["negative"].items():
            result = run(["check", "--host", host, path], 1, host, marker=marker)
            if result.stdout or not result.stderr.startswith(f"CHECK {path} FAIL "):
                raise RuntimeError(f"{host} {path}: missing source refusal")
        with tempfile.TemporaryDirectory(prefix="sole source ") as folder:
            directory = Path(folder)
            path = directory / "with spaces.sole-comb"
            path.write_text("-- case match rec mu record else { ; }\naxiom Nat : Type 0\ndef with : Nat := 1\ndef absurd : Nat := 2\ndef end : Nat := 3\n")
            result = run(["check", "--host", host, path], 0, host, cwd=directory)
            if "defs=4 ok" not in result.stdout:
                raise RuntimeError("reserved words in comments or permitted identifiers were rejected")
            path.write_text('axiom Nat : Type 0\ndef invalid : Nat := b"case match rec else"\n')
            result = run(["check", "--host", host, path], 1, host)
            if "E-R4-MATCH" in result.stderr:
                raise RuntimeError("reserved words inside a byte literal were treated as syntax")
        print(f"PASS public {host}: 55 native files and 2 lexical boundary cases", flush=True)

    with tempfile.TemporaryDirectory(prefix="sole cli ") as folder:
        directory = Path(folder)
        run(["--help"], 0, "cli", cwd=directory)
        run(["check", directory / "missing.sole-comb"], 2, "cli", marker="E-IO")
        run(["check", directory / "wrong.kan"], 64, "cli", marker="E-CLI")
        run(["unknown"], 64, "cli")
        run(["build"], 5, "cli", marker="E-SOURCE-NOT-YET")
        run(["run"], 5, "cli", marker="E-SOURCE-NOT-YET")
        oversized = directory / "large.sole-comb"
        oversized.write_bytes(b" " * 32769)
        run(["check", oversized], 5, "cli", marker="E-SOURCE-SIZE")
    if inputs() != before:
        raise RuntimeError("public compiler sources changed during validation")
    work = ROOT / "_build/public"
    work.mkdir(parents=True, exist_ok=True)
    report = {"schema": 1, "scope": "public checking with native function arms, default arms, closed records and ordinary and product erasure examples", "hosts": hosts,
              "files": 55, "equivalences": equivalents, "observations": len(observations), "sources": before, "results": observations}
    (work / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"PASS public compiler: {len(observations)} command observations; source hashes stable", flush=True)


if __name__ == "__main__":
    main()
