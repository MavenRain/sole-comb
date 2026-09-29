# Runtime, conversion, and checking rules at Kanon 69f3be51 rules.ml.
import Base
import ./foundation.bend as F
import ./kernel_level.bend as L
import ./kernel_quantity.bend as Q
import ./kernel_shape.bend as S
import ./kernel_term.bend as T
import ./kernel_value.bend as V
import ./kernel_error.bend as E
import ./kernel_positivity.bend as Pos
# The placeholder is used only when opening an untyped conversion scope.
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
  InferUses{context: C, mode: Q.t, term: T.t}
  CheckUses{context: C, mode: Q.t, term: T.t, expected: V.t}
  InferUniverse{context: C, term: T.t}

type answer is Data:
  ValueAnswer{value: V.t}
  TermAnswer{term: T.t}
  BoolAnswer{equal: Bool}
  InferredAnswer{ty: V.t, uses: Q.usage}
  UsageAnswer{uses: Q.usage}
  LevelAnswer{level: L.t}

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
    case Done{InferredAnswer{_, _}}: Abort{E.Mismatch{"internal evaluation response kind"}}
    case Done{UsageAnswer{_}}: Abort{E.Mismatch{"internal evaluation response kind"}}
    case Done{LevelAnswer{_}}: Abort{E.Mismatch{"internal evaluation response kind"}}

def term_answer(-C: Data, response: result(answer)) -> program<C, T.t>:
  match response:
    case Fail{e}: Abort{e}
    case Done{ValueAnswer{_}}: Abort{E.Mismatch{"internal readback response kind"}}
    case Done{TermAnswer{t}}: Pure{t}
    case Done{BoolAnswer{_}}: Abort{E.Mismatch{"internal readback response kind"}}
    case Done{InferredAnswer{_, _}}: Abort{E.Mismatch{"internal readback response kind"}}
    case Done{UsageAnswer{_}}: Abort{E.Mismatch{"internal readback response kind"}}
    case Done{LevelAnswer{_}}: Abort{E.Mismatch{"internal readback response kind"}}

def bool_answer(-C: Data, response: result(answer)) -> program<C, Bool>:
  match response:
    case Fail{e}: Abort{e}
    case Done{ValueAnswer{_}}: Abort{E.Mismatch{"internal conversion response kind"}}
    case Done{TermAnswer{_}}: Abort{E.Mismatch{"internal conversion response kind"}}
    case Done{BoolAnswer{b}}: Pure{b}
    case Done{InferredAnswer{_, _}}: Abort{E.Mismatch{"internal conversion response kind"}}
    case Done{UsageAnswer{_}}: Abort{E.Mismatch{"internal conversion response kind"}}
    case Done{LevelAnswer{_}}: Abort{E.Mismatch{"internal conversion response kind"}}

# Nested requests keep their continuations in data rather than the host stack.
type Run.stack<-C: Data> is Type:
  Run.Empty{}
  Run.Frame{resume: result(answer) -> program<C, answer>, rest: Run.stack<C>}

type Run.state<-C: Data, -A: Data> is Type:
  Run.Top{value: program<C, A>}
  Run.Task{value: program<C, answer>, stack: Run.stack<C>, finish: result(answer) -> program<C, A>}

def Run.deliver(-C: Data, -A: Data, value: result(answer), stack: Run.stack<C>, finish: result(answer) -> program<C, A>) -> Run.state<C, A>:
  match stack:
    case Run.Empty{}: Run.Top{finish(value)}
    case Run.Frame{resume, rest}: Run.Task{resume(value), rest, finish}

@unsafe
def Run.drive(~C: Data, ~dispatch: task<C> -> program<C, answer>, -A: Data, state: Run.state<C, A>) -> result(A):
  match state:
    case Run.Top{value}:
      match value:
        case Pure{x}: Done{x}
        case Abort{e}: Fail{e}
        case Request{job, resume}: Run.drive(~C, ~dispatch, A, Run.Task{dispatch(job), Run.Empty{}, resume})
    case Run.Task{value, stack, finish}:
      match value:
        case Pure{x}: Run.drive(~C, ~dispatch, A, Run.deliver(C, A, Done{x}, stack, finish))
        case Abort{e}: Run.drive(~C, ~dispatch, A, Run.deliver(C, A, Fail{e}, stack, finish))
        case Request{job, resume}: Run.drive(~C, ~dispatch, A, Run.Task{dispatch(job), Run.Frame{resume, stack}, finish})

def run(~C: Data, ~dispatch: task<C> -> program<C, answer>, -A: Data, value: program<C, A>) -> result(A):
  Run.drive(~C, ~dispatch, A, Run.Top{value})

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

def inferred() -> Data: F.Pair2<V.t, Q.usage>

type check_ops<-C: Data> is Type:
  CheckOps{runtime: ops<C>, close: C -> F.Int63.t -> Q.t -> Q.usage -> result(Q.usage), pp: C -> V.t -> String}

def c_runtime(-C: Data, op: check_ops<C>) -> ops<C>:
  match op:
    case CheckOps{r, _, _}: r

def c_close(~C: Data, op: check_ops<C>, ctx: C, size: F.Int63.t, q: Q.t, uses: Q.usage) -> program<C, Q.usage>:
  match op:
    case CheckOps{_, f, _}: lift(C, Q.usage, f(ctx, size, q, uses))

def c_pp(-C: Data, op: check_ops<C>, ctx: C, value: V.t) -> String:
  match op:
    case CheckOps{_, _, f}: f(ctx, value)

def c_conv(~C: Data, ~op: check_ops<C>, ctx: C, ty: V.t, a: V.t, b: V.t) -> program<C, Bool>: ask_conv(C, ctx, ty, a, b)

def c_conv_type(~C: Data, ~op: check_ops<C>, ctx: C, a: V.t, b: V.t) -> program<C, Bool>: ask_type(C, ctx, a, b)

def c_eval(~C: Data, ~op: check_ops<C>, +ctx: C, tm: T.t) -> program<C, V.t>:
  lift(C, V.t, o_eval_env(C, c_runtime(C, op), ctx, o_env(C, c_runtime(C, op), ctx), tm))

def c_open(~C: Data, ~op: check_ops<C>, ctx: C, clo: V.closure, args: List<&2, V.t>) -> program<C, V.t>:
  match clo:
    case V.Closure{env, body}: lift(C, V.t, o_eval_env(C, c_runtime(C, op), ctx, rev_append(V.t, args, env), body))

def c_bind(~C: Data, ~op: check_ops<C>, ctx: C, x: String, q: Q.t, ty: V.t) -> C: o_bind(C, c_runtime(C, op), x, q, ty, ctx)

def c_size(~C: Data, ~op: check_ops<C>, ctx: C) -> F.Int63.t: o_size(C, c_runtime(C, op), ctx)

def crequire(~C: Data, ~A: Data, value: Maybe<&2, A>, error: E.t) -> program<C, A>: lift(C, A, require(A, value, error))

def inferred_answer(~C: Data, value: result(answer)) -> program<C, inferred>:
  match value:
    case Fail{e}: Abort{e}
    case Done{InferredAnswer{ty, uses}}: Pure{F.Pair2{ty, uses}}
    case Done{ValueAnswer{_}}: Abort{E.Mismatch{"internal checking answer"}}
    case Done{TermAnswer{_}}: Abort{E.Mismatch{"internal checking answer"}}
    case Done{BoolAnswer{_}}: Abort{E.Mismatch{"internal checking answer"}}
    case Done{UsageAnswer{_}}: Abort{E.Mismatch{"internal checking answer"}}
    case Done{LevelAnswer{_}}: Abort{E.Mismatch{"internal checking answer"}}

def usage_answer(~C: Data, value: result(answer)) -> program<C, Q.usage>:
  match value:
    case Fail{e}: Abort{e}
    case Done{UsageAnswer{uses}}: Pure{uses}
    case Done{ValueAnswer{_}}: Abort{E.Mismatch{"internal checking answer"}}
    case Done{TermAnswer{_}}: Abort{E.Mismatch{"internal checking answer"}}
    case Done{BoolAnswer{_}}: Abort{E.Mismatch{"internal checking answer"}}
    case Done{InferredAnswer{_, _}}: Abort{E.Mismatch{"internal checking answer"}}
    case Done{LevelAnswer{_}}: Abort{E.Mismatch{"internal checking answer"}}

def guard(~C: Data, flag: Bool, error: E.t) -> program<C, Unit>: pchoose(C, Unit, flag, _ => Pure{Unit{}}, _ => Abort{error})

