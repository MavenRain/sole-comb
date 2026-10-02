# Bounded ordinary-definition erasure from Kanon 69f3be51 lib/erase.ml.
# Finite collection products, sums and dependent pairs are supported.
# Recursive families and recursive definitions remain pending.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_check.bend as C
import ../lib/kernel_error.bend as E
import ../lib/kernel_eval.bend as EV
import ../lib/kernel_global.bend as G
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_rules.bend as R
import ../lib/kernel_shape.bend as S
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import eterm.bend as K
import runtime.bend as RT
import type.bend as Ty
import dependencies.bend as Dep
import sum.bend as Sum
import pair.bend as Pair
import mu.bend as Mu

type slot is Data:
  Drop{}
  Keep{}
  Extra{}
type state is Data:
  State{next: Nat, lifted: List<&2, K.kdecl>, groups: List<&2, K.tid>, fuel: B.t}
type entry is Data:
  Dropped{}
  Postulate{repr: K.repr}
  Code{decls: List<&2, K.kdecl>}
type found is Data:
  Found{index: F.Int63.t}
  Gone{}
  Missing{}
def with_leg(-Out: Type, value: T.leg, next: List<&2, F.Pair2<Q.t, String>> -> T.t -> Out) -> Out:
  match value:
    case T.Leg{bs, body}: next(bs, body)
def with_shape(-Out: Type, value: S.t<T.t>, pi: Q.t -> String -> T.t -> Out, coll: F.Int63.t -> Out, par: T.t -> T.t -> Out, mu: String -> List<&2, T.t> -> Out, nu: String -> List<&2, T.t> -> Out) -> Out:
  match value:
    case S.SPi{q, x, dom}: pi(q, x, dom)
    case S.SColl{+n}: coll(n)
    case S.SPar{a, b}: par(a, b)
    case S.SMu{+n, +xs}: mu(n, xs)
    case S.SNu{+n, +xs}: nu(n, xs)
def count(~A: Data, xs: List<&2, A>) -> F.Int63.t:
  match xs:
    case []: F.Int63.zero
    case _ <> rest: F.Int63.succ(count(A, rest))
type application is Data:
  Application{head: T.t, args: List<&2, T.t>}
def point_application(shape: S.t<T.t>, address: T.addr, head: T.t) -> Maybe<&2, F.Pair2<T.t, T.t>>:
  match shape:
    case S.SPi{_, _, _}:
      Ty.inspect(Maybe<&2, F.Pair2<Q.t, T.t>>, Maybe<&2, F.Pair2<T.t, T.t>>, T.as_apt(address), +view3 => Ty.maybe(F.Pair2<Q.t, T.t>, Maybe<&2, F.Pair2<T.t, T.t>>, view3, _ =>
None{}, +payload =>
Ty.pair(Q.t, T.t, Maybe<&2, F.Pair2<T.t, T.t>>, payload, _ => +arg =>
Some{F.Pair2{head, arg}})))
    case S.SColl{_}: None{}
    case S.SPar{_, _}: None{}
    case S.SMu{_, _}: None{}
    case S.SNu{_, _}: None{}
type job is Data:
  Job{ty: V.t, term: T.t}
def free_addr(depth: F.Int63.t, a: T.addr) -> List<&2, F.Pair2<F.Int63.t, T.t>>:
  Ty.inspect(Maybe<&2, F.Pair2<Q.t, T.t>>, List<&2, F.Pair2<F.Int63.t, T.t>>, T.as_apt(a), +view7 => Ty.maybe(F.Pair2<Q.t, T.t>, List<&2, F.Pair2<F.Int63.t, T.t>>, view7, _ =>
[], +payload =>
Ty.pair(Q.t, T.t, List<&2, F.Pair2<F.Int63.t, T.t>>, payload, _ => +arg =>
[F.Pair2{depth, arg}])))
def free_terms(+depth: F.Int63.t, ts: List<&2, T.t>) -> List<&2, F.Pair2<F.Int63.t, T.t>>:
  match ts:
    case []: []
    case tm <> rest: F.Pair2{depth, tm} <> free_terms(depth, rest)
def var_view(tm: T.t) -> Maybe<&2, F.Int63.t>:
  match tm:
    case T.Var{ix}: Some{ix}
    case T.Univ{_}: None{}
    case T.Lan{_, _}: None{}
    case T.Ran{_, _}: None{}
    case T.In{_, _, _}: None{}
    case T.Elim{_}: None{}
    case T.Sec{_, _}: None{}
    case T.Out{_, _, _}: None{}
    case T.Let{_, _, _, _}: None{}
    case T.Ann{_, _}: None{}
    case T.Global{_}: None{}
    case T.Lit{_}: None{}
    case T.Auto{}: None{}
type capture is Data:
  Capture{reprs: List<&2, K.repr>, args: List<&2, K.ktm>}
@unsafe
def eta_vars(+n: F.Int63.t, +i: F.Int63.t) -> List<&2, K.ktm>:
  Ty.inspect(Bool, List<&2, K.ktm>, RT.lt(i, n), +view13 => Ty.boolean(List<&2, K.ktm>, view13, _ =>
K.KVar{F.Int63.pred(F.Int63.sub(n, i))} <> eta_vars(n, F.Int63.succ(i)), _ =>
[]))
def repr_tids(rs: List<&2, K.repr>) -> List<&2, K.tid>:
  match rs:
    case []: []
    case K.RI31{} <> +rest: repr_tids(rest)
    case K.RStruct{+t} <> +rest: t <> repr_tids(rest)
    case K.RUnion{+t} <> +rest: t <> repr_tids(rest)
    case K.RFunc{+t} <> +rest: t <> repr_tids(rest)
    case K.RThunk{+t} <> +rest: t <> repr_tids(rest)
def has_tid(+tid: K.tid, ts: List<&2, K.tid>) -> Bool:
  match ts:
    case []: False{}
    case t <> rest: Bool.or(String.eq(K.tid_text(tid), K.tid_text(t)), has_tid(tid, rest))
def print_decls(ds: List<&2, K.kdecl>) -> String:
  match ds:
    case []: ""
    case d <> rest: String.concat([K.print_decl(d), "\n", print_decls(rest)])
type ctx is Data:
  Context{check: C.ctx, slots: List<&2, slot>, self: String}
def answer() -> Data: F.Pair2<K.ktm, state>
def with_state(-Out: Type, value: state, next: Nat -> List<&2, K.kdecl> -> List<&2, K.tid> -> B.t -> Out) -> Out:
  match value:
    case State{n, ds, groups, b}: next(n, ds, groups, b)
def with_found(-Out: Type, value: found, found: F.Int63.t -> Out, gone: Unit -> Out, missing: Unit -> Out) -> Out:
  match value:
    case Found{i}: found(i)
    case Gone{}: gone(Unit{})
    case Missing{}: missing(Unit{})
