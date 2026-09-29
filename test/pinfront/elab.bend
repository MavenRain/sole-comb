# Expression and declaration semantics from Kanon 69f3be51 surface/elab.ml.
# Recursive program groups belong to the later totality integration.
import Base
import ../../lib/foundation.bend as F
import ../../lib/kernel_budget.bend as B
import ../../lib/kernel_error.bend as E
import ../../lib/kernel_level.bend as L
import ../../lib/kernel_literal.bend as Lit
import ../../lib/kernel_quantity.bend as Q
import ../../lib/kernel_shape.bend as S
import ../../lib/kernel_term.bend as T
import ../../lib/kernel_value.bend as V
import ../../lib/kernel_global.bend as G
import ../../lib/kernel_positivity.bend as P
import ../../lib/kernel_rules.bend as R
import ../../lib/kernel_eval.bend as Ev
import ../../lib/kernel_check.bend as K
import syntax.bend as Syn
import elab_program.bend as M

def walker() -> Type: K.ctx -> Maybe<&2, V.t> -> Syn.t -> M.result(T.t)
def choose(-A: Type, flag: Bool, yes: Unit -> A, no: Unit -> A) -> A:
  match flag:
    case True{}: yes(Unit{})
    case False{}: no(Unit{})
def need(-A: Data, error: E.t, value: Maybe<&2, A>) -> M.result(A):
  match value:
    case None{}: M.Abort{error}
    case Some{x}: M.Pure{x}
def maybe(-A: Data, -D: Type, value: Maybe<&2, A>, no: Unit -> D, yes: A -> D) -> D:
  match value:
    case None{}: no(Unit{})
    case Some{x}: yes(x)
def using(-A: Data, -D: Type, value: A, next: A -> D) -> D: next(value)
def no_expect(what: String) -> E.t: E.Cannot_infer{Syn.cat([what, " needs an expected type"])}
def length(-A: Data, xs: List<&2, A>) -> F.Int63.t:
  match xs:
    case []: F.Int63.zero
    case _ <> rest: F.Int63.succ(length(A, rest))
def reverse_loop(-A: Data, xs: List<&2, A>, acc: List<&2, A>) -> List<&2, A>:
  match xs:
    case []: acc
    case x <> rest: reverse_loop(A, rest, x <> acc)
def append(-A: Data, xs: List<&2, A>, ys: List<&2, A>) -> List<&2, A>:
  match xs:
    case []: ys
    case x <> rest: x <> append(A, rest, ys)
def map(~A: Data, ~D: Data, ~f: A -> D, xs: List<&2, A>) -> List<&2, D>:
  match xs:
    case []: []
    case x <> rest: f(x) <> map(A, D, f, rest)
def collect(~A: Data, ~D: Data, ~Env: Data, ~f: Env -> A -> M.result(D), +environment: Env, xs: List<&2, A>) -> M.result(List<&2, D>):
  match xs:
    case []: M.Pure{[]}
    case x <> rest:
      M.bind(D, List<&2, D>, f(environment, x), y => M.map_result(List<&2, D>, List<&2, D>, ys => y <> ys, collect(A, D, Env, f, environment, rest)))
def index_next(value: Maybe<&2, F.Int63.t>) -> Maybe<&2, F.Int63.t>:
  match value:
    case None{}: None{}
    case Some{i}: Some{F.Int63.succ(i)}
def eval_in(+c: K.ctx, tm: T.t) -> M.result(V.t): M.lift(V.t, Ev.eval(K.globals(c), K.env(c), tm))
def whnf_in(c: K.ctx, value: V.t) -> M.result(V.t): M.lift(V.t, Ev.whnf(K.globals(c), value))
def quote_in(+c: K.ctx, value: V.t) -> M.result(T.t): M.lift(T.t, Ev.quote(K.globals(c), K.size(c), value))
def run_check(-A: Data, c: K.ctx, job: R.program<K.ctx, A>) -> M.result(A): M.lift(A, K.result_of(A, K.drive(A, job, K.budget(c))))
def run_eval(-A: Data, job: R.program<G.t, A>) -> M.result(A): M.lift(A, R.run(G.t, Ev.dispatch, A, job))
def leg_of(tm: T.t) -> T.leg: T.Leg{[], tm}
type mu_ref is Data:
  RFam{family: P.family}
  RCtor{family: P.family, ctor: P.ctor}
def spine_of(s: Syn.t, args: List<&2, Syn.t>) -> F.Pair2<Syn.t, List<&2, Syn.t>>:
  match s:
    case Syn.SApp{f, a}: spine_of(f, a <> args)
    case other: F.Pair2{other, args}
def split_cons(-A: Data, x: A, value: F.Pair2<List<&2, A>, List<&2, A>>) -> F.Pair2<List<&2, A>, List<&2, A>>:
  match value:
    case F.Pair2{before, after}: F.Pair2{x <> before, after}
type bound is Data:
  Bound{quantity: Q.t, name: String, ty: T.t, context: K.ctx}
def coll_motive_plain(m: Syn.motive) -> M.result(Unit):
  match m:
    case Syn.Motive{_, ind, _, _}:
      match ind:
        case None{}: M.Pure{Unit{}}
        case Some{n}: M.Abort{E.Mismatch{String.append("a case at a collection takes no index clause and its motive names ", n)}}
type bound_values is Data:
  Values{context: K.ctx, env: List<&2, V.t>, values: List<&2, V.t>}
def family_indices(fam: P.family) -> P.telescope:
  match fam:
    case P.Family{_, _, indices, _, _, _, _}: indices
def field_binder(field: Syn.field) -> F.Pair2<Q.t, String>:
  match field:
    case Syn.Field{q, name, _}: F.Pair2{q, name}
def joined(value: Maybe<&2, Maybe<&2, V.t>>) -> Maybe<&2, V.t>:
  match value:
    case None{}: None{}
    case Some{x}: x
type telescope_result is Data:
  Telescope{entries: P.telescope, context: K.ctx}
type chain_result is Data:
  Chain{entries: P.telescope, result: Syn.t, context: K.ctx}
def universe_of(tm: T.t) -> M.result(L.t):
  match tm:
    case T.Univ{level}: M.Pure{level}
    case _: M.Abort{E.Universe{"the header of a family ends in a universe"}}
def with_former(-A: Type, value: V.Former, next: S.t<V.t> -> V.closure -> Maybe<&2, L.t> -> A) -> A:
  match value:
    case V.Former{s, clo, u}: next(s, clo, u)
def with_point(-A: Type, value: R.Point, next: Q.t -> String -> V.t -> A) -> A:
  match value:
    case R.Point{q, x, dom}: next(q, x, dom)
def with_motive(-A: Type, value: Syn.motive, next: String -> Maybe<&2, String> -> List<&2, String> -> Syn.t -> A) -> A:
  match value:
    case Syn.Motive{self, ind, idx, body}: next(self, ind, idx, body)
