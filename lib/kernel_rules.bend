# Runtime and conversion rules at Kanon 69f3be51 rules.ml.
# The checking fields of the rule packs remain A.4 work.
import Base
import ./foundation.bend as F
import ./kernel_level.bend as L
import ./kernel_quantity.bend as Q
import ./kernel_shape.bend as S
import ./kernel_term.bend as T
import ./kernel_value.bend as V
import ./kernel_error.bend as E
import ./kernel_positivity.bend as Pos
# Conversion's checker interface remains abstract until A.4.
def placeholder() -> V.t: V.VUniv{L.zero}

def level_default(level: Maybe<&2, L.t>) -> L.t:
  match level:
    case None{}: L.zero
    case Some{l}: l

type Point is Data:
  Point{quantity: Q.t, name: String, domain: V.t}

def collection_view(shape: S.t<V.t>) -> Maybe<&2, F.Int63.t>:
  match shape:
    case S.SPi{_, _, _}: None{}
    case S.SColl{n}: Some{n}
    case S.SPar{_, _}: None{}
    case S.SMu{_, _}: None{}
    case S.SNu{_, _}: None{}

def proj_branch(q: Q.t, second: Bool) -> List<&2, F.Pair2<T.addr, T.leg>>:
  [F.Pair2{T.ALeg{F.Int63.zero}, T.Leg{[F.Pair2{q, "x"}, F.Pair2{Q.Many{}, "y"}], T.Var{F.choose(F.Int63.t, second, F.Int63.zero, F.Int63.one)}}}]

def nat_equal(a: Nat, b: Nat) -> Bool:
  match a, b:
    case Zero{}, Zero{}: True{}
    case Zero{}, Succ{_}: False{}
    case Succ{_}, Zero{}: False{}
    case Succ{x}, Succ{y}: nat_equal(x, y)

def arrow(q: Q.t, name: String, domain: T.t, codomain: T.t) -> T.t:
  T.Ran{S.SPi{q, name, domain}, codomain}

def unit_ty(level: L.t) -> T.t:
  T.Ann{T.Ran{S.SColl{F.Int63.Int{False{}, []}}, T.Sec{S.SColl{F.Int63.Int{False{}, []}}, []}}, T.Univ{level}}

def unit_val() -> T.t:
  T.Sec{S.SColl{F.Int63.Int{False{}, []}}, []}

def bool_value(b: Bool) -> V.t:
  V.VIn{S.SColl{F.Int63.Int{False{}, [2]}}, V.VALeg{F.choose(F.Int63.t, b, F.Int63.Int{False{}, [1]}, F.Int63.Int{False{}, []})}, [V.VSec{S.SColl{F.Int63.Int{False{}, []}}, []}]}

def result(-A: Data) -> Data: Result<&2, &2, E.t, A>

def one_of(-A: Data, xs: List<&2, A>) -> Maybe<&2, A>:
  match xs:
    case []: None{}
    case x <> rest:
      match rest:
        case []: Some{x}
        case _ <> _: None{}

def rev_append(-A: Data, xs: List<&2, A>, tail: List<&2, A>) -> List<&2, A>:
  match xs:
    case []: tail
    case x <> rest: rev_append(A, rest, x <> tail)

def fresh_loop(count: Nat, +size: F.Int63.t) -> List<&2, V.t>:
  match count:
    case Zero{}: []
    case Succ{n}: V.var(size) <> fresh_loop(n, F.Int63.succ(size))

def advance_loop(count: Nat, size: F.Int63.t) -> F.Int63.t:
  match count:
    case Zero{}: size
    case Succ{n}: advance_loop(n, F.Int63.succ(size))

type pack is Data:
  PiPack{}
  CollPack{}
  MuPack{}

def spar_word() -> String: "SPar arrives at M1"

def snu_word() -> String: "SNu arrives at M2"

def auto_word() -> String: "instances arrive at M2"

def mu_ran_word() -> String: "a right former at a mu shape arrives at M2"

def wrong_pack() -> String: "the rule pack does not match the shape of the term"

def stuck_msg() -> String: "an elimination met a value it cannot eliminate"

def diagram_msg() -> String: "the diagram is not a section of the declared width"

def leg_msg() -> String: "the leg address is outside the collection"

def branch_key(key: V.vaddr, address: T.addr) -> Bool:
  match key:
    case V.VAPt{_, _}: False{}
    case V.VALeg{n}:
      match address:
        case T.APt{_, _}: False{}
        case T.ALeg{m}: F.Int63.equal(n, m)
        case T.ACtor{_}: False{}
    case V.VACtor{c}:
      match address:
        case T.APt{_, _}: False{}
        case T.ALeg{_}: False{}
        case T.ACtor{d}: String.eq(c, d)

def collection_key(address: V.vaddr) -> Maybe<&2, V.vaddr>:
  match address:
    case V.VAPt{_, _}: None{}
    case V.VALeg{k}: Some{V.VALeg{k}}
    case V.VACtor{_}: None{}

def constructor_key(address: V.vaddr) -> Maybe<&2, V.vaddr>:
  match address:
    case V.VAPt{_, _}: None{}
    case V.VALeg{_}: None{}
    case V.VACtor{c}: Some{V.VACtor{c}}

type task<-C: Data> is Data:
  Evaluate{context: C, env: List<&2, V.t>, term: T.t}
  Weak{context: C, value: V.t}
  Readback{context: C, size: F.Int63.t, value: V.t}
  Compare{context: C, ty: V.t, left: V.t, right: V.t}

