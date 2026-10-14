/* kernel_rules.c: C twin of lib/kernel_rules.bend (stage S2). Runtime,
   conversion and checking rules at Kanon 69f3be51 rules.ml. Direct style:
   a program returns int (1 = Pure with *out, 0 = Abort with *error), and
   a Request is a call of rules_request (sole.h kernel_rules section).
   Session 2 ports Bend lines 1-450. */
#include "sole.h"

/* ------------------------------------------------------- small values */

/* The placeholder is used only when opening an untyped conversion scope. */
const Value *rules_placeholder(void) { return value_vuniv(level_zero()); }

Level rules_level_default(const Level *level) {
  if (level == NULL) return level_zero();
  return *level;
}

int rules_collection_view(const ValueShape *shape, Int63 *out) {
  switch (shape->tag) {
  case SHAPE_SPI: return 0;
  case SHAPE_SCOLL: *out = shape->as.scoll.size; return 1;
  case SHAPE_SPAR: return 0;
  case SHAPE_SMU: return 0;
  case SHAPE_SNU: return 0;
  }
  return 0; /* each tag returns above */
}

const TermBranches *rules_proj_branch(Quantity q, Bool second) {
  const TermBinders *binders = TermBinders_cons(
      (TermBinder){q, text_of_cstring("x")},
      TermBinders_cons((TermBinder){QUANTITY_MANY, text_of_cstring("y")},
                       NULL));
  Int63 index = second ? 0 : 1; /* F.choose(second, zero, one) */
  TermBranch branch = {term_aleg(0), term_leg(binders, term_var(index))};
  return TermBranches_cons(branch, NULL);
}

/* The Bend self tail call on Zero | Succ is one comparison of the host
   Nat. */
Bool rules_nat_equal(Nat a, Nat b) { return a == b ? YES : NO; }

const Term *rules_arrow(Quantity q, Text name, const Term *domain,
                        const Term *codomain) {
  return term_ran(shape_spi_term(q, name, domain), codomain);
}

const Term *rules_unit_ty(Level level) {
  return term_ann(term_ran(shape_scoll_term(0),
                           term_sec(shape_scoll_term(0), NULL)),
                  term_univ(level));
}

const Term *rules_unit_val(void) {
  return term_sec(shape_scoll_term(0), NULL);
}

const Value *rules_bool_value(Bool b) {
  return value_vin(shape_scoll_value(2), value_valeg(b ? 1 : 0),
                   Values_cons(value_vsec(shape_scoll_value(0), NULL), NULL));
}

/* fresh_loop folded: the list is built with int63_succ in the Bend order,
   then reversed once. */
const Values *rules_fresh(Int63 size, Nat count) {
  const Values *reversed = NULL;
  Int63 at = size;
  for (Nat left = count; left > 0; left--) {
    reversed = Values_cons(value_var(at), reversed);
    at = int63_succ(at);
  }
  return Values_reverse(reversed);
}

/* advance_loop folded. */
Int63 rules_advance(Int63 size, Nat count) {
  Int63 at = size;
  for (Nat left = count; left > 0; left--) at = int63_succ(at);
  return at;
}

/* ------------------------------------------------------------- words */

Text rules_spar_word(void) { return text_of_cstring("SPar arrives at M1"); }

Text rules_snu_word(void) { return text_of_cstring("SNu arrives at M2"); }

Text rules_auto_word(void) {
  return text_of_cstring("instances arrive at M2");
}

Text rules_mu_ran_word(void) {
  return text_of_cstring("a right former at a mu shape arrives at M2");
}

Text rules_wrong_pack(void) {
  return text_of_cstring("the rule pack does not match the shape of the term");
}

Text rules_stuck_msg(void) {
  return text_of_cstring("an elimination met a value it cannot eliminate");
}

Text rules_diagram_msg(void) {
  return text_of_cstring("the diagram is not a section of the declared width");
}

Text rules_leg_msg(void) {
  return text_of_cstring("the leg address is outside the collection");
}

/* -------------------------------------------------------------- keys */

Bool rules_branch_key(const VAddr *key, const TermAddr *address) {
  switch (key->tag) {
  case VALUE_VAPT: return NO;
  case VALUE_VALEG:
    switch (address->tag) {
    case TERM_APT: return NO;
    case TERM_ALEG:
      return int63_equal(key->as.valeg.index, address->as.aleg.index);
    case TERM_ACTOR: return NO;
    }
    return NO; /* each tag returns above */
  case VALUE_VACTOR:
    switch (address->tag) {
    case TERM_APT: return NO;
    case TERM_ALEG: return NO;
    case TERM_ACTOR:
      return text_same(key->as.vactor.name, address->as.actor.name);
    }
    return NO; /* each tag returns above */
  }
  return NO; /* each tag returns above */
}

/* NULL = None{}. */
const VAddr *rules_collection_key(const VAddr *address) {
  switch (address->tag) {
  case VALUE_VAPT: return NULL;
  case VALUE_VALEG: return value_valeg(address->as.valeg.index);
  case VALUE_VACTOR: return NULL;
  }
  return NULL; /* each tag returns above */
}

/* NULL = None{}. */
const VAddr *rules_constructor_key(const VAddr *address) {
  switch (address->tag) {
  case VALUE_VAPT: return NULL;
  case VALUE_VALEG: return NULL;
  case VALUE_VACTOR: return value_vactor(address->as.vactor.name);
  }
  return NULL; /* each tag returns above */
}

/* ------------------------------------------------------ shape views */

/* choose_lazy folded (line 149). */
Bool rules_ann_lvl_eq(const ValueShape *shape, const Level *u1,
                      const Level *u2) {
  switch (shape->tag) {
  case SHAPE_SPI: return YES;
  case SHAPE_SCOLL:
    if (int63_equal(shape->as.scoll.size, 0))
      return level_equal(rules_level_default(u1), rules_level_default(u2));
    return YES;
  case SHAPE_SPAR: return YES;
  case SHAPE_SMU: return YES;
  case SHAPE_SNU: return YES;
  }
  return YES; /* each tag returns above */
}

int rules_point_view(const ValueShape *shape, RulesPoint *out) {
  switch (shape->tag) {
  case SHAPE_SPI:
    out->quantity = shape->as.spi.quantity;
    out->name = shape->as.spi.binder;
    out->domain = shape->as.spi.domain;
    return 1;
  case SHAPE_SCOLL: return 0;
  case SHAPE_SPAR: return 0;
  case SHAPE_SMU: return 0;
  case SHAPE_SNU: return 0;
  }
  return 0; /* each tag returns above */
}

int rules_empty_section(const Value *value, const VLegs **out,
                        const KError **error) {
  switch (value->tag) {
  case VALUE_VSEC: *out = value->as.vsec.legs; return 1;
  case VALUE_VUNIV: break;
  case VALUE_VLAN: break;
  case VALUE_VRAN: break;
  case VALUE_VIN: break;
  case VALUE_VLIT: break;
  case VALUE_VNEUTRAL: break;
  }
  *error = error_mismatch(rules_diagram_msg());
  return 0;
}

/* choose_lazy folded (line 175); the self call is a loop. */
Bool rules_erased_fields(const Telescope *args) {
  for (; args != NULL; args = args->tail)
    if (!quantity_equal(args->head.quantity, QUANTITY_ZERO)) return NO;
  return YES;
}

Bool rules_has_eta(RulesPack p, Bool right) {
  switch (p) {
  case RULES_PI_PACK: return YES;
  case RULES_COLL_PACK: return right;
  case RULES_MU_PACK: return NO;
  }
  return NO; /* each tag returns above */
}

const Term *rules_bool_ty(void) {
  const TermLegs *legs = TermLegs_cons(
      term_leg(NULL, rules_unit_ty(level_one())),
      TermLegs_cons(term_leg(NULL, rules_unit_ty(level_one())), NULL));
  return term_lan(shape_scoll_term(2),
                  term_sec(shape_scoll_term(2), legs));
}

int rules_rules(ShapeTag tag, RulesPack *out, const KError **error) {
  switch (tag) {
  case SHAPE_SPI: *out = RULES_PI_PACK; return 1;
  case SHAPE_SCOLL: *out = RULES_COLL_PACK; return 1;
  case SHAPE_SPAR: *error = error_not_yet(rules_spar_word()); return 0;
  case SHAPE_SMU: *out = RULES_MU_PACK; return 1;
  case SHAPE_SNU: *error = error_not_yet(rules_snu_word()); return 0;
  }
  return 0; /* each tag returns above */
}

Nat rules_diagram_arity(RulesPack p) {
  switch (p) {
  case RULES_PI_PACK: return 1;
  case RULES_COLL_PACK: return 0;
  case RULES_MU_PACK: return 0;
  }
  return 0; /* each tag returns above */
}

/* choose_lazy folded (line 232); the self call is a loop. NULL = None{}. */
const TermLeg *rules_branch_leg(const VAddr *key,
                                const TermBranches *branches) {
  for (; branches != NULL; branches = branches->tail)
    if (rules_branch_key(key, branches->head.address))
      return branches->head.leg;
  return NULL;
}

/* ctor NULL = None{}. */
Bool rules_zero_constructor(const Ctor *ctor) {
  if (ctor == NULL) return NO;
  return (rules_erased_fields(ctor->args) && !ctor->self_rec) ? YES : NO;
}

/* ---------------------------------------------------------- programs */

/* The eq closure of point_equal is ask_type at each caller (kernel_conv
   line 20 through shape_eq), so environment = ctx. */
int rules_point_equal(const RulesHost *host, RulesCtx ctx, const Value *a,
                      const Value *b, Bool same, Bool *out,
                      const KError **error) {
  if (!same) {
    *out = NO;
    return 1;
  }
  return rules_ask_type(host, ctx, a, b, out, error);
}

/* neutral NULL = None{} (the V.as_neutral answer at the caller). */
int rules_freeze_neutral(const ValueNeutral *neutral, const Spine *frame,
                         const Value **out, const KError **error) {
  if (neutral == NULL) {
    *error = error_mismatch(rules_stuck_msg());
    return 0;
  }
  *out = value_vneutral(neutral->head, Spines_cons(frame, neutral->spine));
  return 1;
}

/* The three answer readers. Their Fail{e} arm is the 0 return of
   rules_request at the ask_* call site. */
int rules_value_answer(const RulesAnswer *answer, const Value **out,
                       const KError **error) {
  switch (answer->tag) {
  case RULES_VALUE_ANSWER: *out = answer->as.value; return 1;
  case RULES_TERM_ANSWER: break;
  case RULES_BOOL_ANSWER: break;
  case RULES_INFERRED_ANSWER: break;
  case RULES_USAGE_ANSWER: break;
  case RULES_LEVEL_ANSWER: break;
  }
  *error = error_mismatch(text_of_cstring("internal evaluation response kind"));
  return 0;
}

int rules_term_answer(const RulesAnswer *answer, const Term **out,
                      const KError **error) {
  switch (answer->tag) {
  case RULES_VALUE_ANSWER: break;
  case RULES_TERM_ANSWER: *out = answer->as.term; return 1;
  case RULES_BOOL_ANSWER: break;
  case RULES_INFERRED_ANSWER: break;
  case RULES_USAGE_ANSWER: break;
  case RULES_LEVEL_ANSWER: break;
  }
  *error = error_mismatch(text_of_cstring("internal readback response kind"));
  return 0;
}

int rules_bool_answer(const RulesAnswer *answer, Bool *out,
                      const KError **error) {
  switch (answer->tag) {
  case RULES_VALUE_ANSWER: break;
  case RULES_TERM_ANSWER: break;
  case RULES_BOOL_ANSWER: *out = answer->as.equal; return 1;
  case RULES_INFERRED_ANSWER: break;
  case RULES_USAGE_ANSWER: break;
  case RULES_LEVEL_ANSWER: break;
  }
  *error =
      error_mismatch(text_of_cstring("internal conversion response kind"));
  return 0;
}

/* Request{task, resume} without the resume: Run.drive with the dispatcher
   of the host. The C stack holds the nested requests (no Run.stack). */
int rules_request(const RulesHost *host, const RulesTask *task,
                  RulesAnswer *answer, const KError **error) {
  switch (host->kind) {
  case RULES_HOST_EVAL: return eval_dispatch(host, task, answer, error);
  case RULES_HOST_CONV: return conv_dispatch(host, task, answer, error);
  case RULES_HOST_CHECK: return check_request(host, task, answer, error);
  }
  return 0; /* each kind returns above */
}

Bool rules_zero_complete(const Texts *names, const Family *family) {
  if (names == NULL) return YES;
  if (names->tail != NULL) return NO;
  return rules_zero_constructor(positivity_ctor_of(names->head, family));
}

/* payload_eq at A = V.t. Its eq closure is ask_type at each caller
   (kernel_conv line 23; family_equal line 473 through shape_eq), so
   environment = ctx. pbind (line 415) is the 0 return; pchoose is the if;
   the self call is a loop. The V.vleg and branch instances (kernel_conv
   lines 141 and 144) are loops in lib/kernel_conv.c. */
int rules_payload_eq(const RulesHost *host, RulesCtx ctx, const Values *xs,
                     const Values *ys, Bool *out, const KError **error) {
  while (xs != NULL && ys != NULL) {
    Bool same;
    if (!rules_ask_type(host, ctx, xs->head, ys->head, &same, error))
      return 0;
    if (!same) {
      *out = NO;
      return 1;
    }
    xs = xs->tail;
    ys = ys->tail;
  }
  *out = (xs == NULL && ys == NULL) ? YES : NO;
  return 1;
}

/* ev(env, term): the application of an evaluator(C). */
int rules_evaluate(const RulesHost *host, const RulesEvaluator *ev,
                   const Values *env, const Term *term, const Value **out,
                   const KError **error) {
  switch (ev->kind) {
  case RULES_EV_OPS:
    return runtime_ops_eval_env(ev->ctx.check, env, term, out, error);
  case RULES_EV_ASK:
    return rules_ask_eval(host, ev->ctx, env, term, out, error);
  }
  return 0; /* each kind returns above */
}

