/* kernel_conv.c: C twin of lib/kernel_conv.bend (stage S2, session 6).
   Typed conversion at Kanon 69f3be51 conv.ml. The recursive comparisons
   go through the rules interface (rules_request on the host).

   ops<C> is runtime_ops at each caller (kernel_check line 96), so C is
   K.ctx: the o_* accessors are runtime_ops_* calls on ctx.check. The eq
   closures of shape_equal and values_equal are ask_type (folded, S2
   session 2). legs_equal and branches_equal are the payload_eq instances
   at V.vleg and at branches: one loop each. BranchContext has no C twin:
   branches_equal passes ctx, env1 and env2 to branch_equal. */
#include "sole.h"

Bool conv_prop_level(int done, Level level) {
  return done ? level_equal(level, level_zero()) : NO;
}

int conv_shape_equal(const RulesHost *host, RulesCtx ctx,
                     const ValueShape *a, const ValueShape *b, Bool *out,
                     const KError **error) {
  return rules_shape_eq(host, ctx, a, b, out, error);
}

int conv_values_equal(const RulesHost *host, RulesCtx ctx, const Values *a,
                      const Values *b, Bool *out, const KError **error) {
  return rules_payload_eq(host, ctx, a, b, out, error);
}

/* ty NULL = None{}. */
int conv_point_equal(const RulesHost *host, RulesCtx ctx, const Value *ty,
                     const Value *a, const Value *b, Bool *out,
                     const KError **error) {
  return ty == NULL ? rules_ask_type(host, ctx, a, b, out, error)
                    : rules_ask_conv(host, ctx, ty, a, b, out, error);
}

int conv_open_pair(const RulesHost *host, RulesCtx ctx, Nat count,
                   const Closure *a, const Closure *b, Bool *out,
                   const KError **error) {
  const Value *v1;
  const Value *v2;
  if (!runtime_ops_eval_env(
          ctx.check,
          Values_reverse_onto(rules_fresh(runtime_ops_size(ctx.check), count),
                              a->env),
          a->body, &v1, error))
    return 0;
  if (!runtime_ops_eval_env(
          ctx.check,
          Values_reverse_onto(rules_fresh(runtime_ops_size(ctx.check), count),
                              b->env),
          b->body, &v2, error))
    return 0;
  return rules_ask_type(host, rules_grow(count, ctx), v1, v2, out, error);
}

int conv_branch_addr(const RulesHost *host, RulesCtx ctx, const Values *env1,
                     const Values *env2, const TermAddr *a, const TermAddr *b,
                     Bool *out, const KError **error) {
  switch (a->tag) {
  case TERM_APT: {
    const Value *v1;
    const Value *v2;
    if (b->tag != TERM_APT) {
      *out = NO;
      return 1;
    }
    if (!runtime_ops_eval_env(ctx.check, env1, a->as.apt.arg, &v1, error))
      return 0;
    if (!runtime_ops_eval_env(ctx.check, env2, b->as.apt.arg, &v2, error))
      return 0;
    return rules_ask_type(host, ctx, v1, v2, out, error);
  }
  case TERM_ALEG:
    *out = b->tag == TERM_ALEG
               ? int63_equal(a->as.aleg.index, b->as.aleg.index)
               : NO;
    return 1;
  case TERM_ACTOR:
    *out = b->tag == TERM_ACTOR ? text_same(a->as.actor.name, b->as.actor.name)
                                : NO;
    return 1;
  }
  return 0; /* each tag returns above */
}

const Values *conv_motive_env(Int63 size, Nat count, const Values *env) {
  return Values_cons(value_var(rules_advance(size, count)),
                     Values_reverse_onto(rules_fresh(size, count), env));
}