type answer is Data:
  ValueAnswer{value: V.t}
  TermAnswer{term: T.t}
  BoolAnswer{equal: Bool}

def choose_lazy(-A: Data, flag: Bool, yes: Unit -> A, no: Unit -> A) -> A:
  match flag:
    case True{}: yes(Unit{})
    case False{}: no(Unit{})

def ann_lvl_eq(shape: S.t<V.t>, u1: Maybe<&2, L.t>, u2: Maybe<&2, L.t>) -> Bool:
  match shape:
    case S.SPi{_, _, _}: True{}
    case S.SColl{n}: choose_lazy(Bool, F.Int63.equal(n, F.Int63.zero), _ => L.equal(level_default(u1), level_default(u2)), _ => True{})
    case S.SPar{_, _}: True{}
    case S.SMu{_, _}: True{}
    case S.SNu{_, _}: True{}

def point_view(shape: S.t<V.t>) -> Maybe<&2, Point>:
  match shape:
    case S.SPi{q, name, domain}: Some{Point{q, name, domain}}
    case S.SColl{_}: None{}
    case S.SPar{_, _}: None{}
    case S.SMu{_, _}: None{}
    case S.SNu{_, _}: None{}

def empty_section(value: V.t) -> result(List<&2, V.vleg>):
  match value:
    case V.VSec{_, legs}: Done{legs}
    case V.VUniv{_}: Fail{E.Mismatch{diagram_msg}}
    case V.VLan{_, _, _}: Fail{E.Mismatch{diagram_msg}}
    case V.VRan{_, _, _}: Fail{E.Mismatch{diagram_msg}}
    case V.VIn{_, _, _}: Fail{E.Mismatch{diagram_msg}}
    case V.VLit{_}: Fail{E.Mismatch{diagram_msg}}
    case V.VNeutral{_, _}: Fail{E.Mismatch{diagram_msg}}

def erased_fields(args: Pos.telescope) -> Bool:
  match args:
    case []: True{}
    case F.Pair2{q, _} <> rest: choose_lazy(Bool, Q.equal(q, Q.Zero{}), _ => erased_fields(rest), _ => False{})

def has_eta(p: pack, right: Bool) -> Bool:
  match p:
    case PiPack{}: True{}
    case CollPack{}: right
    case MuPack{}: False{}

type SpineStep is Data:
  SpineStep{argument: Maybe<&2, V.t>, result: V.t}

def fresh(size: F.Int63.t, count: Nat) -> List<&2, V.t>: fresh_loop(count, size)

def advance(size: F.Int63.t, count: Nat) -> F.Int63.t: advance_loop(count, size)

def bool_ty() -> T.t:
  T.Lan{S.SColl{F.Int63.Int{False{}, [2]}}, T.Sec{S.SColl{F.Int63.Int{False{}, [2]}}, [T.Leg{[], unit_ty(L.one)}, T.Leg{[], unit_ty(L.one)}]}}

def bind(-A: Data, -B: Data, value: result(A), next: A -> result(B)) -> result(B):
  match value:
    case Fail{e}: Fail{e}
    case Done{x}: next(x)

def map_result(-A: Data, -B: Data, f: A -> B, value: result(A)) -> result(B):
  match value:
    case Fail{e}: Fail{e}
    case Done{x}: Done{f(x)}

def require(-A: Data, value: Maybe<&2, A>, error: E.t) -> result(A):
  match value:
    case None{}: Fail{error}
    case Some{x}: Done{x}

def at_loop(-A: Data, xs: List<&2, A>, +index: F.Int63.t) -> Maybe<&2, A>:
  match xs:
    case []: None{}
    case x <> rest:
      choose_lazy(Maybe<&2, A>, F.Int63.equal(index, F.Int63.zero), _ => Some{x}, _ => at_loop(A, rest, F.Int63.pred(index)))

def rules(-A: Data, shape: S.t<A>) -> result(pack):
  match shape:
    case S.SPi{_, _, _}: Done{PiPack{}}
    case S.SColl{_}: Done{CollPack{}}
    case S.SPar{_, _}: Fail{E.Not_yet{spar_word}}
    case S.SMu{_, _}: Done{MuPack{}}
    case S.SNu{_, _}: Fail{E.Not_yet{snu_word}}

def diagram_arity(p: pack) -> Nat:
  match p:
    case PiPack{}: 1n
    case CollPack{}: 0n
    case MuPack{}: 0n

def branch_leg(+key: V.vaddr, branches: List<&2, F.Pair2<T.addr, T.leg>>) -> Maybe<&2, T.leg>:
  match branches:
    case []: None{}
    case F.Pair2{address, leg} <> rest:
      choose_lazy(Maybe<&2, T.leg>, branch_key(key, address), _ => Some{leg}, _ => branch_leg(key, rest))

type program<-C: Data, -A: Data> is Type:
  Pure{value: A}
  Abort{error: E.t}
  Request{task: task<C>, resume: result(answer) -> program<C, A>}

type ops<-C: Data> is Type:
  Ops{infer_univ: C -> T.t -> result(L.t), whnf: C -> V.t -> result(V.t), bind: String -> Q.t -> V.t -> C -> C, size: C -> F.Int63.t, env: C -> List<&2, V.t>, eval_env: C -> List<&2, V.t> -> T.t -> result(V.t), quote: C -> V.t -> result(T.t), head_ty: C -> V.head -> result(V.t), family: C -> String -> Maybe<&2, Pos.family>}

