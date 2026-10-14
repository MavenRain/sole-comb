/* kernel_eval.c: C twin of lib/kernel_eval.bend (stage S3, session 1).
   Normalization by evaluation at Kanon 69f3be51 eval.ml. Shape dispatch
   stays in kernel_rules. Environments use indices; neutral locals use
   levels, and neutral spines are newest first.

   C = G.t, so ctx.globals holds the globals. A program returns int (1 =
   Pure with *out, 0 = Abort with *error); pbind is the 0 return of each
   step, in the Bend order. eval_dispatch is the RULES_HOST_EVAL arm of
   rules_request. map_list and map_shape have no C twin (S2 session 3):
   eval_eval_shape, eval_eval_list, eval_quote_shape, eval_quote_list and
   eval_quote_legs call request_eval, request_quote or quote_leg directly,
   left to right. */
#include "sole.h"

/* R.at (kernel_rules line 258) at V.t; NULL = None{}. The kernel_rules.c
   instance is at V.vleg only. */
static const Value *at_value(Int63 index, const Values *xs) {
  for (; xs != NULL; xs = xs->tail) {
    if (int63_equal(index, int63_zero())) return xs->head;
    index = int63_pred(index);
  }
  return NULL;
}

/* The right fold builds the list in order: a loop, then a reverse. */
const VLegs *eval_leg_closures(const Values *env, const TermLegs *legs) {
  const VLegs *acc = NULL;
  for (; legs != NULL; legs = legs->tail)
    acc = VLegs_cons(value_vleg(legs->head->binders,
                                value_close(env, legs->head->body)),
                     acc);
  return VLegs_reverse(acc);
}

const Level *eval_fill(const Level *old, const Level *inferred) {
  return old == NULL ? inferred : old;
}

int eval_found_prim(const GlobalPrim *found, Prim *out) {
  if (found == NULL) return 0;
  *out = found->prim;
  return 1;
}

/* prim_answer without the Fail arm: the caller returns 0 on the Fail of
   prim_apply. found NULL = Done{None{}}. */
const Value *eval_prim_answer(const Value *value, const Value *found) {
  return found == NULL ? value : found;
}

Bool eval_guarded_intro(const ValueIntro *intro) {
  if (intro == NULL) return NO;
  switch (intro->address->tag) {
  case VALUE_VAPT: return NO;
  case VALUE_VALEG: return NO;
  case VALUE_VACTOR: return YES;
  }
  return NO; /* each tag returns above */
}

const Term *eval_annotate_former(const Term *core, const Level *universe) {
  return universe == NULL ? core : term_ann(core, term_univ(*universe));
}

const Term *eval_quote_head(Int63 size, const Head *head) {
  switch (head->tag) {
  case VALUE_HLOCAL:
    return term_var(int63_pred(int63_sub(size, head->as.hlocal.level)));
  case VALUE_HGLOBAL: return term_global(head->as.hglobal.name);
  }
  return NULL; /* each tag returns above */
}

const Literal *eval_frame_literal(const Spine *frame) {
  switch (frame->tag) {
  case VALUE_SELIM: return NULL;
  case VALUE_SOUT:
    switch (frame->as.sout.address->tag) {
    case VALUE_VALEG: return NULL;
    case VALUE_VACTOR: return NULL;
    case VALUE_VAPT: return value_as_lit(frame->as.sout.address->as.vapt.arg);
    }
    return NULL; /* each tag returns above */
  }
  return NULL; /* each tag returns above */
}

int eval_request_eval(const RulesHost *host, RulesCtx ctx, const Values *env,
                      const Term *term, const Value **out,
                      const KError **error) {
  return rules_ask_eval(host, ctx, env, term, out, error);
}

int eval_request_whnf(const RulesHost *host, RulesCtx ctx, const Value *value,
                      const Value **out, const KError **error) {
  return rules_ask_whnf(host, ctx, value, out, error);
}

int eval_request_quote(const RulesHost *host, RulesCtx ctx, Int63 size,
                       const Value *value, const Term **out,
                       const KError **error) {
  return rules_ask_quote(host, ctx, size, value, out, error);
}

/* ev (line 84) = request_eval on the globals (S2 session 2). */
RulesEvaluator eval_ev(RulesCtx ctx) {
  RulesEvaluator ev;
  ev.kind = RULES_EV_ASK;
  ev.ctx = ctx;
  return ev;
}