@unsafe
def lookup(+ix: F.Int63.t, slots: List<&2, slot>, +seen: F.Int63.t) -> found:
  match slots:
    case []: Missing{}
    case Extra{} <> rest: lookup(ix, rest, F.Int63.succ(seen))
    case Keep{} <> +rest:
      Ty.inspect(Bool, found, F.Int63.equal(ix, F.Int63.zero), +view0 => Ty.boolean(found, view0, _ =>
Found{seen}, _ =>
lookup(F.Int63.pred(ix), rest, F.Int63.succ(seen))))
    case Drop{} <> +rest:
      Ty.inspect(Bool, found, F.Int63.equal(ix, F.Int63.zero), +view1 => Ty.boolean(found, view1, _ =>
Gone{}, _ =>
lookup(F.Int63.pred(ix), rest, seen)))
def application_view(tm: T.t) -> Maybe<&2, F.Pair2<T.t, T.t>>:
  match tm:
    case T.Out{shape, address, head}: point_application(shape, address, head)
    case T.Var{_}: None{}
    case T.Univ{_}: None{}
    case T.Lan{_, _}: None{}
    case T.Ran{_, _}: None{}
    case T.In{_, _, _}: None{}
    case T.Elim{_}: None{}
    case T.Sec{_, _}: None{}
    case T.Let{_, _, _, _}: None{}
    case T.Ann{_, _}: None{}
    case T.Global{_}: None{}
    case T.Lit{_}: None{}
    case T.Auto{}: None{}
type arguments is Data:
  Arguments{terms: List<&2, K.ktm>, state: state}
def free_legs(+depth: F.Int63.t, ls: List<&2, T.leg>) -> List<&2, F.Pair2<F.Int63.t, T.t>>:
  match ls:
    case []: []
    case T.Leg{bs, body} <> rest: F.Pair2{F.Int63.add(depth, count(F.Pair2<Q.t, String>, bs)), body} <> free_legs(depth, rest)
def framed(slots: List<&2, slot>, +caps: List<&2, F.Int63.t>, +ix: F.Int63.t) -> List<&2, slot>:
  match slots:
    case []: []
    case Extra{} <> rest: framed(rest, caps, ix)
    case Drop{} <> +rest: Drop{} <> framed(rest, caps, F.Int63.succ(ix))
    case Keep{} <> +rest: F.choose(slot, RT.member(ix, caps), Keep{}, Drop{}) <> framed(rest, caps, F.Int63.succ(ix))
def with_capture(-Out: Type, value: capture, next: List<&2, K.repr> -> List<&2, K.ktm> -> Out) -> Out:
  match value:
    case Capture{ps, xs}: next(ps, xs)
type chain_result is Data:
  Chain{params: List<&2, K.repr>, result: K.repr, body: K.ktm, state: state}
def lambda_body(tm: T.t) -> Maybe<&2, T.t>:
  Ty.inspect(Maybe<&2, F.Pair2<S.t<T.t>, List<&2, T.leg>>>, Maybe<&2, T.t>, R.as_tsec(tm), +view11 => Ty.maybe(F.Pair2<S.t<T.t>, List<&2, T.leg>>, Maybe<&2, T.t>, view11, _ =>
None{}, +payload =>
Ty.pair(S.t<T.t>, List<&2, T.leg>, Maybe<&2, T.t>, payload, +shape => +legs => with_shape(Maybe<&2, T.t>, shape, _ => _ => _ =>

            Ty.inspect(Maybe<&2, T.leg>, Maybe<&2, T.t>, R.one_of(T.leg, legs), +view12 => Ty.maybe(T.leg, Maybe<&2, T.t>, view12, _ =>
None{}, +payload =>
with_leg(Maybe<&2, T.t>, payload, _ => +body =>
Some{body}))), _ =>
None{}, _ => _ =>
None{}, _ => _ =>
None{}, _ => _ =>
None{}))))
def decl_tids(ds: List<&2, K.kdecl>) -> List<&2, K.tid>:
  match ds:
    case []: []
    case K.KFun{_, ps, ret, body} <> +rest: List.append(&2, K.tid, repr_tids(List.append(&2, K.repr, ps, [ret])), List.append(&2, K.tid, Dep.term(body), decl_tids(rest)))
    case K.KRec{ts} <> +rest: List.append(&2, K.tid, ts, decl_tids(rest))
def unique_tids(ts: List<&2, K.tid>, +acc: List<&2, K.tid>) -> List<&2, K.tid>:
  match ts:
    case []: acc
    case +t <> +rest:
      Ty.inspect(Bool, List<&2, K.tid>, has_tid(t, acc), +view16 => Ty.boolean(List<&2, K.tid>, view16, _ =>
unique_tids(rest, acc), _ =>
unique_tids(rest, List.append(&2, K.tid, acc, [t]))))
def rows() -> Data: List<&2, F.Pair2<String, entry>>
def print_entry(+name: String, en: entry) -> String:
  match en:
    case Dropped{}: String.concat(["erased ", name, "\n"])
    case Postulate{r}: String.concat(["axiom ", name, " : ", K.print_repr(r), "\n"])
    case Code{ds}: print_decls(ds)
def with_answer(-Out: Type, value: answer, next: K.ktm -> state -> Out) -> Out:
  match value:
    case F.Pair2{tm, s}: next(tm, s)
def with_context(-Out: Type, value: ctx, next: C.ctx -> List<&2, slot> -> String -> Out) -> Out:
  match value:
    case Context{c, slots, self}: next(c, slots, self)
def checker(+c: ctx) -> C.ctx:
  match c:
    case Context{+c, _, _}: c
def added(c: ctx, q: Q.t, x: String, dom: V.t, keep: Bool) -> ctx:
  match c:
    case Context{check, slots, self}: Context{C.bind(x, q, dom, check), F.choose(slot, keep, Keep{}, Drop{}) <> slots, self}
@unsafe
def spine(+tm: T.t, +args: List<&2, T.t>) -> application:
  Ty.inspect(Maybe<&2, F.Pair2<T.t, T.t>>, application, application_view(tm), +view4 => Ty.maybe(F.Pair2<T.t, T.t>, application, view4, _ =>
Application{tm, args}, +payload =>
Ty.pair(T.t, T.t, application, payload, +head => +arg =>
spine(head, arg <> args))))
def with_arguments(-Out: Type, value: arguments, next: List<&2, K.ktm> -> state -> Out) -> Out:
  match value:
    case Arguments{xs, s}: next(xs, s)
def call_result(+head: K.ktm, +args: List<&2, K.ktm>, +s: state, tail: Bool, bare: Bool) -> answer:
  match bare:
    case True{}: F.Pair2{head, s}
    case False{}: F.Pair2{F.choose(K.ktm, tail, K.KTail{head, args}, K.KApp{head, args}), s}
def free_branches(+depth: F.Int63.t, bs: List<&2, F.Pair2<T.addr, T.leg>>) -> List<&2, F.Pair2<F.Int63.t, T.t>>:
  match bs:
    case []: []
    case F.Pair2{_, leg} <> rest: List.append(&2, F.Pair2<F.Int63.t, T.t>, free_legs(depth, [leg]), free_branches(depth, rest))
def captures(+slots: List<&2, slot>, xs: List<&2, F.Int63.t>) -> List<&2, F.Int63.t>:
  match xs:
    case []: []
    case +ix <> +rest:
      Ty.inspect(found, List<&2, F.Int63.t>, lookup(ix, slots, F.Int63.zero), +view10 => with_found(List<&2, F.Int63.t>, view10, _ =>
ix <> captures(slots, rest), _ =>
captures(slots, rest), _ =>
captures(slots, rest)))
def with_chain(-Out: Type, value: chain_result, next: List<&2, K.repr> -> K.repr -> K.ktm -> state -> Out) -> Out:
  match value:
    case Chain{ps, ret, body, s}: next(ps, ret, body, s)