/* former NULL = None{}; the answer None{} is out->result NULL. */
int conv_spine_step_ran(const RulesHost *host, RulesCtx ctx,
                        const ValueShape *shape, const VAddr *addr,
                        const ValueFormer *former, RulesSpineStep *out,
                        const KError **error) {
  RulesPack p;
  if (former == NULL) {
    out->argument = NULL;
    out->result = NULL;
    return 1;
  }
  if (!rules_rules(shape->tag, &p, error))
    return 0;
  return rules_spine_ty(host, ctx, p, shape, former->diagram, addr, out,
                        error);
}

const Value *conv_step_arg(const RulesSpineStep *step) {
  return step->result == NULL ? NULL : step->argument;
}

const Value *conv_step_next(const RulesSpineStep *step) {
  return step->result == NULL ? rules_placeholder() : step->result;
}

int conv_subsingleton(RulesCtx ctx, const Value *ty, Bool *out,
                      const KError **error) {
  switch (ty->tag) {
  case VALUE_VLAN: {
    RulesPack p;
    if (!rules_rules(ty->as.vlan.shape->tag, &p, error))
      return 0;
    *out = rules_subsingleton(ctx, ty->as.vlan.shape);
    return 1;
  }
  case VALUE_VRAN:
  case VALUE_VUNIV:
  case VALUE_VLIT:
  case VALUE_VIN:
  case VALUE_VSEC:
  case VALUE_VNEUTRAL:
    *out = NO;
    return 1;
  }
  return 0; /* each tag returns above */
}

/* pcatch (Bend line 85): an Abort of the quote or of the universe request
   gives False{}. On the checking host a budget refusal is an Abort too, so
   it is caught here, as in Bend (the fuel cell stays at zero). */
int conv_is_prop(const RulesHost *host, RulesCtx ctx, const Value *ty,
                 Bool *out, const KError **error) {
  const Term *term;
  Level level;
  const KError *caught;
  (void)error;
  if (!runtime_ops_quote(ctx.check, ty, &term, &caught) ||
      !rules_ask_univ(host, ctx, term, &level, &caught)) {
    *out = NO;
    return 1;
  }
  *out = level_equal(level, level_zero());
  return 1;
}

/* ty NULL = None{}. */
int conv_conv_addr(const RulesHost *host, RulesCtx ctx, const Value *ty,
                   const VAddr *a, const VAddr *b, Bool *out,
                   const KError **error) {
  switch (a->tag) {
  case VALUE_VAPT:
    if (b->tag != VALUE_VAPT) {
      *out = NO;
      return 1;
    }
    return conv_point_equal(host, ctx, ty, a->as.vapt.arg, b->as.vapt.arg,
                            out, error);
  case VALUE_VALEG:
    *out = b->tag == VALUE_VALEG
               ? int63_equal(a->as.valeg.index, b->as.valeg.index)
               : NO;
    return 1;
  case VALUE_VACTOR:
    *out = b->tag == VALUE_VACTOR
               ? text_same(a->as.vactor.name, b->as.vactor.name)
               : NO;
    return 1;
  }
  return 0; /* each tag returns above */
}

int conv_closures_equal(const RulesHost *host, RulesCtx ctx, Nat count,
                        const Closure *a, Nat other, const Closure *b,
                        Bool *out, const KError **error) {
  if (!rules_nat_equal(count, other)) {
    *out = NO;
    return 1;
  }
  return conv_open_pair(host, ctx, count, a, b, out, error);
}

int conv_motive_bodies(const RulesHost *host, RulesCtx ctx,
                       const Values *env1, const Values *env2,
                       const Term *body1, const Term *body2, Nat count,
                       Bool *out, const KError **error) {
  const Value *v1;
  const Value *v2;
  if (!runtime_ops_eval_env(
          ctx.check,
          conv_motive_env(runtime_ops_size(ctx.check), count, env1), body1,
          &v1, error))
    return 0;
  if (!runtime_ops_eval_env(
          ctx.check,
          conv_motive_env(runtime_ops_size(ctx.check), count, env2), body2,
          &v2, error))
    return 0;
  /* Succ{count}: Nat + 1. */
  return rules_ask_type(host, rules_grow(count + 1, ctx), v1, v2, out, error);
}

