# Compare layout plans for freshly checked families with pinned Kanon.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_check.bend as C
import ../lib/kernel_error.bend as E
import ../lib/kernel_global.bend as G
import ../lib/kernel_positivity.bend as P
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_rules.bend as R
import ../lib/kernel_value.bend as V
import ../erase/eterm.bend as K
import ../erase/family.bend as Family
import ../erase/type.bend as Ty
import ./pinfront/parser.bend as Parser
import ./pinfront/program.bend as Program
import ./program.bend as Transport

def field_texts(fields: List<&2, Maybe<&2, K.repr>>) -> List<&2, String>:
  match fields:
    case []: []
    case None{} <> rest: "drop" <> field_texts(rest)
    case Some{repr} <> rest: K.print_repr(repr) <> field_texts(rest)
def constructors(+c: C.ctx, +fam: P.family, ctors: List<&2, P.ctor>) -> R.result(List<&2, String>):
  match ctors:
    case []: Done{[]}
    case +ctor <> rest:
      R.bind(List<&2, Maybe<&2, K.repr>>, List<&2, String>, Family.layout(c, fam, ctor), +fields =>
        R.map_result(List<&2, String>, List<&2, String>, more => String.concat([P.ctor_name(ctor), " [", K.join(";", field_texts(fields)), "]\n"]) <> more,
          constructors(c, fam, rest)))
def planned(+c: C.ctx, +name: String, +fam: P.family) -> R.result(String):
  match fam:
    case P.Family{_, _, _, _, _, ctors, _}:
      R.bind(List<&2, K.tid>, String, Family.group_tids(c, name, fam), +tids =>
        R.map_result(List<&2, String>, String, lines => String.concat([name, " [", K.join(";", K.tid_texts(tids)), "]\n", String.concat(lines)]), constructors(c, fam, ctors)))
def families(+c: C.ctx, names: List<&2, String>) -> R.result(String):
  match names:
    case []: Done{""}
    case +name <> rest:
      Ty.maybe(P.family, R.result(String), G.find_family(name, C.globals(c)), _ => Fail{E.Unbound{name}}, fam =>
        R.bind(String, String, planned(c, name, fam), +here => R.map_result(String, String, more => String.append(here, more), families(c, rest))))
def checked(names: List<&2, String>, value: Program.checked) -> R.result(String):
  match value:
    case Program.Checked{globals, _}:
      families(C.bind("ambient", Q.Many{}, V.VNeutral{V.HGlobal{"Nat"}, []}, C.make(globals, B.create(16384n))), names)
def result_text(result: R.result(String)) -> String:
  match result:
    case Fail{error}: String.concat(["FAIL\n", E.to_string(error), "\n"])
    case Done{text}: text
def report(source: String, names: List<&2, String>) -> String:
  result_text(R.bind(Program.checked, String, Transport.checked(Parser.parse(source)), value => checked(names, value)))
def shared(names: List<String>) -> List<&2, String>:
  match names:
    case []: []
    case name <> rest: name <> shared(rest)
def run(args: List<String>) -> IO(Unit):
  match args:
    case []: IO.print("FAIL\nmissing layout source")
    case source <> names: IO.print(report(source, shared(names)))
def main() -> IO(Unit): IO.bind(List<String>, Unit, IO.args, run)