int eval_eval_addr(const RulesHost *host, RulesCtx ctx, const Values *env,
                   const TermAddr *address, const VAddr **out,
                   const KError **error) {
  switch (address->tag) {
  case TERM_APT: {
    const Value *v;
    if (!eval_request_eval(host, ctx, env, address->as.apt.arg, &v, error))
      return 0;
    *out = value_vapt(address->as.apt.quantity, v);
    return 1;
  }
  case TERM_ALEG: *out = value_valeg(address->as.aleg.index); return 1;
  case TERM_ACTOR: *out = value_vactor(address->as.actor.name); return 1;
  }
  return 0; /* each tag returns above */
}

/* global_find with the cache of the term or head that names the global.
   A globals map does not change, so a name has the same entry for each
   lookup in one map. The term or head is an arena cell, not a const
   object, so the cache can change. */
static const GlobalEntry *eval_find_seen(Text name, const GlobalSeen *seen,
                                         const Globals *globals) {
  GlobalSeen *cache = (GlobalSeen *)seen;
  if (cache->globals != globals) {
    cache->entry = global_find(name, globals);
    cache->globals = globals;
  }
  return cache->entry;
}

/* value_hglobal with the cache set to the entry that eval_eval_entry
   found. */
static const Head *eval_hglobal_seen(Text name, const Globals *globals,
                                     const GlobalEntry *found) {
  const Head *head = value_hglobal(name);
  GlobalSeen *cache = (GlobalSeen *)&head->as.hglobal.seen;
  cache->globals = globals;
  cache->entry = found;
  return head;
}

/* A def body is closed, and an evaluation reads only ctx.globals, so each
   reference under one globals map gets the same value. The memo keeps it:
   the body is evaluated once per map, not once per reference. A failed
   evaluation keeps no value. */
static int eval_memo_body(const RulesHost *host, RulesCtx ctx,
                          const GlobalDef *def, const Value **out,
                          const KError **error) {
  GlobalMemo *memo = def->memo;
  if (memo->globals == ctx.globals) {
    *out = memo->value;
    return 1;
  }
  if (!eval_request_eval(host, ctx, NULL, def->body, out, error)) return 0;
  memo->globals = ctx.globals;
  memo->value = *out;
  return 1;
}

int eval_eval_entry(const RulesHost *host, RulesCtx ctx, Text name,
                    const GlobalEntry *found, const Value **out,
                    const KError **error) {
  if (found == NULL) {
    *error = error_unbound(name);
    return 0;
  }
  switch (found->tag) {
  case GLOBAL_AXIOM:
  case GLOBAL_PRIM:
    *out = value_vneutral(eval_hglobal_seen(name, ctx.globals, found), NULL);
    return 1;
  case GLOBAL_DEF:
    if (found->as.def.reducible && found->as.def.rec_arg == NULL)
      return eval_memo_body(host, ctx, &found->as.def, out, error);
    *out = value_vneutral(eval_hglobal_seen(name, ctx.globals, found), NULL);
    return 1;
  }
  return 0; /* each tag returns above */
}

/* pcatch (line 108): a 0 from request_eval gives Pure{None{}}; the caught
   error is dropped. */
int eval_level_of_ann(const RulesHost *host, RulesCtx ctx, const Values *env,
                      const Term *ty, const Level **out,
                      const KError **error) {
  const Value *v;
  const KError *caught;
  (void)error;
  if (!eval_request_eval(host, ctx, env, ty, &v, &caught)) {
    *out = NULL;
    return 1;
  }
  *out = value_as_univ(v);
  return 1;
}

int eval_head_prim(RulesCtx ctx, const Head *head, Prim *out) {
  switch (head->tag) {
  case VALUE_HLOCAL: return 0;
  case VALUE_HGLOBAL:
    return eval_found_prim(
        global_found_prim(eval_find_seen(
            head->as.hglobal.name, &head->as.hglobal.seen, ctx.globals)),
        out);
  }
  return 0; /* each tag returns above */
}

/* The self tail call through literal_next is a loop. 0 = None{}. */
int eval_spine_literals(const Spines *spine, const Literals *acc,
                        const Literals **out) {
  for (; spine != NULL; spine = spine->tail) {
    const Literal *literal = eval_frame_literal(spine->head);
    if (literal == NULL) return 0;
    acc = Literals_cons(literal, acc);
  }
  *out = acc;
  return 1;
}

