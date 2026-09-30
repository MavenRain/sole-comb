# The public source checker uses production parsing and the real kernel.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_error.bend as E
import ../lib/kernel_global.bend as G
import ../lib/kernel_rules.bend as R
import syntax.bend as S
import parser.bend as P
import program.bend as Program

def count(rows: Program.entries) -> Nat:
  match rows:
    case []: 0n
    case _ <> rest: (1n + count(rest) : Nat)
def report(result: R.result(Program.checked)) -> String:
  match result:
    case Fail{error}: S.cat(["FAIL\n", E.to_string(error)])
    case Done{Program.Checked{_, +rows}}:
      S.cat(["OK\n", F.Bignum.to_string(F.Bignum.from_nat(count(rows))), "\n", Program.checked_form(rows)])
def source(bytes: List<&2, U32>) -> String:
  report(R.bind(List<&2, S.decl>, Program.checked, P.parse_bytes(bytes), ds => Program.elab_program_in(B.create(16384n), G.initial, ds)))
