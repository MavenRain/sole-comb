"""Read immutable Kanon observations without compiling or generating OCaml."""
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
DIRECTORY = ROOT / "dev/reference-fixtures"
SNAPSHOTS = {scope: f"dev/validation/stage-a-{scope}-reference.json"
             for scope in ("pinfront", "elaboration", "program")}


def signature(inputs):
    return hashlib.sha256(json.dumps(inputs, ensure_ascii=True, separators=(",", ":")).encode()).hexdigest()


def load(pins, scope, inputs):
    """Reject stale inputs, changed pins, corrupt records, and wrong scopes."""
    try:
        manifest = json.loads((DIRECTORY / "manifest.json").read_text())
        entry = manifest["fixtures"][scope]
        if entry["path"] != f"{scope}.json":
            raise ValueError("unexpected fixture path")
        path = DIRECTORY / entry["path"]
        content = path.read_bytes()
        if hashlib.sha256(content).hexdigest() != entry["sha256"]:
            raise ValueError("fixture checksum changed")
        record = json.loads(content)
        revision = pins["kanon"]["revision"]
        if manifest["schema"] != 1 or manifest["revision"] != revision:
            raise ValueError("fixture manifest revision or schema changed")
        if (record["schema"], record["scope"], record["revision"]) != (1, scope, revision):
            raise ValueError("fixture metadata changed")
        if record["inputs_sha256"] != signature(inputs):
            raise ValueError("reference case inputs changed")
        if "reference" in record:
            reference = record["reference"]
            if reference["path"] != SNAPSHOTS.get(scope):
                raise ValueError("unexpected reference snapshot path")
            snapshot = (ROOT / reference["path"]).read_bytes()
            if hashlib.sha256(snapshot).hexdigest() != reference["sha256"]:
                raise ValueError("reference snapshot checksum changed")
            saved = json.loads(snapshot)
            if saved["schema"] != 1 or saved["revision"] != revision:
                raise ValueError("reference snapshot metadata changed")
            if [[row["name"], row["source_sha256"]] for row in saved["cases"]] != inputs:
                raise ValueError("reference snapshot cases differ from inputs")
            key = {"pinfront": "observation_hex", "elaboration": "observation", "program": "observations"}[scope]
            expected = [row[key] for row in saved["cases"]]
            if scope == "program" and not all(isinstance(value, dict) for value in expected):
                raise ValueError("program reference observations are malformed")
        else:
            expected = record["expected"]
        origin = record["origin"]
        if not isinstance(origin, dict):
            raise ValueError("reference origin is malformed")
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise RuntimeError(f"invalid {scope} reference fixture: {error}") from error
    provenance = {"revision": revision, "fixture": str(path.relative_to(ROOT)),
                  "fixture_sha256": entry["sha256"], "inputs_sha256": record["inputs_sha256"],
                  "build": "recorded Kanon observations; no fresh oracle compilation",
                  "origin": origin}
    return expected, provenance


def expressions(pins, scope, checks):
    inputs = [[name, expr.bend, fixed] for name, expr, fixed in checks]
    expected, provenance = load(pins, scope, inputs)
    if not isinstance(expected, list) or len(expected) != len(checks):
        raise RuntimeError(f"{scope} reference case count changed")
    for (name, _, fixed), value in zip(checks, expected):
        if not isinstance(value, str) or (fixed is not None and fixed != value):
            raise RuntimeError(f"{scope} independent expectation changed: {name}")
    return expected, provenance


def observations(pins, scope, checks):
    inputs = [[row["name"], hashlib.sha256(row["source"]).hexdigest()] for row in checks]
    expected, provenance = load(pins, scope, inputs)
    if not isinstance(expected, list) or len(expected) != len(checks):
        raise RuntimeError(f"{scope} reference case count changed")
    return expected, provenance


def byte_observations(pins, scope, checks):
    expected, provenance = observations(pins, scope, checks)
    if any(not isinstance(value, str) or re.fullmatch(r"(?:[0-9a-f]{2})*", value) is None for value in expected):
        raise RuntimeError(f"{scope} malformed reference bytes")
    return [bytes.fromhex(value) for value in expected], provenance


def source_files(scope):
    return ("dev/reference-fixtures.py", "dev/reference-fixtures/manifest.json",
            f"dev/reference-fixtures/{scope}.json", *([SNAPSHOTS[scope]] if scope in SNAPSHOTS else []))
