#!/usr/bin/env python3
"""Stage 0 serial measurements. Child CPU, rather than wall time, binds R2.

The compatibility shell accepts one command string. Use a foreground command
(preferably exec); wait4 includes only descendants reaped by that child.
The shell path also counts the /bin/zsh -f start-up CPU (about 3 ms on darwin).
--argv splits COMMAND with shlex and runs it without a shell; S0-5 legs use it.
Thread counts are sampled observations of the direct child, not affinity proof.
"""

import argparse
import ctypes
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shlex
import signal
import statistics
import subprocess
import sys
import tempfile
import threading
import time


ROOT = Path(__file__).resolve().parent.parent
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")


def positive(value, name):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{name} must be a positive finite number")
    if not math.isfinite(value) or value <= 0:
        raise ValueError(f"{name} must be a positive finite number")
    return value


def limits(pins):
    return {key: positive(pins[key], key) for key in
            ("load_ceiling", "load_wait_max_s", "leg_deadline_s")}


class TaskInfo(ctypes.Structure):
    # Darwin SDK sys/proc_info.h, struct proc_taskinfo (PROC_PIDTASKINFO = 4).
    _fields_ = [(name, ctypes.c_uint64) for name in
                ("virtual", "resident", "user", "system", "threads_user", "threads_system")]
    _fields_ += [(name, ctypes.c_int32) for name in
                 ("policy", "faults", "pageins", "cow", "sent", "received", "mach",
                  "unix", "switches", "threadnum", "running", "priority")]


def thread_reader():
    if sys.platform == "darwin":
        library = ctypes.CDLL("/usr/lib/libproc.dylib", use_errno=True)
        query = library.proc_pidinfo
        query.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64,
                          ctypes.c_void_p, ctypes.c_int]
        query.restype = ctypes.c_int

        def read(pid):
            info = TaskInfo()
            size = ctypes.sizeof(info)
            if query(pid, 4, 0, ctypes.byref(info), size) != size:
                raise OSError(ctypes.get_errno(), "proc_pidinfo unavailable")
            return info.threadnum

        return read, "proc_pidinfo(PROC_PIDTASKINFO), direct child, 5ms sampling"
    if sys.platform.startswith("linux"):
        def read(pid):
            for line in Path(f"/proc/{pid}/status").read_text().splitlines():
                if line.startswith("Threads:"):
                    return int(line.split()[1])
            raise OSError("Threads field unavailable")

        return read, "/proc/PID/status, direct child, 5ms sampling"
    raise OSError("thread observation unsupported on this platform")


def output_record(stream, accept_line=None):
    stream.seek(0)
    digest = hashlib.sha256()
    excerpt = bytearray()
    matched = False
    continued = False
    # Bound both memory and individual lines, including output without newlines.
    for line in iter(lambda: stream.readline(65536), b""):
        digest.update(line)
        excerpt.extend(line[:max(0, 4096 - len(excerpt))])
        complete = line.endswith(b"\n") or len(line) < 65536
        if not continued and complete and accept_line is not None and ANSI.sub("", line.decode("utf-8", "replace")).strip() == accept_line:
            matched = True
        continued = not line.endswith(b"\n")
    return {"sha256": digest.hexdigest(), "excerpt": excerpt.decode("utf-8", "replace"),
            "bytes": stream.tell()}, matched


def kill_group(child):
    try:
        os.killpg(child.pid, signal.SIGKILL)
    # Darwin killpg returns EPERM when every group member is a zombie; those are dead and wait to be reaped.
    except (ProcessLookupError, PermissionError):
        pass


