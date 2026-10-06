# Branch body checks through checked declarations, with an ambient-context sentinel.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_check.bend as C
import ../lib/kernel_error.bend as E
import ../lib/kernel_positivity.bend as P
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_recursor.bend as Rec
import ../lib/kernel_recursor_body.bend as Body
import ../lib/kernel_rules.bend as R
import ../lib/kernel_shape.bend as S
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import ../erase/eterm.bend as K
import ./pinfront/parser.bend as Parser
import ./pinfront/program.bend as Program
import ./program.bend as Transport
import ./recursor-layout.bend as Layout
import ./recursor-motive.bend as MotiveCase

def var(n: Nat) -> T.t: T.Var{R.advance(F.Int63.zero, n)}

def ctor(family: String, indices: List<&2, T.t>, name: String, args: List<&2, T.t>) -> T.t: T.In{S.SMu{family, indices}, T.ACtor{name}, args}

def out(name: String, args: List<&2, T.t>) -> T.t: ctor("Out", [], name, args)

def zero() -> T.t: ctor("N", [], "zero", [])

def succ(n: T.t) -> T.t: ctor("N", [], "succ", [n])

# Index zero is the last leg binder. Hypotheses follow fields.
def bodies() -> List<&2, F.Pair2<String, T.t>>:
  [F.Pair2{"v0", var(0n)}, F.Pair2{"v1", var(1n)}, F.Pair2{"v2", var(2n)}, F.Pair2{"v3", var(3n)}, F.Pair2{"v4", var(4n)}, F.Pair2{"v5", var(5n)}, F.Pair2{"v6", var(6n)},
   F.Pair2{"stop", out("stop", [])}, F.Pair2{"both", out("both", [var(1n), var(0n)])}, F.Pair2{"hide", out("hide", [var(0n)])},
   F.Pair2{"one", out("one", [var(0n)])}, F.Pair2{"two", out("two", [var(0n), var(0n)])},
   F.Pair2{"at-zero", ctor("At", [zero], "at", [zero])}, F.Pair2{"next", ctor("At", [succ(var(1n))], "next", [var(1n), var(0n)])},
   F.Pair2{"tag-zero", ctor("Tag", [zero], "tag", [zero])}, F.Pair2{"grow", ctor("Tag", [succ(var(3n))], "grow", [var(3n), var(0n)])}]

def find(rows: List<&2, F.Pair2<String, T.t>>, +name: String) -> Maybe<&2, T.t>:
  match rows:
    case []: None{}
    case F.Pair2{key, tm} <> rest: Q.if_else(Maybe<&2, T.t>, String.eq(name, key), _ => Some{tm}, _ => find(rest, name))

def quantity(+text: String) -> Maybe<&2, Q.t>:
  Q.if_else(Maybe<&2, Q.t>, String.eq(text, "0"), _ => Some{Q.Zero{}}, _ =>
    Q.if_else(Maybe<&2, Q.t>, String.eq(text, "1"), _ => Some{Q.One{}}, _ =>
      Q.if_else(Maybe<&2, Q.t>, String.eq(text, "w"), _ => Some{Q.Many{}}, _ => None{})))

# Leg binders arrive as alternating quantity and name arguments.
def marks(args: List<String>, pending: Maybe<&2, Q.t>) -> R.result(List<&2, F.Pair2<Q.t, String>>):
  match args:
    case []:
      match pending:
        case None{}: Done{[]}
        case Some{_}: Fail{E.Mismatch{"a leg binder has a quantity and no name"}}
    case text <> rest:
      match pending:
        case None{}: R.bind(Q.t, List<&2, F.Pair2<Q.t, String>>, R.require(Q.t, quantity(text), E.Mismatch{"a leg binder quantity is 0, 1 or w"}), q => marks(rest, Some{q}))
        case Some{q}: R.map_result(List<&2, F.Pair2<Q.t, String>>, List<&2, F.Pair2<Q.t, String>>, tail => F.Pair2{q, text} <> tail, marks(rest, None{}))

def leg(body: String, rest: List<String>) -> R.result(T.leg):
  R.bind(T.t, T.leg, R.require(T.t, find(bodies, body), E.Mismatch{"expected a known body"}), tm =>
    R.map_result(List<&2, F.Pair2<Q.t, String>>, T.leg, bs => T.Leg{bs, tm}, marks(rest, None{})))

# The result motive is the constant family Out. Other modes come from the motive cases.
def motive(+owner: String, +mode: String, +fam: P.family) -> T.motive:
  Q.if_else(T.motive, String.eq(mode, "result"), _ => T.Motive{Some{owner}, MotiveCase.names(R.family_indices(fam)), "self", MotiveCase.indexed("Out", [])}, _ => MotiveCase.make(owner, mode, fam))

# Parameters with a runtime read after the close, innermost first.
def used(ls: List<&2, C.local>, +level: F.Int63.t, +uses: Q.usage) -> List<&2, String>:
  match ls:
    case []: []
    case C.Local{name, _, _} <> rest:
      List.append(&2, String, Q.if_else(List<&2, String>, Q.used(level, uses), _ => [name], _ => []), used(rest, F.Int63.pred(level), uses))

def report(+origin: C.ctx, uses: Q.usage) -> String:
  String.concat(["PASS origin=", F.Int63.show(C.size(origin)), " used=", K.join(";", used(C.locals(origin), F.Int63.pred(C.size(origin)), uses))])

def planned(+c: C.ctx, owner: String, key: String, grade: Q.t, fam: P.family, mo: T.motive, lg: T.leg) -> R.result(String):
  R.bind(Q.usage, String, Body.check(c, Rec.Abstract{}, grade, owner, key, mo, lg), uses =>
    R.map_result(C.ctx, String, origin => report(origin, uses), Rec.parameters(R.family_params(fam), C.make(C.globals(c), C.budget(c)))))

def checked(+owner: String, key: String, +mode: String, grade: Q.t, lg: T.leg, value: Program.checked) -> R.result(String):
  match value:
    case Program.Checked{globals, _}:
      +c = C.make(globals, B.create(16384n))
      R.bind(P.family, String, Rec.rules(P.family, c, R.mu_family(C.ctx, C.ops, c, owner)), +fam => planned(c, owner, key, grade, fam, motive(owner, mode, fam), lg))

def started(source: String, owner: String, key: String, mode: String, grade: String, body: String, rest: List<String>) -> R.result(String):
  R.bind(Q.t, String, R.require(Q.t, quantity(grade), E.Mismatch{"expected a checking mode 0, 1 or w"}), q =>
    R.bind(T.leg, String, leg(body, rest), lg =>
      R.bind(Program.checked, String, Transport.checked(Parser.parse(source)), value => checked(owner, key, mode, q, lg, value))))

def run(args: List<String>) -> IO(Unit):
  match args:
    case source <> owner <> key <> +mode <> grade <> body <> rest:
      IO.print(Q.if_else(String, MotiveCase.known(mode, ["constant", "parameter", "earlier-parameter", "parameter-self", "parameters-self", "index", "indices", "index-type", "self", "invalid", "prop", "wrong-family", "missing-family", "wrong-indices", "result"]), _ => Layout.result_text(started(source, owner, key, mode, grade, body, rest)), _ => "FAIL\nexpected a known motive mode"))
    case _: IO.print("FAIL\nexpected source, family, constructor, motive, mode, body and leg binders")

def main() -> IO(Unit): IO.bind(List<String>, Unit, IO.args, run)