int eval_prim_arity(const Value *value, Prim prim, const Literals *args,
                    Bool saturated, const Value **out, const KError **error) {
  const Value *found;
  if (!saturated) {
    *out = value;
    return 1;
  }
  if (!prim_apply(prim, args, &found, error)) return 0;
  *out = eval_prim_answer(value, found);
  return 1;
}

int eval_candidate_def(const Spines *spine, const GlobalDef *found,
                       EvalRecursive *out) {
  if (found == NULL) return 0;
  if (!found->reducible || found->rec_arg == NULL) return 0;
  out->body = found->body;
  out->index = *found->rec_arg;
  out->frames = Spines_reverse(spine);
  return 1;
}

const Term *eval_former(Bool right, const TermShape *shape,
                        const Term *diagram, const Level *universe) {
  return eval_annotate_former(
      right ? term_ran(shape, diagram) : term_lan(shape, diagram), universe);
}

int eval_quote_addr(const RulesHost *host, RulesCtx ctx, Int63 size,
                    const VAddr *address, const TermAddr **out,
                    const KError **error) {
  switch (address->tag) {
  case VALUE_VAPT: {
    const Term *t;
    if (!eval_request_quote(host, ctx, size, address->as.vapt.arg, &t, error))
      return 0;
    *out = term_apt(address->as.vapt.quantity, t);
    return 1;
  }
  case VALUE_VALEG: *out = term_aleg(address->as.valeg.index); return 1;
  case VALUE_VACTOR: *out = term_actor(address->as.vactor.name); return 1;
  }
  return 0; /* each tag returns above */
}

int eval_eval_global(const RulesHost *host, RulesCtx ctx, Text name,
                     const Value **out, const KError **error) {
  return eval_eval_entry(host, ctx, name, global_find(name, ctx.globals), out,
                         error);
}

/* VLan and VRan run level_of_ann for each universe, also when it is
   Some{} (fill keeps the old level), as in Bend. */
int eval_annotate(const RulesHost *host, RulesCtx ctx, const Values *env,
                  const Value *value, const Term *ty, const Value **out,
                  const KError **error) {
  const Level *level;
  switch (value->tag) {
  case VALUE_VLAN:
    if (!eval_level_of_ann(host, ctx, env, ty, &level, error)) return 0;
    *out = value_vlan(value->as.vlan.shape, value->as.vlan.diagram,
                      eval_fill(value->as.vlan.universe, level));
    return 1;
  case VALUE_VRAN:
    if (!eval_level_of_ann(host, ctx, env, ty, &level, error)) return 0;
    *out = value_vran(value->as.vran.shape, value->as.vran.diagram,
                      eval_fill(value->as.vran.universe, level));
    return 1;
  case VALUE_VUNIV:
  case VALUE_VIN:
  case VALUE_VSEC:
  case VALUE_VLIT:
  case VALUE_VNEUTRAL:
    *out = value;
    return 1;
  }
  return 0; /* each tag returns above */
}

int eval_prim_arguments(const Value *value, const Prim *prim,
                        int has_literals, const Literals *literals,
                        const Value **out, const KError **error) {
  if (prim == NULL || !has_literals) {
    *out = value;
    return 1;
  }
  return eval_prim_arity(
      value, *prim, literals,
      rules_nat_equal(Literals_length(literals), prim_arity(*prim)), out,
      error);
}

int eval_candidate_neutral(RulesCtx ctx, const ValueNeutral *neutral,
                           EvalRecursive *out) {
  if (neutral == NULL) return 0;
  switch (neutral->head->tag) {
  case VALUE_HLOCAL: return 0;
  case VALUE_HGLOBAL:
    return eval_candidate_def(
        neutral->spine,
        global_found_def(eval_find_seen(neutral->head->as.hglobal.name,
                                        &neutral->head->as.hglobal.seen,
                                        ctx.globals)),
        out);
  }
  return 0; /* each tag returns above */
}