def code(name: String, result: chain_result) -> entry:
  match result:
    case Chain{params, ret, body, State{_, ds, groups, _}}:
      Ty.inspect(List<&2, K.kdecl>, entry, List.append(&2, K.kdecl, ds, [K.KFun{K.Fid{name}, params, ret, body}]), +all =>
        Code{K.KRec{unique_tids(List.append(&2, K.tid, decl_tids(all), groups), [])} <> all})
def print(rows: rows) -> String:
  match rows:
    case []: ""
    case F.Pair2{name, en} <> rest: String.append(print_entry(name, en), print(rest))
type task is Data:
  TermTask{context: ctx, state: state, tail: Bool, expected: Maybe<&2, V.t>, term: T.t}
  ChainTask{context: ctx, state: state, params: List<&2, K.repr>, ty: V.t, term: T.t}
  JobsTask{context: ctx, ty: V.t, args: List<&2, T.t>, acc: List<&2, job>, budget: B.t}
type output is Data:
  TermOutput{answer: answer}
  ChainOutput{answer: chain_result}
  JobsOutput{answer: List<&2, job>}
def free_children(+depth: F.Int63.t, tm: T.t) -> List<&2, F.Pair2<F.Int63.t, T.t>>:
  match tm:
    case T.Var{_}: []
    case T.Univ{_}: []
    case T.Global{_}: []
    case T.Lit{_}: []
    case T.Auto{}: []
    case T.Lan{_, _}: []
    case T.Ran{_, _}: []
    case T.In{_, +a, args}: List.append(&2, F.Pair2<F.Int63.t, T.t>, free_addr(depth, a), free_terms(depth, args))
    case T.Out{_, +a, head}: List.append(&2, F.Pair2<F.Int63.t, T.t>, free_addr(depth, a), [F.Pair2{depth, head}])
    case T.Sec{_, legs}: free_legs(depth, legs)
    case T.Elim{T.Elimination{_, scrut, _, _, branches}}: F.Pair2{depth, scrut} <> free_branches(depth, branches)
    case T.Let{_, _, value, +body}: [F.Pair2{depth, value}, F.Pair2{F.Int63.succ(depth), body}]
    case T.Ann{+body, _}: [F.Pair2{depth, body}]
type computation<-A: Data> is Type:
  Pure{value: A}
  Abort{error: E.t}
  Request{task: task, resume: output -> computation<A>}
def variable(c: ctx, +s: state, +ix: F.Int63.t) -> computation<answer>:
  match c:
    case Context{_, slots, _}:
      Ty.inspect(found, computation<answer>, lookup(ix, slots, F.Int63.zero), +view2 => with_found(computation<answer>, view2, +i =>
Pure{F.Pair2{K.KVar{i}, s}}, _ =>
Pure{F.Pair2{K.KErased{}, s}}, _ =>
Abort{E.Unbound{String.concat(["the erasure context has no binder at index ", F.Int63.show(ix)])}}))
def shifted(params: List<&2, K.repr>, ret: K.repr, s: state, n: F.Int63.t, value: RT.result<K.ktm>) -> computation<chain_result>:
  match value:
    case RT.InvalidStack{}: Abort{E.Mismatch{"invalid erasure runtime stack"}}
    case RT.Done{head}: Pure{Chain{params, ret, K.KTail{head, eta_vars(n, F.Int63.zero)}, s}}
def lifted(+fid: K.fid, +params: List<&2, K.repr>, ret: K.repr, s: state, value: RT.result<RT.pruned>) -> computation<answer>:
  match value:
    case RT.InvalidStack{}: Abort{E.Mismatch{"invalid erasure capture stack"}}
    case RT.Done{RT.Pruned{ps, args, body}}:
      with_state(computation<answer>, s, +n => +ds => +groups => +b =>
        Pure{F.Pair2{K.KClos{fid, count(K.repr, params), args}, State{n, List.append(&2, K.kdecl, ds, [K.KFun{fid, List.append(&2, K.repr, ps, params), ret, body}]), groups, b}}})
@unsafe
def free_loop(todo: List<&2, F.Pair2<F.Int63.t, T.t>>, +acc: List<&2, F.Int63.t>) -> List<&2, F.Int63.t>:
  match todo:
    case []: acc
    case F.Pair2{+depth, +tm} <> +rest:
      Ty.inspect(Maybe<&2, F.Int63.t>, List<&2, F.Int63.t>, var_view(tm), +view8 => Ty.maybe(F.Int63.t, List<&2, F.Int63.t>, view8, _ =>
free_loop(List.append(&2, F.Pair2<F.Int63.t, T.t>, free_children(depth, tm), rest), acc), +ix =>

            Ty.inspect(Bool, List<&2, F.Int63.t>, RT.ge(ix, depth), +view9 => Ty.boolean(List<&2, F.Int63.t>, view9, _ =>
free_loop(rest, RT.insert(F.Int63.sub(ix, depth), acc)), _ =>
free_loop(rest, acc)))))
def lift_result(-A: Data, value: R.result(A)) -> computation<A>:
  match value:
    case Done{x}: Pure{x}
    case Fail{e}: Abort{e}
@unsafe
def bind(-A: Data, -D: Data, value: computation<A>, next: A -> computation<D>) -> computation<D>:
  match value:
    case Pure{x}: next(x)
    case Abort{error}: Abort{error}
    case Request{job, resume}: Request{job, answer => bind(A, D, resume(answer), next)}
def term_output(value: output) -> computation<answer>:
  match value:
    case TermOutput{answer}: Pure{answer}
    case ChainOutput{_}: Abort{E.Mismatch{"internal erasure term response"}}
    case JobsOutput{_}: Abort{E.Mismatch{"internal erasure term response"}}
def chain_output(value: output) -> computation<chain_result>:
  match value:
    case ChainOutput{answer}: Pure{answer}
    case TermOutput{_}: Abort{E.Mismatch{"internal erasure chain response"}}
    case JobsOutput{_}: Abort{E.Mismatch{"internal erasure chain response"}}
def jobs_output(value: output) -> computation<List<&2, job>>:
  match value:
    case JobsOutput{answer}: Pure{answer}
    case TermOutput{_}: Abort{E.Mismatch{"internal erasure jobs response"}}
    case ChainOutput{_}: Abort{E.Mismatch{"internal erasure jobs response"}}
def inferred(+c: ctx, expected: Maybe<&2, V.t>, tm: T.t) -> computation<V.t>:
  match expected:
    case Some{ty}: Pure{ty}
    case None{}: lift_result(V.t, C.infer(checker(c), Q.Many{}, tm))
def map_result(-A: Data, -D: Data, f: A -> D, value: computation<A>) -> computation<D>:
  bind(A, D, value, x => Pure{f(x)})
def term_request(c: ctx, s: state, tail: Bool, expected: Maybe<&2, V.t>, tm: T.t) -> computation<answer>:
  Request{TermTask{c, s, tail, expected, tm}, term_output}
