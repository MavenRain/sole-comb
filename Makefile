.PHONY: build check test test-bench test-r2 test-r2-run test-r2-prepare test-foundation test-representation test-evaluation house bench-preflight r2-report r2-run r2-prepare gates

R2_MANIFEST ?= /private/tmp/claude/kan-elim-lang-m0/r2-risk/manifest.json
R2_TOOLCHAIN ?= dev/toolchain.json
R2_PLAN ?= /private/tmp/claude/kan-elim-lang-m0/r2-risk/run-plan.json
R2_OUTPUT ?=
R2_PREPARE_PLAN ?= /private/tmp/claude/kan-elim-lang-m0/r2-risk/prepare-plan.json

# Stage A adds the real library closure alongside the pinned host and
# benchmark harness. The complete compiler suite arrives with A.5.

build:
	python3 -P dev/build.py

check: house test-foundation test-representation test-evaluation
	python3 -P dev/build.py --check

test: build test-bench test-r2 test-r2-run test-r2-prepare house test-foundation test-representation test-evaluation
	python3 -P dev/pin-check.py

test-bench:
	python3 -P dev/test-bench.py
	python3 -P dev/test-build.py
	python3 -P dev/test-pin-check.py

bench-preflight:
	python3 -P dev/bench.py s0-5 --preflight --json _build/bench/preflight.json

test-r2:
	python3 -P dev/test-r2-risk.py

test-r2-run:
	python3 -P dev/test-r2-run.py

test-r2-prepare:
	python3 -P dev/test-r2-prepare.py

r2-prepare:
	@test -n "$(R2_OUTPUT)" || { echo 'Set R2_OUTPUT to a new scratch directory.' >&2; exit 2; }
	python3 -P dev/r2-prepare.py --plan "$(R2_PREPARE_PLAN)" --toolchain "$(R2_TOOLCHAIN)" --output "$(R2_OUTPUT)"

r2-run:
	@test -n "$(R2_OUTPUT)" || { echo 'Set R2_OUTPUT to a new scratch directory.' >&2; exit 2; }
	python3 -P dev/r2-run.py --plan "$(R2_PLAN)" --toolchain "$(R2_TOOLCHAIN)" --output "$(R2_OUTPUT)"

r2-report:
	python3 -P dev/r2-risk.py --manifest "$(R2_MANIFEST)" --toolchain "$(R2_TOOLCHAIN)" --json dev/r2-risk.json

house:
	python3 -P dev/test-house.py
	python3 -P dev/house-bend.py

test-foundation:
	python3 -P dev/test-foundation.py

test-representation:
	python3 -P dev/test-representation.py

test-evaluation:
	python3 -P dev/test-evaluation.py

gates: test-bench test-r2 test-r2-run test-r2-prepare house test-foundation test-representation test-evaluation
	python3 -P dev/pin-check.py
	python3 -P dev/build.py --check