/* The self tail calls (lines 198, 200) are a loop. */
int eval_replay_frames(const RulesHost *host, RulesCtx ctx,
                       const Spines *frames, const Value *value,
                       const Value **out, const KError **error) {
  RulesEvaluator ev = eval_ev(ctx);
  for (; frames != NULL; frames = frames->tail) {
    const Spine *frame = frames->head;
    switch (frame->tag) {
    case VALUE_SOUT: {
      const Value *v;
      if (!rules_out_value(host, &ev, frame->as.sout.shape,
                           frame->as.sout.address, value, &v, error))
        return 0;
      if (!eval_request_whnf(host, ctx, v, &value, error)) return 0;
      break;
    }
    case VALUE_SELIM: {
      const Stuck *stuck = frame->as.selim.elim;
      if (!rules_elim_value(host, &ev, stuck->shape, stuck->scrut_q,
                            stuck->motive, stuck->branches, stuck->env, value,
                            &value, error))
        return 0;
      break;
    }
    }
  }
  *out = value;
  return 1;
}

int eval_quote_diagram(const RulesHost *host, RulesCtx ctx, Int63 size,
                       const TermShape *shape, const Closure *diagram,
                       const Level *universe, Bool right, Nat arity,
                       const Term **out, const KError **error) {
  RulesEvaluator ev = eval_ev(ctx);
  const Value *v;
  const Term *body;
  if (!rules_open_closure(host, &ev, diagram, rules_fresh(size, arity), &v,
                          error))
    return 0;
  if (!eval_request_quote(host, ctx, rules_advance(size, arity), v, &body,
                          error))
    return 0;
  *out = eval_former(right, shape, body, universe);
  return 1;
}

int eval_quote_leg_arity(const RulesHost *host, RulesCtx ctx, Int63 size,
                         const TermBinders *binders, const Closure *clo,
                         Nat arity, const TermLeg **out,
                         const KError **error) {
  RulesEvaluator ev = eval_ev(ctx);
  const Value *v;
  const Term *body;
  if (!rules_open_closure(host, &ev, clo, rules_fresh(size, arity), &v,
                          error))
    return 0;
  if (!eval_request_quote(host, ctx, rules_advance(size, arity), v, &body,
                          error))
    return 0;
  *out = term_leg(binders, body);
  return 1;
}

/* map_shape (kernel_rules line 531) with mapped_eval. */
int eval_eval_shape(const RulesHost *host, RulesCtx ctx, const Values *env,
                    const TermShape *shape, const ValueShape **out,
                    const KError **error) {
  switch (shape->tag) {
  case SHAPE_SPI: {
    const Value *d;
    if (!eval_request_eval(host, ctx, env, shape->as.spi.domain, &d, error))
      return 0;
    *out = shape_spi_value(shape->as.spi.quantity, shape->as.spi.binder, d);
    return 1;
  }
  case SHAPE_SCOLL: *out = shape_scoll_value(shape->as.scoll.size); return 1;
  case SHAPE_SPAR: *error = error_not_yet(rules_spar_word()); return 0;
  case SHAPE_SMU: {
    const Values *ix;
    if (!eval_eval_list(host, ctx, env, shape->as.smu.indices, &ix, error))
      return 0;
    *out = shape_smu_value(shape->as.smu.family, ix);
    return 1;
  }
  case SHAPE_SNU: *error = error_not_yet(rules_snu_word()); return 0;
  }
  return 0; /* each tag returns above */
}

/* map_list (kernel_rules line 464) with mapped_eval: requests left to
   right, then a reverse. */
int eval_eval_list(const RulesHost *host, RulesCtx ctx, const Values *env,
                   const Terms *args, const Values **out,
                   const KError **error) {
  const Values *acc = NULL;
  for (; args != NULL; args = args->tail) {
    const Value *v;
    if (!eval_request_eval(host, ctx, env, args->head, &v, error)) return 0;
    acc = Values_cons(v, acc);
  }
  *out = Values_reverse(acc);
  return 1;
}

/* map_shape (kernel_rules line 531) with mapped_quote. */
int eval_quote_shape(const RulesHost *host, RulesCtx ctx, Int63 size,
                     const ValueShape *shape, const TermShape **out,
                     const KError **error) {
  switch (shape->tag) {
  case SHAPE_SPI: {
    const Term *d;
    if (!eval_request_quote(host, ctx, size, shape->as.spi.domain, &d, error))
      return 0;
    *out = shape_spi_term(shape->as.spi.quantity, shape->as.spi.binder, d);
    return 1;
  }
  case SHAPE_SCOLL: *out = shape_scoll_term(shape->as.scoll.size); return 1;
  case SHAPE_SPAR: *error = error_not_yet(rules_spar_word()); return 0;
  case SHAPE_SMU: {
    const Terms *ix;
    if (!eval_quote_list(host, ctx, size, shape->as.smu.indices, &ix, error))
      return 0;
    *out = shape_smu_term(shape->as.smu.family, ix);
    return 1;
  }
  case SHAPE_SNU: *error = error_not_yet(rules_snu_word()); return 0;
  }
  return 0; /* each tag returns above */
}