def chain_request(c: ctx, s: state, params: List<&2, K.repr>, ty: V.t, tm: T.t) -> computation<chain_result>:
  Request{ChainTask{c, s, params, ty, tm}, chain_output}
def jobs_request(c: ctx, ty: V.t, args: List<&2, T.t>, acc: List<&2, job>, b: B.t) -> computation<List<&2, job>>:
  Request{JobsTask{c, ty, args, acc, b}, jobs_output}
def tick(s: state) -> computation<state>:
  match s:
    case State{n, ds, groups, b}: map_result(B.t, state, +rest => State{n, ds, groups, rest}, lift_result(B.t, B.spend("type-directed erasure", b)))
def jobs_arrow(+c: ctx, +arg: T.t, rest: List<&2, T.t>, a: Ty.arrow, +acc: List<&2, job>, b: B.t) -> computation<List<&2, job>>:
  match a:
    case Ty.Arrow{q, _, +dom, d}:
      bind(Bool, List<&2, job>, lift_result(Bool, Ty.point_runtime(checker(c), q, dom)), +keep =>
        bind(V.t, List<&2, job>, lift_result(V.t, EV.eval(C.globals(checker(c)), C.env(checker(c)), arg)), +av =>
          bind(V.t, List<&2, job>, lift_result(V.t, Ty.opened(checker(c), d, [av])), +cod =>
            jobs_request(c, cod, rest, F.choose(List<&2, job>, keep, Job{dom, arg} <> acc, acc), b))))
def app_applied(+c: ctx, +head: K.ktm, +args: List<&2, K.ktm>, +s: state, +tail: Bool, ty: V.t) -> computation<answer>:
  bind(V.t, answer, lift_result(V.t, EV.whnf(C.globals(checker(c)), ty)), +w =>
    Ty.inspect(Maybe<&2, Ty.arrow>, computation<answer>, Ty.arrow_of(w), +view18 => Ty.maybe(Ty.arrow, computation<answer>, view18, _ =>
Pure{call_result(head, args, s, tail, False{})}, _ =>
      bind(F.Int63.t, answer, lift_result(F.Int63.t, Ty.arity(checker(c), w, F.Int63.zero, C.budget(checker(c)))), +n =>
        Ty.inspect(Bool, computation<answer>, F.Int63.equal(n, F.Int63.zero), +view19 => Ty.boolean(computation<answer>, view19, _ =>
Abort{E.Not_yet{"applications whose remaining parameters all erase in type-directed erasure"}}, _ =>
Pure{call_result(head, args, s, tail, False{})}))))))
def app_finish(+c: ctx, +head: K.ktm, value: arguments, +tail: Bool, +ty: V.t) -> computation<answer>:
  match value:
    case Arguments{+args, +s}:
      Ty.inspect(Bool, computation<answer>, Ty.empty(K.ktm, args), +view6 => Ty.boolean(computation<answer>, view6, _ =>

            map_result(V.t, answer, +w => call_result(head, args, s, tail, Ty.has(Ty.arrow, Ty.arrow_of(w))), lift_result(V.t, EV.whnf(C.globals(checker(c)), ty))), _ =>
app_applied(c, head, args, s, tail, ty)))
def cap_values(+c: ctx, caps: List<&2, F.Int63.t>) -> computation<capture>:
  match caps:
    case []: Pure{Capture{[], []}}
    case +ix <> rest:
      bind(V.t, capture, lift_result(V.t, C.infer(checker(c), Q.Many{}, T.Var{ix})), +ty =>
        bind(K.repr, capture, lift_result(K.repr, Ty.repr_of(checker(c), ty)), +r =>
          bind(answer, capture, variable(c, State{0n, [], [], B.create(0n)}, ix), +pair => with_answer(computation<capture>, pair, +arg => _ =>

                map_result(capture, capture, +value => with_capture(capture, value, +ps => +args =>
Capture{r <> ps, arg <> args}), cap_values(c, rest))))))
@unsafe
def erase_jobs(+c: ctx, +s: state, js: List<&2, job>, +acc: List<&2, K.ktm>) -> computation<arguments>:
  match js:
    case []: Pure{Arguments{R.rev_append(K.ktm, acc, []), s}}
    case Job{ty, tm} <> rest:
      bind(answer, arguments, term_request(c, s, False{}, Some{ty}, tm), +pair => with_answer(computation<arguments>, pair, +ktm => +s1 =>
erase_jobs(c, s1, rest, ktm <> acc)))
def let_body(c: ctx, s: state, tail: Bool, ty: V.t, x: String, value: K.ktm, body: T.t) -> computation<answer>:
  map_result(answer, answer, +pair => with_answer(answer, pair, +kbody => +s1 =>
F.Pair2{K.KLet{x, value, kbody}, s1}), term_request(c, s, tail, Some{ty}, body))
def chain_stop(+c: ctx, s: state, params: List<&2, K.repr>, +ty: V.t, tm: T.t) -> computation<chain_result>:
  bind(K.repr, chain_result, lift_result(K.repr, Ty.repr_of(checker(c), ty)), +ret =>
    map_result(answer, chain_result, +pair => with_answer(chain_result, pair, +body => +s1 =>
Chain{params, ret, body, s1}), term_request(c, s, True{}, Some{ty}, tm)))
def eta(+c: ctx, s: state, params: List<&2, K.repr>, +ty: V.t, tm: T.t) -> computation<chain_result>:
  bind(answer, chain_result, term_request(c, s, False{}, Some{ty}, tm), +pair =>
    with_answer(computation<chain_result>, pair, +head => +s1 =>
      bind(Ty.signature, chain_result, lift_result(Ty.signature, Ty.signature_of(checker(c), ty, C.budget(checker(c)))), +sig =>
        Ty.with_signature(computation<chain_result>, sig, +extra => +ret =>
          Ty.inspect(F.Int63.t, computation<chain_result>, count(K.repr, extra), +n =>
            shifted(List.append(&2, K.repr, params, extra), ret, s1, n, RT.shift_runtime(n, F.Int63.zero, head)))))))
def chain_arrow(+c: ctx, s: state, params: List<&2, K.repr>, a: Ty.arrow, body: T.t) -> computation<chain_result>:
  match a:
    case Ty.Arrow{+q, x, +dom, d}:
      bind(Bool, chain_result, lift_result(Bool, Ty.point_runtime(checker(c), q, dom)), +keep =>
        bind(List<&2, K.repr>, chain_result, lift_result(List<&2, K.repr>, Ty.parameter(checker(c), q, dom)), +here =>
          bind(V.t, chain_result, lift_result(V.t, Ty.opened(checker(c), d, [V.var(C.size(checker(c)))])), +cod =>
            chain_request(added(c, q, x, dom, keep), s, List.append(&2, K.repr, params, here), cod, body))))