def split(~C: Data, ~A: Data, ~B: Data, -D: Data, pair: F.Pair2<A, B>, next: A -> B -> program<C, D>) -> program<C, D>:
  match pair:
    case F.Pair2{a, b}: next(a, b)

def with_leg(~C: Data, -A: Data, leg: T.leg, next: List<&2, F.Pair2<Q.t, String>> -> T.t -> program<C, A>) -> program<C, A>:
  match leg:
    case T.Leg{bs, body}: next(bs, body)

def former_apply(~C: Data, -A: Data, w: V.t, former: V.Former, next: V.t -> S.t<V.t> -> V.closure -> program<C, A>) -> program<C, A>:
  match former:
    case V.Former{s, d, _}: next(w, s, d)

def point_apply(~C: Data, -A: Data, point: Point, next: Q.t -> String -> V.t -> program<C, A>) -> program<C, A>:
  match point:
    case Point{q, x, dom}: next(q, x, dom)

def with_point(~C: Data, ~A: Data, s: S.t<V.t>, message: String, next: Q.t -> String -> V.t -> program<C, A>) -> program<C, A>:
  pbind(C, Point, A, crequire(C, Point, point_view(s), E.Mismatch{message}), p => point_apply(C, A, p, next))

def binder_quantity(~C: Data, subject: String, source: String, name: String, +marked: Q.t, +declared: Q.t) -> program<C, Unit>:
  guard(C, Q.equal(marked, declared), E.Quantity{String.concat(["the ", subject, " ", name, " is marked ", Q.to_string(marked), " and the ", source, " marks it ", Q.to_string(declared)])})

def elim_result(~C: Data, ~op: check_ops<C>, +ctx: C, mo: Maybe<&2, T.motive>, expected: Maybe<&2, V.t>, self: V.t) -> program<C, V.t>:
  match mo:
    case None{}: crequire(C, V.t, expected, E.Cannot_infer{"an elimination without a motive needs an expected type"})
    case Some{T.Motive{_, _, _, body}}: lift(C, V.t, o_eval_env(C, c_runtime(C, op), ctx, self <> o_env(C, c_runtime(C, op), ctx), body))

def length(~A: Data, xs: List<&2, A>) -> F.Int63.t: advance(F.Int63.zero, List.length(&2, A, xs))

def two_of(-A: Data, xs: List<&2, A>) -> Maybe<&2, F.Pair2<A, A>>:
  match xs:
    case []: None{}
    case x <> rest:
      match rest:
        case []: None{}
        case y <> tail:
          match tail:
            case []: Some{F.Pair2{x, y}}
            case _ <> _: None{}

def coll_width(-A: Data, s: S.t<A>) -> Maybe<&2, F.Int63.t>:
  match s:
    case S.SPi{_, _, _}: None{}
    case S.SColl{n}: Some{n}
    case S.SPar{_, _}: None{}
    case S.SMu{_, _}: None{}
    case S.SNu{_, _}: None{}

def as_tsec(tm: T.t) -> Maybe<&2, F.Pair2<S.t<T.t>, List<&2, T.leg>>>:
  match tm:
    case T.Sec{s, ls}: Some{F.Pair2{s, ls}}
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

def diagram_unbound(legs: List<&2, T.leg>) -> Bool:
  match legs:
    case []: True{}
    case T.Leg{bs, _} <> rest: Bool.and(nat_equal(List.length(&2, F.Pair2<Q.t, String>, bs), 0n), diagram_unbound(rest))

def coll_legs_of(~C: Data, ~op: check_ops<C>, ctx: C, d: V.closure) -> program<C, List<&2, V.vleg>>:
  pbind(C, V.t, List<&2, V.vleg>, c_open(C, op, ctx, d, []), v =>
    pbind(C, F.Pair2<S.t<V.t>, List<&2, V.vleg>>, List<&2, V.vleg>, crequire(C, F.Pair2<S.t<V.t>, List<&2, V.vleg>>, V.as_sec(v), E.Mismatch{diagram_msg}), sec =>
      split(C, S.t<V.t>, List<&2, V.vleg>, List<&2, V.vleg>, sec, _ => legs => Pure{legs})))

def c_open_leg(~C: Data, ~op: check_ops<C>, ctx: C, leg: V.vleg) -> program<C, V.t>:
  match leg:
    case V.VLeg{_, clo}: c_open(C, op, ctx, clo, [])

def coll_leg_ty(~C: Data, ~op: check_ops<C>, ctx: C, legs: List<&2, V.vleg>, k: F.Int63.t) -> program<C, V.t>:
  pbind(C, V.vleg, V.t, crequire(C, V.vleg, at(V.vleg, k, legs), E.Wrong_leg{leg_msg}), lg => c_open_leg(C, op, ctx, lg))

def nonnegative(order: Cmp) -> Bool:
  match order:
    case LT{}: False{}
    case EQ{}: True{}
    case GT{}: True{}

def less_than(order: Cmp) -> Bool:
  match order:
    case LT{}: True{}
    case EQ{}: False{}
    case GT{}: False{}

def leg_bound(~C: Data, +k: F.Int63.t, n: F.Int63.t) -> program<C, Unit>:
  guard(C, Bool.and(nonnegative(F.Int63.compare(k, F.Int63.zero)), less_than(F.Int63.compare(k, n))), E.Wrong_leg{leg_msg})

def samples() -> List<&2, S.t<T.t>>:
  [S.SPi{Q.Many{}, "x", T.Univ{L.zero}}, S.SColl{F.Int63.zero}, S.SPar{T.Univ{L.zero}, T.Univ{L.zero}}, S.SMu{"x", []}, S.SNu{"x", []}]

def present_pack(value: result(pack), name: String, tail: List<&2, F.Pair2<String, pack>>) -> List<&2, F.Pair2<String, pack>>:
  match value:
    case Fail{_}: tail
    case Done{p}: F.Pair2{name, p} <> tail

def collect_packs(xs: List<&2, S.t<T.t>>) -> List<&2, F.Pair2<String, pack>>:
  match xs:
    case []: []
    case +s <> rest: present_pack(rules(T.t, s), S.name(T.t, s), collect_packs(rest))

def packs() -> List<&2, F.Pair2<String, pack>>: collect_packs(samples)

def named_declared() -> List<&2, String>: ["proof-irrelevance", "subsingleton-large-elimination", "literal-fast-path"]

def named_present() -> List<&2, String>: ["proof-irrelevance", "subsingleton-large-elimination", "literal-fast-path"]

def family_params(f: Pos.family) -> Pos.telescope:
  match f:
    case Pos.Family{_, ps, _, _, _, _, _}: ps

def family_indices(f: Pos.family) -> Pos.telescope:
  match f:
    case Pos.Family{_, _, ix, _, _, _, _}: ix

def family_level(f: Pos.family) -> L.t:
  match f:
    case Pos.Family{_, _, _, l, _, _, _}: l

def ctor_args(c: Pos.ctor) -> Pos.telescope:
  match c:
    case Pos.Ctor{_, args, _, _, _}: args

def ctor_indices(c: Pos.ctor) -> List<&2, T.t>:
  match c:
    case Pos.Ctor{_, _, ix, _, _}: ix

def as_mu(-A: Data, shape: S.t<A>) -> Maybe<&2, F.Pair2<String, List<&2, A>>>:
  match shape:
    case S.SPi{_, _, _}: None{}
    case S.SColl{_}: None{}
    case S.SPar{_, _}: None{}
    case S.SMu{n, ix}: Some{F.Pair2{n, ix}}
    case S.SNu{_, _}: None{}

def mu_family_status(~C: Data, +f: Pos.family, view: Pos.family) -> program<C, Pos.family>:
  match view:
    case Pos.Family{_, _, _, _, status, _, positive}:
      match status:
        case Pos.Provisional{}: Pure{f}
        case Pos.Builtin{}: Pure{f}
        case Pos.Complete{_}: pchoose(C, Pos.family, positive, _ => Pure{f}, _ => Abort{E.Not_yet{Pos.nonpositive_word}})

def mu_family(~C: Data, ~op: check_ops<C>, ctx: C, +n: String) -> program<C, Pos.family>:
  pbind(C, Pos.family, Pos.family, crequire(C, Pos.family, o_family(C, c_runtime(C, op), ctx, n), E.Unbound{String.concat(["the family ", n, " is not declared"])}), +f => mu_family_status(C, f, f))

