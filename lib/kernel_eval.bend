# Normalization by evaluation at Kanon 69f3be51 eval.ml.
# Shape dispatch stays in kernel_rules. Environments use indices;
# neutral locals use levels, and neutral spines are newest first.
import Base
import ./foundation.bend as F
import ./kernel_error.bend as E
import ./kernel_global.bend as G
import ./kernel_level.bend as L
import ./kernel_literal.bend as Lit
import ./kernel_prim.bend as P
import ./kernel_quantity.bend as Q
import ./kernel_rules.bend as R
import ./kernel_shape.bend as S
import ./kernel_term.bend as T
import ./kernel_value.bend as V

def leg_closures(+env: List<&2, V.t>, legs: List<&2, T.leg>) -> List<&2, V.vleg>:
  match legs:
    case []: []
    case T.Leg{binders, body} <> rest:
      V.VLeg{binders, V.close(env, body)} <> leg_closures(env, rest)

def fill(old: Maybe<&2, L.t>, inferred: Maybe<&2, L.t>) -> Maybe<&2, L.t>:
  match old:
    case None{}: inferred
    case Some{l}: Some{l}

def found_prim(found: Maybe<&2, G.prim_entry>) -> Maybe<&2, P.t>:
  match found:
    case None{}: None{}
    case Some{G.Primitive{_, prim}}: Some{prim}

def prim_answer(value: V.t, answer: R.result(Maybe<&2, V.t>)) -> R.result(V.t):
  match answer:
    case Fail{e}: Fail{e}
    case Done{None{}}: Done{value}
    case Done{Some{v}}: Done{v}

type recursive is Data:
  Recursive{body: T.t, index: F.Int63.t, frames: List<&2, V.spine>}

def guarded_intro(intro: Maybe<&2, V.Intro>) -> Bool:
  match intro:
    case None{}: False{}
    case Some{V.Intro{_, address, _}}:
      match address:
        case V.VAPt{_, _}: False{}
        case V.VALeg{_}: False{}
        case V.VACtor{_}: True{}

def annotate_former(core: T.t, universe: Maybe<&2, L.t>) -> T.t:
  match universe:
    case None{}: core
    case Some{l}: T.Ann{core, T.Univ{l}}

def quote_head(size: F.Int63.t, head: V.head) -> T.t:
  match head:
    case V.HLocal{level}: T.Var{F.Int63.pred(F.Int63.sub(size, level))}
    case V.HGlobal{name}: T.Global{name}

def frame_literal(frame: V.spine) -> Maybe<&2, Lit.t>:
  match frame:
    case V.SElim{_}: None{}
    case V.SOut{_, address}:
      match address:
        case V.VALeg{_}: None{}
        case V.VACtor{_}: None{}
        case V.VAPt{_, value}: V.as_lit(value)

def literal_next(literal: Maybe<&2, Lit.t>, next: Lit.t -> Maybe<&2, List<&2, Lit.t>>) -> Maybe<&2, List<&2, Lit.t>>:
  match literal:
    case None{}: None{}
    case Some{l}: next(l)

def request_eval(globals: G.t, env: List<&2, V.t>, term: T.t) -> R.program<G.t, V.t>:
  R.ask_eval(G.t, globals, env, term)

def request_whnf(globals: G.t, value: V.t) -> R.program<G.t, V.t>:
  R.ask_whnf(G.t, globals, value)

def request_quote(globals: G.t, size: F.Int63.t, value: V.t) -> R.program<G.t, T.t>:
  R.ask_quote(G.t, globals, size, value)

def ev(+globals: G.t) -> R.evaluator(G.t):
  env => term => request_eval(globals, env, term)

def eval_addr(globals: G.t, env: List<&2, V.t>, address: T.addr) -> R.program<G.t, V.vaddr>:
  match address:
    case T.APt{q, arg}: R.pmap(G.t, V.t, V.vaddr, v => V.VAPt{q, v}, request_eval(globals, env, arg))
    case T.ALeg{k}: R.Pure{V.VALeg{k}}
    case T.ACtor{name}: R.Pure{V.VACtor{name}}

