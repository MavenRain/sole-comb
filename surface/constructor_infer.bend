# Infer constructor parameters from field types, then retain a checked annotation.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_check.bend as K
import ../lib/kernel_error.bend as E
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import ../lib/kernel_shape.bend as S
import ../lib/kernel_positivity.bend as P
import ../lib/kernel_eval.bend as Ev
import ../lib/kernel_conv.bend as Cv
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_rules.bend as R
import elab_program.bend as M
import syntax.bend as Syn
import constructor.bend as C

def slots() -> Data: List<&2, Maybe<&2, V.t>>
def optional(-A: Data, -B: Type, value: Maybe<&2, A>, no: Unit -> B, yes: A -> B) -> B:
  match value:
    case None{}: no(Unit{})
    case Some{v}: yes(v)
type state is Data:
  State{parameters: slots, values: List<&2, V.t>, terms: List<&2, T.t>}
def empty(params: P.telescope) -> slots:
  match params:
    case []: []
    case _ <> rest: None{} <> empty(rest)
def count(params: P.telescope) -> F.Int63.t:
  match params:
    case []: F.Int63.zero
    case _ <> rest: F.Int63.succ(count(rest))
def append(xs: List<&2, V.t>, ys: List<&2, V.t>) -> List<&2, V.t>:
  match xs:
    case []: ys
    case x <> rest: x <> append(rest, ys)
# A slot level is far above every context level. A slot never aliases a local and quotes to a negative index.
def slot_base() -> F.Int63.t: F.Int63.Int{False{}, [0, 0, 4]}
def environment(ps: slots, +base: F.Int63.t, +i: F.Int63.t, acc: List<&2, V.t>) -> List<&2, V.t>:
  match ps:
    case []: acc
    case p <> rest:
      match p:
        case None{}: environment(rest, base, F.Int63.succ(i), V.var(F.Int63.add(base, i)) <> acc)
        case Some{v}: environment(rest, base, F.Int63.succ(i), v <> acc)
def parameter(+base: F.Int63.t, +np: F.Int63.t, v: V.t) -> Maybe<&2, F.Int63.t>:
  match v:
    case V.VNeutral{V.HLocal{+level}, []}:
      C.choose(Maybe<&2, F.Int63.t>, Bool.and(F.Order.le(F.Int63.compare(base, level)), Bool.not(F.Order.le(F.Int63.compare(F.Int63.add(base, np), level)))),
        _ => Some{F.Int63.sub(level, base)}, _ => None{})
    case _: None{}
def field_site(name: String, k: String) -> String: Syn.cat(["field ", k, " of the constructor ", name])
def mismatch(+c: K.ctx, site: String, what: String, given: V.t, wanted: V.t) -> E.t:
  E.Mismatch{Syn.cat([site, " ", what, ": the argument gives ", K.pp_value(c, given), " where ", K.pp_value(c, wanted), " is required"])}
def equal(+c: K.ctx, +a: V.t, +b: V.t, ps: slots, site: String, what: String) -> M.result(slots):
  M.bind(Bool, slots, M.lift(Bool, Cv.conv_type(K.ctx, K.runtime_ops, c, a, b)), yes =>
    C.choose(M.result(slots), yes, _ => M.Pure{ps}, _ => M.Abort{mismatch(c, site, what, b, a)}))
def assign(+c: K.ctx, ps: slots, +i: F.Int63.t, +v: V.t, +site: String) -> M.result(slots):
  match ps:
    case []: M.Abort{E.Cannot_infer{"a constructor parameter has no slot"}}
    case +p <> +rest:
      C.choose(M.result(slots), F.Int63.equal(i, F.Int63.zero), _ =>
        optional(V.t, M.result(slots), p, _ => M.Pure{Some{v} <> rest}, +old => equal(c, old, v, Some{old} <> rest, site, "gives inconsistent parameters")),
        _ => M.map_result(slots, slots, tail => p <> tail, assign(c, rest, F.Int63.pred(i), v, site)))