def leg_bodies(legs: List<&2, T.leg>) -> List<&2, T.t>:
  match legs:
    case []: []
    case T.Leg{_, body} <> rest: body <> leg_bodies(rest)

def mu_params_of(~C: Data, diagram: T.t) -> program<C, List<&2, T.t>>:
  pbind(C, F.Pair2<S.t<T.t>, List<&2, T.leg>>, List<&2, T.t>, crequire(C, F.Pair2<S.t<T.t>, List<&2, T.leg>>, as_tsec(diagram), E.Mismatch{diagram_msg}), sec =>
    split(C, S.t<T.t>, List<&2, T.leg>, List<&2, T.t>, sec, _ => legs => Pure{leg_bodies(legs)}))

def c_eval_env(~C: Data, ~op: check_ops<C>, ctx: C, env: List<&2, V.t>, tm: T.t) -> program<C, V.t>:
  lift(C, V.t, o_eval_env(C, c_runtime(C, op), ctx, env, tm))

def mu_param_legs(~C: Data, ~op: check_ops<C>, +ctx: C, legs: List<&2, V.vleg>, env: List<&2, V.t>) -> program<C, List<&2, V.t>>:
  match legs:
    case []: Pure{env}
    case lg <> rest: pbind(C, V.t, List<&2, V.t>, c_open_leg(C, op, ctx, lg), v => mu_param_legs(C, op, ctx, rest, v <> env))

def mu_param_env(~C: Data, ~op: check_ops<C>, +ctx: C, d: V.closure) -> program<C, List<&2, V.t>>:
  pbind(C, List<&2, V.vleg>, List<&2, V.t>, coll_legs_of(C, op, ctx, d), legs => mu_param_legs(C, op, ctx, legs, []))

def mu_index_walk(~C: Data, ~op: check_ops<C>, +ctx: C, +n: String, +ctor: String, indices: List<&2, T.t>, wants: List<&2, V.t>, +env: List<&2, V.t>) -> program<C, Unit>:
  match indices, wants:
    case [], []: Pure{Unit{}}
    case [], _ <> _: Abort{E.Mismatch{wrong_pack}}
    case _ <> _, []: Abort{E.Mismatch{wrong_pack}}
    case r <> rest, +want <> tail:
      pbind(C, V.t, Unit, c_eval_env(C, op, ctx, env, r), +got =>
        pbind(C, Bool, Unit, c_conv_type(C, op, ctx, got, want), eq =>
          pbind(C, Unit, Unit, guard(C, eq, E.Mismatch{String.concat(["the constructor ", ctor, " of ", n, " gives the index ", c_pp(C, op, ctx, got), " and the type asks for ", c_pp(C, op, ctx, want)])}), _ => mu_index_walk(C, op, ctx, n, ctor, rest, tail, env))))

def mu_indices(~C: Data, ~op: check_ops<C>, ctx: C, +n: String, +ct: Pos.ctor, +ixv: List<&2, V.t>, env: List<&2, V.t>) -> program<C, Unit>:
  pbind(C, Unit, Unit, guard(C, F.Int63.equal(length(T.t, ctor_indices(ct)), length(V.t, ixv)), E.Mismatch{String.concat([Pos.ctor_name(ct), " gives ", F.Int63.show(length(T.t, ctor_indices(ct))), " result indices and ", n, " takes ", F.Int63.show(length(V.t, ixv))])}), _ => mu_index_walk(C, op, ctx, n, Pos.ctor_name(ct), ctor_indices(ct), ixv, env))

def mu_motive_view(~C: Data, +n: String, +want: F.Int63.t, +applied: F.Int63.t, +mo: T.motive, view: T.motive) -> program<C, T.motive>:
  match view:
    case T.Motive{ind, +idx, _, _}:
      pbind(C, String, T.motive, crequire(C, String, ind, E.Mismatch{String.concat(["the motive of an elimination at ", n, " names no family"])}), +mn =>
        pbind(C, Unit, T.motive, guard(C, String.eq(mn, n), E.Mismatch{String.concat(["the motive is built for ", mn, " and the scrutinee is at ", n])}), _ =>
          pbind(C, Unit, T.motive, guard(C, F.Int63.equal(length(String, idx), want), E.Mismatch{String.concat(["the motive of ", n, " binds ", F.Int63.show(length(String, idx)), " indices and the family has ", F.Int63.show(want)])}), _ =>
            pmap(C, Unit, T.motive, _ => mo, guard(C, F.Int63.equal(applied, want), E.Mismatch{String.concat([n, " is applied to ", F.Int63.show(applied), " indices and the family has ", F.Int63.show(want)])})))))

def mu_motive_of(~C: Data, n: String, fam: Pos.family, ixv: List<&2, V.t>, mo: Maybe<&2, T.motive>) -> program<C, T.motive>:
  pbind(C, T.motive, T.motive, crequire(C, T.motive, mo, E.Cannot_infer{"an elimination at a mu shape needs a motive"}), +m => mu_motive_view(C, n, length(F.Pair2<Q.t, F.Pair2<String, T.t>>, family_indices(fam)), length(V.t, ixv), m, m))

def mu_result(~C: Data, ~op: check_ops<C>, +ctx: C, mo: T.motive, idx: List<&2, V.t>, self: V.t) -> program<C, V.t>:
  match mo:
    case T.Motive{_, _, _, body}: c_eval_env(C, op, ctx, self <> rev_append(V.t, idx, o_env(C, c_runtime(C, op), ctx)), body)

type binding_state<-C: Data> is Data:
  BindingState{context: C, env: List<&2, V.t>, values: List<&2, V.t>}

def bound_state(~C: Data, ~op: check_ops<C>, +ctx: C, env: List<&2, V.t>, vals: List<&2, V.t>, name: String, q: Q.t, ty: V.t) -> binding_state<C>:
  BindingState{c_bind(C, op, ctx, name, q, ty), V.var(c_size(C, op, ctx)) <> env, V.var(c_size(C, op, ctx)) <> vals}

def mu_bind_indices(~C: Data, ~op: check_ops<C>, +origin: C, tele: Pos.telescope, names: List<&2, String>, state: binding_state<C>) -> program<C, binding_state<C>>:
  match tele, names, state:
    case [], [], BindingState{ctx, env, vals}: Pure{BindingState{ctx, env, vals}}
    case [], _ <> _, BindingState{_, _, _}: Abort{E.Mismatch{wrong_pack}}
    case _ <> _, [], BindingState{_, _, _}: Abort{E.Mismatch{wrong_pack}}
    case F.Pair2{_, F.Pair2{_, ty}} <> rest, name <> tail, BindingState{ctx, +env, vals}:
      pbind(C, V.t, binding_state<C>, c_eval_env(C, op, origin, env, ty), tyv => mu_bind_indices(C, op, origin, rest, tail, bound_state(C, op, ctx, env, vals, name, Q.Zero{}, tyv)))

def mu_large(~C: Data, n: String, +fam: Pos.family, mlvl: L.t) -> program<C, Unit>:
  guard(C, Bool.or(Bool.not(L.equal(family_level(fam), L.zero)), Bool.or(L.equal(mlvl, L.zero), zero_family_view(fam, fam))), E.Universe{String.concat(["a large elimination out of a proposition needs a subsingleton family at ", n])})

def mu_ctor_names(~C: Data, n: String, fam: Pos.family) -> program<C, List<&2, String>>:
  match fam:
    case Pos.Family{_, _, _, _, status, _, _}:
      match status:
        case Pos.Complete{names}: Pure{names}
        case Pos.Provisional{}: Abort{E.Unbound{String.concat(["the family ", n, " is still under declaration"])}}
        case Pos.Builtin{}: Abort{E.Mismatch{String.concat(["the family ", n, " is a kernel family and has no branches"])}}

def mu_keys(~C: Data, +n: String, branches: List<&2, F.Pair2<T.addr, T.leg>>) -> program<C, List<&2, String>>:
  match branches:
    case []: Pure{[]}
    case F.Pair2{a, _} <> rest:
      pbind(C, String, List<&2, String>, crequire(C, String, T.as_actor(a), E.Wrong_leg{String.concat(["a branch of ", n, " takes the constructor address"])}), key => pmap(C, List<&2, String>, List<&2, String>, tail => key <> tail, mu_keys(C, n, rest)))

def mu_count(+key: String, names: List<&2, String>) -> F.Int63.t:
  match names:
    case []: F.Int63.zero
    case n <> rest: F.Int63.add(F.choose(F.Int63.t, String.eq(key, n), F.Int63.one, F.Int63.zero), mu_count(key, rest))

