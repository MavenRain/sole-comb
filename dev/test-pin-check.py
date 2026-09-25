#!/usr/bin/env python3
"""Negative controls for the pure pin-check limits group."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


PIN_CHECK = Path(__file__).with_name("pin-check.py")
PINS = json.loads(Path(__file__).with_name("toolchain.json").read_text())


def limits(**overrides):
    with tempfile.TemporaryDirectory() as folder:
        toolchain = Path(folder) / "toolchain.json"
        toolchain.write_text(json.dumps({**PINS, **overrides}))
        return subprocess.run([sys.executable, "-P", str(PIN_CHECK), "--toolchain", str(toolchain),
                               "--only", "limits"], capture_output=True, text=True, timeout=60)


class EndpointTests(unittest.TestCase):
    def test_candidate_or_null_endpoint_passes(self):
        for endpoint in (None, "bun", "node-worker"):
            with self.subTest(endpoint=endpoint):
                result = limits(endpoint=endpoint)
                self.assertEqual(result.returncode, 0, result.stdout)

    def test_info_only_native_endpoint_fails(self):
        result = limits(endpoint="native")
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("FAIL limits.endpoint:", result.stdout)

    def test_native_in_candidates_fails(self):
        result = limits(endpoint_candidates=["bun", "node-worker", "native"])
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("FAIL limits.endpoint_candidates:", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
