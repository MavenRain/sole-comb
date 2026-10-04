# Exercise layout metadata through freshly checked declarations.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_check.bend as C
import ../lib/kernel_error.bend as E
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_recursor.bend as Rec
import ../lib/kernel_recursor_uniform.bend as Uniform
import ../lib/kernel_rules.bend as R
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import ../erase/eterm.bend as K
import ./pinfront/parser.bend as Parser
import ./pinfront/program.bend as Program
import ./program.bend as Transport

def quantity(q: Q.t) -> String:
  match q:
    case Q.Zero{}: "0"
    case Q.One{}: "1"
    case Q.Many{}: "w"
def values(+c: C.ctx, vs: List<&2, V.t>) -> List<&2, String>:
  match vs:
    case []: []
    case v <> rest: C.pp_value(c, v) <> values(c, rest)
def role(c: C.ctx, value: Rec.role) -> String:
  match value:
    case Rec.Ordinary{}: "ordinary"
    case Rec.Direct{indices}: String.concat(["recursive[", K.join(",", values(c, indices)), "]"])
def fields(+c: C.ctx, fs: List<&2, Rec.field>) -> List<&2, String>:
  match fs:
    case []: []
    case Rec.Field{position, q, name, ty, value, kind} <> rest:
      String.concat([F.Int63.show(position), ":", quantity(q), ":", name, ":", C.pp_value(c, ty), ":", C.pp_value(c, value), ":", role(c, kind)]) <> fields(c, rest)
def report(+value: Rec.plan) -> String:
  match value:
    case Rec.Plan{+c, +fs}: String.concat(["arity=", F.Int63.show(Rec.arity(value)), " context=", F.Int63.show(C.size(c)), "\nfields=", K.join(";", fields(c, fs)), "\nih=", K.join(";", fields(c, Rec.hypotheses(fs)))])
def selected(+c: C.ctx, +owner: String, +key: String, uniform: Bool) -> R.result(Rec.plan):
  Q.if_else(R.result(Rec.plan), String.eq(owner, "@alias-field"),
    _ => Rec.fields("N", ["N"], [F.Pair2{Q.Many{}, F.Pair2{"child", T.Global{"Alias"}}}], C.make(C.globals(c), C.budget(c)), F.Int63.zero),
    _ => Q.if_else(R.result(Rec.plan), uniform, _ => Uniform.layout(c, owner, key), _ => Rec.layout(c, owner, key)))
def checked(owner: String, key: String, +uniform: Bool, value: Program.checked) -> R.result(String):
  match value:
    case Program.Checked{globals, _}:
      R.map_result(Rec.plan, String, plan => report(plan), selected(C.bind("ambient", Q.Many{}, V.VNeutral{V.HGlobal{"Nat"}, []}, C.make(globals, B.create(16384n))), owner, key, uniform))
def result_text(value: R.result(String)) -> String:
  match value:
    case Fail{e}: String.append("FAIL\n", E.to_string(e))
    case Done{text}: text
def run(args: List<String>) -> IO(Unit):
  match args:
    case source <> owner <> key <> []:
      IO.print(result_text(R.bind(Program.checked, String, Transport.checked(Parser.parse(source)), value => checked(owner, key, False{}, value))))
    case source <> owner <> key <> mode <> []:
      IO.print(Q.if_else(String, String.eq(mode, "uniform"), _ => result_text(R.bind(Program.checked, String, Transport.checked(Parser.parse(source)), value => checked(owner, key, True{}, value))), _ => "FAIL\nexpected mode uniform"))
    case _: IO.print("FAIL\nexpected source, family and constructor")
def main() -> IO(Unit): IO.bind(List<String>, Unit, IO.args, run)