/* rev_append(args, env) is Values_reverse_onto. */
int rules_open_closure(const RulesHost *host, const RulesEvaluator *ev,
                       const Closure *clo, const Values *args,
                       const Value **out, const KError **error) {
  return rules_evaluate(host, ev, Values_reverse_onto(args, clo->env),
                        clo->body, out, error);
}

/* option NULL = None{}. */
int rules_freeze_option(const Value *value, const Spine *frame,
                        const Value *option, const Value **out,
                        const KError **error) {
  ValueNeutral neutral;
  if (option != NULL) {
    *out = option;
    return 1;
  }
  if (value_as_neutral(value, &neutral))
    return rules_freeze_neutral(&neutral, frame, out, error);
  return rules_freeze_neutral(NULL, frame, out, error);
}

/* ------------------------------------------------------------ ask_* */

int rules_ask_eval(const RulesHost *host, RulesCtx ctx, const Values *env,
                   const Term *term, const Value **out, const KError **error) {
  RulesTask task;
  RulesAnswer answer;
  task.tag = RULES_EVALUATE;
  task.context = ctx;
  task.as.evaluate.env = env;
  task.as.evaluate.term = term;
  if (!rules_request(host, &task, &answer, error)) return 0;
  return rules_value_answer(&answer, out, error);
}

int rules_ask_whnf(const RulesHost *host, RulesCtx ctx, const Value *value,
                   const Value **out, const KError **error) {
  RulesTask task;
  RulesAnswer answer;
  task.tag = RULES_WEAK;
  task.context = ctx;
  task.as.weak.value = value;
  if (!rules_request(host, &task, &answer, error)) return 0;
  return rules_value_answer(&answer, out, error);
}

int rules_ask_quote(const RulesHost *host, RulesCtx ctx, Int63 size,
                    const Value *value, const Term **out,
                    const KError **error) {
  RulesTask task;
  RulesAnswer answer;
  task.tag = RULES_READBACK;
  task.context = ctx;
  task.as.readback.size = size;
  task.as.readback.value = value;
  if (!rules_request(host, &task, &answer, error)) return 0;
  return rules_term_answer(&answer, out, error);
}

int rules_ask_conv(const RulesHost *host, RulesCtx ctx, const Value *ty,
                   const Value *a, const Value *b, Bool *out,
                   const KError **error) {
  RulesTask task;
  RulesAnswer answer;
  task.tag = RULES_COMPARE;
  task.context = ctx;
  task.as.compare.ty = ty;
  task.as.compare.left = a;
  task.as.compare.right = b;
  if (!rules_request(host, &task, &answer, error)) return 0;
  return rules_bool_answer(&answer, out, error);
}

/* ops_ev: lift(o_eval_env(...)) is runtime_ops_eval_env (rules_evaluate). */
RulesEvaluator rules_ops_ev(RulesCtx ctx) {
  RulesEvaluator ev;
  ev.kind = RULES_EV_OPS;
  ev.ctx = ctx;
  return ev;
}

int rules_ask_type(const RulesHost *host, RulesCtx ctx, const Value *a,
                   const Value *b, Bool *out, const KError **error) {
  return rules_ask_conv(host, ctx, rules_placeholder(), a, b, out, error);
}

int rules_open_leg(const RulesHost *host, const RulesEvaluator *ev,
                   const VLeg *leg, const Value **out, const KError **error) {
  return rules_open_closure(host, ev, leg->clo, NULL, out, error);
}

/* ============================================ Bend lines 451-720 (S2 s3) */

/* No twin here: map_list (line 464) and map_shape (line 531) have one f
   maker per element type (kernel_eval lines 211-220 and 312, erase/type
   line 92), so each caller writes its own loop with a direct call (S3,
   S4). freeze (line 498) binds the stepped program first, so the caller
   runs the beta step and then calls rules_freeze_option. */

/* o_bind(C, ops, name, q, ty, ctx) on the runtime_ops host. */
static RulesCtx ops_bind(Text name, Quantity q, const Value *ty,
                         RulesCtx ctx) {
  RulesCtx bound;
  bound.check = runtime_ops_bind(name, q, ty, ctx.check);
  return bound;
}

/* V.var(o_size(C, ops, ctx)). */
static const Value *ops_var(RulesCtx ctx) {
  return value_var(runtime_ops_size(ctx.check));
}

Bool rules_zero_family_view(const Family *family, const Family *view) {
  switch (view->status->tag) {
  case CTOR_BUILTIN: return NO;
  case CTOR_PROVISIONAL: return NO;
  case CTOR_COMPLETE:
    return (level_equal(view->level, level_zero()) && view->positive &&
            rules_zero_complete(view->status->as.complete.names, family))
               ? YES
               : NO;
  }
  return NO; /* each tag returns above */
}

/* The self call on Zero | Succ is a loop: count binds, in the Bend order. */
RulesCtx rules_grow(Nat count, RulesCtx ctx) {
  for (; count > 0; count--)
    ctx = ops_bind(text_of_cstring("_"), QUANTITY_MANY, rules_placeholder(),
                   ctx);
  return ctx;
}

/* eq = ask_type (folded), so environment = ctx. */
int rules_family_equal(const RulesHost *host, RulesCtx ctx, const Values *a,
                       const Values *b, Bool same, Bool *out,
                       const KError **error) {
  if (!same) {
    *out = NO;
    return 1;
  }
  return rules_payload_eq(host, ctx, a, b, out, error);
}

/* leg NULL = None{}. *out NULL = None{} (the Maybe<V.t> answer). */
int rules_beta_closure(const RulesHost *host, const RulesEvaluator *ev,
                       const Values *args, const VLeg *leg, const Value **out,
                       const KError **error) {
  if (leg == NULL) {
    *out = NULL;
    return 1;
  }
  return rules_open_closure(host, ev, leg->clo, args, out, error);
}

/* point, arg and branch NULL = None{}; seven arms give None{}. */
int rules_beta_pair(const RulesHost *host, const RulesEvaluator *ev,
                    const Values *env, const ValuePoint *point,
                    const Value *arg, const TermBranch *branch,
                    const Value **out, const KError **error) {
  if (point == NULL || arg == NULL || branch == NULL) {
    *out = NULL;
    return 1;
  }
  return rules_evaluate(host, ev, Values_cons(arg, Values_cons(point->arg, env)),
                        branch->leg->body, out, error);
}

/* pchoose is the if; rev_append is Values_reverse_onto. */
int rules_beta_body(const RulesHost *host, const RulesEvaluator *ev,
                    const Values *env, const Values *args,
                    const TermLeg *branch, const Value **out,
                    const KError **error) {
  if (branch == NULL) {
    *out = NULL;
    return 1;
  }
  if (int63_equal(
          rules_advance(int63_zero(), TermBinders_length(branch->binders)),
          rules_advance(int63_zero(), Values_length(args))))
    return rules_evaluate(host, ev, Values_reverse_onto(args, env),
                          branch->body, out, error);
  *error = error_not_yet(text_of_cstring(
      "delayed recursive evaluation of a recursor branch is not supported"));
  return 0;
}

int rules_coll_legs(const RulesHost *host, RulesCtx ctx,
                    const Closure *diagram, const VLegs **out,
                    const KError **error) {
  RulesEvaluator ev = rules_ops_ev(ctx);
  const Value *v;
  if (!rules_open_closure(host, &ev, diagram, NULL, &v, error)) return 0;
  return rules_empty_section(v, out, error);
}

/* at(V.vleg, index, xs) with at_loop (lines 258, 208); NULL = None{}.
   choose_lazy is the if; the self call is a loop. */
const VLeg *rules_at_vleg(Int63 index, const VLegs *xs) {
  for (; xs != NULL; xs = xs->tail) {
    if (int63_equal(index, int63_zero())) return xs->head;
    index = int63_pred(index);
  }
  return NULL;
}

/* one_of (line 54) at V.vleg, V.t and F.Pair2<T.addr, T.leg>; NULL =
   None{}. */
const VLeg *rules_one_of_vleg(const VLegs *xs) {
  if (xs == NULL || xs->tail != NULL) return NULL;
  return xs->head;
}

const Value *rules_one_of_value(const Values *xs) {
  if (xs == NULL || xs->tail != NULL) return NULL;
  return xs->head;
}

const TermBranch *rules_one_of_branch(const TermBranches *xs) {
  if (xs == NULL || xs->tail != NULL) return NULL;
  return &xs->head;
}

/* require(at(...), E.Wrong_leg{leg_msg}) is the NULL test. */
int rules_leg_type(const RulesHost *host, RulesCtx ctx, const VLegs *legs,
                   Int63 index, const Value **out, const KError **error) {
  RulesEvaluator ev = rules_ops_ev(ctx);
  const VLeg *leg = rules_at_vleg(index, legs);
  if (leg == NULL) {
    *error = error_wrong_leg(rules_leg_msg());
    return 0;
  }
  return rules_open_leg(host, &ev, leg, out, error);
}

Bool rules_zero_family(const Family *family) {
  return rules_zero_family_view(family, family);
}

/* info and address NULL = None{}. out->result NULL = None{} (the result
   of a SpineStep is never NULL). */
int rules_spine_point(const RulesHost *host, RulesCtx ctx,
                      const Closure *diagram, const RulesPoint *info,
                      const ValuePoint *address, RulesSpineStep *out,
                      const KError **error) {
  RulesEvaluator ev = rules_ops_ev(ctx);
  const Value *cod;
  out->argument = NULL;
  out->result = NULL;
  if (info == NULL || address == NULL) return 1;
  if (!rules_open_closure(host, &ev, diagram, Values_cons(address->arg, NULL),
                          &cod, error))
    return 0;
  out->argument = info->domain;
  out->result = cod;
  return 1;
}

int rules_spine_leg(const RulesHost *host, RulesCtx ctx, const VLeg *leg,
                    RulesSpineStep *out, const KError **error) {
  RulesEvaluator ev = rules_ops_ev(ctx);
  const Value *ty;
  out->argument = NULL;
  out->result = NULL;
  if (leg == NULL) return 1;
  if (!rules_open_closure(host, &ev, leg->clo, NULL, &ty, error)) return 0;
  out->result = ty;
  return 1;
}

int rules_point_diagrams(const RulesHost *host, RulesCtx ctx,
                         const Closure *d1, const Closure *d2,
                         const RulesPoint *info, Bool *out,
                         const KError **error) {
  RulesEvaluator ev = rules_ops_ev(ctx);
  const Value *a;
  const Value *b;
  if (!rules_open_closure(host, &ev, d1, Values_cons(ops_var(ctx), NULL), &a,
                          error))
    return 0;
  if (!rules_open_closure(host, &ev, d2, Values_cons(ops_var(ctx), NULL), &b,
                          error))
    return 0;
  return rules_ask_type(host,
                        ops_bind(info->name, info->quantity, info->domain, ctx),
                        a, b, out, error);
}

/* eq = ask_type (folded). Each pair of different tags gives False{} (20
   arms), so one tag test serves them. */
int rules_shape_eq(const RulesHost *host, RulesCtx ctx, const ValueShape *a,
                   const ValueShape *b, Bool *out, const KError **error) {
  if (a->tag != b->tag) {
    *out = NO;
    return 1;
  }
  switch (a->tag) {
  case SHAPE_SPI:
    return rules_point_equal(
        host, ctx, a->as.spi.domain, b->as.spi.domain,
        quantity_equal(a->as.spi.quantity, b->as.spi.quantity), out, error);
  case SHAPE_SCOLL:
    *out = int63_equal(a->as.scoll.size, b->as.scoll.size);
    return 1;
  case SHAPE_SPAR:
    *error = error_not_yet(rules_spar_word());
    return 0;
  case SHAPE_SMU:
    return rules_family_equal(host, ctx, a->as.smu.indices, b->as.smu.indices,
                              text_same(a->as.smu.family, b->as.smu.family),
                              out, error);
  case SHAPE_SNU:
    *error = error_not_yet(rules_snu_word());
    return 0;
  }
  return 0; /* each tag returns above */
}

/* point and section NULL = None{}. */
int rules_beta_point(const RulesHost *host, const RulesEvaluator *ev,
                     const ValuePoint *point, const ValueSec *section,
                     const Value **out, const KError **error) {
  if (point == NULL || section == NULL) {
    *out = NULL;
    return 1;
  }
  return rules_beta_closure(host, ev, Values_cons(point->arg, NULL),
                            rules_one_of_vleg(section->legs), out, error);
}

/* index and section NULL = None{}. */
int rules_beta_collection(const RulesHost *host, const RulesEvaluator *ev,
                          const Int63 *index, const ValueSec *section,
                          const Value **out, const KError **error) {
  if (index == NULL || section == NULL) {
    *out = NULL;
    return 1;
  }
  return rules_beta_closure(host, ev, NULL, rules_at_vleg(*index, section->legs),
                            out, error);
}

/* key NULL = None{}. */
int rules_beta_branch(const RulesHost *host, const RulesEvaluator *ev,
                      const VAddr *key, const TermBranches *branches,
                      const Values *env, const Values *args,
                      const Value **out, const KError **error) {
  if (key == NULL) {
    *out = NULL;
    return 1;
  }
  return rules_beta_body(host, ev, env, args, rules_branch_leg(key, branches),
                         out, error);
}

/* family NULL = None{}. */
Bool rules_subsingleton_found(const Family *family) {
  if (family == NULL) return NO;
  return rules_zero_family(family);
}

/* address NULL = None{}. */
int rules_spine_collection(const RulesHost *host, RulesCtx ctx,
                           const Closure *diagram, const Int63 *address,
                           RulesSpineStep *out, const KError **error) {
  const VLegs *legs;
  out->argument = NULL;
  out->result = NULL;
  if (address == NULL) return 1;
  if (!rules_coll_legs(host, ctx, diagram, &legs, error)) return 0;
  return rules_spine_leg(host, ctx, rules_at_vleg(*address, legs), out, error);
}

