#!/usr/bin/env python3
"""Regression tests for the build cache key and the endpoint launchers."""

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location("sole_comb_build", Path(__file__).with_name("build.py"))
build = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(build)
PINS = json.loads(Path(__file__).with_name("toolchain.json").read_text())
WORKER = "require('node:worker_threads').parentPort; console.log('WORKER-OK')\n"


class SourceTests(unittest.TestCase):
    def test_source_hashes_cover_indented_and_foreign_imports(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            (root / "bin").mkdir()
            (root / "lib").mkdir()
            (root / "bin/sole-comb.bend").write_text(
                'import Base\n  import ../lib/b.bend as B\n\ndef io(p: String) -> String:\n'
                '  import "./os.js"\n\ndef main() -> U32:\n  42\n')
            (root / "lib/b.bend").write_text("def b() -> U32:\n  1\n")
            (root / "bin/os.js").write_text("export const a = 1;\n")
            with patch.object(build, "ROOT", root):
                before = build.source_hashes()
                (root / "bin/os.js").write_text("export const a = 2;\n")
                after = build.source_hashes()
        self.assertEqual(sorted(before), ["bin/os.js", "bin/sole-comb.bend", "lib/b.bend"])
        self.assertNotEqual(before, after)

    def test_foreign_import_cannot_escape_the_repository(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve() / "repo"
            (root / "bin").mkdir(parents=True)
            (root / "bin/sole-comb.bend").write_text('import Base\n\ndef io() -> U32:\n  import "../../os.js"\n')
            with patch.object(build, "ROOT", root), self.assertRaises(ValueError):
                build.source_hashes()


class LauncherTests(unittest.TestCase):
    def launch(self, command, cwd):
        # No shell: darwin /bin/sh `exec a/b` rewrites $0 to an absolute path and hides the defect.
        return subprocess.run([command], cwd=cwd, capture_output=True, timeout=60)

    def test_node_worker_launcher_accepts_bare_relative_invocation(self):
        pins = {"stack_kib": 1024, "endpoint": "node-worker", "endpoint_candidates": ["bun", "node-worker"],
                "tools": {"bun": {"path": "/bin/false"}, "node": {"path": PINS["tools"]["node"]["path"]}}}
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve() / "repo"
            output = root / "_build/sole-comb.js"
            output.parent.mkdir(parents=True)
            output.write_text(WORKER)
            with patch.object(build, "ROOT", root), contextlib.redirect_stdout(io.StringIO()):
                build.endpoint_launchers(pins, "js", output)
                build.activate(pins)
            for command, cwd in [("./_build/endpoint/node-worker", root),
                                 ("_build/endpoint/node-worker", root),
                                 ("repo/sole-comb", root.parent)]:
                with self.subTest(command=command):
                    result = self.launch(command, cwd)
                    self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
                    self.assertIn(b"WORKER-OK", result.stdout)

    def test_activate_refuses_info_only_native_endpoint(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            (root / "_build/endpoint").mkdir(parents=True)
            (root / "_build/endpoint/native").write_text("#!/bin/sh\n")
            pins = {"endpoint": "native", "endpoint_candidates": ["bun", "node-worker"]}
            with patch.object(build, "ROOT", root), self.assertRaises(ValueError):
                build.activate(pins)
            self.assertFalse((root / "sole-comb").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
