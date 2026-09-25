.PHONY: build check test gates

# Stage 0: no compiler code and no test suite yet. test and gates run the
# pin check; later stages add dev/test-all.py and dev/gates.sh.

build:
	python3 -P dev/build.py

check:
	python3 -P dev/build.py --check

test: build
	python3 -P dev/pin-check.py

gates:
	python3 -P dev/pin-check.py
	python3 -P dev/build.py --check
