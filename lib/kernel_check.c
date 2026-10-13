/* kernel_check.c: C twin of lib/kernel_check.bend (stage S3, session 3).
   Bidirectional checking and declaration admission at Kanon 69f3be51.

   C = K.ctx, so each context argument of a program is a RulesCtx
   (ctx.check). A program takes the host first and returns int (1 = Pure
   with *out, 0 = Abort with *error). The records runtime_ops (line 96) and
   ops (line 98) have no C twin: their fields are the runtime_ops_* and
   check_ops_* functions. fuelled, with_fuel, polled, result_of, drive and
   rules have no C twin: the fuel is one Budget cell (host->fuel) that
   check_request threads. The let_bound body closure is the closed
   CheckBody. Self tail calls are loops; the other recursion stays.

   Part 1 (S3 session 3b): Bend lines 1-220, make to declarations_program.
   Part 2 (S3 session 3c): Bend lines 222-435, family_decl to define_ctors. */
#include "sole.h"

/* String.concat of a fixed list of parts (the cat idiom of kernel_rules). */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

static RulesCtx ctx_of(const CheckCtx *c) {
  RulesCtx context;
  context.check = c;
  return context;
}

/* R.at (kernel_rules line 258) at local; NULL = None{}. */
static const CheckLocal *at_local(Int63 index, const CheckLocals *xs) {
  for (; xs != NULL; xs = xs->tail) {
    if (int63_equal(index, int63_zero())) return &xs->head;
    index = int63_pred(index);
  }
  return NULL;
}

const CheckCtx *check_make(const Globals *globals, Budget budget) {
  CheckCtx *c = (CheckCtx *)arena_alloc(sizeof *c);
  c->globals = globals;
  c->env = NULL;
  c->locals = NULL;
  c->size = int63_zero();
  c->budget = budget;
  return c;
}

const Globals *check_globals(const CheckCtx *c) { return c->globals; }

const Values *runtime_ops_env(const CheckCtx *ctx) { return ctx->env; }

const CheckLocals *check_locals(const CheckCtx *c) { return c->locals; }

Int63 runtime_ops_size(const CheckCtx *ctx) { return ctx->size; }

Budget check_budget(const CheckCtx *c) { return c->budget; }

const CheckCtx *runtime_ops_bind(Text name, Quantity quantity,
                                 const Value *ty, const CheckCtx *ctx) {
  CheckCtx *next = (CheckCtx *)arena_alloc(sizeof *next);
  next->globals = ctx->globals;
  next->env = Values_cons(value_var(ctx->size), ctx->env);
  next->locals =
      CheckLocals_cons((CheckLocal){name, quantity, ty}, ctx->locals);
  next->size = int63_succ(ctx->size);
  next->budget = ctx->budget;
  return next;
}

const CheckCtx *runtime_ops_define(Text name, Quantity quantity,
                                   const Value *ty, const Value *value,
                                   const CheckCtx *ctx) {
  CheckCtx *next = (CheckCtx *)arena_alloc(sizeof *next);
  next->globals = ctx->globals;
  next->env = Values_cons(value, ctx->env);
  next->locals =
      CheckLocals_cons((CheckLocal){name, quantity, ty}, ctx->locals);
  next->size = int63_succ(ctx->size);
  next->budget = ctx->budget;
  return next;
}

const Texts *check_local_names(const CheckLocals *ls) {
  return ls == NULL ? NULL
                    : Texts_cons(ls->head.name, check_local_names(ls->tail));
}

/* value = NULL is Fail{_}: the error is not read. */
Text check_pp_result(const Texts *names, const Term *value) {
  return value == NULL ? text_of_cstring("a value that does not read back")
                       : pp_term(names, value);
}

/* pp_value (line 68). */
Text check_ops_pp(const CheckCtx *ctx, const Value *value) {
  const Term *tm = NULL;
  const KError *ignored = NULL;
  const Term *read =
      eval_quote(ctx->globals, ctx->size, value, &tm, &ignored) ? tm : NULL;
  return check_pp_result(check_local_names(ctx->locals), read);
}

const Value *check_local_ty(const CheckLocal *value) { return value->ty; }

/* head_ty (line 74). */
int runtime_ops_head_ty(const CheckCtx *ctx, const Head *head,
                        const Value **out, const KError **error) {
  const CheckLocal *local = NULL;
  const GlobalEntry *entry = NULL;
  switch (head->tag) {
    case VALUE_HLOCAL:
      local = at_local(
          int63_pred(int63_sub(ctx->size, head->as.hlocal.level)),
          ctx->locals);
      if (local == NULL) {
        *error = error_unbound(
            text_of_cstring("a local level is outside the context"));
        return 0;
      }
      *out = check_local_ty(local);
      return 1;
    case VALUE_HGLOBAL:
      break;
  }
  entry = global_find(head->as.hglobal.name, ctx->globals);
  if (entry == NULL) {
    *error = error_unbound(head->as.hglobal.name);
    return 0;
  }
  return eval_eval(ctx->globals, NULL, global_entry_ty(entry), out, error);
}

Bool check_readable(Quantity mode, Quantity q) {
  return (quantity_equal(mode, QUANTITY_ZERO) ||
          !quantity_equal(q, QUANTITY_ZERO))
             ? YES
             : NO;
}

