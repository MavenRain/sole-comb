#!/usr/bin/env python3
"""Fixture integrity, stale-input refusals, and the OCaml source exclusion."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location("reference_fixture_tests", ROOT / "dev/reference-fixtures.py")
H = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(H)


class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root, self.directory = H.ROOT, H.DIRECTORY
        H.ROOT = Path(self.temp.name)
        H.DIRECTORY = H.ROOT / "dev/reference-fixtures"
        H.DIRECTORY.mkdir(parents=True)
        self.pins = {"kanon": {"revision": "test-revision"}}
        self.inputs = [["case", "1n", "1"]]
        self.record = {"schema": 1, "scope": "evaluation", "revision": "test-revision",
                       "inputs_sha256": H.signature(self.inputs), "expected": ["1"], "origin": {"capture": "test"}}
        self.digest = hashlib.sha256(b"x").hexdigest()
        self.rows = [["case", self.digest]]
        self.write()

    def tearDown(self):
        H.ROOT, H.DIRECTORY = self.root, self.directory
        self.temp.cleanup()

    def manifest(self, scope, path, **fields):
        manifest = {"schema": 1, "revision": "test-revision", **fields, "fixtures": {
            scope: {"path": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}}}
        (H.DIRECTORY / "manifest.json").write_text(json.dumps(manifest))

    def write(self, **fields):
        path = H.DIRECTORY / "evaluation.json"
        path.write_text(json.dumps(self.record))
        self.manifest("evaluation", path, **fields)

    def snapshot(self, cases, scope="pinfront", path=None, **meta):
        relative = H.SNAPSHOTS[scope]
        target = H.ROOT / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps({"schema": 1, "revision": "test-revision", "cases": cases, **meta}))
        reference = {"path": path or relative, "sha256": hashlib.sha256(target.read_bytes()).hexdigest()}
        fixture = H.DIRECTORY / f"{scope}.json"
        fixture.write_text(json.dumps(dict(self.record, scope=scope, inputs_sha256=H.signature(self.rows),
                                           reference=reference)))
        self.manifest(scope, fixture)
        return target

    def test_valid_reference(self):
        self.assertEqual(H.load(self.pins, "evaluation", self.inputs)[0], ["1"])

    def test_stale_pin_and_inputs(self):
        for pins, inputs in (({"kanon": {"revision": "other"}}, self.inputs), (self.pins, [["case", "2n", "1"]])):
            with self.subTest(pins=pins, inputs=inputs), self.assertRaises(RuntimeError):
                H.load(pins, "evaluation", inputs)

    def test_corrupt_or_missing_fixture(self):
        path = H.DIRECTORY / "evaluation.json"
        path.write_text("{}")
        with self.assertRaisesRegex(RuntimeError, "checksum"):
            H.load(self.pins, "evaluation", self.inputs)
        path.unlink()
        with self.assertRaises(RuntimeError):
            H.load(self.pins, "evaluation", self.inputs)

    def test_metadata_and_origin(self):
        for field, value in (("scope", "other"), ("schema", 2), ("origin", None), ("origin", [])):
            with self.subTest(field=field, value=value):
                original = self.record[field]
                self.record[field] = value
                self.write()
                with self.assertRaises(RuntimeError):
                    H.load(self.pins, "evaluation", self.inputs)
                self.record[field] = original

    def test_invalid_count_and_expectation(self):
        checks = [("case", SimpleNamespace(bend="1n"), "1")]
        for value in ([], ["2"], [None]):
            with self.subTest(value=value):
                self.record["expected"] = value
                self.write()
                with self.assertRaises(RuntimeError):
                    H.expressions(self.pins, "evaluation", checks)

    def test_snapshot_descriptor_integrity(self):
        path = self.snapshot([{"name": "case", "source_sha256": self.digest, "observation_hex": "78"}])
        self.assertEqual(H.load(self.pins, "pinfront", self.rows)[0], ["78"])
        path.write_text("{}")
        with self.assertRaisesRegex(RuntimeError, "snapshot checksum"):
            H.load(self.pins, "pinfront", self.rows)

    def test_manifest_refusals(self):
        for field, value, message in (("schema", 2, "manifest revision or schema"),
                                      ("revision", "other", "manifest revision or schema")):
            with self.subTest(field=field), self.assertRaisesRegex(RuntimeError, message):
                self.write(**{field: value})
                H.load(self.pins, "evaluation", self.inputs)
        self.write()
        manifest = json.loads((H.DIRECTORY / "manifest.json").read_text())
        manifest["fixtures"]["evaluation"]["path"] = "other.json"
        (H.DIRECTORY / "manifest.json").write_text(json.dumps(manifest))
        with self.assertRaisesRegex(RuntimeError, "unexpected fixture path"):
            H.load(self.pins, "evaluation", self.inputs)

    def test_snapshot_refusals(self):
        case = {"name": "case", "source_sha256": self.digest, "observation_hex": "78"}
        for label, cases, overrides, message in (
                ("path", [case], {"path": "dev/validation/other.json"}, "unexpected reference snapshot path"),
                ("schema", [case], {"schema": 2}, "snapshot metadata"),
                ("revision", [case], {"revision": "other"}, "snapshot metadata"),
                ("name", [dict(case, name="other")], {}, "snapshot cases differ from inputs"),
                ("source", [dict(case, source_sha256="0" * 64)], {}, "snapshot cases differ from inputs"),
                ("count", [case, case], {}, "snapshot cases differ from inputs")):
            with self.subTest(label=label), self.assertRaisesRegex(RuntimeError, message):
                self.snapshot(cases, **overrides)
                H.load(self.pins, "pinfront", self.rows)

    def test_program_observation_must_be_object(self):
        case = {"name": "case", "source_sha256": self.digest, "observations": ["check", "print"]}
        with self.assertRaisesRegex(RuntimeError, "program reference observations are malformed"):
            self.snapshot([case], scope="program")
            H.load(self.pins, "program", self.rows)

    def test_observation_count_and_bytes(self):
        checks = [{"name": "case", "source": b"x"}]
        self.record["inputs_sha256"] = H.signature(self.rows)
        for value, message in ((["78", "79"], "case count changed"), (["7"], "malformed reference bytes"),
                               (["zz"], "malformed reference bytes"), ([None], "malformed reference bytes")):
            with self.subTest(value=value), self.assertRaisesRegex(RuntimeError, message):
                self.record["expected"] = value
                self.write()
                H.byte_observations(self.pins, "evaluation", checks)
        self.record["expected"] = ["78"]
        self.write()
        self.assertEqual(H.byte_observations(self.pins, "evaluation", checks)[0], [b"x"])


class SourceTests(unittest.TestCase):
    def test_no_ocaml_source(self):
        extensions = {".ml", ".mli", ".mll", ".mly"}
        skipped = lambda path: any(part in {"_build", ".git"} or part.startswith(".gatework") for part in path.parts)
        found = [str(path.relative_to(ROOT)) for path in ROOT.rglob("*")
                 if path.suffix in extensions and not skipped(path.relative_to(ROOT))]
        self.assertEqual(found, [])


if __name__ == "__main__":
    unittest.main()
