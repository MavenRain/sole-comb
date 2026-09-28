#!/usr/bin/env python3
"""Failure-injection checks for the scoped host policy."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("house", Path(__file__).with_name("house-bend.py"))
house = importlib.util.module_from_spec(spec)
spec.loader.exec_module(house)


class HouseTests(unittest.TestCase):
    def audit(self, source, policy=None, limits=None):
        return house.audit({"lib/example.bend": source}, policy or {"unsafe": [], "catchalls": []},
                           {} if limits is None else limits)

    def test_strings_and_comments_are_not_effects(self):
        self.assertEqual([], self.audit('# throw IO\nimport Base\ndef x() -> String:\n  "panic File"\n'))

    def test_effect_and_loop_are_rejected(self):
        for token in ["IO", "File", "Ref", "raise", "while", "foreign", "unwrap"]:
            self.assertTrue(self.audit(f"def x() -> U32:\n  {token}\n"), token)

    def test_unsafe_needs_a_live_reviewed_site(self):
        entry = {"path": "lib/example.bend", "function": "x", "reason": "decreases"}
        source = "@unsafe\ndef x() -> U32:\n  0\n"
        self.assertTrue(self.audit(source))
        self.assertEqual([], self.audit(source, {"unsafe": [entry], "catchalls": []}))
        self.assertTrue(self.audit(source.replace("@unsafe\n", ""), {"unsafe": [entry], "catchalls": []}))
        self.assertTrue(self.audit(source, {"unsafe": [dict(entry, reason="")], "catchalls": []}))

    def test_catchall_is_not_an_exhaustive_constructor(self):
        self.assertTrue(self.audit("def x(a: Bool) -> U32:\n  match a:\n    case _:\n      0\n"))
        self.assertEqual([], self.audit("def x(a: Bool) -> U32:\n  match a:\n    case True{}:\n      0\n    case False{}:\n      1\n"))

    def test_catchall_slot_in_a_multi_scrutinee_row(self):
        for row in ["case _, Some{y}:", "case Some{y} rest:", "case None{}, Other:", "case Some{y}, rest: 0"]:
            self.assertTrue(self.audit(f"def x(a: U32, b: U32) -> U32:\n  match a, b:\n    {row}\n      0\n"), row)
        self.assertEqual([], self.audit("def x(a: Bool, b: U32) -> U32:\n  match a, b:\n    case True{}, h <> t:\n"
                                        "      0\n    case False{}, F.Pair2{k, v} <> rest:\n      1\n    case False{}, 1n+p:\n      2\n"))

    def test_division_refusals(self):
        for divisor in ["0", "0n", "variable", "(variable)", "(1 - 1 : U32)"]:
            self.assertTrue(self.audit(f"def x() -> U32:\n  (8 / {divisor} : U32)\n"))
        self.assertEqual([], self.audit("def x() -> U32:\n  (8 / 2 : U32)\n"))

    def test_line_budget_and_missing_source(self):
        self.assertTrue(self.audit("a\nb\n", limits={"lib/example.bend": 1}))
        self.assertTrue(house.audit({}, {"unsafe": [], "catchalls": []}, {"lib/example.bend": 1}))


if __name__ == "__main__":
    unittest.main()
