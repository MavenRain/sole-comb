#!/usr/bin/env python3
"""Reject malformed observations and weakened program reference manifests."""
import copy
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("program_harness", ROOT / "dev/test-program.py")
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)


class HarnessTests(unittest.TestCase):
    def test_unicode_channel_boundary(self):
        self.assertEqual(h.decode("0:2:λ\n".encode()), h.triple(0, "λ\n".encode(), b""))

    def test_framing_and_channels_refuse(self):
        for raw in (b"", b"0", b"0:01:x", b"0:-1:x", b"0:3:x", b"2:0:", b"1:1:x", b"0:0:unexpected", b"0:0:\xff"):
            with self.subTest(raw=raw), self.assertRaises((RuntimeError, UnicodeError)):
                h.decode(raw)

    def test_mode_pair_requires_both_complete_frames(self):
        self.assertEqual(h.decode_both(b"4:0:0:0:0:"), {m: h.triple(0, b"", b"") for m in h.MODES})
        for raw in (b"", b"4:0:0:", b"5:0:0:0:0:", b"04:0:0:0:0:"):
            with self.subTest(raw=raw), self.assertRaises(RuntimeError):
                h.decode_both(raw)

    def record(self):
        checks = [{"name": "one", "source": b""}]
        pins = {"kanon": {"revision": "pin"}}
        record = dict(schema=1, scope=h.SCOPE, revision="pin", modes=list(h.MODES),
                      cases=[dict(h.identity(checks)[0], observations={m: h.triple(0, b"", b"") for m in h.MODES})])
        return pins, checks, record

    def test_reference_requires_cases_modes_and_bytes(self):
        pins, checks, base = self.record()
        changes = [lambda r: r.update(cases=[]), lambda r: r.update(modes=["check"]),
                   lambda r: r["cases"][0].update(name="other"),
                   lambda r: r["cases"][0]["observations"].pop("print"),
                   lambda r: r["cases"][0]["observations"]["print"].update(stdout="GG"),
                   lambda r: r["cases"][0]["observations"]["check"].update(exit=True),
                   lambda r: r["cases"][0]["observations"]["check"].update(stderr="78")]
        h.validate_record(base, pins, checks)
        for change in changes:
            record = copy.deepcopy(base)
            change(record)
            with self.assertRaises(RuntimeError):
                h.validate_record(record, pins, checks)

    def test_replay_binds_inputs(self):
        pins, checks, record = self.record()
        record["inputs"] = {}
        with self.assertRaisesRegex(RuntimeError, "input hashes"):
            h.validate_record(record, pins, checks, replay=True)

    def test_independent_verdict_survives_reference_changes(self):
        _pins, checks, record = self.record()
        checks[0]["code"] = 1
        with self.assertRaisesRegex(RuntimeError, "independent"):
            h.independent(checks, record)


if __name__ == "__main__":
    unittest.main()
