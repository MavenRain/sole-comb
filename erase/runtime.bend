# Runtime scope operations from Kanon 69f3be51 lib/erase.ml:497-555.
# Explicit work stacks replace mutual recursion. InvalidStack is an internal
# invariant error; traversal of a finite erased term produces Done.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_quantity.bend as Q
import eterm.bend as K

type result<-A: Data> is Data:
  Done{value: A}
  InvalidStack{}
def is_lt(order: Cmp) -> Bool:
  match order:
    case LT{}: True{}
    case EQ{}: False{}
    case GT{}: False{}
def lt(a: F.Int63.t, b: F.Int63.t) -> Bool: is_lt(F.Int63.compare(a, b))
def ge(a: F.Int63.t, b: F.Int63.t) -> Bool: Bool.not(lt(a, b))
def scoped() -> Data: F.Pair2<F.Int63.t, K.ktm>
def scope_terms(+depth: F.Int63.t, ts: List<&2, K.ktm>) -> List<&2, scoped>:
  match ts:
    case []: []
    case t <> ts: F.Pair2{depth, t} <> scope_terms(depth, ts)
def scope_branches(+depth: F.Int63.t, bs: List<&2, K.kbranch>) -> List<&2, scoped>:
  match bs:
    case []: []
    case K.Branch{_, n, b} <> bs: F.Pair2{F.Int63.add(depth, n), b} <> scope_branches(depth, bs)
def children(+depth: F.Int63.t, t: K.ktm) -> List<&2, scoped>:
  match t:
    case K.KVar{_}: []
    case K.KLit{_}: []
    case K.KGlobal{_}: []
    case K.KErased{}: []
    case K.KLet{_, v, b}: [F.Pair2{depth, v}, F.Pair2{F.Int63.succ(depth), b}]
    case K.KClos{_, _, cs}: scope_terms(depth, cs)
    case K.KApp{h, args}: F.Pair2{depth, h} <> scope_terms(depth, args)
    case K.KTail{h, args}: F.Pair2{depth, h} <> scope_terms(depth, args)
    case K.KStruct{_, fs}: scope_terms(depth, fs)
    case K.KProj{_, _, x}: [F.Pair2{depth, x}]
    case K.KTag{_, _, fs}: scope_terms(depth, fs)
    case K.KCase{_, s, bs}: F.Pair2{depth, s} <> scope_branches(depth, bs)
    case K.KDelay{_, cs}: scope_terms(depth, cs)
    case K.KForce{x}: [F.Pair2{depth, x}]

type frame is Data:
  Leaf{term: K.ktm}
  Let{name: String}
  Clos{name: K.fid, arity: F.Int63.t, size: Nat}
  App{size: Nat}
  Tail{size: Nat}
  Struct{name: K.tid, size: Nat}
  Proj{name: K.tid, field: F.Int63.t}
  Tag{name: K.tid, tag: F.Int63.t, size: Nat}
  Case{name: K.tid, branches: List<&2, K.kbranch>}
  Delay{name: K.fid, size: Nat}
  Force{}
def frame_of(~A: Data, ~index: A -> F.Int63.t -> F.Int63.t, state: A, +depth: F.Int63.t, t: K.ktm) -> frame:
  match t:
    case K.KVar{+i}: Leaf{K.KVar{Q.if_else(F.Int63.t, ge(i, depth), _ => F.Int63.add(depth, index(state, F.Int63.sub(i, depth))), _ => i)}}
    case K.KLit{l}: Leaf{K.KLit{l}}
    case K.KGlobal{n}: Leaf{K.KGlobal{n}}
    case K.KErased{}: Leaf{K.KErased{}}
    case K.KLet{x, _, _}: Let{x}
    case K.KClos{f, n, cs}: Clos{f, n, List.length(&2, K.ktm, cs)}
    case K.KApp{_, args}: App{List.length(&2, K.ktm, args)}
    case K.KTail{_, args}: Tail{List.length(&2, K.ktm, args)}
    case K.KStruct{t, fs}: Struct{t, List.length(&2, K.ktm, fs)}
    case K.KProj{t, k, _}: Proj{t, k}
    case K.KTag{t, k, ps}: Tag{t, k, List.length(&2, K.ktm, ps)}
    case K.KCase{t, _, bs}: Case{t, bs}
    case K.KDelay{f, cs}: Delay{f, List.length(&2, K.ktm, cs)}
    case K.KForce{_}: Force{}