# The parameter environment holds the last parameter first. The parameters are solved in declaration order.
def nominal(c: K.ctx, d: V.closure) -> M.result(List<&2, V.t>):
  M.map_result(List<&2, V.t>, List<&2, V.t>, xs => C.reverse(xs, []), M.lift(List<&2, V.t>, R.run(K.ctx, K.dispatch, List<&2, V.t>, R.mu_param_env(K.ctx, K.ops, c, d))))
def negative(tm: T.t) -> Bool:
  match tm:
    case T.Var{ix}: Bool.not(F.Order.le(F.Int63.compare(F.Int63.zero, ix)))
    case _: False{}
def pending(state: T.Scan) -> List<&2, T.t>:
  match state:
    case T.Scan{_, ts}: ts
# A slot level quotes to a negative index under every binder. The work list holds every subterm of one quoted type.
@unsafe
def open_term(terms: List<&2, T.t>) -> Bool:
  match terms:
    case []: False{}
    case +tm <> rest: C.choose(Bool, negative(tm), _ => True{}, _ => open_term(pending(T.scan_term(False{}, [], tm, rest))))
def has_hole(+c: K.ctx, v: V.t) -> M.result(Bool):
  M.map_result(T.t, Bool, tm => open_term([tm]), M.lift(T.t, Ev.quote(K.globals(c), K.size(c), v)))
def constraints() -> Data: List<&2, F.Pair2<V.t, V.t>>
def zip(site: String, xs: List<&2, V.t>, ys: List<&2, V.t>) -> M.result(constraints):
  match xs:
    case []:
      match ys:
        case []: M.Pure{[]}
        case _: M.Abort{E.Mismatch{String.append(site, ": constructor field types have different arities")}}
    case x <> rest:
      match ys:
        case []: M.Abort{E.Mismatch{String.append(site, ": constructor field types have different arities")}}
        case y <> tail:
          M.map_result(constraints, constraints, more => F.Pair2{x, y} <> more, zip(site, rest, tail))
def family_error(c: K.ctx, site: String, n: String, b: V.t) -> E.t:
  E.Mismatch{Syn.cat([site, " needs the family ", n, " and the argument has type ", K.pp_value(c, b)])}
def solve_family(+c: K.ctx, +n: String, ix: List<&2, V.t>, d: V.closure, +b: V.t, ps: slots, +site: String) -> M.result(F.Pair2<slots, constraints>):
  match b:
    case V.VLan{S.SMu{m, jx}, e, _}:
      C.choose(M.result(F.Pair2<slots, constraints>), String.eq(n, m), _ =>
        M.bind(List<&2, V.t>, F.Pair2<slots, constraints>, nominal(c, d), xs =>
          M.bind(List<&2, V.t>, F.Pair2<slots, constraints>, nominal(c, e), ys =>
            M.map_result(constraints, F.Pair2<slots, constraints>, pairs => F.Pair2{ps, pairs}, zip(site, append(xs, ix), append(ys, jx))))),
        _ => M.Abort{family_error(c, site, n, b)})
    case _: M.Abort{family_error(c, site, n, b)}
def solve_nominal(+c: K.ctx, +a: V.t, b: V.t, +ps: slots, site: String) -> M.result(F.Pair2<slots, constraints>):
  match a:
    case V.VLan{S.SMu{n, ix}, d, _}: solve_family(c, n, ix, d, b, ps, site)
    case _:
      M.bind(Bool, F.Pair2<slots, constraints>, has_hole(c, a), yes =>
        C.choose(M.result(F.Pair2<slots, constraints>), yes, _ => M.Pure{F.Pair2{ps, []}},
          _ => M.map_result(slots, F.Pair2<slots, constraints>, next => F.Pair2{next, []}, equal(c, a, b, ps, site, "does not match its field type"))))