def eval_entry(globals: G.t, name: String, found: Maybe<&2, G.entry>) -> R.program<G.t, V.t>:
  match found:
    case None{}: R.Abort{E.Unbound{name}}
    case Some{entry}:
      match entry:
        case G.Axiom{_}: R.Pure{V.VNeutral{V.HGlobal{name}, []}}
        case G.Prim{_}: R.Pure{V.VNeutral{V.HGlobal{name}, []}}
        case G.Def{G.Definition{_, body, reducible, recursion, _}}:
          match reducible, recursion:
            case False{}, None{}: R.Pure{V.VNeutral{V.HGlobal{name}, []}}
            case False{}, Some{_}: R.Pure{V.VNeutral{V.HGlobal{name}, []}}
            case True{}, Some{_}: R.Pure{V.VNeutral{V.HGlobal{name}, []}}
            case True{}, None{}: request_eval(globals, [], body)

def level_of_ann(globals: G.t, env: List<&2, V.t>, ty: T.t) -> R.program<G.t, Maybe<&2, L.t>>:
  R.pcatch(G.t, Maybe<&2, L.t>, R.pmap(G.t, V.t, Maybe<&2, L.t>, v => V.as_univ(v), request_eval(globals, env, ty)), _ => R.Pure{None{}})

def head_prim(globals: G.t, head: V.head) -> Maybe<&2, P.t>:
  match head:
    case V.HLocal{_}: None{}
    case V.HGlobal{name}: found_prim(G.find_prim(name, globals))

def spine_literals(spine: List<&2, V.spine>, acc: List<&2, Lit.t>) -> Maybe<&2, List<&2, Lit.t>>:
  match spine:
    case []: Some{acc}
    case frame <> rest: literal_next(frame_literal(frame), l => spine_literals(rest, l <> acc))

def prim_arity(value: V.t, prim: P.t, args: List<&2, Lit.t>, saturated: Bool) -> R.result(V.t):
  match saturated:
    case False{}: Done{value}
    case True{}: prim_answer(value, P.apply(prim, args))

def candidate_def(spine: List<&2, V.spine>, found: Maybe<&2, G.def_entry>) -> Maybe<&2, recursive>:
  match found:
    case None{}: None{}
    case Some{G.Definition{_, body, reducible, recursion, _}}:
      match reducible, recursion:
        case False{}, None{}: None{}
        case False{}, Some{_}: None{}
        case True{}, None{}: None{}
        case True{}, Some{k}: Some{Recursive{body, k, R.rev_append(V.spine, spine, [])}}

def guarded_index(value: V.t, next: Unit -> Bool, order: Cmp) -> Bool:
  match order:
    case LT{}: False{}
    case GT{}: next(Unit{})
    case EQ{}: guarded_intro(V.as_in(value))

def former(right: Bool, shape: S.t<T.t>, diagram: T.t, universe: Maybe<&2, L.t>) -> T.t:
  match right:
    case False{}: annotate_former(T.Lan{shape, diagram}, universe)
    case True{}: annotate_former(T.Ran{shape, diagram}, universe)

def quote_addr(globals: G.t, size: F.Int63.t, address: V.vaddr) -> R.program<G.t, T.addr>:
  match address:
    case V.VAPt{q, arg}: R.pmap(G.t, T.t, T.addr, t => T.APt{q, t}, request_quote(globals, size, arg))
    case V.VALeg{k}: R.Pure{T.ALeg{k}}
    case V.VACtor{name}: R.Pure{T.ACtor{name}}

def mapped_eval(state: F.Pair2<G.t, List<&2, V.t>>, term: T.t) -> R.program<G.t, V.t>:
  match state:
    case F.Pair2{globals, env}: request_eval(globals, env, term)