/* close_loop (line 81): a self tail call, so a loop. */
int check_close_loop(const CheckLocals *ls, Int63 level, Int63 lower,
                     Quantity mode, Bool affine, QuantityUsage uses,
                     QuantityUsage *out, const KError **error) {
  for (; ls != NULL; ls = ls->tail) {
    if (rules_less_than(int63_compare(level, lower))) break;
    if (!quantity_equal(mode, QUANTITY_ZERO) &&
        quantity_equal(ls->head.quantity, QUANTITY_ONE) &&
        !(affine ? quantity_at_most_once(level, uses)
                 : quantity_exactly_once(level, uses))) {
      Text alias[3] = {text_of_cstring("the linear alias "), ls->head.name,
                       text_of_cstring(" may be used at most once")};
      Text binder[3] = {
          text_of_cstring("the linear binder "), ls->head.name,
          text_of_cstring(" must be used exactly once on every runtime path")};
      *error = error_quantity(affine ? cat(alias, 3) : cat(binder, 3));
      return 0;
    }
    uses = quantity_remove(level, uses);
    level = int63_pred(level);
  }
  *out = uses;
  return 1;
}

int check_close(Bool affine, const CheckCtx *c, Int63 lower, Quantity mode,
                QuantityUsage uses, QuantityUsage *out,
                const KError **error) {
  return check_close_loop(c->locals, int63_pred(c->size), lower, mode, affine,
                          uses, out, error);
}

/* quote_value (line 93). */
int runtime_ops_quote(const CheckCtx *ctx, const Value *value,
                      const Term **out, const KError **error) {
  return eval_quote(ctx->globals, ctx->size, value, out, error);
}

/* The closures of runtime_ops (line 96). */
int runtime_ops_infer_univ(const CheckCtx *ctx, const Term *term, Level *out,
                           const KError **error) {
  *error = error_mismatch(text_of_cstring(
      "internal universe request outside the checking interpreter"));
  return 0;
}

int runtime_ops_whnf(const CheckCtx *ctx, const Value *value,
                     const Value **out, const KError **error) {
  return eval_whnf(ctx->globals, value, out, error);
}

int runtime_ops_eval_env(const CheckCtx *ctx, const Values *env,
                         const Term *term, const Value **out,
                         const KError **error) {
  return eval_eval(ctx->globals, env, term, out, error);
}

const Family *runtime_ops_family(const CheckCtx *ctx, Text name) {
  return global_find_family(name, ctx->globals);
}

const NamedFamilies *runtime_ops_families(const CheckCtx *ctx) {
  return global_family_list(ctx->globals);
}

/* The close closure of ops (line 98). */
int check_ops_close(const CheckCtx *ctx, Int63 lower, Quantity mode,
                    QuantityUsage uses, QuantityUsage *out,
                    const KError **error) {
  return check_close(NO, ctx, lower, mode, uses, out, error);
}

int check_infer_request(const RulesHost *host, RulesCtx c, Quantity q,
                        const Term *tm, RulesInferred *out,
                        const KError **error) {
  return rules_c_infer(host, c, q, tm, out, error);
}

int check_check_request(const RulesHost *host, RulesCtx c, Quantity q,
                        const Term *tm, const Value *ty, QuantityUsage *out,
                        const KError **error) {
  return rules_c_check(host, c, q, tm, ty, out, error);
}

int check_univ_request(const RulesHost *host, RulesCtx c, const Term *tm,
                       Level *out, const KError **error) {
  return rules_ask_univ(host, c, tm, out, error);
}

int check_ensure(const RulesHost *host, RulesCtx c, const Value *got,
                 const Value *expected, const KError **error) {
  Bool same = NO;
  if (!rules_ask_type(host, c, got, expected, &same, error)) return 0;
  if (same) return 1;
  {
    Text parts[4] = {text_of_cstring("the term has type "),
                     check_ops_pp(c.check, got),
                     text_of_cstring(" and the expected type is "),
                     check_ops_pp(c.check, expected)};
    *error = error_mismatch(cat(parts, 4));
  }
  return 0;
}

int check_infer_local(const RulesHost *host, RulesCtx c, Quantity mode,
                      Int63 ix, const CheckLocal *entry, RulesInferred *out,
                      const KError **error) {
  if (check_readable(mode, entry->quantity)) {
    out->ty = entry->ty;
    out->uses = quantity_occurrence(
        int63_pred(int63_sub(c.check->size, ix)), mode);
    return 1;
  }
  {
    Text parts[3] = {text_of_cstring("the erased binder "), entry->name,
                     text_of_cstring(" is read in a runtime position")};
    *error = error_quantity(cat(parts, 3));
  }
  return 0;
}

const KError *check_no_infer(Text what) {
  return error_cannot_infer(text_append(
      what,
      text_of_cstring(" has no type of its own;  it needs an expected type")));
}

int check_infer_literal(const RulesHost *host, RulesCtx c,
                        const Literal *lit, RulesInferred *out,
                        const KError **error) {
  const Value *ty = NULL;
  switch (lit->tag) {
    case LITERAL_LSTRING:
      *error = error_not_yet(text_of_cstring("string types arrive at M1"));
      return 0;
    case LITERAL_LINT:
      break;
  }
  if (!rules_nonnegative(bignum_sign(lit->as.lint.value))) {
    *error = error_mismatch(text_of_cstring("a Nat literal must be nonnegative"));
    return 0;
  }
  if (!eval_eval(c.check->globals, NULL, prim_nat_ty(), &ty, error)) return 0;
  out->ty = ty;
  out->uses = quantity_empty();
  return 1;
}

