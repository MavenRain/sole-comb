# Parameterized constructors are checked against their expected family.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_check.bend as K
import ../lib/kernel_error.bend as E
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import ../lib/kernel_shape.bend as S
import ../lib/kernel_positivity.bend as P
import ../lib/kernel_eval.bend as Ev
import ../lib/kernel_rules.bend as R
import elab_program.bend as M
import syntax.bend as Syn

def walker() -> Type: K.ctx -> Maybe<&2, V.t> -> Syn.t -> M.result(T.t)
def choose(-A: Type, flag: Bool, yes: Unit -> A, no: Unit -> A) -> A:
  match flag:
    case True{}: yes(Unit{})
    case False{}: no(Unit{})
def reverse(xs: List<&2, V.t>, acc: List<&2, V.t>) -> List<&2, V.t>:
  match xs:
    case []: acc
    case x <> rest: reverse(rest, x <> acc)
def eval_in(+c: K.ctx, tm: T.t) -> M.result(V.t): M.lift(V.t, Ev.eval(K.globals(c), K.env(c), tm))
def values(+c: K.ctx, terms: List<&2, T.t>) -> M.result(List<&2, V.t>):
  match terms:
    case []: M.Pure{[]}
    case tm <> rest:
      M.bind(V.t, List<&2, V.t>, eval_in(c, tm), v =>
        M.map_result(List<&2, V.t>, List<&2, V.t>, vs => v <> vs, values(c, rest)))
# The family-name check shared by both constructor paths.
def same_family(+family: String, name: String, +n: String) -> M.result(Unit):
  choose(M.result(Unit), String.eq(n, family), _ => M.Pure{Unit{}},
    _ => M.Abort{E.Mismatch{Syn.cat(["the constructor ", name, " of ", family, " cannot have the expected family ", n])}})
def parameters(c: K.ctx, +family: String, name: String, ty: V.t, tm: T.t) -> M.result(F.Pair2<List<&2, T.t>, List<&2, T.t>>):
  match tm:
    case T.Lan{S.SMu{n, ix}, diagram}:
      M.bind(Unit, F.Pair2<List<&2, T.t>, List<&2, T.t>>, same_family(family, name, n), _ =>
        M.map_result(List<&2, T.t>, F.Pair2<List<&2, T.t>, List<&2, T.t>>, ps => F.Pair2{ps, ix},
          M.lift(List<&2, T.t>, R.run(K.ctx, K.dispatch, List<&2, T.t>, R.mu_params_of(K.ctx, diagram)))))
    case _: M.Abort{E.Mismatch{Syn.cat(["the constructor ", name, " of ", family, " needs the family ", family, " as its expected type, but the expected type is ", K.pp_value(c, ty)])}}
# Only a later field type reads a field value, so the last field is not evaluated.
def next_env(c: K.ctx, rest: P.telescope, tm: T.t, env: List<&2, V.t>) -> M.result(List<&2, V.t>):
  match rest:
    case []: M.Pure{env}
    case _ <> _: M.map_result(V.t, List<&2, V.t>, value => value <> env, eval_in(c, tm))
# Fields elaborate against the instantiated telescope. The declaration check
# validates the parameters, fields, indices and usage.
def arguments(~go: walker, fields: P.telescope, +c: K.ctx, +env: List<&2, V.t>, args: List<&2, Syn.t>) -> M.result(List<&2, T.t>):
  match fields:
    case []:
      match args:
        case []: M.Pure{[]}
        case _: M.Abort{E.Mismatch{"a constructor has excess fields"}}
    case F.Pair2{_, F.Pair2{_, ty}} <> +rest:
      match args:
        case []: M.Abort{E.Mismatch{"a constructor has missing fields"}}
        case arg <> tail:
          M.bind(V.t, List<&2, T.t>, M.lift(V.t, Ev.eval(K.globals(c), env, ty)), dom =>
            M.bind(T.t, List<&2, T.t>, go(c, Some{dom}, arg), +tm =>
              M.bind(List<&2, V.t>, List<&2, T.t>, next_env(c, rest, tm, env), next =>
                M.map_result(List<&2, T.t>, List<&2, T.t>, ts => tm <> ts, arguments(go, rest, c, next, tail)))))
def finish(~go: walker, +c: K.ctx, family: String, name: String, fields: P.telescope, args: List<&2, Syn.t>, pair: F.Pair2<List<&2, T.t>, List<&2, T.t>>) -> M.result(T.t):
  match pair:
    case F.Pair2{pt, ix}:
      M.bind(List<&2, V.t>, T.t, values(c, pt), vs =>
        M.map_result(List<&2, T.t>, T.t, ts => T.In{S.SMu{family, ix}, T.ACtor{name}, ts}, arguments(go, fields, c, reverse(vs, []), args)))
# Index terms read the field values, so a family without indices evaluates no field again.
def index_terms(ix: List<&2, T.t>, run: List<&2, T.t> -> M.result(List<&2, T.t>)) -> M.result(List<&2, T.t>):
  match ix:
    case []: M.Pure{[]}
    case i <> more: run(i <> more)
def whnf_target(+c: K.ctx, ty: V.t) -> M.result(T.t):
  M.bind(V.t, T.t, M.lift(V.t, Ev.whnf(K.globals(c), ty)), w => M.lift(T.t, Ev.quote(K.globals(c), K.size(c), w)))
# The caller checks the field count (elab.bend arity_is) before both paths.
def expected(~go: walker, +c: K.ctx, fam: P.family, ctor: P.ctor, args: List<&2, Syn.t>, +ty: V.t) -> M.result(T.t):
  match fam:
    case P.Family{+family, _, _, _, _, _, _}:
      match ctor:
        case P.Ctor{+name, fields, _, _, _}:
          M.bind(T.t, T.t, whnf_target(c, ty), tm => M.bind(F.Pair2<List<&2, T.t>, List<&2, T.t>>, T.t, parameters(c, family, name, ty, tm), ps => finish(go, c, family, name, fields, args, ps)))
def owner_shape(family: String, name: String, tm: T.t) -> M.result(Unit):
  match tm:
    case T.Lan{S.SMu{n, _}, _}: same_family(family, name, n)
    case _: M.Pure{Unit{}}
# A parameterless constructor with an expected family gets the same name check.
def owner(c: K.ctx, family: String, name: String, ty: V.t) -> M.result(Unit):
  M.bind(T.t, Unit, whnf_target(c, ty), tm => owner_shape(family, name, tm))