/* require(point_view(shape), E.Mismatch{wrong_pack}) is the 0 test. */
int rules_conv_diagram(const RulesHost *host, RulesCtx ctx, RulesPack p,
                       const ValueShape *shape, const Closure *d1,
                       const Closure *d2, Bool *out, const KError **error) {
  RulesEvaluator ev = rules_ops_ev(ctx);
  RulesPoint info;
  const Value *a;
  const Value *b;
  switch (p) {
  case RULES_PI_PACK:
    if (!rules_point_view(shape, &info)) {
      *error = error_mismatch(rules_wrong_pack());
      return 0;
    }
    return rules_point_diagrams(host, ctx, d1, d2, &info, out, error);
  case RULES_COLL_PACK: /* the same body as MuPack */
  case RULES_MU_PACK:
    if (!rules_open_closure(host, &ev, d1, NULL, &a, error)) return 0;
    if (!rules_open_closure(host, &ev, d2, NULL, &b, error)) return 0;
    return rules_ask_type(host, ctx, a, b, out, error);
  }
  return 0; /* each pack returns above */
}

int rules_beta_out(const RulesHost *host, RulesPack p,
                   const RulesEvaluator *ev, const VAddr *address,
                   const Value *value, const Value **out,
                   const KError **error) {
  ValuePoint point;
  ValueSec section;
  Int63 index;
  switch (p) {
  case RULES_MU_PACK:
    *error = error_not_yet(rules_mu_ran_word());
    return 0;
  case RULES_PI_PACK:
    return rules_beta_point(
        host, ev, value_as_pt(address, &point) ? &point : NULL,
        value_as_sec(value, &section) ? &section : NULL, out, error);
  case RULES_COLL_PACK:
    return rules_beta_collection(
        host, ev, value_as_leg(address, &index) ? &index : NULL,
        value_as_sec(value, &section) ? &section : NULL, out, error);
  }
  return 0; /* each pack returns above */
}

/* intro NULL = None{}; the three None{} arms are the first test. */
int rules_beta_intro(const RulesHost *host, RulesPack p,
                     const RulesEvaluator *ev, const TermBranches *branches,
                     const Values *env, const ValueIntro *intro,
                     const Value **out, const KError **error) {
  ValuePoint point;
  if (intro == NULL) {
    *out = NULL;
    return 1;
  }
  switch (p) {
  case RULES_PI_PACK:
    return rules_beta_pair(
        host, ev, env, value_as_pt(intro->address, &point) ? &point : NULL,
        rules_one_of_value(intro->args), rules_one_of_branch(branches), out,
        error);
  case RULES_COLL_PACK:
    return rules_beta_branch(host, ev, rules_collection_key(intro->address),
                             branches, env, intro->args, out, error);
  case RULES_MU_PACK:
    return rules_beta_branch(host, ev, rules_constructor_key(intro->address),
                             branches, env, intro->args, out, error);
  }
  return 0; /* each pack returns above */
}

Bool rules_subsingleton(RulesCtx ctx, const ValueShape *shape) {
  switch (shape->tag) {
  case SHAPE_SMU:
    return rules_subsingleton_found(
        runtime_ops_family(ctx.check, shape->as.smu.family));
  case SHAPE_SPI: return NO;
  case SHAPE_SCOLL: return NO;
  case SHAPE_SPAR: return NO;
  case SHAPE_SNU: return NO;
  }
  return NO; /* each tag returns above */
}

int rules_spine_ty(const RulesHost *host, RulesCtx ctx, RulesPack p,
                   const ValueShape *shape, const Closure *diagram,
                   const VAddr *address, RulesSpineStep *out,
                   const KError **error) {
  RulesPoint info;
  ValuePoint point;
  Int63 index;
  switch (p) {
  case RULES_PI_PACK:
    return rules_spine_point(
        host, ctx, diagram, rules_point_view(shape, &info) ? &info : NULL,
        value_as_pt(address, &point) ? &point : NULL, out, error);
  case RULES_COLL_PACK:
    return rules_spine_collection(
        host, ctx, diagram, value_as_leg(address, &index) ? &index : NULL, out,
        error);
  case RULES_MU_PACK:
    out->argument = NULL;
    out->result = NULL;
    return 1;
  }
  return 0; /* each pack returns above */
}

int rules_beta_elim(const RulesHost *host, RulesPack p,
                    const RulesEvaluator *ev, const TermBranches *branches,
                    const Values *env, const Value *value, const Value **out,
                    const KError **error) {
  ValueIntro intro;
  return rules_beta_intro(host, p, ev, branches, env,
                          value_as_in(value, &intro) ? &intro : NULL, out,
                          error);
}

/* freeze (line 498) folded: the beta step, then freeze_option. */
int rules_out_with(const RulesHost *host, RulesPack p,
                   const RulesEvaluator *ev, const ValueShape *shape,
                   const VAddr *address, const Value *value,
                   const Value **out, const KError **error) {
  const Value *option;
  if (!rules_beta_out(host, p, ev, address, value, &option, error)) return 0;
  return rules_freeze_option(value, value_sout(shape, address), option, out,
                             error);
}

int rules_eta_right(const RulesHost *host, RulesCtx ctx,
                    const ValueShape *shape, const Closure *diagram,
                    const Value *a, const Value *b, Quantity q, Text name,
                    const Value *domain, Bool *out, const KError **error) {
  RulesEvaluator ev = rules_ops_ev(ctx);
  const Value *cod;
  const Value *fa;
  const Value *fb;
  if (!rules_open_closure(host, &ev, diagram, Values_cons(ops_var(ctx), NULL),
                          &cod, error))
    return 0;
  if (!rules_out_with(host, RULES_PI_PACK, &ev, shape,
                      value_vapt(q, ops_var(ctx)), a, &fa, error))
    return 0;
  if (!rules_out_with(host, RULES_PI_PACK, &ev, shape,
                      value_vapt(q, ops_var(ctx)), b, &fb, error))
    return 0;
  return rules_ask_conv(host, ops_bind(name, q, domain, ctx), cod, fa, fb, out,
                        error);
}

/* @unsafe (line 662). count_step is the switch (its text stays here),
   pchoose is the if, and the self tail call is a loop. */
int rules_coll_eta_loop(const RulesHost *host, RulesCtx ctx,
                        const ValueShape *shape, const VLegs *legs,
                        const Value *a, const Value *b, Int63 index,
                        Int63 remaining, Bool *out, const KError **error) {
  RulesEvaluator ev = rules_ops_ev(ctx);
  for (;;) {
    const Value *ty;
    const Value *x;
    const Value *y;
    Bool same;
    switch (int63_compare(remaining, int63_zero())) {
    case CMP_LT:
      *error = error_mismatch(
          text_of_cstring("a collection width must be nonnegative"));
      return 0;
    case CMP_EQ:
      *out = YES;
      return 1;
    case CMP_GT: break;
    }
    if (!rules_leg_type(host, ctx, legs, index, &ty, error)) return 0;
    if (!rules_out_with(host, RULES_COLL_PACK, &ev, shape, value_valeg(index),
                        a, &x, error))
      return 0;
    if (!rules_out_with(host, RULES_COLL_PACK, &ev, shape, value_valeg(index),
                        b, &y, error))
      return 0;
    if (!rules_ask_conv(host, ctx, ty, x, y, &same, error)) return 0;
    if (!same) {
      *out = NO;
      return 1;
    }
    index = int63_succ(index);
    remaining = int63_pred(remaining);
  }
}

/* lift(rules(V.t, shape)) is rules_rules on the tag. */
int rules_out_value(const RulesHost *host, const RulesEvaluator *ev,
                    const ValueShape *shape, const VAddr *address,
                    const Value *value, const Value **out,
                    const KError **error) {
  RulesPack p;
  if (!rules_rules(shape->tag, &p, error)) return 0;
  return rules_out_with(host, p, ev, shape, address, value, out, error);
}

/* freeze (line 498) folded: the beta step, then freeze_option. */
int rules_elim_with(const RulesHost *host, RulesPack p,
                    const RulesEvaluator *ev, const ValueShape *shape,
                    Quantity q, const TermMotive *motive,
                    const TermBranches *branches, const Values *env,
                    const Value *value, const Value **out,
                    const KError **error) {
  const Value *option;
  if (!rules_beta_elim(host, p, ev, branches, env, value, &option, error))
    return 0;
  return rules_freeze_option(
      value, value_selim(value_stuck(shape, q, motive, branches, env)), option,
      out, error);
}

int rules_motive_body(const RulesHost *host, RulesCtx ctx,
                      const ValueShape *shape, const Closure *diagram,
                      Quantity q, const Value *domain, Bool second,
                      const Value **out, const KError **error) {
  RulesEvaluator ev = rules_ops_ev(ctx);
  const Value *point;
  if (!second) {
    *out = domain;
    return 1;
  }
  if (!rules_elim_with(host, RULES_PI_PACK, &ev, shape, q, NULL,
                       rules_proj_branch(q, NO), runtime_ops_env(ctx.check),
                       ops_var(ctx), &point, error))
    return 0;
  return rules_open_closure(host, &ev, diagram, Values_cons(point, NULL), out,
                            error);
}

int rules_elim_value(const RulesHost *host, const RulesEvaluator *ev,
                     const ValueShape *shape, Quantity q,
                     const TermMotive *motive, const TermBranches *branches,
                     const Values *env, const Value *value,
                     const Value **out, const KError **error) {
  RulesPack p;
  if (!rules_rules(shape->tag, &p, error)) return 0;
  return rules_elim_with(host, p, ev, shape, q, motive, branches, env, value,
                         out, error);
}

/* The answer is always Some{...}, so *out is never NULL. */
int rules_proj_motive(const RulesHost *host, RulesCtx ctx,
                      const ValueShape *shape, const Closure *diagram,
                      Quantity q, Text name, const Value *domain, Bool second,
                      const TermMotive **out, const KError **error) {
  const Value *value;
  const Term *body;
  if (!rules_motive_body(host, ctx, shape, diagram, q, domain, second, &value,
                         error))
    return 0;
  if (!runtime_ops_quote(ops_bind(name, q, domain, ctx).check, value, &body,
                         error))
    return 0;
  *out = term_motive(maybe_text_none(), NULL, text_of_cstring("self"), body);
  return 1;
}

int rules_eta_second(const RulesHost *host, RulesCtx ctx,
                     const ValueShape *shape, const Closure *diagram,
                     const Value *a, const Value *b, Quantity q, Text name,
                     const Value *domain, const Value *point, Bool *out,
                     const KError **error) {
  RulesEvaluator ev = rules_ops_ev(ctx);
  const Value *cod;
  const TermMotive *mo;
  const Value *a2;
  const Value *b2;
  if (!rules_open_closure(host, &ev, diagram, Values_cons(point, NULL), &cod,
                          error))
    return 0;
  if (!rules_proj_motive(host, ctx, shape, diagram, q, name, domain, YES, &mo,
                         error))
    return 0;
  if (!rules_elim_with(host, RULES_PI_PACK, &ev, shape, QUANTITY_ONE, mo,
                       rules_proj_branch(q, YES), runtime_ops_env(ctx.check),
                       a, &a2, error))
    return 0;
  if (!rules_elim_with(host, RULES_PI_PACK, &ev, shape, QUANTITY_ONE, mo,
                       rules_proj_branch(q, YES), runtime_ops_env(ctx.check),
                       b, &b2, error))
    return 0;
  return rules_ask_conv(host, ctx, cod, a2, b2, out, error);
}

/* pchoose is the if. */
int rules_eta_left(const RulesHost *host, RulesCtx ctx,
                   const ValueShape *shape, const Closure *diagram,
                   const Value *a, const Value *b, Quantity q, Text name,
                   const Value *domain, Bool *out, const KError **error) {
  RulesEvaluator ev = rules_ops_ev(ctx);
  const TermMotive *mo;
  const Value *a1;
  const Value *b1;
  Bool first;
  if (!rules_proj_motive(host, ctx, shape, diagram, q, name, domain, NO, &mo,
                         error))
    return 0;
  if (!rules_elim_with(host, RULES_PI_PACK, &ev, shape, QUANTITY_ONE, mo,
                       rules_proj_branch(q, NO), runtime_ops_env(ctx.check),
                       a, &a1, error))
    return 0;
  if (!rules_elim_with(host, RULES_PI_PACK, &ev, shape, QUANTITY_ONE, mo,
                       rules_proj_branch(q, NO), runtime_ops_env(ctx.check),
                       b, &b1, error))
    return 0;
  if (!rules_ask_conv(host, ctx, domain, a1, b1, &first, error)) return 0;
  if (first)
    return rules_eta_second(host, ctx, shape, diagram, a, b, q, name, domain,
                            a1, out, error);
  *out = NO;
  return 1;
}

int rules_eta_point(const RulesHost *host, RulesCtx ctx,
                    const ValueShape *shape, const Closure *diagram,
                    const Value *a, const Value *b, Bool right,
                    const RulesPoint *info, Bool *out, const KError **error) {
  if (right)
    return rules_eta_right(host, ctx, shape, diagram, a, b, info->quantity,
                           info->name, info->domain, out, error);
  return rules_eta_left(host, ctx, shape, diagram, a, b, info->quantity,
                        info->name, info->domain, out, error);
}

/* Each require(...view(shape), E.Mismatch{wrong_pack}) is a 0 test. */
int rules_expand(const RulesHost *host, RulesCtx ctx, RulesPack p,
                 const ValueShape *shape, const Closure *diagram,
                 const Value *a, const Value *b, Bool right, Bool *out,
                 const KError **error) {
  RulesPoint info;
  Int63 n;
  const VLegs *legs;
  switch (p) {
  case RULES_PI_PACK:
    if (!rules_point_view(shape, &info)) {
      *error = error_mismatch(rules_wrong_pack());
      return 0;
    }
    return rules_eta_point(host, ctx, shape, diagram, a, b, right, &info, out,
                           error);
  case RULES_COLL_PACK:
    if (!rules_collection_view(shape, &n)) {
      *error = error_mismatch(rules_wrong_pack());
      return 0;
    }
    if (!rules_coll_legs(host, ctx, diagram, &legs, error)) return 0;
    return rules_coll_eta_loop(host, ctx, shape, legs, a, b, int63_zero(), n,
                               out, error);
  case RULES_MU_PACK:
    *out = NO;
    return 1;
  }
  return 0; /* each pack returns above */
}