/* linear_definition (line 122): the Bool.or fold stops early (pure). */
Bool check_linear_definition(const CheckLocals *ls, Int63 level,
                             QuantityUsage uses) {
  for (; ls != NULL; ls = ls->tail) {
    if (quantity_equal(ls->head.quantity, QUANTITY_ONE) &&
        quantity_used(level, uses))
      return YES;
    level = int63_pred(level);
  }
  return NO;
}

int check_let_finish(const RulesHost *host, RulesCtx outer, RulesCtx inner,
                     Quantity mode, Bool linear, QuantityUsage def_uses,
                     RulesInferred answer, RulesInferred *out,
                     const KError **error) {
  QuantityUsage free_uses = quantity_empty();
  if (!check_close(linear, inner.check, outer.check->size, mode, answer.uses,
                   &free_uses, error))
    return 0;
  out->ty = answer.ty;
  out->uses = quantity_sequence(quantity_scale(mode, def_uses), free_uses);
  return 1;
}

/* body(inner) of let_bound: the closed CheckBody. */
static int body_run(const RulesHost *host, const CheckBody *body,
                    RulesCtx inner, RulesInferred *out,
                    const KError **error) {
  QuantityUsage uses = quantity_empty();
  switch (body->kind) {
    case CHECK_BODY_INFER:
      return check_infer_request(host, inner, body->mode, body->body, out,
                                 error);
    case CHECK_BODY_CHECK:
      break;
  }
  if (!check_check_request(host, inner, body->mode, body->body,
                           body->expected, &uses, error))
    return 0;
  out->ty = body->expected;
  out->uses = uses;
  return 1;
}

int check_let_bound(const RulesHost *host, RulesCtx outer, Quantity mode,
                    RulesCtx inner, QuantityUsage uses, Bool linear,
                    const CheckBody *body, RulesInferred *out,
                    const KError **error) {
  RulesInferred answer;
  if (!body_run(host, body, inner, &answer, error)) return 0;
  return check_let_finish(host, outer, inner, mode, linear, uses, answer, out,
                          error);
}

int check_inferred_universe(const RulesHost *host, RulesCtx c,
                            RulesInferred value, Level *out,
                            const KError **error) {
  const Value *w = NULL;
  const Level *level = NULL;
  if (!eval_whnf(c.check->globals, value.ty, &w, error)) return 0;
  level = value_as_univ(w);
  if (level == NULL) {
    Text parts[2] = {
        text_of_cstring("a term used as a type is not a universe:  "),
        check_ops_pp(c.check, w)};
    *error = error_universe(cat(parts, 2));
    return 0;
  }
  *out = *level;
  return 1;
}

int check_check_former(const RulesHost *host, RulesCtx c, const TermShape *s,
                       const Term *d, const Value *expected, Bool right,
                       QuantityUsage *out, const KError **error) {
  const Value *w = NULL;
  const Level *l0 = NULL;
  Level l;
  if (!eval_whnf(c.check->globals, expected, &w, error)) return 0;
  l0 = value_as_univ(w);
  if (l0 == NULL) {
    Text parts[2] = {
        text_of_cstring(
            "a type former is checked against a type that is not a universe:  "),
        check_ops_pp(c.check, w)};
    *error = error_universe(cat(parts, 2));
    return 0;
  }
  if (!rules_form(host, c, s, d, l0, right, &l, error)) return 0;
  if (!level_equal(l, *l0)) {
    Text parts[4] = {text_of_cstring("the former lives at "),
                     level_to_string(l),
                     text_of_cstring(" and the expected universe is "),
                     level_to_string(*l0)};
    *error = error_universe(cat(parts, 4));
    return 0;
  }
  *out = quantity_empty();
  return 1;
}

int check_inferred_check(const RulesHost *host, RulesCtx c,
                         const Value *expected, RulesInferred value,
                         QuantityUsage *out, const KError **error) {
  if (!check_ensure(host, c, value.ty, expected, error)) return 0;
  *out = value.uses;
  return 1;
}

QuantityUsage check_inferred_uses(RulesInferred value) { return value.uses; }

int check_infer_then_check(const RulesHost *host, RulesCtx c, Quantity mode,
                           const Term *tm, const Value *expected,
                           QuantityUsage *out, const KError **error) {
  RulesInferred value;
  if (!check_infer_request(host, c, mode, tm, &value, error)) return 0;
  return check_inferred_check(host, c, expected, value, out, error);
}

RulesAnswer check_inferred_answer(RulesInferred value) {
  RulesAnswer answer;
  answer.tag = RULES_INFERRED_ANSWER;
  answer.as.inferred.ty = value.ty;
  answer.as.inferred.uses = value.uses;
  return answer;
}

/* poll (line 166). */
int check_poll(const RulesTask *task, Budget budget, Budget *out,
               const KError **error) {
  switch (task->tag) {
    case RULES_INFER_USES:
      return budget_spend(text_of_cstring("the check budget is exhausted"),
                          budget, out, error);
    case RULES_CHECK_USES:
    case RULES_INFER_UNIVERSE:
    case RULES_COMPARE:
    case RULES_EVALUATE:
    case RULES_WEAK:
    case RULES_READBACK:
      break;
  }
  *out = budget;
  return 1;
}

const Value *check_inferred_type(RulesInferred value) { return value.ty; }

Text check_declaration_name(const CheckDecl *d) { return d->name; }

