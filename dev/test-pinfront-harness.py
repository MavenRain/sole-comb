#!/usr/bin/env python3
"""Failure injection for corpus identity, recorded replay, and the saved reference."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location("pinfront", Path(__file__).with_name("test-pinfront.py"))
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)


class HarnessTests(unittest.TestCase):
    def setUp(self):
        self.checks = [{"name": "a", "source": b"a"}, {"name": "b", "source": b"b"}]

    def test_backend_frames_require_identity_length_and_complete_output(self):
        good = json.dumps("a\t3:x\n\x00b\t1:\xff").encode()
        self.assertEqual([b"x\n\x00", b"\xff"], h.parse_output(good, self.checks))
        self.assertEqual([], h.mismatches(good, self.checks, [b"x\n\x00", b"\xff"]))
        self.assertEqual(["b"], h.mismatches(good, self.checks, [b"x\n\x00", b"y"]))
        malformed = ["", "a\t1:x", "a\t1:xa\t1:y", "b\t1:ya\t1:x",
                     "a\t:x", "a\t-1:x", "a\t2:x", "a\t1:xb\t1:yextra",
                     "a\t1:xb\t1:\u0100", "a\t1:xb\t2:y"]
        for value in malformed:
            with self.subTest(value=value), self.assertRaises(RuntimeError):
                h.parse_output(json.dumps(value).encode(), self.checks)
        for value in [b"", b"null", b"{}", b"[]", b"0", good + b" extra", b"\xff"]:
            with self.subTest(value=value), self.assertRaises(RuntimeError):
                h.parse_output(value, self.checks)

    def test_native_transport_preserves_bytes_and_fails_closed(self):
        row = self.checks[:1]
        self.assertEqual([b"x\n\x00\xff"], h.parse_native("a\t4:x\n\x00\xff\n".encode(), row))
        for bad in [b"", b"a\t1:x", b"a\t1:x\n\n", b"a\t1:\xff\n", b"a\t2:x\n"]:
            with self.subTest(bad=bad), self.assertRaises(RuntimeError):
                h.parse_native(bad, row)
        for source in [b"", b"a\n", "\u00e9".encode(), b"\x00", b"\xff"]:
            mode, value = h.native_args(source)
            actual = value.encode() if mode == "text" else bytes.fromhex(value)
            self.assertEqual(source, actual)

    def test_zero_probes_cannot_pass(self):
        with mock.patch.object(h, "module", return_value=SimpleNamespace(cases=lambda: [])):
            with self.assertRaisesRegex(RuntimeError, "probe count"):
                h.cases({})

    def test_fixed_expectations_detect_wrong_verdict_and_print(self):
        with self.assertRaises(RuntimeError):
            h.independent([{"name": "a", "error": "refused"}], [b"\nPRINT\nrefused\n"])
        with self.assertRaises(RuntimeError):
            h.independent([{"name": "a", "printed": "expected"}], [b"\nPRINT\nactual\nROUNDTRIP\ntrue"])
        with self.assertRaises(RuntimeError):
            h.independent([{"name": "a", "contains": "line 2"}], [b"line 1"])
        h.independent([{"name": "a", "error": "refused"}], [b"LEX-ERROR\nrefused"])

    def test_replay_rejects_stale_and_reordered_evidence(self):
        pins = {"kanon": {"revision": "pin"}}
        record = {"schema": 1, "revision": "pin", "inputs": {"source": "hash"},
                  "cases": [{"name": r["name"], "source_sha256": hashlib.sha256(r["source"]).hexdigest(),
                             "observation_hex": "78"} for r in self.checks]}
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(h, "reference_inputs", return_value={"source": "hash"}):
            path = Path(directory) / "reference.json"
            path.write_text(json.dumps(record))
            self.assertEqual([b"x", b"x"], h.replay(pins, self.checks, path)[0])
            bad_records = []
            for key, value in [("revision", "other"), ("schema", 2), ("inputs", {}), ("cases", [])]:
                altered = copy.deepcopy(record)
                altered[key] = value
                bad_records.append(altered)
            for key, value in [("name", "b"), ("source_sha256", "wrong"), ("observation_hex", "0"), ("observation_hex", "")]:
                altered = copy.deepcopy(record)
                altered["cases"][0][key] = value
                bad_records.append(altered)
            for altered in bad_records:
                path.write_text(json.dumps(altered))
                with self.assertRaises(RuntimeError):
                    h.replay(pins, self.checks, path)

    def test_oracle_cases_must_match_the_saved_reference(self):
        record = {"cases": [{"name": r["name"], "source_sha256": hashlib.sha256(r["source"]).hexdigest(),
                             "observation_hex": "78"} for r in self.checks]}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "reference.json"
            path.write_text(json.dumps(dict(record, inputs={"harness": "old"})))
            h.compare_reference(record, path)
            for key, value in [("observation_hex", "79"), ("source_sha256", "wrong"), ("name", "c")]:
                altered = copy.deepcopy(record)
                altered["cases"][1][key] = value
                with self.subTest(key=key), self.assertRaisesRegex(RuntimeError, "saved reference at [bc];"):
                    h.compare_reference(altered, path)
            for cases in [record["cases"][:1], record["cases"] + record["cases"][:1]]:
                with self.subTest(count=len(cases)), self.assertRaisesRegex(RuntimeError, "case count"):
                    h.compare_reference({"cases": cases}, path)
            for text in ["", "{}", '{"cases": [1]}']:
                path.write_text(text)
                with self.subTest(text=text), self.assertRaisesRegex(RuntimeError, "missing or malformed"):
                    h.compare_reference(record, path)
            path.unlink()
            with self.assertRaisesRegex(RuntimeError, "missing or malformed"):
                h.compare_reference(record, path)

    def test_corpus_refuses_missing_extra_and_modified_files(self):
        pins = json.loads((h.ROOT / "dev/toolchain.json").read_text())
        original = h.ROOT / "test/kanon"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dest = root / "test/kanon"
            shutil.copytree(original, dest)
            with mock.patch.object(h, "ROOT", root):
                self.assertEqual(146, len(h.corpus(pins, False)))
                fixture = sorted(dest.rglob("*.kan"))[0]
                before = fixture.read_bytes()
                fixture.write_bytes(before + b"\n")
                with self.assertRaisesRegex(RuntimeError, "content differs"):
                    h.corpus(pins, False)
                fixture.write_bytes(before)
                extra = dest / "test/extra.kan"
                extra.write_text("")
                with self.assertRaisesRegex(RuntimeError, "missing, extra"):
                    h.corpus(pins, False)
                extra.unlink()
                fixture.unlink()
                with self.assertRaisesRegex(RuntimeError, "content differs"):
                    h.corpus(pins, False)

    def test_corpus_refuses_manifest_truncation_duplicates_and_escape(self):
        pins = json.loads((h.ROOT / "dev/toolchain.json").read_text())
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dest = root / "test/kanon"
            shutil.copytree(h.ROOT / "test/kanon", dest)
            manifest = dest / "SURVIVORS.tsv"
            lines = manifest.read_text().splitlines()
            variants = [lines[:-1], lines[:1] + [lines[1]] * 146,
                        lines[:1] + ["../escape.kan\t" + lines[1].split("\t", 1)[1]] + lines[2:]]
            with mock.patch.object(h, "ROOT", root):
                for variant in variants:
                    manifest.write_text("\n".join(variant) + "\n")
                    with self.assertRaises(RuntimeError):
                        h.corpus(pins, False)


if __name__ == "__main__":
    unittest.main()