/* map_list (kernel_rules line 464) with mapped_quote. */
int eval_quote_list(const RulesHost *host, RulesCtx ctx, Int63 size,
                    const Values *args, const Terms **out,
                    const KError **error) {
  const Terms *acc = NULL;
  for (; args != NULL; args = args->tail) {
    const Term *t;
    if (!eval_request_quote(host, ctx, size, args->head, &t, error)) return 0;
    acc = Terms_cons(t, acc);
  }
  *out = Terms_reverse(acc);
  return 1;
}

int eval_replay(const RulesHost *host, RulesCtx ctx, const Value *value,
                const Spines *frames, const Value **out,
                const KError **error) {
  return eval_replay_frames(host, ctx, frames, value, out, error);
}

/* Both arguments of prim_arguments are computed (pure), as in Bend. */
int eval_prim_neutral(RulesCtx ctx, const Value *value,
                      const ValueNeutral *neutral, const Value **out,
                      const KError **error) {
  Prim prim;
  const Literals *literals = NULL;
  int has_prim;
  int has_literals;
  if (neutral == NULL) {
    *out = value;
    return 1;
  }
  has_prim = eval_head_prim(ctx, neutral->head, &prim);
  has_literals = eval_spine_literals(neutral->spine, NULL, &literals);
  return eval_prim_arguments(value, has_prim ? &prim : NULL, has_literals,
                             literals, out, error);
}

int eval_candidate(RulesCtx ctx, const Value *value, EvalRecursive *out) {
  ValueNeutral neutral;
  return eval_candidate_neutral(
      ctx, value_as_neutral(value, &neutral) ? &neutral : NULL, out);
}

/* guarded_point (line 188) and guarded_index (line 135) are folded: the
   next thunk (line 237) is the next turn of the loop. */
Bool eval_guarded_frames(const Spines *frames, Int63 index) {
  for (; frames != NULL; frames = frames->tail) {
    const Spine *frame = frames->head;
    ValuePoint point;
    switch (frame->tag) {
    case VALUE_SELIM: return NO;
    case VALUE_SOUT: break;
    }
    if (!value_as_pt(frame->as.sout.address, &point)) return NO;
    switch (int63_compare(index, int63_zero())) {
    case CMP_LT: return NO;
    case CMP_EQ: {
      ValueIntro intro;
      return eval_guarded_intro(value_as_in(point.arg, &intro) ? &intro
                                                               : NULL);
    }
    case CMP_GT: break;
    }
    index = int63_pred(index);
  }
  return NO;
}

int eval_quote_former(const RulesHost *host, RulesCtx ctx, Int63 size,
                      const ValueShape *shape, const Closure *diagram,
                      const Level *universe, Bool right, const Term **out,
                      const KError **error) {
  const TermShape *s;
  RulesPack p;
  if (!eval_quote_shape(host, ctx, size, shape, &s, error)) return 0;
  if (!rules_rules(shape->tag, &p, error)) return 0;
  return eval_quote_diagram(host, ctx, size, s, diagram, universe, right,
                            rules_diagram_arity(p), out, error);
}

int eval_quote_leg(const RulesHost *host, RulesCtx ctx, Int63 size,
                   const VLeg *leg, const TermLeg **out,
                   const KError **error) {
  return eval_quote_leg_arity(host, ctx, size, leg->binders, leg->clo,
                              TermBinders_length(leg->binders), out, error);
}

/* rules_out_value with the shape evaluated last. rules_out_with reads the
   shape only to freeze a stuck head, so a beta step does not evaluate it
   (an SPI domain is a type, often a mu former). The caller gets p from
   rules_rules on the shape tag first: SPAR and SNU give the same error as
   eval_eval_shape, before any evaluation. */
static int eval_out_lazy(const RulesHost *host, RulesCtx ctx,
                         const Values *env, const RulesEvaluator *ev,
                         RulesPack p, const TermShape *shape,
                         const VAddr *address, const Value *value,
                         const Value **out, const KError **error) {
  const ValueShape *s;
  if (!rules_beta_out(host, p, ev, address, value, out, error)) return 0;
  if (*out != NULL) return 1;
  if (!eval_eval_shape(host, ctx, env, shape, &s, error)) return 0;
  return rules_freeze_option(value, value_sout(s, address), NULL, out,
                             error);
}