def with_field(-A: Type, value: Syn.field, next: Q.t -> String -> Maybe<&2, Syn.t> -> A) -> A:
  match value:
    case Syn.Field{q, x, ty}: next(q, x, ty)
def with_binder(-A: Type, value: Syn.binder, next: Q.t -> String -> Syn.t -> A) -> A:
  match value:
    case Syn.Binder{q, x, ty}: next(q, x, ty)
def with_ctor(-A: Type, value: P.ctor, next: String -> P.telescope -> List<&2, T.t> -> F.Int63.t -> Bool -> A) -> A:
  match value:
    case P.Ctor{name, args, idx, arity, recursive}: next(name, args, idx, arity, recursive)
def with_term_pair(-A: Type, value: F.Pair2<List<&2, T.t>, List<&2, T.t>>, next: List<&2, T.t> -> List<&2, T.t> -> A) -> A:
  match value:
    case F.Pair2{xs, ys}: next(xs, ys)
def fam_name(fam: Syn.fam) -> String:
  match fam:
    case Syn.Fam{name, _, _, _}: name
def eval_environment(environment: F.Pair2<G.t, List<&2, V.t>>, tm: T.t) -> M.result(V.t):
  match environment:
    case F.Pair2{globals, env}: M.lift(V.t, Ev.eval(globals, env, tm))
type collection_environment is Data:
  CollectionEnv{context: K.ctx, shape: S.t<V.t>, legs: List<&2, V.vleg>, motive: Maybe<&2, T.motive>, expected: Maybe<&2, V.t>}
type family_environment is Data:
  FamilyEnv{context: K.ctx, name: String, family: P.family, motive: Maybe<&2, T.motive>, params: List<&2, V.t>, expected: Maybe<&2, V.t>}
def request(c: K.ctx, expected: Maybe<&2, V.t>, s: Syn.t) -> M.result(T.t): M.request(c, expected, s)
def reverse(-A: Data, xs: List<&2, A>) -> List<&2, A>: reverse_loop(A, xs, [])
def index_of(+x: String, names: List<&2, String>) -> Maybe<&2, F.Int63.t>:
  match names:
    case []: None{}
    case y <> rest:
      choose(Maybe<&2, F.Int63.t>, String.eq(x, y), _ => Some{F.Int63.zero}, _ => index_next(index_of(x, rest)))
def type_of(+c: K.ctx, tm: T.t) -> M.result(V.t): M.bind(V.t, V.t, M.lift(V.t, K.infer(c, Q.Zero{}, tm)), value => whnf_in(c, value))
def open_in(c: K.ctx, clo: V.closure, args: List<&2, V.t>) -> M.result(V.t): run_eval(V.t, R.open_closure(G.t, Ev.ev(K.globals(c)), clo, args))
def univ_of_int(+n: F.Int63.t) -> M.result(T.t):
  M.map_result(L.t, T.t, l => T.Univ{l}, need(L.t, E.Universe{Syn.cat(["the universe level ", F.Int63.show(n), " is not a level"])}, L.of_int(n)))
def sum_ty(+items: List<&2, T.t>) -> T.t: T.Lan{S.SColl{length(T.t, items)}, T.Sec{S.SColl{length(T.t, items)}, map(T.t, T.leg, leg_of, items)}}
def prod_ty(+items: List<&2, T.t>) -> T.t: T.Ran{S.SColl{length(T.t, items)}, T.Sec{S.SColl{length(T.t, items)}, map(T.t, T.leg, leg_of, items)}}
# Positive signed counts decrease to zero without wrapping.
@unsafe
def none_list(+n: F.Int63.t) -> List<&2, Maybe<&2, V.t>>:
  choose(List<&2, Maybe<&2, V.t>>, F.Order.le(F.Int63.compare(n, F.Int63.zero)),
    _ => [], _ => None{} <> none_list(F.Int63.pred(n)))
def expectations_loop(+c: K.ctx, +legs: List<&2, V.vleg>, items: List<&2, Maybe<&2, V.t>>, +i: F.Int63.t) -> M.result(List<&2, Maybe<&2, V.t>>):
  match items:
    case []: M.Pure{[]}
    case _ <> rest:
      M.bind(V.t, List<&2, Maybe<&2, V.t>>, run_check(V.t, c, R.coll_leg_ty(K.ctx, K.ops, c, legs, i)), ty =>
        M.map_result(List<&2, Maybe<&2, V.t>>, List<&2, Maybe<&2, V.t>>, tail => Some{ty} <> tail, expectations_loop(c, legs, rest, F.Int63.succ(i))))
def find_ctor(+x: String, families: F.Map.t<P.family>) -> Maybe<&2, mu_ref>:
  match families:
    case F.Map.Empty{}: None{}
    case F.Map.Bind{_, +fam, rest}:
      maybe(P.ctor, Maybe<&2, mu_ref>, P.ctor_of(x, fam), _ => find_ctor(x, rest), ct => Some{RCtor{fam, ct}})
def split_at(-A: Data, xs: List<&2, A>, +n: F.Int63.t) -> F.Pair2<List<&2, A>, List<&2, A>>:
  match xs:
    case []: F.Pair2{[], []}
    case +x <> +rest:
      choose(F.Pair2<List<&2, A>, List<&2, A>>, F.Order.le(F.Int63.compare(n, F.Int63.zero)),
        _ => F.Pair2{[], x <> rest}, _ => split_cons(A, x, split_at(A, rest, F.Int63.pred(n))))
def arity_is(what: String, +want: F.Int63.t, +got: F.Int63.t) -> M.result(Unit):
  choose(M.result(Unit), F.Int63.equal(want, got), _ => M.Pure{Unit{}}, _ => M.Abort{E.Mismatch{Syn.cat([what, " takes ", F.Int63.show(want), " arguments and the term gives ", F.Int63.show(got)])}})
def fam_split(name: String, np: F.Int63.t, terms: F.Pair2<List<&2, T.t>, List<&2, T.t>>) -> M.result(T.t):
  match terms:
    case F.Pair2{params, indices}: M.Pure{T.Lan{S.SMu{name, indices}, T.Sec{S.SColl{np}, map(T.t, T.leg, leg_of, params)}}}
def leg_projection(scrut: T.t, s: S.t<V.t>, k: F.Int63.t) -> M.result(T.t):
  M.map_result(F.Int63.t, T.t, n => T.Out{S.SColl{n}, T.ALeg{k}, scrut}, need(F.Int63.t, E.Mismatch{"a leg projection needs a collection former"}, R.collection_view(s)))
