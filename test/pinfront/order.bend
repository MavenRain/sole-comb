# Structural certificates and translation at Kanon 69f3be51 lib/order.ml.
import Base
import ../../lib/foundation.bend as F
import ../../lib/kernel_error.bend as E
import ../../lib/kernel_quantity.bend as Q
import ../../lib/kernel_term.bend as T
import ../../lib/kernel_shape.bend as S
import ../../lib/kernel_rules.bend as R

type rule is Data:
  Structural{}
type status is Data:
  Principal{}
  Smaller{}
  Other{}
type step is Data:
  Step{scrut: F.Int63.t, ctor: Maybe<&2, String>}
type call is Data:
  Call{callee: String, chain: List<&2, step>}
type row is Data:
  Row{member: String, calls: List<&2, call>}
type t is Data:
  Certificate{group: List<&2, String>, arg: F.Int63.t, rows: List<&2, row>}
type peeled is Data:
  Peeled{name: String, formals: Nat, body: T.t}
type job is Data:
  Visit{statuses: List<&2, status>, chain: List<&2, step>, term: T.t}
type work is Data:
  Work{calls: List<&2, call>, jobs: List<&2, job>}
type spine_view is Data:
  Spine{head: T.t, args: List<&2, T.t>, shapes: List<&2, S.t<T.t>>}

def index(n: Nat) -> F.Int63.t: F.Int63.raw(F.Bignum.from_nat(n))
def maybe(-A: Data, -D: Type, value: Maybe<&2, A>, no: Unit -> D, yes: A -> D) -> D:
  match value:
    case None{}: no(Unit{})
    case Some{x}: yes(x)
def choose(-A: Type, flag: Bool, yes: Unit -> A, no: Unit -> A) -> A:
  match flag:
    case True{}: yes(Unit{})
    case False{}: no(Unit{})
def mentions(group: List<&2, String>, tm: T.t) -> Bool: T.exists_name(False{}, group, tm)
def as_var(tm: T.t) -> Maybe<&2, F.Int63.t>:
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
def lambda_leg(value: Maybe<&2, T.leg>) -> Maybe<&2, T.t>:
  match value:
    case None{}: None{}
    case Some{T.Leg{bs, body}}:
      maybe(F.Pair2<Q.t, String>, Maybe<&2, T.t>, R.one_of(F.Pair2<Q.t, String>, bs), _ => None{}, _ => Some{body})
def as_lam(tm: T.t) -> Maybe<&2, T.t>:
  match tm:
    case T.Sec{s, legs}:
      maybe(T.t, Maybe<&2, T.t>, S.point_dom(T.t, s), _ => None{}, _ => lambda_leg(R.one_of(T.leg, legs)))
    case T.Var{_}: None{}
    case T.Univ{_}: None{}
    case T.Lan{_, _}: None{}
    case T.Ran{_, _}: None{}
    case T.In{_, _, _}: None{}
    case T.Elim{_}: None{}
    case T.Out{_, _, _}: None{}
    case T.Let{_, _, _, _}: None{}
    case T.Ann{_, _}: None{}
    case T.Global{_}: None{}
    case T.Lit{_}: None{}
    case T.Auto{}: None{}
# The view returns the strict body subterm of a single-binder section.
@unsafe
def peel(+name: String, +n: Nat, +tm: T.t) -> peeled:
  maybe(T.t, peeled, as_lam(tm), _ => Peeled{name, n, tm}, body => peel(name, 1n+n, body))
# Each point projection removes one Out node from the application spine.
@unsafe
def spine(tm: T.t, args: List<&2, T.t>, shapes: List<&2, S.t<T.t>>) -> spine_view:
  match tm:
    case T.Out{s, a, hd}:
      match a:
        case T.APt{_, arg}: spine(hd, arg <> args, s <> shapes)
        case T.ALeg{k}: Spine{T.Out{s, T.ALeg{k}, hd}, args, shapes}
        case T.ACtor{n}: Spine{T.Out{s, T.ACtor{n}, hd}, args, shapes}
    case T.Var{ix}: Spine{T.Var{ix}, args, shapes}
    case T.Univ{u}: Spine{T.Univ{u}, args, shapes}
    case T.Lan{s, d}: Spine{T.Lan{s, d}, args, shapes}
    case T.Ran{s, d}: Spine{T.Ran{s, d}, args, shapes}
    case T.In{s, a, xs}: Spine{T.In{s, a, xs}, args, shapes}
    case T.Elim{e}: Spine{T.Elim{e}, args, shapes}
    case T.Sec{s, ls}: Spine{T.Sec{s, ls}, args, shapes}
    case T.Let{x, ty, d, b}: Spine{T.Let{x, ty, d, b}, args, shapes}
    case T.Ann{t, ty}: Spine{T.Ann{t, ty}, args, shapes}
    case T.Global{g}: Spine{T.Global{g}, args, shapes}
    case T.Lit{l}: Spine{T.Lit{l}, args, shapes}
    case T.Auto{}: Spine{T.Auto{}, args, shapes}
