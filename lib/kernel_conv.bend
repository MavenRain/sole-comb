# Typed conversion at Kanon 69f3be51 conv.ml. Recursive comparisons are
# suspended through the rules interface; shape names stay in that module.
import Base
import ./foundation.bend as F
import ./kernel_error.bend as E
import ./kernel_level.bend as L
import ./kernel_literal.bend as Lit
import ./kernel_quantity.bend as Q
import ./kernel_rules.bend as R
import ./kernel_shape.bend as S
import ./kernel_term.bend as T
import ./kernel_value.bend as V

def prop_level(answer: R.result(L.t)) -> Bool:
  match answer:
    case Fail{_}: False{}
    case Done{level}: L.equal(level, L.zero)

def shape_equal(~C: Data, ctx: C, a: S.t<V.t>, b: S.t<V.t>) -> R.program<C, Bool>:
  R.shape_eq(C, C, ctx => a => b => R.ask_type(C, ctx, a, b), ctx, a, b)

def values_equal(~C: Data, ctx: C, a: List<&2, V.t>, b: List<&2, V.t>) -> R.program<C, Bool>:
  R.payload_eq(C, V.t, C, ctx => a => b => R.ask_type(C, ctx, a, b), ctx, a, b)

def point_equal(~C: Data, ctx: C, ty: Maybe<&2, V.t>, a: V.t, b: V.t) -> R.program<C, Bool>:
  match ty:
    case None{}: R.ask_type(C, ctx, a, b)
    case Some{t}: R.ask_conv(C, ctx, t, a, b)

def open_pair(~C: Data, ~ops: R.ops<C>, +ctx: C, +count: Nat, a: V.closure, b: V.closure) -> R.program<C, Bool>:
  match a, b:
    case V.Closure{env1, body1}, V.Closure{env2, body2}:
      R.pbind(C, V.t, Bool, R.lift(C, V.t, R.o_eval_env(C, ops, ctx, R.rev_append(V.t, R.fresh(R.o_size(C, ops, ctx), count), env1), body1)), v1 =>
        R.pbind(C, V.t, Bool, R.lift(C, V.t, R.o_eval_env(C, ops, ctx, R.rev_append(V.t, R.fresh(R.o_size(C, ops, ctx), count), env2), body2)), v2 =>
          R.ask_type(C, R.grow(C, ops, count, ctx), v1, v2)))

def branch_addr(~C: Data, ~ops: R.ops<C>, +ctx: C, env1: List<&2, V.t>, env2: List<&2, V.t>, a: T.addr, b: T.addr) -> R.program<C, Bool>:
  match a, b:
    case T.APt{_, x}, T.APt{_, y}:
      R.pbind(C, V.t, Bool, R.lift(C, V.t, R.o_eval_env(C, ops, ctx, env1, x)), v1 =>
        R.pbind(C, V.t, Bool, R.lift(C, V.t, R.o_eval_env(C, ops, ctx, env2, y)), v2 => R.ask_type(C, ctx, v1, v2)))
    case T.APt{_, _}, T.ALeg{_}: R.Pure{False{}}
    case T.APt{_, _}, T.ACtor{_}: R.Pure{False{}}
    case T.ALeg{_}, T.APt{_, _}: R.Pure{False{}}
    case T.ALeg{x}, T.ALeg{y}: R.Pure{F.Int63.equal(x, y)}
    case T.ALeg{_}, T.ACtor{_}: R.Pure{False{}}
    case T.ACtor{_}, T.APt{_, _}: R.Pure{False{}}
    case T.ACtor{_}, T.ALeg{_}: R.Pure{False{}}
    case T.ACtor{x}, T.ACtor{y}: R.Pure{String.eq(x, y)}

type BranchContext<-C: Data> is Data:
  BranchContext{context: C, left_env: List<&2, V.t>, right_env: List<&2, V.t>}

def motive_env(+size: F.Int63.t, +count: Nat, env: List<&2, V.t>) -> List<&2, V.t>:
  V.var(R.advance(size, count)) <> R.rev_append(V.t, R.fresh(size, count), env)