def mu_cover_names(~C: Data, +n: String, names: List<&2, String>, +keys: List<&2, String>) -> program<C, Unit>:
  match names:
    case []: Pure{Unit{}}
    case +c <> rest:
      pbind(C, Unit, Unit, guard(C, F.Int63.equal(mu_count(c, keys), F.Int63.one), F.choose(E.t, F.Int63.equal(mu_count(c, keys), F.Int63.zero), E.Missing_branch{String.concat(["the elimination of ", n, " has no branch at ", c])}, E.Wrong_leg{String.concat(["the elimination of ", n, " repeats the branch at ", c])})), _ => mu_cover_names(C, n, rest, keys))

def mu_cover_keys(~C: Data, +n: String, +names: List<&2, String>, keys: List<&2, String>) -> program<C, Unit>:
  match keys:
    case []: Pure{Unit{}}
    case +key <> rest: pbind(C, Unit, Unit, guard(C, T.member(key, names), E.Unbound{String.concat([key, " is not a constructor of ", n])}), _ => mu_cover_keys(C, n, names, rest))

def mu_cover(~C: Data, +n: String, +names: List<&2, String>, branches: List<&2, F.Pair2<T.addr, T.leg>>) -> program<C, Unit>:
  pbind(C, List<&2, String>, Unit, mu_keys(C, n, branches), +keys => pbind(C, Unit, Unit, mu_cover_names(C, n, names, keys), _ => mu_cover_keys(C, n, names, keys)))

def mu_bind_fields(~C: Data, ~op: check_ops<C>, +origin: C, tele: Pos.telescope, binders: List<&2, F.Pair2<Q.t, String>>, state: binding_state<C>) -> program<C, binding_state<C>>:
  match tele, binders, state:
    case [], [], BindingState{ctx, env, vals}: Pure{BindingState{ctx, env, vals}}
    case [], _ <> _, BindingState{_, _, _}: Abort{E.Mismatch{wrong_pack}}
    case _ <> _, [], BindingState{_, _, _}: Abort{E.Mismatch{wrong_pack}}
    case F.Pair2{+q, F.Pair2{_, ty}} <> rest, F.Pair2{bq, +bx} <> tail, BindingState{ctx, +env, vals}:
      pbind(C, V.t, binding_state<C>, c_eval_env(C, op, origin, env, ty), tyv =>
        pbind(C, Unit, binding_state<C>, binder_quantity(C, "branch binder", "field", bx, bq, q), _ => mu_bind_fields(C, op, origin, rest, tail, bound_state(C, op, ctx, env, vals, bx, q, tyv))))

def mu_values(~C: Data, ~op: check_ops<C>, +ctx: C, +env: List<&2, V.t>, terms: List<&2, T.t>) -> program<C, List<&2, V.t>>:
  match terms:
    case []: Pure{[]}
    case tm <> rest: pbind(C, V.t, List<&2, V.t>, c_eval_env(C, op, ctx, env, tm), v => pmap(C, List<&2, V.t>, List<&2, V.t>, tail => v <> tail, mu_values(C, op, ctx, env, rest)))

def annotation(value: V.t) -> Maybe<&2, L.t>:
  match value:
    case V.VUniv{_}: None{}
    case V.VLan{_, _, u}: u
    case V.VRan{_, _, u}: u
    case V.VIn{_, _, _}: None{}
    case V.VSec{_, _}: None{}
    case V.VLit{_}: None{}
    case V.VNeutral{_, _}: None{}

def level_answer(~C: Data, value: result(answer)) -> program<C, L.t>:
  match value:
    case Fail{e}: Abort{e}
    case Done{LevelAnswer{level}}: Pure{level}
    case Done{ValueAnswer{_}}: Abort{E.Mismatch{"internal universe answer"}}
    case Done{TermAnswer{_}}: Abort{E.Mismatch{"internal universe answer"}}
    case Done{BoolAnswer{_}}: Abort{E.Mismatch{"internal universe answer"}}
    case Done{InferredAnswer{_, _}}: Abort{E.Mismatch{"internal universe answer"}}
    case Done{UsageAnswer{_}}: Abort{E.Mismatch{"internal universe answer"}}

def ask_univ(~C: Data, ctx: C, tm: T.t) -> program<C, L.t>:
  Request{InferUniverse{ctx, tm}, answer => level_answer(C, answer)}

def c_infer(~C: Data, ~op: check_ops<C>, ctx: C, q: Q.t, tm: T.t) -> program<C, inferred>:
  Request{InferUses{ctx, q, tm}, answer => inferred_answer(C, answer)}

def c_check(~C: Data, ~op: check_ops<C>, ctx: C, q: Q.t, tm: T.t, ty: V.t) -> program<C, Q.usage>:
  Request{CheckUses{ctx, q, tm, ty}, answer => usage_answer(C, answer)}

def c_univ(~C: Data, ~op: check_ops<C>, ctx: C, tm: T.t) -> program<C, L.t>: ask_univ(C, ctx, tm)

def with_former(~C: Data, ~A: Data, ~op: check_ops<C>, ctx: C, right: Bool, message: String, ty: V.t, next: V.t -> S.t<V.t> -> V.closure -> program<C, A>) -> program<C, A>:
  pbind(C, V.t, A, lift(C, V.t, o_whnf(C, c_runtime(C, op), ctx, ty)), +w =>
    pbind(C, V.Former, A, crequire(C, V.Former, F.choose(Maybe<&2, V.Former>, right, V.as_ran(w), V.as_lan(w)), E.Mismatch{message}), former =>
      former_apply(C, A, w, former, next)))

def infer_scrutinee(~C: Data, ~op: check_ops<C>, ctx: C, +mode: Q.t, +q: Q.t, scrut: T.t) -> program<C, inferred>:
  pbind(C, Unit, inferred, guard(C, Bool.not(Bool.and(Bool.not(Q.equal(mode, Q.Zero{})), Q.equal(q, Q.Zero{}))), E.Quantity{"an erased scrutinee cannot be eliminated at runtime"}), _ =>
    c_infer(C, op, ctx, Q.mul(mode, q), scrut))

def check_motive(~C: Data, ~op: check_ops<C>, ctx: C, mo: Maybe<&2, T.motive>, ty: V.t) -> program<C, Unit>:
  match mo:
    case None{}: Pure{Unit{}}
    case Some{T.Motive{_, _, self, body}}: pmap(C, L.t, Unit, _ => Unit{}, c_univ(C, op, c_bind(C, op, ctx, self, Q.Zero{}, ty), body))

def form_pi(~C: Data, ~op: check_ops<C>, +ctx: C, shape: S.t<T.t>, diagram: T.t, right: Bool) -> program<C, L.t>:
  match shape:
    case S.SPi{q, x, +dom}:
      pbind(C, L.t, L.t, c_univ(C, op, ctx, dom), a =>
        pbind(C, V.t, L.t, c_eval(C, op, ctx, dom), dv =>
          pmap(C, L.t, L.t, +b => F.choose(L.t, Bool.and(right, L.equal(b, L.zero)), L.zero, L.max(a, b)), c_univ(C, op, c_bind(C, op, ctx, x, q, dv), diagram))))
    case S.SColl{_}: Abort{E.Mismatch{wrong_pack}}
    case S.SPar{_, _}: Abort{E.Mismatch{wrong_pack}}
    case S.SMu{_, _}: Abort{E.Mismatch{wrong_pack}}
    case S.SNu{_, _}: Abort{E.Mismatch{wrong_pack}}

def pi_sec_body(~C: Data, ~op: check_ops<C>, ctx: C, +inner: C, +mode: Q.t, body: T.t, cod: V.t) -> program<C, Q.usage>:
  pbind(C, Q.usage, Q.usage, c_check(C, op, inner, Q.runtime(mode), body, cod), uses =>
    pmap(C, Q.usage, Q.usage, free => Q.scale(mode, Q.captures(free)), c_close(C, op, inner, c_size(C, op, ctx), mode, uses)))