def solve_step(+c: K.ctx, base: F.Int63.t, np: F.Int63.t, a: V.t, +b: V.t, +ps: slots, +site: String) -> M.result(F.Pair2<slots, constraints>):
  M.bind(V.t, F.Pair2<slots, constraints>, M.lift(V.t, Ev.whnf(K.globals(c), a)), +w =>
    optional(F.Int63.t, M.result(F.Pair2<slots, constraints>), parameter(base, np, w), _ =>
      M.bind(V.t, F.Pair2<slots, constraints>, M.lift(V.t, Ev.whnf(K.globals(c), b)), z => solve_nominal(c, w, z, ps, site)),
      i => M.map_result(slots, F.Pair2<slots, constraints>, next => F.Pair2{next, []}, assign(c, ps, i, b, site))))
def join(xs: constraints, ys: constraints) -> constraints:
  match xs:
    case []: ys
    case x <> rest: x <> join(rest, ys)
def with_constraint(-A: Type, pair: F.Pair2<V.t, V.t>, next: V.t -> V.t -> A) -> A:
  match pair:
    case F.Pair2{a, b}: next(a, b)
def with_solution(-A: Type, pair: F.Pair2<slots, constraints>, next: slots -> constraints -> A) -> A:
  match pair:
    case F.Pair2{ps, more}: next(ps, more)
@unsafe
def solve(+c: K.ctx, +base: F.Int63.t, +np: F.Int63.t, pairs: constraints, ps: slots, +site: String) -> M.result(slots):
  match pairs:
    case []: M.Pure{ps}
    case pair <> rest:
      with_constraint(M.result(slots), pair, a => b =>
        M.bind(F.Pair2<slots, constraints>, slots, solve_step(c, base, np, a, b, ps, site), result => with_solution(M.result(slots), result, next => more =>
          solve(c, base, np, join(more, rest), next, site))))
def located(name: String, k: String, +error: E.t) -> E.t:
  match error:
    case E.Cannot_infer{m}: E.Cannot_infer{Syn.cat(["the constructor ", name, " needs an expected type or an inferable field: field ", k, " has no inferable type (", m, ")"])}
    case _: error
# Each resumed program answers one request of the same argument elaboration.
@unsafe
def inferable(-A: Data, name: String, k: String, prog: M.program<A>) -> M.program<A>:
  match prog:
    case M.Pure{x}: M.Pure{x}
    case M.Abort{error}: M.Abort{located(name, k, error)}
    case M.Request{c, e, s, resume}: M.Request{c, e, s, answer => inferable(A, name, k, resume(answer))}
# A field type with a slot is not used as an expected type. The argument is inferred and its type is solved.
# A refusal of the elaboration or of the kernel type of the term names the constructor and the field.
def field(~go: C.walker, +c: K.ctx, +name: String, +k: String, base: F.Int63.t, np: F.Int63.t, +ps: slots, +dom: V.t, +arg: Syn.t) -> M.result(F.Pair2<slots, T.t>):
  M.bind(Bool, F.Pair2<slots, T.t>, has_hole(c, dom), yes =>
    C.choose(M.result(F.Pair2<slots, T.t>), yes, _ =>
      M.bind(T.t, F.Pair2<slots, T.t>, inferable(T.t, name, k, go(c, None{}, arg)), +tm =>
        M.bind(V.t, F.Pair2<slots, T.t>, inferable(V.t, name, k, M.lift(V.t, K.infer(c, Q.Zero{}, tm))), actual =>
          M.map_result(slots, F.Pair2<slots, T.t>, next => F.Pair2{next, tm}, solve(c, base, np, [F.Pair2{dom, actual}], ps, field_site(name, k))))),
      _ => M.map_result(T.t, F.Pair2<slots, T.t>, tm => F.Pair2{ps, tm}, go(c, Some{dom}, arg))))
def with_field(-A: Type, pair: F.Pair2<slots, T.t>, next: slots -> T.t -> A) -> A:
  match pair:
    case F.Pair2{ps, tm}: next(ps, tm)