def measure(command, deadline_s, accept_line=None):
    """Reap this child with wait4; observer CPU never enters its CPU accounting."""
    observations = {"threads_peak": None, "thread_observations": 0,
                    "thread_observation_error": None, "timed_out": False,
                    "load1_peak": os.getloadavg()[0], "load_observation_error": None}
    try:
        read_threads, method = thread_reader()
    except OSError as error:
        read_threads, method = None, "unavailable"
        observations["thread_observation_error"] = str(error)
    done = threading.Event()
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        start = time.monotonic()
        child = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=stdout,
                                 stderr=stderr, start_new_session=True,
                                 env={**os.environ, "BEND_NO_TELEMETRY": "1"})

        def observe():
            while not done.is_set():
                remaining = deadline_s - (time.monotonic() - start)
                if remaining <= 0:
                    observations["timed_out"] = True
                    kill_group(child)
                    return
                try:
                    load = os.getloadavg()[0]
                    if not math.isfinite(load):
                        raise OSError("non-finite load average")
                    observations["load1_peak"] = max(observations["load1_peak"], load)
                except OSError as error:
                    observations["load_observation_error"] = str(error)
                if read_threads is not None:
                    try:
                        count = read_threads(child.pid)
                        if count > 0:
                            observations["threads_peak"] = max(observations["threads_peak"] or 0, count)
                            observations["thread_observations"] += 1
                    except (OSError, ValueError) as error:
                        observations["thread_observation_error"] = str(error)
                done.wait(min(0.005, remaining))

        observer = threading.Thread(target=observe, daemon=True)
        observer.start()
        try:
            _, status, usage = os.wait4(child.pid, 0)
            child.returncode = os.waitstatus_to_exitcode(status)
            wall_ms = (time.monotonic() - start) * 1000
        except BaseException:
            kill_group(child)
            _, status, _ = os.wait4(child.pid, 0)
            child.returncode = os.waitstatus_to_exitcode(status)
            raise
        finally:
            done.set()
            observer.join()
        # A late watchdog cannot turn an over-deadline run into a valid sample.
        if wall_ms > deadline_s * 1000:
            observations["timed_out"] = True
            kill_group(child)
        out, matched = output_record(stdout, accept_line)
        err, _ = output_record(stderr)
    return {**observations, "thread_observation_method": method,
            "exit_code": child.returncode, "wall_ms": wall_ms,
            "user_cpu_ms": usage.ru_utime * 1000, "system_cpu_ms": usage.ru_stime * 1000,
            "cpu_ms": (usage.ru_utime + usage.ru_stime) * 1000,
            "stdout": out, "stderr": err, "accept_line_found": matched if accept_line else None}


def summary(samples):
    timed = [sample for sample in samples if sample["accepted"] and sample["run"] != "warm"]
    result = {"runs": len(timed)}
    for field in ("wall_ms", "cpu_ms", "user_cpu_ms", "system_cpu_ms"):
        values = [sample[field] for sample in timed]
        result[field] = {"median": statistics.median(values), "min": min(values), "max": max(values)}
    result["load1_min"] = min(sample["load1_start"] for sample in timed)
    result["load1_max"] = max(sample["load1_peak"] for sample in timed)
    return result