/* ============================================ Bend lines 721-1002 (S2 s4) */

/* String.concat of a fixed list of parts (the pp_cat idiom of kernel_pp). */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

/* check_ops (line 721) has one maker, kernel_check ops (line 98), and
   c_runtime (724) only reads the record: no C twin. c_close and c_pp call
   the two other fields, check_ops_close and check_ops_pp (S3). */
int rules_c_close(RulesCtx ctx, Int63 size, Quantity q, QuantityUsage uses,
                  QuantityUsage *out, const KError **error) {
  return check_ops_close(ctx.check, size, q, uses, out, error);
}

Text rules_c_pp(RulesCtx ctx, const Value *value) {
  return check_ops_pp(ctx.check, value);
}

int rules_c_conv(const RulesHost *host, RulesCtx ctx, const Value *ty,
                 const Value *a, const Value *b, Bool *out,
                 const KError **error) {
  return rules_ask_conv(host, ctx, ty, a, b, out, error);
}

int rules_c_conv_type(const RulesHost *host, RulesCtx ctx, const Value *a,
                      const Value *b, Bool *out, const KError **error) {
  return rules_ask_type(host, ctx, a, b, out, error);
}

int rules_c_eval(RulesCtx ctx, const Term *tm, const Value **out,
                 const KError **error) {
  return runtime_ops_eval_env(ctx.check, runtime_ops_env(ctx.check), tm, out,
                              error);
}

/* args is rev_append onto the closure environment. */
int rules_c_open(RulesCtx ctx, const Closure *clo, const Values *args,
                 const Value **out, const KError **error) {
  return runtime_ops_eval_env(ctx.check, Values_reverse_onto(args, clo->env),
                              clo->body, out, error);
}

/* c_open_at (S14): c_eval of arg, then c_open of clo at that value. When the
   closure body has no T.Var, it does not read the value, thus the c_eval is
   not done. The argument is checked before this call, its evaluation spends
   no fuel and gives no error, thus the result stays the same. This stops
   the O(depth) evaluation of each argument of a nested application. */
int rules_c_open_at(RulesCtx ctx, const Closure *clo, const Term *arg,
                    const Value **out, const KError **error) {
  const Value *v;
  if (!term_exists_var(clo->body))
    return rules_c_open(ctx, clo, NULL, out, error);
  if (!rules_c_eval(ctx, arg, &v, error)) return 0;
  return rules_c_open(ctx, clo, Values_cons(v, NULL), out, error);
}

RulesCtx rules_c_bind(RulesCtx ctx, Text x, Quantity q, const Value *ty) {
  return ops_bind(x, q, ty, ctx);
}

RulesCtx rules_c_define(RulesCtx ctx, Text x, Quantity q, const Value *ty,
                        const Value *value) {
  RulesCtx defined;
  defined.check = runtime_ops_define(x, q, ty, value, ctx.check);
  return defined;
}

Int63 rules_c_size(RulesCtx ctx) { return runtime_ops_size(ctx.check); }

/* crequire (753) is require: each site is a 0 test. The Fail{e} arm of
   inferred_answer and usage_answer is the 0 return of rules_request. */
int rules_inferred_answer(const RulesAnswer *answer, RulesInferred *out,
                          const KError **error) {
  switch (answer->tag) {
  case RULES_INFERRED_ANSWER:
    out->ty = answer->as.inferred.ty;
    out->uses = answer->as.inferred.uses;
    return 1;
  case RULES_VALUE_ANSWER:
  case RULES_TERM_ANSWER:
  case RULES_BOOL_ANSWER:
  case RULES_USAGE_ANSWER:
  case RULES_LEVEL_ANSWER:
    break;
  }
  *error = error_mismatch(text_of_cstring("internal checking answer"));
  return 0;
}

int rules_usage_answer(const RulesAnswer *answer, QuantityUsage *out,
                       const KError **error) {
  switch (answer->tag) {
  case RULES_USAGE_ANSWER:
    *out = answer->as.uses;
    return 1;
  case RULES_VALUE_ANSWER:
  case RULES_TERM_ANSWER:
  case RULES_BOOL_ANSWER:
  case RULES_INFERRED_ANSWER:
  case RULES_LEVEL_ANSWER:
    break;
  }
  *error = error_mismatch(text_of_cstring("internal checking answer"));
  return 0;
}

/* guard (775) is an if at each site. split, with_leg, former_apply and
   point_apply (777-791) only take a record apart for next: each site reads
   the fields. with_point gives the point in *out (next is the caller). */
int rules_with_point(const ValueShape *s, Text message, RulesPoint *out,
                     const KError **error) {
  if (rules_point_view(s, out)) return 1;
  *error = error_mismatch(message);
  return 0;
}

int rules_binder_quantity(Text subject, Text source, Text name,
                          Quantity marked, Quantity declared,
                          const KError **error) {
  Text parts[10];
  if (quantity_equal(marked, declared)) return 1;
  parts[0] = text_of_cstring("the ");
  parts[1] = subject;
  parts[2] = text_of_cstring(" ");
  parts[3] = name;
  parts[4] = text_of_cstring(" is marked ");
  parts[5] = quantity_to_string(marked);
  parts[6] = text_of_cstring(" and the ");
  parts[7] = source;
  parts[8] = text_of_cstring(" marks it ");
  parts[9] = quantity_to_string(declared);
  *error = error_quantity(cat(parts, 10));
  return 0;
}

/* mo and expected: NULL = None{}. */
int rules_elim_result(RulesCtx ctx, const TermMotive *mo,
                      const Value *expected, const Value *self,
                      const Value **out, const KError **error) {
  if (mo != NULL)
    return runtime_ops_eval_env(
        ctx.check, Values_cons(self, runtime_ops_env(ctx.check)), mo->body,
        out, error);
  if (expected == NULL) {
    *error = error_cannot_infer(text_of_cstring(
        "an elimination without a motive needs an expected type"));
    return 0;
  }
  *out = expected;
  return 1;
}

/* length (804) is rules_advance(int63_zero(), X_length(xs)) at each site.
   two_of (806), coll_width (817) and as_mu (912) are generic: the first
   callers are in Bend lines 1136-1318 (session 5). */
int rules_as_tsec(const Term *tm, const TermShape **shape,
                  const TermLegs **legs) {
  if (tm->tag != TERM_SEC) return 0;
  *shape = tm->as.sec.shape;
  *legs = tm->as.sec.legs;
  return 1;
}

Bool rules_diagram_unbound(const TermLegs *legs) {
  for (; legs != NULL; legs = legs->tail)
    if (TermBinders_length(legs->head->binders) != 0) return NO;
  return YES;
}

int rules_coll_legs_of(RulesCtx ctx, const Closure *d, const VLegs **out,
                       const KError **error) {
  const Value *v;
  ValueSec sec;
  if (!rules_c_open(ctx, d, NULL, &v, error)) return 0;
  if (!value_as_sec(v, &sec)) {
    *error = error_mismatch(rules_diagram_msg());
    return 0;
  }
  *out = sec.legs;
  return 1;
}

int rules_c_open_leg(RulesCtx ctx, const VLeg *leg, const Value **out,
                     const KError **error) {
  return rules_c_open(ctx, leg->clo, NULL, out, error);
}

int rules_coll_leg_ty(RulesCtx ctx, const VLegs *legs, Int63 k,
                      const Value **out, const KError **error) {
  const VLeg *lg = rules_at_vleg(k, legs);
  if (lg == NULL) {
    *error = error_wrong_leg(rules_leg_msg());
    return 0;
  }
  return rules_c_open_leg(ctx, lg, out, error);
}

Bool rules_nonnegative(Cmp order) {
  switch (order) {
  case CMP_LT:
    return NO;
  case CMP_EQ:
  case CMP_GT:
    return YES;
  }
  return NO;
}

Bool rules_less_than(Cmp order) {
  switch (order) {
  case CMP_LT:
    return YES;
  case CMP_EQ:
  case CMP_GT:
    return NO;
  }
  return NO;
}

int rules_leg_bound(Int63 k, Int63 n, const KError **error) {
  if (rules_nonnegative(int63_compare(k, int63_zero())) &&
      rules_less_than(int63_compare(k, n)))
    return 1;
  *error = error_wrong_leg(rules_leg_msg());
  return 0;
}

/* The three T.Univ{L.zero} cells (the SPi domain and both SPar fields) are
   one shared cell. */
const RulesShapes *rules_samples(void) {
  const Term *univ = term_univ(level_zero());
  Text x = text_of_cstring("x");
  return RulesShapes_cons(
      shape_spi_term(QUANTITY_MANY, x, univ),
      RulesShapes_cons(
          shape_scoll_term(int63_zero()),
          RulesShapes_cons(
              shape_spar_term(univ, univ),
              RulesShapes_cons(shape_smu_term(x, NULL),
                               RulesShapes_cons(shape_snu_term(x, NULL),
                                                NULL)))));
}

/* value: NULL = Fail{_}. */
const RulesNamedPacks *rules_present_pack(const RulesPack *value, Text name,
                                          const RulesNamedPacks *tail) {
  RulesNamedPack row;
  if (value == NULL) return tail;
  row.name = name;
  row.pack = *value;
  return RulesNamedPacks_cons(row, tail);
}

/* The recursion is a loop over the reversed list (the same order). The
   error of a Fail{_} is not read. */
const RulesNamedPacks *rules_collect_packs(const RulesShapes *xs) {
  const RulesNamedPacks *acc = NULL;
  const KError *unread;
  RulesPack p;
  for (xs = RulesShapes_reverse(xs); xs != NULL; xs = xs->tail)
    acc = rules_present_pack(
        rules_rules(xs->head->tag, &p, &unread) ? &p : NULL,
        shape_name_term(xs->head), acc);
  return acc;
}

const RulesNamedPacks *rules_packs(void) {
  return rules_collect_packs(rules_samples());
}

static const Texts *named_rules(void) {
  return Texts_cons(
      text_of_cstring("proof-irrelevance"),
      Texts_cons(text_of_cstring("subsingleton-large-elimination"),
                 Texts_cons(text_of_cstring("literal-fast-path"), NULL)));
}

const Texts *rules_named_declared(void) { return named_rules(); }

const Texts *rules_named_present(void) { return named_rules(); }

const Telescope *rules_family_params(const Family *f) { return f->params; }

const Telescope *rules_family_indices(const Family *f) { return f->indices; }

Level rules_family_level(const Family *f) { return f->level; }

const Telescope *rules_ctor_args(const Ctor *c) { return c->args; }

const Terms *rules_ctor_indices(const Ctor *c) { return c->res_idx; }

/* pchoose is the if. */
int rules_mu_family_status(const Family *f, const Family *view,
                           const Family **out, const KError **error) {
  switch (view->status->tag) {
  case CTOR_PROVISIONAL:
  case CTOR_BUILTIN:
    *out = f;
    return 1;
  case CTOR_COMPLETE:
    if (view->positive) {
      *out = f;
      return 1;
    }
    *error = error_not_yet(positivity_nonpositive_word());
    return 0;
  }
  return 0; /* each status returns above */
}

int rules_mu_family(RulesCtx ctx, Text n, const Family **out,
                    const KError **error) {
  const Family *f = runtime_ops_family(ctx.check, n);
  Text parts[3];
  if (f != NULL) return rules_mu_family_status(f, f, out, error);
  parts[0] = text_of_cstring("the family ");
  parts[1] = n;
  parts[2] = text_of_cstring(" is not declared");
  *error = error_unbound(cat(parts, 3));
  return 0;
}

/* The recursion builds the list in order: a loop, then a reverse. */
const Terms *rules_leg_bodies(const TermLegs *legs) {
  const Terms *acc = NULL;
  for (; legs != NULL; legs = legs->tail)
    acc = Terms_cons(legs->head->body, acc);
  return Terms_reverse(acc);
}

int rules_mu_params_of(const Term *diagram, const Terms **out,
                       const KError **error) {
  const TermShape *shape;
  const TermLegs *legs;
  if (!rules_as_tsec(diagram, &shape, &legs)) {
    *error = error_mismatch(rules_diagram_msg());
    return 0;
  }
  *out = rules_leg_bodies(legs);
  return 1;
}

int rules_c_eval_env(RulesCtx ctx, const Values *env, const Term *tm,
                     const Value **out, const KError **error) {
  return runtime_ops_eval_env(ctx.check, env, tm, out, error);
}

/* The self tail call is a loop; each value goes onto env. */
int rules_mu_param_legs(RulesCtx ctx, const VLegs *legs, const Values *env,
                        const Values **out, const KError **error) {
  const Value *v;
  for (; legs != NULL; legs = legs->tail) {
    if (!rules_c_open_leg(ctx, legs->head, &v, error)) return 0;
    env = Values_cons(v, env);
  }
  *out = env;
  return 1;
}

int rules_mu_param_env(RulesCtx ctx, const Closure *d, const Values **out,
                       const KError **error) {
  const VLegs *legs;
  if (!rules_coll_legs_of(ctx, d, &legs, error)) return 0;
  return rules_mu_param_legs(ctx, legs, NULL, out, error);
}

/* The self tail call is a loop. The guard text (with two c_pp calls) is
   made only when the guard fails. */
int rules_mu_index_walk(const RulesHost *host, RulesCtx ctx, Text n,
                        Text ctor, const Terms *indices, const Values *wants,
                        const Values *env, const KError **error) {
  const Value *got;
  Bool eq;
  Text parts[8];
  for (; indices != NULL && wants != NULL;
       indices = indices->tail, wants = wants->tail) {
    if (!rules_c_eval_env(ctx, env, indices->head, &got, error)) return 0;
    if (!rules_c_conv_type(host, ctx, got, wants->head, &eq, error)) return 0;
    if (!eq) {
      parts[0] = text_of_cstring("the constructor ");
      parts[1] = ctor;
      parts[2] = text_of_cstring(" of ");
      parts[3] = n;
      parts[4] = text_of_cstring(" gives the index ");
      parts[5] = rules_c_pp(ctx, got);
      parts[6] = text_of_cstring(" and the type asks for ");
      parts[7] = rules_c_pp(ctx, wants->head);
      *error = error_mismatch(cat(parts, 8));
      return 0;
    }
  }
  if (indices == NULL && wants == NULL) return 1;
  *error = error_mismatch(rules_wrong_pack());
  return 0;
}