def option_case(-C: Data, -A: Data, option: Maybe<&2, A>, next: A -> program<C, Bool>) -> program<C, Bool>:
  match option:
    case None{}: Pure{False{}}
    case Some{x}: next(x)

def zero_constructor(ctor: Maybe<&2, Pos.ctor>) -> Bool:
  match ctor:
    case None{}: False{}
    case Some{Pos.Ctor{_, args, _, _, self}}: Bool.and(erased_fields(args), Bool.not(self))

def count_step(-C: Data, order: Cmp, next: Unit -> program<C, Bool>) -> program<C, Bool>:
  match order:
    case LT{}: Abort{E.Mismatch{"a collection width must be nonnegative"}}
    case EQ{}: Pure{True{}}
    case GT{}: next(Unit{})

def at(-A: Data, index: F.Int63.t, xs: List<&2, A>) -> Maybe<&2, A>: at_loop(A, xs, index)

def evaluator(-C: Data) -> Type: List<&2, V.t> -> T.t -> program<C, V.t>

def point_equal(~C: Data, ~Env: Data, ~eq: Env -> V.t -> V.t -> program<C, Bool>, +environment: Env, a: V.t, b: V.t, same: Bool) -> program<C, Bool>:
  match same:
    case False{}: Pure{False{}}
    case True{}: eq(environment, a, b)

def freeze_neutral(-C: Data, neutral: Maybe<&2, F.Pair2<V.head, List<&2, V.spine>>>, frame: V.spine) -> program<C, V.t>:
  match neutral:
    case None{}: Abort{E.Mismatch{stuck_msg}}
    case Some{F.Pair2{h, sp}}: Pure{V.VNeutral{h, frame <> sp}}

def lift(-C: Data, -A: Data, value: result(A)) -> program<C, A>:
  match value:
    case Fail{e}: Abort{e}
    case Done{x}: Pure{x}

@unsafe
def pbind(-C: Data, -A: Data, -B: Data, value: program<C, A>, next: A -> program<C, B>) -> program<C, B>:
  match value:
    case Pure{x}: next(x)
    case Abort{e}: Abort{e}
    case Request{job, resume}: Request{job, answer => pbind(C, A, B, resume(answer), next)}

@unsafe
def pcatch(-C: Data, -A: Data, value: program<C, A>, recover: E.t -> program<C, A>) -> program<C, A>:
  match value:
    case Pure{x}: Pure{x}
    case Abort{e}: recover(e)
    case Request{job, resume}: Request{job, answer => pcatch(C, A, resume(answer), recover)}

def pchoose(-C: Data, -A: Data, flag: Bool, yes: Unit -> program<C, A>, no: Unit -> program<C, A>) -> program<C, A>:
  match flag:
    case True{}: yes(Unit{})
    case False{}: no(Unit{})

def value_answer(-C: Data, response: result(answer)) -> program<C, V.t>:
  match response:
    case Fail{e}: Abort{e}
    case Done{ValueAnswer{v}}: Pure{v}
    case Done{TermAnswer{_}}: Abort{E.Mismatch{"internal evaluation response kind"}}
    case Done{BoolAnswer{_}}: Abort{E.Mismatch{"internal evaluation response kind"}}

def term_answer(-C: Data, response: result(answer)) -> program<C, T.t>:
  match response:
    case Fail{e}: Abort{e}
    case Done{ValueAnswer{_}}: Abort{E.Mismatch{"internal readback response kind"}}
    case Done{TermAnswer{t}}: Pure{t}
    case Done{BoolAnswer{_}}: Abort{E.Mismatch{"internal readback response kind"}}

def bool_answer(-C: Data, response: result(answer)) -> program<C, Bool>:
  match response:
    case Fail{e}: Abort{e}
    case Done{ValueAnswer{_}}: Abort{E.Mismatch{"internal conversion response kind"}}
    case Done{TermAnswer{_}}: Abort{E.Mismatch{"internal conversion response kind"}}
    case Done{BoolAnswer{b}}: Pure{b}

@unsafe
def run(~C: Data, ~dispatch: task<C> -> program<C, answer>, -A: Data, value: program<C, A>) -> result(A):
  match value:
    case Pure{x}: Done{x}
    case Abort{e}: Fail{e}
    case Request{job, resume}: run(~C, ~dispatch, A, resume(run(~C, ~dispatch, answer, dispatch(job))))

def o_infer_univ(-C: Data, ops: ops<C>, ctx: C, term: T.t) -> result(L.t):
  match ops:
    case Ops{f, _, _, _, _, _, _, _, _}: f(ctx, term)

def o_whnf(-C: Data, ops: ops<C>, ctx: C, value: V.t) -> result(V.t):
  match ops:
    case Ops{_, f, _, _, _, _, _, _, _}: f(ctx, value)

def o_bind(-C: Data, ops: ops<C>, name: String, quantity: Q.t, ty: V.t, ctx: C) -> C:
  match ops:
    case Ops{_, _, f, _, _, _, _, _, _}: f(name, quantity, ty, ctx)

def o_size(-C: Data, ops: ops<C>, ctx: C) -> F.Int63.t:
  match ops:
    case Ops{_, _, _, f, _, _, _, _, _}: f(ctx)

def o_env(-C: Data, ops: ops<C>, ctx: C) -> List<&2, V.t>:
  match ops:
    case Ops{_, _, _, _, f, _, _, _, _}: f(ctx)