def run_leg(report, settings, command, runs=5, poll_s=30, accept_line=None,
            require_threads=False, cache_state="unknown", runner=measure,
            load=lambda: os.getloadavg()[0], now=time.monotonic, sleep=time.sleep,
            emit=print):
    """Keep rejected attempts in the report; only a complete leg has a median."""
    start = now()
    report.update(status="RUNNING", samples=[], command=command, cache_state=cache_state)
    for label in ["warm", *range(1, runs + 1)]:
        while True:
            before = load()
            if not math.isfinite(before):
                report.update(status="UNMET", reason="load unavailable")
                return 3
            if before > settings["load_ceiling"]:
                remaining = settings["load_wait_max_s"] - (now() - start)
                if remaining <= 0:
                    report.update(status="UNMET", reason="load wait exhausted", load1=before)
                    return 3
                emit(f"WAIT {report['name']} load1={before:.2f} ceiling={settings['load_ceiling']}")
                sleep(min(poll_s, remaining))
                continue
            sample = runner(command, settings["leg_deadline_s"], accept_line)
            after = load()
            valid_load = math.isfinite(after) and math.isfinite(sample["load1_peak"])
            sample.update(run=label, load1_start=before, load1_end=after if math.isfinite(after) else None,
                          cache_state=cache_state, accepted=False)
            sample["load1_peak"] = max(before, after, sample["load1_peak"]) if valid_load else None
            report["samples"].append(sample)
            if sample["timed_out"]:
                report.update(status="UNMET", reason="child deadline exceeded")
                return 3
            if sample["exit_code"] != 0 or (accept_line is not None and not sample["accept_line_found"]):
                report.update(status="ERROR", reason="child exit or acceptance line mismatch")
                return 1
            if sample.get("load_observation_error") or not valid_load:
                report.update(status="UNMET", reason="load observation unavailable")
                return 3
            if require_threads and sample["threads_peak"] is None:
                report.update(status="UNMET", reason="child thread count unavailable")
                return 3
            if sample["load1_peak"] > settings["load_ceiling"]:
                sample["discard_reason"] = "load above ceiling"
                emit(f"DISCARD {report['name']} run={label} load1_peak={sample['load1_peak']:.2f}")
                if now() - start >= settings["load_wait_max_s"]:
                    report.update(status="UNMET", reason="load retry budget exhausted")
                    return 3
                continue
            sample["accepted"] = True
            emit(f"SAMPLE {report['name']} run={label} wall_ms={sample['wall_ms']:.3f} "
                 f"cpu_ms={sample['cpu_ms']:.3f} user_cpu_ms={sample['user_cpu_ms']:.3f} "
                 f"system_cpu_ms={sample['system_cpu_ms']:.3f} load1={before:.2f} "
                 f"load1_peak={sample['load1_peak']:.2f} threads_peak={sample['threads_peak']}")
            break
    report.update(status="PASS", summary=summary(report["samples"]))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name")
    parser.add_argument("command", nargs="?", help="one foreground zsh command string, or an argv string with --argv")
    parser.add_argument("--argv", action="store_true", help="split COMMAND with shlex and run it without a shell")
    parser.add_argument("--toolchain", type=Path, default=Path(os.environ.get("SOLE_COMB_TOOLCHAIN", ROOT / "dev/toolchain.json")))
    parser.add_argument("--runs", type=int, default=os.environ.get("RUNS", "5"))
    parser.add_argument("--poll-s", type=float, default=os.environ.get("BENCH_POLL_S", "30"))
    parser.add_argument("--json", type=Path, help="write the full result, including failed attempts")
    parser.add_argument("--accept-line", help="require this complete stdout line, ignoring ANSI colors")
    parser.add_argument("--require-threads", action="store_true")
    parser.add_argument("--cache-state", choices=("cold", "warm", "unknown"), default="unknown")
    parser.add_argument("--preflight", action="store_true", help="check pins and current load without waiting or running a sample")
    args = parser.parse_args()
    if not args.preflight and not args.command:
        parser.error("command is required unless --preflight is used")
    report = {"schema": 1, "name": args.name, "phase": "preflight" if args.preflight else "measurement",
              "recorded_at": datetime.now(timezone.utc).isoformat(),
              "status": "ERROR", "toolchain": str(args.toolchain.resolve()), "samples": [],
              "launcher": "direct" if args.argv else "/bin/zsh -f -c",
              "cpu_method": ("wait4 child user + system CPU; observer excluded" if args.argv else
                             "wait4 child user + system CPU; observer excluded; "
                             "includes /bin/zsh -f start-up CPU (about 3 ms on darwin)"),
              "affinity": "not set", "compile_cache_state": args.cache_state,
              "accept_line": args.accept_line}
    code = 1
    try:
        positive(args.runs, "runs")
        positive(args.poll_s, "poll_s")
        data = args.toolchain.read_bytes()
        pins = json.loads(data)
        report.update(toolchain_sha256=hashlib.sha256(data).hexdigest(), limits=limits(pins))
        check = subprocess.run([sys.executable, "-P", str(ROOT / "dev/pin-check.py"),
                                "--toolchain", str(args.toolchain)], capture_output=True, text=True, timeout=180)
        report["pin_check"] = {"exit_code": check.returncode, "stdout": check.stdout, "stderr": check.stderr}
        if check.returncode:
            raise ValueError("toolchain pin check failed")
        if args.preflight:
            load = os.getloadavg()[0]
            ready = math.isfinite(load) and load <= report["limits"]["load_ceiling"]
            report.update(status="READY" if ready else "NOT_READY", load1=load if math.isfinite(load) else None,
                          reason="preflight only; no timed leg or R2 verdict")
            code = 0 if ready else 3
        else:
            command = shlex.split(args.command) if args.argv else ["/bin/zsh", "-f", "-c", args.command]
            if not command:
                raise ValueError("--argv COMMAND has no words")
            code = run_leg(report, report["limits"], command,
                           args.runs, args.poll_s, args.accept_line, args.require_threads,
                           args.cache_state, emit=lambda line: print(line, flush=True))
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        report.update(status="ERROR", reason=f"{type(error).__name__}: {error}")
    except KeyboardInterrupt:
        report.update(status="INTERRUPTED", reason="cancelled by operator")
        code = 130
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(mode="w", dir=args.json.parent, delete=False) as handle:
            json.dump(report, handle, indent=2, allow_nan=False)
            handle.write("\n")
            temporary = Path(handle.name)
        temporary.replace(args.json)
    if report["status"] == "PASS":
        result = report["summary"]
        print(f"BENCH {args.name} median_ms={result['wall_ms']['median']:.3f} "
              f"min_ms={result['wall_ms']['min']:.3f} max_ms={result['wall_ms']['max']:.3f} "
              f"cpu_median_ms={result['cpu_ms']['median']:.3f} cpu_min_ms={result['cpu_ms']['min']:.3f} "
              f"cpu_max_ms={result['cpu_ms']['max']:.3f} load1_min={result['load1_min']:.2f} "
              f"load1_max={result['load1_max']:.2f} runs={result['runs']}")
    else:
        print(f"BENCH-{report['status']} {args.name} {report.get('reason', '')}")
    return code


if __name__ == "__main__":
    sys.exit(main())