int check_declaration_body(const RulesHost *host, RulesCtx c, Text n,
                           CheckKind kind, const Term *ty, const Term *body,
                           const Value *tyv, const GlobalEntry **out,
                           const KError **error) {
  QuantityUsage uses = quantity_empty();
  switch (kind) {
    case CHECK_POSTULATE:
      *out = global_postulate(ty);
      return 1;
    case CHECK_DEFINITION:
      break;
  }
  if (body == NULL) {
    Text parts[3] = {text_of_cstring("the definition "), n,
                     text_of_cstring(" has no body")};
    *error = error_cannot_infer(cat(parts, 3));
    return 0;
  }
  if (!check_check_request(host, c, QUANTITY_ONE, body, tyv, &uses, error))
    return 0;
  *out = global_definition(ty, body, YES, NULL, NO);
  return 1;
}

int check_check_decl_program(const RulesHost *host, RulesCtx c,
                             const CheckDecl *d, const GlobalEntry **out,
                             const KError **error) {
  Level level;
  const Value *tyv = NULL;
  if (!check_univ_request(host, c, d->ty, &level, error)) return 0;
  if (!rules_c_eval(c, d->ty, &tyv, error)) return 0;
  return check_declaration_body(host, c, d->name, d->kind, d->ty, d->body,
                                tyv, out, error);
}

/* declarations_program (line 215): recursion stays. Each declaration is
   checked in a fresh make(g, b), and g grows by the admitted entry. */
int check_declarations_program(const RulesHost *host, const CheckDecls *ds,
                               const Globals *g, Budget b,
                               const CheckNamedEntries **out,
                               const KError **error) {
  const GlobalEntry *entry = NULL;
  const CheckNamedEntries *tail = NULL;
  if (ds == NULL) {
    *out = NULL;
    return 1;
  }
  if (!check_check_decl_program(host, ctx_of(check_make(g, b)), &ds->head,
                                &entry, error))
    return 0;
  if (!check_declarations_program(
          host, ds->tail,
          global_add(check_declaration_name(&ds->head), entry, g), b, &tail,
          error))
    return 0;
  *out = CheckNamedEntries_cons(
      (CheckNamedEntry){check_declaration_name(&ds->head), entry}, tail);
  return 1;
}

/* family_decl (line 222) and ctor_decl (line 225) are CheckFamilyDecl and
   CheckCtorDecl (sole.h). */

/* check_tele_loop (line 228): a self tail call, so a loop. */
int check_check_tele_loop(const RulesHost *host, const Telescope *tele,
                          RulesCtx c, RulesCtx *out, const KError **error) {
  for (; tele != NULL; tele = tele->tail) {
    Level level;
    const Value *tyv = NULL;
    if (!check_univ_request(host, c, tele->head.binding.type, &level, error))
      return 0;
    if (!rules_c_eval(c, tele->head.binding.type, &tyv, error)) return 0;
    c = ctx_of(runtime_ops_bind(tele->head.binding.name, tele->head.quantity,
                                tyv, c.check));
  }
  *out = c;
  return 1;
}

int check_check_telescope(const RulesHost *host, RulesCtx c,
                          const Telescope *tele, RulesCtx *out,
                          const KError **error) {
  return check_check_tele_loop(host, tele, c, out, error);
}

int check_index_rules(const RulesHost *host, RulesCtx c, Text name,
                      Level level, TeleEntry entry, const KError **error) {
  Level l;
  if (!quantity_equal(entry.quantity, QUANTITY_ZERO)) {
    Text parts[7] = {text_of_cstring("the index "), entry.binding.name,
                     text_of_cstring(" of "), name,
                     text_of_cstring(" is at quantity "),
                     quantity_to_string(entry.quantity),
                     text_of_cstring(" and every index binder is at 0")};
    *error = error_index_not_zero(cat(parts, 7));
    return 0;
  }
  if (!check_univ_request(host, c, entry.binding.type, &l, error)) return 0;
  if (!level_le(l, level)) {
    Text parts[10] = {text_of_cstring("the index "), entry.binding.name,
                      text_of_cstring(" of "), name,
                      text_of_cstring(" lives at "), level_to_string(l),
                      text_of_cstring(" and "), name,
                      text_of_cstring(" is declared at "),
                      level_to_string(level)};
    *error = error_index_above_universe(cat(parts, 10));
    return 0;
  }
  return 1;
}

/* check_index_loop (line 242): a self tail call, so a loop. Each entry is
   checked as the singleton telescope [e]. */
int check_check_index_loop(const RulesHost *host, const Telescope *tele,
                           RulesCtx c, Text name, Level level, RulesCtx *out,
                           const KError **error) {
  for (; tele != NULL; tele = tele->tail) {
    RulesCtx inner;
    if (!check_index_rules(host, c, name, level, tele->head, error)) return 0;
    if (!check_check_telescope(host, c, Telescope_cons(tele->head, NULL),
                               &inner, error))
      return 0;
    c = inner;
  }
  *out = c;
  return 1;
}

int check_check_index_telescope(const RulesHost *host, RulesCtx c, Text name,
                                Level level, const Telescope *tele,
                                RulesCtx *out, const KError **error) {
  return check_check_index_loop(host, tele, c, name, level, out, error);
}

/* family = NULL is None{}. */
Bool check_absent_family(const Family *family) {
  return family == NULL ? YES : NO;
}

int check_declare_family_program(const RulesHost *host, const Globals *g,
                                 Budget b, const CheckFamilyDecl *d,
                                 const Globals **out, const KError **error) {
  RulesCtx pctx;
  RulesCtx ictx;
  if (!check_absent_family(global_find_family(d->name, g))) {
    Text parts[3] = {text_of_cstring("the family "), d->name,
                     text_of_cstring(" is already declared")};
    *error = error_mismatch(cat(parts, 3));
    return 0;
  }
  if (!check_check_telescope(host, ctx_of(check_make(g, b)), d->params,
                             &pctx, error))
    return 0;
  if (!check_check_index_telescope(host, pctx, d->name, d->level, d->indices,
                                   &ictx, error))
    return 0;
  *out = global_add_family(
      d->name,
      positivity_family(d->name, d->params, d->indices, d->level,
                        positivity_provisional(), NULL, NO),
      g);
  return 1;
}