def decl_value(+c: C.ctx, +name: String, body: Maybe<&2, T.t>, +ty: V.t, b: B.t, recursive: Bool) -> computation<entry>:
  bind(Bool, entry, lift_result(Bool, Ty.runtime_ty(c, ty)), +keep => Ty.boolean(computation<entry>, keep, _ => Ty.boolean(computation<entry>, recursive, _ =>
Abort{E.Not_yet{"recursive definitions in type-directed erasure"}}, _ => Ty.maybe(T.t, computation<entry>, body, _ =>
map_result(K.repr, entry, +r => Postulate{r}, lift_result(K.repr, Ty.repr_of(c, ty))), +tm =>
map_result(chain_result, entry, +result => code(name, result), chain_request(Context{c, [], name}, State{0n, [], [], b}, [], ty, tm)))), _ =>
Pure{Dropped{}}))
def jobs_weak(c: ctx, ty: V.t, arg: T.t, rest: List<&2, T.t>, acc: List<&2, job>, b: B.t) -> computation<List<&2, job>>:
  Ty.inspect(Maybe<&2, Ty.arrow>, computation<List<&2, job>>, Ty.arrow_of(ty), +view5 => Ty.maybe(Ty.arrow, computation<List<&2, job>>, view5, _ =>
Abort{E.Mismatch{"an application needs a point function type"}}, +a =>
jobs_arrow(c, arg, rest, a, acc, b)))
def app(+c: ctx, s: state, tail: Bool, ty: V.t, a: application) -> computation<answer>:
  match a:
    case Application{+head, args}:
      bind(V.t, answer, lift_result(V.t, C.infer(checker(c), Q.Many{}, head)), +head_ty =>
        bind(List<&2, job>, answer, jobs_request(c, head_ty, args, [], C.budget(checker(c))), +js =>
          bind(answer, answer, term_request(c, s, False{}, Some{head_ty}, head), +pair => with_answer(computation<answer>, pair, +khead => +s1 =>

                bind(arguments, answer, erase_jobs(c, s1, js, []), +value => app_finish(c, khead, value, tail, ty))))))
def let_defined(c: ctx, +s: state, +tail: Bool, +ty: V.t, +x: String, +lty: V.t, value: T.t, +body: T.t, +keep: Bool, vv: V.t) -> computation<answer>:
  match c:
    case Context{+check, +slots, +self}:
      Ty.inspect(ctx, computation<answer>, Context{C.define(x, Q.Many{}, lty, vv, check), F.choose(slot, keep, Keep{}, Drop{}) <> slots, self}, +next =>
        Ty.boolean(computation<answer>, keep, _ =>
        bind(answer, answer, term_request(Context{check, slots, self}, s, False{}, Some{lty}, value), +pair =>
          with_answer(computation<answer>, pair, +kv => +s1 => let_body(next, s1, tail, ty, x, kv, body))),
        _ => term_request(next, s, tail, Some{ty}, body)))
def chain_weak(+c: ctx, +s: state, +params: List<&2, K.repr>, +ty: V.t, +tm: T.t, weak: V.t) -> computation<chain_result>:
  Ty.inspect(Maybe<&2, Ty.arrow>, computation<chain_result>, Ty.arrow_of(weak), +view14 => Ty.maybe(Ty.arrow, computation<chain_result>, view14, _ =>
chain_stop(c, s, params, ty, tm), +a =>

        Ty.inspect(Maybe<&2, T.t>, computation<chain_result>, lambda_body(tm), +view15 => Ty.maybe(T.t, computation<chain_result>, view15, _ =>
eta(c, s, params, ty, tm), +body =>
chain_arrow(c, s, params, a, body)))))
def decl_type(+g: G.t, +b: B.t, name: String, ty: T.t, body: Maybe<&2, T.t>, recursive: Bool) -> computation<entry>:
  bind(V.t, entry, lift_result(V.t, EV.eval(g, [], ty)), +tyv => decl_value(C.make(g, b), name, body, tyv, b, recursive))
def lift_closure(c: ctx, s: state, ty: V.t, +tm: T.t) -> computation<answer>:
  match c:
    case Context{+check, +slots, +self}:
      Ty.inspect(List<&2, F.Int63.t>, computation<answer>, captures(slots, free_loop([F.Pair2{F.Int63.zero, tm}], [])), +caps =>
        bind(capture, answer, cap_values(Context{check, slots, self}, R.rev_append(F.Int63.t, caps, [])), value =>
          with_capture(computation<answer>, value, ps => args =>
            with_state(computation<answer>, s, +n => ds => groups => b =>
              Ty.inspect(K.fid, computation<answer>, K.Fid{String.concat([self, "$", F.Bignum.to_string(F.Bignum.from_nat(n))])}, fid =>
                bind(chain_result, answer, chain_request(Context{check, framed(slots, caps, F.Int63.zero), self}, State{(n + 1n : Nat), ds, groups, b}, [], ty, tm), result =>
                  with_chain(computation<answer>, result, +params => ret => body => s1 =>
                    lifted(fid, params, ret, s1, RT.prune_captures(ps, args, count(K.repr, params), body)))))))))
@unsafe
def tuple_jobs(+c: C.ctx, legs: List<&2, T.leg>, tys: List<&2, V.t>) -> R.result(List<&2, job>):
  match legs, tys:
    case [], []: Done{[]}
    case lg <> rest, +ty <> tail:
      R.bind(Bool, List<&2, job>, Ty.runtime_ty(c, ty), +keep =>
        R.map_result(List<&2, job>, List<&2, job>, +jobs => with_leg(List<&2, job>, lg, _ => +body =>
          F.choose(List<&2, job>, keep, Job{ty, body} <> jobs, jobs)), tuple_jobs(c, rest, tail)))
    case [], _ <> _: Fail{E.Mismatch{"a tuple has the wrong number of legs"}}
    case _ <> _, []: Fail{E.Mismatch{"a tuple has the wrong number of legs"}}
def tuple_finish(c: ctx, ty: V.t, value: arguments) -> computation<answer>:
  match value:
    case Arguments{+fields, +s}:
      Ty.boolean(computation<answer>, Ty.empty(K.ktm, fields), _ => Pure{F.Pair2{K.KErased{}, s}}, _ =>
        map_result(K.tid, answer, +tid => F.Pair2{K.KStruct{tid, fields}, s}, lift_result(K.tid, Ty.tid_of(checker(c), ty))))
def tuple_former(+c: ctx, s: state, +ty: V.t, n: F.Int63.t, +legs: List<&2, T.leg>, former: V.Former) -> computation<answer>:
  match former:
    case V.Former{_, diagram, _}:
      bind(List<&2, V.t>, answer, lift_result(List<&2, V.t>, Ty.leg_types(checker(c), n, diagram)), tys =>
        bind(List<&2, job>, answer, lift_result(List<&2, job>, tuple_jobs(checker(c), legs, tys)), jobs =>
          bind(arguments, answer, erase_jobs(c, s, jobs, []), value => tuple_finish(c, ty, value))))
def tuple(+c: ctx, s: state, +ty: V.t, +n: F.Int63.t, +legs: List<&2, T.leg>) -> computation<answer>:
  bind(V.t, answer, lift_result(V.t, EV.whnf(C.globals(checker(c)), ty)), +w =>
    Ty.maybe(V.Former, computation<answer>, V.as_ran(w), _ => Abort{E.Mismatch{"a tuple stands at a type that is not a right former"}}, former => tuple_former(c, s, ty, n, legs, former)))
