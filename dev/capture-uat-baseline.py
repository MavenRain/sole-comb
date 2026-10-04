#!/usr/bin/env python3
"""Inspect a clean local UAT revision without fetching or changing its sources."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent
MODULES = ("UnifiedAggregation.Characterization", "UnifiedAggregation.FunctorExt",
           "UnifiedAggregation.Z2Group")
LEAN_VERSION = re.compile(r"^Lean \(version (\S+), \S+, commit ([0-9a-f]{40}), Release\)$")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run(argv, cwd):
    result = subprocess.run(argv, cwd=cwd, capture_output=True, timeout=900)
    if result.returncode or result.stderr:
        raise RuntimeError(f"{argv}: exit {result.returncode}\n"
                           + (result.stdout + result.stderr).decode("utf-8"))
    return result.stdout


def lean_version(text):
    """Keep the Lean release and commit; omit the platform triple."""
    match = LEAN_VERSION.match(text)
    if not match:
        raise RuntimeError(f"unrecognized Lean version: {text}")
    return f"Lean (version {match[1]}, commit {match[2]}, Release)"


def clean_sources(checkout, revision):
    if run(["git", "rev-parse", "HEAD"], checkout).decode().strip() != revision:
        raise RuntimeError(f"{checkout}: revision differs from the requested pin")
    if run(["git", "status", "--porcelain", "--untracked-files=no"], checkout):
        raise RuntimeError(f"{checkout}: tracked baseline sources are dirty")
    names = run(["git", "ls-files", "-z"], checkout).decode().split("\0")
    paths = [p for p in names if p and (p.endswith((".lean", ".toml", ".json"))
                                       or p == "lean-toolchain")]
    return {p: digest((checkout / p).read_bytes()) for p in sorted(paths)}


def snapshot(checkout, revision):
    manifest_bytes = (checkout / "lake-manifest.json").read_bytes()
    manifest = json.loads(manifest_bytes)
    dependencies = {}
    for package in manifest["packages"]:
        name = package["name"].strip("«»")
        package_root = checkout / manifest["packagesDir"] / name
        dependencies[name] = {
            "revision": package["rev"], "url": package["url"],
            "sources": clean_sources(package_root, package["rev"]),
        }
    return {
        "revision": revision,
        "toolchain": (checkout / "lean-toolchain").read_text().strip(),
        "manifest_sha256": digest(manifest_bytes),
        "sources": clean_sources(checkout, revision),
        "dependencies": dependencies,
    }


def parse_axioms(text, names):
    rows = {}
    for name, listed in re.findall(r"'([^']+)' depends on axioms: \[([^\]]*)\]", text):
        if name in rows:
            raise RuntimeError(f"repeated axiom observation: {name}")
        rows[name] = sorted(x.strip() for x in listed.split(",") if x.strip())
    for name in re.findall(r"'([^']+)' does not depend on any axioms", text):
        if name in rows:
            raise RuntimeError(f"repeated axiom observation: {name}")
        rows[name] = []
    if set(rows) != set(names) or len(names) != len(set(names)):
        raise RuntimeError("the Lean axiom census differs from the inspection script")
    if any("sorryAx" in axioms for axioms in rows.values()):
        raise RuntimeError("the selected baseline depends on sorryAx")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkout", type=Path, required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    checkout = args.checkout.resolve()
    script = ROOT / "test/uat-baseline.lean"
    script_bytes = script.read_bytes()
    names = re.findall(r"^#print axioms (\S+)$", script_bytes.decode(), re.M)
    checks = re.findall(r"^#check @(\S+)$", script_bytes.decode(), re.M)
    if not names or names != checks:
        raise RuntimeError("each selected declaration needs its type and axiom census")
    before = snapshot(checkout, args.revision)
    build = run(["leancho", *MODULES], checkout)
    version = lean_version(run(["lake", "env", "lean", "--version"], checkout).decode().strip())
    output = run(["lake", "env", "lean", str(script)], checkout)
    axioms = parse_axioms(output.decode(), names)
    if snapshot(checkout, args.revision) != before or script.read_bytes() != script_bytes:
        raise RuntimeError("baseline inputs changed during the inspection")
    target = args.output.resolve()
    target.mkdir(parents=True, exist_ok=True)
    (target / "uat-baseline.stdout").write_bytes(output)
    (target / "uat-baseline-build.stdout").write_bytes(build)
    record = {"schema": 1, "stage": "UAT.0", "baseline": before,
              "lean_version": version, "modules": list(MODULES),
              "script_sha256": digest(script_bytes), "stdout_sha256": digest(output),
              "build_stdout_sha256": digest(build), "declarations": names, "axioms": axioms}
    (target / "uat-baseline.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"UAT BASELINE PASS: {len(names)} declarations, "
          f"{len(before['dependencies'])} locked dependencies; {target}")


if __name__ == "__main__":
    main()
