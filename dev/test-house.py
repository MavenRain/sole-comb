#!/usr/bin/env python3
"""Failure-injection checks for the scoped host policy."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("house", Path(__file__).with_name("house-bend.py"))
house = importlib.util.module_from_spec(spec)
spec.loader.exec_module(house)


def case_folds():
    with tempfile.TemporaryDirectory() as directory:
        (Path(directory) / "probe").mkdir()
        return (Path(directory) / "PROBE").exists()


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

    def test_catchall_entry_counts_every_row(self):
        entry = {"path": "lib/example.bend", "function": "x", "reason": "fallback", "sites": 1}
        one = "def x(a: Bool) -> U32:\n  match a:\n    case True{}:\n      0\n    case _:\n      1\n"
        two = one + "    case other:\n      2\n"
        self.assertEqual([], self.audit(one, {"unsafe": [], "catchalls": [entry]}))
        self.assertTrue(self.audit(two, {"unsafe": [], "catchalls": [entry]}))
        self.assertEqual([], self.audit(two, {"unsafe": [], "catchalls": [dict(entry, sites=2)]}))
        self.assertTrue(self.audit(one, {"unsafe": [], "catchalls": [{k: v for k, v in entry.items() if k != "sites"}]}))

    def test_division_refusals(self):
        for divisor in ["0", "0n", "variable", "(variable)", "(1 - 1 : U32)"]:
            self.assertTrue(self.audit(f"def x() -> U32:\n  (8 / {divisor} : U32)\n"))
        self.assertEqual([], self.audit("def x() -> U32:\n  (8 / 2 : U32)\n"))

    def test_line_budget_and_missing_source(self):
        self.assertTrue(self.audit("a\nb\n", limits={"lib/example.bend": 1}))
        self.assertTrue(house.audit({}, {"unsafe": [], "catchalls": []}, {"lib/example.bend": 1}))

    def boundary(self, sources, alias=None):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, source in sources.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(source)
            if alias:
                link, target = alias
                (root / link).symlink_to(root / target)
            return house.pinfront_boundary(root)

    def test_pinfront_direct_and_transitive_imports_are_rejected(self):
        for source in ["import ../test/pinfront/parser.bend as P\n",
                       "  import ../test/pinfront/parser.bend # comment\n",
                       "import ../test/pinfront/parser.bend // comment\n"]:
            self.assertTrue(self.boundary({"bin/sole-comb.bend": source}))
        self.assertTrue(self.boundary({"bin/sole-comb.bend": "import ../lib/a.bend\n",
                                      "lib/a.bend": "import ./../test/pinfront/parser.bend\n"}))

    def test_pinfront_symlink_alias_is_rejected(self):
        self.assertTrue(self.boundary({"bin/sole-comb.bend": "import alias.bend\n",
                                      "test/pinfront/parser.bend": "import Base\n"},
                                     ("bin/alias.bend", "test/pinfront/parser.bend")))

    @unittest.skipUnless(case_folds(), "the temporary file system is case-sensitive")
    def test_pinfront_case_variant_is_rejected(self):
        self.assertTrue(self.boundary({"bin/sole-comb.bend": "import ../TEST/pinfront/parser.bend\n",
                                      "test/pinfront/parser.bend": "import Base\n"}))

    def test_pinfront_imports_from_every_production_module_are_rejected(self):
        for name in ["bin/tool.bend", "lib/unreachable.bend", "surface/a.bend", "erase/a.bend", "wasm/a.bend"]:
            self.assertTrue(self.boundary({"bin/sole-comb.bend": "import Base\n",
                                          name: "import ../test/pinfront/parser.bend\n"}), name)

    def test_pinfront_comments_and_non_production_imports_are_allowed(self):
        self.assertEqual([], self.boundary({"bin/sole-comb.bend": "# import ../test/pinfront/parser.bend\nimport Base\n",
                                           "test/other.bend": "import ./pinfront/parser.bend\n",
                                           "dev/tool.bend": "import ../test/pinfront/parser.bend\n",
                                           "test/pinfront/parser.bend": "import Base\n"}))

    def test_production_missing_import_and_cycles(self):
        self.assertTrue(self.boundary({"bin/sole-comb.bend": "import missing.bend\n"}))
        self.assertEqual([], self.boundary({"bin/sole-comb.bend": "import ../lib/a.bend\n",
                                           "lib/a.bend": "import ../bin/sole-comb.bend\n"}))

    def test_pinfront_sources_are_pure_and_policy_checked(self):
        for source in ["def x() -> U32: IO\n", "@unsafe\ndef x() -> U32: 0\n"]:
            self.assertTrue(house.audit({"test/pinfront/example.bend": source}, {"unsafe": [], "catchalls": []}, {}))


if __name__ == "__main__":
    unittest.main()
