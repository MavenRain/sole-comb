# Public source-checking validation

On 2026-09-29, `python3 -P dev/test-cli.py --hosts bun,node-worker,native`
passed 55 public-command observations. Each host accepted the four native
examples and rejected ten source fixtures, including missing elimination
arms, wrong payload annotations and result types, affine duplication,
unbound names, and reserved legacy forms. Two additional lexical boundary
cases per host covered comments, permitted identifiers, and byte literals.
Seven command checks covered help, missing files, wrong suffixes, unknown
commands, pending build/run, and the source-size limit.

The exact stdout, stderr, exit statuses, and tested source hashes are in
`public-source-checking.json`. Validation used `./sole-comb`; it did not invoke
the separate pinned test driver. Source hashes were stable throughout the run. Each host printed the same
kernel definitions for each positive example.

`dev/test-build.py` passed five regressions, including public launcher paths
and arguments with spaces. `dev/test-house.py` passed fifteen regressions.
The source policy passed over 42 host files, 32 line budgets, 35 registered
unsafe sites, and 24 registered catch-all functions. Existing production
import boundaries and original kernel budgets remain enforced.

This validates the supported public source-checking prefix. Full M0 semantics,
recursive-family recursors, records, default arms, erased-mode oracle
integration, and WebAssembly build/run remain pending. The native host pass
does not qualify a performance endpoint or complete R2.

## Review fixes

A review of this increment made seven changes. The run above used the
source after these changes.

- `make gates` again runs `dev/pin-check.py` and `dev/build.py --check`.
  The new `test-cli` target runs only `dev/test-cli.py`.
- The parser errors for `elim` name `'{'` in place of the old `'with'`.
- `dev/cli.py` builds all backends when the configured endpoint uses a
  different backend than the selected host, because `activate()` also
  requires the launcher of that endpoint.
- `dev/cli.py` gives a separate `E-HOST` message when a host writes to
  stderr and exits with status 0.
- Each refusal fixture in `examples/EXPECTATIONS.json` has a marker from
  its rule text, and `dev/test-cli.py` rejects an empty marker.
- `dev/test-cli.py` tests the native host by default.
- `dev/test-cli.py` requires the same `--print` output from each host.
