#!/usr/bin/env python3
"""Exercise the real A.1 library on both JS hosts and native (informational)."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import random
import subprocess

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/foundation"


def quoted(text):
    return json.dumps(text, ensure_ascii=False)


def number(n):
    value = f"T.number({quoted(str(abs(n)))})"
    return f"F.Bignum.negate({value})" if n < 0 else value


def cases():
    result = []

    def case(name, expression, expected):
        result.append((name, expression, str(expected)))

    for n in [0, 1, 32767, 32768, 1073741823, 4294967295]:
        case(f"u32-{n}", f"F.Bignum.to_string(F.Bignum.from_u32({n}))", n)
    for n in [0, 1, 32768, 2**48 - 1]:
        host = f"{n}n" if n < 2**32 else f"({n // 65536}n * 65536n + {n % 65536}n : Nat)"
        case(f"nat-{n}", f"F.Bignum.to_string(F.Bignum.from_nat({host}))", n)
    for i, text in enumerate(["0", "0000", "000123", "9" * 250, "32768"]):
        case(f"parse-{i}", f"T.decimal({quoted(text)})", int(text))
    for i, text in enumerate(["", "+1", "-1", " 1", "1 ", "1_2", "１２", "1\n2", "0x10"]):
        case(f"reject-{i}", f"T.decimal({quoted(text)})", "none")
    rng = random.Random(69)
    pairs = [(0, 0), (0, -1), (-1, 0), (-1, 1), (-7, -3), (32767, 32768),
             (2**62 - 1, 1), (10**120 - 1, 10**90 + 1)]
    pairs += [(rng.randrange(-2**bits, 2**bits), rng.randrange(-2**bits, 2**bits))
              for bits in (15, 31, 48, 63, 100, 255) for _ in range(3)]
    for i, (a, b) in enumerate(pairs):
        for op, expected in [("add", a + b), ("sub", max(0, a - b)), ("mul", a * b)]:
            case(f"{op}-{i}", f"F.Bignum.to_string(F.Bignum.{op}({number(a)}, {number(b)}))", expected)
        case(f"compare-{i}", f"T.order(F.Bignum.compare({number(a)}, {number(b)}))", (a > b) - (a < b))
    for n in [-1, 0, 32767, 32768, 2**100 + 32769]:
        limbs, remaining = [], n
        while remaining > 0:
            limbs.append(remaining % 32768)
            remaining //= 32768
        case(f"limbs-{n}", f"T.limbs(F.Bignum.limbs15({number(n)}))",
             "none" if n < 0 else "".join(f"{v}," for v in limbs))
    for n in [-1, 0, 2**30 - 1, 2**30, 2**48 - 1, 2**48]:
        case(f"to-nat-{n}", f"T.nat(F.Bignum.to_nat({number(n)}))", n if 0 <= n < 2**48 else "none")
        case(f"to-i31-{n}", f"T.nat(F.Bignum.to_i31({number(n)}))", n if 0 <= n < 2**30 else "none")
    for n in [-2**62 - 1, -2**62, -1, 0, 2**62 - 1, 2**62]:
        case(f"to-int-{n}", f"T.int63(F.Bignum.to_int({number(n)}))", n if -2**62 <= n < 2**62 else "none")
    for n in [-1, 0, 1, 2**62 - 1]:
        case(f"level-of-int-{n}", f"T.level_of_number({number(n)})", n if n >= 0 else "none")
    case("level-succ", "L.to_string(L.succ(L.one))", 2)
    case("level-succ-int63-wrap", f"L.to_string(L.succ({number(2**62 - 1)}))", -2**62)
    case("level-succ-after-wrap", f"L.to_string(L.succ(L.succ({number(2**62 - 1)})))", -2**62 + 1)
    case("level-max-left", "L.to_string(L.max(L.of_nat(99n), L.one))", 99)
    case("level-max-right", "L.to_string(L.max(L.one, L.of_nat(99n)))", 99)
    case("level-max-tie", "L.to_string(L.max(L.one, L.one))", 1)
    case("level-le", "T.boolean(L.le(L.zero, L.one))", "true")
    case("level-not-le", "T.boolean(L.le(L.one, L.zero))", "false")
    case("level-equal", "T.boolean(L.equal(L.one, L.succ(L.zero)))", "true")
    case("level-not-equal", "T.boolean(L.equal(L.one, L.zero))", "false")
    case("budget-zero", 'T.budget(B.spend("eval", B.create(0n)))', "budget: eval")
    case("budget-one", 'T.budget(B.spend("eval", B.create(1n)))', 0)
    case("budget-two", 'T.spend_twice(B.spend("first", B.create(2n)))', 0)
    case("budget-exhausted", 'T.spend_twice(B.spend("first", B.create(1n)))', "budget: second")
    case("budget-empty", "T.boolean(B.exhausted(B.create(0n)))", "true")
    case("budget-live", "T.boolean(B.exhausted(B.create(1n)))", "false")
    prefixes = {"Not_yet": "not yet", "Carry": "carry", "Unbound": "unbound",
                "Mismatch": "mismatch", "Universe": "universe", "Quantity": "quantity",
                "Wrong_leg": "wrong leg", "Missing_branch": "missing branch", "Overflow": "overflow",
                "Cannot_infer": "cannot infer", "Budget_exhausted": "budget",
                "Index_not_zero": "index not zero", "Index_above_universe": "index above universe"}
    for constructor, prefix in prefixes.items():
        case(f"error-{constructor}", f'E.to_string(E.{constructor}{{"payload"}})', f"{prefix}: payload")
        case(f"message-{constructor}", f'E.message(E.{constructor}{{"payload"}})', "payload")
    case("error-parse", 'E.to_string(E.Parse{"bad token", 12n, 34n})', "line 12, column 34: bad token")
    case("message-parse", 'E.message(E.Parse{"bad token", 12n, 34n})', "bad token")
    termination = "recursive definition loop failed the structural termination guard"
    case("error-termination", 'E.to_string(E.Termination{"loop"})', "termination: " + termination)
    case("message-termination", 'E.message(E.Termination{"loop"})', termination)
    for i, (a, b, expected) in enumerate([
        ('Lit.LString{"x"}', 'Lit.LString{"x"}', "true"),
        ('Lit.LString{"x"}', 'Lit.LString{"y"}', "false"),
        (f"Lit.LInt{{{number(-7)}}}", f"Lit.LInt{{{number(-7)}}}", "true"),
        (f"Lit.LInt{{{number(7)}}}", f"Lit.LInt{{{number(8)}}}", "false"),
        ('Lit.LString{"7"}', f"Lit.LInt{{{number(7)}}}", "false"),
        (f"Lit.LInt{{{number(7)}}}", 'Lit.LString{"7"}', "false")]):
        case(f"literal-{i}", f"T.boolean(Lit.equal({a}, {b}))", expected)
    map_expr = "F.Map.empty(U32)"
    for key, value in [("z", 9), ("a", 1), ("m", 5), ("a", 2), ("", 0), ("aa", 3)]:
        map_expr = f"F.Map.insert(U32, {quoted(key)}, {value}, {map_expr})"
    case("map-order-replace", f"T.bindings(F.Map.bindings(U32, {map_expr}))", "=0;a=2;aa=3;m=5;z=9;")
    case("map-size", f"F.Bignum.to_string(F.Bignum.from_nat(F.Map.size(U32, {map_expr})))", 5)
    for key, expected in [("a", 2), ("m", 5), ("z", 9), ("", 0), ("b", "none"), ("zz", "none")]:
        case(f"map-get-{key}", f"T.map_value(F.Map.get(U32, {quoted(key)}, {map_expr}))", expected)
    for key, expected in [("a", "=0;aa=3;m=5;z=9;"), ("", "a=2;aa=3;m=5;z=9;"),
                          ("z", "=0;a=2;aa=3;m=5;"), ("b", "=0;a=2;aa=3;m=5;z=9;")]:
        case(f"map-remove-{key}", f"T.bindings(F.Map.bindings(U32, F.Map.remove(U32, {quoted(key)}, {map_expr})))", expected)
    return result


def run(name, argv, env, timeout=120):
    completed = subprocess.run(argv, cwd=ROOT, env=env, capture_output=True, text=True, timeout=timeout)
    (WORK / f"{name}.log").write_text(completed.stdout + completed.stderr)
    if completed.returncode:
        raise RuntimeError(f"{name} failed ({completed.returncode}):\n{(completed.stdout + completed.stderr)[-4000:]}")
    return completed.stdout


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    spec = importlib.util.spec_from_file_location("sole_build", ROOT / "dev/build.py")
    build = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(build)
    binary, _, _ = build.compiler(pins)
    checks = cases()
    imports = ["import Base", "import ../../test/foundation.bend as T"]
    for filename, alias in [("foundation", "F"), ("kernel_budget", "B"), ("kernel_error", "E"),
                            ("kernel_level", "L"), ("kernel_literal", "Lit")]:
        imports.append(f"import ../../lib/{filename}.bend as {alias}")
    # Each case returns its name on failure. Expected values come from Python,
    # not from another invocation of the Bend implementation.
    definitions = [f"def case_{i}() -> String:\n  F.choose(String, String.eq({expr}, {quoted(expected)}), \"\", {quoted(name + chr(10))})"
                   for i, (name, expr, expected) in enumerate(checks)]
    body = '"PASS"'
    for i in reversed(range(len(checks))):
        body = f"String.append(case_{i}, {body})"
    source = WORK / "checks.bend"
    source.write_text("\n\n".join(imports + definitions + [f"def main() -> String:\n  {body}\n"]))
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    (ROOT / "_build/bend-cache").mkdir(parents=True, exist_ok=True)
    run("check", [str(binary), str(source), "--check-only"], env)
    run("compile-js", [str(binary), str(source), "-o", str(WORK / "checks.js")], env)
    endpoints = {"bun": [pins["tools"]["bun"]["path"], str(WORK / "checks.js")],
                 "node-worker": [pins["tools"]["node"]["path"], "-e", build.node_worker_script(pins), str(WORK / "checks.js")]}
    for endpoint, argv in endpoints.items():
        output = run(endpoint, argv, env, timeout=60).strip()
        if output not in ('"PASS"', "PASS"):
            raise RuntimeError(f"{endpoint} failed foundation cases:\n{output}")
        print(f"PASS A.1 {endpoint}: {len(checks)} cases", flush=True)
    run("compile-native", [str(binary), str(source), "-o", str(WORK / "checks.exe")], env)
    output = run("native-info", [str(WORK / "checks.exe")], env, timeout=60).strip()
    if output not in ('"PASS"', "PASS"):
        raise RuntimeError(f"native INFO failed foundation cases:\n{output}")
    print(f"PASS A.1 native INFO: {len(checks)} cases", flush=True)
    inputs = sorted((ROOT / "lib").glob("*.bend")) + [
        ROOT / name for name in ("test/foundation.bend", "dev/test-foundation.py",
                                 "dev/build.py", "dev/pin-check.py", "dev/toolchain.json",
                                 "dev/house-bend.py", "dev/test-house.py", "dev/bend-policy.json",
                                 "Makefile")]
    sources = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    report = {"milestone": "A.1", "cases_per_endpoint": len(checks), "endpoints": list(endpoints),
              "informational_endpoints": ["native"],
              "sources": sources, "harness_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
              "r2_qualification": "pending", "stage_a_oracle": "pending"}
    (WORK / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