def o_eval_env(-C: Data, ops: ops<C>, ctx: C, env: List<&2, V.t>, term: T.t) -> result(V.t):
  match ops:
    case Ops{_, _, _, _, _, f, _, _, _}: f(ctx, env, term)

def o_quote(-C: Data, ops: ops<C>, ctx: C, value: V.t) -> result(T.t):
  match ops:
    case Ops{_, _, _, _, _, _, f, _, _}: f(ctx, value)

def o_head_ty(-C: Data, ops: ops<C>, ctx: C, head: V.head) -> result(V.t):
  match ops:
    case Ops{_, _, _, _, _, _, _, f, _}: f(ctx, head)

def o_family(-C: Data, ops: ops<C>, ctx: C, name: String) -> Maybe<&2, Pos.family>:
  match ops:
    case Ops{_, _, _, _, _, _, _, _, f}: f(ctx, name)

def zero_complete(names: List<&2, String>, family: Pos.family) -> Bool:
  match names:
    case []: True{}
    case c <> rest:
      match rest:
        case []: zero_constructor(Pos.ctor_of(c, family))
        case _ <> _: False{}

def payload_eq(~C: Data, ~A: Data, ~Env: Data, ~eq: Env -> A -> A -> program<C, Bool>, +environment: Env, xs: List<&2, A>, ys: List<&2, A>) -> program<C, Bool>:
  match xs, ys:
    case [], []: Pure{True{}}
    case [], _ <> _: Pure{False{}}
    case _ <> _, []: Pure{False{}}
    case x <> rest, y <> tail:
      pbind(C, Bool, Bool, eq(environment, x, y), same => pchoose(C, Bool, same, _ => payload_eq(~C, ~A, ~Env, ~eq, environment, rest, tail), _ => Pure{False{}}))

def open_closure(-C: Data, ev: evaluator(C), clo: V.closure, args: List<&2, V.t>) -> program<C, V.t>:
  match clo:
    case V.Closure{env, body}: ev(rev_append(V.t, args, env), body)

def freeze_option(-C: Data, value: V.t, frame: V.spine, option: Maybe<&2, V.t>) -> program<C, V.t>:
  match option:
    case Some{x}: Pure{x}
    case None{}: freeze_neutral(C, V.as_neutral(value), frame)

def pmap(-C: Data, -A: Data, -B: Data, f: A -> B, value: program<C, A>) -> program<C, B>:
  pbind(C, A, B, value, x => Pure{f(x)})

def ask_eval(-C: Data, ctx: C, env: List<&2, V.t>, term: T.t) -> program<C, V.t>:
  Request{Evaluate{ctx, env, term}, reply => value_answer(C, reply)}

def ask_whnf(-C: Data, ctx: C, value: V.t) -> program<C, V.t>:
  Request{Weak{ctx, value}, reply => value_answer(C, reply)}

def ask_quote(-C: Data, ctx: C, size: F.Int63.t, value: V.t) -> program<C, T.t>:
  Request{Readback{ctx, size, value}, reply => term_answer(C, reply)}

def ask_conv(-C: Data, ctx: C, ty: V.t, a: V.t, b: V.t) -> program<C, Bool>:
  Request{Compare{ctx, ty, a, b}, reply => bool_answer(C, reply)}

def ops_ev(-C: Data, ops: ops<C>, ctx: C) -> evaluator(C):
  env => term => lift(C, V.t, o_eval_env(C, ops, ctx, env, term))

def ask_type(-C: Data, ctx: C, a: V.t, b: V.t) -> program<C, Bool>:
  ask_conv(C, ctx, placeholder, a, b)

def open_leg(-C: Data, ev: evaluator(C), leg: V.vleg) -> program<C, V.t>:
  match leg:
    case V.VLeg{_, clo}: open_closure(C, ev, clo, [])

def zero_family_view(family: Pos.family, view: Pos.family) -> Bool:
  match view:
    case Pos.Family{_, _, _, level, status, _, positive}:
      match status:
        case Pos.Builtin{}: False{}
        case Pos.Provisional{}: False{}
        case Pos.Complete{names}: Bool.and(Bool.and(L.equal(level, L.zero), positive), zero_complete(names, family))

def grow(~C: Data, ~ops: ops<C>, count: Nat, ctx: C) -> C:
  match count:
    case Zero{}: ctx
    case Succ{n}: grow(~C, ~ops, n, o_bind(C, ops, "_", Q.Many{}, placeholder, ctx))

def map_list(~C: Data, ~A: Data, ~B: Data, ~Env: Data, ~f: Env -> A -> program<C, B>, +environment: Env, xs: List<&2, A>) -> program<C, List<&2, B>>:
  match xs:
    case []: Pure{[]}
    case x <> rest:
      pbind(C, B, List<&2, B>, f(environment, x), y => pmap(C, List<&2, B>, List<&2, B>, ys => y <> ys, map_list(~C, ~A, ~B, ~Env, ~f, environment, rest)))

def family_equal(~C: Data, ~Env: Data, ~eq: Env -> V.t -> V.t -> program<C, Bool>, +environment: Env, a: List<&2, V.t>, b: List<&2, V.t>, same: Bool) -> program<C, Bool>:
  match same:
    case False{}: Pure{False{}}
    case True{}: payload_eq(C, V.t, Env, eq, environment, a, b)

