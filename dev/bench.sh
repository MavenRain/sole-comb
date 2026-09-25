#!/bin/zsh
# dev/bench.sh NAME CMD
# Copied from attest/dev/bench.sh (M0 plan S0-3). Adds the child CPU from
# rusage and the one-minute load average per sample, and the load gate.
#
# Runs CMD once untimed as a warm-up, then RUNS timed runs (RUNS defaults to 5).
# CMD is one string, run as zsh -f -c CMD, with stdout and stderr sent to /dev/null.
# The wall timer is perf_counter_ns around subprocess.run. The CPU is the
# RUSAGE_CHILDREN user plus system time delta around the same call. Interpreter
# start-up is outside every measurement.
#
# Load gate: before every run (the warm-up too) the one-minute load average is
# read. While it is over load_ceiling of dev/toolchain.json the harness prints
# WAIT and sleeps. Past load_wait_max_s (for the whole leg) it prints
# BENCH-UNMET and runs nothing more. Each run is bounded by leg_deadline_s.
# $SOLE_COMB_TOOLCHAIN overrides the toolchain path. $BENCH_POLL_S sets the
# WAIT poll interval (default 30).
#
# Output:
#   SAMPLE NAME run=warm wall_ms=12.345 cpu_ms=10.000 load1=3.10
#   SAMPLE NAME run=1 wall_ms=... cpu_ms=... load1=...
#   BENCH NAME median_ms=12.345 min_ms=11.000 max_ms=14.200 cpu_median_ms=10.000 cpu_min_ms=9.000 cpu_max_ms=11.000 load1_min=3.10 load1_max=3.40 runs=5
# A non-zero child exit prints BENCH-ERROR NAME exit=N and exits 1.
# A load wait or a run past its deadline prints BENCH-UNMET NAME ... and exits 3.

set -u

if [ $# -ne 2 ]; then
  print -r -- "BENCH-USAGE bench.sh NAME CMD"
  exit 2
fi

BENCH_NAME=$1
BENCH_CMD=$2
BENCH_RUNS=${RUNS:-5}
BENCH_TOOLCHAIN=${SOLE_COMB_TOOLCHAIN:-${0:A:h:h}/dev/toolchain.json}

exec python3 -P - "$BENCH_NAME" "$BENCH_CMD" "$BENCH_RUNS" "$BENCH_TOOLCHAIN" "${BENCH_POLL_S:-30}" <<'BENCH_TIMER_EOF'
import json
import os
import resource
import statistics
import subprocess
import sys
import time

name = sys.argv[1]
cmd = sys.argv[2]
runs = int(sys.argv[3])
with open(sys.argv[4]) as handle:
    pins = json.load(handle)
poll_s = float(sys.argv[5])
ceiling = float(pins["load_ceiling"])
wait_max_s = float(pins["load_wait_max_s"])
deadline_s = float(pins["leg_deadline_s"])
leg_start = time.monotonic()


def children_cpu_ms():
    usage = resource.getrusage(resource.RUSAGE_CHILDREN)
    return (usage.ru_utime + usage.ru_stime) * 1000.0


def load1():
    return os.getloadavg()[0]


def unmet(detail):
    print("BENCH-UNMET {0} {1}".format(name, detail))
    sys.exit(3)


def gate():
    load = load1()
    while load > ceiling:
        waited = time.monotonic() - leg_start
        if waited >= wait_max_s:
            unmet("reason=load load1={0:.2f} ceiling={1} waited_s={2:.0f}".format(load, ceiling, waited))
        print("WAIT {0} load1={1:.2f} ceiling={2} waited_s={3:.0f}".format(name, load, ceiling, waited), flush=True)
        time.sleep(max(0.0, min(poll_s, wait_max_s - waited)))
        load = load1()
    return load


def timed(devnull, label):
    load = gate()
    cpu0 = children_cpu_ms()
    start = time.perf_counter_ns()
    try:
        code = subprocess.run(
            ["/bin/zsh", "-f", "-c", cmd],
            stdin=devnull,
            stdout=devnull,
            stderr=devnull,
            timeout=deadline_s,
        ).returncode
    except subprocess.TimeoutExpired:
        unmet("reason=deadline run={0} leg_deadline_s={1:.0f}".format(label, deadline_s))
    stop = time.perf_counter_ns()
    sample = (code, (stop - start) / 1000000.0, children_cpu_ms() - cpu0, load)
    print("SAMPLE {0} run={1} wall_ms={2:.3f} cpu_ms={3:.3f} load1={4:.2f}".format(name, label, *sample[1:]), flush=True)
    if code != 0:
        print("BENCH-ERROR {0} exit={1}".format(name, code))
        sys.exit(1)
    return sample


def report(samples):
    wall = [s[1] for s in samples]
    cpu = [s[2] for s in samples]
    loads = [s[3] for s in samples]
    print(
        "BENCH {0} median_ms={1:.3f} min_ms={2:.3f} max_ms={3:.3f} "
        "cpu_median_ms={4:.3f} cpu_min_ms={5:.3f} cpu_max_ms={6:.3f} "
        "load1_min={7:.2f} load1_max={8:.2f} runs={9}".format(
            name, statistics.median(wall), min(wall), max(wall),
            statistics.median(cpu), min(cpu), max(cpu), min(loads), max(loads), runs
        )
    )
    sys.exit(0)


with open(os.devnull, "r+b") as devnull:
    timed(devnull, "warm")
    samples = [timed(devnull, str(i + 1)) for i in range(runs)]

report(samples)
BENCH_TIMER_EOF