def fields(~go: C.walker, +c: K.ctx, +name: String, +np: F.Int63.t, tele: P.telescope, +k: F.Int63.t, args: List<&2, Syn.t>, st: state) -> M.result(state):
  match tele:
    case []: M.Pure{st}
    case F.Pair2{_, F.Pair2{_, ty}} <> rest:
      match args:
        case []: M.Abort{E.Mismatch{"a constructor has missing fields"}}
        case arg <> tail:
          match st:
            case State{+ps, +vs, ts}:
              M.bind(V.t, state, M.lift(V.t, Ev.eval(K.globals(c), append(vs, environment(ps, slot_base, F.Int63.zero, [])), ty)), dom =>
                M.bind(F.Pair2<slots, T.t>, state, field(go, c, name, F.Int63.show(F.Int63.succ(k)), slot_base, np, ps, dom, arg), pair => with_field(M.result(state), pair, next => +tm =>
                  M.bind(V.t, state, C.eval_in(c, tm), v => fields(go, c, name, np, rest, F.Int63.succ(k), tail, State{next, v <> vs, tm <> ts})))))
def binder(tele: P.telescope) -> String:
  match tele:
    case []: "?"
    case F.Pair2{_, F.Pair2{x, _}} <> _: x
def after(tele: P.telescope) -> P.telescope:
  match tele:
    case []: []
    case _ <> rest: rest
def resolved(name: String, ps: slots, tele: P.telescope) -> M.result(List<&2, V.t>):
  match ps:
    case []: M.Pure{[]}
    case p <> rest:
      match p:
        case None{}: M.Abort{E.Cannot_infer{Syn.cat(["the constructor ", name, " needs an expected type or an inferable field for the parameter ", binder(tele)])}}
        case Some{v}: M.map_result(List<&2, V.t>, List<&2, V.t>, tail => v <> tail, resolved(name, rest, after(tele)))
def quote(+c: K.ctx, vs: List<&2, V.t>) -> M.result(List<&2, T.t>):
  match vs:
    case []: M.Pure{[]}
    case v <> rest:
      M.bind(T.t, List<&2, T.t>, M.lift(T.t, Ev.quote(K.globals(c), K.size(c), v)), tm =>
        M.map_result(List<&2, T.t>, List<&2, T.t>, tail => tm <> tail, quote(c, rest)))
def indices(+c: K.ctx, +env: List<&2, V.t>, ix: List<&2, T.t>) -> M.result(List<&2, V.t>):
  match ix:
    case []: M.Pure{[]}
    case tm <> rest:
      M.bind(V.t, List<&2, V.t>, M.lift(V.t, Ev.eval(K.globals(c), env, tm)), v =>
        M.map_result(List<&2, V.t>, List<&2, V.t>, tail => v <> tail, indices(c, env, rest)))
def legs(ts: List<&2, T.t>) -> List<&2, T.leg>:
  match ts:
    case []: []
    case tm <> rest: T.Leg{[], tm} <> legs(rest)
def terms(ts: List<&2, T.t>, acc: List<&2, T.t>) -> List<&2, T.t>:
  match ts:
    case []: acc
    case tm <> rest: terms(rest, tm <> acc)
def finish(+c: K.ctx, +family: String, +name: String, +np: F.Int63.t, params: P.telescope, ix: List<&2, T.t>, st: state) -> M.result(T.t):
  match st:
    case State{ps, vs, ts}:
      M.bind(List<&2, V.t>, T.t, resolved(name, ps, params), +pv =>
        M.bind(List<&2, T.t>, T.t, quote(c, pv), pt =>
          M.bind(List<&2, V.t>, T.t, indices(c, append(vs, C.reverse(pv, [])), ix), iv =>
            M.map_result(List<&2, T.t>, T.t, +it => T.Ann{T.In{S.SMu{family, it}, T.ACtor{name}, terms(ts, [])},
              T.Lan{S.SMu{family, it}, T.Sec{S.SColl{np}, legs(pt)}}}, quote(c, iv)))))
def infer(~go: C.walker, +c: K.ctx, fam: P.family, ctor: P.ctor, args: List<&2, Syn.t>) -> M.result(T.t):
  match fam:
    case P.Family{family, +params, _, _, _, _, _}:
      match ctor:
        case P.Ctor{+name, tele, ix, _, _}:
          M.bind(state, T.t, fields(go, c, name, count(params), tele, F.Int63.zero, args, State{empty(params), [], []}), st => finish(c, family, name, count(params), params, ix, st))