int conv_spine_step(const RulesHost *host, RulesCtx ctx, const Value *hty,
                    const ValueShape *shape, const VAddr *addr,
                    RulesSpineStep *out, const KError **error) {
  const Value *w;
  ValueFormer former;
  if (!runtime_ops_whnf(ctx.check, hty, &w, error))
    return 0;
  return conv_spine_step_ran(host, ctx, shape, addr,
                             value_as_ran(w, &former) ? &former : NULL, out,
                             error);
}

/* u1 NULL = None{}. */
int conv_former_pair(const RulesHost *host, RulesCtx ctx,
                     const ValueShape *s1, const Closure *d1,
                     const Level *u1, const ValueFormer *other, Bool *out,
                     const KError **error) {
  RulesPack p;
  Bool same;
  if (!rules_rules(s1->tag, &p, error))
    return 0;
  if (!conv_shape_equal(host, ctx, s1, other->shape, &same, error))
    return 0;
  if (!(same && rules_ann_lvl_eq(s1, u1, other->universe))) {
    *out = NO;
    return 1;
  }
  return rules_conv_diagram(host, ctx, p, s1, d1, other->diagram, out,
                            error);
}

int conv_leg_equal(const RulesHost *host, RulesCtx ctx, const VLeg *a,
                   const VLeg *b, Bool *out, const KError **error) {
  return conv_closures_equal(host, ctx, TermBinders_length(a->binders),
                             a->clo, TermBinders_length(b->binders), b->clo,
                             out, error);
}

int conv_branch_equal(const RulesHost *host, RulesCtx ctx,
                      const Values *env1, const Values *env2,
                      const TermBranch *a, const TermBranch *b, Bool *out,
                      const KError **error) {
  Bool same;
  if (!conv_branch_addr(host, ctx, env1, env2, a->address, b->address, &same,
                        error))
    return 0;
  if (!same) {
    *out = NO;
    return 1;
  }
  return conv_closures_equal(host, ctx, TermBinders_length(a->leg->binders),
                             value_close(env1, a->leg->body),
                             TermBinders_length(b->leg->binders),
                             value_close(env2, b->leg->body), out, error);
}

int conv_motive_counts(const RulesHost *host, RulesCtx ctx,
                       const Values *env1, const Values *env2,
                       const Term *body1, const Term *body2, Nat count,
                       Nat other, Bool *out, const KError **error) {
  if (!rules_nat_equal(count, other)) {
    *out = NO;
    return 1;
  }
  return conv_motive_bodies(host, ctx, env1, env2, body1, body2, count, out,
                            error);
}

/* Both shape_equal and conv_addr run before the and (Bend evaluates both
   pbind steps), so the requests are the same as in Bend. */
int conv_in_pair(const RulesHost *host, RulesCtx ctx, const ValueShape *s1,
                 const VAddr *a1, const Values *args1,
                 const ValueIntro *other, Bool *out, const KError **error) {
  Bool shape;
  Bool address;
  if (!conv_shape_equal(host, ctx, s1, other->shape, &shape, error))
    return 0;
  if (!conv_conv_addr(host, ctx, NULL, a1, other->address, &address, error))
    return 0;
  if (!(shape && address)) {
    *out = NO;
    return 1;
  }
  return conv_values_equal(host, ctx, args1, other->args, out, error);
}

/* payload_eq at V.vleg (eq = leg_equal): the loop of rules_payload_eq. */
int conv_legs_equal(const RulesHost *host, RulesCtx ctx, const VLegs *a,
                    const VLegs *b, Bool *out, const KError **error) {
  while (a != NULL && b != NULL) {
    Bool same;
    if (!conv_leg_equal(host, ctx, a->head, b->head, &same, error))
      return 0;
    if (!same) {
      *out = NO;
      return 1;
    }
    a = a->tail;
    b = b->tail;
  }
  *out = (a == NULL && b == NULL) ? YES : NO;
  return 1;
}