def beta_closure(-C: Data, ev: evaluator(C), args: List<&2, V.t>, leg: Maybe<&2, V.vleg>) -> program<C, Maybe<&2, V.t>>:
  match leg:
    case None{}: Pure{None{}}
    case Some{V.VLeg{_, clo}}: pmap(C, V.t, Maybe<&2, V.t>, x => Some{x}, open_closure(C, ev, clo, args))

def beta_pair(-C: Data, ev: evaluator(C), env: List<&2, V.t>, point: Maybe<&2, F.Pair2<Q.t, V.t>>, arg: Maybe<&2, V.t>, branch: Maybe<&2, F.Pair2<T.addr, T.leg>>) -> program<C, Maybe<&2, V.t>>:
  match point, arg, branch:
    case None{}, None{}, None{}: Pure{None{}}
    case None{}, None{}, Some{_}: Pure{None{}}
    case None{}, Some{_}, None{}: Pure{None{}}
    case None{}, Some{_}, Some{_}: Pure{None{}}
    case Some{_}, None{}, None{}: Pure{None{}}
    case Some{_}, None{}, Some{_}: Pure{None{}}
    case Some{_}, Some{_}, None{}: Pure{None{}}
    case Some{F.Pair2{_, value}}, Some{fibre}, Some{F.Pair2{_, T.Leg{_, body}}}:
      pmap(C, V.t, Maybe<&2, V.t>, x => Some{x}, ev(fibre <> value <> env, body))

def beta_body(-C: Data, ev: evaluator(C), env: List<&2, V.t>, args: List<&2, V.t>, branch: Maybe<&2, T.leg>) -> program<C, Maybe<&2, V.t>>:
  match branch:
    case None{}: Pure{None{}}
    case Some{T.Leg{_, body}}: pmap(C, V.t, Maybe<&2, V.t>, x => Some{x}, ev(rev_append(V.t, args, env), body))

def freeze(-C: Data, stepped: program<C, Maybe<&2, V.t>>, value: V.t, frame: V.spine) -> program<C, V.t>:
  pbind(C, Maybe<&2, V.t>, V.t, stepped, option => freeze_option(C, value, frame, option))

def coll_legs(~C: Data, ~ops: ops<C>, ctx: C, diagram: V.closure) -> program<C, List<&2, V.vleg>>:
  pbind(C, V.t, List<&2, V.vleg>, open_closure(C, ops_ev(C, ops, ctx), diagram, []), v => lift(C, List<&2, V.vleg>, empty_section(v)))

def leg_type(~C: Data, ~ops: ops<C>, ctx: C, legs: List<&2, V.vleg>, index: F.Int63.t) -> program<C, V.t>:
  pbind(C, V.vleg, V.t, lift(C, V.vleg, require(V.vleg, at(V.vleg, index, legs), E.Wrong_leg{leg_msg})), leg => open_leg(C, ops_ev(C, ops, ctx), leg))

def zero_family(+family: Pos.family) -> Bool:
  zero_family_view(family, family)

def spine_point(~C: Data, ~ops: ops<C>, ctx: C, diagram: V.closure, info: Maybe<&2, Point>, address: Maybe<&2, F.Pair2<Q.t, V.t>>) -> program<C, Maybe<&2, SpineStep>>:
  match info, address:
    case None{}, None{}: Pure{None{}}
    case None{}, Some{_}: Pure{None{}}
    case Some{_}, None{}: Pure{None{}}
    case Some{Point{_, _, domain}}, Some{F.Pair2{_, arg}}:
      pmap(C, V.t, Maybe<&2, SpineStep>, cod => Some{SpineStep{Some{domain}, cod}}, open_closure(C, ops_ev(C, ops, ctx), diagram, [arg]))

def spine_leg(~C: Data, ~ops: ops<C>, ctx: C, leg: Maybe<&2, V.vleg>) -> program<C, Maybe<&2, SpineStep>>:
  match leg:
    case None{}: Pure{None{}}
    case Some{V.VLeg{_, clo}}:
      pmap(C, V.t, Maybe<&2, SpineStep>, ty => Some{SpineStep{None{}, ty}}, open_closure(C, ops_ev(C, ops, ctx), clo, []))

def point_diagrams(~C: Data, ~ops: ops<C>, +ctx: C, d1: V.closure, d2: V.closure, info: Point) -> program<C, Bool>:
  match info:
    case Point{q, name, domain}:
      pbind(C, V.t, Bool, open_closure(C, ops_ev(C, ops, ctx), d1, [V.var(o_size(C, ops, ctx))]), a =>
        pbind(C, V.t, Bool, open_closure(C, ops_ev(C, ops, ctx), d2, [V.var(o_size(C, ops, ctx))]), b =>
          ask_type(C, o_bind(C, ops, name, q, domain, ctx), a, b)))

def map_shape(~C: Data, ~A: Data, ~B: Data, ~Env: Data, ~f: Env -> A -> program<C, B>, +environment: Env, shape: S.t<A>) -> program<C, S.t<B>>:
  match shape:
    case S.SPi{q, name, domain}: pmap(C, B, S.t<B>, d => S.SPi{q, name, d}, f(environment, domain))
    case S.SColl{n}: Pure{S.SColl{n}}
    case S.SPar{_, _}: Abort{E.Not_yet{spar_word}}
    case S.SMu{name, indices}: pmap(C, List<&2, B>, S.t<B>, ix => S.SMu{name, ix}, map_list(~C, ~A, ~B, ~Env, ~f, environment, indices))
    case S.SNu{_, _}: Abort{E.Not_yet{snu_word}}

