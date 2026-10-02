# Recursive-family erasure plans from Kanon 69f3be51 lib/erase.ml:942-1134.
# Field values, the field arity and the constructor groups are checked eagerly, before any erasure.
# Kanon checks fields one at a time and computes groups last. The output is the same; only the
# error text can differ, and only on input with two faults.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_check.bend as C
import ../lib/kernel_error.bend as E
import ../lib/kernel_eval.bend as EV
import ../lib/kernel_positivity.bend as P
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_rules.bend as R
import ../lib/kernel_shape.bend as S
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import eterm.bend as K
import family.bend as Family
import pair.bend as Pair
import type.bend as Ty

type view is Data:
  View{name: String, indices: List<&2, V.t>, diagram: V.closure}
type tag is Data:
  Tag{tid: K.tid, index: F.Int63.t, fields: List<&2, Pair.field>, group: List<&2, K.tid>}
type case_plan is Data:
  Case{sty: V.t, name: String, family: P.family, motive: T.motive, env: List<&2, V.t>, names: List<&2, String>, group: List<&2, K.tid>}
type binder is Data:
  Binder{quantity: Q.t, name: String, domain: V.t, keep: Bool}
type binding is Data:
  Binding{check: C.ctx, env: List<&2, V.t>, values: List<&2, V.t>, arity: F.Int63.t, binders: List<&2, binder>}
type branch is Data:
  Branch{binders: List<&2, binder>, arity: F.Int63.t, target: V.t, body: T.t}

def rules(-A: Data, +c: C.ctx, p: R.program<C.ctx, A>) -> R.result(A):
  C.result_of(A, C.drive(A, p, C.budget(c)))
def former(+action: String, value: V.Former) -> R.result(view):
  match value:
    case V.Former{shape, +d, _}:
      match shape:
        case S.SMu{n, indices}: Done{View{n, indices, d}}
        case S.SPi{_, _, _}: Fail{E.Mismatch{String.append(action, " at a shape past M0")}}
        case S.SColl{_}: Fail{E.Mismatch{String.append(action, " at a shape past M0")}}
        case S.SPar{_, _}: Fail{E.Mismatch{String.append(action, " at a shape past M0")}}
        case S.SNu{_, _}: Fail{E.Mismatch{String.append(action, " at a shape past M0")}}
def of_type(+c: C.ctx, ty: V.t, +action: String, +stray: String) -> R.result(view):
  R.bind(V.t, view, EV.whnf(C.globals(c), ty), w =>
    R.bind(V.Former, view, Pair.require(V.Former, V.as_lan(w), stray), value => former(action, value)))
def position(+n: String, names: List<&2, String>, +index: F.Int63.t) -> Maybe<&2, F.Int63.t>:
  match names:
    case []: None{}
    case x <> rest: Ty.boolean(Maybe<&2, F.Int63.t>, String.eq(n, x),
      _ => Some{index}, _ => position(n, rest, F.Int63.succ(index)))
def ctor(+n: String, fam: P.family, +key: String) -> R.result(P.ctor):
  Ty.maybe(P.ctor, R.result(P.ctor), P.ctor_of(key, fam),
    _ => Fail{Family.not_ctor(key, n)}, c => Done{c})
def arity_word() -> String: "a branch binds a different number of fields than its constructor takes"
def fields(+c: C.ctx, tele: P.telescope, +env: List<&2, V.t>, args: List<&2, T.t>, layout: List<&2, Maybe<&2, K.repr>>) -> R.result(List<&2, Pair.field>):
  match tele:
    case []: Ty.boolean(R.result(List<&2, Pair.field>), Bool.and(Ty.empty(T.t, args), Ty.empty(Maybe<&2, K.repr>, layout)),
      _ => Done{[]}, _ => Fail{E.Mismatch{arity_word}})
    case F.Pair2{_, F.Pair2{_, ty}} <> rest:
      match args:
        case []: Fail{E.Mismatch{arity_word}}
        case +arg <> more:
          match layout:
            case []: Fail{E.Mismatch{arity_word}}
            case +field <> later:
              R.bind(V.t, List<&2, Pair.field>, EV.eval(C.globals(c), env, ty), +tyv =>
                R.bind(V.t, List<&2, Pair.field>, EV.eval(C.globals(c), C.env(c), arg), vv =>
                  R.map_result(List<&2, Pair.field>, List<&2, Pair.field>, +xs =>
                    Ty.maybe(K.repr, List<&2, Pair.field>, field, _ => xs, _ => Pair.Field{tyv, arg} <> xs),
                    fields(c, rest, vv <> env, more, later))))