def spine_step_ran(~C: Data, ~ops: R.ops<C>, ctx: C, +shape: S.t<V.t>, addr: V.vaddr, former: Maybe<&2, V.Former>) -> R.program<C, Maybe<&2, R.SpineStep>>:
  match former:
    case None{}: R.Pure{None{}}
    case Some{V.Former{_, diagram, _}}:
      R.pbind(C, R.pack, Maybe<&2, R.SpineStep>, R.lift(C, R.pack, R.rules(V.t, shape)), p => R.spine_ty(C, ops, ctx, p, shape, diagram, addr))

def step_arg(step: Maybe<&2, R.SpineStep>) -> Maybe<&2, V.t>:
  match step:
    case None{}: None{}
    case Some{R.SpineStep{ty, _}}: ty

def step_next(step: Maybe<&2, R.SpineStep>) -> V.t:
  match step:
    case None{}: R.placeholder
    case Some{R.SpineStep{_, ty}}: ty

def subsingleton(~C: Data, ~ops: R.ops<C>, ctx: C, ty: V.t) -> R.program<C, Bool>:
  match ty:
    case V.VLan{+shape, _, _}:
      R.pbind(C, R.pack, Bool, R.lift(C, R.pack, R.rules(V.t, shape)), _ => R.Pure{R.subsingleton(C, ops, ctx, shape)})
    case V.VRan{_, _, _}: R.Pure{False{}}
    case V.VUniv{_}: R.Pure{False{}}
    case V.VLit{_}: R.Pure{False{}}
    case V.VIn{_, _, _}: R.Pure{False{}}
    case V.VSec{_, _}: R.Pure{False{}}
    case V.VNeutral{_, _}: R.Pure{False{}}

def is_prop(~C: Data, ~ops: R.ops<C>, +ctx: C, ty: V.t) -> Bool:
  prop_level(R.bind(T.t, L.t, R.o_quote(C, ops, ctx, ty), term => R.o_infer_univ(C, ops, ctx, term)))

def conv_addr(~C: Data, ctx: C, ty: Maybe<&2, V.t>, a: V.vaddr, b: V.vaddr) -> R.program<C, Bool>:
  match a, b:
    case V.VAPt{_, x}, V.VAPt{_, y}: point_equal(C, ctx, ty, x, y)
    case V.VAPt{_, _}, V.VALeg{_}: R.Pure{False{}}
    case V.VAPt{_, _}, V.VACtor{_}: R.Pure{False{}}
    case V.VALeg{_}, V.VAPt{_, _}: R.Pure{False{}}
    case V.VALeg{x}, V.VALeg{y}: R.Pure{F.Int63.equal(x, y)}
    case V.VALeg{_}, V.VACtor{_}: R.Pure{False{}}
    case V.VACtor{_}, V.VAPt{_, _}: R.Pure{False{}}
    case V.VACtor{_}, V.VALeg{_}: R.Pure{False{}}
    case V.VACtor{x}, V.VACtor{y}: R.Pure{String.eq(x, y)}

def closures_equal(~C: Data, ~ops: R.ops<C>, ctx: C, +count: Nat, a: V.closure, other: Nat, b: V.closure) -> R.program<C, Bool>:
  R.pchoose(C, Bool, R.nat_equal(count, other), _ => open_pair(C, ops, ctx, count, a, b), _ => R.Pure{False{}})

def motive_bodies(~C: Data, ~ops: R.ops<C>, +ctx: C, env1: List<&2, V.t>, env2: List<&2, V.t>, body1: T.t, body2: T.t, +count: Nat) -> R.program<C, Bool>:
  R.pbind(C, V.t, Bool, R.lift(C, V.t, R.o_eval_env(C, ops, ctx, motive_env(R.o_size(C, ops, ctx), count, env1), body1)), v1 =>
    R.pbind(C, V.t, Bool, R.lift(C, V.t, R.o_eval_env(C, ops, ctx, motive_env(R.o_size(C, ops, ctx), count, env2), body2)), v2 =>
      R.ask_type(C, R.grow(C, ops, Succ{count}, ctx), v1, v2)))

def spine_step(~C: Data, ~ops: R.ops<C>, +ctx: C, hty: V.t, shape: S.t<V.t>, addr: V.vaddr) -> R.program<C, Maybe<&2, R.SpineStep>>:
  R.pbind(C, V.t, Maybe<&2, R.SpineStep>, R.lift(C, V.t, R.o_whnf(C, ops, ctx, hty)), w => spine_step_ran(C, ops, ctx, shape, addr, V.as_ran(w)))