def mapped_quote(state: F.Pair2<G.t, F.Int63.t>, value: V.t) -> R.program<G.t, T.t>:
  match state:
    case F.Pair2{globals, size}: request_quote(globals, size, value)

def eval_global(+globals: G.t, +name: String) -> R.program<G.t, V.t>:
  eval_entry(globals, name, G.find(name, globals))

def annotate(globals: G.t, env: List<&2, V.t>, value: V.t, ty: T.t) -> R.program<G.t, V.t>:
  match value:
    case V.VLan{s, d, u}: R.pmap(G.t, Maybe<&2, L.t>, V.t, level => V.VLan{s, d, fill(u, level)}, level_of_ann(globals, env, ty))
    case V.VRan{s, d, u}: R.pmap(G.t, Maybe<&2, L.t>, V.t, level => V.VRan{s, d, fill(u, level)}, level_of_ann(globals, env, ty))
    case V.VUniv{l}: R.Pure{V.VUniv{l}}
    case V.VIn{s, a, args}: R.Pure{V.VIn{s, a, args}}
    case V.VSec{s, legs}: R.Pure{V.VSec{s, legs}}
    case V.VLit{l}: R.Pure{V.VLit{l}}
    case V.VNeutral{h, sp}: R.Pure{V.VNeutral{h, sp}}

def prim_arguments(value: V.t, prim: Maybe<&2, P.t>, literals: Maybe<&2, List<&2, Lit.t>>) -> R.result(V.t):
  match prim, literals:
    case None{}, None{}: Done{value}
    case None{}, Some{_}: Done{value}
    case Some{_}, None{}: Done{value}
    case Some{+p}, Some{+args}: prim_arity(value, p, args, R.nat_equal(List.length(&2, Lit.t, args), P.arity(p)))

def candidate_neutral(globals: G.t, neutral: Maybe<&2, F.Pair2<V.head, List<&2, V.spine>>>) -> Maybe<&2, recursive>:
  match neutral:
    case None{}: None{}
    case Some{F.Pair2{head, spine}}:
      match head:
        case V.HLocal{_}: None{}
        case V.HGlobal{name}: candidate_def(spine, G.find_def(name, globals))

def guarded_point(index: F.Int63.t, point: Maybe<&2, F.Pair2<Q.t, V.t>>, next: Unit -> Bool) -> Bool:
  match point:
    case None{}: False{}
    case Some{F.Pair2{_, value}}: guarded_index(value, next, F.Int63.compare(index, F.Int63.zero))

def replay_frames(+globals: G.t, frames: List<&2, V.spine>, value: V.t) -> R.program<G.t, V.t>:
  match frames:
    case []: R.Pure{value}
    case V.SOut{s, a} <> rest:
      R.pbind(G.t, V.t, V.t, R.out_value(G.t, ev(globals), s, a, value), v =>
        R.pbind(G.t, V.t, V.t, request_whnf(globals, v), w => replay_frames(globals, rest, w)))
    case V.SElim{V.Stuck{s, q, mo, branches, env}} <> rest:
      R.pbind(G.t, V.t, V.t, R.elim_value(G.t, ev(globals), s, q, mo, branches, env, value), v => replay_frames(globals, rest, v))

def quote_diagram(+globals: G.t, +size: F.Int63.t, shape: S.t<T.t>, diagram: V.closure, universe: Maybe<&2, L.t>, right: Bool, +arity: Nat) -> R.program<G.t, T.t>:
  R.pbind(G.t, V.t, T.t, R.open_closure(G.t, ev(globals), diagram, R.fresh(size, arity)), v =>
    R.pmap(G.t, T.t, T.t, body => former(right, shape, body, universe), request_quote(globals, R.advance(size, arity), v)))

