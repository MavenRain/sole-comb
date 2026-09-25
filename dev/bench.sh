#!/bin/zsh
# Compatibility entry point: dev/bench.sh NAME 'exec COMMAND ARGS'.
# RUNS defaults to 5; BENCH_POLL_S and SOLE_COMB_TOOLCHAIN remain supported.
# The Python CLI also supports JSON reports, acceptance lines and preflight.
set -u
if [ $# -ne 2 ]; then
  print -r -- "BENCH-USAGE bench.sh NAME CMD"
  exit 2
fi
exec python3 -P "${0:A:h}/bench.py" -- "$1" "$2"