def pi_intro_in(~C: Data, ~op: check_ops<C>, +ctx: C, +mode: Q.t, addr: T.addr, args: List<&2, T.t>, expected: V.t) -> program<C, Q.usage>:
  with_former(C, Q.usage, op, ctx, False{}, "a pair needs a left former as its expected type", expected, _ => s => d =>
    with_point(C, Q.usage, s, "a pair needs a point former", q => _ => dom =>
      pbind(C, F.Pair2<Q.t, T.t>, Q.usage, crequire(C, F.Pair2<Q.t, T.t>, T.as_apt(addr), E.Wrong_leg{"a pair takes the point address"}), a =>
        split(C, Q.t, T.t, Q.usage, a, _ => +point =>
          pbind(C, T.t, Q.usage, crequire(C, T.t, one_of(T.t, args), E.Mismatch{"a pair carries one fibre element"}), fibre =>
            pbind(C, Q.usage, Q.usage, c_check(C, op, ctx, Q.mul(mode, q), point, dom), pu =>
              pbind(C, V.t, Q.usage, c_eval(C, op, ctx, point), pv =>
                pbind(C, V.t, Q.usage, c_open(C, op, ctx, d, [pv]), cod =>
                  pmap(C, Q.usage, Q.usage, fu => Q.sequence(pu, fu), c_check(C, op, ctx, mode, fibre, cod))))))))))

def pi_elim_out(~C: Data, ~op: check_ops<C>, +ctx: C, +mode: Q.t, addr: T.addr, head: T.t) -> program<C, inferred>:
  pbind(C, F.Pair2<Q.t, T.t>, inferred, crequire(C, F.Pair2<Q.t, T.t>, T.as_apt(addr), E.Wrong_leg{"an application takes the point address"}), a =>
    split(C, Q.t, T.t, inferred, a, _ => +point =>
      pbind(C, inferred, inferred, c_infer(C, op, ctx, mode, head), inf =>
        split(C, V.t, Q.usage, inferred, inf, ty => hu =>
          with_former(C, inferred, op, ctx, True{}, "the head of an application is not a function", ty, _ => s => d =>
            with_point(C, inferred, s, "the head of an application is not a function", q => _ => dom =>
              pbind(C, Q.usage, inferred, c_check(C, op, ctx, Q.mul(mode, q), point, dom), pu =>
                pbind(C, V.t, inferred, c_eval(C, op, ctx, point), pv =>
                  pmap(C, V.t, inferred, ty => F.Pair2{ty, Q.sequence(hu, pu)}, c_open(C, op, ctx, d, [pv]))))))))))

def pi_branch_body(~C: Data, ~op: check_ops<C>, +ctx: C, +inner: C, +mode: Q.t, body: T.t, mo: Maybe<&2, T.motive>, expected: Maybe<&2, V.t>, self: V.t) -> program<C, Q.usage>:
  pbind(C, V.t, Q.usage, elim_result(C, op, ctx, mo, expected, self), target =>
    pbind(C, Q.usage, Q.usage, c_check(C, op, inner, Q.runtime(mode), body, target), uses => c_close(C, op, inner, c_size(C, op, ctx), mode, uses)))

def coll_levels(~C: Data, ~op: check_ops<C>, +ctx: C, legs: List<&2, T.leg>) -> program<C, L.t>:
  match legs:
    case []: Pure{L.zero}
    case T.Leg{_, body} <> rest:
      pbind(C, L.t, L.t, c_univ(C, op, ctx, body), level => pmap(C, L.t, L.t, tail => L.max(level, tail), coll_levels(C, op, ctx, rest)))

def form_coll(~C: Data, ~op: check_ops<C>, ctx: C, shape: S.t<T.t>, diagram: T.t, expected: Maybe<&2, L.t>) -> program<C, L.t>:
  pbind(C, F.Int63.t, L.t, crequire(C, F.Int63.t, coll_width(T.t, shape), E.Mismatch{wrong_pack}), +n =>
    pbind(C, F.Pair2<S.t<T.t>, List<&2, T.leg>>, L.t, crequire(C, F.Pair2<S.t<T.t>, List<&2, T.leg>>, as_tsec(diagram), E.Mismatch{diagram_msg}), sec =>
      split(C, S.t<T.t>, List<&2, T.leg>, L.t, sec, ds => +legs =>
        pbind(C, F.Int63.t, L.t, crequire(C, F.Int63.t, coll_width(T.t, ds), E.Mismatch{diagram_msg}), dn =>
          pbind(C, Unit, L.t, guard(C, Bool.and(F.Int63.equal(dn, n), F.Int63.equal(length(T.leg, legs), n)), E.Mismatch{diagram_msg}), _ =>
            pbind(C, Unit, L.t, guard(C, diagram_unbound(legs), E.Mismatch{"a diagram leg binds nothing"}), _ =>
              pmap(C, L.t, L.t, l => F.choose(L.t, F.Int63.equal(n, F.Int63.zero), level_default(expected), l), coll_levels(C, op, ctx, legs))))))))

def coll_tuple(~C: Data, ~op: check_ops<C>, +ctx: C, +mode: Q.t, legs: List<&2, T.leg>, dlegs: List<&2, V.vleg>) -> program<C, Q.usage>:
  match legs, dlegs:
    case [], []: Pure{Q.empty}
    case [], _ <> _: Abort{E.Mismatch{diagram_msg}}
    case _ <> _, []: Abort{E.Mismatch{diagram_msg}}
    case T.Leg{_, body} <> rest, dl <> tail:
      pbind(C, V.t, Q.usage, c_open_leg(C, op, ctx, dl), ty =>
        pbind(C, Q.usage, Q.usage, c_check(C, op, ctx, mode, body, ty), uses =>
          pmap(C, Q.usage, Q.usage, more => Q.sequence(uses, more), coll_tuple(C, op, ctx, mode, rest, tail))))

def coll_intro_sec(~C: Data, ~op: check_ops<C>, +ctx: C, mode: Q.t, s: S.t<T.t>, +legs: List<&2, T.leg>, expected: V.t) -> program<C, Q.usage>:
  pbind(C, F.Int63.t, Q.usage, crequire(C, F.Int63.t, coll_width(T.t, s), E.Mismatch{wrong_pack}), +n =>
    with_former(C, Q.usage, op, ctx, True{}, "a tuple needs a right former as its expected type", expected, _ => vs => d =>
      pbind(C, F.Int63.t, Q.usage, crequire(C, F.Int63.t, collection_view(vs), E.Mismatch{wrong_pack}), dn =>
        pbind(C, Unit, Q.usage, guard(C, Bool.and(F.Int63.equal(dn, n), F.Int63.equal(length(T.leg, legs), n)), E.Mismatch{"the tuple width and the type width differ"}), _ =>
          pbind(C, List<&2, V.vleg>, Q.usage, coll_legs_of(C, op, ctx, d), dlegs => coll_tuple(C, op, ctx, mode, legs, dlegs))))))

def coll_intro_in(~C: Data, ~op: check_ops<C>, +ctx: C, mode: Q.t, addr: T.addr, args: List<&2, T.t>, expected: V.t) -> program<C, Q.usage>:
  with_former(C, Q.usage, op, ctx, False{}, "an injection needs a left former as its expected type", expected, _ => vs => d =>
    pbind(C, F.Int63.t, Q.usage, crequire(C, F.Int63.t, collection_view(vs), E.Mismatch{wrong_pack}), n =>
      pbind(C, F.Int63.t, Q.usage, crequire(C, F.Int63.t, T.as_aleg(addr), E.Wrong_leg{"an injection takes the leg address"}), +k =>
        pbind(C, Unit, Q.usage, leg_bound(C, k, n), _ =>
          pbind(C, List<&2, V.vleg>, Q.usage, coll_legs_of(C, op, ctx, d), dlegs =>
            pbind(C, V.t, Q.usage, coll_leg_ty(C, op, ctx, dlegs, k), ty =>
              pbind(C, T.t, Q.usage, crequire(C, T.t, one_of(T.t, args), E.Mismatch{"an injection carries one payload"}), payload => c_check(C, op, ctx, mode, payload, ty))))))))

def coll_elim_out(~C: Data, ~op: check_ops<C>, +ctx: C, mode: Q.t, addr: T.addr, head: T.t) -> program<C, inferred>:
  pbind(C, F.Int63.t, inferred, crequire(C, F.Int63.t, T.as_aleg(addr), E.Wrong_leg{"a projection takes the leg address"}), +k =>
    pbind(C, inferred, inferred, c_infer(C, op, ctx, mode, head), inf =>
      split(C, V.t, Q.usage, inferred, inf, ty => uses =>
        with_former(C, inferred, op, ctx, True{}, "a projection needs a right former", ty, _ => vs => d =>
          pbind(C, F.Int63.t, inferred, crequire(C, F.Int63.t, collection_view(vs), E.Mismatch{wrong_pack}), n =>
            pbind(C, Unit, inferred, leg_bound(C, k, n), _ =>
              pbind(C, List<&2, V.vleg>, inferred, coll_legs_of(C, op, ctx, d), dlegs =>
                pmap(C, V.t, inferred, ty => F.Pair2{ty, uses}, coll_leg_ty(C, op, ctx, dlegs, k)))))))))