def arity(f: frame) -> Nat:
  match f:
    case Leaf{_}: 0n
    case Let{_}: 2n
    case Clos{_, _, n}: n
    case App{n}: (1n + n : Nat)
    case Tail{n}: (1n + n : Nat)
    case Struct{_, n}: n
    case Proj{_, _}: 1n
    case Tag{_, _, n}: n
    case Case{_, bs}: (1n + List.length(&2, K.kbranch, bs) : Nat)
    case Delay{_, n}: n
    case Force{}: 1n
type popped is Data:
  Popped{items: List<&2, K.ktm>, rest: List<&2, K.ktm>}
def pop(count: Nat, values: List<&2, K.ktm>, acc: List<&2, K.ktm>) -> result<popped>:
  match count:
    case Zero{}: Done{Popped{acc, values}}
    case Succ{n}:
      match values:
        case []: InvalidStack{}
        case v <> vs: pop(n, vs, v <> acc)
def pack_one(xs: List<&2, K.ktm>, make: K.ktm -> K.ktm) -> result<K.ktm>:
  match xs:
    case []: InvalidStack{}
    case x <> []: Done{make(x)}
    case _ <> _ <> _: InvalidStack{}
def pack_let(name: String, xs: List<&2, K.ktm>) -> result<K.ktm>:
  match xs:
    case []: InvalidStack{}
    case _ <> []: InvalidStack{}
    case v <> b <> []: Done{K.KLet{name, v, b}}
    case _ <> _ <> _ <> _: InvalidStack{}
def pack_app(xs: List<&2, K.ktm>, make: K.ktm -> List<&2, K.ktm> -> K.ktm) -> result<K.ktm>:
  match xs:
    case []: InvalidStack{}
    case h <> args: Done{make(h, args)}
def branch_cons(k: F.Int63.t, n: F.Int63.t, t: K.ktm, rest: result<List<&2, K.kbranch>>) -> result<List<&2, K.kbranch>>:
  match rest:
    case InvalidStack{}: InvalidStack{}
    case Done{bs}: Done{K.Branch{k, n, t} <> bs}
def zip_branches(bs: List<&2, K.kbranch>, ts: List<&2, K.ktm>) -> result<List<&2, K.kbranch>>:
  match bs, ts:
    case [], []: Done{[]}
    case [], _ <> _: InvalidStack{}
    case _ <> _, []: InvalidStack{}
    case K.Branch{k, n, _} <> bs, t <> ts: branch_cons(k, n, t, zip_branches(bs, ts))
def case_body(name: K.tid, scrutinee: K.ktm, bs: result<List<&2, K.kbranch>>) -> result<K.ktm>:
  match bs:
    case InvalidStack{}: InvalidStack{}
    case Done{branches}: Done{K.KCase{name, scrutinee, branches}}
def pack_case(name: K.tid, branches: List<&2, K.kbranch>, xs: List<&2, K.ktm>) -> result<K.ktm>:
  match xs:
    case []: InvalidStack{}
    case s <> ts: case_body(name, s, zip_branches(branches, ts))
def pack(f: frame, xs: List<&2, K.ktm>) -> result<K.ktm>:
  match f:
    case Leaf{t}: Done{t}
    case Let{name}: pack_let(name, xs)
    case Clos{name, n, _}: Done{K.KClos{name, n, xs}}
    case App{_}: pack_app(xs, h => args => K.KApp{h, args})
    case Tail{_}: pack_app(xs, h => args => K.KTail{h, args})
    case Struct{name, _}: Done{K.KStruct{name, xs}}
    case Proj{name, k}: pack_one(xs, x => K.KProj{name, k, x})
    case Tag{name, k, _}: Done{K.KTag{name, k, xs}}
    case Case{name, branches}: pack_case(name, branches, xs)
    case Delay{name, _}: Done{K.KDelay{name, xs}}
    case Force{}: pack_one(xs, x => K.KForce{x})