def tag_fields(+c: C.ctx, +n: String, +fam: P.family, +index: F.Int63.t, args: List<&2, T.t>, env: List<&2, V.t>, layout: List<&2, Maybe<&2, K.repr>>, ct: P.ctor) -> R.result(tag):
  match ct:
    case P.Ctor{_, tele, _, _, _}:
      R.bind(List<&2, Pair.field>, tag, fields(c, tele, env, args, layout), +fs =>
        R.map_result(List<&2, K.tid>, tag, group => Tag{Ty.mu_tid(n), index, fs, group}, Family.group_tids(c, n, fam)))
def tag_ctor(+c: C.ctx, +n: String, +d: V.closure, +fam: P.family, +key: String, +args: List<&2, T.t>, names: List<&2, String>) -> R.result(tag):
  R.bind(F.Int63.t, tag, Ty.maybe(F.Int63.t, R.result(F.Int63.t), position(key, names, F.Int63.zero),
    _ => Fail{Family.not_ctor(key, n)}, k => Done{k}), +index =>
      R.bind(P.ctor, tag, ctor(n, fam, key), +ct =>
        R.bind(List<&2, V.t>, tag, rules(List<&2, V.t>, c, R.mu_param_env(C.ctx, C.ops, c, d)), env =>
          R.bind(List<&2, Maybe<&2, K.repr>>, tag, Family.layout(c, fam, ct), layout =>
            tag_fields(c, n, fam, index, args, env, layout, ct)))))
def injection_view(+c: C.ctx, address: T.addr, +args: List<&2, T.t>, value: view) -> R.result(tag):
  match value:
    case View{+n, _, +d}:
      R.bind(String, tag, Pair.require(String, T.as_actor(address), "a constructor takes a constructor address"), +key =>
        R.bind(P.family, tag, rules(P.family, c, R.mu_family(C.ctx, C.ops, c, n)), +fam =>
          R.bind(List<&2, String>, tag, rules(List<&2, String>, c, R.mu_ctor_names(C.ctx, n, fam)), names =>
            tag_ctor(c, n, d, fam, key, args, names))))
def injection(+c: C.ctx, ty: V.t, +address: T.addr, +args: List<&2, T.t>) -> R.result(tag):
  R.bind(view, tag, of_type(c, ty, "an introduction", "an introduction stands at a type that is not a left former"), value => injection_view(c, address, args, value))
def case_view(+c: C.ctx, +sty: V.t, +motive: Maybe<&2, T.motive>, value: view) -> R.result(case_plan):
  match value:
    case View{+n, indices, +d}:
      R.bind(P.family, case_plan, rules(P.family, c, R.mu_family(C.ctx, C.ops, c, n)), +fam =>
        R.bind(T.motive, case_plan, rules(T.motive, c, R.mu_motive_of(C.ctx, n, fam, indices, motive)), +mo =>
          R.bind(List<&2, String>, case_plan, rules(List<&2, String>, c, R.mu_ctor_names(C.ctx, n, fam)), +names =>
            R.bind(List<&2, V.t>, case_plan, rules(List<&2, V.t>, c, R.mu_param_env(C.ctx, C.ops, c, d)), +env =>
              R.map_result(List<&2, K.tid>, case_plan, group => Case{sty, n, fam, mo, env, names, group}, Family.group_tids(c, n, fam))))))
def case_type(+c: C.ctx, +sty: V.t, +motive: Maybe<&2, T.motive>) -> R.result(case_plan):
  R.bind(view, case_plan, of_type(c, sty, "an elimination", "an elimination stands on a value that is not a left former"), value => case_view(c, sty, motive, value))
def case_of(+c: C.ctx, scrut: T.t, +motive: Maybe<&2, T.motive>) -> R.result(case_plan):
  R.bind(V.t, case_plan, C.infer(c, Q.Many{}, scrut), sty => case_type(c, sty, motive))
