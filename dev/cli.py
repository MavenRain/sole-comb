#!/usr/bin/env python3
"""Public sole-comb source-checking command."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
MAX_SOURCE_BYTES = 32768


class Parser(argparse.ArgumentParser):
    def error(self, message):
        self.print_usage(sys.stderr)
        self.exit(64, f"sole-comb: {message}\n")


def fail(code, message):
    print(message, file=sys.stderr)
    return code


def main():
    parser = Parser(prog="sole-comb", description="Check .sole-comb source with the public compiler.")
    commands = parser.add_subparsers(dest="command", parser_class=Parser)
    check = commands.add_parser("check", help="parse and type-check a source file")
    check.add_argument("file", type=Path)
    check.add_argument("--print", action="store_true", help="also print the checked kernel definitions")
    check.add_argument("--host", choices=("bun", "node-worker", "native"), help="override the development execution host")
    for verb in ("build", "run"):
        pending = commands.add_parser(verb, help="pending the erasure and WebAssembly backend")
        pending.add_argument("file", nargs="?")
    args = parser.parse_args()
    if args.command is None:
        parser.print_help(sys.stderr)
        return 64
    if args.command != "check":
        return fail(5, f"E-SOURCE-NOT-YET: sole-comb {args.command} needs the erasure and WebAssembly backend; use check")
    path = args.file
    if path.suffix != ".sole-comb":
        return fail(64, "E-CLI: source files must use the .sole-comb suffix")
    try:
        with path.open("rb") as stream:
            source = stream.read(MAX_SOURCE_BYTES + 1)
    except OSError as error:
        return fail(2, f"E-IO: {path}: {error.strerror}")
    if len(source) > MAX_SOURCE_BYTES:
        return fail(5, f"E-SOURCE-SIZE: {path}: this source-checking increment supports at most {MAX_SOURCE_BYTES} bytes")
    try:
        pins = json.loads((ROOT / "dev/toolchain.json").read_text())
        host = args.host or pins["endpoint"] or "bun"
        backend = "native" if host == "native" else "js"
        # build.py activate() also requires the launcher of the configured endpoint.
        endpoint = pins["endpoint"]
        needed = backend if endpoint in (None, host) or ("native" if endpoint == "native" else "js") == backend else "all"
        env = dict(os.environ, BEND_NO_TELEMETRY="1")
        built = subprocess.run([sys.executable, "-P", str(ROOT / "dev/build.py"), "--backend", needed],
                               cwd=ROOT, env=env, capture_output=True, timeout=pins["build_deadline_s"] + 30)
        if built.returncode:
            sys.stderr.buffer.write(built.stdout + built.stderr)
            return fail(2, "E-BUILD: the public compiler could not be built")
        launcher = ROOT / "_build/endpoint" / host
        result = subprocess.run([str(launcher), "check", source.hex()], cwd=ROOT, env=env,
                                capture_output=True, timeout=120)
    except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
        return fail(2, f"E-HOST: {error}")
    if result.returncode or result.stderr:
        sys.stderr.buffer.write(result.stderr)
        return fail(2, f"E-HOST: compiler host {host} failed with exit {result.returncode}" if result.returncode
                    else f"E-HOST: compiler host {host} wrote to stderr on exit 0")
    try:
        text = result.stdout.decode("utf-8")
    except UnicodeDecodeError:
        return fail(2, "E-HOST: invalid compiler output encoding")
    if not text.endswith("\n"):
        return fail(2, "E-HOST: incomplete compiler response")
    response = text[:-1]
    if response.startswith("FAIL\n"):
        return fail(1, f"CHECK {path} FAIL {response[5:]}")
    parts = response.split("\n", 2)
    if len(parts) != 3 or parts[0] != "OK" or not re.fullmatch(r"0|[1-9][0-9]*", parts[1]):
        return fail(2, "E-HOST: malformed compiler response")
    print(f"CHECK {path} defs={parts[1]} ok")
    if args.print:
        sys.stdout.write(parts[2])
    return 0


if __name__ == "__main__":
    sys.exit(main())