def push(value: result<K.ktm>, rest: List<&2, K.ktm>) -> result<List<&2, K.ktm>>:
  match value:
    case InvalidStack{}: InvalidStack{}
    case Done{t}: Done{t <> rest}
def rebuild(f: frame, values: result<popped>) -> result<List<&2, K.ktm>>:
  match values:
    case InvalidStack{}: InvalidStack{}
    case Done{Popped{xs, rest}}: push(pack(f, xs), rest)
type task is Data:
  Visit{depth: F.Int63.t, term: K.ktm}
  Build{frame: frame}
def schedule(xs: List<&2, scoped>, tail: List<&2, task>) -> List<&2, task>:
  match xs:
    case []: tail
    case F.Pair2{d, t} <> xs: Visit{d, t} <> schedule(xs, tail)
def finish(values: List<&2, K.ktm>) -> result<K.ktm>:
  match values:
    case []: InvalidStack{}
    case t <> []: Done{t}
    case _ <> _ <> _: InvalidStack{}
def resumed(value: result<List<&2, K.ktm>>, next: List<&2, K.ktm> -> result<K.ktm>) -> result<K.ktm>:
  match value:
    case InvalidStack{}: InvalidStack{}
    case Done{values}: next(values)
@unsafe
def map_loop(~A: Data, ~index: A -> F.Int63.t -> F.Int63.t, +state: A, todo: List<&2, task>, values: List<&2, K.ktm>) -> result<K.ktm>:
  match todo:
    case []: finish(values)
    case Visit{+depth, +t} <> rest:
      map_loop(A, index, state, schedule(children(depth, t), Build{frame_of(A, index, state, depth, t)} <> rest), values)
    case Build{+f} <> rest:
      resumed(rebuild(f, pop(arity(f), values, [])), vs => map_loop(A, index, state, rest, vs))
def reindex_runtime(~A: Data, ~index: A -> F.Int63.t -> F.Int63.t, state: A, depth: F.Int63.t, t: K.ktm) -> result<K.ktm>:
  map_loop(A, index, state, [Visit{depth, t}], [])
def shift_runtime(by: F.Int63.t, depth: F.Int63.t, t: K.ktm) -> result<K.ktm>:
  reindex_runtime(F.Int63.t, offset => i => F.Int63.add(i, offset), by, depth, t)

def append_scoped(xs: List<&2, scoped>, ys: List<&2, scoped>) -> List<&2, scoped>:
  match xs:
    case []: ys
    case x <> xs: x <> append_scoped(xs, ys)
def reverse_vars(xs: List<&2, F.Int63.t>, ys: List<&2, F.Int63.t>) -> List<&2, F.Int63.t>:
  match xs:
    case []: ys
    case x <> xs: reverse_vars(xs, x <> ys)
def record_var(+depth: F.Int63.t, t: K.ktm, +acc: List<&2, F.Int63.t>) -> List<&2, F.Int63.t>:
  match t:
    case K.KVar{+i}: F.choose(List<&2, F.Int63.t>, ge(i, depth), F.Int63.sub(i, depth) <> acc, acc)
    case K.KLit{_}: acc
    case K.KGlobal{_}: acc
    case K.KErased{}: acc
    case K.KLet{_, _, _}: acc
    case K.KClos{_, _, _}: acc
    case K.KApp{_, _}: acc
    case K.KTail{_, _}: acc
    case K.KStruct{_, _}: acc
    case K.KProj{_, _, _}: acc
    case K.KTag{_, _, _}: acc
    case K.KCase{_, _, _}: acc
    case K.KDelay{_, _}: acc
    case K.KForce{_}: acc
@unsafe
def vars_loop(todo: List<&2, scoped>, acc: List<&2, F.Int63.t>) -> List<&2, F.Int63.t>:
  match todo:
    case []: reverse_vars(acc, [])
    case F.Pair2{+depth, +t} <> rest: vars_loop(append_scoped(children(depth, t), rest), record_var(depth, t, acc))
def runtime_vars(depth: F.Int63.t, t: K.ktm) -> List<&2, F.Int63.t>: vars_loop([F.Pair2{depth, t}], [])