def find_branch(+n: String, +key: String, branches: List<&2, F.Pair2<T.addr, T.leg>>) -> R.result(T.leg):
  match branches:
    case []: Fail{E.Missing_branch{String.concat(["the elimination of ", n, " has no branch at ", key])}}
    case F.Pair2{address, +leg} <> +rest:
      Ty.maybe(String, R.result(T.leg), T.as_actor(address), _ => find_branch(n, key, rest),
        x => Ty.boolean(R.result(T.leg), String.eq(key, x), _ => Done{leg}, _ => find_branch(n, key, rest)))
def bound_cons(+q: Q.t, +x: String, +dom: V.t, +keep: Bool, done: binding) -> binding:
  match done:
    case Binding{c1, env1, vals1, a1, bs1}: Binding{c1, env1, vals1, a1, Binder{q, x, dom, keep} <> bs1}
def binders(tele: P.telescope, names: List<&2, F.Pair2<Q.t, String>>, layout: List<&2, Maybe<&2, K.repr>>, value: binding) -> R.result(binding):
  match tele:
    case []: Ty.boolean(R.result(binding), Bool.and(Ty.empty(F.Pair2<Q.t, String>, names), Ty.empty(Maybe<&2, K.repr>, layout)),
      _ => Done{value}, _ => Fail{E.Missing_branch{arity_word}})
    case F.Pair2{+q, F.Pair2{_, ty}} <> rest:
      match names:
        case []: Fail{E.Missing_branch{arity_word}}
        case F.Pair2{_, +x} <> more:
          match layout:
            case []: Fail{E.Missing_branch{arity_word}}
            case field <> later:
              match value:
                case Binding{+c, +env, +vals, +arity, +bs}:
                  R.bind(V.t, binding, EV.eval(C.globals(c), env, ty), +dom =>
                    Ty.inspect(Bool, R.result(binding), Ty.maybe(K.repr, Bool, field, _ => False{}, _ => True{}), +keep =>
                      Ty.inspect(V.t, R.result(binding), V.var(C.size(c)), +v =>
                        R.map_result(binding, binding, done => bound_cons(q, x, dom, keep, done),
                          binders(rest, more, later, Binding{C.bind(x, q, dom, c), v <> env, v <> vals,
                            F.choose(F.Int63.t, keep, F.Int63.succ(arity), arity), bs})))))
def indices(+c: C.ctx, +env: List<&2, V.t>, terms: List<&2, T.t>) -> R.result(List<&2, V.t>):
  match terms:
    case []: Done{[]}
    case tm <> rest: R.bind(V.t, List<&2, V.t>, EV.eval(C.globals(c), env, tm), +v =>
      R.map_result(List<&2, V.t>, List<&2, V.t>, vs => v <> vs, indices(c, env, rest)))
def branch_target(+origin: C.ctx, +n: String, +key: String, +mo: T.motive, results: List<&2, T.t>, +body: T.t, value: binding) -> R.result(branch):
  match value:
    case Binding{c, env, +vals, +arity, +bs}:
      R.bind(List<&2, V.t>, branch, indices(c, env, results), +idx =>
        R.map_result(V.t, branch, target => Branch{bs, arity, target, body},
          rules(V.t, origin, R.mu_result(C.ctx, C.ops, origin, mo, idx,
            V.VIn{S.SMu{n, idx}, V.VACtor{key}, R.rev_append(V.t, vals, [])}))))
def branch_leg(+c: C.ctx, +n: String, +key: String, +mo: T.motive, +env: List<&2, V.t>, ct: P.ctor, leg: T.leg, layout: List<&2, Maybe<&2, K.repr>>) -> R.result(branch):
  match ct:
    case P.Ctor{_, tele, +results, _, _}:
      match leg:
        case T.Leg{bs, +body}:
          R.bind(binding, branch, binders(tele, bs, layout, Binding{c, env, [], F.Int63.zero, []}),
            value => branch_target(c, n, key, mo, results, body, value))
def branch_of(+c: C.ctx, +n: String, +fam: P.family, +mo: T.motive, +env: List<&2, V.t>, +key: String, +branches: List<&2, F.Pair2<T.addr, T.leg>>) -> R.result(branch):
  R.bind(P.ctor, branch, ctor(n, fam, key), +ct =>
    R.bind(T.leg, branch, find_branch(n, key, branches), +leg =>
      R.bind(List<&2, Maybe<&2, K.repr>>, branch, Family.layout(c, fam, ct),
        layout => branch_leg(c, n, key, mo, env, ct, leg, layout))))