/* rules_elim_value with the shape evaluated last, as eval_out_lazy. */
static int eval_elim_lazy(const RulesHost *host, RulesCtx ctx,
                          const Values *env, const RulesEvaluator *ev,
                          RulesPack p, const TermElim *elim,
                          const Value *value, const Value **out,
                          const KError **error) {
  const ValueShape *s;
  if (!rules_beta_elim(host, p, ev, elim->branches, env, value, out, error))
    return 0;
  if (*out != NULL) return 1;
  if (!eval_eval_shape(host, ctx, env, elim->shape, &s, error)) return 0;
  return rules_freeze_option(
      value,
      value_selim(value_stuck(s, elim->scrut_q, elim->motive, elim->branches,
                              env)),
      NULL, out, error);
}

int eval_eval_step(const RulesHost *host, RulesCtx ctx, const Values *env,
                   const Term *term, const Value **out, const KError **error) {
  RulesEvaluator ev = eval_ev(ctx);
  switch (term->tag) {
  case TERM_VAR: {
    const Value *found = at_value(term->as.var.index, env);
    if (found == NULL) {
      *error = error_unbound(text_append(
          text_of_cstring("de Bruijn index "),
          text_append(int63_show(term->as.var.index),
                      text_of_cstring(" is outside the environment"))));
      return 0;
    }
    *out = found;
    return 1;
  }
  case TERM_UNIV: *out = value_vuniv(term->as.univ.level); return 1;
  case TERM_LIT: *out = value_vlit(term->as.lit.literal); return 1;
  case TERM_LAN: {
    const ValueShape *s;
    if (!eval_eval_shape(host, ctx, env, term->as.lan.shape, &s, error))
      return 0;
    *out = value_vlan(s, value_close(env, term->as.lan.diagram), NULL);
    return 1;
  }
  case TERM_RAN: {
    const ValueShape *s;
    if (!eval_eval_shape(host, ctx, env, term->as.ran.shape, &s, error))
      return 0;
    *out = value_vran(s, value_close(env, term->as.ran.diagram), NULL);
    return 1;
  }
  case TERM_IN: {
    const ValueShape *s;
    const VAddr *a;
    const Values *xs;
    if (!eval_eval_shape(host, ctx, env, term->as.in.shape, &s, error))
      return 0;
    if (!eval_eval_addr(host, ctx, env, term->as.in.address, &a, error))
      return 0;
    if (!eval_eval_list(host, ctx, env, term->as.in.args, &xs, error))
      return 0;
    *out = value_vin(s, a, xs);
    return 1;
  }
  case TERM_SEC: {
    const ValueShape *s;
    if (!eval_eval_shape(host, ctx, env, term->as.sec.shape, &s, error))
      return 0;
    *out = value_vsec(s, eval_leg_closures(env, term->as.sec.legs));
    return 1;
  }
  case TERM_OUT: {
    RulesPack p;
    const VAddr *a;
    const Value *v;
    const Value *applied;
    if (!rules_rules(term->as.out.shape->tag, &p, error)) return 0;
    if (!eval_eval_addr(host, ctx, env, term->as.out.address, &a, error))
      return 0;
    if (!eval_request_eval(host, ctx, env, term->as.out.head, &v, error))
      return 0;
    if (!eval_out_lazy(host, ctx, env, &ev, p, term->as.out.shape, a, v,
                       &applied, error))
      return 0;
    return eval_request_whnf(host, ctx, applied, out, error);
  }
  case TERM_ELIM: {
    const TermElim *elim = term->as.elim.elim;
    RulesPack p;
    const Value *v;
    if (!rules_rules(elim->shape->tag, &p, error)) return 0;
    if (!eval_request_eval(host, ctx, env, elim->scrut, &v, error)) return 0;
    return eval_elim_lazy(host, ctx, env, &ev, p, elim, v, out, error);
  }
  case TERM_LET: {
    const Value *v;
    if (!eval_request_eval(host, ctx, env, term->as.let.value, &v, error))
      return 0;
    return eval_request_eval(host, ctx, Values_cons(v, env),
                             term->as.let.body, out, error);
  }
  case TERM_ANN: {
    const Value *v;
    if (!eval_request_eval(host, ctx, env, term->as.ann.term, &v, error))
      return 0;
    return eval_annotate(host, ctx, env, v, term->as.ann.ty, out, error);
  }
  case TERM_GLOBAL:
    return eval_eval_entry(host, ctx, term->as.global.name,
                           eval_find_seen(term->as.global.name,
                                          &term->as.global.seen, ctx.globals),
                           out, error);
  case TERM_AUTO: *error = error_not_yet(rules_auto_word()); return 0;
  }
  return 0; /* each tag returns above */
}