int rules_mu_indices(const RulesHost *host, RulesCtx ctx, Text n,
                     const Ctor *ct, const Values *ixv, const Values *env,
                     const KError **error) {
  Int63 gives = rules_advance(int63_zero(),
                              Terms_length(rules_ctor_indices(ct)));
  Int63 takes = rules_advance(int63_zero(), Values_length(ixv));
  Text parts[7];
  if (int63_equal(gives, takes))
    return rules_mu_index_walk(host, ctx, n, positivity_ctor_name(ct),
                               rules_ctor_indices(ct), ixv, env, error);
  parts[0] = positivity_ctor_name(ct);
  parts[1] = text_of_cstring(" gives ");
  parts[2] = int63_show(gives);
  parts[3] = text_of_cstring(" result indices and ");
  parts[4] = n;
  parts[5] = text_of_cstring(" takes ");
  parts[6] = int63_show(takes);
  *error = error_mismatch(cat(parts, 7));
  return 0;
}

int rules_mu_motive_view(Text n, Int63 want, Int63 applied,
                         const TermMotive *mo, const TermMotive *view,
                         const TermMotive **out, const KError **error) {
  Int63 bound = rules_advance(int63_zero(), Texts_length(view->idx));
  Text parts[6];
  if (!view->ind.some) {
    parts[0] = text_of_cstring("the motive of an elimination at ");
    parts[1] = n;
    parts[2] = text_of_cstring(" names no family");
    *error = error_mismatch(cat(parts, 3));
    return 0;
  }
  if (!text_same(view->ind.text, n)) {
    parts[0] = text_of_cstring("the motive is built for ");
    parts[1] = view->ind.text;
    parts[2] = text_of_cstring(" and the scrutinee is at ");
    parts[3] = n;
    *error = error_mismatch(cat(parts, 4));
    return 0;
  }
  if (!int63_equal(bound, want)) {
    parts[0] = text_of_cstring("the motive of ");
    parts[1] = n;
    parts[2] = text_of_cstring(" binds ");
    parts[3] = int63_show(bound);
    parts[4] = text_of_cstring(" indices and the family has ");
    parts[5] = int63_show(want);
    *error = error_mismatch(cat(parts, 6));
    return 0;
  }
  if (!int63_equal(applied, want)) {
    parts[0] = n;
    parts[1] = text_of_cstring(" is applied to ");
    parts[2] = int63_show(applied);
    parts[3] = text_of_cstring(" indices and the family has ");
    parts[4] = int63_show(want);
    *error = error_mismatch(cat(parts, 5));
    return 0;
  }
  *out = mo;
  return 1;
}

/* mo: NULL = None{}. */
int rules_mu_motive_of(Text n, const Family *fam, const Values *ixv,
                       const TermMotive *mo, const TermMotive **out,
                       const KError **error) {
  if (mo == NULL) {
    *error = error_cannot_infer(
        text_of_cstring("an elimination at a mu shape needs a motive"));
    return 0;
  }
  return rules_mu_motive_view(
      n,
      rules_advance(int63_zero(), Telescope_length(rules_family_indices(fam))),
      rules_advance(int63_zero(), Values_length(ixv)), mo, mo, out, error);
}

int rules_mu_result(RulesCtx ctx, const TermMotive *mo, const Values *idx,
                    const Value *self, const Value **out,
                    const KError **error) {
  return rules_c_eval_env(
      ctx,
      Values_cons(self, Values_reverse_onto(idx, runtime_ops_env(ctx.check))),
      mo->body, out, error);
}

/* The two V.var(c_size(ctx)) cells are one shared cell. */
RulesBindingState rules_bound_state(RulesCtx ctx, const Values *env,
                                    const Values *vals, Text name,
                                    Quantity q, const Value *ty) {
  RulesBindingState next;
  const Value *var = ops_var(ctx);
  next.context = rules_c_bind(ctx, name, q, ty);
  next.env = Values_cons(var, env);
  next.values = Values_cons(var, vals);
  return next;
}

/* The self tail call is a loop on state. */
int rules_mu_bind_indices(RulesCtx origin, const Telescope *tele,
                          const Texts *names, RulesBindingState state,
                          RulesBindingState *out, const KError **error) {
  const Value *tyv;
  for (; tele != NULL && names != NULL;
       tele = tele->tail, names = names->tail) {
    if (!rules_c_eval_env(origin, state.env, tele->head.binding.type, &tyv,
                          error))
      return 0;
    state = rules_bound_state(state.context, state.env, state.values,
                              names->head, QUANTITY_ZERO, tyv);
  }
  if (tele == NULL && names == NULL) {
    *out = state;
    return 1;
  }
  *error = error_mismatch(rules_wrong_pack());
  return 0;
}

int rules_mu_large(Text n, const Family *fam, Level mlvl,
                   const KError **error) {
  Text parts[2];
  if (!level_equal(rules_family_level(fam), level_zero()) ||
      level_equal(mlvl, level_zero()) || rules_zero_family_view(fam, fam))
    return 1;
  parts[0] = text_of_cstring("a large elimination out of a proposition "
                             "needs a subsingleton family at ");
  parts[1] = n;
  *error = error_universe(cat(parts, 2));
  return 0;
}

int rules_mu_ctor_names(Text n, const Family *fam, const Texts **out,
                        const KError **error) {
  Text parts[3];
  parts[0] = text_of_cstring("the family ");
  parts[1] = n;
  switch (fam->status->tag) {
  case CTOR_COMPLETE:
    *out = fam->status->as.complete.names;
    return 1;
  case CTOR_PROVISIONAL:
    parts[2] = text_of_cstring(" is still under declaration");
    *error = error_unbound(cat(parts, 3));
    return 0;
  case CTOR_BUILTIN:
    parts[2] = text_of_cstring(" is a kernel family and has no branches");
    *error = error_mismatch(cat(parts, 3));
    return 0;
  }
  return 0; /* each status returns above */
}

/* ============================================ Bend lines 1004-1376 (S2 s5) */

/* Instances of the generic defs of session 4, one per element type at the
   first caller: coll_width (817) at T.t, as_mu (912) at T.t and V.t, two_of
   (806) at F.Pair2<Q.t, String>. coll_width at V.t is
   rules_collection_view. */
int rules_coll_width_term(const TermShape *s, Int63 *out) {
  switch (s->tag) {
  case SHAPE_SPI: return 0;
  case SHAPE_SCOLL: *out = s->as.scoll.size; return 1;
  case SHAPE_SPAR: return 0;
  case SHAPE_SMU: return 0;
  case SHAPE_SNU: return 0;
  }
  return 0; /* each tag returns above */
}

int rules_as_mu_term(const TermShape *s, Text *name, const Terms **indices) {
  switch (s->tag) {
  case SHAPE_SPI: return 0;
  case SHAPE_SCOLL: return 0;
  case SHAPE_SPAR: return 0;
  case SHAPE_SMU:
    *name = s->as.smu.family;
    *indices = s->as.smu.indices;
    return 1;
  case SHAPE_SNU: return 0;
  }
  return 0; /* each tag returns above */
}

int rules_as_mu_value(const ValueShape *s, Text *name,
                      const Values **indices) {
  switch (s->tag) {
  case SHAPE_SPI: return 0;
  case SHAPE_SCOLL: return 0;
  case SHAPE_SPAR: return 0;
  case SHAPE_SMU:
    *name = s->as.smu.family;
    *indices = s->as.smu.indices;
    return 1;
  case SHAPE_SNU: return 0;
  }
  return 0; /* each tag returns above */
}

int rules_two_of_binders(const TermBinders *xs, TermBinder *x, TermBinder *y) {
  if (xs == NULL || xs->tail == NULL || xs->tail->tail != NULL) return 0;
  *x = xs->head;
  *y = xs->tail->head;
  return 1;
}

/* one_of (Bend line 795) at T.t and at F.Pair2<Q.t, String>. */
const Term *rules_one_of_term(const Terms *xs) {
  if (xs == NULL || xs->tail != NULL) return NULL;
  return xs->head;
}

const TermBinder *rules_one_of_binder(const TermBinders *xs) {
  if (xs == NULL || xs->tail != NULL) return NULL;
  return &xs->head;
}

/* The non-tail recursion is a loop, then Texts_reverse (the same order). */
int rules_mu_keys(Text n, const TermBranches *branches, const Texts **out,
                  const KError **error) {
  const Texts *keys = NULL;
  Text parts[3];
  for (; branches != NULL; branches = branches->tail) {
    MaybeText key = term_as_actor(branches->head.address);
    if (!key.some) {
      parts[0] = text_of_cstring("a branch of ");
      parts[1] = n;
      parts[2] = text_of_cstring(" takes the constructor address");
      *error = error_wrong_leg(cat(parts, 3));
      return 0;
    }
    keys = Texts_cons(key.text, keys);
  }
  *out = Texts_reverse(keys);
  return 1;
}

/* A loop; the add of a zero is skipped (the same sum). */
Int63 rules_mu_count(Text key, const Texts *names) {
  Int63 count = int63_zero();
  for (; names != NULL; names = names->tail)
    if (text_same(key, names->head)) count = int63_add(count, int63_one());
  return count;
}

int rules_mu_cover_names(Text n, const Texts *names, const Texts *keys,
                         const KError **error) {
  Text parts[4];
  Int63 count;
  for (; names != NULL; names = names->tail) {
    count = rules_mu_count(names->head, keys);
    if (int63_equal(count, int63_one())) continue;
    parts[0] = text_of_cstring("the elimination of ");
    parts[1] = n;
    parts[3] = names->head;
    if (int63_equal(count, int63_zero())) {
      parts[2] = text_of_cstring(" has no branch at ");
      *error = error_missing_branch(cat(parts, 4));
    } else {
      parts[2] = text_of_cstring(" repeats the branch at ");
      *error = error_wrong_leg(cat(parts, 4));
    }
    return 0;
  }
  return 1;
}

int rules_mu_cover_keys(Text n, const Texts *names, const Texts *keys,
                        const KError **error) {
  Text parts[3];
  for (; keys != NULL; keys = keys->tail) {
    if (term_member(keys->head, names)) continue;
    parts[0] = keys->head;
    parts[1] = text_of_cstring(" is not a constructor of ");
    parts[2] = n;
    *error = error_unbound(cat(parts, 3));
    return 0;
  }
  return 1;
}

int rules_mu_cover(Text n, const Texts *names, const TermBranches *branches,
                   const KError **error) {
  const Texts *keys;
  if (!rules_mu_keys(n, branches, &keys, error)) return 0;
  if (!rules_mu_cover_names(n, names, keys, error)) return 0;
  return rules_mu_cover_keys(n, names, keys, error);
}

/* The self tail call is a loop on state (as mu_bind_indices). */
int rules_mu_bind_fields(RulesCtx origin, const Telescope *tele,
                         const TermBinders *binders, RulesBindingState state,
                         RulesBindingState *out, const KError **error) {
  const Value *tyv;
  for (; tele != NULL && binders != NULL;
       tele = tele->tail, binders = binders->tail) {
    if (!rules_c_eval_env(origin, state.env, tele->head.binding.type, &tyv,
                          error))
      return 0;
    if (!rules_binder_quantity(text_of_cstring("branch binder"),
                               text_of_cstring("field"), binders->head.name,
                               binders->head.quantity, tele->head.quantity,
                               error))
      return 0;
    state = rules_bound_state(state.context, state.env, state.values,
                              binders->head.name, tele->head.quantity, tyv);
  }
  if (tele == NULL && binders == NULL) {
    *out = state;
    return 1;
  }
  *error = error_mismatch(rules_wrong_pack());
  return 0;
}

/* The non-tail recursion is a loop, then Values_reverse (the same order). */
int rules_mu_values(RulesCtx ctx, const Values *env, const Terms *terms,
                    const Values **out, const KError **error) {
  const Values *vals = NULL;
  const Value *v;
  for (; terms != NULL; terms = terms->tail) {
    if (!rules_c_eval_env(ctx, env, terms->head, &v, error)) return 0;
    vals = Values_cons(v, vals);
  }
  *out = Values_reverse(vals);
  return 1;
}

/* NULL = None{}. */
const Level *rules_annotation(const Value *value) {
  switch (value->tag) {
  case VALUE_VUNIV: return NULL;
  case VALUE_VLAN: return value->as.vlan.universe;
  case VALUE_VRAN: return value->as.vran.universe;
  case VALUE_VIN: return NULL;
  case VALUE_VSEC: return NULL;
  case VALUE_VLIT: return NULL;
  case VALUE_VNEUTRAL: return NULL;
  }
  return NULL; /* each tag returns above */
}

/* The Fail{e} arm is the 0 return of rules_request. */
int rules_level_answer(const RulesAnswer *answer, Level *out,
                       const KError **error) {
  switch (answer->tag) {
  case RULES_LEVEL_ANSWER:
    *out = answer->as.level;
    return 1;
  case RULES_VALUE_ANSWER:
  case RULES_TERM_ANSWER:
  case RULES_BOOL_ANSWER:
  case RULES_INFERRED_ANSWER:
  case RULES_USAGE_ANSWER:
    break;
  }
  *error = error_mismatch(text_of_cstring("internal universe answer"));
  return 0;
}

int rules_ask_univ(const RulesHost *host, RulesCtx ctx, const Term *tm,
                   Level *out, const KError **error) {
  RulesTask task;
  RulesAnswer answer;
  task.tag = RULES_INFER_UNIVERSE;
  task.context = ctx;
  task.as.infer_universe.term = tm;
  if (!rules_request(host, &task, &answer, error)) return 0;
  return rules_level_answer(&answer, out, error);
}