def coll_branch(~C: Data, ~op: check_ops<C>, +ctx: C, +mode: Q.t, vs: S.t<V.t>, dlegs: List<&2, V.vleg>, mo: Maybe<&2, T.motive>, expected: Maybe<&2, V.t>, branches: List<&2, F.Pair2<T.addr, T.leg>>, +k: F.Int63.t) -> program<C, Q.usage>:
  pbind(C, T.leg, Q.usage, crequire(C, T.leg, branch_leg(V.VALeg{k}, branches), E.Missing_branch{String.concat(["no branch at leg ", F.Int63.show(k)])}), lg =>
    with_leg(C, Q.usage, lg, bs => body =>
      pbind(C, F.Pair2<Q.t, String>, Q.usage, crequire(C, F.Pair2<Q.t, String>, one_of(F.Pair2<Q.t, String>, bs), E.Missing_branch{"each branch binds its payload once"}), b =>
        split(C, Q.t, String, Q.usage, b, bq => bx =>
          pbind(C, V.t, Q.usage, coll_leg_ty(C, op, ctx, dlegs, k), ty =>
            pi_branch_body(C, op, ctx, c_bind(C, op, ctx, bx, bq, ty), mode, body, mo, expected, V.VIn{vs, V.VALeg{k}, [V.var(c_size(C, op, ctx))]}))))))

def coll_branches(~C: Data, ~op: check_ops<C>, +ctx: C, +mode: Q.t, +vs: S.t<V.t>, +dlegs: List<&2, V.vleg>, +mo: Maybe<&2, T.motive>, +expected: Maybe<&2, V.t>, +branches: List<&2, F.Pair2<T.addr, T.leg>>, steps: List<&2, F.Pair2<T.addr, T.leg>>, +k: F.Int63.t) -> program<C, Q.usage>:
  match steps:
    case []: Pure{Q.unreachable}
    case _ <> rest:
      pbind(C, Q.usage, Q.usage, coll_branch(C, op, ctx, mode, vs, dlegs, mo, expected, branches, k), uses =>
        pmap(C, Q.usage, Q.usage, more => Q.alternative(uses, more), coll_branches(C, op, ctx, mode, vs, dlegs, mo, expected, branches, rest, F.Int63.succ(k))))

def coll_elim_elim(~C: Data, ~op: check_ops<C>, +ctx: C, +mode: Q.t, e: T.elim, +expected: Maybe<&2, V.t>) -> program<C, inferred>:
  match e:
    case T.Elimination{_, +scrut, sq, +mo, +branches}:
      pbind(C, inferred, inferred, infer_scrutinee(C, op, ctx, mode, sq, scrut), inf =>
        split(C, V.t, Q.usage, inferred, inf, ty => su =>
          with_former(C, inferred, op, ctx, False{}, "the scrutinee is not a left former", ty, w => +vs => d =>
            pbind(C, F.Int63.t, inferred, crequire(C, F.Int63.t, collection_view(vs), E.Mismatch{wrong_pack}), +n =>
              pbind(C, Unit, inferred, check_motive(C, op, ctx, mo, w), _ =>
                pbind(C, V.t, inferred, c_eval(C, op, ctx, scrut), sv =>
                  pbind(C, V.t, inferred, elim_result(C, op, ctx, mo, expected, sv), ty =>
                    pbind(C, Unit, inferred, guard(C, F.Int63.equal(length(F.Pair2<T.addr, T.leg>, branches), n), E.Missing_branch{String.concat(["the case has ", F.Int63.show(length(F.Pair2<T.addr, T.leg>, branches)), " branches and the type has ", F.Int63.show(n), " legs"])}), _ =>
                      pbind(C, List<&2, V.vleg>, inferred, coll_legs_of(C, op, ctx, d), dlegs =>
                        pmap(C, Q.usage, inferred, uses => F.Pair2{ty, Q.sequence(su, Q.scale(mode, uses))}, coll_branches(C, op, ctx, mode, vs, dlegs, mo, expected, branches, branches, F.Int63.zero)))))))))))

def elim_out_pack(~C: Data, ~op: check_ops<C>, ctx: C, mode: Q.t, p: pack, addr: T.addr, head: T.t) -> program<C, inferred>:
  match p:
    case PiPack{}: pi_elim_out(C, op, ctx, mode, addr, head)
    case CollPack{}: coll_elim_out(C, op, ctx, mode, addr, head)
    case MuPack{}: Abort{E.Not_yet{mu_ran_word}}

def elim_out(~C: Data, ~op: check_ops<C>, ctx: C, mode: Q.t, s: S.t<T.t>, addr: T.addr, head: T.t) -> program<C, inferred>:
  pbind(C, pack, inferred, lift(C, pack, rules(T.t, s)), p => elim_out_pack(C, op, ctx, mode, p, addr, head))

def tele_walk(~C: Data, ~op: check_ops<C>, +ctx: C, +mode: Q.t, tele: Pos.telescope, args: List<&2, T.t>, +env: List<&2, V.t>, uses: Q.usage) -> program<C, F.Pair2<List<&2, V.t>, Q.usage>>:
  match tele, args:
    case [], []: Pure{F.Pair2{env, uses}}
    case [], _ <> _: Abort{E.Mismatch{wrong_pack}}
    case _ <> _, []: Abort{E.Mismatch{wrong_pack}}
    case F.Pair2{q, F.Pair2{_, ty}} <> rest, +arg <> tail:
      pbind(C, V.t, F.Pair2<List<&2, V.t>, Q.usage>, c_eval_env(C, op, ctx, env, ty), tyv =>
        pbind(C, Q.usage, F.Pair2<List<&2, V.t>, Q.usage>, c_check(C, op, ctx, Q.mul(mode, q), arg, tyv), au =>
          pbind(C, V.t, F.Pair2<List<&2, V.t>, Q.usage>, c_eval(C, op, ctx, arg), v =>
            tele_walk(C, op, ctx, mode, rest, tail, v <> env, Q.sequence(uses, au)))))

def mu_telescope_uses(~C: Data, ~op: check_ops<C>, ctx: C, mode: Q.t, what: String, +tele: Pos.telescope, +args: List<&2, T.t>, env: List<&2, V.t>) -> program<C, F.Pair2<List<&2, V.t>, Q.usage>>:
  pbind(C, Unit, F.Pair2<List<&2, V.t>, Q.usage>, guard(C, F.Int63.equal(length(F.Pair2<Q.t, F.Pair2<String, T.t>>, tele), length(T.t, args)), E.Mismatch{String.concat([what, " takes ", F.Int63.show(length(F.Pair2<Q.t, F.Pair2<String, T.t>>, tele)), " arguments and the term gives ", F.Int63.show(length(T.t, args))])}), _ => tele_walk(C, op, ctx, mode, tele, args, env, Q.empty))

def mu_telescope(~C: Data, ~op: check_ops<C>, ctx: C, mode: Q.t, what: String, tele: Pos.telescope, args: List<&2, T.t>, env: List<&2, V.t>) -> program<C, List<&2, V.t>>:
  pbind(C, F.Pair2<List<&2, V.t>, Q.usage>, List<&2, V.t>, mu_telescope_uses(C, op, ctx, mode, what, tele, args, env), pair => split(C, List<&2, V.t>, Q.usage, List<&2, V.t>, pair, values => _ => Pure{values}))

def form_mu(~C: Data, ~op: check_ops<C>, +ctx: C, s: S.t<T.t>, diagram: T.t) -> program<C, L.t>:
  pbind(C, F.Pair2<String, List<&2, T.t>>, L.t, crequire(C, F.Pair2<String, List<&2, T.t>>, as_mu(T.t, s), E.Mismatch{wrong_pack}), view =>
    split(C, String, List<&2, T.t>, L.t, view, +n => ix =>
      pbind(C, Pos.family, L.t, mu_family(C, op, ctx, n), +fam =>
        pbind(C, List<&2, T.t>, L.t, mu_params_of(C, diagram), ps =>
          pbind(C, List<&2, V.t>, L.t, mu_telescope(C, op, ctx, Q.Zero{}, n, family_params(fam), ps, []), penv =>
            pmap(C, List<&2, V.t>, L.t, _ => family_level(fam), mu_telescope(C, op, ctx, Q.Zero{}, String.append("the indices of ", n), family_indices(fam), ix, penv)))))))

