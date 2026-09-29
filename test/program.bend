# Test transport for the pinned check and check --print observations.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_error.bend as E
import ../lib/kernel_global.bend as G
import ../lib/kernel_rules.bend as R
import pinfront/syntax.bend as S
import pinfront/token.bend as Tok
import pinfront/lexer.bend as L
import pinfront/parser.bend as P
import pinfront/program.bend as Program

def nat(n: Nat) -> String: F.Bignum.to_string(F.Bignum.from_nat(n))
def observation(code: String, +stdout: String, stderr: String) -> String:
  S.cat([code, ":", nat(String.length(stdout)), ":", stdout, stderr])
def printed(print_form: Bool, rows: Program.entries) -> String:
  match print_form:
    case False{}: ""
    case True{}: Program.checked_form(rows)
def result_text(print_form: Bool, result: R.result(Program.checked)) -> String:
  match result:
    case Fail{error}: observation("1", "", String.append(E.to_string(error), "\n"))
    case Done{Program.Checked{_, rows}}: observation("0", printed(print_form, rows), "")
def checked(result: R.result(List<&2, S.decl>)) -> R.result(Program.checked):
  R.bind(List<&2, S.decl>, Program.checked, result, ds => Program.elab_program_in(B.create(16384n), G.initial, ds))
def parsed(print_form: Bool, result: R.result(List<&2, S.decl>)) -> String:
  result_text(print_form, checked(result))
def observe(print_form: Bool, source: String) -> String: parsed(print_form, P.parse(source))
def observe_bytes(print_form: Bool, bytes: List<&2, U32>) -> String:
  parsed(print_form, R.bind(List<&2, Tok.t>, List<&2, S.decl>, L.lex_bytes(bytes), ts => P.parse_tokens(ts)))
def pack(+check: String, printed: String) -> String: S.cat([nat(String.length(check)), ":", check, printed])
def error_both(+error: String) -> String: pack(observation("1", "", error), observation("1", "", error))
def result_both(result: R.result(Program.checked)) -> String:
  match result:
    case Fail{error}: error_both(String.append(E.to_string(error), "\n"))
    case Done{Program.Checked{_, rows}}: pack(observation("0", "", ""), observation("0", Program.checked_form(rows), ""))
def observe_both(source: String) -> String: result_both(checked(P.parse(source)))
def observe_both_bytes(bytes: List<&2, U32>) -> String:
  result_both(checked(R.bind(List<&2, Tok.t>, List<&2, S.decl>, L.lex_bytes(bytes), ts => P.parse_tokens(ts))))