def elab_binder(~go: walker, +c: K.ctx, b: Syn.binder) -> M.result(bound):
  match b:
    case Syn.Binder{+q, +x, ty}:
      M.bind(T.t, bound, go(c, None{}, ty), +tm => M.map_result(V.t, bound, v => Bound{q, x, tm, K.bind(x, q, v, c)}, eval_in(c, tm)))
def zip(~A: Data, ~D: Data, xs: List<&2, A>, ys: List<&2, D>) -> Maybe<&2, List<&2, F.Pair2<A, D>>>:
  match xs:
    case []:
      match ys:
        case []: Some{[]}
        case _ <> _: None{}
    case x <> rest:
      match ys:
        case []: None{}
        case y <> tail:
          maybe(List<&2, F.Pair2<A, D>>, Maybe<&2, List<&2, F.Pair2<A, D>>>, zip(A, D, rest, tail), _ => None{}, pairs => Some{F.Pair2{x, y} <> pairs})
def elab_motive(~go: walker, c: K.ctx, mo: Maybe<&2, Syn.motive>, ty: V.t) -> M.result(Maybe<&2, T.motive>):
  match mo:
    case None{}: M.Pure{None{}}
    case Some{+m}:
      M.bind(Unit, Maybe<&2, T.motive>, coll_motive_plain(m), _ =>
        using(Syn.motive, M.result(Maybe<&2, T.motive>), m, motive => with_motive(M.result(Maybe<&2, T.motive>), motive, +self => _ => _ => body =>
            M.map_result(T.t, Maybe<&2, T.motive>, tm => Some{T.Motive{None{}, [], self, tm}}, go(K.bind(self, Q.Zero{}, ty, c), None{}, body)))))
def branch_target(+c: K.ctx, mo: Maybe<&2, T.motive>, +expected: Maybe<&2, V.t>, self: V.t) -> M.result(Maybe<&2, V.t>):
  match mo:
    case None{}: M.Pure{expected}
    case Some{m}: M.map_result(V.t, Maybe<&2, V.t>, v => Some{v}, run_check(V.t, c, R.elim_result(K.ctx, K.ops, c, Some{m}, expected, self)))
def mu_branch_target(+c: K.ctx, mo: Maybe<&2, T.motive>, expected: Maybe<&2, V.t>, indices: List<&2, V.t>, self: V.t) -> M.result(Maybe<&2, V.t>):
  match mo:
    case None{}: M.Pure{expected}
    case Some{m}: M.map_result(V.t, Maybe<&2, V.t>, v => Some{v}, run_check(V.t, c, R.mu_result(K.ctx, K.ops, c, m, indices, self)))
def bind_indices(pairs: List<&2, F.Pair2<F.Pair2<Q.t, F.Pair2<String, T.t>>, String>>, state: bound_values) -> M.result(bound_values):
  match pairs:
    case []: M.Pure{state}
    case F.Pair2{F.Pair2{_, F.Pair2{_, ty}}, +x} <> rest:
      match state:
        case Values{+c, +env, vals}:
          M.bind(V.t, bound_values, M.lift(V.t, Ev.eval(K.globals(c), env, ty)), tv =>
            using(V.t, M.result(bound_values), V.var(K.size(c)), +v =>
              bind_indices(rest, Values{K.bind(x, Q.Zero{}, tv, c), v <> env, v <> vals})))
def check_field_annotation(~go: walker, +c: K.ctx, expected: V.t, field: Syn.field) -> M.result(Unit):
  match field:
    case Syn.Field{_, name, annotation}:
      match annotation:
        case None{}: M.Pure{Unit{}}
        case Some{ty}:
          M.bind(T.t, Unit, go(c, None{}, ty), +tm =>
            M.bind(L.t, Unit, M.lift(L.t, K.infer_univ(c, tm)), _ =>
              M.bind(V.t, Unit, eval_in(c, tm), actual =>
                M.bind(Bool, Unit, M.lift(Bool, K.conv_type(c, actual, expected)), agrees =>
                  choose(M.result(Unit), agrees, _ => M.Pure{Unit{}}, _ => M.Abort{E.Mismatch{Syn.cat(["the annotation of constructor field ", name, " differs from its declared type"])}})))))
def item_elab(~go: walker, c: K.ctx, pair: F.Pair2<Syn.t, Maybe<&2, V.t>>) -> M.result(T.t):
  match pair:
    case F.Pair2{item, expected}: go(c, expected, item)
def with_bound(-A: Type, value: bound, next: Q.t -> String -> T.t -> K.ctx -> A) -> A:
  match value:
    case Bound{q, x, ty, c}: next(q, x, ty, c)
def with_values(-A: Type, value: bound_values, next: K.ctx -> List<&2, V.t> -> List<&2, V.t> -> A) -> A:
  match value:
    case Values{c, env, vals}: next(c, env, vals)
def with_telescope(-A: Type, value: telescope_result, next: P.telescope -> K.ctx -> A) -> A:
  match value:
    case Telescope{entries, c}: next(entries, c)
def with_chain(-A: Type, value: chain_result, next: P.telescope -> Syn.t -> K.ctx -> A) -> A:
  match value:
    case Chain{entries, res, c}: next(entries, res, c)
def mu_result(+name: String, tm: T.t) -> M.result(F.Pair2<List<&2, T.t>, List<&2, T.t>>):
  match tm:
    case T.Lan{S.SMu{n, ix}, diagram}:
      choose(M.result(F.Pair2<List<&2, T.t>, List<&2, T.t>>), String.eq(n, name), _ =>
        M.map_result(List<&2, T.t>, F.Pair2<List<&2, T.t>, List<&2, T.t>>, params => F.Pair2{params, ix}, M.lift(List<&2, T.t>, R.run(K.ctx, K.dispatch, List<&2, T.t>, R.mu_params_of(K.ctx, diagram)))),
        _ => M.Abort{E.Mismatch{Syn.cat(["a constructor of ", name, " ends at another type"])}})
    case _: M.Abort{E.Mismatch{Syn.cat(["a constructor of ", name, " ends at another type"])}}
def elab_univ(c: K.ctx, s: Syn.t) -> M.result(L.t):
  M.bind(T.t, L.t, request(c, None{}, s), universe_of)
def elab_decl_program(+c: K.ctx, d: Syn.decl) -> M.result(K.decl):
  match d:
    case Syn.DDef{name, ty, body}:
      M.bind(T.t, K.decl, request(c, None{}, ty), +tyt =>
        M.bind(V.t, K.decl, eval_in(c, tyt), tyv =>
          M.map_result(T.t, K.decl, tm => K.Declaration{name, K.Definition{}, tyt, Some{tm}}, request(c, Some{tyv}, body))))
    case Syn.DAxiom{name, ty}: M.map_result(T.t, K.decl, tm => K.Declaration{name, K.Postulate{}, tm, None{}}, request(c, None{}, ty))
    case Syn.DMu{_}: M.Abort{E.Mismatch{"a mu group declares no entry of its own"}}
    case Syn.DRec{_}: M.Abort{E.Mismatch{"a recursive group is elaborated as a group"}}