def mu_intro_in(~C: Data, ~op: check_ops<C>, +ctx: C, mode: Q.t, addr: T.addr, args: List<&2, T.t>, expected: V.t) -> program<C, Q.usage>:
  with_former(C, Q.usage, op, ctx, False{}, "a constructor needs a left former", expected, _ => vs => d =>
    pbind(C, F.Pair2<String, List<&2, V.t>>, Q.usage, crequire(C, F.Pair2<String, List<&2, V.t>>, as_mu(V.t, vs), E.Mismatch{wrong_pack}), view =>
      split(C, String, List<&2, V.t>, Q.usage, view, +n => ixv =>
        pbind(C, String, Q.usage, crequire(C, String, T.as_actor(addr), E.Wrong_leg{"a constructor takes a constructor address"}), +c =>
          pbind(C, Pos.family, Q.usage, mu_family(C, op, ctx, n), fam =>
            pbind(C, Pos.ctor, Q.usage, crequire(C, Pos.ctor, Pos.ctor_of(c, fam), E.Unbound{String.concat([c, " is not a constructor of ", n])}), +ct =>
              pbind(C, List<&2, V.t>, Q.usage, mu_param_env(C, op, ctx, d), penv =>
                pbind(C, F.Pair2<List<&2, V.t>, Q.usage>, Q.usage, mu_telescope_uses(C, op, ctx, mode, c, ctor_args(ct), args, penv), pair =>
                  split(C, List<&2, V.t>, Q.usage, Q.usage, pair, env => uses =>
                    pmap(C, Unit, Q.usage, _ => uses, mu_indices(C, op, ctx, n, ct, ixv, env)))))))))))

def mu_motive_body(~C: Data, ~op: check_ops<C>, n: String, self: String, body: T.t, d: V.closure, u: Maybe<&2, L.t>, state: binding_state<C>) -> program<C, L.t>:
  match state:
    case BindingState{ctx, _, vals}: c_univ(C, op, c_bind(C, op, ctx, self, Q.Zero{}, V.VLan{S.SMu{n, rev_append(V.t, vals, [])}, d, u}), body)

def mu_motive_lvl(~C: Data, ~op: check_ops<C>, +ctx: C, +n: String, +fam: Pos.family, mo: T.motive, d: V.closure, u: Maybe<&2, L.t>, penv: List<&2, V.t>) -> program<C, L.t>:
  match mo:
    case T.Motive{_, +idx, self, body}:
      pbind(C, Unit, L.t, guard(C, F.Int63.equal(length(F.Pair2<Q.t, F.Pair2<String, T.t>>, family_indices(fam)), length(String, idx)), E.Mismatch{String.concat(["the motive of ", n, " does not bind the indices"])}), _ =>
        pbind(C, binding_state<C>, L.t, mu_bind_indices(C, op, ctx, family_indices(fam), idx, BindingState{ctx, penv, []}), state => mu_motive_body(C, op, n, self, body, d, u, state)))

def mu_branch_body(~C: Data, ~op: check_ops<C>, +ctx: C, +mode: Q.t, n: String, ctor: String, mo: T.motive, indices: List<&2, T.t>, body: T.t, state: binding_state<C>) -> program<C, Q.usage>:
  match state:
    case BindingState{+inner, env, vals}:
      pbind(C, List<&2, V.t>, Q.usage, mu_values(C, op, ctx, env, indices), +idx =>
        pbind(C, V.t, Q.usage, mu_result(C, op, ctx, mo, idx, V.VIn{S.SMu{n, idx}, V.VACtor{ctor}, rev_append(V.t, vals, [])}), target =>
          pbind(C, Q.usage, Q.usage, c_check(C, op, inner, Q.runtime(mode), body, target), uses => c_close(C, op, inner, c_size(C, op, ctx), mode, uses))))

def mu_branch(~C: Data, ~op: check_ops<C>, +ctx: C, mode: Q.t, +n: String, fam: Pos.family, mo: T.motive, penv: List<&2, V.t>, branches: List<&2, F.Pair2<T.addr, T.leg>>, +c: String) -> program<C, Q.usage>:
  pbind(C, Pos.ctor, Q.usage, crequire(C, Pos.ctor, Pos.ctor_of(c, fam), E.Unbound{String.concat([c, " is not a constructor of ", n])}), +ct =>
    pbind(C, T.leg, Q.usage, crequire(C, T.leg, branch_leg(V.VACtor{c}, branches), E.Missing_branch{String.concat(["the elimination of ", n, " has no branch at ", c])}), lg =>
      with_leg(C, Q.usage, lg, +bs => body =>
        pbind(C, Unit, Q.usage, guard(C, F.Int63.equal(length(F.Pair2<Q.t, String>, bs), length(F.Pair2<Q.t, F.Pair2<String, T.t>>, ctor_args(ct))), E.Missing_branch{String.concat(["the branch at ", c, " binds ", F.Int63.show(length(F.Pair2<Q.t, String>, bs)), " fields and ", c, " takes ", F.Int63.show(length(F.Pair2<Q.t, F.Pair2<String, T.t>>, ctor_args(ct)))])}), _ =>
          pbind(C, binding_state<C>, Q.usage, mu_bind_fields(C, op, ctx, ctor_args(ct), bs, BindingState{ctx, penv, []}), state => mu_branch_body(C, op, ctx, mode, n, c, mo, ctor_indices(ct), body, state))))))

def mu_branches(~C: Data, ~op: check_ops<C>, +ctx: C, +mode: Q.t, +n: String, +fam: Pos.family, +mo: T.motive, +penv: List<&2, V.t>, +branches: List<&2, F.Pair2<T.addr, T.leg>>, names: List<&2, String>) -> program<C, Q.usage>:
  match names:
    case []: Pure{Q.unreachable}
    case c <> rest: pbind(C, Q.usage, Q.usage, mu_branch(C, op, ctx, mode, n, fam, mo, penv, branches, c), uses => pmap(C, Q.usage, Q.usage, tail => Q.alternative(uses, tail), mu_branches(C, op, ctx, mode, n, fam, mo, penv, branches, rest)))

def mu_elim_elim(~C: Data, ~op: check_ops<C>, +ctx: C, +mode: Q.t, e: T.elim) -> program<C, inferred>:
  match e:
    case T.Elimination{_, +scrut, sq, mo0, +branches}:
      pbind(C, inferred, inferred, infer_scrutinee(C, op, ctx, mode, sq, scrut), inf =>
        split(C, V.t, Q.usage, inferred, inf, ty => su =>
          with_former(C, inferred, op, ctx, False{}, "the scrutinee is not a left former", ty, w => vs => +d =>
            pbind(C, F.Pair2<String, List<&2, V.t>>, inferred, crequire(C, F.Pair2<String, List<&2, V.t>>, as_mu(V.t, vs), E.Mismatch{wrong_pack}), view =>
              split(C, String, List<&2, V.t>, inferred, view, +n => +ixv =>
                pbind(C, Pos.family, inferred, mu_family(C, op, ctx, n), +fam =>
                  pbind(C, T.motive, inferred, mu_motive_of(C, n, fam, ixv, mo0), +mo =>
                    pbind(C, List<&2, V.t>, inferred, mu_param_env(C, op, ctx, d), +penv =>
                      pbind(C, L.t, inferred, mu_motive_lvl(C, op, ctx, n, fam, mo, d, annotation(w), penv), mlvl =>
                        pbind(C, Unit, inferred, mu_large(C, n, fam, mlvl), _ =>
                          pbind(C, List<&2, String>, inferred, mu_ctor_names(C, n, fam), +names =>
                            pbind(C, Unit, inferred, mu_cover(C, n, names, branches), _ =>
                              pbind(C, Q.usage, inferred, mu_branches(C, op, ctx, mode, n, fam, mo, penv, branches, names), bu =>
                                pbind(C, V.t, inferred, c_eval(C, op, ctx, scrut), sv =>
                                  pmap(C, V.t, inferred, result_ty => F.Pair2{result_ty, Q.sequence(su, Q.scale(mode, bu))}, mu_result(C, op, ctx, mo, ixv, sv))))))))))))))))

