# Branch type plans through checked declarations, with an ambient-context sentinel.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_check.bend as C
import ../lib/kernel_error.bend as E
import ../lib/kernel_positivity.bend as P
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_recursor.bend as Rec
import ../lib/kernel_recursor_branch.bend as Branch
import ../lib/kernel_recursor_bridge.bend as Bridge
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
import ./recursor-motive.bend as MotiveCase

def binders(+c: C.ctx, bs: List<&2, Branch.binder>) -> List<&2, String>:
  match bs:
    case []: []
    case Branch.Binder{position, q, name, ty, _} <> rest:
      String.concat([F.Int63.show(position), ":", Layout.quantity(q), ":", name, ":", C.pp_value(c, ty)]) <> binders(c, rest)

def agree(+c: C.ctx, +ordinary: V.t, +ty: V.t) -> String:
  Q.if_else(String, String.eq(C.pp_value(c, ordinary), C.pp_value(c, ty)), _ => "same", _ => String.concat(["differs: ", C.pp_value(c, ordinary)]))

def report(pair: F.Pair2<C.ctx, V.t>, value: Branch.plan<C.ctx>) -> String:
  match pair:
    case F.Pair2{+inner, +ordinary}:
      match value:
        case Branch.Plan{motive, +c, bs, +ty}:
          match motive:
            case Motive.Plan{_, lvl, _}:
              String.concat(["context=", F.Int63.show(C.size(c)), " level=", F.Bignum.to_string(lvl), "\nbinders=", K.join(";", binders(c, bs)), "\ntarget=", C.pp_value(c, ty), "\nordinary=", agree(inner, ordinary, ty)])

def marks(tele: P.telescope) -> List<&2, F.Pair2<Q.t, String>>:
  match tele:
    case []: []
    case F.Pair2{q, F.Pair2{x, _}} <> rest: F.Pair2{q, x} <> marks(rest)

# The ordinary elimination target, built after the kernel's own branch field binding.
def elimination(+origin: C.ctx, +owner: String, +key: String, mo: T.motive, ct: P.ctor, state: R.binding_state<C.ctx>) -> R.result(F.Pair2<C.ctx, V.t>):
  match state:
    case R.BindingState{inner, env, vals}:
      R.bind(List<&2, V.t>, F.Pair2<C.ctx, V.t>, Bridge.rules(List<&2, V.t>, origin, R.mu_values(C.ctx, C.ops, origin, env, R.ctor_indices(ct))), +idx =>
        R.map_result(V.t, F.Pair2<C.ctx, V.t>, ty => F.Pair2{inner, ty}, Bridge.rules(V.t, origin, R.mu_result(C.ctx, C.ops, origin, mo, idx, V.VIn{S.SMu{owner, idx}, V.VACtor{key}, R.rev_append(V.t, vals, [])}))))

def ordinary(+origin: C.ctx, +owner: String, +key: String, mo: T.motive, +ct: P.ctor) -> R.result(F.Pair2<C.ctx, V.t>):
  R.bind(R.binding_state<C.ctx>, F.Pair2<C.ctx, V.t>, Bridge.rules(R.binding_state<C.ctx>, origin, R.mu_bind_fields(C.ctx, C.ops, origin, R.ctor_args(ct), marks(R.ctor_args(ct)), R.BindingState{origin, C.env(origin), []})), state => elimination(origin, owner, key, mo, ct, state))

# Certificate probes: a non-type target, or a non-type binder after the plan's binders.
def probe(+origin: C.ctx, +mode: String, value: Branch.plan<C.ctx>) -> R.result(String):
  match value:
    case Branch.Plan{_, _, +bs, ty}:
      R.map_result(C.ctx, String, _ => "the certificate accepted an ill-formed plan",
        Q.if_else(R.result(C.ctx), String.eq(mode, "ill-formed-target"),
          _ => Bridge.rules(C.ctx, origin, Branch.certify(C.ctx, C.ops, bs, origin, V.var(C.size(origin)))),
          _ => Bridge.rules(C.ctx, origin, Branch.certify(C.ctx, C.ops, List.append(&2, Branch.binder, bs, [Branch.Binder{F.Int63.zero, Q.Many{}, "bad", V.var(C.size(origin)), Branch.FieldBinder{}}]), origin, ty))))

def planned(+c: C.ctx, +owner: String, +key: String, +mode: String, +fam: P.family, +mo: T.motive) -> R.result(String):
  R.bind(Branch.plan<C.ctx>, String, Bridge.rules(Branch.plan<C.ctx>, c, Branch.layout(C.ctx, C.ops, c, Rec.Abstract{}, owner, key, mo)), +value =>
    R.bind(C.ctx, String, Bridge.rules(C.ctx, c, Rec.parameters(C.ctx, C.ops, R.family_params(fam), C.make(C.globals(c), C.budget(c)))), +origin =>
      Q.if_else(R.result(String), MotiveCase.known(mode, ["ill-formed-target", "ill-formed-binder"]),
        _ => probe(origin, mode, value),
        _ => R.bind(P.ctor, String, R.require(P.ctor, P.ctor_of(key, fam), E.Unbound{"the branch constructor is not declared"}), ct =>
          R.map_result(F.Pair2<C.ctx, V.t>, String, pair => report(pair, value), ordinary(origin, owner, key, mo, ct))))))

def checked(+owner: String, +key: String, +mode: String, value: Program.checked) -> R.result(String):
  match value:
    case Program.Checked{globals, _}:
      +c = C.make(globals, B.create(16384n))
      R.bind(P.family, String, Bridge.rules(P.family, c, R.mu_family(C.ctx, C.ops, c, owner)), +fam => planned(c, owner, key, mode, fam, MotiveCase.make(owner, mode, fam)))

def run(args: List<String>) -> IO(Unit):
  match args:
    case source <> owner <> key <> +mode <> []:
      IO.print(Q.if_else(String, MotiveCase.known(mode, ["constant", "parameter", "earlier-parameter", "parameter-self", "parameters-self", "index", "indices", "index-type", "self", "invalid", "prop", "wrong-family", "missing-family", "wrong-indices", "ill-formed-target", "ill-formed-binder"]), _ => Layout.result_text(R.bind(Program.checked, String, Transport.checked(Parser.parse(source)), value => checked(owner, key, mode, value))), _ => "FAIL\nexpected a known motive mode"))
    case _: IO.print("FAIL\nexpected source, family, constructor and motive")

def main() -> IO(Unit): IO.bind(List<String>, Unit, IO.args, run)
