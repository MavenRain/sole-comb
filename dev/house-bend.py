#!/usr/bin/env python3
"""Scoped host-source policy for landed modules, not the full A.close HOUSE gate."""
import itertools
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
LIMITS = {"lib/foundation.bend": 800, "lib/kernel_budget.bend": 40,
          "lib/kernel_error.bend": 130, "lib/kernel_level.bend": 30,
          "lib/kernel_literal.bend": 20}
HOST = ("bin", "lib", "surface", "erase", "wasm", "dev")
PURE = {"lib", "erase", "wasm"}


def code_only(source):
    # Retain line boundaries while masking strings and comments.
    return re.sub(r'"(?:\\.|[^"\\])*"|#[^\n]*',
                  lambda m: "".join("\n" if c == "\n" else " " for c in m[0]), source)


def case_slots(line):
    # Top-level patterns of one case row. Nested fields become '#'; a bare
    # name slot is a binder because Bend refuses an unbraced constructor.
    row = re.match(r"\s*case\s(.*)", line)
    if row is None:
        return []
    depths = itertools.accumulate((c in "{([") - (c in "})]") for c in row[1])
    top = "".join(c if depth == 0 and c not in "{}()[]" else "#" for c, depth in zip(row[1], depths))
    head = re.sub(r"\s*(<>|\+)\s*", r"\1", top.split(":", 1)[0])
    return re.split(r"[\s,]+", head.strip())


def audit(sources, policy, limits=LIMITS):
    failures = []
    observed = {"unsafe": set(), "catchalls": set()}
    for path, source in sources.items():
        if path in limits and len(source.splitlines()) > limits[path]:
            failures.append(f"{path}: exceeds {limits[path]} physical lines")
        code = code_only(source)
        function, pending_unsafe = None, False
        for line in code.splitlines():
            if line.strip() == "@unsafe":
                pending_unsafe = True
            definition = re.match(r"def ([A-Za-z_][\w.]*)\(", line)
            if definition:
                function = definition[1]
                if pending_unsafe:
                    observed["unsafe"].add((path, function))
                    pending_unsafe = False
            if any(re.fullmatch(r"[A-Za-z_][\w.]*", slot) for slot in case_slots(line)):
                observed["catchalls"].add((path, function))
        if pending_unsafe:
            failures.append(f"{path}: unattached @unsafe")
        if Path(path).parts[0] in PURE:
            banned = re.search(r"\b(?:IO|File|Ref|mutable|foreign|ffi|extern|raise|throw|panic|assert|try|catch|unwrap|for|while)\b", code)
            if banned:
                failures.append(f"{path}: forbidden pure-source token {banned[0]}")
            body = "\n".join(line for line in code.splitlines() if not line.startswith("import "))
            for operator in re.finditer(r"[/%%]", body):
                literal = re.match(r"\s*([0-9]+)[nu]?\b", body[operator.end():])
                if literal is None or int(literal[1]) == 0:
                    failures.append(f"{path}: divisor needs a nonzero literal; add a reviewed proof check before using computed divisors")
    for path in limits.keys() - sources.keys():
        failures.append(f"{path}: required milestone source missing")
    for kind, sites in observed.items():
        entries = policy.get(kind, [])
        registered = {(entry["path"], entry["function"]) for entry in entries}
        if len(registered) != len(entries) or any(not entry.get("reason", "").strip() for entry in entries):
            failures.append(f"{kind}: duplicate site or missing review reason")
        failures.extend(f"{kind}: unregistered {path}:{name}" for path, name in sorted(sites - registered))
        failures.extend(f"{kind}: stale {path}:{name}" for path, name in sorted(registered - sites))
    return failures


def main():
    sources = {str(path.relative_to(ROOT)): path.read_text()
               for root in HOST for path in sorted((ROOT / root).rglob("*.bend"))}
    policy = json.loads((ROOT / "dev/bend-policy.json").read_text())
    failures = audit(sources, policy)
    if failures:
        print("\n".join(f"HOUSE FAIL {failure}" for failure in failures))
        return 1
    print(f"HOUSE PASS scoped host policy: {len(sources)} files, {len(LIMITS)} line budgets, "
          f"{len(policy['unsafe'])} reviewed unsafe sites, {len(policy['catchalls'])} catchalls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