def insert(+x: F.Int63.t, xs: List<&2, F.Int63.t>) -> List<&2, F.Int63.t>:
  match xs:
    case []: [x]
    case +y <> +ys:
      Q.if_else(List<&2, F.Int63.t>, lt(x, y), _ => x <> y <> ys,
        _ => Q.if_else(List<&2, F.Int63.t>, F.Int63.equal(x, y), _ => y <> ys, _ => y <> insert(x, ys)))
def live_step(+params: F.Int63.t, count: F.Int63.t, +i: F.Int63.t, +rest: List<&2, F.Int63.t>) -> List<&2, F.Int63.t>:
  Q.if_else(List<&2, F.Int63.t>, Bool.and(ge(i, params), lt(i, F.Int63.add(params, count))),
    _ => insert(F.Int63.sub(i, params), rest), _ => rest)
def live_indices(+params: F.Int63.t, +count: F.Int63.t, xs: List<&2, F.Int63.t>) -> List<&2, F.Int63.t>:
  match xs:
    case []: []
    case i <> xs: live_step(params, count, i, live_indices(params, count, xs))
def member(+x: F.Int63.t, xs: List<&2, F.Int63.t>) -> Bool:
  match xs:
    case []: False{}
    case y <> ys: Bool.or(F.Int63.equal(x, y), member(x, ys))
def smaller(+x: F.Int63.t, xs: List<&2, F.Int63.t>) -> F.Int63.t:
  match xs:
    case []: F.Int63.zero
    case y <> ys: F.Int63.add(F.choose(F.Int63.t, lt(y, x), F.Int63.one, F.Int63.zero), smaller(x, ys))
def compact_index(+params: F.Int63.t, +live: List<&2, F.Int63.t>, +i: F.Int63.t) -> F.Int63.t:
  Q.if_else(F.Int63.t, lt(i, params), _ => i, _ => F.Int63.add(params, smaller(F.Int63.sub(i, params), live)))
def count_reprs(xs: List<&2, K.repr>) -> F.Int63.t:
  match xs:
    case []: F.Int63.zero
    case _ <> xs: F.Int63.succ(count_reprs(xs))
def select_step(~A: Data, keep: Bool, x: A, rest: List<&2, A>) -> List<&2, A>:
  match keep:
    case True{}: x <> rest
    case False{}: rest
def select(~A: Data, +live: List<&2, F.Int63.t>, xs: List<&2, A>, +index: F.Int63.t) -> List<&2, A>:
  match xs:
    case []: []
    case x <> xs: select_step(A, member(index, live), x, select(A, live, xs, F.Int63.pred(index)))
type pruned is Data:
  Pruned{captures: List<&2, K.repr>, args: List<&2, K.ktm>, body: K.ktm}
def capture_index(state: F.Pair2<F.Int63.t, List<&2, F.Int63.t>>, i: F.Int63.t) -> F.Int63.t:
  match state:
    case F.Pair2{params, live}: compact_index(params, live, i)
def pruned_body(ps: List<&2, K.repr>, args: List<&2, K.ktm>, value: result<K.ktm>) -> result<pruned>:
  match value:
    case InvalidStack{}: InvalidStack{}
    case Done{body}: Done{Pruned{ps, args, body}}
def prune_live(ps: List<&2, K.repr>, args: List<&2, K.ktm>, +params: F.Int63.t, body: K.ktm, +count: F.Int63.t, +live: List<&2, F.Int63.t>) -> result<pruned>:
  pruned_body(select(K.repr, live, ps, F.Int63.pred(count)), select(K.ktm, live, args, F.Int63.pred(count)),
    reindex_runtime(F.Pair2<F.Int63.t, List<&2, F.Int63.t>>, capture_index, F.Pair2{params, live}, F.Int63.zero, body))
def prune_count(ps: List<&2, K.repr>, args: List<&2, K.ktm>, +params: F.Int63.t, +body: K.ktm, +count: F.Int63.t) -> result<pruned>:
  prune_live(ps, args, params, body, count, live_indices(params, count, runtime_vars(F.Int63.zero, body)))
def prune_captures(+ps: List<&2, K.repr>, args: List<&2, K.ktm>, params: F.Int63.t, body: K.ktm) -> result<pruned>:
  prune_count(ps, args, params, body, count_reprs(ps))