int rules_c_infer(const RulesHost *host, RulesCtx ctx, Quantity q,
                  const Term *tm, RulesInferred *out, const KError **error) {
  RulesTask task;
  RulesAnswer answer;
  task.tag = RULES_INFER_USES;
  task.context = ctx;
  task.as.infer_uses.mode = q;
  task.as.infer_uses.term = tm;
  if (!rules_request(host, &task, &answer, error)) return 0;
  return rules_inferred_answer(&answer, out, error);
}

int rules_c_check(const RulesHost *host, RulesCtx ctx, Quantity q,
                  const Term *tm, const Value *ty, QuantityUsage *out,
                  const KError **error) {
  RulesTask task;
  RulesAnswer answer;
  task.tag = RULES_CHECK_USES;
  task.context = ctx;
  task.as.check_uses.mode = q;
  task.as.check_uses.term = tm;
  task.as.check_uses.expected = ty;
  if (!rules_request(host, &task, &answer, error)) return 0;
  return rules_usage_answer(&answer, out, error);
}

int rules_c_univ(const RulesHost *host, RulesCtx ctx, const Term *tm,
                 Level *out, const KError **error) {
  return rules_ask_univ(host, ctx, tm, out, error);
}

/* next is the caller: w and the former come back in out parameters
   (former_apply only takes the record apart). */
int rules_with_former(RulesCtx ctx, Bool right, Text message, const Value *ty,
                      const Value **w, ValueFormer *former,
                      const KError **error) {
  int found;
  if (!runtime_ops_whnf(ctx.check, ty, w, error)) return 0;
  if (right)
    found = value_as_ran(*w, former);
  else
    found = value_as_lan(*w, former);
  if (found) return 1;
  *error = error_mismatch(message);
  return 0;
}

int rules_infer_scrutinee(const RulesHost *host, RulesCtx ctx, Quantity mode,
                          Quantity q, const Term *scrut, RulesInferred *out,
                          const KError **error) {
  if (!quantity_equal(mode, QUANTITY_ZERO) &&
      quantity_equal(q, QUANTITY_ZERO)) {
    *error = error_quantity(text_of_cstring(
        "an erased scrutinee cannot be eliminated at runtime"));
    return 0;
  }
  return rules_c_infer(host, ctx, quantity_mul(mode, q), scrut, out, error);
}

/* mo NULL = None{}. */
int rules_check_motive(const RulesHost *host, RulesCtx ctx,
                       const TermMotive *mo, const Value *ty,
                       const KError **error) {
  Level level;
  if (mo == NULL) return 1;
  return rules_c_univ(host, rules_c_bind(ctx, mo->self, QUANTITY_ZERO, ty),
                      mo->body, &level, error);
}

int rules_form_pi(const RulesHost *host, RulesCtx ctx, const TermShape *shape,
                  const Term *diagram, Bool right, Level *out,
                  const KError **error) {
  Level a;
  Level b;
  const Value *dv;
  switch (shape->tag) {
  case SHAPE_SPI:
    if (!rules_c_univ(host, ctx, shape->as.spi.domain, &a, error)) return 0;
    if (!rules_c_eval(ctx, shape->as.spi.domain, &dv, error)) return 0;
    if (!rules_c_univ(host,
                      rules_c_bind(ctx, shape->as.spi.binder,
                                   shape->as.spi.quantity, dv),
                      diagram, &b, error))
      return 0;
    if (right && level_equal(b, level_zero()))
      *out = level_zero();
    else
      *out = level_max(a, b);
    return 1;
  case SHAPE_SCOLL:
  case SHAPE_SPAR:
  case SHAPE_SMU:
  case SHAPE_SNU:
    break;
  }
  *error = error_mismatch(rules_wrong_pack());
  return 0;
}

int rules_pi_sec_body(const RulesHost *host, RulesCtx ctx, RulesCtx inner,
                      Quantity mode, const Term *body, const Value *cod,
                      QuantityUsage *out, const KError **error) {
  QuantityUsage uses;
  QuantityUsage free;
  if (!rules_c_check(host, inner, quantity_runtime(mode), body, cod, &uses,
                     error))
    return 0;
  if (!rules_c_close(inner, rules_c_size(ctx), mode, uses, &free, error))
    return 0;
  *out = quantity_scale(mode, quantity_captures(free));
  return 1;
}

int rules_pi_intro_in(const RulesHost *host, RulesCtx ctx, Quantity mode,
                      const TermAddr *addr, const Terms *args,
                      const Value *expected, QuantityUsage *out,
                      const KError **error) {
  const Value *w;
  ValueFormer former;
  RulesPoint point;
  TermPoint a;
  const Term *fibre;
  QuantityUsage pu;
  QuantityUsage fu;
  const Value *cod;
  if (!rules_with_former(
          ctx, NO,
          text_of_cstring("a pair needs a left former as its expected type"),
          expected, &w, &former, error))
    return 0;
  if (!rules_with_point(former.shape,
                        text_of_cstring("a pair needs a point former"), &point,
                        error))
    return 0;
  if (!term_as_apt(addr, &a)) {
    *error = error_wrong_leg(text_of_cstring("a pair takes the point address"));
    return 0;
  }
  fibre = rules_one_of_term(args);
  if (fibre == NULL) {
    *error =
        error_mismatch(text_of_cstring("a pair carries one fibre element"));
    return 0;
  }
  if (!rules_c_check(host, ctx, quantity_mul(mode, point.quantity), a.arg,
                     point.domain, &pu, error))
    return 0;
  if (!rules_c_open_at(ctx, former.diagram, a.arg, &cod, error)) return 0;
  if (!rules_c_check(host, ctx, mode, fibre, cod, &fu, error)) return 0;
  *out = quantity_sequence(pu, fu);
  return 1;
}

int rules_pi_elim_out(const RulesHost *host, RulesCtx ctx, Quantity mode,
                      const TermAddr *addr, const Term *head,
                      RulesInferred *out, const KError **error) {
  Text message = text_of_cstring("the head of an application is not a "
                                 "function");
  TermPoint a;
  RulesInferred inf;
  const Value *w;
  ValueFormer former;
  RulesPoint point;
  QuantityUsage pu;
  if (!term_as_apt(addr, &a)) {
    *error = error_wrong_leg(
        text_of_cstring("an application takes the point address"));
    return 0;
  }
  if (!rules_c_infer(host, ctx, mode, head, &inf, error)) return 0;
  if (!rules_with_former(ctx, YES, message, inf.ty, &w, &former, error))
    return 0;
  if (!rules_with_point(former.shape, message, &point, error)) return 0;
  if (!rules_c_check(host, ctx, quantity_mul(mode, point.quantity), a.arg,
                     point.domain, &pu, error))
    return 0;
  if (!rules_c_open_at(ctx, former.diagram, a.arg, &out->ty, error))
    return 0;
  out->uses = quantity_sequence(inf.uses, pu);
  return 1;
}

/* mo and expected: NULL = None{}. */
int rules_pi_branch_body(const RulesHost *host, RulesCtx ctx, RulesCtx inner,
                         Quantity mode, const Term *body, const TermMotive *mo,
                         const Value *expected, const Value *self,
                         QuantityUsage *out, const KError **error) {
  const Value *target;
  QuantityUsage uses;
  if (!rules_elim_result(ctx, mo, expected, self, &target, error)) return 0;
  if (!rules_c_check(host, inner, quantity_runtime(mode), body, target, &uses,
                     error))
    return 0;
  return rules_c_close(inner, rules_c_size(ctx), mode, uses, out, error);
}

/* The non-tail recursion stays a recursion (depth = the leg count): the
   c_univ requests run in leg order and L.max folds from the right. */
int rules_coll_levels(const RulesHost *host, RulesCtx ctx,
                      const TermLegs *legs, Level *out, const KError **error) {
  Level level;
  Level tail;
  if (legs == NULL) {
    *out = level_zero();
    return 1;
  }
  if (!rules_c_univ(host, ctx, legs->head->body, &level, error)) return 0;
  if (!rules_coll_levels(host, ctx, legs->tail, &tail, error)) return 0;
  *out = level_max(level, tail);
  return 1;
}

/* expected NULL = None{}. The three diagram_msg requires are one test. */
int rules_form_coll(const RulesHost *host, RulesCtx ctx,
                    const TermShape *shape, const Term *diagram,
                    const Level *expected, Level *out, const KError **error) {
  Int63 n;
  Int63 dn;
  const TermShape *ds;
  const TermLegs *legs;
  Level l;
  if (!rules_coll_width_term(shape, &n)) {
    *error = error_mismatch(rules_wrong_pack());
    return 0;
  }
  if (!rules_as_tsec(diagram, &ds, &legs) || !rules_coll_width_term(ds, &dn) ||
      !(int63_equal(dn, n) &&
        int63_equal(rules_advance(int63_zero(), TermLegs_length(legs)), n))) {
    *error = error_mismatch(rules_diagram_msg());
    return 0;
  }
  if (!rules_diagram_unbound(legs)) {
    *error = error_mismatch(text_of_cstring("a diagram leg binds nothing"));
    return 0;
  }
  if (!rules_coll_levels(host, ctx, legs, &l, error)) return 0;
  if (int63_equal(n, int63_zero()))
    *out = rules_level_default(expected);
  else
    *out = l;
  return 1;
}

/* The non-tail recursion stays a recursion (Q.sequence folds from the
   right; the width mismatch aborts after the checked legs). */
int rules_coll_tuple(const RulesHost *host, RulesCtx ctx, Quantity mode,
                     const TermLegs *legs, const VLegs *dlegs,
                     QuantityUsage *out, const KError **error) {
  const Value *ty;
  QuantityUsage uses;
  QuantityUsage more;
  if (legs == NULL && dlegs == NULL) {
    *out = quantity_empty();
    return 1;
  }
  if (legs == NULL || dlegs == NULL) {
    *error = error_mismatch(rules_diagram_msg());
    return 0;
  }
  if (!rules_c_open_leg(ctx, dlegs->head, &ty, error)) return 0;
  if (!rules_c_check(host, ctx, mode, legs->head->body, ty, &uses, error))
    return 0;
  if (!rules_coll_tuple(host, ctx, mode, legs->tail, dlegs->tail, &more,
                        error))
    return 0;
  *out = quantity_sequence(uses, more);
  return 1;
}

int rules_coll_intro_sec(const RulesHost *host, RulesCtx ctx, Quantity mode,
                         const TermShape *s, const TermLegs *legs,
                         const Value *expected, QuantityUsage *out,
                         const KError **error) {
  Int63 n;
  Int63 dn;
  const Value *w;
  ValueFormer former;
  const VLegs *dlegs;
  if (!rules_coll_width_term(s, &n)) {
    *error = error_mismatch(rules_wrong_pack());
    return 0;
  }
  if (!rules_with_former(
          ctx, YES,
          text_of_cstring("a tuple needs a right former as its expected type"),
          expected, &w, &former, error))
    return 0;
  if (!rules_collection_view(former.shape, &dn)) {
    *error = error_mismatch(rules_wrong_pack());
    return 0;
  }
  if (!(int63_equal(dn, n) &&
        int63_equal(rules_advance(int63_zero(), TermLegs_length(legs)), n))) {
    *error = error_mismatch(
        text_of_cstring("the tuple width and the type width differ"));
    return 0;
  }
  if (!rules_coll_legs_of(ctx, former.diagram, &dlegs, error)) return 0;
  return rules_coll_tuple(host, ctx, mode, legs, dlegs, out, error);
}

int rules_coll_intro_in(const RulesHost *host, RulesCtx ctx, Quantity mode,
                        const TermAddr *addr, const Terms *args,
                        const Value *expected, QuantityUsage *out,
                        const KError **error) {
  const Value *w;
  ValueFormer former;
  Int63 n;
  Int63 k;
  const VLegs *dlegs;
  const Value *ty;
  const Term *payload;
  if (!rules_with_former(ctx, NO,
                         text_of_cstring("an injection needs a left former "
                                         "as its expected type"),
                         expected, &w, &former, error))
    return 0;
  if (!rules_collection_view(former.shape, &n)) {
    *error = error_mismatch(rules_wrong_pack());
    return 0;
  }
  if (!term_as_aleg(addr, &k)) {
    *error =
        error_wrong_leg(text_of_cstring("an injection takes the leg address"));
    return 0;
  }
  if (!rules_leg_bound(k, n, error)) return 0;
  if (!rules_coll_legs_of(ctx, former.diagram, &dlegs, error)) return 0;
  if (!rules_coll_leg_ty(ctx, dlegs, k, &ty, error)) return 0;
  payload = rules_one_of_term(args);
  if (payload == NULL) {
    *error =
        error_mismatch(text_of_cstring("an injection carries one payload"));
    return 0;
  }
  return rules_c_check(host, ctx, mode, payload, ty, out, error);
}

int rules_coll_elim_out(const RulesHost *host, RulesCtx ctx, Quantity mode,
                        const TermAddr *addr, const Term *head,
                        RulesInferred *out, const KError **error) {
  Int63 k;
  Int63 n;
  RulesInferred inf;
  const Value *w;
  ValueFormer former;
  const VLegs *dlegs;
  if (!term_as_aleg(addr, &k)) {
    *error =
        error_wrong_leg(text_of_cstring("a projection takes the leg address"));
    return 0;
  }
  if (!rules_c_infer(host, ctx, mode, head, &inf, error)) return 0;
  if (!rules_with_former(ctx, YES,
                         text_of_cstring("a projection needs a right former"),
                         inf.ty, &w, &former, error))
    return 0;
  if (!rules_collection_view(former.shape, &n)) {
    *error = error_mismatch(rules_wrong_pack());
    return 0;
  }
  if (!rules_leg_bound(k, n, error)) return 0;
  if (!rules_coll_legs_of(ctx, former.diagram, &dlegs, error)) return 0;
  if (!rules_coll_leg_ty(ctx, dlegs, k, &out->ty, error)) return 0;
  out->uses = inf.uses;
  return 1;
}

