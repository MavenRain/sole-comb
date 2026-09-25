#!/usr/bin/env python3
"""Check the pins of dev/toolchain.json against this machine.

Usage: python3 -P dev/pin-check.py [--toolchain PATH] [--only GROUP]... [--verbose]
Groups: bend, kanon, tools, limits. The default is all groups.
The toolchain path defaults to $SOLE_COMB_TOOLCHAIN, then dev/toolchain.json.

The last line is PINCHECK PASS ... (exit 0) or PINCHECK FAIL ... (exit 1).
A mismatch on a tool marked informational prints WARN and does not fail.

The check never runs bend: the Bend version is pinned by the checkout
revision and the binary sha256. It never builds, updates or writes in the
kanon, attest or Bend trees.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
GROUPS = ("bend", "kanon", "tools", "limits")
HEX = frozenset("0123456789abcdef")


def sha256_file(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(argv):
    return subprocess.run(argv, capture_output=True, text=True, timeout=120)


def git_head(checkout):
    result = run(["git", "-C", str(checkout), "rev-parse", "HEAD"])
    return result.stdout.strip() if result.returncode == 0 else f"git exit {result.returncode}"


def git_clean(checkout):
    return all(run(["git", "-C", str(checkout), *argv]).returncode == 0
               for argv in (["diff", "--quiet"], ["diff", "--cached", "--quiet"]))


def is_sha256(value):
    return isinstance(value, str) and len(value) == 64 and set(value) <= HEX


def check(name, thunk, severity="FAIL"):
    """Run one check. thunk returns (ok, detail). An exception is a failed check."""
    try:
        ok, detail = thunk()
    except (OSError, KeyError, TypeError, ValueError, subprocess.SubprocessError) as error:
        ok, detail = False, f"{type(error).__name__}: {error}"
    return (name, "ok" if ok else severity, detail)


def equal(expected, observed):
    return (expected == observed, f"expected {expected} observed {observed}")


def bend_checks(pins):
    bend = pins["bend"]
    checkout = Path(bend["checkout"])
    binary = Path(bend["binary"])
    return [
        check("bend.revision.format", lambda: (isinstance(bend["revision"], str) and len(bend["revision"]) == 40
                                               and set(bend["revision"]) <= HEX, bend["revision"])),
        check("bend.checkout.head", lambda: equal(bend["revision"], git_head(checkout))),
        check("bend.checkout.clean", lambda: (git_clean(checkout), "tracked sources must be unmodified")),
        check("bend.binary.in_checkout", lambda: equal(checkout.resolve(), binary.resolve().parent.parent)),
        check("bend.binary.sha256", lambda: equal(bend["binary_sha256"], sha256_file(binary))),
        check("bend.env.BEND_NO_TELEMETRY", lambda: equal("1", bend["env"]["BEND_NO_TELEMETRY"])),
    ]


def kanon_checks(pins):
    kanon = pins["kanon"]
    oracle = kanon["oracle"]
    return [
        check("kanon.checkout.head", lambda: equal(kanon["revision"], git_head(kanon["checkout"]))),
        check("kanon.head_observed", lambda: equal(kanon["revision"], kanon["head_observed"])),
        check("kanon.oracle.sha256", lambda: equal(oracle["sha256"], sha256_file(oracle["path"]))),
    ]


def first_line(argv):
    result = run(argv)
    return (result.stdout or result.stderr).strip().splitlines()[0] if result.returncode == 0 else f"exit {result.returncode}"


def tool_checks(name, tool):
    severity = "WARN" if tool.get("informational") else "FAIL"
    return [
        check(f"tools.{name}.realpath", lambda: equal(tool["realpath"], os.path.realpath(tool["path"])), severity),
        check(f"tools.{name}.sha256", lambda: equal(tool["sha256"], sha256_file(tool["path"])), severity),
        check(f"tools.{name}.version", lambda: equal(tool["version"], first_line([tool["path"], "--version"])), severity),
    ]


def node_major(pins):
    version = first_line([pins["tools"]["node"]["path"], "--version"])
    major = int(version.removeprefix("v").split(".")[0])
    return (major >= pins["node_minimum_major"], f"node {version}, minimum major {pins['node_minimum_major']}")


def tools_checks(pins):
    return [row for name, tool in sorted(pins["tools"].items()) for row in tool_checks(name, tool)] + [
        check("tools.node.minimum_major", lambda: node_major(pins))]


def positive_int(pins, key):
    value = pins[key]
    return (isinstance(value, int) and not isinstance(value, bool) and value > 0, f"{key}={value}")


def limits_checks(pins):
    ints = ("node_minimum_major", "stack_kib", "load_wait_max_s",
            "build_deadline_s", "leg_deadline_s", "mutant_deadline_s")
    return [check(f"limits.{key}", lambda key=key: positive_int(pins, key)) for key in ints] + [
        check("limits.load_ceiling", lambda: (isinstance(pins["load_ceiling"], (int, float))
                                              and not isinstance(pins["load_ceiling"], bool)
                                              and pins["load_ceiling"] > 0, f"load_ceiling={pins['load_ceiling']}")),
        check("limits.endpoint", lambda: (pins["endpoint"] is None or pins["endpoint"] in pins["endpoint_candidates"],
                                          f"endpoint={pins['endpoint']}")),
        check("limits.sha256.format", lambda: (all(map(is_sha256, [
            pins["bend"]["binary_sha256"], pins["kanon"]["oracle"]["sha256"],
            *(tool["sha256"] for tool in pins["tools"].values())])), "every sha256 is 64 lower-case hex digits")),
    ]


CHECKS = {"bend": bend_checks, "kanon": kanon_checks, "tools": tools_checks, "limits": limits_checks}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--toolchain", default=os.environ.get("SOLE_COMB_TOOLCHAIN", str(ROOT / "dev/toolchain.json")))
    parser.add_argument("--only", action="append", choices=GROUPS)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    groups = tuple(args.only or GROUPS)
    try:
        pins = json.loads(Path(args.toolchain).read_text())
        rows = [row for group in groups for row in CHECKS[group](pins)]
    except (OSError, KeyError, TypeError, ValueError) as error:
        rows = [("toolchain.read", "FAIL", f"{type(error).__name__}: {error}")]
    shown = rows if args.verbose else [row for row in rows if row[1] != "ok"]
    print("\n".join(f"{status} {name}: {detail}" for name, status, detail in shown)) if shown else None
    failed = sum(1 for row in rows if row[1] == "FAIL")
    warned = sum(1 for row in rows if row[1] == "WARN")
    verdict = "FAIL" if failed else "PASS"
    print(f"PINCHECK {verdict} groups={','.join(groups)} checks={len(rows)} failed={failed} warned={warned} "
          f"toolchain={args.toolchain}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
