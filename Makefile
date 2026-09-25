.PHONY: build check test test-bench bench-preflight gates

# Stage 0 validates the pinned host and benchmark harness. The compiler
# suite and dev/gates.sh are added by later stages.

build:
	python3 -P dev/build.py

check:
	python3 -P dev/build.py --check

test: build test-bench
	python3 -P dev/pin-check.py

test-bench:
	python3 -P dev/test-bench.py
	python3 -P dev/test-build.py
	python3 -P dev/test-pin-check.py

bench-preflight:
	python3 -P dev/bench.py s0-5 --preflight --json _build/bench/preflight.json

gates: test-bench
	python3 -P dev/pin-check.py
	python3 -P dev/build.py --check