/* payload_eq at F.Pair2<T.addr, T.leg> (eq = branch_equal with
   BranchContext{ctx, env1, env2}): the loop of rules_payload_eq. */
int conv_branches_equal(const RulesHost *host, RulesCtx ctx,
                        const Values *env1, const Values *env2,
                        const TermBranches *a, const TermBranches *b,
                        Bool *out, const KError **error) {
  while (a != NULL && b != NULL) {
    Bool same;
    if (!conv_branch_equal(host, ctx, env1, env2, &a->head, &b->head, &same,
                           error))
      return 0;
    if (!same) {
      *out = NO;
      return 1;
    }
    a = a->tail;
    b = b->tail;
  }
  *out = (a == NULL && b == NULL) ? YES : NO;
  return 1;
}

/* a, b NULL = None{}. */
int conv_motives_equal(const RulesHost *host, RulesCtx ctx,
                       const Values *env1, const Values *env2,
                       const TermMotive *a, const TermMotive *b, Bool *out,
                       const KError **error) {
  if (a == NULL || b == NULL) {
    *out = (a == NULL && b == NULL) ? YES : NO;
    return 1;
  }
  return conv_motive_counts(host, ctx, env1, env2, a->body, b->body,
                            Texts_length(a->idx), Texts_length(b->idx), out,
                            error);
}

int conv_stuck_equal(const RulesHost *host, RulesCtx ctx, const Stuck *a,
                     const Stuck *b, Bool *out, const KError **error) {
  Bool shape;
  Bool motive;
  if (!conv_shape_equal(host, ctx, a->shape, b->shape, &shape, error))
    return 0;
  if (!conv_motives_equal(host, ctx, a->env, b->env, a->motive, b->motive,
                          &motive, error))
    return 0;
  if (!(shape && quantity_equal(a->scrut_q, b->scrut_q) && motive)) {
    *out = NO;
    return 1;
  }
  return conv_branches_equal(host, ctx, a->env, b->env, a->branches,
                             b->branches, out, error);
}

int conv_sec_pair(const RulesHost *host, RulesCtx ctx, const ValueShape *s1,
                  const VLegs *legs1, const ValueSec *other, Bool *out,
                  const KError **error) {
  Bool same;
  if (!conv_shape_equal(host, ctx, s1, other->shape, &same, error))
    return 0;
  if (!same) {
    *out = NO;
    return 1;
  }
  return conv_legs_equal(host, ctx, legs1, other->legs, out, error);
}

/* The self tail calls (Bend lines 176 and 179) are the loop; hty is the
   head type of the next frame. */
int conv_spine_equal(const RulesHost *host, RulesCtx ctx, const Spines *a,
                     const Spines *b, const Value *hty, Bool *out,
                     const KError **error) {
  while (a != NULL && b != NULL) {
    const Spine *x = a->head;
    const Spine *y = b->head;
    Bool same;
    switch (x->tag) {
    case VALUE_SOUT: {
      RulesSpineStep step;
      Bool address;
      if (y->tag != VALUE_SOUT) {
        *out = NO;
        return 1;
      }
      if (!conv_shape_equal(host, ctx, x->as.sout.shape, y->as.sout.shape,
                            &same, error))
        return 0;
      if (!conv_spine_step(host, ctx, hty, x->as.sout.shape,
                           x->as.sout.address, &step, error))
        return 0;
      if (!conv_conv_addr(host, ctx, conv_step_arg(&step), x->as.sout.address,
                          y->as.sout.address, &address, error))
        return 0;
      if (!(same && address)) {
        *out = NO;
        return 1;
      }
      hty = conv_step_next(&step);
      break;
    }
    case VALUE_SELIM:
      if (y->tag != VALUE_SELIM) {
        *out = NO;
        return 1;
      }
      if (!conv_stuck_equal(host, ctx, x->as.selim.elim, y->as.selim.elim,
                            &same, error))
        return 0;
      if (!same) {
        *out = NO;
        return 1;
      }
      hty = rules_placeholder();
      break;
    }
    a = a->tail;
    b = b->tail;
  }
  *out = (a == NULL && b == NULL) ? YES : NO;
  return 1;
}