/* mo and expected: NULL = None{}. The two V.VALeg{k} cells are one cell. */
int rules_coll_branch(const RulesHost *host, RulesCtx ctx, Quantity mode,
                      const ValueShape *vs, const VLegs *dlegs,
                      const TermMotive *mo, const Value *expected,
                      const TermBranches *branches, Int63 k,
                      QuantityUsage *out, const KError **error) {
  const VAddr *key = value_valeg(k);
  const TermLeg *lg = rules_branch_leg(key, branches);
  const TermBinder *b;
  const Value *ty;
  Text parts[2];
  if (lg == NULL) {
    parts[0] = text_of_cstring("no branch at leg ");
    parts[1] = int63_show(k);
    *error = error_missing_branch(cat(parts, 2));
    return 0;
  }
  b = rules_one_of_binder(lg->binders);
  if (b == NULL) {
    *error = error_missing_branch(
        text_of_cstring("each branch binds its payload once"));
    return 0;
  }
  if (!rules_coll_leg_ty(ctx, dlegs, k, &ty, error)) return 0;
  return rules_pi_branch_body(
      host, ctx, rules_c_bind(ctx, b->name, b->quantity, ty), mode, lg->body,
      mo, expected,
      value_vin(vs, key, Values_cons(value_var(rules_c_size(ctx)), NULL)), out,
      error);
}

/* The non-tail recursion stays a recursion (Q.alternative folds from the
   right). steps only counts the legs. */
int rules_coll_branches(const RulesHost *host, RulesCtx ctx, Quantity mode,
                        const ValueShape *vs, const VLegs *dlegs,
                        const TermMotive *mo, const Value *expected,
                        const TermBranches *branches,
                        const TermBranches *steps, Int63 k,
                        QuantityUsage *out, const KError **error) {
  QuantityUsage uses;
  QuantityUsage more;
  if (steps == NULL) {
    *out = quantity_unreachable();
    return 1;
  }
  if (!rules_coll_branch(host, ctx, mode, vs, dlegs, mo, expected, branches, k,
                         &uses, error))
    return 0;
  if (!rules_coll_branches(host, ctx, mode, vs, dlegs, mo, expected, branches,
                           steps->tail, int63_succ(k), &more, error))
    return 0;
  *out = quantity_alternative(uses, more);
  return 1;
}

/* ============================================ Bend lines 1195-1376 (S2 s5b) */

/* e = T.Elimination{_, scrut, sq, mo, branches}. expected: NULL = None{}.
   The guard text is made only on failure. */
int rules_coll_elim_elim(const RulesHost *host, RulesCtx ctx, Quantity mode,
                         const TermElim *e, const Value *expected,
                         RulesInferred *out, const KError **error) {
  RulesInferred inf;
  const Value *w;
  ValueFormer former;
  Int63 n;
  Int63 count;
  const Value *sv;
  const Value *ty;
  const VLegs *dlegs;
  QuantityUsage uses;
  Text parts[5];
  if (!rules_infer_scrutinee(host, ctx, mode, e->scrut_q, e->scrut, &inf,
                             error))
    return 0;
  if (!rules_with_former(
          ctx, NO, text_of_cstring("the scrutinee is not a left former"),
          inf.ty, &w, &former, error))
    return 0;
  if (!rules_collection_view(former.shape, &n)) {
    *error = error_mismatch(rules_wrong_pack());
    return 0;
  }
  if (!rules_check_motive(host, ctx, e->motive, w, error)) return 0;
  if (!rules_c_eval(ctx, e->scrut, &sv, error)) return 0;
  if (!rules_elim_result(ctx, e->motive, expected, sv, &ty, error)) return 0;
  count = rules_advance(int63_zero(), TermBranches_length(e->branches));
  if (!int63_equal(count, n)) {
    parts[0] = text_of_cstring("the case has ");
    parts[1] = int63_show(count);
    parts[2] = text_of_cstring(" branches and the type has ");
    parts[3] = int63_show(n);
    parts[4] = text_of_cstring(" legs");
    *error = error_missing_branch(cat(parts, 5));
    return 0;
  }
  if (!rules_coll_legs_of(ctx, former.diagram, &dlegs, error)) return 0;
  if (!rules_coll_branches(host, ctx, mode, former.shape, dlegs, e->motive,
                           expected, e->branches, e->branches, int63_zero(),
                           &uses, error))
    return 0;
  out->ty = ty;
  out->uses = quantity_sequence(inf.uses, quantity_scale(mode, uses));
  return 1;
}

/* MuPack{}: Abort{E.Not_yet{mu_ran_word}}. */
int rules_elim_out_pack(const RulesHost *host, RulesCtx ctx, Quantity mode,
                        RulesPack p, const TermAddr *addr, const Term *head,
                        RulesInferred *out, const KError **error) {
  switch (p) {
  case RULES_PI_PACK:
    return rules_pi_elim_out(host, ctx, mode, addr, head, out, error);
  case RULES_COLL_PACK:
    return rules_coll_elim_out(host, ctx, mode, addr, head, out, error);
  case RULES_MU_PACK:
    *error = error_not_yet(rules_mu_ran_word());
    return 0;
  }
  return 0;
}

int rules_elim_out(const RulesHost *host, RulesCtx ctx, Quantity mode,
                   const TermShape *s, const TermAddr *addr, const Term *head,
                   RulesInferred *out, const KError **error) {
  RulesPack p;
  if (!rules_rules(s->tag, &p, error)) return 0;
  return rules_elim_out_pack(host, ctx, mode, p, addr, head, out, error);
}

/* tele_reads (S14): YES when a binder type of tele has a T.Var. */
static Bool rules_tele_reads(const Telescope *tele) {
  for (; tele != NULL; tele = tele->tail)
    if (term_exists_var(tele->head.binding.type)) return YES;
  return NO;
}

/* The pair comes back in two out parameters. The self tail call is a loop.
   The two length mismatch arms are one test after the loop. env_out NULL
   (S14) = the caller does not read the environment. Then an argument that
   no later binder type reads is not evaluated and not put in env (as
   rules_c_open_at). */
int rules_tele_walk(const RulesHost *host, RulesCtx ctx, Quantity mode,
                    const Telescope *tele, const Terms *args,
                    const Values *env, QuantityUsage uses,
                    const Values **env_out, QuantityUsage *uses_out,
                    const KError **error) {
  const Value *tyv;
  QuantityUsage au;
  const Value *v;
  while (tele != NULL && args != NULL) {
    if (!rules_c_eval_env(ctx, env, tele->head.binding.type, &tyv, error))
      return 0;
    if (!rules_c_check(host, ctx, quantity_mul(mode, tele->head.quantity),
                       args->head, tyv, &au, error))
      return 0;
    if (env_out != NULL || rules_tele_reads(tele->tail)) {
      if (!rules_c_eval(ctx, args->head, &v, error)) return 0;
      env = Values_cons(v, env);
    }
    uses = quantity_sequence(uses, au);
    tele = tele->tail;
    args = args->tail;
  }
  if (tele != NULL || args != NULL) {
    *error = error_mismatch(rules_wrong_pack());
    return 0;
  }
  if (env_out != NULL) *env_out = env;
  *uses_out = uses;
  return 1;
}

/* The guard text is made only on failure. */
int rules_mu_telescope_uses(const RulesHost *host, RulesCtx ctx, Quantity mode,
                            Text what, const Telescope *tele,
                            const Terms *args, const Values *env,
                            const Values **env_out, QuantityUsage *uses_out,
                            const KError **error) {
  Int63 takes = rules_advance(int63_zero(), Telescope_length(tele));
  Int63 gives = rules_advance(int63_zero(), Terms_length(args));
  Text parts[5];
  if (!int63_equal(takes, gives)) {
    parts[0] = what;
    parts[1] = text_of_cstring(" takes ");
    parts[2] = int63_show(takes);
    parts[3] = text_of_cstring(" arguments and the term gives ");
    parts[4] = int63_show(gives);
    *error = error_mismatch(cat(parts, 5));
    return 0;
  }
  return rules_tele_walk(host, ctx, mode, tele, args, env, quantity_empty(),
                         env_out, uses_out, error);
}

/* split only drops the usage. */
int rules_mu_telescope(const RulesHost *host, RulesCtx ctx, Quantity mode,
                       Text what, const Telescope *tele, const Terms *args,
                       const Values *env, const Values **out,
                       const KError **error) {
  QuantityUsage uses;
  return rules_mu_telescope_uses(host, ctx, mode, what, tele, args, env, out,
                                 &uses, error);
}

int rules_form_mu(const RulesHost *host, RulesCtx ctx, const TermShape *s,
                  const Term *diagram, Level *out, const KError **error) {
  Text n;
  const Terms *ix;
  const Family *fam;
  const Terms *ps;
  const Values *penv;
  if (!rules_as_mu_term(s, &n, &ix)) {
    *error = error_mismatch(rules_wrong_pack());
    return 0;
  }
  if (!rules_mu_family(ctx, n, &fam, error)) return 0;
  if (!rules_mu_params_of(diagram, &ps, error)) return 0;
  if (!rules_mu_telescope(host, ctx, QUANTITY_ZERO, n, rules_family_params(fam),
                          ps, NULL, &penv, error))
    return 0;
  if (!rules_mu_telescope(host, ctx, QUANTITY_ZERO,
                          text_append(text_of_cstring("the indices of "), n),
                          rules_family_indices(fam), ix, penv, NULL, error))
    return 0;
  *out = rules_family_level(fam);
  return 1;
}

/* The unbound text is made only on failure. */
int rules_mu_intro_in(const RulesHost *host, RulesCtx ctx, Quantity mode,
                      const TermAddr *addr, const Terms *args,
                      const Value *expected, QuantityUsage *out,
                      const KError **error) {
  const Value *w;
  ValueFormer former;
  Text n;
  const Values *ixv;
  MaybeText c;
  const Family *fam;
  const Ctor *ct;
  const Values *penv;
  const Values *env;
  QuantityUsage uses;
  Text parts[3];
  if (!rules_with_former(
          ctx, NO, text_of_cstring("a constructor needs a left former"),
          expected, &w, &former, error))
    return 0;
  if (!rules_as_mu_value(former.shape, &n, &ixv)) {
    *error = error_mismatch(rules_wrong_pack());
    return 0;
  }
  c = term_as_actor(addr);
  if (!c.some) {
    *error = error_wrong_leg(
        text_of_cstring("a constructor takes a constructor address"));
    return 0;
  }
  if (!rules_mu_family(ctx, n, &fam, error)) return 0;
  ct = positivity_ctor_of(c.text, fam);
  if (ct == NULL) {
    parts[0] = c.text;
    parts[1] = text_of_cstring(" is not a constructor of ");
    parts[2] = n;
    *error = error_unbound(cat(parts, 3));
    return 0;
  }
  if (!rules_mu_param_env(ctx, former.diagram, &penv, error)) return 0;
  if (!rules_mu_telescope_uses(host, ctx, mode, c.text, rules_ctor_args(ct),
                               args, penv, &env, &uses, error))
    return 0;
  if (!rules_mu_indices(host, ctx, n, ct, ixv, env, error)) return 0;
  *out = uses;
  return 1;
}

/* u: NULL = None{}. rev_append(vals, []) is Values_reverse. */
int rules_mu_motive_body(const RulesHost *host, Text n, Text self,
                         const Term *body, const Closure *d, const Level *u,
                         RulesBindingState state, Level *out,
                         const KError **error) {
  return rules_c_univ(
      host,
      rules_c_bind(state.context, self, QUANTITY_ZERO,
                   value_vlan(shape_smu_value(n, Values_reverse(state.values)),
                              d, u)),
      body, out, error);
}

/* The guard text is made only on failure. */
int rules_mu_motive_lvl(const RulesHost *host, RulesCtx ctx, Text n,
                        const Family *fam, const TermMotive *mo,
                        const Closure *d, const Level *u, const Values *penv,
                        Level *out, const KError **error) {
  Int63 bound =
      rules_advance(int63_zero(), Telescope_length(rules_family_indices(fam)));
  Int63 binds = rules_advance(int63_zero(), Texts_length(mo->idx));
  RulesBindingState start;
  RulesBindingState state;
  Text parts[3];
  if (!int63_equal(bound, binds)) {
    parts[0] = text_of_cstring("the motive of ");
    parts[1] = n;
    parts[2] = text_of_cstring(" does not bind the indices");
    *error = error_mismatch(cat(parts, 3));
    return 0;
  }
  start.context = ctx;
  start.env = penv;
  start.values = NULL;
  if (!rules_mu_bind_indices(ctx, rules_family_indices(fam), mo->idx, start,
                             &state, error))
    return 0;
  return rules_mu_motive_body(host, n, mo->self, mo->body, d, u, state, out,
                              error);
}

int rules_mu_branch_body(const RulesHost *host, RulesCtx ctx, Quantity mode,
                         Text n, Text ctor, const TermMotive *mo,
                         const Terms *indices, const Term *body,
                         RulesBindingState state, QuantityUsage *out,
                         const KError **error) {
  const Values *idx;
  const Value *target;
  QuantityUsage uses;
  if (!rules_mu_values(ctx, state.env, indices, &idx, error)) return 0;
  if (!rules_mu_result(ctx, mo, idx,
                       value_vin(shape_smu_value(n, idx), value_vactor(ctor),
                                 Values_reverse(state.values)),
                       &target, error))
    return 0;
  if (!rules_c_check(host, state.context, quantity_runtime(mode), body, target,
                     &uses, error))
    return 0;
  return rules_c_close(state.context, rules_c_size(ctx), mode, uses, out,
                       error);
}

