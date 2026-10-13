.PHONY: build check test gates test-runner test-cli test-core-erasure test-public-family test-constructor-parameters test-constructor-inference test-family-defaults test-uat-readiness test-reference-fixtures

# sole-comb is a C host (tcc). build, check, test and gates are the targets;
# the harness targets below run the dev/test-*.py probes against build/sole-comb.

# The C build (dev/c.mk, tcc): c-check compiles each unit with -Wall -Werror,
# c-build links build/sole-comb. The launcher ./sole-comb execs that binary.
build:
	$(MAKE) -f dev/c.mk c-check c-build

# check = the C build + the `sole-comb test` verb over test/guest (the
# recorded goldens; the CHECK lines hold paths relative to test/guest).
check: build
	cd test/guest && ../../build/sole-comb test --expect 836 examples corpus test kanon

test: build check test-runner test-cli test-core-erasure test-public-family test-constructor-parameters test-constructor-inference test-family-defaults test-uat-readiness test-reference-fixtures

test-reference-fixtures:
	python3 -P dev/test-reference-fixtures.py

gates: test

test-runner: build
	$(MAKE) -f dev/c.mk c-test

# Every retained compiler probe also needs the binary in a parallel build.
test-cli test-core-erasure test-public-family test-constructor-parameters test-constructor-inference test-family-defaults test-uat-readiness: build

test-cli:
	python3 -P dev/test-cli.py

test-core-erasure:
	python3 -P dev/test-core-erasure.py

test-public-family:
	python3 -P dev/test-public-family.py

test-constructor-parameters:
	python3 -P dev/test-constructor-parameters.py

test-constructor-inference:
	python3 -P dev/test-constructor-inference.py

test-family-defaults:
	python3 -P dev/test-family-defaults.py

test-uat-readiness:
	python3 -P dev/test-uat-harness.py
	python3 -P dev/test-uat-readiness.py
