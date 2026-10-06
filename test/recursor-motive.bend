# Motive plans through checked declarations, with an ambient-context sentinel.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_check.bend as C
import ../lib/kernel_level.bend as L
import ../lib/kernel_positivity.bend as P
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_recursor.bend as Rec
import ../lib/kernel_recursor_motive.bend as Motive
import ../lib/kernel_rules.bend as R
import ../lib/kernel_shape.bend as S
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import ../erase/eterm.bend as K
import ./pinfront/parser.bend as Parser
import ./pinfront/program.bend as Program
import ./program.bend as Transport
import ./recursor-layout.bend as Layout

def names(tele: P.telescope) -> List<&2, String>:
  match tele:
    case []: []
    case F.Pair2{_, F.Pair2{x, _}} <> rest: x <> names(rest)

def indexed(name: String, args: List<&2, T.t>) -> T.t:
  T.Lan{S.SMu{name, args}, T.Sec{S.SColl{F.Int63.zero}, []}}

def self_parameterized(two: Bool) -> T.t:
  F.choose(T.t, two,
    T.Lan{S.SMu{"At", [T.Var{F.Int63.zero}]}, T.Sec{S.SColl{F.Int63.succ(F.Int63.one)}, [T.Leg{[], T.Var{F.Int63.succ(F.Int63.one)}}, T.Leg{[], T.Var{F.Int63.one}}]}},
    T.Lan{S.SMu{"At", [T.Var{F.Int63.zero}]}, T.Sec{S.SColl{F.Int63.one}, [T.Leg{[], T.Var{F.Int63.one}}]}})

def body(+mode: String, +count: F.Int63.t) -> T.t:
  F.choose(T.t, String.eq(mode, "parameter-self"), self_parameterized(False{}),
    F.choose(T.t, String.eq(mode, "parameters-self"), self_parameterized(True{}),
      F.choose(T.t, String.eq(mode, "index-type"), T.Var{F.Int63.succ(F.Int63.one)},
        F.choose(T.t, String.eq(mode, "parameter"), T.Var{F.Int63.succ(count)},
    F.choose(T.t, String.eq(mode, "earlier-parameter"), T.Var{F.Int63.succ(F.Int63.succ(count))},
      F.choose(T.t, String.eq(mode, "index"), indexed("Tag", [T.Var{F.Int63.one}]),
        F.choose(T.t, String.eq(mode, "indices"), indexed("Tag", [T.Var{F.Int63.succ(F.Int63.one)}, T.Var{F.Int63.one}]),
          F.choose(T.t, String.eq(mode, "self"), indexed("At", [T.Var{F.Int63.zero}]),
            F.choose(T.t, String.eq(mode, "invalid"), T.Var{F.Int63.zero},
              F.choose(T.t, String.eq(mode, "prop"), indexed("Truth", []), T.Global{"Nat"}))))))))))

def make(+owner: String, +mode: String, fam: P.family) -> T.motive:
  +idx = names(R.family_indices(fam))
  T.Motive{Q.if_else(Maybe<&2, String>, String.eq(mode, "missing-family"), _ => None{}, _ => Some{Q.if_else(String, String.eq(mode, "wrong-family"), _ => "Wrong", _ => owner)}), Q.if_else(List<&2, String>, String.eq(mode, "wrong-indices"), _ => [], _ => idx), "self", body(mode, R.length(String, idx))}

def hypotheses(+c: C.ctx, hs: List<&2, Motive.hypothesis>) -> List<&2, String>:
  match hs:
    case []: []
    case Motive.Hypothesis{position, name, ty} <> rest:
      String.concat([F.Int63.show(position), ":", name, ":", C.pp_value(c, ty)]) <> hypotheses(c, rest)

def report(value: Motive.plan) -> String:
  match value:
    case Motive.Plan{+raw, lvl, hs}:
      match raw:
        case Rec.Plan{c, _}: String.concat([Layout.report(raw), "\nlevel=", F.Bignum.to_string(lvl), "\ntypes=", K.join(";", hypotheses(c, hs))])

def checked(+owner: String, key: String, mode: String, value: Program.checked) -> R.result(String):
  match value:
    case Program.Checked{globals, _}:
      +c = C.make(globals, B.create(16384n))
      R.bind(P.family, String, Rec.rules(P.family, c, R.mu_family(C.ctx, C.ops, c, owner)), fam =>
        R.map_result(Motive.plan, String, report, Motive.layout(c, Rec.Abstract{}, owner, key, make(owner, mode, fam))))

def known(+mode: String, modes: List<&2, String>) -> Bool:
  match modes:
    case []: False{}
    case name <> rest: Bool.or(String.eq(mode, name), known(mode, rest))

def run(args: List<String>) -> IO(Unit):
  match args:
    case source <> owner <> key <> +mode <> []:
      IO.print(Q.if_else(String, known(mode, ["constant", "parameter", "earlier-parameter", "parameter-self", "parameters-self", "index", "indices", "index-type", "self", "invalid", "prop", "wrong-family", "missing-family", "wrong-indices"]), _ => Layout.result_text(R.bind(Program.checked, String, Transport.checked(Parser.parse(source)), value => checked(owner, key, mode, value))), _ => "FAIL\nexpected a known motive mode"))
    case _: IO.print("FAIL\nexpected source, family, constructor and motive")

def main() -> IO(Unit): IO.bind(List<String>, Unit, IO.args, run)