def under(-A: Data, +s: status, bs: List<&2, A>, st: List<&2, status>) -> List<&2, status>:
  match bs:
    case []: st
    case _ <> rest: under(A, s, rest, s <> st)
def under_point(s: S.t<T.t>, +st: List<&2, status>) -> List<&2, status>:
  maybe(T.t, List<&2, status>, S.point_dom(T.t, s), _ => st, _ => Other{} <> st)
def smaller(value: Maybe<&2, status>) -> Bool:
  match value:
    case None{}: False{}
    case Some{Smaller{}}: True{}
    case Some{Principal{}}: False{}
    case Some{Other{}}: False{}
def special(value: Maybe<&2, status>) -> Bool:
  match value:
    case None{}: False{}
    case Some{Smaller{}}: True{}
    case Some{Principal{}}: True{}
    case Some{Other{}}: False{}
def guarded_call(k: F.Int63.t, st: List<&2, status>, args: List<&2, T.t>) -> Bool:
  maybe(T.t, Bool, R.at(T.t, k, args), _ => False{}, tm =>
    maybe(F.Int63.t, Bool, as_var(tm), _ => False{}, ix => smaller(R.at(status, ix, st))))
def terms(+st: List<&2, status>, +chain: List<&2, step>, ts: List<&2, T.t>) -> List<&2, job>:
  match ts:
    case []: []
    case tm <> rest: Visit{st, chain, tm} <> terms(st, chain, rest)
def shape(st: List<&2, status>, chain: List<&2, step>, s: S.t<T.t>) -> List<&2, job>: terms(st, chain, S.payload(T.t, s))
def shapes(+st: List<&2, status>, +chain: List<&2, step>, ss: List<&2, S.t<T.t>>) -> List<&2, job>:
  match ss:
    case []: []
    case s <> rest: List.append(&2, job, shape(st, chain, s), shapes(st, chain, rest))
def address(st: List<&2, status>, chain: List<&2, step>, a: T.addr) -> List<&2, job>:
  match a:
    case T.APt{_, tm}: [Visit{st, chain, tm}]
    case T.ALeg{_}: []
    case T.ACtor{_}: []
def legs(+st: List<&2, status>, +chain: List<&2, step>, ls: List<&2, T.leg>) -> List<&2, job>:
  match ls:
    case []: []
    case T.Leg{bs, body} <> rest:
      Visit{under(F.Pair2<Q.t, String>, Other{}, bs, st), chain, body} <> legs(st, chain, rest)
def motive(st: List<&2, status>, chain: List<&2, step>, mo: Maybe<&2, T.motive>) -> List<&2, job>:
  match mo:
    case None{}: []
    case Some{T.Motive{_, ix, _, body}}: [Visit{under(String, Other{}, ix, Other{} <> st), chain, body}]
def chain_at(st: List<&2, status>, +chain: List<&2, step>, sv: Maybe<&2, F.Int63.t>, a: T.addr) -> List<&2, step>:
  match sv:
    case None{}: chain
    case Some{+ix}: F.choose(List<&2, step>, special(R.at(status, ix, st)), Step{ix, T.as_actor(a)} <> chain, chain)
def branch_status(st: List<&2, status>, sv: Maybe<&2, F.Int63.t>) -> status:
  match sv:
    case None{}: Other{}
    case Some{ix}: F.choose(status, special(R.at(status, ix, st)), Smaller{}, Other{})
def branches(+st: List<&2, status>, +chain: List<&2, step>, +sv: Maybe<&2, F.Int63.t>, bs: List<&2, F.Pair2<T.addr, T.leg>>) -> List<&2, job>:
  match bs:
    case []: []
    case F.Pair2{+a, T.Leg{binders, body}} <> rest:
      List.append(&2, job, address(st, chain, a), Visit{under(F.Pair2<Q.t, String>, branch_status(st, sv), binders, st), chain_at(st, chain, sv, a), body} <> branches(st, chain, sv, rest))
