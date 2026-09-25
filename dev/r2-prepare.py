#!/usr/bin/env python3
"""Build and qualify a scratch S0-5 probe without collecting timing samples."""

import argparse
from contextlib import ExitStack, contextmanager
import importlib.util
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location("sole_comb_collection", ROOT / "dev/r2-run.py")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)
r2 = runner.r2
REJECTIONS = ("lex", "parse", "type", "conversion")


class Interrupted(Exception):
    def __init__(self, signum):
        super().__init__(f"interrupted by signal {signum}")
        self.signum = signum


class Hold:
    """Defer cancellation while a child starts or its group is cleaned up, then deliver it.

    Blocking the signal mask instead would leak the mask into the child through exec.
    """
    depth = 0
    pending = None

    def __enter__(self):
        Hold.depth += 1

    def __exit__(self, kind, error, trace):
        Hold.depth -= 1
        signum = Hold.pending if Hold.depth == 0 else None
        if signum is not None:
            Hold.pending = None
        if signum is not None and not isinstance(error, Interrupted):
            raise Interrupted(signum)
        return False


@contextmanager
def cancellation():
    def stop(signum, frame):
        if Hold.depth > 0:
            Hold.pending = Hold.pending or signum
            return
        raise Interrupted(signum)

    previous = {s: signal.signal(s, stop) for s in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}
    try:
        yield
    finally:
        for signum, handler in previous.items():
            signal.signal(signum, handler)


def stop_group(child):
    # A cancellation during cleanup is delivered after the group is dead and reaped.
    with Hold():
        try:
            os.killpg(child.pid, signal.SIGKILL)
        # Darwin killpg returns EPERM when every group member is a zombie; those are dead and wait to be reaped.
        except (ProcessLookupError, PermissionError):
            pass
        finally:
            child.wait()


def execute(argv, cwd, env, deadline, stdout, stderr):
    """Stream logs to disk, with a deadline and cleanup of the process group."""
    with stdout.open("xb") as out, stderr.open("xb") as err, ExitStack() as spawn:
        # A signal before the try below would otherwise skip stop_group.
        spawn.enter_context(Hold())
        with subprocess.Popen(argv, cwd=cwd, env=env, stdout=out, stderr=err,
                              start_new_session=True) as child:
            try:
                spawn.close()
                return child.wait(timeout=deadline)
            except BaseException:
                stop_group(child)
                raise


def has_line(path, expected):
    needle = expected.encode()
    with path.open("rb") as stream:
        return any(line.rstrip(b"\r\n") == needle for line in stream)