def quote_leg_arity(+globals: G.t, +size: F.Int63.t, binders: List<&2, F.Pair2<Q.t, String>>, clo: V.closure, +arity: Nat) -> R.program<G.t, T.leg>:
  R.pbind(G.t, V.t, T.leg, R.open_closure(G.t, ev(globals), clo, R.fresh(size, arity)), v =>
    R.pmap(G.t, T.t, T.leg, body => T.Leg{binders, body}, request_quote(globals, R.advance(size, arity), v)))

def eval_shape(globals: G.t, env: List<&2, V.t>, shape: S.t<T.t>) -> R.program<G.t, S.t<V.t>>:
  R.map_shape(G.t, T.t, V.t, F.Pair2<G.t, List<&2, V.t>>, mapped_eval, F.Pair2{globals, env}, shape)

def eval_list(globals: G.t, env: List<&2, V.t>, args: List<&2, T.t>) -> R.program<G.t, List<&2, V.t>>:
  R.map_list(G.t, T.t, V.t, F.Pair2<G.t, List<&2, V.t>>, mapped_eval, F.Pair2{globals, env}, args)

def quote_shape(globals: G.t, size: F.Int63.t, shape: S.t<V.t>) -> R.program<G.t, S.t<T.t>>:
  R.map_shape(G.t, V.t, T.t, F.Pair2<G.t, F.Int63.t>, mapped_quote, F.Pair2{globals, size}, shape)

def quote_list(globals: G.t, size: F.Int63.t, args: List<&2, V.t>) -> R.program<G.t, List<&2, T.t>>:
  R.map_list(G.t, V.t, T.t, F.Pair2<G.t, F.Int63.t>, mapped_quote, F.Pair2{globals, size}, args)

def replay(globals: G.t, value: V.t, frames: List<&2, V.spine>) -> R.program<G.t, V.t>: replay_frames(globals, frames, value)

def prim_neutral(globals: G.t, value: V.t, neutral: Maybe<&2, F.Pair2<V.head, List<&2, V.spine>>>) -> R.result(V.t):
  match neutral:
    case None{}: Done{value}
    case Some{F.Pair2{head, spine}}: prim_arguments(value, head_prim(globals, head), spine_literals(spine, []))

def candidate(globals: G.t, value: V.t) -> Maybe<&2, recursive>:
  candidate_neutral(globals, V.as_neutral(value))

def guarded_frames(frames: List<&2, V.spine>, +index: F.Int63.t) -> Bool:
  match frames:
    case []: False{}
    case V.SElim{_} <> _: False{}
    case V.SOut{_, address} <> rest:
      guarded_point(index, V.as_pt(address), _ => guarded_frames(rest, F.Int63.pred(index)))

def quote_former(+globals: G.t, +size: F.Int63.t, +shape: S.t<V.t>, diagram: V.closure, universe: Maybe<&2, L.t>, right: Bool) -> R.program<G.t, T.t>:
  R.pbind(G.t, S.t<T.t>, T.t, quote_shape(globals, size, shape), s =>
    R.pbind(G.t, R.pack, T.t, R.lift(G.t, R.pack, R.rules(V.t, shape)), p => quote_diagram(globals, size, s, diagram, universe, right, R.diagram_arity(p))))

def quote_leg(+globals: G.t, +size: F.Int63.t, leg: V.vleg) -> R.program<G.t, T.leg>:
  match leg:
    case V.VLeg{+binders, clo}:
      quote_leg_arity(globals, size, binders, clo, List.length(&2, F.Pair2<Q.t, String>, binders))

