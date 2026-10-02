#!/usr/bin/env python3
"""Compare pinfront behavior with immutable pinned Kanon fixtures."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/pinfront"
REFERENCE = ROOT / "dev/validation/stage-a-pinfront-reference.json"
EXPECTED_CORPUS = 146
EXPECTED_PROBES = 415
EXPECTED_FIXED = 68
DEADLINE = 600


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def run(name, argv, env, cwd=ROOT):
    result = subprocess.run(argv, cwd=cwd, env=env, capture_output=True, timeout=DEADLINE)
    (WORK / f"{name}.stdout.log").write_bytes(result.stdout)
    (WORK / f"{name}.stderr.log").write_bytes(result.stderr)
    if result.returncode:
        message = (result.stdout + result.stderr)[-4000:].decode(errors="replace")
        raise RuntimeError(f"{name} failed ({result.returncode}): {message}")
    return result.stdout


def git(upstream, *args):
    return subprocess.run(["git", "-C", str(upstream), *args], check=True, capture_output=True).stdout


def pin_state(pins):
    upstream = Path(pins["kanon"]["checkout"])
    revision = pins["kanon"]["revision"]
    if git(upstream, "rev-parse", "HEAD").decode().strip() != revision or git(upstream, "status", "--porcelain"):
        raise RuntimeError("Kanon must be clean at the configured pin")
    return upstream, revision


def corpus(pins, verify_upstream=True):
    directory = ROOT / "test/kanon"
    metadata = json.loads((directory / "SOURCE.json").read_text())
    if (metadata.get("schema"), metadata.get("revision"), metadata.get("files"), metadata.get("roots")) != (
            1, pins["kanon"]["revision"], EXPECTED_CORPUS, ["test", "examples"]):
        raise RuntimeError("corpus metadata differs from the required pin and universe")
    lines = (directory / "SURVIVORS.tsv").read_text().splitlines()
    if not lines or lines[0] != "source_path\tsha256\tstage_a\tr4_status":
        raise RuntimeError("invalid corpus manifest header")
    rows = []
    for line in lines[1:]:
        fields = line.split("\t")
        if len(fields) != 4:
            raise RuntimeError("invalid corpus manifest row")
        name, sha, stage, r4 = fields
        path = Path(name)
        if (path.is_absolute() or ".." in path.parts or path.as_posix() != name
                or not path.parts or path.parts[0] not in ("test", "examples")
                or path.suffix != ".kan" or not re.fullmatch("[0-9a-f]{64}", sha)
                or stage != "include" or r4 != "pending"):
            raise RuntimeError(f"invalid corpus member: {name}")
        target = directory / path
        if target.is_symlink() or not target.is_file() or digest(target) != sha:
            raise RuntimeError(f"corpus content differs: {name}")
        rows.append({"name": name, "source": target.read_bytes()})
    names = [row["name"] for row in rows]
    actual = sorted(p.relative_to(directory).as_posix() for p in directory.rglob("*.kan"))
    if len(names) != EXPECTED_CORPUS or len(set(names)) != len(names) or names != sorted(names) or names != actual:
        raise RuntimeError("corpus has missing, extra, duplicate, or reordered files")
    if verify_upstream:
        upstream, revision = pin_state(pins)
        pinned = [p for p in git(upstream, "ls-tree", "-r", "--name-only", revision, "test", "examples").decode().splitlines() if p.endswith(".kan")]
        if names != pinned:
            raise RuntimeError("corpus is not the complete pinned universe")
        for row in rows:
            if row["source"] != git(upstream, "show", f"{revision}:{row['name']}"):
                raise RuntimeError(f"corpus differs from the pinned blob: {row['name']}")
    return rows


def cases(pins, verify_upstream=True):
    probes = module("pinfront_cases", ROOT / "dev/pinfront-cases.py").cases()
    if len(probes) != EXPECTED_PROBES:
        raise RuntimeError("pinfront probe count changed")
    fixed = sum(any(k in r for k in ("printed", "error", "contains")) for r in probes)
    if fixed != EXPECTED_FIXED:
        raise RuntimeError("pinfront independent-expectation count changed")
    rows = corpus(pins, verify_upstream) + probes
    if len({row["name"] for row in rows}) != len(rows):
        raise RuntimeError("duplicate case names")
    return rows


def reference_inputs():
    paths = ["dev/test-pinfront.py",
             "dev/pinfront-cases.py",
             "test/kanon/SOURCE.json",
             "test/kanon/SURVIVORS.tsv",
             "dev/reference-fixtures.py",
             "dev/reference-fixtures/manifest.json",
             "dev/reference-fixtures/pinfront.json",
             "dev/validation/stage-a-pinfront-reference.json"]
    return {name: digest(ROOT / name) for name in paths}





def oracle(pins, checks, env):
    reference = module("sole_reference", ROOT / "dev/reference-fixtures.py")
    observations, provenance = reference.byte_observations(pins, "pinfront", checks)
    record = {"schema": 1, "scope": "pinned lexer, parser, syntax printer; no elaboration or kernel verdict", "revision": pins["kanon"]["revision"],
              "inputs": reference_inputs(), "fixture": provenance,
              "cases": [{"name": row["name"], "source_sha256": hashlib.sha256(row["source"]).hexdigest(),
                         "observation_hex": value.hex()} for row, value in zip(checks, observations)]}
    (WORK / "reference.json").write_text(json.dumps(record, indent=2) + "\n")
    compare_reference(record)
    return observations, record


def compare_reference(record, path=REFERENCE):
    # Input hashes change with the harness, so only the per-case observations must stay equal.
    # The observation part is identity-only: the snapshot is the reference itself.
    fields = ("name", "source_sha256", "observation_hex")
    try:
        saved = [[case[key] for key in fields] for case in json.loads(path.read_text())["cases"]]
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise RuntimeError(f"saved reference is missing or malformed: {path}") from error
    fresh = [[case[key] for key in fields] for case in record["cases"]]
    changed = [new[0] for new, old in zip(fresh, saved) if new != old]
    if changed or len(fresh) != len(saved):
        where = changed[0] if changed else "the case count"
        raise RuntimeError(f"reference cases differ from the saved reference at {where}; "
                           f"review {WORK / 'reference.json'} and copy it to {path}")


def replay(pins, checks, path=REFERENCE):
    record = json.loads(path.read_text())
    if record.get("schema") != 1 or record.get("revision") != pins["kanon"]["revision"] or record.get("inputs") != reference_inputs():
        raise RuntimeError("reference provenance or input hashes changed")
    saved = record.get("cases", [])
    if len(saved) != len(checks):
        raise RuntimeError("reference case count changed")
    observations = []
    for row, expected in zip(checks, saved):
        if expected.get("name") != row["name"] or expected.get("source_sha256") != hashlib.sha256(row["source"]).hexdigest():
            raise RuntimeError("reference case identity or source changed")
        value = expected.get("observation_hex", "")
        if not isinstance(value, str) or re.fullmatch("(?:[0-9a-f]{2})+", value) is None:
            raise RuntimeError("reference observation is missing or malformed")
        observations.append(bytes.fromhex(value))
    return observations, record


def independent(checks, observations):
    for row, observation in zip(checks, observations):
        text = observation.decode("latin1")
        if "printed" in row:
            parts = text.split("\nPRINT\n")
            if len(parts) != 2 or parts[1].split("\nROUNDTRIP\n")[0] != row["printed"]:
                raise RuntimeError(f"independent printed form failed: {row['name']}: {text!r}")
        if "error" in row and (not ("LEX-ERROR\n" in text or "\nPARSE-ERROR\n" in text) or row["error"] not in text):
            raise RuntimeError(f"independent refusal failed: {row['name']}: {text!r}")
        if "contains" in row and row["contains"] not in text:
            raise RuntimeError(f"independent observation failed: {row['name']}: {text!r}")


def parse_output(output, checks):
    try:
        text = json.loads(output)
    except (ValueError, UnicodeDecodeError) as error:
        raise RuntimeError("backend did not print a framed observation string") from error
    if not isinstance(text, str):
        raise RuntimeError("backend result is not a string")
    return parse_frames(text, checks)


def parse_native(output, checks):
    if not output.endswith(b"\n"):
        raise RuntimeError("native output lacks its print terminator")
    try:
        text = output[:-1].decode("utf8")
    except UnicodeDecodeError as error:
        raise RuntimeError("native output is not UTF-8") from error
    return parse_frames(text, checks)


def parse_frames(text, checks):
    offset, observed = 0, []
    for row in checks:
        prefix = row["name"] + "\t"
        if not text.startswith(prefix, offset):
            raise RuntimeError(f"missing, duplicate, or reordered backend row: {row['name']}")
        start = offset + len(prefix)
        colon = text.find(":", start)
        if colon < 0 or re.fullmatch("[0-9]+", text[start:colon]) is None:
            raise RuntimeError("backend row has an invalid length")
        size = int(text[start:colon])
        offset = colon + 1 + size
        if offset > len(text):
            raise RuntimeError("backend row was truncated")
        try:
            observed.append(text[colon + 1:offset].encode("latin1"))
        except UnicodeEncodeError as error:
            raise RuntimeError("backend observation is not a byte string") from error
    if offset != len(text):
        raise RuntimeError("backend has extra rows or trailing data")
    return observed


def mismatches(output, checks, observations):
    actual = parse_output(output, checks)
    return [row["name"] for row, expected, value in zip(checks, observations, actual) if expected != value]


def native_args(source):
    try:
        text = source.decode("utf8")
    except UnicodeDecodeError:
        text = None
    if text is not None and "\x00" not in text:
        return ["text", text]
    return ["hex", source.hex()]


def harness(checks, destination):
    definitions = ["import Base", "import ../../test/pinfront/observe.bend as O",
                   'def frame(name: String, +value: String) -> String:\n'
                   '  String.append(name, String.append("\\t", String.append(O.nat(String.length(value)), String.append(":", value))))']
    for i, row in enumerate(checks):
        source = row["source"]
        try:
            text = source.decode("utf8")
        except UnicodeDecodeError:
            text = None
        if text is not None and all(ord(c) >= 32 or c in "\n\r\t" for c in text):
            chunks = []
            for j, start in enumerate(range(0, len(text), 512)):
                name = f"input_{i}_{j}"
                definitions.append(f"def {name}() -> String:\n  {json.dumps(text[start:start + 512], ensure_ascii=False)}")
                chunks.append(name)
            while len(chunks) > 1:
                combined = []
                for j in range(0, len(chunks), 2):
                    if j + 1 == len(chunks):
                        combined.append(chunks[j])
                    else:
                        name = f"input_{i}_join_{len(definitions)}"
                        definitions.append(f"def {name}() -> String:\n  String.append({chunks[j]}, {chunks[j + 1]})")
                        combined.append(name)
                chunks = combined
            value = "O.observe(" + (chunks[0] if chunks else '""') + ")"
        else:
            value = "O.observe_bytes([" + ",".join(map(str, source)) + "])"
        definitions.append(f"def case_{i}() -> String:\n  frame({json.dumps(row['name'])}, {value})")
    groups = []
    for start in range(0, len(checks), 16):
        body = '""'
        for i in reversed(range(start, min(start + 16, len(checks)))):
            body = f"String.append(case_{i}, {body})"
        name = f"group_{start}"
        groups.append(name)
        definitions.append(f"def {name}() -> String:\n  {body}")
    body = '""'
    for group in reversed(groups):
        body = f"String.append({group}, {body})"
    definitions.append(f"def main() -> String:\n  {body}")
    destination.write_text("\n\n".join(definitions) + "\n")


def source_hashes():
    paths = [ROOT / "Makefile",
             ROOT / "dev/bend-policy.json",
             ROOT / "dev/house-bend.py",
             ROOT / "dev/test-house.py",
             ROOT / "dev/test-pinfront.py",
             ROOT / "dev/pinfront-cases.py",
             ROOT / "dev/toolchain.json",
             ROOT / "dev/build.py",
             ROOT / "test/pinfront-native.bend",
             ROOT / "dev/test-pinfront-harness.py",
             ROOT / "dev/test-pinfront-mutations.py",
             ROOT / "dev/reference-fixtures.py",
             ROOT / "dev/reference-fixtures/manifest.json",
             ROOT / "dev/reference-fixtures/pinfront.json",
             ROOT / "dev/validation/stage-a-pinfront-reference.json"]
    paths += sorted((ROOT / "lib").glob("*.bend")) + sorted((ROOT / "test/pinfront").glob("*.bend"))
    return {str(path.relative_to(ROOT)): digest(path) for path in paths}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--replay", action="store_true", help="compatibility flag; recorded pinned fixtures are always used")
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    build = module("sole_build", ROOT / "dev/build.py")
    binary, _, _ = build.compiler(pins)
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    checks = cases(pins, False)
    before = source_hashes()
    observations, reference = oracle(pins, checks, env)
    independent(checks, observations)
    print(f"PASS A.5a pinned parser fixtures: {len(checks)} cases, {EXPECTED_FIXED} independent expectations", flush=True)
    source = WORK / "checks.bend"
    harness(checks, source)
    run("check", [str(binary), str(source), "--check-only"], env)
    run("compile-js", [str(binary), str(source), "-o", str(WORK / "checks.js")], env)
    endpoints = {"bun": [pins["tools"]["bun"]["path"], str(WORK / "checks.js")],
                 "node-worker": [pins["tools"]["node"]["path"], "-e", build.node_worker_script(pins), str(WORK / "checks.js")]}
    for name, argv in endpoints.items():
        failures = mismatches(run(name, argv, env), checks, observations)
        if failures:
            raise RuntimeError(f"{name} parser differential failed in {failures[:10]}; see {WORK / (name + '.stdout.log')}")
        print(f"PASS A.5a {name}: {len(checks)} cases", flush=True)
    native_source = ROOT / "test/pinfront-native.bend"
    native_c = WORK / "checks-native.c"
    run("emit-native", [str(binary), str(native_source), "-o", str(native_c)], env)
    cc = shutil.which(os.environ.get("SOLE_COMB_CC", os.environ.get("CC", "clang")))
    if cc is None:
        raise RuntimeError("native parser checks require clang 14 or newer")
    cc_version = run("cc-version", [cc, "--version"], env).decode().strip()
    version = re.match(r"(?:Apple )?(?:\w+ )?clang version (\d+)", cc_version)
    if version is None or int(version[1]) < 14:
        raise RuntimeError("native parser checks require clang 14 or newer")
    # Match the pinned Bend driver's CPU flags and retain the emitted C.
    cc_flags = ["-std=c11", "-O3", str(native_c), "-lpthread", "-lm", "-o", str(WORK / "checks-native.exe")]
    run("compile-native", [cc, *cc_flags], env)
    (WORK / "native").mkdir(exist_ok=True)
    failures = []
    for i, (row, expected) in enumerate(zip(checks, observations)):
        output = run(f"native/{i:04d}", [str(WORK / "checks-native.exe"), "--", row["name"], *native_args(row["source"])], env)
        if parse_native(output, [row]) != [expected]:
            failures.append(row["name"])
    if failures:
        raise RuntimeError(f"native parser differential failed in {failures[:10]}; see {WORK / 'native'}")
    print(f"PASS A.5a native INFO: {len(checks)} cases", flush=True)
    if before != source_hashes():
        raise RuntimeError("sources changed during validation")
    record = {"schema": 1, "scope": "A.5a parsing only; full KANON-DIFF pending",
              "cases": len(checks), "corpus_files": EXPECTED_CORPUS, "probes": EXPECTED_PROBES,
              "independent": EXPECTED_FIXED, "sources": before, "corpus": reference["inputs"],
              "harness_sha256": digest(source), "observations_sha256": hashlib.sha256(b"".join(observations)).hexdigest(),
              "reference_sha256": digest(WORK / "reference.json"),
              "endpoints": {name: "PASS" for name in ("bun", "node-worker", "native")},
              "native": {"role": "behavioral INFO, not a performance qualification", "compiler": cc,
                         "version": cc_version, "flags": ["-std=c11", "-O3", "-lpthread", "-lm"],
                         "c_sha256": digest(native_c), "executable_sha256": digest(WORK / "checks-native.exe")},
              "deadline_s": DEADLINE, "replay": args.replay}
    (WORK / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