class Preparation:
    def __init__(self, plan, toolchain, output):
        self.tracker = runner.Collection(plan, toolchain, output)
        self.output = self.tracker.output
        self.rows = []
        self.record = None

    def save(self):
        if self.record is not None:
            r2.write_report(self.output / "preparation.json", self.record)

    def validate(self):
        tracker = self.tracker
        plan_ref = tracker.reference(tracker.plan_path)
        toolchain_ref = tracker.reference(tracker.toolchain)
        self.plan = r2.read_json(tracker.plan_path.read_bytes())
        self.pins = r2.read_json(tracker.toolchain.read_bytes())
        r2.fields(self.plan, ("schema", "probe", "workloads", "empty", "startup", "rejects"), "preparation plan")
        if type(self.plan["schema"]) is not int or self.plan["schema"] != 1:
            raise ValueError("unsupported preparation plan schema")
        r2.fields(self.plan["probe"], ("entry",), "probe")
        self.entry = Path(tracker.input(self.plan["probe"]["entry"])["path"])
        if self.entry.suffix != ".bend":
            raise ValueError("probe entry must be a Bend source")
        protected = [ROOT, *(Path(p["checkout"]).resolve() for p in self.pins["prior_art"].values())]
        protected += [Path(self.pins[k]["checkout"]).resolve() for k in ("bend", "kanon")
                      if "checkout" in self.pins.get(k, {})]
        if self.output.exists() or any(self.output.is_relative_to(p) for p in protected):
            raise ValueError("output must be a new scratch directory outside project and tool checkouts")
        if any(self.entry.is_relative_to(p) for p in protected):
            raise ValueError("probe entry must be scratch code outside project and tool checkouts")
        self.sources = sorted(r2.bend_closure(self.entry))
        for source in self.sources:
            if source.is_relative_to(ROOT):
                raise ValueError("scratch probe sources must stay outside the sole-comb tree")
            tracker.reference(source)
        r2.fields(self.plan["workloads"], r2.WORKLOADS, "workloads")
        self.workloads = {}
        for name in r2.WORKLOADS:
            pair = self.plan["workloads"][name]
            r2.fields(pair, ("bend", "sole"), f"workload {name}")
            self.workloads[name] = {language: tracker.input(pair[language])["path"]
                                    for language in ("bend", "sole")}
            for path in r2.bend_closure(self.workloads[name]["bend"]):
                tracker.reference(path)
        for language in ("bend", "sole"):
            paths = [self.workloads[w][language] for w in r2.WORKLOADS]
            if tracker.files[paths[0]]["sha256"] == tracker.files[paths[1]]["sha256"]:
                raise ValueError("small and conversion workloads must be distinct")
        self.empty = tracker.input(self.plan["empty"])["path"]
        if Path(self.empty).read_bytes().strip():
            raise ValueError("empty probe input must contain only whitespace")
        r2.fields(self.plan["rejects"], REJECTIONS, "rejects")
        self.rejects = {name: tracker.input(path)["path"] for name, path in self.plan["rejects"].items()}
        reject_hashes = [tracker.files[path]["sha256"] for path in self.rejects.values()]
        accepted_hashes = {tracker.files[self.empty]["sha256"],
                           *(tracker.files[pair["sole"]]["sha256"] for pair in self.workloads.values())}
        if len(set(reject_hashes)) != len(REJECTIONS) or accepted_hashes.intersection(reject_hashes):
            raise ValueError("rejection cases must be distinct from each other and accepted inputs")
        r2.fields(self.plan["startup"], r2.CANDIDATES, "startup")
        self.startup = {}
        for endpoint, project in zip(r2.CANDIDATES, ("attest", "assay")):
            entry = self.plan["startup"][endpoint]
            r2.fields(entry, ("origin", "input", "arguments", "accept_line", "cache_state"), f"startup {endpoint}")
            origin = tracker.input(entry["origin"])["path"]
            checkout = Path(self.pins["prior_art"][project]["checkout"]).resolve()
            if not Path(origin).is_relative_to(checkout) or Path(origin).name != f"{project}.js":
                raise ValueError(f"{endpoint}: origin must be the prebuilt {project}.js")
            if (not isinstance(entry["arguments"], list) or
                    any(not isinstance(a, str) or "\0" in a for a in entry["arguments"])):
                raise ValueError("startup arguments must be a list of strings")
            accept = entry["accept_line"]
            if not isinstance(accept, str) or not accept.strip() or any(c in accept for c in "\n\r\0"):
                raise ValueError("startup acceptance must be one nonempty line")
            if entry["cache_state"] not in ("cold", "warm"):
                raise ValueError("startup cache state must be cold or warm")
            self.startup[endpoint] = {**entry, "origin": origin, "input": tracker.input(entry["input"])["path"]}
        for key in ("build_deadline_s", "leg_deadline_s", "stack_kib"):
            if type(self.pins[key]) is not int or self.pins[key] <= 0:
                raise ValueError(f"{key} must be a positive integer")
        tracker.verify()
        self.output.mkdir(parents=True, exist_ok=False)
        (self.output / "logs").mkdir()
        (self.output / "startup").mkdir()
        (self.output / "bend-cache").mkdir()
        self.env = {**os.environ, "BEND_NO_TELEMETRY": "1", "BEND_LIB": str(self.output / "bend-cache"),
                    "CC": os.environ.get("SOLE_COMB_CC", os.environ.get("CC", shutil.which("clang") or "clang"))}
        self.record = {"schema": 1, "status": "PREPARING", "recorded_at": runner.now(),
                       "plan": plan_ref, "toolchain": toolchain_ref, "steps": self.rows,
                       "endpoint": None, "verdict": None}
        self.save()

    def run(self, name, argv, deadline, accept=None, reject=False):
        self.tracker.verify()
        out = self.output / "logs" / f"{name}.stdout"
        err = self.output / "logs" / f"{name}.stderr"
        row = {"name": name, "command": argv, "status": "RUNNING", "deadline_s": deadline}
        self.rows.append(row)
        self.save()
        print(f"R2-PREPARE {name}", flush=True)
        try:
            code = execute(argv, self.output, self.env, deadline, out, err)
            row["exit_code"] = code
            # A crash or a missing file is not evidence of deliberate rejection.
            ok = code == (1 if reject else 0)
            if accept is not None:
                ok = ok and has_line(out, accept)
            if accept == "ACCEPT":
                ok = ok and not has_line(out, "REJECT")
            if reject:
                ok = ok and not has_line(out, "ACCEPT")
            row["status"] = "OK" if ok else "FAILED"
        except subprocess.TimeoutExpired:
            row["status"] = "UNMET"
            row["reason"] = "child deadline exceeded"
        except (Interrupted, KeyboardInterrupt):
            row["status"] = "CANCELLED"
            raise
        except OSError as error:
            row["status"] = "ERROR"
            row["reason"] = str(error)
        finally:
            for key, path in (("stdout", out), ("stderr", err)):
                if path.is_file():
                    row[key] = self.tracker.reference(path)
            self.save()
        self.tracker.verify()
        return row

    def require(self, row):
        if row["status"] != "OK":
            raise runner.Stopped(row["status"], f"{row['name']}: {row['status']}",
                                 3 if row["status"] == "UNMET" else 1)

    def pin_check(self, name):
        self.require(self.run(name, [sys.executable, "-P", str(ROOT / "dev/pin-check.py"),
                                    "--toolchain", str(self.tracker.toolchain)], self.pins["build_deadline_s"]))

    def build(self, backend, target):
        row = self.run(f"build-{backend}", [self.pins["bend"]["binary"], str(self.entry), "-o", str(target)],
                       self.pins["build_deadline_s"])
        # The pinned compiler reports on stderr, so the handed-off build log is stdout + stderr.
        log = self.output / "logs" / f"build-{backend}.log"
        with log.open("xb") as handle:
            handle.write(b"".join(Path(row[k]["path"]).read_bytes() for k in ("stdout", "stderr") if k in row))
        row["log"] = self.tracker.reference(log)
        if row["status"] == "OK":
            if not target.is_file() or target.stat().st_size == 0:
                row.update(status="ERROR", reason="build did not produce a nonempty artifact")
            elif backend == "native" and not os.access(target, os.X_OK):
                row.update(status="ERROR", reason="native build did not produce an executable")
            else:
                row["artifact"] = self.tracker.reference(target)
        self.save()
        return row

    def qualify(self, endpoint, bundle):
        accepted = {**{name: pair["sole"] for name, pair in self.workloads.items()}, "empty": self.empty}
        rows = []
        for name, path in accepted.items():
            rows.append(self.run(f"{endpoint}-{name}", r2.command(self.pins, endpoint, str(bundle), [path]),
                                 self.pins["leg_deadline_s"], "ACCEPT"))
        for name, path in self.rejects.items():
            rows.append(self.run(f"{endpoint}-reject-{name}", r2.command(self.pins, endpoint, str(bundle), [path]),
                                 self.pins["leg_deadline_s"], "REJECT", reject=True))
        return all(row["status"] == "OK" for row in rows)

    def prepare(self):
        self.validate()
        self.pin_check("pins-before")
        javascript, native = self.output / "probe.js", self.output / "probe-native.exe"
        js_row = self.build("javascript", javascript)
        self.require(js_row)
        native_row = self.build("native", native)
        for name, pair in self.workloads.items():
            self.require(self.run(f"denominator-{name}",
                                  [self.pins["bend"]["binary"], pair["bend"], "--check-only"],
                                  self.pins["leg_deadline_s"], "All terms check."))
        candidates = {endpoint: self.qualify(endpoint, javascript) for endpoint in r2.CANDIDATES}
        self.record["qualified_candidates"] = candidates
        bad = {e: [row for row in self.rows if row["status"] != "OK" and row["name"].startswith(f"{e}-")]
               for e in r2.CANDIDATES}
        # A harness error is not evidence about the endpoint, so it stops preparation instead of dropping it.
        errors = [row for e in r2.CANDIDATES for row in bad[e] if row["status"] == "ERROR"]
        if errors:
            raise runner.Stopped("ERROR", "candidate check harness error: "
                                 + ", ".join(f"{row['name']} {row.get('reason', '')}" for row in errors), 1)
        # A failed check fails again on retry, so it outranks a deadline on the same endpoint.
        failed = [e for e in r2.CANDIDATES if any(row["status"] == "FAILED" for row in bad[e])]
        unmet = [e for e in r2.CANDIDATES if not candidates[e] and e not in failed]
        if len(failed) == len(r2.CANDIDATES):
            raise ValueError("no candidate endpoint passed correctness qualification: "
                             + ", ".join(f"{row['name']} {row['status']}"
                                         for e in failed for row in bad[e] if row["status"] == "FAILED"))
        if unmet:
            raise runner.Stopped("UNMET", "candidate correctness check exceeded its deadline: " + ", ".join(unmet), 3)
        # S0-5 records and drops a failing endpoint. The run plan carries the drop so that
        # the collector keeps an invalid checker out of selection even when it is faster.
        self.record["disqualified"] = failed
        if native_row["status"] == "OK":
            self.record["native_qualified"] = self.qualify("native", native)
        for endpoint, entry in self.startup.items():
            copy = self.output / "startup" / Path(entry["origin"]).name
            shutil.copyfile(entry["origin"], copy)
            ref = self.tracker.reference(copy)
            if ref["sha256"] != self.tracker.files[entry["origin"]]["sha256"]:
                raise ValueError("startup origin changed while copying")
            self.require(self.run(f"startup-{endpoint}",
                                  r2.command(self.pins, endpoint, str(copy), [*entry["arguments"], entry["input"]]),
                                  self.pins["leg_deadline_s"], entry["accept_line"]))
        self.pin_check("pins-after")
        self.tracker.verify()
        plan = {"schema": 1, "probe": {"sources": [str(p) for p in self.sources],
                                        "javascript": str(javascript), "build_log": js_row["log"]["path"]},
                "workloads": self.workloads, "empty": self.empty, "startup": self.startup,
                "native": ({"binary": str(native), "build_log": native_row["log"]["path"]}
                           if native_row["status"] == "OK" and self.record.get("native_qualified")
                           else {"unavailable": str(self.output / "native-unavailable.json")})}
        if "unavailable" in plan["native"]:
            r2.write_report(Path(plan["native"]["unavailable"]),
                            {"build": native_row, "qualified": self.record.get("native_qualified"),
                             "checks": [row for row in self.rows if row["name"].startswith("native-")]})
            self.tracker.reference(Path(plan["native"]["unavailable"]))
        # Validate the exact handoff with the existing collector before publishing.
        candidate = self.output / "candidate-plan.json"
        r2.write_report(candidate, plan)
        try:
            validation = runner.Collection(candidate, self.tracker.toolchain, self.output / "contract-check")
            validation.prepare()
            validation.verify()
        finally:
            # A retained contract-check input must not load as an unbound manual run plan.
            r2.write_report(candidate, {"diagnostic": "collector contract-check input; not a run plan", "plan": plan})
        self.tracker.verify()
        # A manual plan cannot drop a candidate, so the drop is published only with the preparation binding.
        plan.update(preparation=str(self.output / "preparation.json"), disqualified=self.record["disqualified"])
        pending = self.output / "run-plan.pending.json"
        r2.write_report(pending, plan)
        self.record.update(status="READY", completed_at=runner.now(),
                           run_plan={"path": str(self.output / "run-plan.json"),
                                     "sha256": r2.digest(pending.read_bytes())},
                           files=list(self.tracker.files.values()))
        self.save()
        pending.replace(self.output / "run-plan.json")
        print(f"R2-PREPARE READY plan={self.output / 'run-plan.json'}", flush=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--toolchain", type=Path, default=ROOT / "dev/toolchain.json")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    preparation = None
    try:
        with cancellation():
            preparation = Preparation(args.plan, args.toolchain, args.output)
            preparation.prepare()
        return 0
    except (Interrupted, KeyboardInterrupt) as error:
        status, code = "CANCELLED", 128 + (error.signum if isinstance(error, Interrupted) else signal.SIGINT)
        reason = str(error) or "interrupted"
    except runner.Stopped as error:
        status, code = error.status, error.code
        reason = str(error)
    except (ValueError, OSError, KeyError, TypeError) as error:
        status, code = "ERROR", 1
        reason = str(error)
    if preparation is not None and preparation.record is not None:
        preparation.record.update(status=status, reason=reason, completed_at=runner.now())
        preparation.save()
    print(f"R2-PREPARE {status}: {reason}", file=sys.stderr)
    return code


if __name__ == "__main__":
    sys.exit(main())