def former_expectations(+c: K.ctx, +n: F.Int63.t, value: Maybe<&2, V.Former>) -> M.result(List<&2, Maybe<&2, V.t>>):
  match value:
    case None{}: M.Pure{none_list(n)}
    case Some{V.Former{s, clo, _}}:
      maybe(F.Int63.t, M.result(List<&2, Maybe<&2, V.t>>), R.collection_view(s), _ => M.Pure{none_list(n)}, dn =>
          choose(M.result(List<&2, Maybe<&2, V.t>>), F.Int63.equal(dn, n), _ =>
            M.bind(List<&2, V.vleg>, List<&2, Maybe<&2, V.t>>, run_check(List<&2, V.vleg>, c, R.coll_legs_of(K.ctx, K.ops, c, clo)), legs => expectations_loop(c, legs, none_list(n), F.Int63.zero)),
            _ => M.Pure{none_list(n)}))
def family_ctor(x: String, g: G.t) -> Maybe<&2, mu_ref>:
  match g:
    case G.Globals{_, families}: find_ctor(x, families)
def local_or_global(c: K.ctx, +x: String) -> T.t:
  maybe(F.Int63.t, T.t, index_of(x, K.local_names(K.locals(c))), _ => T.Global{x}, i => T.Var{i})
def fam_term(name: String, +np: F.Int63.t, terms: List<&2, T.t>) -> M.result(T.t):
  fam_split(name, np, split_at(T.t, terms, np))
def elab_ctor_ref(~go: walker, +c: K.ctx, fam: P.family, ctor: P.ctor, +args: List<&2, Syn.t>) -> M.result(T.t):
  match fam:
    case P.Family{fname, params, _, _, _, _, _}:
      match ctor:
        case P.Ctor{+name, fields, indices, _, _}:
          M.bind(Unit, T.t, arity_is(name, length(F.Pair2<Q.t, F.Pair2<String, T.t>>, fields), length(Syn.t, args)), _ =>
            choose(M.result(T.t), F.Int63.equal(length(F.Pair2<Q.t, F.Pair2<String, T.t>>, params), F.Int63.zero), _ =>
                M.bind(List<&2, T.t>, T.t, collect(Syn.t, T.t, K.ctx, ctx => s => go(ctx, None{}, s), c, args), +terms =>
                  M.bind(List<&2, V.t>, T.t, collect(T.t, V.t, K.ctx, ctx => tm => eval_in(ctx, tm), c, terms), +values =>
                    M.bind(List<&2, V.t>, T.t, collect(T.t, V.t, F.Pair2<G.t, List<&2, V.t>>, eval_environment, F.Pair2{K.globals(c), reverse(V.t, values)}, indices), ixv =>
                      M.map_result(List<&2, T.t>, T.t, ix => T.In{S.SMu{fname, ix}, T.ACtor{name}, terms}, collect(V.t, T.t, K.ctx, ctx => v => quote_in(ctx, v), c, ixv))))),
              _ => M.Abort{no_expect(String.append("the constructor ", name))}))
def elab_pair(~go: walker, +c: K.ctx, expected: Maybe<&2, V.t>, a: Syn.t, b: Syn.t) -> M.result(T.t):
  M.bind(V.t, T.t, need(V.t, no_expect("a pair"), expected), ty =>
    M.bind(V.t, T.t, whnf_in(c, ty), w =>
      M.bind(V.Former, T.t, need(V.Former, E.Mismatch{"a pair needs a left former as its expected type"}, V.as_lan(w)), former => with_former(M.result(T.t), former, s => clo => _ =>
            M.bind(R.Point, T.t, need(R.Point, E.Mismatch{"a pair needs a point former"}, R.point_view(s)), point => with_point(M.result(T.t), point, +q => x => +dom =>
                  M.bind(T.t, T.t, go(c, Some{dom}, a), +pt =>
                    M.bind(V.t, T.t, eval_in(c, pt), pv =>
                      M.bind(V.t, T.t, open_in(c, clo, [pv]), cod =>
                        M.bind(T.t, T.t, go(c, Some{cod}, b), fibre =>
                          M.map_result(T.t, T.t, dt => T.In{S.SPi{q, x, dt}, T.APt{q, pt}, [fibre]}, quote_in(c, dom))))))))))))
def pair_projection(+c: K.ctx, scrut: T.t, s: S.t<V.t>, clo: V.closure, +k: F.Int63.t) -> M.result(T.t):
  M.bind(R.Point, T.t, need(R.Point, E.Mismatch{"a pair projection needs a point former"}, R.point_view(s)), point => with_point(M.result(T.t), point, +q => +x => +dom =>
        using(Bool, M.result(T.t), F.Int63.equal(k, F.Int63.succ(F.Int63.one)), +second =>
          choose(M.result(T.t), Bool.or(F.Int63.equal(k, F.Int63.one), second), _ =>
          M.bind(V.t, T.t, choose(M.result(V.t), second, _ =>
            M.bind(V.t, V.t, run_eval(V.t, R.elim_value(G.t, Ev.ev(K.globals(c)), S.SPi{q, x, dom}, q, None{}, R.proj_branch(q, False{}), K.env(c), V.var(K.size(c)))), pt => open_in(c, clo, [pt])),
            _ => M.Pure{dom}), body =>
              M.bind(T.t, T.t, M.lift(T.t, Ev.quote(K.globals(c), F.Int63.succ(K.size(c)), body)), motive =>
                M.map_result(T.t, T.t, dt => T.Elim{T.Elimination{S.SPi{q, x, dt}, scrut, Q.One{}, Some{T.Motive{None{}, [], "self", motive}}, R.proj_branch(q, second)}}, quote_in(c, dom)))),
          _ => M.Abort{E.Wrong_leg{"a pair carries the projections .1 and .2 only"}}))))
def app_point(~go: walker, +c: K.ctx, f: Syn.t, a: Syn.t) -> M.result(T.t):
  M.bind(T.t, T.t, go(c, None{}, f), +head =>
    M.bind(V.t, T.t, type_of(c, head), w =>
      M.bind(V.Former, T.t, need(V.Former, E.Mismatch{"the head of an application is not a function"}, V.as_ran(w)), former => with_former(M.result(T.t), former, s => _ => _ =>
            M.bind(R.Point, T.t, need(R.Point, E.Mismatch{"the head of an application is not a function"}, R.point_view(s)), point => with_point(M.result(T.t), point, +q => x => +dom =>
                  M.bind(T.t, T.t, go(c, Some{dom}, a), arg =>
                    M.map_result(T.t, T.t, dt => T.Out{S.SPi{q, x, dt}, T.APt{q, arg}, head}, quote_in(c, dom)))))))))