@unsafe
def projection_flags(+c: C.ctx, tys: List<&2, V.t>, +before: F.Int63.t) -> R.result(F.Int63.t):
  match tys:
    case []: Done{before}
    case ty <> rest: R.bind(Bool, F.Int63.t, Ty.runtime_ty(c, ty), _ => projection_flags(c, rest, before))
# Kanon flags every leg in leg order, so a later leg can also fail the projection.
@unsafe
def projection_offset(+c: C.ctx, tys: List<&2, V.t>, +index: F.Int63.t, +before: F.Int63.t) -> R.result(F.Int63.t):
  match tys:
    case []: Fail{E.Wrong_leg{R.leg_msg}}
    case ty <> +rest:
      R.bind(Bool, F.Int63.t, Ty.runtime_ty(c, ty), keep =>
        Ty.boolean(R.result(F.Int63.t), F.Int63.equal(index, F.Int63.zero), _ => projection_flags(c, rest, before), _ =>
          projection_offset(c, rest, F.Int63.pred(index), F.choose(F.Int63.t, keep, F.Int63.succ(before), before))))
def projection_former(+c: ctx, s: state, +sty: V.t, n: F.Int63.t, +index: F.Int63.t, head: T.t, former: V.Former) -> computation<answer>:
  match former:
    case V.Former{_, diagram, _}:
      bind(List<&2, V.t>, answer, lift_result(List<&2, V.t>, Ty.leg_types(checker(c), n, diagram)), tys =>
        bind(F.Int63.t, answer, lift_result(F.Int63.t, projection_offset(checker(c), tys, index, F.Int63.zero)), +before =>
          bind(answer, answer, term_request(c, s, False{}, Some{sty}, head), pair =>
            with_answer(computation<answer>, pair, +scrut => +s1 =>
              map_result(K.tid, answer, +tid => F.Pair2{K.KProj{tid, before, scrut}, s1}, lift_result(K.tid, Ty.tid_of(checker(c), sty)))))))
def projection(+c: ctx, s: state, +n: F.Int63.t, address: T.addr, +head: T.t) -> computation<answer>:
  bind(F.Int63.t, answer, lift_result(F.Int63.t, R.require(F.Int63.t, T.as_aleg(address), E.Mismatch{"a projection takes a leg address"})), +index =>
    bind(V.t, answer, lift_result(V.t, C.infer(checker(c), Q.Many{}, head)), +sty =>
      bind(V.t, answer, lift_result(V.t, EV.whnf(C.globals(checker(c)), sty)), w =>
        Ty.maybe(V.Former, computation<answer>, V.as_ran(w), _ => Abort{E.Mismatch{"a projection stands on a value that is not a right former"}}, former => projection_former(c, s, sty, n, index, head, former)))))
def section(c: ctx, s: state, ty: V.t, shape: S.t<T.t>, legs: List<&2, T.leg>) -> computation<answer>:
  match shape:
    case S.SPi{q, x, dom}: lift_closure(c, s, ty, T.Sec{S.SPi{q, x, dom}, legs})
    case S.SColl{n}: tuple(c, s, ty, n, legs)
    case S.SPar{_, _}: Abort{E.Not_yet{"parallel shapes in type-directed erasure"}}
    case S.SMu{_, _}: Abort{E.Not_yet{"recursive families in type-directed erasure"}}
    case S.SNu{_, _}: Abort{E.Not_yet{"coinductive shapes in type-directed erasure"}}
def application_shape(c: ctx, s: state, tail: Bool, ty: V.t, shape: S.t<T.t>, address: T.addr, head: T.t) -> computation<answer>:
  match shape:
    case S.SPi{q, x, dom}: app(c, s, tail, ty, spine(T.Out{S.SPi{q, x, dom}, address, head}, []))
    case S.SColl{n}: projection(c, s, n, address, head)
    case S.SPar{_, _}: Abort{E.Not_yet{"parallel shapes in type-directed erasure"}}
    case S.SMu{_, _}: Abort{E.Not_yet{"recursive families in type-directed erasure"}}
    case S.SNu{_, _}: Abort{E.Not_yet{"coinductive shapes in type-directed erasure"}}
def let_value(+c: ctx, s: state, tail: Bool, ty: V.t, x: String, lty: V.t, +value: T.t, body: T.t, keep: Bool) -> computation<answer>:
  bind(V.t, answer, lift_result(V.t, EV.eval(C.globals(checker(c)), C.env(checker(c)), value)), +vv => let_defined(c, s, tail, ty, x, lty, value, body, keep, vv))
def chain_step(+c: ctx, s: state, params: List<&2, K.repr>, +ty: V.t, tm: T.t) -> computation<chain_result>:
  bind(state, chain_result, tick(s), +s1 =>
    bind(V.t, chain_result, lift_result(V.t, EV.whnf(C.globals(checker(c)), ty)), +w => chain_weak(c, s1, params, ty, tm, w)))
def jobs_step(+c: ctx, ty: V.t, args: List<&2, T.t>, +acc: List<&2, job>, b: B.t) -> computation<List<&2, job>>:
  match args:
    case []: Pure{R.rev_append(job, acc, [])}
    case arg <> rest:
      bind(B.t, List<&2, job>, lift_result(B.t, B.spend("erasure application spine", b)), +fuel =>
        bind(V.t, List<&2, job>, lift_result(V.t, EV.whnf(C.globals(checker(c)), ty)), +w => jobs_weak(c, w, arg, rest, acc, fuel)))
def declaration_program(+g: G.t, +b: B.t, +name: String, en: G.entry) -> computation<entry>:
  match en:
    case G.Prim{_}: Pure{Dropped{}}
    case G.Axiom{G.Postulate{+ty}}: decl_type(g, b, name, ty, None{}, False{})
    case G.Def{G.Definition{+ty, body, _, rec, partial}}:
      Ty.inspect(Bool, computation<entry>, Bool.or(Ty.has(F.Int63.t, rec), partial), +view17 => decl_type(g, b, name, ty, Some{body}, view17))
def let_term(+c: ctx, s: state, tail: Bool, ty: V.t, x: String, lty: T.t, value: T.t, body: T.t) -> computation<answer>:
  bind(V.t, answer, lift_result(V.t, EV.eval(C.globals(checker(c)), C.env(checker(c)), lty)), +ltyv =>
    bind(Bool, answer, lift_result(Bool, Ty.runtime_ty(checker(c), ltyv)), +keep => let_value(c, s, tail, ty, x, ltyv, value, body, keep)))

# Kernel capture candidates exclude type syntax. The runtime pass prunes them.
@unsafe
def program_loop(+g: G.t, +b: B.t, entries: List<&2, F.Pair2<String, G.entry>>, +acc: rows) -> computation<rows>:
  match entries:
    case []: Pure{R.rev_append(F.Pair2<String, entry>, acc, [])}
    case F.Pair2{+name, +en} <> rest:
      bind(entry, rows, declaration_program(g, b, name, en), +erased => program_loop(G.add(name, en, g), b, rest, F.Pair2{name, erased} <> acc))
def tag_body(c: ctx, s: state, +tid: K.tid, +index: F.Int63.t, value: Sum.payload) -> computation<answer>:
  match value:
    case Sum.Payload{ty, body}:
      map_result(answer, answer, +pair => with_answer(answer, pair, +field => +s1 => F.Pair2{K.KTag{tid, index, [field]}, s1}), term_request(c, s, False{}, Some{ty}, body))
