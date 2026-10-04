#!/usr/bin/env python3
"""Scoped host-source policy for landed modules, not the full A.close HOUSE gate."""
from collections import Counter
import itertools
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
LIMITS = {"lib/foundation.bend": 800, "lib/kernel_budget.bend": 40,
          "lib/kernel_error.bend": 130, "lib/kernel_level.bend": 30,
          "lib/kernel_literal.bend": 20, "lib/kernel_term.bend": 180,
          "lib/kernel_value.bend": 220, "lib/kernel_shape.bend": 90,
          "lib/kernel_global.bend": 180, "lib/kernel_prim.bend": 200,
          "lib/kernel_quantity.bend": 260, "lib/kernel_positivity.bend": 180,
          "lib/kernel_rules.bend": 2100, "lib/kernel_eval.bend": 520,
          "lib/kernel_conv.bend": 600, "lib/kernel_check.bend": 800,
          "lib/kernel_pp.bend": 150, "lib/kernel_spec_count.bend": 90,
          "erase/eterm.bend": 170, "erase/runtime.bend": 300,
          "erase/type.bend": 230, "erase/core.bend": 730,
          "erase/sum.bend": 90, "erase/pair.bend": 90,
          "erase/family.bend": 100, "erase/mu.bend": 180,
          "erase/dependencies.bend": 45,
          "surface/token.bend": 160, "surface/lexer.bend": 200,
          "surface/syntax.bend": 260, "surface/parser.bend": 900,
          "surface/elab.bend": 640, "surface/elab_program.bend": 40,
          "surface/order.bend": 340, "surface/totality.bend": 50,
          "surface/program.bend": 140, "surface/source.bend": 50,
          "surface/check.bend": 40, "surface/record.bend": 150,
          "surface/family.bend": 130, "surface/constructor.bend": 90,
          "surface/constructor_infer.bend": 220,  # the full slot occurs check and the located refusal texts
          "bin/sole-comb.bend": 60}
HOST = ("bin", "lib", "surface", "erase", "wasm", "dev", "test/pinfront")
PRODUCTION = ("bin", "lib", "surface", "erase", "wasm")
PURE = {"lib", "erase", "wasm"}
IMPORT = re.compile(r"^[^\S\n]*import[^\S\n]+(\S+\.bend)(?:[^\S\n]+as[^\S\n]+\w+)?[^\S\n]*(?://[^\n]*)?$", re.M)


def within(path, directory):
    # samefile also matches spellings that resolve() keeps, such as case variants.
    return path.is_relative_to(directory) or directory.is_dir() and any(
        parent.exists() and parent.samefile(directory) for parent in (path, *path.parents))


def pinfront_boundary(root=ROOT):
    """Resolve the import closure of the production entry and every production
    module, including aliases, symlinks, and case variants."""
    root = root.resolve()
    pending = [root / "bin/sole-comb.bend"] + [path for tree in PRODUCTION for path in sorted((root / tree).rglob("*.bend"))]
    forbidden = root / "test/pinfront"
    seen, failures = set(), []
    while pending:
        path = pending.pop().resolve()
        if path in seen:
            continue
        seen.add(path)
        if within(path, forbidden):
            failures.append(f"production imports test/pinfront: {path.relative_to(root)}")
        elif not path.is_relative_to(root):
            failures.append(f"production import escapes repository: {path}")
        elif not path.is_file():
            failures.append(f"production import is missing: {path.relative_to(root)}")
        else:
            for name in IMPORT.findall(code_only(path.read_text())):
                if not re.match(r"^0x[0-9a-f]+/", name):
                    pending.append(path.parent / name)
    return failures


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
    observed = {"unsafe": set(), "catchalls": Counter()}
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
                observed["catchalls"][(path, function)] += 1
        if pending_unsafe:
            failures.append(f"{path}: unattached @unsafe")
        if Path(path).parts[0] in PURE or path.startswith("test/pinfront/"):
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
        failures.extend(f"{kind}: unregistered {path}:{name}" for path, name in sorted(set(sites) - registered))
        failures.extend(f"{kind}: stale {path}:{name}" for path, name in sorted(registered - set(sites)))
    # Each catchall entry records its exact row count, so a new row is a registry change.
    counts = observed["catchalls"]
    failures.extend(f"catchalls: {entry['path']}:{entry['function']} has {counts[site]} sites, registry has {entry.get('sites')}"
                    for entry in policy.get("catchalls", []) for site in [(entry["path"], entry["function"])]
                    if site in counts and entry.get("sites") != counts[site])
    return failures


def main():
    sources = {str(path.relative_to(ROOT)): path.read_text()
               for root in HOST for path in sorted((ROOT / root).rglob("*.bend"))}
    policy = json.loads((ROOT / "dev/bend-policy.json").read_text())
    failures = audit(sources, policy) + pinfront_boundary(ROOT)
    if failures:
        print("\n".join(f"HOUSE FAIL {failure}" for failure in failures))
        return 1
    print(f"HOUSE PASS scoped host policy: {len(sources)} files, {len(LIMITS)} line budgets, "
          f"{len(policy['unsafe'])} reviewed unsafe sites, {len(policy['catchalls'])} catchalls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