def shape_eq(~C: Data, ~Env: Data, ~eq: Env -> V.t -> V.t -> program<C, Bool>, +environment: Env, a: S.t<V.t>, b: S.t<V.t>) -> program<C, Bool>:
  match a:
    case S.SPi{q, _, domain}:
      match b:
        case S.SPi{r, _, other}:
          point_equal(C, Env, eq, environment, domain, other, Q.equal(q, r))
        case S.SColl{_}: Pure{False{}}
        case S.SPar{_, _}: Pure{False{}}
        case S.SMu{_, _}: Pure{False{}}
        case S.SNu{_, _}: Pure{False{}}
    case S.SColl{n}:
      match b:
        case S.SPi{_, _, _}: Pure{False{}}
        case S.SColl{m}: Pure{F.Int63.equal(n, m)}
        case S.SPar{_, _}: Pure{False{}}
        case S.SMu{_, _}: Pure{False{}}
        case S.SNu{_, _}: Pure{False{}}
    case S.SPar{_, _}:
      match b:
        case S.SPi{_, _, _}: Pure{False{}}
        case S.SColl{_}: Pure{False{}}
        case S.SPar{_, _}: Abort{E.Not_yet{spar_word}}
        case S.SMu{_, _}: Pure{False{}}
        case S.SNu{_, _}: Pure{False{}}
    case S.SMu{+name, indices}:
      match b:
        case S.SPi{_, _, _}: Pure{False{}}
        case S.SColl{_}: Pure{False{}}
        case S.SPar{_, _}: Pure{False{}}
        case S.SMu{other, rest}:
          family_equal(C, Env, eq, environment, indices, rest, String.eq(name, other))
        case S.SNu{_, _}: Pure{False{}}
    case S.SNu{_, _}:
      match b:
        case S.SPi{_, _, _}: Pure{False{}}
        case S.SColl{_}: Pure{False{}}
        case S.SPar{_, _}: Pure{False{}}
        case S.SMu{_, _}: Pure{False{}}
        case S.SNu{_, _}: Abort{E.Not_yet{snu_word}}

def beta_point(-C: Data, ev: evaluator(C), point: Maybe<&2, F.Pair2<Q.t, V.t>>, section: Maybe<&2, F.Pair2<S.t<V.t>, List<&2, V.vleg>>>) -> program<C, Maybe<&2, V.t>>:
  match point, section:
    case None{}, None{}: Pure{None{}}
    case None{}, Some{_}: Pure{None{}}
    case Some{_}, None{}: Pure{None{}}
    case Some{F.Pair2{_, arg}}, Some{F.Pair2{_, legs}}: beta_closure(C, ev, [arg], one_of(V.vleg, legs))

def beta_collection(-C: Data, ev: evaluator(C), index: Maybe<&2, F.Int63.t>, section: Maybe<&2, F.Pair2<S.t<V.t>, List<&2, V.vleg>>>) -> program<C, Maybe<&2, V.t>>:
  match index, section:
    case None{}, None{}: Pure{None{}}
    case None{}, Some{_}: Pure{None{}}
    case Some{_}, None{}: Pure{None{}}
    case Some{k}, Some{F.Pair2{_, legs}}: beta_closure(C, ev, [], at(V.vleg, k, legs))

def beta_branch(-C: Data, ev: evaluator(C), key: Maybe<&2, V.vaddr>, branches: List<&2, F.Pair2<T.addr, T.leg>>, env: List<&2, V.t>, args: List<&2, V.t>) -> program<C, Maybe<&2, V.t>>:
  match key:
    case None{}: Pure{None{}}
    case Some{k}: beta_body(C, ev, env, args, branch_leg(k, branches))

def subsingleton_found(family: Maybe<&2, Pos.family>) -> Bool:
  match family:
    case None{}: False{}
    case Some{f}: zero_family(f)

def spine_collection(~C: Data, ~ops: ops<C>, +ctx: C, diagram: V.closure, address: Maybe<&2, F.Int63.t>) -> program<C, Maybe<&2, SpineStep>>:
  match address:
    case None{}: Pure{None{}}
    case Some{k}:
      pbind(C, List<&2, V.vleg>, Maybe<&2, SpineStep>, coll_legs(C, ops, ctx, diagram), legs => spine_leg(C, ops, ctx, at(V.vleg, k, legs)))

def conv_diagram(~C: Data, ~ops: ops<C>, +ctx: C, p: pack, shape: S.t<V.t>, d1: V.closure, d2: V.closure) -> program<C, Bool>:
  match p:
    case PiPack{}:
      pbind(C, Point, Bool, lift(C, Point, require(Point, point_view(shape), E.Mismatch{wrong_pack})), info => point_diagrams(C, ops, ctx, d1, d2, info))
    case CollPack{}:
      pbind(C, V.t, Bool, open_closure(C, ops_ev(C, ops, ctx), d1, []), a =>
        pbind(C, V.t, Bool, open_closure(C, ops_ev(C, ops, ctx), d2, []), b => ask_type(C, ctx, a, b)))
    case MuPack{}:
      pbind(C, V.t, Bool, open_closure(C, ops_ev(C, ops, ctx), d1, []), a =>
        pbind(C, V.t, Bool, open_closure(C, ops_ev(C, ops, ctx), d2, []), b => ask_type(C, ctx, a, b)))