def cod_former(+c: K.ctx, former: Maybe<&2, V.Former>) -> M.result(Maybe<&2, V.t>):
  match former:
    case None{}: M.Pure{None{}}
    case Some{V.Former{s, clo, _}}:
      maybe(R.Point, M.result(Maybe<&2, V.t>), R.point_view(s), _ => M.Pure{None{}}, _ =>
        M.map_result(V.t, Maybe<&2, V.t>, v => Some{v}, open_in(c, clo, [V.var(K.size(c))])))
def elab_group(~go: walker, c: K.ctx, b: Syn.binder, cod: Syn.t, left: Bool) -> M.result(T.t):
  M.bind(bound, T.t, elab_binder(go, c, b), bound => with_bound(M.result(T.t), bound, +q => +x => +ty => inner =>
        M.map_result(T.t, T.t, +tm => F.choose(T.t, left, T.Lan{S.SPi{q, x, ty}, tm}, T.Ran{S.SPi{q, x, ty}, tm}), go(inner, None{}, cod))))
def elab_branch(~go: walker, +c: K.ctx, s: S.t<V.t>, legs: List<&2, V.vleg>, mo: Maybe<&2, T.motive>, expected: Maybe<&2, V.t>, br: Syn.branch) -> M.result(F.Pair2<T.addr, T.leg>):
  match br:
    case Syn.BrCtor{name, _, _}: M.Abort{E.Wrong_leg{Syn.cat(["the branch at ", name, " keys a constructor and a case at a collection keys a leg"])}}
    case Syn.BrLeg{+k, binders, body}:
      M.bind(Syn.binder, F.Pair2<T.addr, T.leg>, need(Syn.binder, E.Missing_branch{"each branch binds its payload once"}, R.one_of(Syn.binder, binders)), b => with_binder(M.result(F.Pair2<T.addr, T.leg>), b, +q => +x => _ =>
            M.bind(V.t, F.Pair2<T.addr, T.leg>, run_check(V.t, c, R.coll_leg_ty(K.ctx, K.ops, c, legs, k)), ty =>
              M.bind(Maybe<&2, V.t>, F.Pair2<T.addr, T.leg>, branch_target(c, mo, expected, V.VIn{s, V.VALeg{k}, [V.var(K.size(c))]}), target =>
                M.map_result(T.t, F.Pair2<T.addr, T.leg>, tm => F.Pair2{T.ALeg{k}, T.Leg{[F.Pair2{q, x}], tm}}, go(K.bind(x, q, ty, c), target, body))))))
def mu_motive_indices(~go: walker, c: K.ctx, +n: String, +indices: P.telescope, clo: V.closure, u: Maybe<&2, L.t>, penv: List<&2, V.t>, mo: Maybe<&2, Syn.motive>) -> M.result(Maybe<&2, T.motive>):
  match mo:
    case None{}: M.Pure{None{}}
    case Some{Syn.Motive{+self, ind, +idx, body}}:
          M.bind(List<&2, F.Pair2<F.Pair2<Q.t, F.Pair2<String, T.t>>, String>>, Maybe<&2, T.motive>,
            need(List<&2, F.Pair2<F.Pair2<Q.t, F.Pair2<String, T.t>>, String>>, E.Mismatch{Syn.cat(["the motive of ", n, " binds ", F.Int63.show(length(String, idx)), " index names and the family has ", F.Int63.show(length(F.Pair2<Q.t, F.Pair2<String, T.t>>, indices))])}, zip(F.Pair2<Q.t, F.Pair2<String, T.t>>, String, indices, idx)), pairs =>
              M.bind(bound_values, Maybe<&2, T.motive>, bind_indices(pairs, Values{c, penv, []}), bound => with_values(M.result(Maybe<&2, T.motive>), bound, ci => _ => vals =>
                    M.map_result(T.t, Maybe<&2, T.motive>, tm => Some{T.Motive{ind, idx, self, tm}},
                      go(K.bind(self, Q.Zero{}, V.VLan{S.SMu{n, reverse(V.t, vals)}, clo, u}, ci), None{}, body)))))
def bind_fields(~go: walker, pairs: List<&2, F.Pair2<F.Pair2<Q.t, F.Pair2<String, T.t>>, Syn.field>>, state: bound_values) -> M.result(bound_values):
  match pairs:
    case []: M.Pure{state}
    case F.Pair2{F.Pair2{_, F.Pair2{_, ty}}, +field} <> rest:
      match state:
        case Values{+c, +env, vals}:
          M.bind(V.t, bound_values, M.lift(V.t, Ev.eval(K.globals(c), env, ty)), +tv =>
            M.bind(Unit, bound_values, check_field_annotation(go, c, tv, field), _ =>
              using(Syn.field, M.result(bound_values), field, fd => with_field(M.result(bound_values), fd, q => name => _ =>
                  using(V.t, M.result(bound_values), V.var(K.size(c)), +v =>
                    bind_fields(go, rest, Values{K.bind(name, q, tv, c), v <> env, v <> vals}))))))
def elab_telescope(bs: List<&2, Syn.binder>, c: K.ctx) -> M.result(telescope_result):
  match bs:
    case []: M.Pure{Telescope{[], c}}
    case b <> rest:
      M.bind(bound, telescope_result, elab_binder(request, c, b), bound => with_bound(M.result(telescope_result), bound, q => x => ty => ci =>
            M.map_result(telescope_result, telescope_result, tail => with_telescope(telescope_result, tail, entries => final =>
                                                Telescope{F.Pair2{q, F.Pair2{x, ty}} <> entries, final}), elab_telescope(rest, ci))))
def elab_chain(s: Syn.t, c: K.ctx) -> M.result(chain_result):
  match s:
    case Syn.SArrow{b, cod}:
      M.bind(bound, chain_result, elab_binder(request, c, b), bound => with_bound(M.result(chain_result), bound, q => x => ty => ci =>
            M.map_result(chain_result, chain_result, tail => with_chain(chain_result, tail, entries => res => final =>
                                                 Chain{F.Pair2{q, F.Pair2{x, ty}} <> entries, res, final}), elab_chain(cod, ci))))
    case other: M.Pure{Chain{[], other, c}}
def leg_expectations(+c: K.ctx, expected: Maybe<&2, V.t>, n: F.Int63.t, left: Bool) -> M.result(List<&2, Maybe<&2, V.t>>):
  match expected:
    case None{}: M.Pure{none_list(n)}
    case Some{ty}:
      M.bind(V.t, List<&2, Maybe<&2, V.t>>, whnf_in(c, ty), +w => former_expectations(c, n, F.choose(Maybe<&2, V.Former>, left, V.as_lan(w), V.as_ran(w))))