/* parameter_at (line 262): the T.Ann arm is a self tail call, so a loop. */
Bool check_parameter_at(Int63 index, const Term *tm) {
  while (tm->tag == TERM_ANN) tm = tm->as.ann.term;
  switch (tm->tag) {
    case TERM_ANN:
    case TERM_UNIV:
    case TERM_LAN:
    case TERM_RAN:
    case TERM_IN:
    case TERM_ELIM:
    case TERM_SEC:
    case TERM_OUT:
    case TERM_LET:
    case TERM_GLOBAL:
    case TERM_LIT:
    case TERM_AUTO:
      return NO;
    case TERM_VAR:
      break;
  }
  return int63_equal(tm->as.var.index, index);
}

/* parameters_at (line 278): the Bool.and fold stops early (pure). */
Bool check_parameters_at(const Terms *params, Int63 index) {
  for (; params != NULL; params = params->tail) {
    if (!check_parameter_at(index, params->head)) return NO;
    index = int63_pred(index);
  }
  return YES;
}

/* check_field_loop (line 283): a self tail call, so a loop. */
int check_check_field_loop(const RulesHost *host, const Telescope *tele,
                           RulesCtx c, Text name, Level level, RulesCtx *out,
                           const KError **error) {
  for (; tele != NULL; tele = tele->tail) {
    Level l;
    RulesCtx inner;
    if (!check_univ_request(host, c, tele->head.binding.type, &l, error))
      return 0;
    if (!level_le(l, level)) {
      Text parts[3] = {text_of_cstring("a field of "), name,
                       text_of_cstring(" exceeds its family universe")};
      *error = error_universe(cat(parts, 3));
      return 0;
    }
    if (!check_check_telescope(host, c, Telescope_cons(tele->head, NULL),
                               &inner, error))
      return 0;
    c = inner;
  }
  *out = c;
  return 1;
}

/* R.length (kernel_rules) at T.t. */
static Int63 terms_length(const Terms *xs) {
  return rules_advance(int63_zero(), Terms_length(xs));
}

/* R.length (kernel_rules) at the telescope entry. */
static Int63 tele_length(const Telescope *xs) {
  return rules_advance(int63_zero(), Telescope_length(xs));
}

int check_check_ctor(const RulesHost *host, RulesCtx c, const Family *fam,
                     const Texts *group, const CheckCtorDecl *cd,
                     const Ctor **out, const KError **error) {
  Int63 nparams = tele_length(fam->params);
  Int63 nargs = tele_length(cd->args);
  Int63 nindices = tele_length(fam->indices);
  RulesCtx actx;
  const Values *penv = NULL;
  const Values *ienv = NULL;
  if (!check_check_field_loop(host, cd->args, c, cd->name, fam->level, &actx,
                              error))
    return 0;
  if (!positivity_ctor_fields(group, cd->args, error)) return 0;
  if (!(int63_equal(terms_length(cd->params), nparams) &&
        check_parameters_at(cd->params,
                            int63_pred(int63_add(nargs, nparams))))) {
    Text parts[3] = {text_of_cstring("the constructor "), cd->name,
                     text_of_cstring(" must preserve its family parameters")};
    *error = error_mismatch(cat(parts, 3));
    return 0;
  }
  if (!rules_mu_telescope(
          host, actx, QUANTITY_ZERO,
          text_append(text_of_cstring("the parameters of "), cd->name),
          fam->params, cd->params, NULL, &penv, error))
    return 0;
  if (!int63_equal(terms_length(cd->indices), nindices)) {
    Text parts[7] = {cd->name,
                     text_of_cstring(" gives "),
                     int63_show(terms_length(cd->indices)),
                     text_of_cstring(" result indices and "),
                     fam->name,
                     text_of_cstring(" takes "),
                     int63_show(nindices)};
    *error = error_mismatch(cat(parts, 7));
    return 0;
  }
  if (!rules_mu_telescope(
          host, actx, QUANTITY_ZERO,
          text_append(text_of_cstring("the result indices of "), cd->name),
          fam->indices, cd->indices, penv, &ienv, error))
    return 0;
  *out = positivity_ctor(cd->name, cd->args, cd->indices,
                         int63_add(nparams, nargs),
                         positivity_self_rec(group, cd->args));
  return 1;
}

/* ctors_program (line 301): recursion stays. */
int check_ctors_program(const RulesHost *host, const CheckCtorDecls *cds,
                        RulesCtx c, const Family *fam, const Texts *group,
                        const Ctors **out, const KError **error) {
  const Ctor *ctor = NULL;
  const Ctors *tail = NULL;
  if (cds == NULL) {
    *out = NULL;
    return 1;
  }
  if (!check_check_ctor(host, c, fam, group, &cds->head, &ctor, error))
    return 0;
  if (!check_ctors_program(host, cds->tail, c, fam, group, &tail, error))
    return 0;
  *out = Ctors_cons(ctor, tail);
  return 1;
}

Bool check_provisional(const Family *f) {
  switch (f->status->tag) {
    case CTOR_BUILTIN:
    case CTOR_COMPLETE:
      return NO;
    case CTOR_PROVISIONAL:
      break;
  }
  return YES;
}