def former_pair(~C: Data, ~ops: R.ops<C>, +ctx: C, +s1: S.t<V.t>, d1: V.closure, u1: Maybe<&2, L.t>, other: V.Former) -> R.program<C, Bool>:
  match other:
    case V.Former{s2, d2, u2}:
      R.pbind(C, R.pack, Bool, R.lift(C, R.pack, R.rules(V.t, s1)), p =>
        R.pbind(C, Bool, Bool, shape_equal(C, ctx, s1, s2), same =>
          R.pchoose(C, Bool, Bool.and(same, R.ann_lvl_eq(s1, u1, u2)), _ => R.conv_diagram(C, ops, ctx, p, s1, d1, d2), _ => R.Pure{False{}})))

def leg_equal(~C: Data, ~ops: R.ops<C>, ctx: C, a: V.vleg, b: V.vleg) -> R.program<C, Bool>:
  match a, b:
    case V.VLeg{binders1, clo1}, V.VLeg{binders2, clo2}:
      closures_equal(C, ops, ctx, List.length(&2, F.Pair2<Q.t, String>, binders1), clo1, List.length(&2, F.Pair2<Q.t, String>, binders2), clo2)

def branch_equal(~C: Data, ~ops: R.ops<C>, state: BranchContext<C>, a: F.Pair2<T.addr, T.leg>, b: F.Pair2<T.addr, T.leg>) -> R.program<C, Bool>:
  match state, a, b:
    case BranchContext{+ctx, +env1, +env2}, F.Pair2{addr1, T.Leg{binders1, body1}}, F.Pair2{addr2, T.Leg{binders2, body2}}:
      R.pbind(C, Bool, Bool, branch_addr(C, ops, ctx, env1, env2, addr1, addr2), same =>
        R.pchoose(C, Bool, same, _ => closures_equal(C, ops, ctx, List.length(&2, F.Pair2<Q.t, String>, binders1), V.close(env1, body1), List.length(&2, F.Pair2<Q.t, String>, binders2), V.close(env2, body2)), _ => R.Pure{False{}}))

def motive_counts(~C: Data, ~ops: R.ops<C>, ctx: C, env1: List<&2, V.t>, env2: List<&2, V.t>, body1: T.t, body2: T.t, +count: Nat, other: Nat) -> R.program<C, Bool>:
  R.pchoose(C, Bool, R.nat_equal(count, other), _ => motive_bodies(C, ops, ctx, env1, env2, body1, body2, count), _ => R.Pure{False{}})

def in_pair(~C: Data, +ctx: C, s1: S.t<V.t>, a1: V.vaddr, args1: List<&2, V.t>, other: V.Intro) -> R.program<C, Bool>:
  match other:
    case V.Intro{s2, a2, args2}:
      R.pbind(C, Bool, Bool, shape_equal(C, ctx, s1, s2), shape =>
        R.pbind(C, Bool, Bool, conv_addr(C, ctx, None{}, a1, a2), address =>
          R.pchoose(C, Bool, Bool.and(shape, address), _ => values_equal(C, ctx, args1, args2), _ => R.Pure{False{}})))

def legs_equal(~C: Data, ~ops: R.ops<C>, ctx: C, a: List<&2, V.vleg>, b: List<&2, V.vleg>) -> R.program<C, Bool>:
  R.payload_eq(C, V.vleg, C, ctx => a => b => leg_equal(C, ops, ctx, a, b), ctx, a, b)

def branches_equal(~C: Data, ~ops: R.ops<C>, ctx: C, env1: List<&2, V.t>, env2: List<&2, V.t>, a: List<&2, F.Pair2<T.addr, T.leg>>, b: List<&2, F.Pair2<T.addr, T.leg>>) -> R.program<C, Bool>:
  R.payload_eq(C, F.Pair2<T.addr, T.leg>, BranchContext<C>, state => a => b => branch_equal(C, ops, state, a, b), BranchContext{ctx, env1, env2}, a, b)

def motives_equal(~C: Data, ~ops: R.ops<C>, ctx: C, env1: List<&2, V.t>, env2: List<&2, V.t>, a: Maybe<&2, T.motive>, b: Maybe<&2, T.motive>) -> R.program<C, Bool>:
  match a, b:
    case None{}, None{}: R.Pure{True{}}
    case None{}, Some{_}: R.Pure{False{}}
    case Some{_}, None{}: R.Pure{False{}}
    case Some{T.Motive{_, idx1, _, body1}}, Some{T.Motive{_, idx2, _, body2}}:
      motive_counts(C, ops, ctx, env1, env2, body1, body2, List.length(&2, String, idx1), List.length(&2, String, idx2))