int conv_neutral_pair(const RulesHost *host, RulesCtx ctx, const Head *h1,
                      const Spines *sp1, const ValueNeutral *other,
                      Bool *out, const KError **error) {
  const Value *ty;
  if (!value_head_equal(h1, other->head)) {
    *out = NO;
    return 1;
  }
  if (!runtime_ops_head_ty(ctx.check, h1, &ty, error))
    return 0;
  return conv_spine_equal(host, ctx, Spines_reverse_onto(sp1, NULL),
                          Spines_reverse_onto(other->spine, NULL), ty, out,
                          error);
}

/* option_case: None{} gives False{}. */
int conv_structural_values(const RulesHost *host, RulesCtx ctx,
                           const Value *a, const Value *b, Bool *out,
                           const KError **error) {
  switch (a->tag) {
  case VALUE_VUNIV: {
    const Level *y = value_as_univ(b);
    *out = y == NULL ? NO : level_equal(a->as.vuniv.level, *y);
    return 1;
  }
  case VALUE_VLIT: {
    const Literal *y = value_as_lit(b);
    *out = y == NULL ? NO : literal_equal(a->as.vlit.literal, y);
    return 1;
  }
  case VALUE_VRAN: {
    ValueFormer other;
    if (!value_as_ran(b, &other)) {
      *out = NO;
      return 1;
    }
    return conv_former_pair(host, ctx, a->as.vran.shape, a->as.vran.diagram,
                            a->as.vran.universe, &other, out, error);
  }
  case VALUE_VLAN: {
    ValueFormer other;
    if (!value_as_lan(b, &other)) {
      *out = NO;
      return 1;
    }
    return conv_former_pair(host, ctx, a->as.vlan.shape, a->as.vlan.diagram,
                            a->as.vlan.universe, &other, out, error);
  }
  case VALUE_VIN: {
    ValueIntro other;
    if (!value_as_in(b, &other)) {
      *out = NO;
      return 1;
    }
    return conv_in_pair(host, ctx, a->as.vin.shape, a->as.vin.address,
                        a->as.vin.args, &other, out, error);
  }
  case VALUE_VSEC: {
    ValueSec other;
    if (!value_as_sec(b, &other)) {
      *out = NO;
      return 1;
    }
    return conv_sec_pair(host, ctx, a->as.vsec.shape, a->as.vsec.legs, &other,
                         out, error);
  }
  case VALUE_VNEUTRAL: {
    ValueNeutral other;
    if (!value_as_neutral(b, &other)) {
      *out = NO;
      return 1;
    }
    return conv_neutral_pair(host, ctx, a->as.vneutral.head,
                             a->as.vneutral.spine, &other, out, error);
  }
  }
  return 0; /* each tag returns above */
}

int conv_structural(const RulesHost *host, RulesCtx ctx, const Value *a,
                    const Value *b, Bool *out, const KError **error) {
  const Value *wa;
  const Value *wb;
  if (!runtime_ops_whnf(ctx.check, a, &wa, error))
    return 0;
  if (!runtime_ops_whnf(ctx.check, b, &wb, error))
    return 0;
  return conv_structural_values(host, ctx, wa, wb, out, error);
}