def tag_plan(c: ctx, +s: state, plan: Sum.tag) -> computation<answer>:
  match plan:
    case Sum.Tag{+tid, +index, payload}:
      Ty.maybe(Sum.payload, computation<answer>, payload,
        _ => Pure{F.Pair2{K.KTag{tid, index, []}, s}},
        +value => tag_body(c, s, tid, index, value))
def pair_jobs(fields: List<&2, Pair.field>) -> List<&2, job>:
  match fields:
    case []: []
    case Pair.Field{ty, body} <> rest: Job{ty, body} <> pair_jobs(rest)
def pair_fields(+c: ctx, s: state, +ty: V.t, fields: List<&2, Pair.field>) -> computation<answer>:
  bind(arguments, answer, erase_jobs(c, s, pair_jobs(fields), []), value => tuple_finish(c, ty, value))
def extra(c: ctx) -> ctx:
  match c:
    case Context{check, slots, self}: Context{check, Extra{} <> slots, self}
def pair_lets(+tid: K.tid, x: String, second: String, +keep1: Bool, keep2: Bool, +body: K.ktm) -> K.ktm:
  Ty.inspect(F.Int63.t, K.ktm, F.choose(F.Int63.t, keep1, F.Int63.one, F.Int63.zero), +offset =>
    Ty.inspect(K.ktm, K.ktm, F.choose(K.ktm, keep2, K.KLet{second, K.KProj{tid, offset, K.KVar{offset}}, body}, body), +inner =>
      F.choose(K.ktm, keep1, K.KLet{x, K.KProj{tid, F.Int63.zero, K.KVar{F.Int63.zero}}, inner}, inner)))
def pair_branch(+c: ctx, s: state, tail: Bool, scrut: T.t, plan: Pair.branch) -> computation<answer>:
  match plan:
    case Pair.Branch{sty, +tid, q, +x, dom, +keep1, +second, cod, +keep2, target, body}:
      Ty.inspect(ctx, computation<answer>, added(added(extra(c), q, x, dom, keep1), Q.Many{}, second, cod, keep2), +c2 =>
        bind(answer, answer, term_request(c, s, False{}, Some{sty}, scrut), value =>
          with_answer(computation<answer>, value, +kscrut => +s1 =>
            map_result(answer, answer, result => with_answer(answer, result, +ktm => +s2 =>
              F.Pair2{K.KLet{"scrut", kscrut, pair_lets(tid, x, second, keep1, keep2, ktm)}, s2}), term_request(c2, s1, tail, Some{target}, body)))))
type branches_answer is Data:
  Branches{branches: List<&2, K.kbranch>, state: state}
def with_groups(s: state, +more: List<&2, K.tid>) -> state:
  match s:
    case State{n, ds, groups, b}: State{n, ds, List.append(&2, K.tid, groups, more), b}
def mu_tag(+c: ctx, s: state, plan: Mu.tag) -> computation<answer>:
  match plan:
    case Mu.Tag{+tid, +index, fields, +group}:
      map_result(arguments, answer, value => with_arguments(answer, value, terms => s1 =>
        F.Pair2{K.KTag{tid, index, terms}, with_groups(s1, group)}), erase_jobs(c, s, pair_jobs(fields), []))
def mu_context(bs: List<&2, Mu.binder>, c: ctx) -> ctx:
  match bs:
    case []: c
    case Mu.Binder{q, x, dom, keep} <> rest: mu_context(rest, added(c, q, x, dom, keep))
def mu_body(c: ctx, s: state, tail: Bool, +index: F.Int63.t, plan: Mu.branch) -> computation<F.Pair2<K.kbranch, state>>:
  match plan:
    case Mu.Branch{bs, +arity, target, body}:
      map_result(answer, F.Pair2<K.kbranch, state>, value => with_answer(F.Pair2<K.kbranch, state>, value, ktm => s1 =>
        F.Pair2{K.Branch{index, arity, ktm}, s1}), term_request(mu_context(bs, c), s, tail, Some{target}, body))
@unsafe
def mu_branches(+c: ctx, +s: state, +tail: Bool, names: List<&2, String>, +index: F.Int63.t, +source: List<&2, F.Pair2<T.addr, T.leg>>, +acc: List<&2, K.kbranch>, fam: Mu.case_plan) -> computation<branches_answer>:
  match names:
    case []: Pure{Branches{R.rev_append(K.kbranch, acc, []), s}}
    case key <> rest:
      match fam:
        case Mu.Case{+sty, +name, +family, +motive, +env, +all_names, +group}:
          bind(Mu.branch, branches_answer, lift_result(Mu.branch, Mu.branch_of(checker(c), name, family, motive, env, key, source)), plan =>
            bind(F.Pair2<K.kbranch, state>, branches_answer, mu_body(c, s, tail, index, plan), value =>
              Ty.pair(K.kbranch, state, computation<branches_answer>, value, branch => s1 =>
                mu_branches(c, s1, tail, rest, F.Int63.succ(index), source, branch <> acc,
                  Mu.Case{sty, name, family, motive, env, all_names, group}))))
def mu_finish(+n: String, +scrut: K.ktm, +group: List<&2, K.tid>, result: branches_answer) -> answer:
  match result:
    case Branches{bs, s}: F.Pair2{K.KCase{Ty.mu_tid(n), scrut, bs}, with_groups(s, group)}
def mu_case(+c: ctx, s: state, +tail: Bool, scrut: T.t, +branches: List<&2, F.Pair2<T.addr, T.leg>>, plan: Mu.case_plan) -> computation<answer>:
  match plan:
    case Mu.Case{+sty, +n, +fam, +mo, +env, +names, +group}:
      bind(answer, answer, term_request(c, s, False{}, Some{sty}, scrut), value =>
        with_answer(computation<answer>, value, +kscrut => s1 =>
          map_result(branches_answer, answer, result => mu_finish(n, kscrut, group, result),
            mu_branches(c, s1, tail, names, F.Int63.zero, branches, [], Mu.Case{sty, n, fam, mo, env, names, group}))))
def injection_shape(+c: ctx, +s: state, +ty: V.t, shape: S.t<T.t>, address: T.addr, args: List<&2, T.t>) -> computation<answer>:
  match shape:
    case S.SColl{_}: bind(Sum.tag, answer, lift_result(Sum.tag, Sum.injection(checker(c), ty, address, args)), plan => tag_plan(c, s, plan))
    case S.SPi{_, _, _}: bind(List<&2, Pair.field>, answer, lift_result(List<&2, Pair.field>, Pair.introduction(checker(c), ty, address, args)), fields => pair_fields(c, s, ty, fields))
    case S.SPar{_, _}: Abort{E.Not_yet{"parallel shapes in type-directed erasure"}}
    case S.SMu{_, _}: bind(Mu.tag, answer, lift_result(Mu.tag, Mu.injection(checker(c), ty, address, args)), plan => mu_tag(c, s, plan))
    case S.SNu{_, _}: Abort{E.Not_yet{"coinductive shapes in type-directed erasure"}}