def projection(+st: List<&2, status>, +chain: List<&2, step>, s: S.t<T.t>, a: T.addr, hd: T.t) -> List<&2, job>:
  List.append(&2, job, shape(st, chain, s), List.append(&2, job, address(st, chain, a), [Visit{st, chain, hd}]))
def spine_head(group: List<&2, String>, k: F.Int63.t, st: List<&2, status>, chain: List<&2, step>, args: List<&2, T.t>, hd: T.t) -> Maybe<&2, work>:
  match hd:
    case T.Global{+g}:
      choose(Maybe<&2, work>, T.member(g, group), _ =>
        F.choose(Maybe<&2, work>, guarded_call(k, st, args), Some{Work{[Call{g, chain}], []}}, None{}), _ => Some{Work{[], []}})
    case T.Var{_}: Some{Work{[], []}}
    case T.Univ{_}: Some{Work{[], []}}
    case T.Lit{_}: Some{Work{[], []}}
    case T.Auto{}: Some{Work{[], []}}
    case T.Out{s, a, h}: Some{Work{[], projection(st, chain, s, a, h)}}
    case T.Lan{s, d}: Some{Work{[], [Visit{st, chain, T.Lan{s, d}}]}}
    case T.Ran{s, d}: Some{Work{[], [Visit{st, chain, T.Ran{s, d}}]}}
    case T.In{s, a, xs}: Some{Work{[], [Visit{st, chain, T.In{s, a, xs}}]}}
    case T.Elim{e}: Some{Work{[], [Visit{st, chain, T.Elim{e}}]}}
    case T.Sec{s, ls}: Some{Work{[], [Visit{st, chain, T.Sec{s, ls}}]}}
    case T.Let{x, ty, d, b}: Some{Work{[], [Visit{st, chain, T.Let{x, ty, d, b}}]}}
    case T.Ann{t, ty}: Some{Work{[], [Visit{st, chain, T.Ann{t, ty}}]}}
def extend(value: Maybe<&2, work>, more: List<&2, job>) -> Maybe<&2, work>:
  match value:
    case None{}: None{}
    case Some{Work{cs, js}}: Some{Work{cs, List.append(&2, job, js, more)}}
def spine_jobs(group: List<&2, String>, k: F.Int63.t, +st: List<&2, status>, +chain: List<&2, step>, view: spine_view) -> Maybe<&2, work>:
  match view:
    case Spine{hd, +args, ss}: extend(spine_head(group, k, st, chain, args, hd), List.append(&2, job, shapes(st, chain, ss), terms(st, chain, args)))
def node(group: List<&2, String>, k: F.Int63.t, task: job) -> Maybe<&2, work>:
  match task:
    case Visit{+st, +chain, tm}:
      match tm:
        case T.Var{_}: Some{Work{[], []}}
        case T.Univ{_}: Some{Work{[], []}}
        case T.Lit{_}: Some{Work{[], []}}
        case T.Auto{}: Some{Work{[], []}}
        case T.Global{g}: F.choose(Maybe<&2, work>, T.member(g, group), None{}, Some{Work{[], []}})
        case T.Lan{+s, d}: Some{Work{[], List.append(&2, job, shape(st, chain, s), [Visit{under_point(s, st), chain, d}])}}
        case T.Ran{+s, d}: Some{Work{[], List.append(&2, job, shape(st, chain, s), [Visit{under_point(s, st), chain, d}])}}
        case T.In{s, a, xs}: Some{Work{[], List.append(&2, job, shape(st, chain, s), List.append(&2, job, address(st, chain, a), terms(st, chain, xs)))}}
        case T.Sec{s, ls}: Some{Work{[], List.append(&2, job, shape(st, chain, s), legs(st, chain, ls))}}
        case T.Out{s, a, hd}: spine_jobs(group, k, st, chain, spine(T.Out{s, a, hd}, [], []))
        case T.Elim{T.Elimination{s, +scrut, _, mo, bs}}:
          Some{Work{[], Visit{st, chain, scrut} <> List.append(&2, job, shape(st, chain, s), List.append(&2, job, motive(st, chain, mo), branches(st, chain, as_var(scrut), bs)))}}
        case T.Let{_, ty, d, b}: Some{Work{[], [Visit{st, chain, ty}, Visit{st, chain, d}, Visit{Other{} <> st, chain, b}]}}
        case T.Ann{tm, ty}: Some{Work{[], [Visit{st, chain, tm}, Visit{st, chain, ty}]}}