/* The unbound, missing branch and count texts are made only on failure. */
int rules_mu_branch(const RulesHost *host, RulesCtx ctx, Quantity mode, Text n,
                    const Family *fam, const TermMotive *mo,
                    const Values *penv, const TermBranches *branches, Text c,
                    QuantityUsage *out, const KError **error) {
  const Ctor *ct = positivity_ctor_of(c, fam);
  const TermLeg *lg;
  Int63 binds;
  Int63 takes;
  RulesBindingState start;
  RulesBindingState state;
  Text parts[8];
  if (ct == NULL) {
    parts[0] = c;
    parts[1] = text_of_cstring(" is not a constructor of ");
    parts[2] = n;
    *error = error_unbound(cat(parts, 3));
    return 0;
  }
  lg = rules_branch_leg(value_vactor(c), branches);
  if (lg == NULL) {
    parts[0] = text_of_cstring("the elimination of ");
    parts[1] = n;
    parts[2] = text_of_cstring(" has no branch at ");
    parts[3] = c;
    *error = error_missing_branch(cat(parts, 4));
    return 0;
  }
  binds = rules_advance(int63_zero(), TermBinders_length(lg->binders));
  takes = rules_advance(int63_zero(), Telescope_length(rules_ctor_args(ct)));
  if (!int63_equal(binds, takes)) {
    parts[0] = text_of_cstring("the branch at ");
    parts[1] = c;
    parts[2] = text_of_cstring(" binds ");
    parts[3] = int63_show(binds);
    parts[4] = text_of_cstring(" fields and ");
    parts[5] = c;
    parts[6] = text_of_cstring(" takes ");
    parts[7] = int63_show(takes);
    *error = error_missing_branch(cat(parts, 8));
    return 0;
  }
  start.context = ctx;
  start.env = penv;
  start.values = NULL;
  if (!rules_mu_bind_fields(ctx, rules_ctor_args(ct), lg->binders, start,
                            &state, error))
    return 0;
  return rules_mu_branch_body(host, ctx, mode, n, c, mo, rules_ctor_indices(ct),
                              lg->body, state, out, error);
}

/* The non-tail recursion stays a recursion (Q.alternative folds from the
   right). */
int rules_mu_branches(const RulesHost *host, RulesCtx ctx, Quantity mode,
                      Text n, const Family *fam, const TermMotive *mo,
                      const Values *penv, const TermBranches *branches,
                      const Texts *names, QuantityUsage *out,
                      const KError **error) {
  QuantityUsage uses;
  QuantityUsage more;
  if (names == NULL) {
    *out = quantity_unreachable();
    return 1;
  }
  if (!rules_mu_branch(host, ctx, mode, n, fam, mo, penv, branches,
                       names->head, &uses, error))
    return 0;
  if (!rules_mu_branches(host, ctx, mode, n, fam, mo, penv, branches,
                         names->tail, &more, error))
    return 0;
  *out = quantity_alternative(uses, more);
  return 1;
}

/* one_of (session 3) at T.leg. */
const TermLeg *rules_one_of_leg(const TermLegs *xs) {
  if (xs == NULL || xs->tail != NULL) return NULL;
  return xs->head;
}

/* e = T.Elimination{_, scrut, sq, mo0, branches}. */
int rules_mu_elim_elim(const RulesHost *host, RulesCtx ctx, Quantity mode,
                       const TermElim *e, RulesInferred *out,
                       const KError **error) {
  RulesInferred inf;
  const Value *w;
  ValueFormer former;
  Text n;
  const Values *ixv;
  const Family *fam;
  const TermMotive *mo;
  const Values *penv;
  Level mlvl;
  const Texts *names;
  QuantityUsage bu;
  const Value *sv;
  const Value *result_ty;
  if (!rules_infer_scrutinee(host, ctx, mode, e->scrut_q, e->scrut, &inf,
                             error))
    return 0;
  if (!rules_with_former(
          ctx, NO, text_of_cstring("the scrutinee is not a left former"),
          inf.ty, &w, &former, error))
    return 0;
  if (!rules_as_mu_value(former.shape, &n, &ixv)) {
    *error = error_mismatch(rules_wrong_pack());
    return 0;
  }
  if (!rules_mu_family(ctx, n, &fam, error)) return 0;
  if (!rules_mu_motive_of(n, fam, ixv, e->motive, &mo, error)) return 0;
  if (!rules_mu_param_env(ctx, former.diagram, &penv, error)) return 0;
  if (!rules_mu_motive_lvl(host, ctx, n, fam, mo, former.diagram,
                           rules_annotation(w), penv, &mlvl, error))
    return 0;
  if (!rules_mu_large(n, fam, mlvl, error)) return 0;
  if (!rules_mu_ctor_names(n, fam, &names, error)) return 0;
  if (!rules_mu_cover(n, names, e->branches, error)) return 0;
  if (!rules_mu_branches(host, ctx, mode, n, fam, mo, penv, e->branches, names,
                         &bu, error))
    return 0;
  if (!rules_c_eval(ctx, e->scrut, &sv, error)) return 0;
  if (!rules_mu_result(ctx, mo, ixv, sv, &result_ty, error)) return 0;
  out->ty = result_ty;
  out->uses = quantity_sequence(inf.uses, quantity_scale(mode, bu));
  return 1;
}

int rules_pi_intro_sec(const RulesHost *host, RulesCtx ctx, Quantity mode,
                       const TermLegs *legs, const Value *expected,
                       QuantityUsage *out, const KError **error) {
  const Value *w;
  ValueFormer former;
  RulesPoint pt;
  const TermLeg *lg;
  const TermBinder *b;
  const Value *cod;
  if (!rules_with_former(
          ctx, YES,
          text_of_cstring("a section needs a right former as its expected type"),
          expected, &w, &former, error))
    return 0;
  if (!rules_with_point(
          former.shape,
          text_of_cstring("a section at the point shape needs a point former"),
          &pt, error))
    return 0;
  lg = rules_one_of_leg(legs);
  if (lg == NULL) {
    *error = error_mismatch(
        text_of_cstring("a section at the point shape has one leg"));
    return 0;
  }
  b = rules_one_of_binder(lg->binders);
  if (b == NULL) {
    *error = error_mismatch(text_of_cstring("the leg binds the point once"));
    return 0;
  }
  if (!rules_binder_quantity(text_of_cstring("binder"), text_of_cstring("type"),
                             b->name, b->quantity, pt.quantity, error))
    return 0;
  if (!rules_c_open(ctx, former.diagram,
                    Values_cons(value_var(rules_c_size(ctx)), NULL), &cod,
                    error))
    return 0;
  return rules_pi_sec_body(host, ctx,
                           rules_c_bind(ctx, b->name, pt.quantity, pt.domain),
                           mode, lg->body, cod, out, error);
}

/* mo and expected: NULL = None{}. The two V.var(c_size(ctx)) cells are one
   cell. */
int rules_pi_branch(const RulesHost *host, RulesCtx ctx, Quantity mode,
                    const ValueShape *vs, const Closure *d, Quantity q,
                    const Value *dom, const TermMotive *mo,
                    const Value *expected, const TermAddr *key,
                    const TermLeg *leg, QuantityUsage *out,
                    const KError **error) {
  Int63 k;
  TermBinder a;
  TermBinder b;
  const Value *x;
  const Value *cod;
  if (!term_as_aleg(key, &k)) {
    *error =
        error_wrong_leg(text_of_cstring("the branch takes the leg address"));
    return 0;
  }
  if (!int63_equal(k, int63_zero())) {
    *error = error_wrong_leg(rules_leg_msg());
    return 0;
  }
  if (!rules_two_of_binders(leg->binders, &a, &b)) {
    *error = error_missing_branch(text_of_cstring(
        "the branch binds the point and the fibre element (D-M0-3)"));
    return 0;
  }
  if (!rules_binder_quantity(text_of_cstring("branch binder"),
                             text_of_cstring("type"), a.name, a.quantity, q,
                             error))
    return 0;
  x = value_var(rules_c_size(ctx));
  if (!rules_c_open(ctx, d, Values_cons(x, NULL), &cod, error)) return 0;
  return rules_pi_branch_body(
      host, ctx,
      rules_c_bind(rules_c_bind(ctx, a.name, a.quantity, dom), b.name,
                   b.quantity, cod),
      mode, leg->body, mo, expected,
      value_vin(vs, value_vapt(q, x),
                Values_cons(value_var(int63_succ(rules_c_size(ctx))), NULL)),
      out, error);
}

/* e = T.Elimination{_, scrut, sq, mo, branches}. expected: NULL = None{}. */
int rules_pi_elim_elim(const RulesHost *host, RulesCtx ctx, Quantity mode,
                       const TermElim *e, const Value *expected,
                       RulesInferred *out, const KError **error) {
  RulesInferred inf;
  const Value *w;
  ValueFormer former;
  RulesPoint pt;
  const Value *sv;
  const Value *result_ty;
  const TermBranch *branch;
  QuantityUsage uses;
  if (!rules_infer_scrutinee(host, ctx, mode, e->scrut_q, e->scrut, &inf,
                             error))
    return 0;
  if (!rules_with_former(
          ctx, NO, text_of_cstring("the scrutinee is not a left former"),
          inf.ty, &w, &former, error))
    return 0;
  if (!rules_with_point(former.shape, rules_wrong_pack(), &pt, error))
    return 0;
  if (!rules_check_motive(host, ctx, e->motive, w, error)) return 0;
  if (!rules_c_eval(ctx, e->scrut, &sv, error)) return 0;
  if (!rules_elim_result(ctx, e->motive, expected, sv, &result_ty, error))
    return 0;
  branch = rules_one_of_branch(e->branches);
  if (branch == NULL) {
    *error = error_missing_branch(
        text_of_cstring("an elimination at the point shape has one branch"));
    return 0;
  }
  if (!rules_pi_branch(host, ctx, mode, former.shape, former.diagram,
                       pt.quantity, pt.domain, e->motive, expected,
                       branch->address, branch->leg, &uses, error))
    return 0;
  out->ty = result_ty;
  out->uses = quantity_sequence(inf.uses, quantity_scale(mode, uses));
  return 1;
}

/* expected: NULL = None{}. MuPack{}: pchoose on right is an if. */
int rules_form_pack(const RulesHost *host, RulesCtx ctx, RulesPack p,
                    const TermShape *s, const Term *d, const Level *expected,
                    Bool right, Level *out, const KError **error) {
  switch (p) {
  case RULES_PI_PACK:
    return rules_form_pi(host, ctx, s, d, right, out, error);
  case RULES_COLL_PACK:
    return rules_form_coll(host, ctx, s, d, expected, out, error);
  case RULES_MU_PACK:
    if (right) {
      *error = error_not_yet(rules_mu_ran_word());
      return 0;
    }
    return rules_form_mu(host, ctx, s, d, out, error);
  }
  return 0;
}

int rules_form(const RulesHost *host, RulesCtx ctx, const TermShape *s,
               const Term *d, const Level *expected, Bool right, Level *out,
               const KError **error) {
  RulesPack p;
  if (!rules_rules(s->tag, &p, error)) return 0;
  return rules_form_pack(host, ctx, p, s, d, expected, right, out, error);
}

int rules_intro_sec_pack(const RulesHost *host, RulesCtx ctx, Quantity mode,
                         RulesPack p, const TermShape *s, const TermLegs *legs,
                         const Value *expected, QuantityUsage *out,
                         const KError **error) {
  switch (p) {
  case RULES_PI_PACK:
    return rules_pi_intro_sec(host, ctx, mode, legs, expected, out, error);
  case RULES_COLL_PACK:
    return rules_coll_intro_sec(host, ctx, mode, s, legs, expected, out,
                                error);
  case RULES_MU_PACK:
    *error = error_not_yet(rules_mu_ran_word());
    return 0;
  }
  return 0;
}

int rules_intro_sec(const RulesHost *host, RulesCtx ctx, Quantity mode,
                    const TermShape *s, const TermLegs *legs,
                    const Value *expected, QuantityUsage *out,
                    const KError **error) {
  RulesPack p;
  if (!rules_rules(s->tag, &p, error)) return 0;
  return rules_intro_sec_pack(host, ctx, mode, p, s, legs, expected, out,
                              error);
}

int rules_intro_in_pack(const RulesHost *host, RulesCtx ctx, Quantity mode,
                        RulesPack p, const TermAddr *addr, const Terms *args,
                        const Value *expected, QuantityUsage *out,
                        const KError **error) {
  switch (p) {
  case RULES_PI_PACK:
    return rules_pi_intro_in(host, ctx, mode, addr, args, expected, out,
                             error);
  case RULES_COLL_PACK:
    return rules_coll_intro_in(host, ctx, mode, addr, args, expected, out,
                               error);
  case RULES_MU_PACK:
    return rules_mu_intro_in(host, ctx, mode, addr, args, expected, out,
                             error);
  }
  return 0;
}

int rules_intro_in(const RulesHost *host, RulesCtx ctx, Quantity mode,
                   const TermShape *s, const TermAddr *addr, const Terms *args,
                   const Value *expected, QuantityUsage *out,
                   const KError **error) {
  RulesPack p;
  if (!rules_rules(s->tag, &p, error)) return 0;
  return rules_intro_in_pack(host, ctx, mode, p, addr, args, expected, out,
                             error);
}

/* expected: NULL = None{}. */
int rules_elim_elim_pack(const RulesHost *host, RulesCtx ctx, Quantity mode,
                         RulesPack p, const TermElim *e, const Value *expected,
                         RulesInferred *out, const KError **error) {
  switch (p) {
  case RULES_PI_PACK:
    return rules_pi_elim_elim(host, ctx, mode, e, expected, out, error);
  case RULES_COLL_PACK:
    return rules_coll_elim_elim(host, ctx, mode, e, expected, out, error);
  case RULES_MU_PACK:
    return rules_mu_elim_elim(host, ctx, mode, e, out, error);
  }
  return 0;
}

/* Bend makes a new T.Elimination cell with the same five fields: e goes on
   as it is. */
int rules_elim_elim(const RulesHost *host, RulesCtx ctx, Quantity mode,
                    const TermElim *e, const Value *expected,
                    RulesInferred *out, const KError **error) {
  RulesPack p;
  if (!rules_rules(e->shape->tag, &p, error)) return 0;
  return rules_elim_elim_pack(host, ctx, mode, p, e, expected, out, error);
}