const Texts *check_ctor_names(const Ctors *ctors) {
  return ctors == NULL ? NULL
                       : Texts_cons(positivity_ctor_name(ctors->head),
                                    check_ctor_names(ctors->tail));
}

const Family *check_complete_family(const Family *f, const Ctors *ctors) {
  return positivity_family(f->name, f->params, f->indices, f->level,
                           positivity_complete(check_ctor_names(ctors)),
                           ctors, YES);
}

int check_define_ctors_program(const RulesHost *host, const Globals *g,
                               Budget b, const Texts *group, Text name,
                               const CheckCtorDecls *cds, const Globals **out,
                               const KError **error) {
  const Family *fam = global_find_family(name, g);
  const Ctors *ctors = NULL;
  RulesCtx pctx;
  if (fam == NULL) {
    Text parts[3] = {text_of_cstring("the family "), name,
                     text_of_cstring(" is not declared")};
    *error = error_unbound(cat(parts, 3));
    return 0;
  }
  if (!check_provisional(fam)) {
    Text parts[3] = {text_of_cstring("the constructors of "), name,
                     text_of_cstring(" are already installed")};
    *error = error_mismatch(cat(parts, 3));
    return 0;
  }
  if (!check_check_telescope(host, ctx_of(check_make(g, b)),
                             rules_family_params(fam), &pctx, error))
    return 0;
  if (!check_ctors_program(host, cds, pctx, fam, group, &ctors, error))
    return 0;
  *out = global_add_family(name, check_complete_family(fam, ctors), g);
  return 1;
}

int check_let_continue(const RulesHost *host, RulesCtx c, Quantity mode,
                       Text x, const Value *ty, const Value *value,
                       QuantityUsage uses, Bool linear, const CheckBody *body,
                       RulesInferred *out, const KError **error) {
  Quantity q = QUANTITY_MANY;
  if (linear) q = QUANTITY_ONE;
  if (quantity_equal(mode, QUANTITY_ZERO)) q = QUANTITY_ZERO;
  return check_let_bound(host, c, mode,
                         ctx_of(runtime_ops_define(x, q, ty, value, c.check)),
                         uses, linear, body, out, error);
}

int check_let_body(const RulesHost *host, RulesCtx c, Quantity mode, Text x,
                   const Term *ty, const Term *definition,
                   const CheckBody *body, RulesInferred *out,
                   const KError **error) {
  Level level;
  const Value *tyv = NULL;
  const Value *v = NULL;
  QuantityUsage uses = quantity_empty();
  if (!check_univ_request(host, c, ty, &level, error)) return 0;
  if (!rules_c_eval(c, ty, &tyv, error)) return 0;
  if (!check_check_request(host, c, quantity_runtime(mode), definition, tyv,
                           &uses, error))
    return 0;
  if (!rules_c_eval(c, definition, &v, error)) return 0;
  return check_let_continue(
      host, c, mode, x, tyv, v, uses,
      check_linear_definition(c.check->locals, int63_pred(c.check->size),
                              uses),
      body, out, error);
}

/* routed (line 339): the family of a mu elimination whose leg binds
   hypotheses. None keeps the term on the kernel rule. */
MaybeText check_routed(const CheckCtx *c, const TermShape *s,
                       const TermBranches *bs) {
  switch (s->tag) {
    case SHAPE_SPI:
    case SHAPE_SCOLL:
    case SHAPE_SPAR:
    case SHAPE_SNU:
      return maybe_text_none();
    case SHAPE_SMU:
      break;
  }
  return recursor_route_routes(global_find_family(s->as.smu.family, c->globals),
                               bs)
             ? maybe_text_some(s->as.smu.family)
             : maybe_text_none();
}

int check_recursor(const RulesHost *host, RulesCtx c, Quantity mode,
                   MaybeText owner, const TermShape *s, const Term *scrut,
                   Quantity sq, const TermMotive *mo, const TermBranches *bs,
                   const Value *expected, RulesInferred *out,
                   const KError **error) {
  if (!owner.some)
    return rules_elim_elim(host, c, mode,
                           term_elimination(s, scrut, sq, mo, bs), expected,
                           out, error);
  if (mo == NULL) {
    *error = error_cannot_infer(
        text_of_cstring("an elimination at a mu shape needs a motive"));
    return 0;
  }
  return recursor_elim_check(host, c, mode, owner.text, sq, scrut, mo, bs, out,
                             error);
}

/* route (line 353): a kernel Elim term routes to the checked recursor when a
   leg binds hypotheses; every other term stays on the kernel rule. */
int check_route(const RulesHost *host, RulesCtx c, Quantity mode,
                const TermElim *e, const Value *expected, RulesInferred *out,
                const KError **error) {
  return check_recursor(host, c, mode,
                        check_routed(c.check, e->shape, e->branches), e->shape,
                        e->scrut, e->scrut_q, e->motive, e->branches, expected,
                        out, error);
}