# Each expansion visits strict subterms. A stopped non-point Out is expanded once.
def next_work(cs: List<&2, call>, rest: List<&2, job>, next: work) -> work:
  match next:
    case Work{more, jobs}: Work{List.append(&2, call, cs, more), List.append(&2, job, jobs, rest)}
@unsafe
def walk(+group: List<&2, String>, +k: F.Int63.t, state: work) -> Maybe<&2, List<&2, call>>:
  match state:
    case Work{cs, []}: Some{cs}
    case Work{cs, j <> rest}:
      maybe(work, Maybe<&2, List<&2, call>>, node(group, k, j), _ => None{}, next => walk(group, k, next_work(cs, rest, next)))
def seed(n: Nat, +principal: F.Int63.t, +ix: F.Int63.t) -> List<&2, status>:
  match n:
    case 0n: []
    case 1n+rest: F.choose(status, F.Int63.equal(ix, principal), Principal{}, Other{}) <> seed(rest, principal, F.Int63.succ(ix))
def passes(rule: rule, group: List<&2, String>, +k: F.Int63.t, +formals: Nat, body: T.t) -> Maybe<&2, List<&2, call>>:
  match rule:
    case Structural{}: walk(group, k, Work{[], [Visit{seed(formals, F.Int63.sub(F.Int63.pred(index(formals)), k), F.Int63.zero), [], body}]})
def names(ms: List<&2, F.Pair2<String, T.t>>) -> List<&2, String>:
  match ms:
    case []: []
    case F.Pair2{n, _} <> rest: n <> names(rest)
def peel_members(ms: List<&2, F.Pair2<String, T.t>>) -> List<&2, peeled>:
  match ms:
    case []: []
    case F.Pair2{n, b} <> rest: peel(n, 0n, b) <> peel_members(rest)
def any_calls(+group: List<&2, String>, ms: List<&2, F.Pair2<String, T.t>>) -> Bool:
  match ms:
    case []: False{}
    case F.Pair2{_, b} <> rest:
      choose(Bool, mentions(group, b), _ => True{}, _ => any_calls(group, rest))
def minimum(+n: Nat, prev: Maybe<&2, Nat>) -> Maybe<&2, Nat>:
  match prev:
    case None{}: Some{n}
    case Some{+old}: Some{F.choose(Nat, Nat.is_lt(n, old), n, old)}
def bound(+group: List<&2, String>, ms: List<&2, peeled>, +acc: Maybe<&2, Nat>) -> Nat:
  match ms:
    case []:
      match acc:
        case None{}: 0n
        case Some{n}: n
    case Peeled{_, n, b} <> +rest:
      choose(Nat, mentions(group, b), _ => bound(group, rest, minimum(n, acc)), _ => bound(group, rest, acc))
def rows_at(+rule: rule, +group: List<&2, String>, +k: F.Int63.t, ms: List<&2, peeled>) -> Maybe<&2, List<&2, row>>:
  match ms:
    case []: Some{[]}
    case Peeled{n, formals, b} <> rest:
      maybe(List<&2, call>, Maybe<&2, List<&2, row>>, passes(rule, group, k, formals, b), _ => None{}, cs =>
        maybe(List<&2, row>, Maybe<&2, List<&2, row>>, rows_at(rule, group, k, rest), _ => None{}, rs => Some{Row{n, cs} <> rs}))
def refusal(ms: List<&2, F.Pair2<String, T.t>>) -> E.t:
  match ms:
    case []: E.Termination{""}
    case F.Pair2{n, _} <> _: E.Termination{n}
def first_fit(remaining: Nat, +rule: rule, +group: List<&2, String>, +ms: List<&2, peeled>, +k: F.Int63.t, error: E.t) -> R.result(t):
  match remaining:
    case 0n: Fail{error}
    case 1n+rest:
      maybe(List<&2, row>, R.result(t), rows_at(rule, group, k, ms), _ => first_fit(rest, rule, group, ms, F.Int63.succ(k), error), rs => Done{Certificate{group, k, rs}})
def certify_peeled(rule: rule, +group: List<&2, String>, +ps: List<&2, peeled>, error: E.t) -> R.result(Maybe<&2, t>):
  R.map_result(t, Maybe<&2, t>, c => Some{c}, first_fit(bound(group, ps, None{}), rule, group, ps, F.Int63.zero, error))
