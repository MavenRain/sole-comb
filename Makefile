.PHONY: build check test test-bench test-r2 test-r2-run test-r2-prepare test-foundation test-representation test-evaluation test-checking test-checking-mutations test-pinfront test-pinfront-mutations test-elaboration test-elaboration-mutations test-program test-program-mutations test-erasure test-erasure-mutations house bench-preflight r2-report r2-run r2-prepare gates
.PHONY: test-cli test-core-erasure test-core-erasure-mutations test-family-layout test-family-layout-mutations
.PHONY: test-reference-fixtures
.PHONY: test-family-erasure test-family-erasure-mutations
.PHONY: test-public-family test-public-family-mutations
.PHONY: test-constructor-parameters test-constructor-parameters-mutations
.PHONY: test-constructor-inference test-constructor-inference-mutations
.PHONY: test-family-defaults test-family-defaults-mutations
.PHONY: test-uat-readiness
.PHONY: test-recursor-layout test-recursor-layout-mutations
.PHONY: test-recursor-uniform test-recursor-uniform-mutations

R2_MANIFEST ?= /private/tmp/claude/kan-elim-lang-m0/r2-risk/manifest.json
R2_TOOLCHAIN ?= dev/toolchain.json
R2_PLAN ?= /private/tmp/claude/kan-elim-lang-m0/r2-risk/run-plan.json
R2_OUTPUT ?=
R2_PREPARE_PLAN ?= /private/tmp/claude/kan-elim-lang-m0/r2-risk/prepare-plan.json

# Stage A adds the real library closure alongside the pinned host and
# benchmark harness. The complete compiler suite arrives with A.5.

build:
	python3 -P dev/build.py

check: house test-reference-fixtures test-foundation test-representation test-evaluation test-checking test-pinfront test-elaboration test-program test-erasure test-cli test-core-erasure test-family-layout test-family-erasure test-public-family test-constructor-parameters test-constructor-inference test-family-defaults test-uat-readiness test-recursor-layout test-recursor-uniform
	python3 -P dev/build.py --check

test: build test-bench test-r2 test-r2-run test-r2-prepare house test-reference-fixtures test-foundation test-representation test-evaluation test-checking-mutations test-pinfront-mutations test-elaboration-mutations test-program-mutations test-erasure-mutations test-cli test-core-erasure-mutations test-family-layout-mutations test-family-erasure-mutations test-public-family-mutations test-constructor-parameters-mutations test-constructor-inference-mutations test-family-defaults-mutations test-uat-readiness test-recursor-layout-mutations test-recursor-uniform-mutations
	python3 -P dev/pin-check.py

test-bench:
	python3 -P dev/test-bench.py
	python3 -P dev/test-build.py
	python3 -P dev/test-pin-check.py

test-reference-fixtures:
	python3 -P dev/test-reference-fixtures.py

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

test-checking:
	python3 -P dev/test-checking.py

test-checking-mutations: test-checking
	python3 -P dev/test-checking-mutations.py

test-pinfront:
	python3 -P dev/test-pinfront-harness.py
	python3 -P dev/test-pinfront.py

test-pinfront-mutations: test-pinfront
	python3 -P dev/test-pinfront-mutations.py

test-elaboration:
	python3 -P dev/test-elaboration.py

test-elaboration-mutations: test-elaboration
	python3 -P dev/test-elaboration-mutations.py

test-program:
	python3 -P dev/test-program-harness.py
	python3 -P dev/test-program.py

test-program-mutations: test-program
	python3 -P dev/test-program-mutations.py

test-erasure:
	python3 -P dev/test-erasure.py

test-erasure-mutations: test-erasure
	python3 -P dev/test-erasure-mutations.py

gates: test-bench test-r2 test-r2-run test-r2-prepare house test-reference-fixtures test-foundation test-representation test-evaluation test-checking-mutations test-pinfront-mutations test-elaboration-mutations test-program-mutations test-erasure-mutations test-cli test-core-erasure-mutations test-family-layout-mutations test-family-erasure-mutations test-public-family-mutations test-constructor-parameters-mutations test-constructor-inference-mutations test-family-defaults-mutations test-uat-readiness test-recursor-layout-mutations test-recursor-uniform-mutations
	python3 -P dev/pin-check.py
	python3 -P dev/build.py --check

test-cli:
	python3 -P dev/test-cli.py

test-core-erasure:
	python3 -P dev/test-core-erasure.py

test-core-erasure-mutations: test-core-erasure
	python3 -P dev/test-core-erasure-mutations.py

test-family-layout:
	python3 -P dev/test-family-layout.py

test-family-layout-mutations:
	python3 -P dev/test-family-layout.py --mutations

test-family-erasure:
	python3 -P dev/test-family-erasure.py

test-family-erasure-mutations:
	python3 -P dev/test-family-erasure.py --mutations

test-public-family:
	python3 -P dev/test-public-family.py

test-public-family-mutations:
	python3 -P dev/test-public-family.py --mutations

test-constructor-parameters:
	python3 -P dev/test-constructor-parameters.py

test-constructor-parameters-mutations:
	python3 -P dev/test-constructor-parameters.py --mutations

test-constructor-inference:
	python3 -P dev/test-constructor-inference.py

test-constructor-inference-mutations:
	python3 -P dev/test-constructor-inference.py --mutations

test-family-defaults:
	python3 -P dev/test-family-defaults.py

test-family-defaults-mutations:
	python3 -P dev/test-family-defaults.py --mutations

test-uat-readiness:
	python3 -P dev/test-uat-harness.py
	python3 -P dev/test-uat-readiness.py

test-recursor-layout:
	python3 -P dev/test-recursor-layout.py

test-recursor-layout-mutations:
	python3 -P dev/test-recursor-layout.py --mutations

test-recursor-uniform:
	python3 -P dev/test-recursor-layout.py --uniform

test-recursor-uniform-mutations:
	python3 -P dev/test-recursor-layout.py --uniform --mutations

.PHONY: test-recursor-motive test-recursor-motive-mutations

check: test-recursor-motive
test gates: test-recursor-motive-mutations

test-recursor-motive:
	python3 -P dev/test-recursor-motive.py

test-recursor-motive-mutations:
	python3 -P dev/test-recursor-motive.py --mutations

.PHONY: test-recursor-branch test-recursor-branch-mutations

check: test-recursor-branch
test gates: test-recursor-branch-mutations

test-recursor-branch:
	python3 -P dev/test-recursor-branch.py

test-recursor-branch-mutations:
	python3 -P dev/test-recursor-branch.py --mutations

.PHONY: test-recursor-body test-recursor-body-mutations

check: test-recursor-body
test gates: test-recursor-body-mutations

test-recursor-body:
	python3 -P dev/test-recursor-body.py

test-recursor-body-mutations:
	python3 -P dev/test-recursor-body.py --mutations