int check_infer_node(const RulesHost *host, RulesCtx c, Quantity mode,
                     const Term *tm, RulesInferred *out,
                     const KError **error) {
  const CheckLocal *entry = NULL;
  const Value *ty = NULL;
  QuantityUsage uses = quantity_empty();
  Level l;
  Head head;
  CheckBody body;
  switch (tm->tag) {
    case TERM_VAR:
      entry = at_local(tm->as.var.index, c.check->locals);
      if (entry == NULL) {
        Text parts[3] = {text_of_cstring("de Bruijn index "),
                         int63_show(tm->as.var.index),
                         text_of_cstring(" is outside the context")};
        *error = error_unbound(cat(parts, 3));
        return 0;
      }
      return check_infer_local(host, c, mode, tm->as.var.index, entry, out,
                               error);
    case TERM_UNIV:
      out->ty = value_vuniv(level_succ(tm->as.univ.level));
      out->uses = quantity_empty();
      return 1;
    case TERM_LAN:
      if (!rules_form(host, c, tm->as.lan.shape, tm->as.lan.diagram, NULL, NO,
                      &l, error))
        return 0;
      out->ty = value_vuniv(l);
      out->uses = quantity_empty();
      return 1;
    case TERM_RAN:
      if (!rules_form(host, c, tm->as.ran.shape, tm->as.ran.diagram, NULL, YES,
                      &l, error))
        return 0;
      out->ty = value_vuniv(l);
      out->uses = quantity_empty();
      return 1;
    case TERM_OUT:
      return rules_elim_out(host, c, mode, tm->as.out.shape,
                            tm->as.out.address, tm->as.out.head, out, error);
    case TERM_ELIM:
      return check_route(host, c, mode, tm->as.elim.elim, NULL, out, error);
    case TERM_LET:
      body.kind = CHECK_BODY_INFER;
      body.mode = mode;
      body.body = tm->as.let.body;
      body.expected = NULL;
      return check_let_body(host, c, mode, tm->as.let.name, tm->as.let.ty,
                            tm->as.let.value, &body, out, error);
    case TERM_ANN:
      if (!check_univ_request(host, c, tm->as.ann.ty, &l, error)) return 0;
      if (!rules_c_eval(c, tm->as.ann.ty, &ty, error)) return 0;
      if (!check_check_request(host, c, mode, tm->as.ann.term, ty, &uses,
                               error))
        return 0;
      out->ty = ty;
      out->uses = uses;
      return 1;
    case TERM_GLOBAL:
      head.tag = VALUE_HGLOBAL;
      head.as.hglobal.name = tm->as.global.name;
      if (!runtime_ops_head_ty(c.check, &head, &ty, error)) return 0;
      out->ty = ty;
      out->uses = quantity_empty();
      return 1;
    case TERM_LIT:
      return check_infer_literal(host, c, tm->as.lit.literal, out, error);
    case TERM_IN:
      *error = check_no_infer(text_of_cstring("an injection"));
      return 0;
    case TERM_SEC:
      *error = check_no_infer(text_of_cstring("a section"));
      return 0;
    case TERM_AUTO:
      break;
  }
  *error = error_not_yet(rules_auto_word());
  return 0;
}

/* check_node (line 373): Var, Univ, Out, Ann, Global, Lit and Auto go to
   infer_then_check with tm itself (Bend rebuilds the same constructor). */
int check_check_node(const RulesHost *host, RulesCtx c, Quantity mode,
                     const Term *tm, const Value *expected,
                     QuantityUsage *out, const KError **error) {
  RulesInferred value;
  CheckBody body;
  switch (tm->tag) {
    case TERM_SEC:
      return rules_intro_sec(host, c, mode, tm->as.sec.shape, tm->as.sec.legs,
                             expected, out, error);
    case TERM_IN:
      return rules_intro_in(host, c, mode, tm->as.in.shape, tm->as.in.address,
                            tm->as.in.args, expected, out, error);
    case TERM_ELIM:
      if (!check_route(host, c, mode, tm->as.elim.elim, expected, &value,
                       error))
        return 0;
      return check_inferred_check(host, c, expected, value, out, error);
    case TERM_LAN:
      return check_check_former(host, c, tm->as.lan.shape, tm->as.lan.diagram,
                                expected, NO, out, error);
    case TERM_RAN:
      return check_check_former(host, c, tm->as.ran.shape, tm->as.ran.diagram,
                                expected, YES, out, error);
    case TERM_LET:
      body.kind = CHECK_BODY_CHECK;
      body.mode = mode;
      body.body = tm->as.let.body;
      body.expected = expected;
      if (!check_let_body(host, c, mode, tm->as.let.name, tm->as.let.ty,
                          tm->as.let.value, &body, &value, error))
        return 0;
      *out = check_inferred_uses(value);
      return 1;
    case TERM_VAR:
    case TERM_UNIV:
    case TERM_OUT:
    case TERM_ANN:
    case TERM_GLOBAL:
    case TERM_LIT:
    case TERM_AUTO:
      break;
  }
  return check_infer_then_check(host, c, mode, tm, expected, out, error);
}