/* The self tail calls (lines 284, 287) are a loop over the frames. */
int eval_quote_spine(const RulesHost *host, RulesCtx ctx, Int63 size,
                     const Spines *frames, const Term *term,
                     const Term **out, const KError **error) {
  for (; frames != NULL; frames = frames->tail) {
    const Spine *frame = frames->head;
    const TermShape *shape;
    switch (frame->tag) {
    case VALUE_SOUT: {
      const TermAddr *address;
      if (!eval_quote_shape(host, ctx, size, frame->as.sout.shape, &shape,
                            error))
        return 0;
      if (!eval_quote_addr(host, ctx, size, frame->as.sout.address, &address,
                           error))
        return 0;
      term = term_out(shape, address, term);
      break;
    }
    case VALUE_SELIM: {
      const Stuck *stuck = frame->as.selim.elim;
      if (!eval_quote_shape(host, ctx, size, stuck->shape, &shape, error))
        return 0;
      term = term_elim(term_elimination(shape, term, stuck->scrut_q,
                                        stuck->motive, stuck->branches));
      break;
    }
    }
  }
  *out = term;
  return 1;
}

int eval_quote_frames(const RulesHost *host, RulesCtx ctx, Int63 size,
                      const Term *term, const Spines *frames,
                      const Term **out, const KError **error) {
  return eval_quote_spine(host, ctx, size, frames, term, out, error);
}

Bool eval_guarded_frame(Int63 index, const Spines *frames) {
  return eval_guarded_frames(frames, index);
}

int eval_whnf_guard(const RulesHost *host, RulesCtx ctx, const Value *value,
                    const Term *body, const Spines *frames, Bool guarded,
                    const Value **out, const KError **error) {
  const Value *head;
  if (!guarded) {
    *out = value;
    return 1;
  }
  if (!eval_request_eval(host, ctx, NULL, body, &head, error)) return 0;
  return eval_replay(host, ctx, head, frames, out, error);
}

int eval_prim_step(RulesCtx ctx, const Value *value, const Value **out,
                   const KError **error) {
  ValueNeutral neutral;
  return eval_prim_neutral(
      ctx, value, value_as_neutral(value, &neutral) ? &neutral : NULL, out,
      error);
}

int eval_whnf_candidate(const RulesHost *host, RulesCtx ctx,
                        const Value *value, const EvalRecursive *recursion,
                        const Value **out, const KError **error) {
  if (recursion == NULL) {
    *out = value;
    return 1;
  }
  return eval_whnf_guard(
      host, ctx, value, recursion->body, recursion->frames,
      eval_guarded_frame(recursion->index, recursion->frames), out, error);
}

/* map_list (kernel_rules line 464) with mapped_leg. */
int eval_quote_legs(const RulesHost *host, RulesCtx ctx, Int63 size,
                    const VLegs *legs, const TermLegs **out,
                    const KError **error) {
  const TermLegs *acc = NULL;
  for (; legs != NULL; legs = legs->tail) {
    const TermLeg *leg;
    if (!eval_quote_leg(host, ctx, size, legs->head, &leg, error)) return 0;
    acc = TermLegs_cons(leg, acc);
  }
  *out = TermLegs_reverse(acc);
  return 1;
}