def certify_named(rule: rule, +group: List<&2, String>, +ms: List<&2, F.Pair2<String, T.t>>) -> R.result(Maybe<&2, t>):
  choose(R.result(Maybe<&2, t>), any_calls(group, ms), _ => certify_peeled(rule, group, peel_members(ms), refusal(ms)), _ => Done{None{}})
def certify(rule: rule, +ms: List<&2, F.Pair2<String, T.t>>) -> R.result(Maybe<&2, t>):
  certify_named(rule, names(ms), ms)
def one_elim_msg() -> String: "a structurally recursive definition eliminates its recursive argument at the head of its body"
def motive_msg() -> String: "the elimination of a structurally recursive definition carries its motive"
def ctor_key_msg() -> String: "every branch of the elimination of a structurally recursive definition keys a constructor"
def ctor_keys(bs: List<&2, F.Pair2<T.addr, T.leg>>) -> R.result(Unit):
  match bs:
    case []: Done{Unit{}}
    case F.Pair2{a, _} <> rest:
      maybe(String, R.result(Unit), T.as_actor(a), _ => Fail{E.Wrong_leg{ctor_key_msg}}, _ => ctor_keys(rest))
def call_chains(+name: String, cs: List<&2, call>) -> R.result(Unit):
  match cs:
    case []: Done{Unit{}}
    case Call{_, chain} <> rest:
      match chain:
        case []: Fail{E.Termination{name}}
        case Step{_, ctor} <> _:
          match ctor:
            case None{}: Fail{E.Termination{name}}
            case Some{_}: call_chains(name, rest)
def row_chains(rs: List<&2, row>) -> R.result(Unit):
  match rs:
    case []: Done{Unit{}}
    case Row{n, cs} <> rest: R.bind(Unit, Unit, call_chains(n, cs), _ => row_chains(rest))
def translated_motive(mo: Maybe<&2, T.motive>, bs: List<&2, F.Pair2<T.addr, T.leg>>, rs: List<&2, row>) -> R.result(Unit):
  match mo:
    case None{}: Fail{E.Missing_branch{motive_msg}}
    case Some{_}: R.bind(Unit, Unit, ctor_keys(bs), _ => row_chains(rs))
def translated_elim(k: F.Int63.t, rs: List<&2, row>, n: Nat, e: T.elim) -> R.result(Unit):
  match e:
    case T.Elimination{_, scrut, _, mo, bs}:
      maybe(F.Int63.t, R.result(Unit), as_var(scrut), _ => Fail{E.Mismatch{one_elim_msg}}, ix =>
        choose(R.result(Unit), F.Int63.equal(ix, F.Int63.sub(F.Int63.pred(index(n)), k)), _ => translated_motive(mo, bs, rs), _ => Fail{E.Mismatch{one_elim_msg}}))
def translated_inner(k: F.Int63.t, rs: List<&2, row>, p: peeled) -> R.result(Unit):
  match p:
    case Peeled{_, n, tm}:
      match tm:
        case T.Elim{e}: translated_elim(k, rs, n, e)
        case T.Var{_}: Fail{E.Mismatch{one_elim_msg}}
        case T.Univ{_}: Fail{E.Mismatch{one_elim_msg}}
        case T.Lan{_, _}: Fail{E.Mismatch{one_elim_msg}}
        case T.Ran{_, _}: Fail{E.Mismatch{one_elim_msg}}
        case T.In{_, _, _}: Fail{E.Mismatch{one_elim_msg}}
        case T.Sec{_, _}: Fail{E.Mismatch{one_elim_msg}}
        case T.Out{_, _, _}: Fail{E.Mismatch{one_elim_msg}}
        case T.Let{_, _, _, _}: Fail{E.Mismatch{one_elim_msg}}
        case T.Ann{_, _}: Fail{E.Mismatch{one_elim_msg}}
        case T.Global{_}: Fail{E.Mismatch{one_elim_msg}}
        case T.Lit{_}: Fail{E.Mismatch{one_elim_msg}}
        case T.Auto{}: Fail{E.Mismatch{one_elim_msg}}
def translate(cert: t, +body: T.t) -> R.result(T.t):
  match cert:
    case Certificate{group, k, rs}:
      choose(R.result(T.t), mentions(group, body), _ => R.map_result(Unit, T.t, _ => body, translated_inner(k, rs, peel("", 0n, body))), _ => Done{body})
