# Stage A entry decision

Adopted on 2026-09-27 by the user: "By the end of the next turn, we must be
working on the real compiler inside the actual compiler codebase."
This approves moving real compiler implementation ahead of scratch R2
completion. It supersedes the scratch-first entry conditions in M0-PLAN,
the S0-5 closeout, and the initial Stage A milestone schedule.

Stage A now proceeds in the actual repository under `lib/`, starting with
A.1 foundation and then A.2 representation. Further scratch compiler
feature work is retired. Existing scratch code and validation records
remain historical evidence with their original, limited claims.

R2 qualification, measurement, confirmation, and endpoint selection remain
pending. This decision is neither an S0-5 pass nor a runtime selection.
The existing measurement method, load ceiling, sample counts, thresholds,
GREEN confirmation, and requirement for a written ruling on FAIL remain.
R2 becomes a gate on the operational real compiler before M0 closure,
and remains binding at M1. R2-RISK-A moves to that gate because Stage A's
pinned Kanon frontend cannot qualify sole-language workloads.

Before measurement, qualify complete workload twins and rejection fixtures
through the real compiler. Reconcile the old scratch translations with the
actual M0 language while preserving the declaration map and all four core
obligations. In particular, scratch U32 syntax does not authorize a new M0
primitive. Record any translation revision and qualify it afresh.

All other Stage A integration gates remain: the pinned semantics, complete
146-file oracle corpus, divergence policy, HOUSE, build-time checks, mutants,
fixes, and final review. A local milestone pass is not A.close.

The A.1 test and policy plumbing moves forward from A7 because the first
real source milestone needs it. The bootstrap CLI remains a host probe;
the compiler library is built and exercised through `make test-foundation`.
The compiler driver and an operational source checker are later milestones.
