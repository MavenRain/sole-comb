# Full structural observations for the generated A.3 cases.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_error.bend as E
import ../lib/kernel_level.bend as L
import ../lib/kernel_literal.bend as Lit
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_shape.bend as S
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import ../lib/kernel_global.bend as G
import ../lib/kernel_positivity.bend as Pos
import ../lib/kernel_rules.bend as R
import ../lib/kernel_eval.bend as EV
import ../lib/kernel_conv.bend as CV
import ./representation.bend as RT

type Item is Data:
  Text{text: String}
  Term{term: T.t}
  Terms{terms: List<&2, T.t>}
  Shape{shape: S.t<T.t>}
  Addr{address: T.addr}
  Leg{leg: T.leg}
  Legs{legs: List<&2, T.leg>}
  Binders{binders: List<&2, F.Pair2<Q.t, String>>}
  Motive{motive: Maybe<&2, T.motive>}
  Branches{branches: List<&2, F.Pair2<T.addr, T.leg>>}

def quantity(q: Q.t) -> String:
  match q:
    case Q.Zero{}: "0"
    case Q.One{}: "1"
    case Q.Many{}: "w"

def strings(items: List<&2, String>) -> String:
  match items:
    case []: ""
    case x <> xs: String.append(x, String.append(";", strings(xs)))

def expand(item: Item) -> List<&2, Item>:
  match item:
    case Text{s}: [Text{s}]
    case Term{T.Var{i}}: [Text{String.append("v", F.Int63.show(i))}]
    case Term{T.Univ{l}}: [Text{String.append("u", F.Bignum.to_string(l))}]
    case Term{T.Global{n}}: [Text{String.append("g:", n)}]
    case Term{T.Lit{l}}: [Text{String.append("lit:", RT.literal(l))}]
    case Term{T.Auto{}}: [Text{"auto"}]
    case Term{T.Lan{s, d}}: [Text{"lan("}, Shape{s}, Text{","}, Term{d}, Text{")"}]
    case Term{T.Ran{s, d}}: [Text{"ran("}, Shape{s}, Text{","}, Term{d}, Text{")"}]
    case Term{T.In{s, a, xs}}: [Text{"in("}, Shape{s}, Text{","}, Addr{a}, Text{",["}, Terms{xs}, Text{"])"}]
    case Term{T.Sec{s, ls}}: [Text{"sec("}, Shape{s}, Text{",["}, Legs{ls}, Text{"])"}]
    case Term{T.Out{s, a, v}}: [Text{"out("}, Shape{s}, Text{","}, Addr{a}, Text{","}, Term{v}, Text{")"}]
    case Term{T.Let{n, ty, v, b}}: [Text{String.append("let(", n)}, Text{","}, Term{ty}, Text{","}, Term{v}, Text{","}, Term{b}, Text{")"}]
    case Term{T.Ann{t, ty}}: [Text{"ann("}, Term{t}, Text{","}, Term{ty}, Text{")"}]
    case Term{T.Elim{T.Elimination{s, v, q, mo, bs}}}:
      [Text{"elim("}, Shape{s}, Text{","}, Term{v}, Text{","}, Text{quantity(q)}, Text{","}, Motive{mo}, Text{",["}, Branches{bs}, Text{"])"}]
    case Terms{[]}: []
    case Terms{t <> ts}: [Term{t}, Text{";"}, Terms{ts}]
    case Shape{S.SPi{q, n, a}}: [Text{String.append("pi(", quantity(q))}, Text{","}, Text{n}, Text{","}, Term{a}, Text{")"}]
    case Shape{S.SColl{n}}: [Text{String.append("coll", F.Int63.show(n))}]
    case Shape{S.SPar{a, b}}: [Text{"par("}, Term{a}, Text{","}, Term{b}, Text{")"}]
    case Shape{S.SMu{n, xs}}: [Text{String.append("mu(", n)}, Text{",["}, Terms{xs}, Text{"])"}]
    case Shape{S.SNu{n, xs}}: [Text{String.append("nu(", n)}, Text{",["}, Terms{xs}, Text{"])"}]
    case Addr{T.APt{q, a}}: [Text{String.append("pt(", quantity(q))}, Text{","}, Term{a}, Text{")"}]
    case Addr{T.ALeg{i}}: [Text{String.append("leg", F.Int63.show(i))}]
    case Addr{T.ACtor{n}}: [Text{String.append("ctor:", n)}]
    case Leg{T.Leg{bs, b}}: [Text{"(["}, Binders{bs}, Text{"],"}, Term{b}, Text{")"}]
    case Legs{[]}: []
    case Legs{l <> ls}: [Leg{l}, Text{";"}, Legs{ls}]
    case Binders{[]}: []
    case Binders{F.Pair2{q, n} <> bs}: [Text{quantity(q)}, Text{":"}, Text{n}, Text{";"}, Binders{bs}]
    case Motive{None{}}: [Text{"none"}]
    case Motive{Some{T.Motive{ind, idx, self, b}}}:
      [Text{"some("}, Text{RT.opt_text(ind)}, Text{",["}, Text{strings(idx)}, Text{"],"}, Text{self}, Text{","}, Term{b}, Text{")"}]
    case Branches{[]}: []
    case Branches{F.Pair2{a, l} <> bs}: [Addr{a}, Text{":"}, Leg{l}, Text{";"}, Branches{bs}]