def eval_step(+globals: G.t, +env: List<&2, V.t>, term: T.t) -> R.program<G.t, V.t>:
  match term:
    case T.Var{+index}:
      R.lift(G.t, V.t, R.require(V.t, R.at(V.t, index, env), E.Unbound{String.append("de Bruijn index ", String.append(F.Int63.show(index), " is outside the environment"))}))
    case T.Univ{level}: R.Pure{V.VUniv{level}}
    case T.Lit{literal}: R.Pure{V.VLit{literal}}
    case T.Lan{shape, diagram}:
      R.pmap(G.t, S.t<V.t>, V.t, s => V.VLan{s, V.close(env, diagram), None{}}, eval_shape(globals, env, shape))
    case T.Ran{shape, diagram}:
      R.pmap(G.t, S.t<V.t>, V.t, s => V.VRan{s, V.close(env, diagram), None{}}, eval_shape(globals, env, shape))
    case T.In{shape, address, args}:
      R.pbind(G.t, S.t<V.t>, V.t, eval_shape(globals, env, shape), s =>
        R.pbind(G.t, V.vaddr, V.t, eval_addr(globals, env, address), a =>
          R.pmap(G.t, List<&2, V.t>, V.t, xs => V.VIn{s, a, xs}, eval_list(globals, env, args))))
    case T.Sec{shape, legs}:
      R.pmap(G.t, S.t<V.t>, V.t, s => V.VSec{s, leg_closures(env, legs)}, eval_shape(globals, env, shape))
    case T.Out{shape, address, head}:
      R.pbind(G.t, S.t<V.t>, V.t, eval_shape(globals, env, shape), s =>
        R.pbind(G.t, V.vaddr, V.t, eval_addr(globals, env, address), a =>
          R.pbind(G.t, V.t, V.t, request_eval(globals, env, head), v =>
            R.pbind(G.t, V.t, V.t, R.out_value(G.t, ev(globals), s, a, v), applied => request_whnf(globals, applied)))))
    case T.Elim{T.Elimination{shape, scrut, q, motive, branches}}:
      R.pbind(G.t, S.t<V.t>, V.t, eval_shape(globals, env, shape), s =>
        R.pbind(G.t, V.t, V.t, request_eval(globals, env, scrut), v => R.elim_value(G.t, ev(globals), s, q, motive, branches, env, v)))
    case T.Let{_, _, value, body}:
      R.pbind(G.t, V.t, V.t, request_eval(globals, env, value), v => request_eval(globals, v <> env, body))
    case T.Ann{value, ty}:
      R.pbind(G.t, V.t, V.t, request_eval(globals, env, value), v => annotate(globals, env, v, ty))
    case T.Global{name}: eval_global(globals, name)
    case T.Auto{}: R.Abort{E.Not_yet{R.auto_word}}

def quote_spine(+globals: G.t, +size: F.Int63.t, frames: List<&2, V.spine>, term: T.t) -> R.program<G.t, T.t>:
  match frames:
    case []: R.Pure{term}
    case V.SOut{s, a} <> rest:
      R.pbind(G.t, S.t<T.t>, T.t, quote_shape(globals, size, s), shape =>
        R.pbind(G.t, T.addr, T.t, quote_addr(globals, size, a), address => quote_spine(globals, size, rest, T.Out{shape, address, term})))
    case V.SElim{V.Stuck{s, q, mo, branches, _}} <> rest:
      R.pbind(G.t, S.t<T.t>, T.t, quote_shape(globals, size, s), shape =>
        quote_spine(globals, size, rest, T.Elim{T.Elimination{shape, term, q, mo, branches}}))

def quote_frames(globals: G.t, size: F.Int63.t, term: T.t, frames: List<&2, V.spine>) -> R.program<G.t, T.t>:
  quote_spine(globals, size, frames, term)

def guarded_frame(index: F.Int63.t, frames: List<&2, V.spine>) -> Bool: guarded_frames(frames, index)

def whnf_guard(+globals: G.t, value: V.t, body: T.t, frames: List<&2, V.spine>, guarded: Bool) -> R.program<G.t, V.t>:
  match guarded:
    case False{}: R.Pure{value}
    case True{}: R.pbind(G.t, V.t, V.t, request_eval(globals, [], body), head => replay(globals, head, frames))

def prim_step(globals: G.t, +value: V.t) -> R.result(V.t):
  prim_neutral(globals, value, V.as_neutral(value))

def mapped_leg(state: F.Pair2<G.t, F.Int63.t>, leg: V.vleg) -> R.program<G.t, T.leg>:
  match state:
    case F.Pair2{globals, size}: quote_leg(globals, size, leg)