def pi_intro_sec(~C: Data, ~op: check_ops<C>, +ctx: C, +mode: Q.t, legs: List<&2, T.leg>, expected: V.t) -> program<C, Q.usage>:
  with_former(C, Q.usage, op, ctx, True{}, "a section needs a right former as its expected type", expected, _ => s => d =>
    with_point(C, Q.usage, s, "a section at the point shape needs a point former", +q => _ => dom =>
      pbind(C, T.leg, Q.usage, crequire(C, T.leg, one_of(T.leg, legs), E.Mismatch{"a section at the point shape has one leg"}), lg =>
        with_leg(C, Q.usage, lg, bs => body =>
          pbind(C, F.Pair2<Q.t, String>, Q.usage, crequire(C, F.Pair2<Q.t, String>, one_of(F.Pair2<Q.t, String>, bs), E.Mismatch{"the leg binds the point once"}), b =>
            split(C, Q.t, String, Q.usage, b, bq => +bx =>
              pbind(C, Unit, Q.usage, binder_quantity(C, "binder", "type", bx, bq, q), _ =>
                pbind(C, V.t, Q.usage, c_open(C, op, ctx, d, [V.var(c_size(C, op, ctx))]), cod =>
                  pi_sec_body(C, op, ctx, c_bind(C, op, ctx, bx, q, dom), mode, body, cod)))))))))

def pi_branch(~C: Data, ~op: check_ops<C>, +ctx: C, +mode: Q.t, vs: S.t<V.t>, d: V.closure, +q: Q.t, dom: V.t, mo: Maybe<&2, T.motive>, expected: Maybe<&2, V.t>, key: T.addr, leg: T.leg) -> program<C, Q.usage>:
  pbind(C, F.Int63.t, Q.usage, crequire(C, F.Int63.t, T.as_aleg(key), E.Wrong_leg{"the branch takes the leg address"}), k =>
    pbind(C, Unit, Q.usage, guard(C, F.Int63.equal(k, F.Int63.zero), E.Wrong_leg{leg_msg}), _ =>
      with_leg(C, Q.usage, leg, bs => body =>
        pbind(C, F.Pair2<F.Pair2<Q.t, String>, F.Pair2<Q.t, String>>, Q.usage, crequire(C, F.Pair2<F.Pair2<Q.t, String>, F.Pair2<Q.t, String>>, two_of(F.Pair2<Q.t, String>, bs), E.Missing_branch{"the branch binds the point and the fibre element (D-M0-3)"}), pair =>
          split(C, F.Pair2<Q.t, String>, F.Pair2<Q.t, String>, Q.usage, pair, a => b =>
            split(C, Q.t, String, Q.usage, a, +q1 => +x1 =>
              split(C, Q.t, String, Q.usage, b, q2 => x2 =>
                pbind(C, Unit, Q.usage, binder_quantity(C, "branch binder", "type", x1, q1, q), _ =>
                  pbind(C, V.t, Q.usage, c_open(C, op, ctx, d, [V.var(c_size(C, op, ctx))]), cod =>
                    pi_branch_body(C, op, ctx, c_bind(C, op, c_bind(C, op, ctx, x1, q1, dom), x2, q2, cod), mode, body, mo, expected, V.VIn{vs, V.VAPt{q, V.var(c_size(C, op, ctx))}, [V.var(F.Int63.succ(c_size(C, op, ctx)))]}))))))))))

def pi_elim_elim(~C: Data, ~op: check_ops<C>, +ctx: C, +mode: Q.t, e: T.elim, +expected: Maybe<&2, V.t>) -> program<C, inferred>:
  match e:
    case T.Elimination{_, +scrut, sq, +mo, branches}:
      pbind(C, inferred, inferred, infer_scrutinee(C, op, ctx, mode, sq, scrut), inf =>
        split(C, V.t, Q.usage, inferred, inf, ty => su =>
          with_former(C, inferred, op, ctx, False{}, "the scrutinee is not a left former", ty, w => +vs => d =>
            with_point(C, inferred, vs, wrong_pack, q => _ => dom =>
              pbind(C, Unit, inferred, check_motive(C, op, ctx, mo, w), _ =>
                pbind(C, V.t, inferred, c_eval(C, op, ctx, scrut), sv =>
                  pbind(C, V.t, inferred, elim_result(C, op, ctx, mo, expected, sv), result_ty =>
                    pbind(C, F.Pair2<T.addr, T.leg>, inferred, crequire(C, F.Pair2<T.addr, T.leg>, one_of(F.Pair2<T.addr, T.leg>, branches), E.Missing_branch{"an elimination at the point shape has one branch"}), branch =>
                      split(C, T.addr, T.leg, inferred, branch, key => lg =>
                        pmap(C, Q.usage, inferred, uses => F.Pair2{result_ty, Q.sequence(su, Q.scale(mode, uses))}, pi_branch(C, op, ctx, mode, vs, d, q, dom, mo, expected, key, lg)))))))))))

def form_pack(~C: Data, ~op: check_ops<C>, ctx: C, p: pack, s: S.t<T.t>, d: T.t, expected: Maybe<&2, L.t>, right: Bool) -> program<C, L.t>:
  match p:
    case PiPack{}: form_pi(C, op, ctx, s, d, right)
    case CollPack{}: form_coll(C, op, ctx, s, d, expected)
    case MuPack{}: pchoose(C, L.t, right, _ => Abort{E.Not_yet{mu_ran_word}}, _ => form_mu(C, op, ctx, s, d))

def form(~C: Data, ~op: check_ops<C>, ctx: C, +s: S.t<T.t>, d: T.t, expected: Maybe<&2, L.t>, right: Bool) -> program<C, L.t>:
  pbind(C, pack, L.t, lift(C, pack, rules(T.t, s)), p => form_pack(C, op, ctx, p, s, d, expected, right))

def intro_sec_pack(~C: Data, ~op: check_ops<C>, ctx: C, mode: Q.t, p: pack, s: S.t<T.t>, legs: List<&2, T.leg>, expected: V.t) -> program<C, Q.usage>:
  match p:
    case PiPack{}: pi_intro_sec(C, op, ctx, mode, legs, expected)
    case CollPack{}: coll_intro_sec(C, op, ctx, mode, s, legs, expected)
    case MuPack{}: Abort{E.Not_yet{mu_ran_word}}

def intro_sec(~C: Data, ~op: check_ops<C>, ctx: C, mode: Q.t, +s: S.t<T.t>, legs: List<&2, T.leg>, expected: V.t) -> program<C, Q.usage>:
  pbind(C, pack, Q.usage, lift(C, pack, rules(T.t, s)), p => intro_sec_pack(C, op, ctx, mode, p, s, legs, expected))

def intro_in_pack(~C: Data, ~op: check_ops<C>, ctx: C, mode: Q.t, p: pack, addr: T.addr, args: List<&2, T.t>, expected: V.t) -> program<C, Q.usage>:
  match p:
    case PiPack{}: pi_intro_in(C, op, ctx, mode, addr, args, expected)
    case CollPack{}: coll_intro_in(C, op, ctx, mode, addr, args, expected)
    case MuPack{}: mu_intro_in(C, op, ctx, mode, addr, args, expected)

def intro_in(~C: Data, ~op: check_ops<C>, ctx: C, mode: Q.t, s: S.t<T.t>, addr: T.addr, args: List<&2, T.t>, expected: V.t) -> program<C, Q.usage>:
  pbind(C, pack, Q.usage, lift(C, pack, rules(T.t, s)), p => intro_in_pack(C, op, ctx, mode, p, addr, args, expected))

def elim_elim_pack(~C: Data, ~op: check_ops<C>, ctx: C, mode: Q.t, p: pack, e: T.elim, expected: Maybe<&2, V.t>) -> program<C, inferred>:
  match p:
    case PiPack{}: pi_elim_elim(C, op, ctx, mode, e, expected)
    case CollPack{}: coll_elim_elim(C, op, ctx, mode, e, expected)
    case MuPack{}: mu_elim_elim(C, op, ctx, mode, e)

def elim_elim(~C: Data, ~op: check_ops<C>, ctx: C, mode: Q.t, e: T.elim, expected: Maybe<&2, V.t>) -> program<C, inferred>:
  match e:
    case T.Elimination{+s, scrut, q, mo, bs}:
      pbind(C, pack, inferred, lift(C, pack, rules(T.t, s)), p => elim_elim_pack(C, op, ctx, mode, p, T.Elimination{s, scrut, q, mo, bs}, expected))