def stuck_equal(~C: Data, ~ops: R.ops<C>, +ctx: C, a: V.stuck_elim, b: V.stuck_elim) -> R.program<C, Bool>:
  match a, b:
    case V.Stuck{s1, q1, mo1, bs1, +env1}, V.Stuck{s2, q2, mo2, bs2, +env2}:
      R.pbind(C, Bool, Bool, shape_equal(C, ctx, s1, s2), shape =>
        R.pbind(C, Bool, Bool, motives_equal(C, ops, ctx, env1, env2, mo1, mo2), motive =>
          R.pchoose(C, Bool, Bool.and(shape, Bool.and(Q.equal(q1, q2), motive)), _ => branches_equal(C, ops, ctx, env1, env2, bs1, bs2), _ => R.Pure{False{}})))

def sec_pair(~C: Data, ~ops: R.ops<C>, +ctx: C, s1: S.t<V.t>, legs1: List<&2, V.vleg>, other: F.Pair2<S.t<V.t>, List<&2, V.vleg>>) -> R.program<C, Bool>:
  match other:
    case F.Pair2{s2, legs2}:
      R.pbind(C, Bool, Bool, shape_equal(C, ctx, s1, s2), same =>
        R.pchoose(C, Bool, same, _ => legs_equal(C, ops, ctx, legs1, legs2), _ => R.Pure{False{}}))

def spine_equal(~C: Data, ~ops: R.ops<C>, +ctx: C, a: List<&2, V.spine>, b: List<&2, V.spine>, hty: V.t) -> R.program<C, Bool>:
  match a, b:
    case [], []: R.Pure{True{}}
    case [], _ <> _: R.Pure{False{}}
    case _ <> _, []: R.Pure{False{}}
    case V.SOut{+s1, +a1} <> r1, V.SOut{s2, a2} <> r2:
      R.pbind(C, Bool, Bool, shape_equal(C, ctx, s1, s2), shape =>
        R.pbind(C, Maybe<&2, R.SpineStep>, Bool, spine_step(C, ops, ctx, hty, s1, a1), +step =>
          R.pbind(C, Bool, Bool, conv_addr(C, ctx, step_arg(step), a1, a2), address =>
            R.pchoose(C, Bool, Bool.and(shape, address), _ => spine_equal(~C, ~ops, ctx, r1, r2, step_next(step)), _ => R.Pure{False{}}))))
    case V.SElim{m1} <> r1, V.SElim{m2} <> r2:
      R.pbind(C, Bool, Bool, stuck_equal(C, ops, ctx, m1, m2), same =>
        R.pchoose(C, Bool, same, _ => spine_equal(~C, ~ops, ctx, r1, r2, R.placeholder), _ => R.Pure{False{}}))
    case V.SOut{_, _} <> _, V.SElim{_} <> _: R.Pure{False{}}
    case V.SElim{_} <> _, V.SOut{_, _} <> _: R.Pure{False{}}

def neutral_pair(~C: Data, ~ops: R.ops<C>, +ctx: C, +h1: V.head, sp1: List<&2, V.spine>, other: F.Pair2<V.head, List<&2, V.spine>>) -> R.program<C, Bool>:
  match other:
    case F.Pair2{h2, sp2}:
      R.pchoose(C, Bool, V.head_equal(h1, h2), _ =>
        R.pbind(C, V.t, Bool, R.lift(C, V.t, R.o_head_ty(C, ops, ctx, h1)), ty =>
          spine_equal(C, ops, ctx, R.rev_append(V.spine, sp1, []), R.rev_append(V.spine, sp2, []), ty)), _ => R.Pure{False{}})