def mu_ref_of(+c: K.ctx, +x: String) -> Maybe<&2, mu_ref>:
  maybe(F.Int63.t, Maybe<&2, mu_ref>, index_of(x, K.local_names(K.locals(c))), _ =>
    maybe(P.family, Maybe<&2, mu_ref>, G.find_family(x, K.globals(c)), _ => family_ctor(x, K.globals(c)), fam => Some{RFam{fam}}), _ => None{})
def elab_fam_ref(~go: walker, +c: K.ctx, f: P.family, +args: List<&2, Syn.t>) -> M.result(T.t):
  match f:
    case P.Family{+name, params, indices, _, _, _, _}:
      using(F.Int63.t, M.result(T.t), length(F.Pair2<Q.t, F.Pair2<String, T.t>>, params), +np =>
        M.bind(Unit, T.t, arity_is(name, F.Int63.add(np, length(F.Pair2<Q.t, F.Pair2<String, T.t>>, indices)), length(Syn.t, args)), _ =>
          M.bind(List<&2, T.t>, T.t, collect(Syn.t, T.t, K.ctx, ctx => s => go(ctx, None{}, s), c, args), terms => fam_term(name, np, terms))))
def project_type(c: K.ctx, +scrut: T.t, +w: V.t, +k: F.Int63.t) -> M.result(T.t):
  maybe(V.Former, M.result(T.t), V.as_lan(w), _ =>
    maybe(V.Former, M.result(T.t), V.as_ran(w), _ => M.Abort{E.Mismatch{"a projection reads a pair or a tuple, and this scrutinee is neither"}}, former => with_former(M.result(T.t), former, s => _ => _ =>
                                leg_projection(scrut, s, k))), former => with_former(M.result(T.t), former, s => clo => _ =>
                                pair_projection(c, scrut, s, clo, k)))
def cod_of(+c: K.ctx, expected: Maybe<&2, V.t>) -> M.result(Maybe<&2, V.t>):
  match expected:
    case None{}: M.Pure{None{}}
    case Some{ty}: M.bind(V.t, Maybe<&2, V.t>, whnf_in(c, ty), w => cod_former(c, V.as_ran(w)))
def elab_mu_motive(~go: walker, c: K.ctx, n: String, fam: P.family, clo: V.closure, u: Maybe<&2, L.t>, penv: List<&2, V.t>, mo: Maybe<&2, Syn.motive>) -> M.result(Maybe<&2, T.motive>):
  mu_motive_indices(go, c, n, family_indices(fam), clo, u, penv, mo)
def elab_mu_branch(~go: walker, +c: K.ctx, +n: String, fam: P.family, mo: Maybe<&2, T.motive>, penv: List<&2, V.t>, expected: Maybe<&2, V.t>, br: Syn.branch) -> M.result(F.Pair2<T.addr, T.leg>):
  match br:
    case Syn.BrLeg{k, _, _}: M.Abort{E.Wrong_leg{Syn.cat(["the branch ", F.Int63.show(k), " keys a leg and a case at ", n, " keys a constructor"])}}
    case Syn.BrCtor{+name, +fields, body}:
      M.bind(P.ctor, F.Pair2<T.addr, T.leg>, need(P.ctor, E.Unbound{Syn.cat([name, " is not a constructor of ", n])}, P.ctor_of(name, fam)), ct => with_ctor(M.result(F.Pair2<T.addr, T.leg>), ct, _ => +args => indices => _ => _ =>
            M.bind(List<&2, F.Pair2<F.Pair2<Q.t, F.Pair2<String, T.t>>, Syn.field>>, F.Pair2<T.addr, T.leg>,
              need(List<&2, F.Pair2<F.Pair2<Q.t, F.Pair2<String, T.t>>, Syn.field>>, E.Missing_branch{Syn.cat(["the branch at ", name, " binds ", F.Int63.show(length(Syn.field, fields)), " fields and ", name, " takes ", F.Int63.show(length(F.Pair2<Q.t, F.Pair2<String, T.t>>, args))])}, zip(F.Pair2<Q.t, F.Pair2<String, T.t>>, Syn.field, args, fields)), pairs =>
                M.bind(bound_values, F.Pair2<T.addr, T.leg>, bind_fields(go, pairs, Values{c, penv, []}), bound => with_values(M.result(F.Pair2<T.addr, T.leg>), bound, cf => +env => vals =>
                      M.bind(List<&2, V.t>, F.Pair2<T.addr, T.leg>, collect(T.t, V.t, F.Pair2<G.t, List<&2, V.t>>, eval_environment, F.Pair2{K.globals(c), env}, indices), +idx =>
                        M.bind(Maybe<&2, V.t>, F.Pair2<T.addr, T.leg>, mu_branch_target(c, mo, expected, idx, V.VIn{S.SMu{n, idx}, V.VACtor{name}, reverse(V.t, vals)}), target =>
                          M.map_result(T.t, F.Pair2<T.addr, T.leg>, tm => F.Pair2{T.ACtor{name}, T.Leg{map(Syn.field, F.Pair2<Q.t, String>, field_binder, fields), tm}}, go(cf, target, body)))))))))
def collection_branch(~go: walker, environment: collection_environment, br: Syn.branch) -> M.result(F.Pair2<T.addr, T.leg>):
  match environment:
    case CollectionEnv{c, s, legs, mo, expected}: elab_branch(go, c, s, legs, mo, expected, br)
def elab_fam_decl(c: K.ctx, fam: Syn.fam) -> M.result(K.family_decl):
  match fam:
    case Syn.Fam{name, params, ty, _}:
      M.bind(telescope_result, K.family_decl, elab_telescope(params, c), tele => with_telescope(M.result(K.family_decl), tele, ps => cp =>
            M.bind(chain_result, K.family_decl, elab_chain(ty, cp), chain => with_chain(M.result(K.family_decl), chain, indices => res => ci =>
                  M.map_result(L.t, K.family_decl, level => K.FamilyDecl{name, ps, indices, level}, elab_univ(ci, res))))))
def elab_ctor_decl(c: K.ctx, family: String, ctor: Syn.fam_ctor) -> M.result(K.ctor_decl):
  match ctor:
    case Syn.FamCtor{name, ty}:
      M.bind(chain_result, K.ctor_decl, elab_chain(ty, c), chain => with_chain(M.result(K.ctor_decl), chain, args => res => cr =>
            M.bind(T.t, K.ctor_decl, request(cr, None{}, res), tm =>
              M.map_result(F.Pair2<List<&2, T.t>, List<&2, T.t>>, K.ctor_decl, result => with_term_pair(K.ctor_decl, result, params => ix =>
                                            K.CtorDecl{name, args, params, ix}), mu_result(family, tm)))))