int check_dispatch(const RulesHost *host, const RulesTask *task,
                   RulesAnswer *out, const KError **error) {
  RulesInferred value;
  QuantityUsage uses = quantity_empty();
  Level level;
  Bool eq = NO;
  const Value *v = NULL;
  const Term *t = NULL;
  switch (task->tag) {
    case RULES_INFER_USES:
      if (!check_infer_node(host, task->context, task->as.infer_uses.mode,
                            task->as.infer_uses.term, &value, error))
        return 0;
      *out = check_inferred_answer(value);
      return 1;
    case RULES_CHECK_USES:
      if (!check_check_node(host, task->context, task->as.check_uses.mode,
                            task->as.check_uses.term,
                            task->as.check_uses.expected, &uses, error))
        return 0;
      out->tag = RULES_USAGE_ANSWER;
      out->as.uses = uses;
      return 1;
    case RULES_INFER_UNIVERSE:
      if (!check_infer_request(host, task->context, QUANTITY_ZERO,
                               task->as.infer_universe.term, &value, error))
        return 0;
      if (!check_inferred_universe(host, task->context, value, &level, error))
        return 0;
      out->tag = RULES_LEVEL_ANSWER;
      out->as.level = level;
      return 1;
    case RULES_COMPARE:
      if (!conv_conv_step(host, task->context, task->as.compare.ty,
                          task->as.compare.left, task->as.compare.right, &eq,
                          error))
        return 0;
      out->tag = RULES_BOOL_ANSWER;
      out->as.equal = eq;
      return 1;
    case RULES_EVALUATE:
      if (!eval_eval(task->context.check->globals, task->as.evaluate.env,
                     task->as.evaluate.term, &v, error))
        return 0;
      out->tag = RULES_VALUE_ANSWER;
      out->as.value = v;
      return 1;
    case RULES_WEAK:
      if (!eval_whnf(task->context.check->globals, task->as.weak.value, &v,
                     error))
        return 0;
      out->tag = RULES_VALUE_ANSWER;
      out->as.value = v;
      return 1;
    case RULES_READBACK:
      break;
  }
  if (!eval_quote(task->context.check->globals, task->as.readback.size,
                  task->as.readback.value, &t, error))
    return 0;
  out->tag = RULES_TERM_ANSWER;
  out->as.term = t;
  return 1;
}

/* The Request arm of drive (lines 404-405): poll on the fuel cell; on Fail
   the fuel stays (polled), else the cell takes the spent budget and the
   check dispatch serves the task. */
int check_request(const RulesHost *host, const RulesTask *task,
                  RulesAnswer *answer, const KError **error) {
  Budget next;
  if (!check_poll(task, *host->fuel, &next, error)) return 0;
  *host->fuel = next;
  return check_dispatch(host, task, answer, error);
}

/* drive with dispatch: a check host on the stack over one fuel cell. */
static RulesHost check_host(Budget *fuel) {
  RulesHost host;
  host.kind = RULES_HOST_CHECK;
  host.fuel = fuel;
  return host;
}

/* infer_with_budget (line 411): *remaining is the fuel cell. */
int check_infer_with_budget(const CheckCtx *c, Quantity mode, const Term *tm,
                            const Value **out, Budget *remaining,
                            const KError **error) {
  RulesHost host = check_host(remaining);
  RulesInferred value;
  *remaining = c->budget;
  if (!check_infer_request(&host, ctx_of(c), quantity_runtime(mode), tm,
                           &value, error))
    return 0;
  *out = check_inferred_type(value);
  return 1;
}

int check_infer(const CheckCtx *c, Quantity mode, const Term *tm,
                const Value **out, const KError **error) {
  Budget remaining = c->budget;
  return check_infer_with_budget(c, mode, tm, out, &remaining, error);
}

/* check_with_budget (line 416): *remaining is the fuel cell. */
int check_check_with_budget(const CheckCtx *c, Quantity mode, const Term *tm,
                            const Value *expected, Budget *remaining,
                            const KError **error) {
  RulesHost host = check_host(remaining);
  QuantityUsage uses = quantity_empty();
  *remaining = c->budget;
  return check_check_request(&host, ctx_of(c), quantity_runtime(mode), tm,
                             expected, &uses, error);
}

int check_check(const CheckCtx *c, Quantity mode, const Term *tm,
                const Value *expected, const KError **error) {
  Budget remaining = c->budget;
  return check_check_with_budget(c, mode, tm, expected, &remaining, error);
}

int check_infer_univ(const CheckCtx *c, const Term *tm, Level *out,
                     const KError **error) {
  Budget fuel = c->budget;
  RulesHost host = check_host(&fuel);
  return check_univ_request(&host, ctx_of(c), tm, out, error);
}

int check_conv_type(const CheckCtx *c, const Value *a, const Value *b,
                    Bool *out, const KError **error) {
  Budget fuel = c->budget;
  RulesHost host = check_host(&fuel);
  return rules_ask_type(&host, ctx_of(c), a, b, out, error);
}

int check_infer_term(const Globals *globals, Budget budget, const Term *tm,
                     const Value **out, const KError **error) {
  return check_infer(check_make(globals, budget), QUANTITY_MANY, tm, out,
                     error);
}

int check_check_term(const Globals *globals, Budget budget, const Term *tm,
                     const Value *ty, const KError **error) {
  return check_check(check_make(globals, budget), QUANTITY_MANY, tm, ty,
                     error);
}

int check_check_decl(const Globals *g, Budget b, const CheckDecl *d,
                     const GlobalEntry **out, const KError **error) {
  Budget fuel = b;
  RulesHost host = check_host(&fuel);
  return check_check_decl_program(&host, ctx_of(check_make(g, b)), d, out,
                                  error);
}

int check_check_decls(const Globals *g, Budget b, const CheckDecls *ds,
                      const CheckNamedEntries **out, const KError **error) {
  Budget fuel = b;
  RulesHost host = check_host(&fuel);
  return check_declarations_program(&host, ds, g, b, out, error);
}

int check_declare_family(const Globals *g, Budget b, const CheckFamilyDecl *d,
                         const Globals **out, const KError **error) {
  Budget fuel = b;
  RulesHost host = check_host(&fuel);
  return check_declare_family_program(&host, g, b, d, out, error);
}

int check_define_ctors(const Globals *g, Budget b, const Texts *group,
                       Text name, const CheckCtorDecls *cds,
                       const Globals **out, const KError **error) {
  Budget fuel = b;
  RulesHost host = check_host(&fuel);
  return check_define_ctors_program(&host, g, b, group, name, cds, out, error);
}