def whnf_candidate(globals: G.t, value: V.t, recursion: Maybe<&2, recursive>) -> R.program<G.t, V.t>:
  match recursion:
    case None{}: R.Pure{value}
    case Some{Recursive{body, index, +frames}}: whnf_guard(globals, value, body, frames, guarded_frame(index, frames))

def quote_legs(globals: G.t, size: F.Int63.t, legs: List<&2, V.vleg>) -> R.program<G.t, List<&2, T.leg>>:
  R.map_list(G.t, V.vleg, T.leg, F.Pair2<G.t, F.Int63.t>, mapped_leg, F.Pair2{globals, size}, legs)

def quote_step(+globals: G.t, +size: F.Int63.t, value: V.t) -> R.program<G.t, T.t>:
  match value:
    case V.VUniv{l}: R.Pure{T.Univ{l}}
    case V.VLit{l}: R.Pure{T.Lit{l}}
    case V.VLan{s, d, u}: quote_former(globals, size, s, d, u, False{})
    case V.VRan{s, d, u}: quote_former(globals, size, s, d, u, True{})
    case V.VIn{shape, address, args}:
      R.pbind(G.t, S.t<T.t>, T.t, quote_shape(globals, size, shape), s =>
        R.pbind(G.t, T.addr, T.t, quote_addr(globals, size, address), a =>
          R.pmap(G.t, List<&2, T.t>, T.t, xs => T.In{s, a, xs}, quote_list(globals, size, args))))
    case V.VSec{shape, legs}:
      R.pbind(G.t, S.t<T.t>, T.t, quote_shape(globals, size, shape), s =>
        R.pmap(G.t, List<&2, T.leg>, T.t, ls => T.Sec{s, ls}, quote_legs(globals, size, legs)))
    case V.VNeutral{head, spine}: quote_frames(globals, size, quote_head(size, head), R.rev_append(V.spine, spine, []))

def whnf_value(+globals: G.t, +value: V.t) -> R.program<G.t, V.t>:
  whnf_candidate(globals, value, candidate(globals, value))

def whnf_step(+globals: G.t, value: V.t) -> R.program<G.t, V.t>:
  R.pbind(G.t, V.t, V.t, R.lift(G.t, V.t, prim_step(globals, value)), v => whnf_value(globals, v))

def dispatch(job: R.task<G.t>) -> R.program<G.t, R.answer>:
  match job:
    case R.Evaluate{globals, env, term}: R.pmap(G.t, V.t, R.answer, v => R.ValueAnswer{v}, eval_step(globals, env, term))
    case R.Weak{globals, value}: R.pmap(G.t, V.t, R.answer, v => R.ValueAnswer{v}, whnf_step(globals, value))
    case R.Readback{globals, size, value}: R.pmap(G.t, T.t, R.answer, t => R.TermAnswer{t}, quote_step(globals, size, value))
    case R.Compare{_, _, _, _}: R.Abort{E.Mismatch{"internal conversion request in evaluation"}}
    case R.InferUses{_, _, _}: R.Abort{E.Mismatch{"internal checking request in evaluation"}}
    case R.CheckUses{_, _, _, _}: R.Abort{E.Mismatch{"internal checking request in evaluation"}}
    case R.InferUniverse{_, _}: R.Abort{E.Mismatch{"internal checking request in evaluation"}}

def eval(globals: G.t, env: List<&2, V.t>, term: T.t) -> R.result(V.t):
  R.run(G.t, dispatch, V.t, request_eval(globals, env, term))

def whnf(globals: G.t, value: V.t) -> R.result(V.t):
  R.run(G.t, dispatch, V.t, request_whnf(globals, value))

def quote(globals: G.t, size: F.Int63.t, value: V.t) -> R.result(T.t):
  R.run(G.t, dispatch, T.t, request_quote(globals, size, value))