def elab_coll_case(~go: walker, +c: K.ctx, +expected: Maybe<&2, V.t>, scrut: T.t, w: V.t, +s: S.t<V.t>, clo: V.closure, mo: Maybe<&2, Syn.motive>, brs: List<&2, Syn.branch>) -> M.result(T.t):
  M.bind(F.Int63.t, T.t, need(F.Int63.t, E.Mismatch{"a case eliminates a collection"}, R.collection_view(s)), n =>
    M.bind(List<&2, V.vleg>, T.t, run_check(List<&2, V.vleg>, c, R.coll_legs_of(K.ctx, K.ops, c, clo)), +legs =>
      M.bind(Maybe<&2, T.motive>, T.t, elab_motive(go, c, mo, w), +motive =>
        M.map_result(List<&2, F.Pair2<T.addr, T.leg>>, T.t, branches => T.Elim{T.Elimination{S.SColl{n}, scrut, Q.One{}, motive, branches}},
          collect(Syn.branch, F.Pair2<T.addr, T.leg>, collection_environment, env => br => collection_branch(go, env, br), CollectionEnv{c, s, legs, motive, expected}, brs)))))
def elab_mu_ref(~go: walker, c: K.ctx, r: mu_ref, args: List<&2, Syn.t>) -> M.result(T.t):
  match r:
    case RFam{f}: elab_fam_ref(go, c, f, args)
    case RCtor{f, ct}: elab_ctor_ref(go, c, f, ct, args)
def elab_proj(~go: walker, +c: K.ctx, a: Syn.t, k: F.Int63.t) -> M.result(T.t):
  M.bind(T.t, T.t, go(c, None{}, a), +scrut => M.bind(V.t, T.t, type_of(c, scrut), w => project_type(c, scrut, w, k)))
def app_head(c: K.ctx, head: Syn.t) -> Maybe<&2, mu_ref>:
  match head:
    case Syn.SVar{x}: mu_ref_of(c, x)
    case _: None{}
def elab_fun(~go: walker, bs: List<&2, Syn.binder>, +c: K.ctx, expected: Maybe<&2, V.t>, body: Syn.t) -> M.result(T.t):
  match bs:
    case []: go(c, expected, body)
    case b <> rest:
      M.bind(bound, T.t, elab_binder(go, c, b), bound => with_bound(M.result(T.t), bound, +q => +x => ty => inner =>
            M.bind(Maybe<&2, V.t>, T.t, cod_of(c, expected), cod =>
              M.map_result(T.t, T.t, tm => T.Sec{S.SPi{q, x, ty}, [T.Leg{[F.Pair2{q, x}], tm}]}, elab_fun(go, rest, inner, cod, body)))))
def elab_items(~go: walker, +c: K.ctx, expected: Maybe<&2, V.t>, +items: List<&2, Syn.t>) -> M.result(List<&2, T.t>):
  M.bind(List<&2, Maybe<&2, V.t>>, List<&2, T.t>, leg_expectations(c, expected, length(Syn.t, items), False{}), tys =>
    M.bind(List<&2, F.Pair2<Syn.t, Maybe<&2, V.t>>>, List<&2, T.t>, need(List<&2, F.Pair2<Syn.t, Maybe<&2, V.t>>>, E.Mismatch{"the item count and the leg count differ"}, zip(Syn.t, Maybe<&2, V.t>, items, tys)), pairs =>
      collect(F.Pair2<Syn.t, Maybe<&2, V.t>>, T.t, K.ctx, ctx => pair => item_elab(go, ctx, pair), c, pairs)))
def family_branch(~go: walker, environment: family_environment, br: Syn.branch) -> M.result(F.Pair2<T.addr, T.leg>):
  match environment:
    case FamilyEnv{c, n, fam, mo, params, expected}: elab_mu_branch(go, c, n, fam, mo, params, expected, br)
def declare_families(fams: List<&2, Syn.fam>, +budget: B.t, +globals: G.t) -> M.result(G.t):
  match fams:
    case []: M.Pure{globals}
    case fm <> rest:
      M.bind(K.family_decl, G.t, elab_fam_decl(K.make(globals, budget), fm), d =>
        M.bind(G.t, G.t, M.lift(G.t, K.declare_family(globals, budget, d)), g => declare_families(rest, budget, g)))
def ctor_environment(environment: F.Pair2<K.ctx, String>, ct: Syn.fam_ctor) -> M.result(K.ctor_decl):
  match environment:
    case F.Pair2{c, name}: elab_ctor_decl(c, name, ct)
def elab_mu_case(~go: walker, +c: K.ctx, +expected: Maybe<&2, V.t>, scrut: T.t, +n: String, ixv: List<&2, V.t>, +clo: V.closure, u: Maybe<&2, L.t>, mo: Maybe<&2, Syn.motive>, brs: List<&2, Syn.branch>) -> M.result(T.t):
  M.bind(P.family, T.t, need(P.family, E.Unbound{Syn.cat(["the family ", n, " is not declared"])}, G.find_family(n, K.globals(c))), +fam =>
    M.bind(List<&2, V.t>, T.t, run_check(List<&2, V.t>, c, R.mu_param_env(K.ctx, K.ops, c, clo)), +penv =>
      M.bind(Maybe<&2, T.motive>, T.t, elab_mu_motive(go, c, n, fam, clo, u, penv, mo), +motive =>
        M.bind(List<&2, F.Pair2<T.addr, T.leg>>, T.t, collect(Syn.branch, F.Pair2<T.addr, T.leg>, family_environment, env => br => family_branch(go, env, br), FamilyEnv{c, n, fam, motive, penv, expected}, brs), branches =>
          M.map_result(List<&2, T.t>, T.t, ix => T.Elim{T.Elimination{S.SMu{n, ix}, scrut, Q.One{}, motive, branches}}, collect(V.t, T.t, K.ctx, ctx => v => quote_in(ctx, v), c, ixv))))))
def app_spine(~go: walker, +c: K.ctx, f: Syn.t, a: Syn.t, spine: F.Pair2<Syn.t, List<&2, Syn.t>>) -> M.result(T.t):
  match spine:
    case F.Pair2{head, args}:
      maybe(mu_ref, M.result(T.t), app_head(c, head), _ => app_point(go, c, f, a), r => elab_mu_ref(go, c, r, args))
def define_families(fams: List<&2, Syn.fam>, +budget: B.t, +group: List<&2, String>, +globals: G.t) -> M.result(G.t):
  match fams:
    case []: M.Pure{globals}
    case Syn.Fam{+name, params, _, ctors} <> rest:
      M.bind(telescope_result, G.t, elab_telescope(params, K.make(globals, budget)), tele => with_telescope(M.result(G.t), tele, _ => +cp =>
            M.bind(List<&2, K.ctor_decl>, G.t, collect(Syn.fam_ctor, K.ctor_decl, F.Pair2<K.ctx, String>, ctor_environment, F.Pair2{cp, name}, ctors), cds =>
              M.bind(G.t, G.t, M.lift(G.t, K.define_ctors(globals, budget, group, name, cds)), g => define_families(rest, budget, group, g)))))