def branch_body(c: ctx, s: state, tail: Bool, +index: F.Int63.t, value: Sum.branch) -> computation<F.Pair2<K.kbranch, state>>:
  match value:
    case Sum.Branch{q, x, dom, +keep, target, body}:
      map_result(answer, F.Pair2<K.kbranch, state>, +pair => with_answer(F.Pair2<K.kbranch, state>, pair, +ktm => +s1 =>
        F.Pair2{K.Branch{index, F.choose(F.Int63.t, keep, F.Int63.one, F.Int63.zero), ktm}, s1}), term_request(added(c, q, x, dom, keep), s, tail, Some{target}, body))
@unsafe
def case_branches(+c: ctx, +s: state, +tail: Bool, +ty: V.t, +motive: Maybe<&2, T.motive>, +tys: List<&2, V.t>, remaining: List<&2, V.t>, +index: F.Int63.t, +source: List<&2, F.Pair2<T.addr, T.leg>>, +acc: List<&2, K.kbranch>) -> computation<branches_answer>:
  match remaining:
    case []: Pure{Branches{R.rev_append(K.kbranch, acc, []), s}}
    case _ <> rest:
      bind(Sum.branch, branches_answer, lift_result(Sum.branch, Sum.branch_of(checker(c), ty, motive, tys, index, source)), plan =>
        bind(F.Pair2<K.kbranch, state>, branches_answer, branch_body(c, s, tail, index, plan), value =>
          Ty.pair(K.kbranch, state, computation<branches_answer>, value, +branch => +s1 =>
            case_branches(c, s1, tail, ty, motive, tys, rest, F.Int63.succ(index), source, branch <> acc))))
def case_finish(+tid: K.tid, +scrut: K.ktm, value: branches_answer) -> answer:
  match value:
    case Branches{branches, s}: F.Pair2{K.KCase{tid, scrut, branches}, s}
def case_plan(+c: ctx, s: state, +tail: Bool, +ty: V.t, scrut: T.t, +motive: Maybe<&2, T.motive>, +branches: List<&2, F.Pair2<T.addr, T.leg>>, plan: Sum.case_plan) -> computation<answer>:
  match plan:
    case Sum.Case{sty, +tid, +tys}:
      bind(answer, answer, term_request(c, s, False{}, Some{sty}, scrut), +pair => with_answer(computation<answer>, pair, +kscrut => +s1 =>
        map_result(branches_answer, answer, value => case_finish(tid, kscrut, value), case_branches(c, s1, tail, ty, motive, tys, tys, F.Int63.zero, branches, []))))
def elimination_shape(+c: ctx, +s: state, +tail: Bool, +ty: V.t, shape: S.t<T.t>, +scrut: T.t, motive: Maybe<&2, T.motive>, branches: List<&2, F.Pair2<T.addr, T.leg>>) -> computation<answer>:
  match shape:
    case S.SColl{_}: bind(Sum.case_plan, answer, lift_result(Sum.case_plan, Sum.case_of(checker(c), scrut)), plan => case_plan(c, s, tail, ty, scrut, motive, branches, plan))
    case S.SPi{q, x, dom}: bind(Pair.branch, answer, lift_result(Pair.branch, Pair.elimination(checker(c), ty, S.SPi{q, x, dom}, scrut, motive, branches)), plan => pair_branch(c, s, tail, scrut, plan))
    case S.SPar{_, _}: Abort{E.Not_yet{"parallel shapes in type-directed erasure"}}
    case S.SMu{_, _}: bind(Mu.case_plan, answer, lift_result(Mu.case_plan, Mu.case_of(checker(c), scrut, motive)), plan => mu_case(c, s, tail, scrut, branches, plan))
    case S.SNu{_, _}: Abort{E.Not_yet{"coinductive shapes in type-directed erasure"}}
def elimination(c: ctx, s: state, tail: Bool, ty: V.t, value: T.elim) -> computation<answer>:
  match value:
    case T.Elimination{shape, scrut, _, motive, branches}: elimination_shape(c, s, tail, ty, shape, scrut, motive, branches)
def node(+c: ctx, +s: state, +tail: Bool, +ty: V.t, +tm: T.t) -> computation<answer>:
  match tm:
    case T.Var{ix}: variable(c, s, ix)
    case T.Univ{_}: Pure{F.Pair2{K.KErased{}, s}}
    case T.Lan{_, _}: Pure{F.Pair2{K.KErased{}, s}}
    case T.Ran{_, _}: Pure{F.Pair2{K.KErased{}, s}}
    case T.Global{n}: Pure{F.Pair2{K.KGlobal{n}, s}}
    case T.Lit{lit}: Pure{F.Pair2{K.KLit{lit}, s}}
    case T.Ann{+body, _}: term_request(c, s, tail, Some{ty}, body)
    case T.Let{x, lty, value, +body}: let_term(c, s, tail, ty, x, lty, value, body)
    case T.Sec{shape, legs}: section(c, s, ty, shape, legs)
    case T.Out{shape, address, head}: application_shape(c, s, tail, ty, shape, address, head)
    case T.In{shape, address, args}: injection_shape(c, s, ty, shape, address, args)
    case T.Elim{elim}: elimination(c, s, tail, ty, elim)
    case T.Auto{}: Abort{E.Not_yet{R.auto_word}}
def term_step(+c: ctx, s: state, tail: Bool, expected: Maybe<&2, V.t>, +tm: T.t) -> computation<answer>:
  bind(state, answer, tick(s), +s1 =>
    bind(V.t, answer, inferred(c, expected, tm), +ty =>
      bind(Bool, answer, lift_result(Bool, Ty.runtime_ty(checker(c), ty)), +keep => Ty.boolean(computation<answer>, keep, _ =>
node(c, s1, tail, ty, tm), _ =>
Pure{F.Pair2{K.KErased{}, s1}}))))
def dispatch(request: task) -> computation<output>:
  match request:
    case TermTask{c, s, tail, expected, tm}:
      map_result(answer, output, value => TermOutput{value}, term_step(c, s, tail, expected, tm))
    case ChainTask{c, s, params, ty, tm}:
      map_result(chain_result, output, value => ChainOutput{value}, chain_step(c, s, params, ty, tm))
    case JobsTask{c, ty, args, acc, b}:
      map_result(List<&2, job>, output, value => JobsOutput{value}, jobs_step(c, ty, args, acc, b))
@unsafe
def drive(-A: Data, value: computation<A>) -> R.result(A):
  match value:
    case Pure{x}: Done{x}
    case Abort{error}: Fail{error}
    case Request{job, resume}:
      R.bind(output, A, drive(output, dispatch(job)), answer => drive(A, resume(answer)))
def term(c: ctx, s: state, tail: Bool, expected: Maybe<&2, V.t>, tm: T.t) -> R.result(answer):
  drive(answer, term_request(c, s, tail, expected, tm))
def chain(c: ctx, s: state, params: List<&2, K.repr>, ty: V.t, tm: T.t) -> R.result(chain_result):
  drive(chain_result, chain_request(c, s, params, ty, tm))
def declaration(g: G.t, b: B.t, name: String, en: G.entry) -> R.result(entry):
  drive(entry, declaration_program(g, b, name, en))
def program(g: G.t, b: B.t, entries: List<&2, F.Pair2<String, G.entry>>) -> R.result(rows):
  drive(rows, program_loop(g, b, entries, []))