int conv_eta_former(const RulesHost *host, RulesCtx ctx,
                    const ValueShape *shape, const Closure *diagram,
                    const Value *a, const Value *b, Bool right, Bool *out,
                    const KError **error) {
  RulesPack p;
  if (!rules_rules(shape->tag, &p, error))
    return 0;
  return rules_has_eta(p, right)
             ? rules_expand(host, ctx, p, shape, diagram, a, b, right, out,
                            error)
             : conv_structural(host, ctx, a, b, out, error);
}

int conv_eta_by_type(const RulesHost *host, RulesCtx ctx, const Value *ty,
                     const Value *a, const Value *b, Bool *out,
                     const KError **error) {
  switch (ty->tag) {
  case VALUE_VRAN:
    return conv_eta_former(host, ctx, ty->as.vran.shape, ty->as.vran.diagram,
                           a, b, YES, out, error);
  case VALUE_VLAN:
    return conv_eta_former(host, ctx, ty->as.vlan.shape, ty->as.vlan.diagram,
                           a, b, NO, out, error);
  case VALUE_VUNIV:
  case VALUE_VLIT:
  case VALUE_VIN:
  case VALUE_VSEC:
  case VALUE_VNEUTRAL:
    return conv_structural(host, ctx, a, b, out, error);
  }
  return 0; /* each tag returns above */
}

/* CV.conv_step: also the Compare arm of kernel_check dispatch (S3). */
int conv_conv_step(const RulesHost *host, RulesCtx ctx, const Value *ty,
                   const Value *a, const Value *b, Bool *out,
                   const KError **error) {
  Bool prop;
  Bool sub;
  const Value *w;
  if (!conv_is_prop(host, ctx, ty, &prop, error))
    return 0;
  if (prop) {
    *out = YES;
    return 1;
  }
  if (!runtime_ops_whnf(ctx.check, ty, &w, error))
    return 0;
  if (!conv_subsingleton(ctx, w, &sub, error))
    return 0;
  if (sub) {
    *out = YES;
    return 1;
  }
  return conv_eta_by_type(host, ctx, w, a, b, out, error);
}

/* dispatch (line 225): the RULES_HOST_CONV arm of rules_request. */
int conv_dispatch(const RulesHost *host, const RulesTask *task,
                  RulesAnswer *answer, const KError **error) {
  switch (task->tag) {
  case RULES_COMPARE: {
    Bool same;
    if (!conv_conv_step(host, task->context, task->as.compare.ty,
                        task->as.compare.left, task->as.compare.right, &same,
                        error))
      return 0;
    answer->tag = RULES_BOOL_ANSWER;
    answer->as.equal = same;
    return 1;
  }
  case RULES_EVALUATE:
  case RULES_WEAK:
    *error = error_mismatch(
        text_of_cstring("internal evaluation request in conversion"));
    return 0;
  case RULES_READBACK:
    *error = error_mismatch(
        text_of_cstring("internal readback request in conversion"));
    return 0;
  case RULES_INFER_UNIVERSE: {
    Level level;
    if (!runtime_ops_infer_univ(task->context.check,
                                task->as.infer_universe.term, &level, error))
      return 0;
    answer->tag = RULES_LEVEL_ANSWER;
    answer->as.level = level;
    return 1;
  }
  case RULES_INFER_USES:
  case RULES_CHECK_USES:
    *error = error_mismatch(
        text_of_cstring("internal checking request in conversion"));
    return 0;
  }
  return 0; /* each tag returns above */
}

/* R.run with dispatch: a conversion host on the stack (no fuel). */
int conv_conv(const CheckCtx *ctx, const Value *ty, const Value *a,
              const Value *b, Bool *out, const KError **error) {
  RulesHost host;
  RulesCtx context;
  host.kind = RULES_HOST_CONV;
  host.fuel = NULL;
  context.check = ctx;
  return rules_ask_conv(&host, context, ty, a, b, out, error);
}

int conv_conv_type(const CheckCtx *ctx, const Value *a, const Value *b,
                   Bool *out, const KError **error) {
  return conv_conv(ctx, rules_placeholder(), a, b, out, error);
}
