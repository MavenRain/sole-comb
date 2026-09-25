.PHONY: build check test test-bench test-r2 bench-preflight r2-report gates

R2_MANIFEST ?= /private/tmp/claude/kan-elim-lang-m0/r2-risk/manifest.json
R2_TOOLCHAIN ?= dev/toolchain.json

# Stage 0 validates the pinned host and benchmark harness. The compiler
# suite and dev/gates.sh are added by later stages.

build:
	python3 -P dev/build.py

check:
	python3 -P dev/build.py --check

test: build test-bench test-r2
	python3 -P dev/pin-check.py

test-bench:
	python3 -P dev/test-bench.py
	python3 -P dev/test-build.py
	python3 -P dev/test-pin-check.py

bench-preflight:
	python3 -P dev/bench.py s0-5 --preflight --json _build/bench/preflight.json

test-r2:
	python3 -P dev/test-r2-risk.py

r2-report:
	python3 -P dev/r2-risk.py --manifest "$(R2_MANIFEST)" --toolchain "$(R2_TOOLCHAIN)" --json dev/r2-risk.json

gates: test-bench test-r2
	python3 -P dev/pin-check.py
	python3 -P dev/build.py --check
