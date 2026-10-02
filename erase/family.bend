# Nominal family layouts from Kanon 69f3be51 lib/erase.ml: mu_layout and mu_group_tids.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_check.bend as C
import ../lib/kernel_error.bend as E
import ../lib/kernel_eval.bend as EV
import ../lib/kernel_positivity.bend as P
import ../lib/kernel_rules.bend as R
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import ./eterm.bend as K
import ./type.bend as Ty

def kept(fields: List<&2, Maybe<&2, K.repr>>) -> List<&2, K.repr>:
  match fields:
    case []: []
    case None{} <> rest: kept(rest)
    case Some{repr} <> rest: repr <> kept(rest)
def leg_tid(name: String, index: F.Int63.t, +fields: List<&2, K.repr>) -> K.tid:
  K.Tid{String.concat(["leg<", K.tid_text(Ty.mu_tid(name)), ",", F.Int63.show(index),
    Ty.boolean(String, Ty.empty(K.repr, fields), _ => "", _ => String.append(",", K.join(",", K.repr_texts(fields)))), ">"])}
def parameters(tele: P.telescope, +c: C.ctx) -> R.result(C.ctx):
  match tele:
    case []: Done{c}
    case F.Pair2{q, F.Pair2{x, ty}} <> rest:
      R.bind(V.t, C.ctx, EV.eval(C.globals(c), C.env(c), ty), tyv => parameters(rest, C.bind(x, q, tyv, c)))
def field_repr(+c: C.ctx, +ty: V.t, keep: Bool) -> R.result(Maybe<&2, K.repr>):
  Ty.boolean(R.result(Maybe<&2, K.repr>), keep,
    _ => R.map_result(K.repr, Maybe<&2, K.repr>, repr => Some{repr}, Ty.repr_of(c, ty)), _ => Done{None{}})
def leg_reprs(tele: P.telescope, +c: C.ctx, +env: List<&2, V.t>) -> R.result(List<&2, Maybe<&2, K.repr>>):
  match tele:
    case []: Done{[]}
    case F.Pair2{+q, x_ty} <> rest:
      Ty.pair(String, T.t, R.result(List<&2, Maybe<&2, K.repr>>), x_ty, x => ty =>
        R.bind(V.t, List<&2, Maybe<&2, K.repr>>, EV.eval(C.globals(c), env, ty), +tyv =>
          R.bind(Bool, List<&2, Maybe<&2, K.repr>>, Ty.point_runtime(c, q, tyv), keep =>
            R.bind(Maybe<&2, K.repr>, List<&2, Maybe<&2, K.repr>>, field_repr(c, tyv, keep), +here =>
              R.map_result(List<&2, Maybe<&2, K.repr>>, List<&2, Maybe<&2, K.repr>>, more => here <> more,
                leg_reprs(rest, C.bind(x, q, tyv, c), V.var(C.size(c)) <> env))))))
def layout(+c: C.ctx, fam: P.family, ctor: P.ctor) -> R.result(List<&2, Maybe<&2, K.repr>>):
  match fam:
    case P.Family{_, params, _, _, _, _, _}:
      match ctor:
        case P.Ctor{_, fields, _, _, _}:
          R.bind(C.ctx, List<&2, Maybe<&2, K.repr>>, parameters(params, C.make(C.globals(c), C.budget(c))),
            +fresh => leg_reprs(fields, fresh, C.env(fresh)))
def group_loop(xs: List<&2, String>, +c: C.ctx, +name: String, +fam: P.family, +index: F.Int63.t) -> R.result(List<&2, K.tid>):
  match xs:
    case []: Done{[]}
    case +key <> rest:
      Ty.maybe(P.ctor, R.result(List<&2, K.tid>), P.ctor_of(key, fam),
        _ => Fail{E.Unbound{String.concat([key, " is not a constructor of ", name])}}, ctor =>
          R.bind(List<&2, Maybe<&2, K.repr>>, List<&2, K.tid>, layout(c, fam, ctor), +fields =>
            R.map_result(List<&2, K.tid>, List<&2, K.tid>, more => leg_tid(name, index, kept(fields)) <> more,
              group_loop(rest, c, name, fam, F.Int63.succ(index)))))
def group_tids(+c: C.ctx, +name: String, +fam: P.family) -> R.result(List<&2, K.tid>):
  R.bind(List<&2, String>, List<&2, K.tid>,
    C.result_of(List<&2, String>, C.drive(List<&2, String>, R.mu_ctor_names(C.ctx, name, fam), C.budget(c))),
    xs => group_loop(xs, c, name, fam, F.Int63.zero))