int eval_quote_step(const RulesHost *host, RulesCtx ctx, Int63 size,
                    const Value *value, const Term **out,
                    const KError **error) {
  switch (value->tag) {
  case VALUE_VUNIV: *out = term_univ(value->as.vuniv.level); return 1;
  case VALUE_VLIT: *out = term_lit(value->as.vlit.literal); return 1;
  case VALUE_VLAN:
    return eval_quote_former(host, ctx, size, value->as.vlan.shape,
                             value->as.vlan.diagram, value->as.vlan.universe,
                             NO, out, error);
  case VALUE_VRAN:
    return eval_quote_former(host, ctx, size, value->as.vran.shape,
                             value->as.vran.diagram, value->as.vran.universe,
                             YES, out, error);
  case VALUE_VIN: {
    const TermShape *s;
    const TermAddr *a;
    const Terms *xs;
    if (!eval_quote_shape(host, ctx, size, value->as.vin.shape, &s, error))
      return 0;
    if (!eval_quote_addr(host, ctx, size, value->as.vin.address, &a, error))
      return 0;
    if (!eval_quote_list(host, ctx, size, value->as.vin.args, &xs, error))
      return 0;
    *out = term_in(s, a, xs);
    return 1;
  }
  case VALUE_VSEC: {
    const TermShape *s;
    const TermLegs *ls;
    if (!eval_quote_shape(host, ctx, size, value->as.vsec.shape, &s, error))
      return 0;
    if (!eval_quote_legs(host, ctx, size, value->as.vsec.legs, &ls, error))
      return 0;
    *out = term_sec(s, ls);
    return 1;
  }
  case VALUE_VNEUTRAL:
    return eval_quote_frames(
        host, ctx, size, eval_quote_head(size, value->as.vneutral.head),
        Spines_reverse(value->as.vneutral.spine), out, error);
  }
  return 0; /* each tag returns above */
}

int eval_whnf_value(const RulesHost *host, RulesCtx ctx, const Value *value,
                    const Value **out, const KError **error) {
  EvalRecursive recursion;
  return eval_whnf_candidate(
      host, ctx, value,
      eval_candidate(ctx, value, &recursion) ? &recursion : NULL, out, error);
}

int eval_whnf_step(const RulesHost *host, RulesCtx ctx, const Value *value,
                   const Value **out, const KError **error) {
  const Value *v;
  if (!eval_prim_step(ctx, value, &v, error)) return 0;
  return eval_whnf_value(host, ctx, v, out, error);
}

/* dispatch (line 335): the RULES_HOST_EVAL arm of rules_request. */
int eval_dispatch(const RulesHost *host, const RulesTask *task,
                  RulesAnswer *answer, const KError **error) {
  switch (task->tag) {
  case RULES_EVALUATE: {
    const Value *v;
    if (!eval_eval_step(host, task->context, task->as.evaluate.env,
                        task->as.evaluate.term, &v, error))
      return 0;
    answer->tag = RULES_VALUE_ANSWER;
    answer->as.value = v;
    return 1;
  }
  case RULES_WEAK: {
    const Value *v;
    if (!eval_whnf_step(host, task->context, task->as.weak.value, &v, error))
      return 0;
    answer->tag = RULES_VALUE_ANSWER;
    answer->as.value = v;
    return 1;
  }
  case RULES_READBACK: {
    const Term *t;
    if (!eval_quote_step(host, task->context, task->as.readback.size,
                         task->as.readback.value, &t, error))
      return 0;
    answer->tag = RULES_TERM_ANSWER;
    answer->as.term = t;
    return 1;
  }
  case RULES_COMPARE:
    *error = error_mismatch(
        text_of_cstring("internal conversion request in evaluation"));
    return 0;
  case RULES_INFER_USES:
  case RULES_CHECK_USES:
  case RULES_INFER_UNIVERSE:
    *error = error_mismatch(
        text_of_cstring("internal checking request in evaluation"));
    return 0;
  }
  return 0; /* each tag returns above */
}

/* R.run with dispatch: an evaluation host on the stack (no fuel). */
static RulesHost eval_host(void) {
  RulesHost host;
  host.kind = RULES_HOST_EVAL;
  host.fuel = NULL;
  return host;
}

int eval_eval(const Globals *globals, const Values *env, const Term *term,
              const Value **out, const KError **error) {
  RulesHost host = eval_host();
  RulesCtx ctx;
  ctx.globals = globals;
  return eval_request_eval(&host, ctx, env, term, out, error);
}

int eval_whnf(const Globals *globals, const Value *value, const Value **out,
              const KError **error) {
  RulesHost host = eval_host();
  RulesCtx ctx;
  ctx.globals = globals;
  return eval_request_whnf(&host, ctx, value, out, error);
}

int eval_quote(const Globals *globals, Int63 size, const Value *value,
               const Term **out, const KError **error) {
  RulesHost host = eval_host();
  RulesCtx ctx;
  ctx.globals = globals;
  return eval_request_quote(&host, ctx, size, value, out, error);
}