def structural_values(~C: Data, ~ops: R.ops<C>, ctx: C, a: V.t, b: V.t) -> R.program<C, Bool>:
  match a:
    case V.VUniv{x}: R.option_case(C, L.t, V.as_univ(b), y => R.Pure{L.equal(x, y)})
    case V.VLit{x}: R.option_case(C, Lit.t, V.as_lit(b), y => R.Pure{Lit.equal(x, y)})
    case V.VRan{s, d, u}: R.option_case(C, V.Former, V.as_ran(b), other => former_pair(C, ops, ctx, s, d, u, other))
    case V.VLan{s, d, u}: R.option_case(C, V.Former, V.as_lan(b), other => former_pair(C, ops, ctx, s, d, u, other))
    case V.VIn{s, a, args}: R.option_case(C, V.Intro, V.as_in(b), other => in_pair(C, ctx, s, a, args, other))
    case V.VSec{s, legs}: R.option_case(C, F.Pair2<S.t<V.t>, List<&2, V.vleg>>, V.as_sec(b), other => sec_pair(C, ops, ctx, s, legs, other))
    case V.VNeutral{h, sp}: R.option_case(C, F.Pair2<V.head, List<&2, V.spine>>, V.as_neutral(b), other => neutral_pair(C, ops, ctx, h, sp, other))

def structural(~C: Data, ~ops: R.ops<C>, +ctx: C, a: V.t, b: V.t) -> R.program<C, Bool>:
  R.pbind(C, V.t, Bool, R.lift(C, V.t, R.o_whnf(C, ops, ctx, a)), wa =>
    R.pbind(C, V.t, Bool, R.lift(C, V.t, R.o_whnf(C, ops, ctx, b)), wb => structural_values(C, ops, ctx, wa, wb)))

def eta_former(~C: Data, ~ops: R.ops<C>, +ctx: C, +shape: S.t<V.t>, diagram: V.closure, +a: V.t, +b: V.t, +right: Bool) -> R.program<C, Bool>:
  R.pbind(C, R.pack, Bool, R.lift(C, R.pack, R.rules(V.t, shape)), +p =>
    R.pchoose(C, Bool, R.has_eta(p, right), _ => R.expand(C, ops, ctx, p, shape, diagram, a, b, right), _ => structural(C, ops, ctx, a, b)))

def eta_by_type(~C: Data, ~ops: R.ops<C>, ctx: C, ty: V.t, a: V.t, b: V.t) -> R.program<C, Bool>:
  match ty:
    case V.VRan{s, d, _}: eta_former(C, ops, ctx, s, d, a, b, True{})
    case V.VLan{s, d, _}: eta_former(C, ops, ctx, s, d, a, b, False{})
    case V.VUniv{_}: structural(C, ops, ctx, a, b)
    case V.VLit{_}: structural(C, ops, ctx, a, b)
    case V.VIn{_, _, _}: structural(C, ops, ctx, a, b)
    case V.VSec{_, _}: structural(C, ops, ctx, a, b)
    case V.VNeutral{_, _}: structural(C, ops, ctx, a, b)

def conv_step(~C: Data, ~ops: R.ops<C>, +ctx: C, +ty: V.t, a: V.t, b: V.t) -> R.program<C, Bool>:
  R.pchoose(C, Bool, is_prop(C, ops, ctx, ty), _ => R.Pure{True{}}, _ =>
    R.pbind(C, V.t, Bool, R.lift(C, V.t, R.o_whnf(C, ops, ctx, ty)), +w =>
      R.pbind(C, Bool, Bool, subsingleton(C, ops, ctx, w), sub =>
        R.pchoose(C, Bool, sub, _ => R.Pure{True{}}, _ => eta_by_type(C, ops, ctx, w, a, b)))))

def dispatch(~C: Data, ~ops: R.ops<C>, job: R.task<C>) -> R.program<C, R.answer>:
  match job:
    case R.Compare{ctx, ty, a, b}: R.pmap(C, Bool, R.answer, same => R.BoolAnswer{same}, conv_step(C, ops, ctx, ty, a, b))
    case R.Evaluate{_, _, _}: R.Abort{E.Mismatch{"internal evaluation request in conversion"}}
    case R.Weak{_, _}: R.Abort{E.Mismatch{"internal evaluation request in conversion"}}
    case R.Readback{_, _, _}: R.Abort{E.Mismatch{"internal readback request in conversion"}}

def conv(~C: Data, ~ops: R.ops<C>, ctx: C, ty: V.t, a: V.t, b: V.t) -> R.result(Bool):
  R.run(C, job => dispatch(C, ops, job), Bool, R.ask_conv(C, ctx, ty, a, b))

def conv_type(~C: Data, ~ops: R.ops<C>, ctx: C, a: V.t, b: V.t) -> R.result(Bool):
  conv(C, ops, ctx, R.placeholder, a, b)