def beta_out(-C: Data, p: pack, ev: evaluator(C), address: V.vaddr, value: V.t) -> program<C, Maybe<&2, V.t>>:
  match p:
    case MuPack{}: Abort{E.Not_yet{mu_ran_word}}
    case PiPack{}: beta_point(C, ev, V.as_pt(address), V.as_sec(value))
    case CollPack{}: beta_collection(C, ev, V.as_leg(address), V.as_sec(value))

def beta_intro(-C: Data, p: pack, ev: evaluator(C), branches: List<&2, F.Pair2<T.addr, T.leg>>, env: List<&2, V.t>, intro: Maybe<&2, V.Intro>) -> program<C, Maybe<&2, V.t>>:
  match p, intro:
    case PiPack{}, None{}: Pure{None{}}
    case CollPack{}, None{}: Pure{None{}}
    case MuPack{}, None{}: Pure{None{}}
    case PiPack{}, Some{V.Intro{_, address, args}}:
      beta_pair(C, ev, env, V.as_pt(address), one_of(V.t, args), one_of(F.Pair2<T.addr, T.leg>, branches))
    case CollPack{}, Some{V.Intro{_, address, args}}: beta_branch(C, ev, collection_key(address), branches, env, args)
    case MuPack{}, Some{V.Intro{_, address, args}}: beta_branch(C, ev, constructor_key(address), branches, env, args)

def subsingleton(~C: Data, ~ops: ops<C>, ctx: C, shape: S.t<V.t>) -> Bool:
  match shape:
    case S.SMu{name, _}: subsingleton_found(o_family(C, ops, ctx, name))
    case S.SPi{_, _, _}: False{}
    case S.SColl{_}: False{}
    case S.SPar{_, _}: False{}
    case S.SNu{_, _}: False{}

def spine_ty(~C: Data, ~ops: ops<C>, ctx: C, p: pack, shape: S.t<V.t>, diagram: V.closure, address: V.vaddr) -> program<C, Maybe<&2, SpineStep>>:
  match p:
    case PiPack{}: spine_point(C, ops, ctx, diagram, point_view(shape), V.as_pt(address))
    case CollPack{}: spine_collection(C, ops, ctx, diagram, V.as_leg(address))
    case MuPack{}: Pure{None{}}

def beta_elim(-C: Data, p: pack, ev: evaluator(C), branches: List<&2, F.Pair2<T.addr, T.leg>>, env: List<&2, V.t>, value: V.t) -> program<C, Maybe<&2, V.t>>:
  beta_intro(C, p, ev, branches, env, V.as_in(value))

def out_with(-C: Data, p: pack, ev: evaluator(C), shape: S.t<V.t>, +address: V.vaddr, +value: V.t) -> program<C, V.t>:
  freeze(C, beta_out(C, p, ev, address, value), value, V.SOut{shape, address})

def eta_right(~C: Data, ~ops: ops<C>, +ctx: C, +shape: S.t<V.t>, diagram: V.closure, a: V.t, b: V.t, +q: Q.t, name: String, domain: V.t) -> program<C, Bool>:
  pbind(C, V.t, Bool, open_closure(C, ops_ev(C, ops, ctx), diagram, [V.var(o_size(C, ops, ctx))]), cod =>
    pbind(C, V.t, Bool, out_with(C, PiPack{}, ops_ev(C, ops, ctx), shape, V.VAPt{q, V.var(o_size(C, ops, ctx))}, a), fa =>
      pbind(C, V.t, Bool, out_with(C, PiPack{}, ops_ev(C, ops, ctx), shape, V.VAPt{q, V.var(o_size(C, ops, ctx))}, b), fb =>
        ask_conv(C, o_bind(C, ops, name, q, domain, ctx), cod, fa, fb))))

@unsafe
def coll_eta_loop(~C: Data, ~ops: ops<C>, +ctx: C, +shape: S.t<V.t>, +legs: List<&2, V.vleg>, +a: V.t, +b: V.t, +index: F.Int63.t, +remaining: F.Int63.t) -> program<C, Bool>:
  count_step(C, F.Int63.compare(remaining, F.Int63.zero), _ =>
    pbind(C, V.t, Bool, leg_type(C, ops, ctx, legs, index), ty =>
      pbind(C, V.t, Bool, out_with(C, CollPack{}, ops_ev(C, ops, ctx), shape, V.VALeg{index}, a), x =>
        pbind(C, V.t, Bool, out_with(C, CollPack{}, ops_ev(C, ops, ctx), shape, V.VALeg{index}, b), y =>
          pbind(C, Bool, Bool, ask_conv(C, ctx, ty, x, y), same =>
            pchoose(C, Bool, same, _ => coll_eta_loop(~C, ~ops, ctx, shape, legs, a, b, F.Int63.succ(index), F.Int63.pred(remaining)), _ => Pure{False{}}))))))

def out_value(-C: Data, ev: evaluator(C), +shape: S.t<V.t>, address: V.vaddr, value: V.t) -> program<C, V.t>:
  pbind(C, pack, V.t, lift(C, pack, rules(V.t, shape)), p => out_with(C, p, ev, shape, address, value))

def elim_with(-C: Data, p: pack, ev: evaluator(C), shape: S.t<V.t>, q: Q.t, motive: Maybe<&2, T.motive>, +branches: List<&2, F.Pair2<T.addr, T.leg>>, +env: List<&2, V.t>, +value: V.t) -> program<C, V.t>:
  freeze(C, beta_elim(C, p, ev, branches, env, value), value, V.SElim{V.Stuck{shape, q, motive, branches, env}})

