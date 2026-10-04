#!/usr/bin/env python3
"""Reject baseline tampering, incomplete inspections, host failures and inexact refusals."""
import json
from pathlib import Path
import runpy
import shutil
import tempfile
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parent.parent
API = runpy.run_path(str(ROOT / "dev/test-uat-readiness.py"))
CAPTURE = runpy.run_path(str(ROOT / "dev/capture-uat-baseline.py"))


class UATHarness(unittest.TestCase):
    def test_frozen_baseline(self):
        record = API["load_baseline"]()
        self.assertEqual(len(record["declarations"]), 21)
        self.assertEqual(len(record["baseline"]["dependencies"]), 10)

    def test_changed_baseline_bytes_are_refused(self):
        for name in ("dev/uat-baseline/uat-baseline.json", "dev/uat-baseline/uat-baseline.stdout",
                     "dev/uat-baseline/uat-baseline-build.stdout", "test/uat-baseline.lean"):
            with self.subTest(path=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                shutil.copytree(ROOT / "dev/uat-baseline", root / "dev/uat-baseline")
                (root / "test").mkdir()
                shutil.copyfile(ROOT / "test/uat-baseline.lean", root / "test/uat-baseline.lean")
                path = root / name
                path.write_bytes(path.read_bytes() + b"\n")
                with self.assertRaises(RuntimeError):
                    API["load_baseline"](root)

    def test_incomplete_and_duplicate_axiom_census_are_refused(self):
        for output in ("", "'T' does not depend on any axioms\n" * 2):
            with self.subTest(output=output), self.assertRaises(RuntimeError):
                CAPTURE["parse_axioms"](output, ["T"])

    def test_sorry_dependency_is_refused(self):
        with self.assertRaisesRegex(RuntimeError, "sorryAx"):
            CAPTURE["parse_axioms"]("'T' depends on axioms: [sorryAx]\n", ["T"])

    def test_host_failure_is_not_a_source_refusal(self):
        case = next(c for c in API["load_cases"]() if c["id"] == "equality-nat-prop")
        for code in (0, 2, 5):
            result = SimpleNamespace(returncode=code, stdout="",
                                     stderr=f"CHECK {case['path']} FAIL {case['diagnostic']}\n")
            with self.subTest(exit=code), self.assertRaises(RuntimeError):
                API["check_result"](case, result)

    def test_unrelated_refusal_is_rejected(self):
        case = next(c for c in API["load_cases"]() if c["id"] == "structural-hypothesis")
        result = SimpleNamespace(returncode=1, stdout="",
                                 stderr=f"CHECK {case['path']} FAIL unbound: accidental\n")
        with self.assertRaises(RuntimeError):
            API["check_result"](case, result)

    def test_refusal_needs_the_complete_diagnostic(self):
        case = next(c for c in API["load_cases"]() if c["id"] == "parameterized-record")
        line = f"CHECK {case['path']} FAIL {case['diagnostic']}\n"
        self.assertEqual(API["check_result"](case, SimpleNamespace(returncode=1, stdout="", stderr=line)), line)
        fragment = f"CHECK {case['path']} FAIL line 1, column 8: expected 'name : Type' after 'record'"
        for stderr in (line[:-1] + " and more\n", line + "extra\n", line[:-1],
                       line.replace("line 1, column 8", "line 2, column 8"),
                       line.replace("line 1, column 8", "line 1, column 9"),
                       fragment + ", found end of input\n"):
            with self.subTest(stderr=stderr), self.assertRaises(RuntimeError):
                API["check_result"](case, SimpleNamespace(returncode=1, stdout="", stderr=stderr))

    def test_refusal_with_stdout_is_rejected(self):
        case = next(c for c in API["load_cases"]() if c["id"] == "constructor-unsolved")
        result = SimpleNamespace(returncode=1, stdout="def ambiguous : Nat\n",
                                 stderr=f"CHECK {case['path']} FAIL {case['diagnostic']}\n")
        with self.assertRaises(RuntimeError):
            API["check_result"](case, result)

    def test_landed_case_failures_are_rejected(self):
        case = next(c for c in API["load_cases"]() if c["status"] == "landed")
        header = f"CHECK {case['path']} defs={case['definitions']} ok\n"
        other = f"CHECK {case['path']} defs={case['definitions'] + 1} ok\n"
        good = SimpleNamespace(returncode=0, stdout=header + "def flip\n", stderr="")
        self.assertEqual(API["check_result"](case, good), "def flip\n")
        for result in (SimpleNamespace(returncode=1, stdout=header, stderr=""),
                       SimpleNamespace(returncode=2, stdout=header, stderr=""),
                       SimpleNamespace(returncode=0, stdout=header, stderr="warning\n"),
                       SimpleNamespace(returncode=0, stdout=other, stderr=""),
                       SimpleNamespace(returncode=0, stdout="", stderr="")):
            with self.subTest(result=result), self.assertRaises(RuntimeError):
                API["check_result"](case, result)

    def test_host_disagreement_is_rejected(self):
        first = {}
        API["compare_hosts"](first, ("foundation", "print"), "def flip\n")
        API["compare_hosts"](first, ("foundation", "print"), "def flip\n")
        API["compare_hosts"](first, ("foundation", "erased"), "rec flip\n")
        with self.assertRaisesRegex(RuntimeError, "differs across hosts"):
            API["compare_hosts"](first, ("foundation", "print"), "def flop\n")

    def test_changed_inputs_are_rejected(self):
        cases = API["load_cases"]()
        for name in ("test/uat/def-rec-excluded.sole-comb", "dev/uat-readiness.json", "bin/extra.bend"):
            with self.subTest(path=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                for source in API["sources"](cases):
                    (root / source).parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(ROOT / source, root / source)
                before = API["sources"](cases, root)
                self.assertEqual(before, API["sources"](cases))
                API["assert_inputs_unchanged"](cases, before, root)
                with (root / name).open("ab") as handle:
                    handle.write(b"\n")
                with self.assertRaisesRegex(RuntimeError, "inputs changed"):
                    API["assert_inputs_unchanged"](cases, before, root)

    def test_lean_version_omits_the_platform(self):
        raw = ("Lean (version 4.31.0, arm64-apple-darwin24.6.0, "
               "commit 68218e876d2a38b1985b8590fff244a83c321783, Release)")
        frozen = API["load_baseline"]()["lean_version"]
        self.assertEqual(CAPTURE["lean_version"](raw), frozen)
        self.assertEqual(CAPTURE["lean_version"](raw.replace("arm64-apple-darwin24.6.0", "x86_64-unknown-linux-gnu")),
                         frozen)
        for text in ("", frozen, raw.replace("Release", "Debug"), raw.replace("68218e87", "68218e8")):
            with self.subTest(text=text), self.assertRaises(RuntimeError):
                CAPTURE["lean_version"](text)

    def test_case_and_source_census_are_checked(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "dev").mkdir()
            shutil.copyfile(ROOT / "dev/uat-readiness.json", root / "dev/uat-readiness.json")
            shutil.copytree(ROOT / "test/uat", root / "test/uat")
            (root / "examples").mkdir()
            shutil.copyfile(ROOT / "examples/uat-foundation.sole-comb", root / "examples/uat-foundation.sole-comb")
            self.assertEqual(len(API["load_cases"](root)), 12)
            (root / "test/uat/forgotten.sole-comb").write_text("def forgotten : Nat := 0\n")
            with self.assertRaisesRegex(RuntimeError, "source census"):
                API["load_cases"](root)
            (root / "test/uat/forgotten.sole-comb").unlink()
            manifest = json.loads((root / "dev/uat-readiness.json").read_text())
            next(c for c in manifest["cases"] if c["status"] == "required-refusal")["diagnostic"] = ""
            (root / "dev/uat-readiness.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(RuntimeError, "complete diagnostic"):
                API["load_cases"](root)
            manifest["cases"] = []
            (root / "dev/uat-readiness.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(RuntimeError, "case census"):
                API["load_cases"](root)


if __name__ == "__main__":
    unittest.main()
