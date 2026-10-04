#!/usr/bin/env python3
"""Compare the gate graph of the working Makefile with a baseline Makefile.

The record holds the default goal and the prerequisites of check, test and
gates for both Makefiles, their SHA-256 digests, and two verdicts: the default
goal is unchanged, and no baseline prerequisite of these targets is removed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
TARGETS = ("check", "gates", "test")
ASSIGNMENT = re.compile(r"[A-Za-z_][\w.]*\s*(?:\?|\+|!|:{1,3})?=")
RULE = re.compile(r"([^:#=\s][^:#=]*?)\s*:(?![:=])([^#;]*)")


def rules(text):
    """Yield (targets, prerequisites) for each rule line, after line continuations are joined."""
    lines = text.replace("\\\n", " ").splitlines()
    matches = (RULE.match(line) for line in lines if not line.startswith("\t") and not ASSIGNMENT.match(line))
    return [(found[1].split(), found[2].split()) for found in matches if found is not None]


def graph(text):
    """GNU make merges the prerequisites of repeated rules, so a target collects every rule line."""
    found = rules(text)
    default = next((name for names, _ in found for name in names if not name.startswith(".")), None)
    targets = {target: sorted({dep for names, deps in found if target in names for dep in deps}) for target in TARGETS}
    return {"default": default, "targets": targets}


def record(baseline, current):
    """Build the integration record from the bytes of the two Makefiles."""
    old, new = graph(baseline.decode()), graph(current.decode())
    return {
        "baseline": old,
        "baseline_makefile_sha256": hashlib.sha256(baseline).hexdigest(),
        "current": new,
        "current_makefile_sha256": hashlib.sha256(current).hexdigest(),
        "default_preserved": old["default"] == new["default"],
        "existing_gate_dependencies_preserved": all(
            set(old["targets"][target]) <= set(new["targets"][target]) for target in TARGETS),
        "schema": 1,
    }


def baseline_bytes(args):
    if args.base_file is not None:
        return args.base_file.read_bytes()
    return subprocess.run(["git", "-C", str(ROOT), "show", f"{args.base}:Makefile"],
                          check=True, capture_output=True).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--base", help="git revision that holds the baseline Makefile")
    source.add_argument("--base-file", type=Path, help="baseline Makefile path, for a tree without git")
    parser.add_argument("--current", type=Path, default=ROOT / "Makefile", help="working Makefile path")
    parser.add_argument("--output", type=Path, help="record path; the default is standard output")
    args = parser.parse_args()
    value = record(baseline_bytes(args), args.current.read_bytes())
    text = json.dumps(value, indent=2, sort_keys=True) + "\n"
    if args.output is None:
        sys.stdout.write(text)
    else:
        args.output.write_text(text)
    if not (value["default_preserved"] and value["existing_gate_dependencies_preserved"]):
        sys.exit("makefile graph: the default goal or a baseline gate prerequisite changed")


if __name__ == "__main__":
    main()