def motive_body(~C: Data, ~ops: ops<C>, +ctx: C, shape: S.t<V.t>, diagram: V.closure, +q: Q.t, domain: V.t, second: Bool) -> program<C, V.t>:
  match second:
    case False{}: Pure{domain}
    case True{}:
      pbind(C, V.t, V.t, elim_with(C, PiPack{}, ops_ev(C, ops, ctx), shape, q, None{}, proj_branch(q, False{}), o_env(C, ops, ctx), V.var(o_size(C, ops, ctx))), point =>
        open_closure(C, ops_ev(C, ops, ctx), diagram, [point]))

def elim_value(-C: Data, ev: evaluator(C), +shape: S.t<V.t>, q: Q.t, motive: Maybe<&2, T.motive>, branches: List<&2, F.Pair2<T.addr, T.leg>>, env: List<&2, V.t>, value: V.t) -> program<C, V.t>:
  pbind(C, pack, V.t, lift(C, pack, rules(V.t, shape)), p => elim_with(C, p, ev, shape, q, motive, branches, env, value))

def proj_motive(~C: Data, ~ops: ops<C>, +ctx: C, shape: S.t<V.t>, diagram: V.closure, +q: Q.t, name: String, +domain: V.t, second: Bool) -> program<C, Maybe<&2, T.motive>>:
  pbind(C, V.t, Maybe<&2, T.motive>, motive_body(C, ops, ctx, shape, diagram, q, domain, second), value =>
    pmap(C, T.t, Maybe<&2, T.motive>, body => Some{T.Motive{None{}, [], "self", body}}, lift(C, T.t, o_quote(C, ops, o_bind(C, ops, name, q, domain, ctx), value))))

def eta_second(~C: Data, ~ops: ops<C>, +ctx: C, +shape: S.t<V.t>, +diagram: V.closure, a: V.t, b: V.t, +q: Q.t, name: String, domain: V.t, point: V.t) -> program<C, Bool>:
  pbind(C, V.t, Bool, open_closure(C, ops_ev(C, ops, ctx), diagram, [point]), cod =>
    pbind(C, Maybe<&2, T.motive>, Bool, proj_motive(C, ops, ctx, shape, diagram, q, name, domain, True{}), +mo =>
      pbind(C, V.t, Bool, elim_with(C, PiPack{}, ops_ev(C, ops, ctx), shape, Q.One{}, mo, proj_branch(q, True{}), o_env(C, ops, ctx), a), a2 =>
        pbind(C, V.t, Bool, elim_with(C, PiPack{}, ops_ev(C, ops, ctx), shape, Q.One{}, mo, proj_branch(q, True{}), o_env(C, ops, ctx), b), b2 =>
          ask_conv(C, ctx, cod, a2, b2)))))

def eta_left(~C: Data, ~ops: ops<C>, +ctx: C, +shape: S.t<V.t>, +diagram: V.closure, +a: V.t, +b: V.t, +q: Q.t, +name: String, +domain: V.t) -> program<C, Bool>:
  pbind(C, Maybe<&2, T.motive>, Bool, proj_motive(C, ops, ctx, shape, diagram, q, name, domain, False{}), +mo =>
    pbind(C, V.t, Bool, elim_with(C, PiPack{}, ops_ev(C, ops, ctx), shape, Q.One{}, mo, proj_branch(q, False{}), o_env(C, ops, ctx), a), +a1 =>
      pbind(C, V.t, Bool, elim_with(C, PiPack{}, ops_ev(C, ops, ctx), shape, Q.One{}, mo, proj_branch(q, False{}), o_env(C, ops, ctx), b), b1 =>
        pbind(C, Bool, Bool, ask_conv(C, ctx, domain, a1, b1), first =>
          pchoose(C, Bool, first, _ => eta_second(C, ops, ctx, shape, diagram, a, b, q, name, domain, a1), _ => Pure{False{}})))))

def eta_point(~C: Data, ~ops: ops<C>, ctx: C, shape: S.t<V.t>, diagram: V.closure, a: V.t, b: V.t, right: Bool, info: Point) -> program<C, Bool>:
  match right, info:
    case True{}, Point{q, name, domain}: eta_right(C, ops, ctx, shape, diagram, a, b, q, name, domain)
    case False{}, Point{q, name, domain}: eta_left(C, ops, ctx, shape, diagram, a, b, q, name, domain)

def expand(~C: Data, ~ops: ops<C>, +ctx: C, p: pack, +shape: S.t<V.t>, diagram: V.closure, a: V.t, b: V.t, right: Bool) -> program<C, Bool>:
  match p:
    case PiPack{}:
      pbind(C, Point, Bool, lift(C, Point, require(Point, point_view(shape), E.Mismatch{wrong_pack})), info => eta_point(C, ops, ctx, shape, diagram, a, b, right, info))
    case CollPack{}:
      pbind(C, F.Int63.t, Bool, lift(C, F.Int63.t, require(F.Int63.t, collection_view(shape), E.Mismatch{wrong_pack})), n =>
        pbind(C, List<&2, V.vleg>, Bool, coll_legs(C, ops, ctx, diagram), legs => coll_eta_loop(C, ops, ctx, shape, legs, a, b, F.Int63.zero, n)))
    case MuPack{}: Pure{False{}}