def case_former(~go: walker, c: K.ctx, expected: Maybe<&2, V.t>, scrut: T.t, w: V.t, former: V.Former, mo: Maybe<&2, Syn.motive>, brs: List<&2, Syn.branch>, only_mu: Bool) -> M.result(T.t):
  match former:
    case V.Former{s, clo, u}:
      match s:
        case S.SMu{n, ix}: elab_mu_case(go, c, expected, scrut, n, ix, clo, u, mo, brs)
        case other:
          choose(M.result(T.t), only_mu, _ => M.Abort{E.Mismatch{"a match needs a mu family as the type of its scrutinee"}},
            _ => elab_coll_case(go, c, expected, scrut, w, other, clo, mo, brs))
def elab_app(~go: walker, +c: K.ctx, +f: Syn.t, +a: Syn.t) -> M.result(T.t):
  app_spine(go, c, f, a, spine_of(Syn.SApp{f, a}, []))
def elab_mu_group_program(+budget: B.t, globals: G.t, +fams: List<&2, Syn.fam>) -> M.result(G.t):
  M.bind(G.t, G.t, declare_families(fams, budget, globals), g => define_families(fams, budget, map(Syn.fam, String, fam_name, fams), g))

# The driver recursively answers surface requests and propagates typed errors.
def elab_case(~go: walker, +c: K.ctx, expected: Maybe<&2, V.t>, scrut: Syn.t, mo: Maybe<&2, Syn.motive>, brs: List<&2, Syn.branch>, +only_mu: Bool) -> M.result(T.t):
  M.bind(T.t, T.t, go(c, None{}, scrut), +tm =>
    M.bind(V.t, T.t, type_of(c, tm), +w =>
      M.bind(V.Former, T.t, need(V.Former, E.Mismatch{F.choose(String, only_mu, "a match needs a mu family as the type of its scrutinee", "a case needs a left former as the type of its scrutinee")}, V.as_lan(w)), former => case_former(go, c, expected, tm, w, former, mo, brs, only_mu))))
def elab_node(~go: walker, +c: K.ctx, expected: Maybe<&2, V.t>, s: Syn.t) -> M.result(T.t):
  match s:
    case Syn.SVar{+x}:
      maybe(mu_ref, M.result(T.t), mu_ref_of(c, x), _ => M.Pure{local_or_global(c, x)}, r => elab_mu_ref(go, c, r, []))
    case Syn.SNat{n}: M.Pure{T.Lit{Lit.LInt{n}}}
    case Syn.SProp{}: M.Pure{T.Univ{L.zero}}
    case Syn.SType{n}: univ_of_int(F.Int63.succ(n))
    case Syn.SPrim{p}: M.Pure{T.Global{Syn.prim_name(p)}}
    case Syn.SUnit{}: M.Pure{R.unit_val}
    case Syn.SAuto{}: M.Pure{T.Auto{}}
    case Syn.SPair{a, b}: elab_pair(go, c, expected, a, b)
    case Syn.STuple{+items}: M.map_result(List<&2, T.t>, T.t, terms => T.Sec{S.SColl{length(Syn.t, items)}, map(T.t, T.leg, leg_of, terms)}, elab_items(go, c, expected, items))
    case Syn.SSum{items}: M.map_result(List<&2, T.t>, T.t, terms => sum_ty(terms), collect(Syn.t, T.t, K.ctx, ctx => it => go(ctx, None{}, it), c, items))
    case Syn.SProd{items}: M.map_result(List<&2, T.t>, T.t, terms => prod_ty(terms), collect(Syn.t, T.t, K.ctx, ctx => it => go(ctx, None{}, it), c, items))
    case Syn.SProj{a, k}: elab_proj(go, c, a, k)
    case Syn.SInj{+k, +n, a}:
      M.bind(List<&2, Maybe<&2, V.t>>, T.t, leg_expectations(c, expected, n, True{}), tys =>
        M.map_result(T.t, T.t, tm => T.In{S.SColl{n}, T.ALeg{k}, [tm]}, go(c, joined(R.at(Maybe<&2, V.t>, k, tys)), a)))
    case Syn.SAbsurd{a}: M.map_result(T.t, T.t, tm => T.Elim{T.Elimination{S.SColl{F.Int63.zero}, tm, Q.One{}, None{}, []}}, go(c, None{}, a))
    case Syn.SApp{f, a}: elab_app(go, c, f, a)
    case Syn.SFun{bs, body}: elab_fun(go, bs, c, expected, body)
    case Syn.SArrow{b, cod}: elab_group(go, c, b, cod, False{})
    case Syn.SStar{b, cod}: elab_group(go, c, b, cod, True{})
    case Syn.SLet{+x, ty, definition, body}:
      M.bind(T.t, T.t, go(c, None{}, ty), +tyt =>
        M.bind(V.t, T.t, eval_in(c, tyt), +tyv =>
          M.bind(T.t, T.t, go(c, Some{tyv}, definition), +dt =>
            M.bind(V.t, T.t, eval_in(c, dt), dv =>
              M.map_result(T.t, T.t, bt => T.Let{x, tyt, dt, bt}, go(K.define(x, Q.Many{}, tyv, dv, c), expected, body))))))
    case Syn.SAnn{a, ty}:
      M.bind(T.t, T.t, go(c, None{}, ty), +tyt =>
        M.bind(V.t, T.t, eval_in(c, tyt), tyv =>
          M.map_result(T.t, T.t, tm => T.Ann{tm, tyt}, go(c, Some{tyv}, a))))
    case Syn.SCase{scrut, mo, brs}: elab_case(go, c, expected, scrut, mo, brs, False{})
    case Syn.SMatch{scrut, mo, brs}: elab_case(go, c, expected, scrut, mo, brs, True{})

# Surface requests recurse through finite syntax and declaration telescopes.
@unsafe
def drive(-A: Data, value: M.program<A>) -> R.result(A):
  match value:
    case M.Pure{x}: Done{x}
    case M.Abort{error}: Fail{error}
    case M.Request{c, expected, s, resume}: drive(A, resume(drive(T.t, elab_node(request, c, expected, s))))
def elab(c: K.ctx, expected: Maybe<&2, V.t>, s: Syn.t) -> R.result(T.t): drive(T.t, request(c, expected, s))
def elab_decl(c: K.ctx, d: Syn.decl) -> R.result(K.decl): drive(K.decl, elab_decl_program(c, d))
def elab_mu_group(budget: B.t, globals: G.t, fams: List<&2, Syn.fam>) -> R.result(G.t): drive(G.t, elab_mu_group_program(budget, globals, fams))
