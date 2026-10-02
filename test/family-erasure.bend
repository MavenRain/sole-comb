# The test frontend admits recursive families before the public source grammar.
import Base
import ../lib/kernel_budget.bend as B
import ../lib/kernel_error.bend as E
import ../lib/kernel_rules.bend as R
import ../erase/core.bend as Erase
import ./pinfront/parser.bend as Parser
import ./pinfront/program.bend as Program
import ./program.bend as Transport

def erased(value: Program.checked) -> R.result(String):
  match value:
    case Program.Checked{globals, rows}:
      R.map_result(Erase.rows, String, Erase.print, Erase.program(globals, B.create(16384n), rows))
def text(result: R.result(String)) -> String:
  match result:
    case Done{out}: out
    case Fail{error}: String.append("FAIL ", E.to_string(error))
def report(source: String) -> String:
  text(R.bind(Program.checked, String, Transport.checked(Parser.parse(source)), erased))
def run(args: List<String>) -> IO(Unit):
  match args:
    case []: IO.print("FAIL missing erasure source")
    case source <> _: IO.print(report(source))
def main() -> IO(Unit): IO.bind(List<String>, Unit, IO.args, run)