# The worklist expands strict subterms of finite syntax; no kernel operation is used.
@unsafe
def render_items(items: List<&2, Item>) -> String:
  match items:
    case []: ""
    case Text{s} <> rest: String.append(s, render_items(rest))
    case item <> rest: render_items(List.append(&2, Item, expand(item), rest))

def render(term: T.t) -> String: render_items([Term{term}])

def term_result(answer: R.result(T.t)) -> String:
  match answer:
    case Done{t}: render(t)
    case Fail{e}: String.append("error:", E.to_string(e))

def bool_result(answer: R.result(Bool)) -> String:
  match answer:
    case Done{True{}}: "true"
    case Done{False{}}: "false"
    case Fail{e}: String.append("error:", E.to_string(e))

def nf(+globals: G.t, +size: F.Int63.t, env: List<&2, V.t>, term: T.t) -> String:
  term_result(R.bind(V.t, T.t, EV.eval(globals, env, term), v => EV.quote(globals, size, v)))

def weak(+globals: G.t, +size: F.Int63.t, value: V.t) -> String:
  term_result(R.bind(V.t, T.t, EV.whnf(globals, value), v => EV.quote(globals, size, v)))

type Context is Data:
  Context{globals: G.t, size: F.Int63.t, env: List<&2, V.t>, head_ty: V.t, probe: Nat, exhausted: Bool}

def context(globals: G.t, ty: V.t, probe: Nat, exhausted: Bool) -> Context:
  Context{globals, F.Int63.zero, [], ty, probe, exhausted}

def guard(-A: Data, exhausted: Bool, value: R.result(A)) -> R.result(A):
  match exhausted:
    case True{}: Fail{E.Budget_exhausted{"evaluation-test"}}
    case False{}: value

def infer_probe(probe: Nat) -> R.result(L.t):
  match probe:
    case 0n: Done{L.one}
    case 1n: Done{L.zero}
    case _: Fail{E.Cannot_infer{"evaluation-test"}}

def infer_univ(ctx: Context, term: T.t) -> R.result(L.t):
  match ctx:
    case Context{_, _, _, _, probe, exhausted}: guard(L.t, exhausted, infer_probe(probe))

def whnf(ctx: Context, value: V.t) -> R.result(V.t):
  match ctx:
    case Context{g, _, _, _, _, exhausted}: guard(V.t, exhausted, EV.whnf(g, value))

def bind(name: String, q: Q.t, ty: V.t, +ctx: Context) -> Context:
  match ctx:
    case Context{g, size, env, h, p, b}: Context{g, F.Int63.succ(size), V.var(size) <> env, h, p, b}

def define(name: String, q: Q.t, ty: V.t, value: V.t, +ctx: Context) -> Context:
  match ctx:
    case Context{g, size, env, h, p, b}: Context{g, F.Int63.succ(size), value <> env, h, p, b}

def size(ctx: Context) -> F.Int63.t:
  match ctx:
    case Context{_, n, _, _, _, _}: n

def env(ctx: Context) -> List<&2, V.t>:
  match ctx:
    case Context{_, _, vs, _, _, _}: vs

def eval_env(ctx: Context, env: List<&2, V.t>, term: T.t) -> R.result(V.t):
  match ctx:
    case Context{g, _, _, _, _, b}: guard(V.t, b, EV.eval(g, env, term))

def quote(ctx: Context, value: V.t) -> R.result(T.t):
  match ctx:
    case Context{g, n, _, _, _, b}: guard(T.t, b, EV.quote(g, n, value))

def head_ty(ctx: Context, head: V.head) -> R.result(V.t):
  match ctx:
    case Context{_, _, _, ty, _, b}: guard(V.t, b, Done{ty})

def family(ctx: Context, name: String) -> Maybe<&2, Pos.family>:
  match ctx:
    case Context{g, _, _, _, _, _}: G.find_family(name, g)

def families(ctx: Context) -> List<&2, F.Pair2<String, Pos.family>>:
  match ctx:
    case Context{g, _, _, _, _, _}: G.family_list(g)

def ops() -> R.ops<Context>:
  R.Ops{infer_univ, whnf, n => q => ty => c => bind(n, q, ty, c), n => q => ty => v => c => define(n, q, ty, v, c), size, env, eval_env, quote, head_ty, family, families}

def conv(ctx: Context, ty: V.t, a: V.t, b: V.t) -> String:
  bool_result(CV.conv(Context, ops, ctx, ty, a, b))

def conv_type(ctx: Context, a: V.t, b: V.t) -> String:
  bool_result(CV.conv_type(Context, ops, ctx, a, b))
