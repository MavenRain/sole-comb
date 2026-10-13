/* lib/surface_elab.c: twin of surface/elab.bend (S6 sessions 2b to 2c).
   M.result(A) is the int return (1 = Pure with *out, 0 = Abort with
   *error); a Unit result has no out. The walker ~go is the (walk_env, go)
   pair of SurfaceElabWalk; go(c, e, s) is a direct call. M.bind and
   M.map_result are C if statements and M.lift is the identity. choose,
   need, maybe, using and the with_* matchers are C code at the use site
   (no twin). run_check is a rules call on ctx_of(c); run_eval is a rules
   call on an evaluation host on the stack (no fuel). A Maybe of a boxed
   value is NULL; a Maybe of a struct is an int return (1 = Some) with
   *out. */
#include "sole.h"

/* Syn.cat: String.concat of a fixed list of parts. */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

/* The RulesCtx of the ops calls (as surface_constructor_infer.c ctx_of). */
static RulesCtx ctx_of(const CheckCtx *c) {
  RulesCtx context;
  context.check = c;
  return context;
}

/* The drive of line 637 (S6 session 2c): elab_node is the elaboration of
   one node with the walker go; node_walk(walk_env, ...) is
   elab_node(walk_env, request, ...). */
static int elab_node(const void *walk_env, SurfaceElabWalk go,
                     const CheckCtx *c, const Value *expected,
                     const SurfaceTerm *s, const Term **out,
                     const KError **error);
static int node_walk(const void *walk_env, const CheckCtx *c,
                     const Value *expected, const SurfaceTerm *s,
                     const Term **out, const KError **error);

/* no_expect (line 38). */
static const KError *no_expect(Text what) {
  Text parts[2];
  parts[0] = what;
  parts[1] = text_of_cstring(" needs an expected type");
  return error_cannot_infer(cat(parts, 2));
}

/* length (line 39) at T.t: a loop of int63_succ (the count of a list,
   thus no overflow). */
static Int63 length_terms(const Terms *xs) {
  Int63 n = int63_zero();
  for (; xs != NULL; xs = xs->tail) n = int63_succ(n);
  return n;
}

/* map(leg_of) (lines 51 and 69): leg_of is T.Leg{[], tm}; a loop plus a
   reverse. */
static const TermLegs *legs_of(const Terms *items) {
  const TermLegs *acc = NULL;
  for (; items != NULL; items = items->tail)
    acc = TermLegs_cons(term_leg(NULL, items->head), acc);
  return TermLegs_reverse(acc);
}

/* eval_in (line 64). */
static int eval_in(const CheckCtx *c, const Term *tm, const Value **out,
                   const KError **error) {
  return eval_eval(c->globals, c->env, tm, out, error);
}

/* whnf_in (line 65). */
static int whnf_in(const CheckCtx *c, const Value *value, const Value **out,
                   const KError **error) {
  return eval_whnf(c->globals, value, out, error);
}

/* quote_in (line 66). */
static int quote_in(const CheckCtx *c, const Value *value, const Term **out,
                    const KError **error) {
  return eval_quote(c->globals, c->size, value, out, error);
}

/* collect (line 55) at eval_in: a loop plus a reverse. The terms evaluate
   in list order, so the first refusal is the same as the bind chain. */
static int collect_eval(const CheckCtx *c, const Terms *xs,
                        const Values **out, const KError **error) {
  const Values *acc = NULL;
  for (; xs != NULL; xs = xs->tail) {
    const Value *v;
    if (!eval_in(c, xs->head, &v, error)) return 0;
    acc = Values_cons(v, acc);
  }
  *out = Values_reverse(acc);
  return 1;
}

/* collect (line 55) at quote_in: a loop plus a reverse. */
static int collect_quote(const CheckCtx *c, const Values *xs,
                         const Terms **out, const KError **error) {
  const Terms *acc = NULL;
  for (; xs != NULL; xs = xs->tail) {
    const Term *tm;
    if (!quote_in(c, xs->head, &tm, error)) return 0;
    acc = Terms_cons(tm, acc);
  }
  *out = Terms_reverse(acc);
  return 1;
}

/* mu_ref (line 70). */
typedef enum { MU_REF_RFAM, MU_REF_RCTOR } MuRefTag;
typedef struct {
  MuRefTag tag;
  const Family *family;
  const Ctor *ctor; /* RCtor only */
} MuRef;

/* spine_of (line 73): the self tail call is a loop. The head is the
   return; the arguments go to *args_out. */
static const SurfaceTerm *spine_of(const SurfaceTerm *s,
                                   const SurfaceTerms *args,
                                   const SurfaceTerms **args_out) {
  while (s->tag == SURFACE_SAPP) {
    args = SurfaceTerms_cons(s->as.app.argument, args);
    s = s->as.app.function;
  }
  *args_out = args;
  return s;
}

/* bound (line 80). */
typedef struct {
  Quantity quantity;
  Text name;
  const Term *ty;
  const CheckCtx *context;
} Bound;

/* coll_motive_plain (line 82). */
static int coll_motive_plain(const SurfaceMotive *m, const KError **error) {
  if (!m->ind.some) return 1;
  *error = error_mismatch(text_append(
      text_of_cstring("a case at a collection takes no index clause and its "
                      "motive names "),
      m->ind.text));
  return 0;
}

/* bound_values (line 88). */
typedef struct {
  const CheckCtx *context;
  const Values *env;
  const Values *values;
} BoundValues;

/* family_indices (line 90). */
static const Telescope *family_indices(const Family *fam) {
  return fam->indices;
}

/* telescope_result (line 100). */
typedef struct {
  const Telescope *entries;
  const CheckCtx *context;
} TelescopeResult;

/* chain_result (line 102). */
typedef struct {
  const Telescope *entries;
  const SurfaceTerm *result;
  const CheckCtx *context;
} ChainResult;

/* universe_of (line 104). */
static int universe_of(const Term *tm, Level *out, const KError **error) {
  if (tm->tag == TERM_UNIV) {
    *out = tm->as.univ.level;
    return 1;
  }
  *error = error_universe(
      text_of_cstring("the header of a family ends in a universe"));
  return 0;
}

/* eval_environment (line 132): the pair {globals, env} is two
   parameters. */
static int eval_environment(const Globals *globals, const Values *env,
                            const Term *tm, const Value **out,
                            const KError **error) {
  return eval_eval(globals, env, tm, out, error);
}

/* collect (line 55) at eval_environment: a loop plus a reverse. */
static int collect_eval_environment(const Globals *globals,
                                    const Values *env, const Terms *xs,
                                    const Values **out,
                                    const KError **error) {
  const Values *acc = NULL;
  for (; xs != NULL; xs = xs->tail) {
    const Value *v;
    if (!eval_environment(globals, env, xs->head, &v, error)) return 0;
    acc = Values_cons(v, acc);
  }
  *out = Values_reverse(acc);
  return 1;
}

/* collection_environment (line 135). */
typedef struct {
  const CheckCtx *context;
  const ValueShape *shape;
  const VLegs *legs;
  const TermMotive *motive; /* NULL = None */
  const Value *expected;    /* NULL = None */
} CollectionEnv;

/* family_environment (line 137). */
typedef struct {
  const CheckCtx *context;
  Text name;
  const Family *family;
  const TermMotive *motive; /* NULL = None */
  const Values *params;
  const Value *expected; /* NULL = None */
} FamilyEnv;

/* request (line 139): itself a SurfaceElabWalk. M.request drives the
   walker node_walk. */
static int request(const void *walk_env, const CheckCtx *c,
                   const Value *expected, const SurfaceTerm *s,
                   const Term **out, const KError **error) {
  return surface_elab_program_request(walk_env, node_walk, c, expected, s,
                                      out, error);
}

/* index_of (line 141): a loop (1 = Some with *out); index_next is
   i = int63_succ(i) (the count of a list, thus no overflow). */
static int index_of(Text x, const Texts *names, Int63 *out) {
  Int63 i = int63_zero();
  for (; names != NULL; names = names->tail) {
    if (text_same(x, names->head)) {
      *out = i;
      return 1;
    }
    i = int63_succ(i);
  }
  return 0;
}

/* qualifier (line 147): a loop with a U32 count n of the code points
   before the first '.' (46); n is less than the size of the name. */
static MaybeText qualifier(Text name) {
  Text rest = name;
  U32 n = 0;
  while (!text_is_end(rest)) {
    if (text_head(rest) == 46) return maybe_text_some(text_take(name, n));
    rest = text_tail(rest);
    n++;
  }
  return maybe_text_none();
}

/* local_projection (line 153). */
static Bool local_projection(const CheckCtx *c, Text x) {
  MaybeText q = qualifier(x);
  Int63 i;
  if (!q.some) return NO;
  return index_of(q.text, check_local_names(check_locals(c)), &i) ? YES
                                                                  : NO;
}

/* type_of (line 155). */
static int type_of(const CheckCtx *c, const Term *tm, const Value **out,
                   const KError **error) {
  const Value *value;
  if (!check_infer(c, QUANTITY_ZERO, tm, &value, error)) return 0;
  return whnf_in(c, value, out, error);
}

/* open_in (line 156): run_eval, so an evaluation host on the stack (as
   erase_type_opened). */
static int open_in(const CheckCtx *c, const Closure *clo, const Values *args,
                   const Value **out, const KError **error) {
  RulesHost host;
  RulesCtx ctx;
  RulesEvaluator ev;
  host.kind = RULES_HOST_EVAL;
  host.fuel = NULL;
  ctx.globals = c->globals;
  ev = eval_ev(ctx);
  return rules_open_closure(&host, &ev, clo, args, out, error);
}

/* univ_of_int (line 157). */
static int univ_of_int(Int63 n, const Term **out, const KError **error) {
  Level level;
  if (!level_of_int(n, &level)) {
    Text parts[3];
    parts[0] = text_of_cstring("the universe level ");
    parts[1] = int63_show(n);
    parts[2] = text_of_cstring(" is not a level");
    *error = error_universe(cat(parts, 3));
    return 0;
  }
  *out = term_univ(level);
  return 1;
}

/* sum_ty (line 159). */
static const Term *sum_ty(const Terms *items) {
  return term_lan(shape_scoll_term(length_terms(items)),
                  term_sec(shape_scoll_term(length_terms(items)),
                           legs_of(items)));
}

/* prod_ty (line 160). */
static const Term *prod_ty(const Terms *items) {
  return term_ran(shape_scoll_term(length_terms(items)),
                  term_sec(shape_scoll_term(length_terms(items)),
                           legs_of(items)));
}

/* none_list (line 163, @unsafe): a loop; a positive n decreases to zero,
   so int63_pred does not wrap. The list has n None entries (NULL). */
static const Values *none_list(Int63 n) {
  const Values *acc = NULL;
  while (!order_le(int63_compare(n, int63_zero()))) {
    acc = Values_cons(NULL, acc);
    n = int63_pred(n);
  }
  return acc;
}

/* expectations_loop (line 166): a loop plus a reverse; i = int63_succ(i)
   is less than the count of items plus the start. */
static int expectations_loop(const CheckCtx *c, const VLegs *legs,
                             const Values *items, Int63 i,
                             const Values **out, const KError **error) {
  const Values *acc = NULL;
  for (; items != NULL; items = items->tail) {
    const Value *ty;
    if (!rules_coll_leg_ty(ctx_of(c), legs, i, &ty, error)) return 0;
    acc = Values_cons(ty, acc);
    i = int63_succ(i);
  }
  *out = Values_reverse(acc);
  return 1;
}

/* find_ctor_rows (line 172): a loop over the map rows (1 = Some with
   *out). */
static int find_ctor_rows(Text x, const MapBindings *families, MuRef *out) {
  for (; families != NULL; families = families->tail) {
    const Family *fam = families->head.value;
    const Ctor *ct = positivity_ctor_of(x, fam);
    if (ct != NULL) {
      out->tag = MU_REF_RCTOR;
      out->family = fam;
      out->ctor = ct;
      return 1;
    }
  }
  return 0;
}

/* find_ctor (line 178). */
static int find_ctor(Text x, const Map *families, MuRef *out) {
  return find_ctor_rows(x, map_bindings(families), out);
}

/* split_at (line 180) at T.t: a loop plus a reverse. The part before is the
   return; the part after goes to *after. A positive n decreases to zero,
   so int63_pred does not wrap. */
static const Terms *split_at(const Terms *xs, Int63 n, const Terms **after) {
  const Terms *acc = NULL;
  while (xs != NULL && !order_le(int63_compare(n, int63_zero()))) {
    acc = Terms_cons(xs->head, acc);
    xs = xs->tail;
    n = int63_pred(n);
  }
  *after = xs;
  return Terms_reverse(acc);
}

/* arity_is (line 186). */
static int arity_is(Text what, Int63 want, Int63 got,
                    const KError **error) {
  Text parts[5];
  if (int63_equal(want, got)) return 1;
  parts[0] = what;
  parts[1] = text_of_cstring(" takes ");
  parts[2] = int63_show(want);
  parts[3] = text_of_cstring(" arguments and the term gives ");
  parts[4] = int63_show(got);
  *error = error_mismatch(cat(parts, 5));
  return 0;
}

/* fam_split (line 188): always Pure, thus a plain return; the pair
   {params, indices} is two parameters. */
static const Term *fam_split(Text name, Int63 np, const Terms *params,
                             const Terms *indices) {
  return term_lan(shape_smu_term(name, indices),
                  term_sec(shape_scoll_term(np), legs_of(params)));
}

/* leg_projection (line 191). */
static int leg_projection(const Term *scrut, const ValueShape *s, Int63 k,
                          const Term **out, const KError **error) {
  Int63 n;
  if (!rules_collection_view(s, &n)) {
    *error = error_mismatch(
        text_of_cstring("a leg projection needs a collection former"));
    return 0;
  }
  *out = term_out(shape_scoll_term(n), term_aleg(k), scrut);
  return 1;
}

/* elab_binder (line 193). */
static int elab_binder(const void *walk_env, SurfaceElabWalk go,
                       const CheckCtx *c, SurfaceBinder b, Bound *out,
                       const KError **error) {
  const Term *tm;
  const Value *v;
  if (!go(walk_env, c, NULL, b.ty, &tm, error)) return 0;
  if (!eval_in(c, tm, &v, error)) return 0;
  out->quantity = b.q;
  out->name = b.name;
  out->ty = tm;
  out->context = runtime_ops_bind(b.name, b.q, v, c);
  return 1;
}

/* elab_motive (line 208): *out NULL = None. */
static int elab_motive(const void *walk_env, SurfaceElabWalk go,
                       const CheckCtx *c, const SurfaceMotive *mo,
                       const Value *ty, const TermMotive **out,
                       const KError **error) {
  const Term *tm;
  if (mo == NULL) {
    *out = NULL;
    return 1;
  }
  if (!coll_motive_plain(mo, error)) return 0;
  if (!go(walk_env, runtime_ops_bind(mo->self, QUANTITY_ZERO, ty, c), NULL,
          mo->body, &tm, error))
    return 0;
  *out = term_motive(maybe_text_none(), NULL, mo->self, tm);
  return 1;
}

/* branch_target (line 215): *out NULL = None. */
static int branch_target(const CheckCtx *c, const TermMotive *mo,
                         const Value *expected, const Value *self,
                         const Value **out, const KError **error) {
  if (mo == NULL) {
    *out = expected;
    return 1;
  }
  return rules_elim_result(ctx_of(c), mo, expected, self, out, error);
}

/* mu_branch_target (line 219): *out NULL = None. */
static int mu_branch_target(const CheckCtx *c, const TermMotive *mo,
                            const Value *expected, const Values *indices,
                            const Value *self, const Value **out,
                            const KError **error) {
  if (mo == NULL) {
    *out = expected;
    return 1;
  }
  return rules_mu_result(ctx_of(c), mo, indices, self, out, error);
}

/* bind_indices (line 223): the zip pair list (telescope entry, index
   name) is two parallel lists of the same length; the self tail call is a
   loop. v is the variable at the size before the bind. */
static int bind_indices(const Telescope *entries, const Texts *names,
                        BoundValues state, BoundValues *out,
                        const KError **error) {
  for (; entries != NULL && names != NULL;
       entries = entries->tail, names = names->tail) {
    const Value *tv;
    const Value *v;
    if (!eval_eval(state.context->globals, state.env,
                   entries->head.binding.type, &tv, error))
      return 0;
    v = value_var(state.context->size);
    state.context = runtime_ops_bind(names->head, QUANTITY_ZERO, tv,
                                     state.context);
    state.env = Values_cons(v, state.env);
    state.values = Values_cons(v, state.values);
  }
  *out = state;
  return 1;
}

/* check_field_annotation (line 232). */
static int check_field_annotation(const void *walk_env, SurfaceElabWalk go,
                                  const CheckCtx *c, const Value *expected,
                                  SurfaceField field, const KError **error) {
  const Term *tm;
  Level level;
  const Value *actual;
  Bool agrees;
  Text parts[3];
  if (field.ty == NULL) return 1;
  if (!go(walk_env, c, NULL, field.ty, &tm, error)) return 0;
  if (!check_infer_univ(c, tm, &level, error)) return 0;
  if (!eval_in(c, tm, &actual, error)) return 0;
  if (!check_conv_type(c, actual, expected, &agrees, error)) return 0;
  if (agrees) return 1;
  parts[0] = text_of_cstring("the annotation of constructor field ");
  parts[1] = field.name;
  parts[2] = text_of_cstring(" differs from its declared type");
  *error = error_mismatch(cat(parts, 3));
  return 0;
}

/* item_elab (line 243): the pair {item, expected} is two parameters. */
static int item_elab(const void *walk_env, SurfaceElabWalk go,
                     const CheckCtx *c, const SurfaceTerm *item,
                     const Value *expected, const Term **out,
                     const KError **error) {
  return go(walk_env, c, expected, item, out, error);
}

/* mu_result (line 258): the two mismatch arms (the same message) are one
   test. The pair {params, indices} is two outs. R.run with K.dispatch:
   rules_mu_params_of has no host. */
static int mu_result(Text name, const Term *tm, const Terms **params,
                     const Terms **indices, const KError **error) {
  Text parts[3];
  if (tm->tag == TERM_LAN && tm->as.lan.shape->tag == SHAPE_SMU &&
      text_same(tm->as.lan.shape->as.smu.family, name)) {
    if (!rules_mu_params_of(tm->as.lan.diagram, params, error)) return 0;
    *indices = tm->as.lan.shape->as.smu.indices;
    return 1;
  }
  parts[0] = text_of_cstring("a constructor of ");
  parts[1] = name;
  parts[2] = text_of_cstring(" ends at another type");
  *error = error_mismatch(cat(parts, 3));
  return 0;
}

/* elab_univ (line 265). */
static int elab_univ(const CheckCtx *c, const SurfaceTerm *s, Level *out,
                     const KError **error) {
  const Term *tm;
  if (!request(NULL, c, NULL, s, &tm, error)) return 0;
  return universe_of(tm, out, error);
}

/* elab_decl_program (line 267): CheckDecl by value in *out. */
static int elab_decl_program(const CheckCtx *c, const SurfaceDecl *d,
                             CheckDecl *out, const KError **error) {
  switch (d->tag) {
  case SURFACE_DDEF: {
    const Term *tyt;
    const Value *tyv;
    const Term *tm;
    if (!request(NULL, c, NULL, d->as.def.ty, &tyt, error)) return 0;
    if (!eval_in(c, tyt, &tyv, error)) return 0;
    if (!request(NULL, c, tyv, d->as.def.body, &tm, error)) return 0;
    out->name = d->as.def.name;
    out->kind = CHECK_DEFINITION;
    out->ty = tyt;
    out->body = tm;
    return 1;
  }
  case SURFACE_DAXIOM: {
    const Term *tm;
    if (!request(NULL, c, NULL, d->as.axiom.ty, &tm, error)) return 0;
    out->name = d->as.axiom.name;
    out->kind = CHECK_POSTULATE;
    out->ty = tm;
    out->body = NULL;
    return 1;
  }
  case SURFACE_DMU:
    *error = error_mismatch(
        text_of_cstring("a mu group declares no entry of its own"));
    return 0;
  case SURFACE_DREC:
    *error = error_mismatch(
        text_of_cstring("a recursive group is elaborated as a group"));
    return 0;
  }
  return 0;
}

/* former_expectations (line 276): value NULL = None. The three none_list(n)
   arms (no former, no collection view, dn != n) are one test. */
static int former_expectations(const CheckCtx *c, Int63 n,
                               const ValueFormer *value, const Values **out,
                               const KError **error) {
  Int63 dn;
  const VLegs *legs;
  if (value == NULL || !rules_collection_view(value->shape, &dn) ||
      !int63_equal(dn, n)) {
    *out = none_list(n);
    return 1;
  }
  if (!rules_coll_legs_of(ctx_of(c), value->diagram, &legs, error)) return 0;
  return expectations_loop(c, legs, none_list(n), int63_zero(), out, error);
}

/* family_ctor (line 284): 1 = Some with *out. */
static int family_ctor(Text x, const Globals *g, MuRef *out) {
  return find_ctor(x, g->families, out);
}

/* local_or_global (line 287). */
static const Term *local_or_global(const CheckCtx *c, Text x) {
  Int63 i;
  return index_of(x, check_local_names(check_locals(c)), &i)
             ? term_var(i)
             : term_global(x);
}

/* fam_term (line 289): always Pure, thus a plain return. */
static const Term *fam_term(Text name, Int63 np, const Terms *terms) {
  const Terms *after;
  const Terms *before = split_at(terms, np, &after);
  return fam_split(name, np, before, after);
}

/* The closure ixs => ... of elab_ctor_ref (lines 298 to 301): run_env is
   {c, terms}. */
typedef struct {
  const CheckCtx *context;
  const Terms *terms;
} CtorRefRun;

static int ctor_ref_run(const void *run_env, const Terms *ixs,
                        const Terms **out, const KError **error) {
  const CtorRefRun *run = run_env;
  const Values *values;
  const Values *ixv;
  if (!collect_eval(run->context, run->terms, &values, error)) return 0;
  if (!collect_eval_environment(run->context->globals,
                                Values_reverse(values), ixs, &ixv, error))
    return 0;
  return collect_quote(run->context, ixv, out, error);
}

/* elab_ctor_ref (line 291): length(params) = 0 is params NULL. */
static int elab_ctor_ref(const void *walk_env, SurfaceElabWalk go,
                         const CheckCtx *c, const Family *fam,
                         const Ctor *ctor, const SurfaceTerms *args,
                         const Term **out, const KError **error) {
  const Terms *terms;
  const Terms *ix;
  CtorRefRun run;
  if (fam->params != NULL) {
    *error = no_expect(
        text_append(text_of_cstring("the constructor "), ctor->name));
    return 0;
  }
  if (!surface_constructor_arguments(walk_env, go, ctor->args, c, NULL, args,
                                     &terms, error))
    return 0;
  run.context = c;
  run.terms = terms;
  if (!surface_constructor_index_terms(ctor->res_idx, &run, ctor_ref_run,
                                       &ix, error))
    return 0;
  *out = term_in(shape_smu_term(fam->name, ix), term_actor(ctor->name),
                 terms);
  return 1;
}

/* elab_pair (line 303): expected NULL = None. */
static int elab_pair(const void *walk_env, SurfaceElabWalk go,
                     const CheckCtx *c, const Value *expected,
                     const SurfaceTerm *a, const SurfaceTerm *b,
                     const Term **out, const KError **error) {
  const Value *w;
  ValueFormer former;
  RulesPoint point;
  const Term *pt;
  const Value *pv;
  const Value *cod;
  const Term *fibre;
  const Term *dt;
  if (expected == NULL) {
    *error = no_expect(text_of_cstring("a pair"));
    return 0;
  }
  if (!whnf_in(c, expected, &w, error)) return 0;
  if (!value_as_lan(w, &former)) {
    *error = error_mismatch(
        text_of_cstring("a pair needs a left former as its expected type"));
    return 0;
  }
  if (!rules_point_view(former.shape, &point)) {
    *error = error_mismatch(text_of_cstring("a pair needs a point former"));
    return 0;
  }
  if (!go(walk_env, c, point.domain, a, &pt, error)) return 0;
  if (!eval_in(c, pt, &pv, error)) return 0;
  if (!open_in(c, former.diagram, Values_cons(pv, NULL), &cod, error))
    return 0;
  if (!go(walk_env, c, cod, b, &fibre, error)) return 0;
  if (!quote_in(c, point.domain, &dt, error)) return 0;
  *out = term_in(shape_spi_term(point.quantity, point.name, dt),
                 term_apt(point.quantity, pt), Terms_cons(fibre, NULL));
  return 1;
}

/* pair_projection (line 313): second is k = 2 (int63_succ(int63_one), no
   overflow); the motive quote is at int63_succ(c->size) (a context size,
   no overflow). The elim_value of .2 is run_eval on an evaluation host. */
static int pair_projection(const CheckCtx *c, const Term *scrut,
                           const ValueShape *s, const Closure *clo, Int63 k,
                           const Term **out, const KError **error) {
  RulesPoint point;
  Bool second;
  const Value *body;
  const Term *motive;
  const Term *dt;
  if (!rules_point_view(s, &point)) {
    *error = error_mismatch(
        text_of_cstring("a pair projection needs a point former"));
    return 0;
  }
  second = int63_equal(k, int63_succ(int63_one()));
  if (!int63_equal(k, int63_one()) && !second) {
    *error = error_wrong_leg(
        text_of_cstring("a pair carries the projections .1 and .2 only"));
    return 0;
  }
  if (second) {
    RulesHost host;
    RulesCtx ctx;
    RulesEvaluator ev;
    const Value *pt;
    host.kind = RULES_HOST_EVAL;
    host.fuel = NULL;
    ctx.globals = c->globals;
    ev = eval_ev(ctx);
    if (!rules_elim_value(&host, &ev,
                          shape_spi_value(point.quantity, point.name,
                                          point.domain),
                          point.quantity, NULL,
                          rules_proj_branch(point.quantity, NO), c->env,
                          value_var(c->size), &pt, error))
      return 0;
    if (!open_in(c, clo, Values_cons(pt, NULL), &body, error)) return 0;
  } else {
    body = point.domain;
  }
  if (!eval_quote(c->globals, int63_succ(c->size), body, &motive, error))
    return 0;
  if (!quote_in(c, point.domain, &dt, error)) return 0;
  *out = term_elim(term_elimination(
      shape_spi_term(point.quantity, point.name, dt), scrut, QUANTITY_ONE,
      term_motive(maybe_text_none(), NULL, text_of_cstring("self"), motive),
      rules_proj_branch(point.quantity, second)));
  return 1;
}

/* Part 2 (Bend lines 323 to 640), S6 session 2d. */

/* An M.Abort: store the error, answer 0. */
static int abort_with(const KError *e, const KError **error) {
  *error = e;
  return 0;
}

/* run_check of K.ensure (the shape of check_conv_type, kernel_check.c). */
static int ensure_in(const CheckCtx *c, const Value *got,
                     const Value *expected, const KError **error) {
  Budget fuel = c->budget;
  RulesHost host;
  host.kind = RULES_HOST_CHECK;
  host.fuel = &fuel;
  return check_ensure(&host, ctx_of(c), got, expected, error);
}

static Int63 length_texts(const Texts *xs) {
  Int63 n = int63_zero();
  for (; xs != NULL; xs = xs->tail) n = int63_succ(n);
  return n;
}

static Int63 length_tele(const Telescope *xs) {
  Int63 n = int63_zero();
  for (; xs != NULL; xs = xs->tail) n = int63_succ(n);
  return n;
}

static Int63 length_fields(const SurfaceFields *xs) {
  Int63 n = int63_zero();
  for (; xs != NULL; xs = xs->tail) n = int63_succ(n);
  return n;
}

static Int63 length_surface(const SurfaceTerms *xs) {
  Int63 n = int63_zero();
  for (; xs != NULL; xs = xs->tail) n = int63_succ(n);
  return n;
}

/* R.one_of (rules line 54): Some only for exactly one entry. */
static int one_binder(const SurfaceBinders *bs, SurfaceBinder *out) {
  if (bs == NULL || bs->tail != NULL) return 0;
  *out = bs->head;
  return 1;
}

static int one_branch(const SurfaceBranches *bs, SurfaceBranch *out) {
  if (bs == NULL || bs->tail != NULL) return 0;
  *out = bs->head;
  return 1;
}

/* joined (line 96) of R.at (rules line 258): NULL for an index past the
   end and for a None entry. */
static const Value *joined_at(Int63 k, const Values *tys) {
  Int63 i = int63_zero();
  for (; tys != NULL; tys = tys->tail) {
    if (int63_equal(i, k)) return tys->head;
    i = int63_succ(i);
  }
  return NULL;
}

/* map field_binder (line 93) over the fields (line 443). */
static const TermBinders *field_binders(const SurfaceFields *fields) {
  TermBinder b;
  if (fields == NULL) return NULL;
  b.quantity = fields->head.q;
  b.name = fields->head.name;
  return TermBinders_cons(b, field_binders(fields->tail));
}

/* map fam_name (line 129) over the fams (lines 586 and 587). */
static const Texts *fam_names(const SurfaceFams *fams) {
  if (fams == NULL) return NULL;
  return Texts_cons(fams->head.name, fam_names(fams->tail));
}

/* collect of go with no expectation (lines 607 and 608). */
static int collect_walk(const void *walk_env, SurfaceElabWalk go,
                        const CheckCtx *c, const SurfaceTerms *items,
                        const Terms **out, const KError **error) {
  const Terms *acc = NULL;
  for (; items != NULL; items = items->tail) {
    const Term *tm;
    if (!go(walk_env, c, NULL, items->head, &tm, error)) return 0;
    acc = Terms_cons(tm, acc);
  }
  *out = Terms_reverse(acc);
  return 1;
}

static TermBinder binder_of(Quantity q, Text name) {
  TermBinder b;
  b.quantity = q;
  b.name = name;
  return b;
}

static TeleEntry tele_entry(const Bound *bound) {
  TeleEntry e;
  e.quantity = bound->quantity;
  e.binding.name = bound->name;
  e.binding.type = bound->ty;
  return e;
}

/* [V.var(level)]. */
static const Values *var_list(Int63 level) {
  return Values_cons(value_var(level), NULL);
}

static const KError *needs_mu(void) {
  return error_mismatch(text_of_cstring(
      "a match needs a mu family as the type of its scrutinee"));
}

/* app_point (line 323). */
static int app_point(const void *walk_env, SurfaceElabWalk go,
                     const CheckCtx *c, const SurfaceTerm *f,
                     const SurfaceTerm *a, const Term **out,
                     const KError **error) {
  const Term *head;
  const Value *w;
  ValueFormer former;
  RulesPoint point;
  const Term *arg;
  const Term *dt;
  if (!go(walk_env, c, NULL, f, &head, error)) return 0;
  if (!type_of(c, head, &w, error)) return 0;
  if (!value_as_ran(w, &former) || !rules_point_view(former.shape, &point))
    return abort_with(error_mismatch(text_of_cstring(
                          "the head of an application is not a function")),
                      error);
  if (!go(walk_env, c, point.domain, a, &arg, error)) return 0;
  if (!quote_in(c, point.domain, &dt, error)) return 0;
  *out = term_out(shape_spi_term(point.quantity, point.name, dt),
                  term_apt(point.quantity, arg), head);
  return 1;
}

/* binder_domain (line 330). */
static int binder_domain(const CheckCtx *c, Text x, const Term *ann,
                         const Value *dom, const KError **error) {
  const Value *actual;
  Bool agrees;
  Text parts[6];
  if (!eval_in(c, ann, &actual, error)) return 0;
  if (!check_conv_type(c, actual, dom, &agrees, error)) return 0;
  if (agrees) return 1;
  parts[0] = text_of_cstring("the binder ");
  parts[1] = x;
  parts[2] = text_of_cstring(" is annotated with ");
  parts[3] = check_ops_pp(c, actual);
  parts[4] = text_of_cstring(" and the expected domain is ");
  parts[5] = check_ops_pp(c, dom);
  return abort_with(error_mismatch(cat(parts, 6)), error);
}

/* cod_former (line 332): *out NULL = None (no former, no point view). */
static int cod_former(const CheckCtx *c, Text x, const Term *ann,
                      const ValueFormer *former, const Value **out,
                      const KError **error) {
  RulesPoint point;
  *out = NULL;
  if (former == NULL) return 1;
  if (!rules_point_view(former->shape, &point)) return 1;
  if (!binder_domain(c, x, ann, point.domain, error)) return 0;
  return open_in(c, former->diagram, var_list(c->size), out, error);
}

/* elab_group (line 338). */
static int elab_group(const void *walk_env, SurfaceElabWalk go,
                      const CheckCtx *c, SurfaceBinder b,
                      const SurfaceTerm *cod, Bool left, const Term **out,
                      const KError **error) {
  Bound bound;
  const Term *tm;
  const TermShape *shape;
  if (!elab_binder(walk_env, go, c, b, &bound, error)) return 0;
  if (!go(walk_env, bound.context, NULL, cod, &tm, error)) return 0;
  shape = shape_spi_term(bound.quantity, bound.name, bound.ty);
  if (left) {
    *out = term_lan(shape, tm);
  } else {
    *out = term_ran(shape, tm);
  }
  return 1;
}

/* checked_payload (line 341). */
static int checked_payload(const void *walk_env, SurfaceElabWalk go,
                           const CheckCtx *c, const Value *ty,
                           const SurfaceTerm *annotation,
                           const KError **error) {
  const Term *tm;
  const Value *declared;
  if (!go(walk_env, c, NULL, annotation, &tm, error)) return 0;
  if (!eval_in(c, tm, &declared, error)) return 0;
  return ensure_in(c, declared, ty, error);
}

/* elab_arm (line 349): a native function-valued arm. */
static int elab_arm(const void *walk_env, SurfaceElabWalk go,
                    const CheckCtx *c, const ValueShape *s,
                    const VLegs *legs, const TermMotive *mo,
                    const Value *expected, Int63 k, const SurfaceTerm *body,
                    TermBranch *out, const KError **error) {
  Text elim = text_of_cstring("$elim");
  const Value *ty;
  const CheckCtx *hidden;
  const Term *head;
  const Value *w;
  ValueFormer former;
  RulesPoint point;
  const CheckCtx *named;
  const Value *cod;
  const Value *target;
  const Term *dt;
  if (!rules_coll_leg_ty(ctx_of(c), legs, k, &ty, error)) return 0;
  hidden = runtime_ops_bind(elim, QUANTITY_ZERO, ty, c);
  if (!go(walk_env, hidden, NULL, body, &head, error)) return 0;
  if (!type_of(hidden, head, &w, error)) return 0;
  if (!value_as_ran(w, &former) || !rules_point_view(former.shape, &point))
    return abort_with(
        error_mismatch(text_of_cstring("an elim arm must be a function")),
        error);
  named = runtime_ops_bind(point.name, point.quantity, ty, c);
  if (!ensure_in(named, point.domain, ty, error)) return 0;
  if (!open_in(named, former.diagram, var_list(c->size), &cod, error))
    return 0;
  if (!branch_target(c, mo, expected,
                     value_vin(s, value_valeg(k), var_list(c->size)), &target,
                     error))
    return 0;
  if (target != NULL && !ensure_in(named, cod, target, error)) return 0;
  if (!quote_in(named, point.domain, &dt, error)) return 0;
  out->address = term_aleg(k);
  out->leg = term_leg(
      TermBinders_cons(binder_of(point.quantity, elim), NULL),
      term_out(shape_spi_term(point.quantity, point.name, dt),
               term_apt(point.quantity, term_var(int63_zero())), head));
  return 1;
}

/* elab_branch (line 362). */
static int elab_branch(const void *walk_env, SurfaceElabWalk go,
                       const CheckCtx *c, const ValueShape *s,
                       const VLegs *legs, const TermMotive *mo,
                       const Value *expected, const SurfaceBranch *br,
                       TermBranch *out, const KError **error) {
  Int63 k;
  SurfaceBinder b;
  const Value *ty;
  const Value *target;
  const Term *tm;
  switch (br->tag) {
  case SURFACE_BR_DEFAULT:
    return abort_with(error_wrong_leg(text_of_cstring(
                          "a default elim arm must be expanded before branch "
                          "elaboration")),
                      error);
  case SURFACE_BR_CTOR: {
    Text parts[3];
    parts[0] = text_of_cstring("the branch at ");
    parts[1] = br->as.ctor.name;
    parts[2] = text_of_cstring(
        " keys a constructor and a case at a collection keys a leg");
    return abort_with(error_wrong_leg(cat(parts, 3)), error);
  }
  case SURFACE_BR_LEG:
    break;
  }
  k = br->as.leg.key;
  if (br->as.leg.binders == NULL)
    return elab_arm(walk_env, go, c, s, legs, mo, expected, k,
                    br->as.leg.body, out, error);
  if (!one_binder(br->as.leg.binders, &b))
    return abort_with(error_missing_branch(text_of_cstring(
                          "each branch binds its payload once")),
                      error);
  if (!rules_coll_leg_ty(ctx_of(c), legs, k, &ty, error)) return 0;
  if (!checked_payload(walk_env, go, c, ty, b.ty, error)) return 0;
  if (!branch_target(c, mo, expected,
                     value_vin(s, value_valeg(k), var_list(c->size)), &target,
                     error))
    return 0;
  if (!go(walk_env, runtime_ops_bind(b.name, b.q, ty, c), target,
          br->as.leg.body, &tm, error))
    return 0;
  out->address = term_aleg(k);
  out->leg = term_leg(TermBinders_cons(binder_of(b.q, b.name), NULL), tm);
  return 1;
}

/* mu_motive_indices (line 373): *out NULL = None. */
static int mu_motive_indices(const void *walk_env, SurfaceElabWalk go,
                             const CheckCtx *c, Text n,
                             const Telescope *indices, const Closure *clo,
                             const Level *u, const Values *penv,
                             const SurfaceMotive *mo, const TermMotive **out,
                             const KError **error) {
  BoundValues start;
  BoundValues bound;
  const Value *self_ty;
  const Term *tm;
  *out = NULL;
  if (mo == NULL) return 1;
  if (!int63_equal(length_texts(mo->idx), length_tele(indices))) {
    Text parts[6];
    parts[0] = text_of_cstring("the motive of ");
    parts[1] = n;
    parts[2] = text_of_cstring(" binds ");
    parts[3] = int63_show(length_texts(mo->idx));
    parts[4] = text_of_cstring(" index names and the family has ");
    parts[5] = int63_show(length_tele(indices));
    return abort_with(error_mismatch(cat(parts, 6)), error);
  }
  start.context = c;
  start.env = penv;
  start.values = NULL;
  if (!bind_indices(indices, mo->idx, start, &bound, error)) return 0;
  self_ty = value_vlan(shape_smu_value(n, Values_reverse(bound.values)), clo,
                       u);
  if (!go(walk_env,
          runtime_ops_bind(mo->self, QUANTITY_ZERO, self_ty, bound.context),
          NULL, mo->body, &tm, error))
    return 0;
  *out = term_motive(mo->ind, mo->idx, mo->self, tm);
  return 1;
}

/* bind_fields (line 382): the self tail call is a loop; the zip pairs are
   two parallel lists (the caller tests the lengths). */
static int bind_fields(const void *walk_env, SurfaceElabWalk go,
                       const Telescope *entries, const SurfaceFields *fields,
                       BoundValues state, BoundValues *out,
                       const KError **error) {
  for (; entries != NULL && fields != NULL;
       entries = entries->tail, fields = fields->tail) {
    const Value *tv;
    const Value *v;
    if (!eval_environment(check_globals(state.context), state.env,
                          entries->head.binding.type, &tv, error))
      return 0;
    if (!check_field_annotation(walk_env, go, state.context, tv,
                                fields->head, error))
      return 0;
    v = value_var(state.context->size);
    state.context = runtime_ops_bind(fields->head.name, fields->head.q, tv,
                                     state.context);
    state.env = Values_cons(v, state.env);
    state.values = Values_cons(v, state.values);
  }
  *out = state;
  return 1;
}

/* elab_telescope (line 393): C recursion (depth = binder count). */
static int elab_telescope(const SurfaceBinders *bs, const CheckCtx *c,
                          TelescopeResult *out, const KError **error) {
  Bound bound;
  TelescopeResult tail;
  if (bs == NULL) {
    out->entries = NULL;
    out->context = c;
    return 1;
  }
  if (!elab_binder(NULL, request, c, bs->head, &bound, error)) return 0;
  if (!elab_telescope(bs->tail, bound.context, &tail, error)) return 0;
  out->entries = Telescope_cons(tele_entry(&bound), tail.entries);
  out->context = tail.context;
  return 1;
}

/* elab_chain (line 400): C recursion (depth = arrow count); the chain
   stops at a node that is not an SArrow. */
static int elab_chain(const SurfaceTerm *s, const CheckCtx *c,
                      ChainResult *out, const KError **error) {
  Bound bound;
  ChainResult tail;
  if (s->tag != SURFACE_SARROW) {
    out->entries = NULL;
    out->result = s;
    out->context = c;
    return 1;
  }
  if (!elab_binder(NULL, request, c, s->as.arrow.binder, &bound, error))
    return 0;
  if (!elab_chain(s->as.arrow.body, bound.context, &tail, error)) return 0;
  out->entries = Telescope_cons(tele_entry(&bound), tail.entries);
  out->result = tail.result;
  out->context = tail.context;
  return 1;
}

/* leg_expectations (line 407). */
static int leg_expectations(const CheckCtx *c, const Value *expected,
                            Int63 n, Bool left, const Values **out,
                            const KError **error) {
  const Value *w;
  ValueFormer former;
  int some;
  if (expected == NULL) {
    *out = none_list(n);
    return 1;
  }
  if (!whnf_in(c, expected, &w, error)) return 0;
  if (left) {
    some = value_as_lan(w, &former);
  } else {
    some = value_as_ran(w, &former);
  }
  if (!some) return former_expectations(c, n, NULL, out, error);
  return former_expectations(c, n, &former, out, error);
}

/* mu_ref_of (line 412): 1 = Some. */
static int mu_ref_of(const CheckCtx *c, Text x, MuRef *out) {
  Int63 i;
  const Family *fam;
  if (index_of(x, check_local_names(check_locals(c)), &i)) return 0;
  fam = global_find_family(x, check_globals(c));
  if (fam == NULL) return family_ctor(x, check_globals(c), out);
  out->tag = MU_REF_RFAM;
  out->family = fam;
  out->ctor = NULL;
  return 1;
}

/* elab_fam_ref (line 415). */
static int elab_fam_ref(const void *walk_env, SurfaceElabWalk go,
                        const CheckCtx *c, const Family *f,
                        const SurfaceTerms *args, const Term **out,
                        const KError **error) {
  Int63 np = length_tele(f->params);
  const Terms *terms;
  if (!arity_is(f->name, int63_add(np, length_tele(f->indices)),
                length_surface(args), error))
    return 0;
  if (!surface_constructor_arguments(walk_env, go,
                                     Telescope_append(f->params, f->indices),
                                     c, NULL, args, &terms, error))
    return 0;
  *out = fam_term(f->name, np, terms);
  return 1;
}

/* project_type (line 421). */
static int project_type(const CheckCtx *c, const Term *scrut, const Value *w,
                        Int63 k, const Term **out, const KError **error) {
  ValueFormer former;
  if (value_as_lan(w, &former))
    return pair_projection(c, scrut, former.shape, former.diagram, k, out,
                           error);
  if (value_as_ran(w, &former))
    return leg_projection(scrut, former.shape, k, out, error);
  return abort_with(error_mismatch(text_of_cstring(
                        "a projection reads a pair or a tuple, and this "
                        "scrutinee is neither")),
                    error);
}

/* cod_of (line 426): *out NULL = None. */
static int cod_of(const CheckCtx *c, Text x, const Term *ann,
                  const Value *expected, const Value **out,
                  const KError **error) {
  const Value *w;
  ValueFormer former;
  if (expected == NULL) {
    *out = NULL;
    return 1;
  }
  if (!whnf_in(c, expected, &w, error)) return 0;
  if (!value_as_ran(w, &former)) return cod_former(c, x, ann, NULL, out, error);
  return cod_former(c, x, ann, &former, out, error);
}

/* elab_mu_motive (line 430). */
static int elab_mu_motive(const void *walk_env, SurfaceElabWalk go,
                          const CheckCtx *c, Text n, const Family *fam,
                          const Closure *clo, const Level *u,
                          const Values *penv, const SurfaceMotive *mo,
                          const TermMotive **out, const KError **error) {
  return mu_motive_indices(walk_env, go, c, n, family_indices(fam), clo, u,
                           penv, mo, out, error);
}

/* elab_mu_branch (line 432). */
static int elab_mu_branch(const void *walk_env, SurfaceElabWalk go,
                          const CheckCtx *c, Text n, const Family *fam,
                          const TermMotive *mo, const Values *penv,
                          const Value *expected, const SurfaceBranch *br,
                          TermBranch *out, const KError **error) {
  Text name;
  const SurfaceFields *fields;
  const Ctor *ct;
  BoundValues start;
  BoundValues bound;
  const Values *idx;
  const Value *target;
  const Term *tm;
  switch (br->tag) {
  case SURFACE_BR_DEFAULT:
    return abort_with(error_wrong_leg(text_of_cstring(
                          "default elim arms currently require a collection")),
                      error);
  case SURFACE_BR_LEG: {
    Text parts[5];
    parts[0] = text_of_cstring("the branch ");
    parts[1] = int63_show(br->as.leg.key);
    parts[2] = text_of_cstring(" keys a leg and a case at ");
    parts[3] = n;
    parts[4] = text_of_cstring(" keys a constructor");
    return abort_with(error_wrong_leg(cat(parts, 5)), error);
  }
  case SURFACE_BR_CTOR:
    break;
  }
  name = br->as.ctor.name;
  fields = br->as.ctor.fields;
  ct = positivity_ctor_of(name, fam);
  if (ct == NULL) {
    Text parts[3];
    parts[0] = name;
    parts[1] = text_of_cstring(" is not a constructor of ");
    parts[2] = n;
    return abort_with(error_unbound(cat(parts, 3)), error);
  }
  if (!int63_equal(length_fields(fields), length_tele(ct->args))) {
    Text parts[8];
    parts[0] = text_of_cstring("the branch at ");
    parts[1] = name;
    parts[2] = text_of_cstring(" binds ");
    parts[3] = int63_show(length_fields(fields));
    parts[4] = text_of_cstring(" fields and ");
    parts[5] = name;
    parts[6] = text_of_cstring(" takes ");
    parts[7] = int63_show(length_tele(ct->args));
    return abort_with(error_missing_branch(cat(parts, 8)), error);
  }
  start.context = c;
  start.env = penv;
  start.values = NULL;
  if (!bind_fields(walk_env, go, ct->args, fields, start, &bound, error))
    return 0;
  if (!collect_eval_environment(check_globals(c), bound.env, ct->res_idx,
                                &idx, error))
    return 0;
  if (!mu_branch_target(c, mo, expected, idx,
                        value_vin(shape_smu_value(n, idx), value_vactor(name),
                                  Values_reverse(bound.values)),
                        &target, error))
    return 0;
  if (!go(walk_env, bound.context, target, br->as.ctor.body, &tm, error))
    return 0;
  out->address = term_actor(name);
  out->leg = term_leg(field_binders(fields), tm);
  return 1;
}

/* collection_branch (line 444). */
static int collection_branch(const void *walk_env, SurfaceElabWalk go,
                             const CollectionEnv *env,
                             const SurfaceBranch *br, TermBranch *out,
                             const KError **error) {
  return elab_branch(walk_env, go, env->context, env->shape, env->legs,
                     env->motive, env->expected, br, out, error);
}

/* elab_fam_decl (line 447). */
static int elab_fam_decl(const CheckCtx *c, const SurfaceFam *fam,
                         CheckFamilyDecl *out, const KError **error) {
  TelescopeResult tele;
  ChainResult chain;
  Level level;
  if (!elab_telescope(fam->params, c, &tele, error)) return 0;
  if (!elab_chain(fam->ty, tele.context, &chain, error)) return 0;
  if (!elab_univ(chain.context, chain.result, &level, error)) return 0;
  out->name = fam->name;
  out->params = tele.entries;
  out->indices = chain.entries;
  out->level = level;
  return 1;
}

/* elab_ctor_decl (line 453). */
static int elab_ctor_decl(const CheckCtx *c, Text family,
                          const SurfaceFamCtor *ctor, CheckCtorDecl *out,
                          const KError **error) {
  ChainResult chain;
  const Term *tm;
  const Terms *params;
  const Terms *ix;
  if (!elab_chain(ctor->ty, c, &chain, error)) return 0;
  if (!request(NULL, chain.context, NULL, chain.result, &tm, error)) return 0;
  if (!mu_result(family, tm, &params, &ix, error)) return 0;
  out->name = ctor->name;
  out->args = chain.entries;
  out->params = params;
  out->indices = ix;
  return 1;
}

/* elab_coll_case (line 460). */
static int elab_coll_case(const void *walk_env, SurfaceElabWalk go,
                          const CheckCtx *c, const Value *expected,
                          const Term *scrut, const Value *w,
                          const ValueShape *s, const Closure *clo,
                          const SurfaceMotive *mo,
                          const SurfaceBranches *brs, const Term **out,
                          const KError **error) {
  Int63 n;
  const VLegs *legs;
  const TermMotive *motive;
  const SurfaceBranches *expanded;
  CollectionEnv env;
  const TermBranches *branches = NULL;
  if (!rules_collection_view(s, &n))
    return abort_with(
        error_mismatch(text_of_cstring("a case eliminates a collection")),
        error);
  if (!rules_coll_legs_of(ctx_of(c), clo, &legs, error)) return 0;
  if (!elab_motive(walk_env, go, c, mo, w, &motive, error)) return 0;
  if (!surface_sugar_expand(brs, int63_zero(), VLegs_length(legs), &expanded,
                            error))
    return 0;
  env.context = c;
  env.shape = s;
  env.legs = legs;
  env.motive = motive;
  env.expected = expected;
  for (; expanded != NULL; expanded = expanded->tail) {
    TermBranch b;
    if (!collection_branch(walk_env, go, &env, &expanded->head, &b, error))
      return 0;
    branches = TermBranches_cons(b, branches);
  }
  *out = term_elim(term_elimination(shape_scoll_term(n), scrut, QUANTITY_ONE,
                                    motive, TermBranches_reverse(branches)));
  return 1;
}

/* elab_mu_ref (line 468): one count check (arity_is) runs before both
   constructor paths. */
static int elab_mu_ref(const void *walk_env, SurfaceElabWalk go,
                       const CheckCtx *c, const MuRef *r,
                       const SurfaceTerms *args, const Value *expected,
                       const Term **out, const KError **error) {
  const Family *f;
  const Ctor *ct;
  switch (r->tag) {
  case MU_REF_RFAM:
    return elab_fam_ref(walk_env, go, c, r->family, args, out, error);
  case MU_REF_RCTOR:
    break;
  }
  f = r->family;
  ct = r->ctor;
  if (!arity_is(ct->name, length_tele(ct->args), length_surface(args), error))
    return 0;
  if (expected == NULL)
    return surface_constructor_infer_infer(walk_env, go, c, f, ct, args, out,
                                           error);
  if (!int63_equal(length_tele(f->params), int63_zero()))
    return surface_constructor_expected(walk_env, go, c, f, ct, args,
                                        expected, out, error);
  if (!surface_constructor_owner(c, f->name, ct->name, expected, error))
    return 0;
  return elab_ctor_ref(walk_env, go, c, f, ct, args, out, error);
}

/* elab_proj (line 480). */
static int elab_proj(const void *walk_env, SurfaceElabWalk go,
                     const CheckCtx *c, const SurfaceTerm *a, Int63 k,
                     const Term **out, const KError **error) {
  const Term *scrut;
  const Value *w;
  if (!go(walk_env, c, NULL, a, &scrut, error)) return 0;
  if (!type_of(c, scrut, &w, error)) return 0;
  return project_type(c, scrut, w, k, out, error);
}

/* app_head (line 482): 1 = Some. */
static int app_head(const CheckCtx *c, const SurfaceTerm *head, MuRef *out) {
  if (head->tag != SURFACE_SVAR) return 0;
  return mu_ref_of(c, head->as.var.name, out);
}

/* elab_fun (line 486): C recursion (depth = binder count). */
static int elab_fun(const void *walk_env, SurfaceElabWalk go,
                    const SurfaceBinders *bs, const CheckCtx *c,
                    const Value *expected, const SurfaceTerm *body,
                    const Term **out, const KError **error) {
  Bound bound;
  const Value *cod;
  const Term *tm;
  if (bs == NULL) return go(walk_env, c, expected, body, out, error);
  if (!elab_binder(walk_env, go, c, bs->head, &bound, error)) return 0;
  if (!cod_of(c, bound.name, bound.ty, expected, &cod, error)) return 0;
  if (!elab_fun(walk_env, go, bs->tail, bound.context, cod, body, &tm, error))
    return 0;
  *out = term_sec(
      shape_spi_term(bound.quantity, bound.name, bound.ty),
      TermLegs_cons(
          term_leg(TermBinders_cons(binder_of(bound.quantity, bound.name),
                                    NULL),
                   tm),
          NULL));
  return 1;
}

/* elab_items (line 493). */
static int elab_items(const void *walk_env, SurfaceElabWalk go,
                      const CheckCtx *c, const Value *expected,
                      const SurfaceTerms *items, const Terms **out,
                      const KError **error) {
  const Values *tys;
  const Terms *acc = NULL;
  if (!leg_expectations(c, expected, length_surface(items), NO, &tys, error))
    return 0;
  if (SurfaceTerms_length(items) != Values_length(tys))
    return abort_with(error_mismatch(text_of_cstring(
                          "the item count and the leg count differ")),
                      error);
  for (; items != NULL && tys != NULL;
       items = items->tail, tys = tys->tail) {
    const Term *tm;
    if (!item_elab(walk_env, go, c, items->head, tys->head, &tm, error))
      return 0;
    acc = Terms_cons(tm, acc);
  }
  *out = Terms_reverse(acc);
  return 1;
}

/* family_branch (line 497). */
static int family_branch(const void *walk_env, SurfaceElabWalk go,
                         const FamilyEnv *env, const SurfaceBranch *br,
                         TermBranch *out, const KError **error) {
  return elab_mu_branch(walk_env, go, env->context, env->name, env->family,
                        env->motive, env->params, env->expected, br, out,
                        error);
}

/* declare_families (line 500): the self tail call is a loop. The decl is
   an arena cell, so a kept pointer stays valid. */
static int declare_families(const SurfaceFams *fams, Budget budget,
                            const Globals *globals, const Globals **out,
                            const KError **error) {
  for (; fams != NULL; fams = fams->tail) {
    CheckFamilyDecl *d = arena_alloc(sizeof *d);
    const Globals *g;
    if (!elab_fam_decl(check_make(globals, budget), &fams->head, d, error))
      return 0;
    if (!check_declare_family(globals, budget, d, &g, error)) return 0;
    globals = g;
  }
  *out = globals;
  return 1;
}

/* collect of ctor_environment (lines 506 and 525): C recursion (depth =
   constructor count). */
static int ctor_decls(const CheckCtx *c, Text name,
                      const SurfaceFamCtors *ctors,
                      const CheckCtorDecls **out, const KError **error) {
  CheckCtorDecl d;
  const CheckCtorDecls *rest;
  if (ctors == NULL) {
    *out = NULL;
    return 1;
  }
  if (!elab_ctor_decl(c, name, &ctors->head, &d, error)) return 0;
  if (!ctor_decls(c, name, ctors->tail, &rest, error)) return 0;
  *out = CheckCtorDecls_cons(d, rest);
  return 1;
}

/* elab_mu_case (line 509). */
static int elab_mu_case(const void *walk_env, SurfaceElabWalk go,
                        const CheckCtx *c, const Value *expected,
                        const Term *scrut, Text n, const Values *ixv,
                        const Closure *clo, const Level *u,
                        const SurfaceMotive *mo, const SurfaceBranches *brs,
                        const Term **out, const KError **error) {
  const Family *fam = global_find_family(n, check_globals(c));
  const SurfaceBranches *expanded;
  const Values *penv;
  const TermMotive *motive;
  FamilyEnv env;
  const TermBranches *branches = NULL;
  const Terms *ix;
  if (fam == NULL) {
    Text parts[3];
    parts[0] = text_of_cstring("the family ");
    parts[1] = n;
    parts[2] = text_of_cstring(" is not declared");
    return abort_with(error_unbound(cat(parts, 3)), error);
  }
  if (!surface_family_expanded(fam, brs, &expanded, error)) return 0;
  if (!rules_mu_param_env(ctx_of(c), clo, &penv, error)) return 0;
  if (!elab_mu_motive(walk_env, go, c, n, fam, clo, u, penv, mo, &motive,
                      error))
    return 0;
  env.context = c;
  env.name = n;
  env.family = fam;
  env.motive = motive;
  env.params = penv;
  env.expected = expected;
  for (; expanded != NULL; expanded = expanded->tail) {
    TermBranch b;
    if (!family_branch(walk_env, go, &env, &expanded->head, &b, error))
      return 0;
    branches = TermBranches_cons(b, branches);
  }
  if (!collect_quote(c, ixv, &ix, error)) return 0;
  *out = term_elim(term_elimination(shape_smu_term(n, ix), scrut,
                                    QUANTITY_ONE, motive,
                                    TermBranches_reverse(branches)));
  return 1;
}

/* app_spine (line 516). */
static int app_spine(const void *walk_env, SurfaceElabWalk go,
                     const CheckCtx *c, const SurfaceTerm *f,
                     const SurfaceTerm *a, const SurfaceTerm *head,
                     const SurfaceTerms *args, const Value *expected,
                     const Term **out, const KError **error) {
  MuRef r;
  if (!app_head(c, head, &r)) return app_point(walk_env, go, c, f, a, out, error);
  return elab_mu_ref(walk_env, go, c, &r, args, expected, out, error);
}

/* define_families (line 520): the self tail call is a loop. */
static int define_families(const SurfaceFams *fams, Budget budget,
                           const Texts *group, const Globals *globals,
                           const Globals **out, const KError **error) {
  for (; fams != NULL; fams = fams->tail) {
    TelescopeResult tele;
    const CheckCtorDecls *cds;
    const Globals *g;
    if (!elab_telescope(fams->head.params, check_make(globals, budget), &tele,
                        error))
      return 0;
    if (!ctor_decls(tele.context, fams->head.name, fams->head.ctors, &cds,
                    error))
      return 0;
    if (!check_define_ctors(globals, budget, group, fams->head.name, cds, &g,
                            error))
      return 0;
    globals = g;
  }
  *out = globals;
  return 1;
}

/* with_pair (line 527): no twin; the callers take the pair fields. */

/* The shared Missing_branch of pair_second and pair_arm. */
static const KError *two_binder_arm(void) {
  return error_missing_branch(
      text_of_cstring("a pair elim arm must be a two-binder lambda"));
}

/* pair_second (line 530): the first binder stays with the caller. */
static int pair_second(const SurfaceTerm *body, SurfaceBinder *second,
                       const SurfaceTerm **inner, const KError **error) {
  if (body->tag != SURFACE_SFUN || !one_binder(body->as.fun.binders, second))
    return abort_with(two_binder_arm(), error);
  *inner = body->as.fun.body;
  return 1;
}

/* pair_arm (line 535). */
static int pair_arm(SurfaceBranch br, SurfaceBinder *first,
                    SurfaceBinder *second, const SurfaceTerm **body,
                    const KError **error) {
  switch (br.tag) {
  case SURFACE_BR_DEFAULT:
    return abort_with(error_wrong_leg(text_of_cstring(
                          "a pair elim requires an explicit arm")),
                      error);
  case SURFACE_BR_CTOR:
    return abort_with(error_wrong_leg(text_of_cstring(
                          "a pair elim arm cannot key a constructor")),
                      error);
  case SURFACE_BR_LEG:
    if (!one_binder(br.as.leg.binders, first))
      return abort_with(two_binder_arm(), error);
    return pair_second(br.as.leg.body, second, body, error);
  }
  return 0; /* unreachable: the switch is exhaustive */
}

/* pair_body (line 541): the leg binders are [{q1, x1}, {q2, x2}]. */
static int pair_body(const void *walk_env, SurfaceElabWalk go,
                     const CheckCtx *c, const Value *expected,
                     const ValueShape *s, const Closure *clo,
                     const TermMotive *mo, Quantity q, const Value *dom,
                     SurfaceBinder second, const SurfaceTerm *body,
                     SurfaceBinder first, const TermLeg **out,
                     const KError **error) {
  const Value *cod;
  const CheckCtx *inner;
  const Value *target;
  const Term *tm;
  if (!checked_payload(walk_env, go, c, dom, first.ty, error)) return 0;
  if (!open_in(c, clo, var_list(c->size), &cod, error)) return 0;
  inner = runtime_ops_bind(first.name, first.q, dom, c);
  if (!checked_payload(walk_env, go, inner, cod, second.ty, error)) return 0;
  if (!branch_target(c, mo, expected,
                     value_vin(s, value_vapt(q, value_var(c->size)),
                               var_list(int63_succ(c->size))),
                     &target, error))
    return 0;
  if (!go(walk_env, runtime_ops_bind(second.name, second.q, cod, inner),
          target, body, &tm, error))
    return 0;
  *out = term_leg(
      TermBinders_cons(binder_of(first.q, first.name),
                       TermBinders_cons(binder_of(second.q, second.name),
                                        NULL)),
      tm);
  return 1;
}

/* elab_pair_case (line 550). */
static int elab_pair_case(const void *walk_env, SurfaceElabWalk go,
                          const CheckCtx *c, const Value *expected,
                          const Term *scrut, const Value *w, Quantity q,
                          Text x, const Value *dom, const Closure *clo,
                          const SurfaceMotive *mo,
                          const SurfaceBranches *brs, const Term **out,
                          const KError **error) {
  SurfaceBranch br;
  SurfaceBinder first;
  SurfaceBinder second;
  const SurfaceTerm *body;
  const TermMotive *motive;
  const TermLeg *leg;
  const Term *dt;
  TermBranch branch;
  if (!one_branch(brs, &br))
    return abort_with(error_missing_branch(text_of_cstring(
                          "a pair elim takes exactly one arm")),
                      error);
  if (!pair_arm(br, &first, &second, &body, error)) return 0;
  if (!elab_motive(walk_env, go, c, mo, w, &motive, error)) return 0;
  if (!pair_body(walk_env, go, c, expected, shape_spi_value(q, x, dom), clo,
                 motive, q, dom, second, body, first, &leg, error))
    return 0;
  if (!quote_in(c, dom, &dt, error)) return 0;
  branch.address = term_aleg(int63_zero());
  branch.leg = leg;
  *out = term_elim(term_elimination(shape_spi_term(q, x, dt), scrut,
                                    QUANTITY_ONE, motive,
                                    TermBranches_cons(branch, NULL)));
  return 1;
}

/* case_former (line 558): SCOLL, SPAR and SNU share the last arm. */
static int case_former(const void *walk_env, SurfaceElabWalk go,
                       const CheckCtx *c, const Value *expected,
                       const Term *scrut, const Value *w,
                       const ValueFormer *former, const SurfaceMotive *mo,
                       const SurfaceBranches *brs, Bool only_mu,
                       const Term **out, const KError **error) {
  const ValueShape *s = former->shape;
  switch (s->tag) {
  case SHAPE_SMU:
    return elab_mu_case(walk_env, go, c, expected, scrut, s->as.smu.family,
                        s->as.smu.indices, former->diagram, former->universe,
                        mo, brs, out, error);
  case SHAPE_SPI:
    if (only_mu) return abort_with(needs_mu(), error);
    return elab_pair_case(walk_env, go, c, expected, scrut, w,
                          s->as.spi.quantity, s->as.spi.binder,
                          s->as.spi.domain, former->diagram, mo, brs, out,
                          error);
  case SHAPE_SCOLL:
  case SHAPE_SPAR:
  case SHAPE_SNU:
    break;
  }
  if (only_mu) return abort_with(needs_mu(), error);
  return elab_coll_case(walk_env, go, c, expected, scrut, w, s,
                        former->diagram, mo, brs, out, error);
}

/* elab_app (line 569): takes the SApp node (no rebuild of SApp). */
static int elab_app(const void *walk_env, SurfaceElabWalk go,
                    const CheckCtx *c, const SurfaceTerm *node,
                    const Value *expected, const Term **out,
                    const KError **error) {
  const SurfaceTerms *args;
  const SurfaceTerm *head = spine_of(node, NULL, &args);
  return app_spine(walk_env, go, c, node->as.app.function,
                   node->as.app.argument, head, args, expected, out, error);
}

/* present (line 572): no twin (a NULL or int test at the use site). */

/* taken (line 574): the Bool.or chain short circuits. */
static Bool taken(Text x, const Globals *g, const Texts *group) {
  MuRef r;
  if (global_find_family(x, g) != NULL) return YES;
  if (family_ctor(x, g, &r)) return YES;
  return surface_family_known(x, group);
}

/* clash (line 576): the self tail call is a loop; 1 = Some. */
static int clash(const Globals *g, const Texts *group,
                 const SurfaceFamCtors *cs, Text *out) {
  for (; cs != NULL; cs = cs->tail) {
    if (taken(cs->head.name, g, group)) {
      *out = cs->head.name;
      return 1;
    }
  }
  return 0;
}

/* fresh_group (line 580): status only; its answer is the input g. */
static int fresh_group(const Globals *g, const Texts *group,
                       const SurfaceFams *fams, const KError **error) {
  for (; fams != NULL; fams = fams->tail) {
    Text x;
    if (clash(g, group, fams->head.ctors, &x)) {
      Text parts[2];
      parts[0] = text_of_cstring("E-FAMILY-CTOR: duplicate constructor ");
      parts[1] = x;
      return abort_with(error_mismatch(cat(parts, 2)), error);
    }
  }
  return 1;
}

/* elab_mu_group_program (line 585): fam_names once for both uses. */
static int elab_mu_group_program(Budget budget, const Globals *globals,
                                 const SurfaceFams *fams,
                                 const Globals **out, const KError **error) {
  const Texts *group = fam_names(fams);
  const Globals *g;
  if (!fresh_group(globals, group, fams, error)) return 0;
  if (!declare_families(fams, budget, globals, &g, error)) return 0;
  return define_families(fams, budget, group, g, out, error);
}

/* elab_case (line 590). */
static int elab_case(const void *walk_env, SurfaceElabWalk go,
                     const CheckCtx *c, const Value *expected,
                     const SurfaceTerm *scrut, const SurfaceMotive *mo,
                     const SurfaceBranches *brs, Bool only_mu,
                     const Term **out, const KError **error) {
  const Term *tm;
  const Value *w;
  ValueFormer former;
  if (!go(walk_env, c, NULL, scrut, &tm, error)) return 0;
  if (!type_of(c, tm, &w, error)) return 0;
  if (value_as_lan(w, &former))
    return case_former(walk_env, go, c, expected, tm, w, &former, mo, brs,
                       only_mu, out, error);
  if (only_mu) return abort_with(needs_mu(), error);
  return abort_with(error_mismatch(text_of_cstring(
                        "a case needs a left former as the type of its "
                        "scrutinee")),
                    error);
}

/* elab_node (line 594): one switch over the 22 tags. */
static int elab_node(const void *walk_env, SurfaceElabWalk go,
                     const CheckCtx *c, const Value *expected,
                     const SurfaceTerm *s, const Term **out,
                     const KError **error) {
  switch (s->tag) {
  case SURFACE_SVAR: {
    MuRef r;
    if (local_projection(c, s->as.var.name))
      return abort_with(error_mismatch(text_of_cstring(
                            "E-RECORD-PROJECTION: use a qualified getter "
                            "such as Pair.first value")),
                        error);
    if (mu_ref_of(c, s->as.var.name, &r))
      return elab_mu_ref(walk_env, go, c, &r, NULL, expected, out, error);
    *out = local_or_global(c, s->as.var.name);
    return 1;
  }
  case SURFACE_SNAT:
    *out = term_lit(literal_lint(s->as.nat.value));
    return 1;
  case SURFACE_SPROP:
    *out = term_univ(level_zero());
    return 1;
  case SURFACE_STYPE:
    return univ_of_int(int63_succ(s->as.type.level), out, error);
  case SURFACE_SPRIM:
    *out = term_global(surface_syntax_prim_name(s->as.prim.prim));
    return 1;
  case SURFACE_SUNIT:
    *out = rules_unit_val();
    return 1;
  case SURFACE_SAUTO:
    *out = term_auto();
    return 1;
  case SURFACE_SPAIR:
    return elab_pair(walk_env, go, c, expected, s->as.pair.fst,
                     s->as.pair.snd, out, error);
  case SURFACE_STUPLE: {
    const Terms *terms;
    if (!elab_items(walk_env, go, c, expected, s->as.tuple.items, &terms,
                    error))
      return 0;
    *out = term_sec(shape_scoll_term(length_surface(s->as.tuple.items)),
                    legs_of(terms));
    return 1;
  }
  case SURFACE_SSUM: {
    const Terms *terms;
    if (!collect_walk(walk_env, go, c, s->as.sum.items, &terms, error))
      return 0;
    *out = sum_ty(terms);
    return 1;
  }
  case SURFACE_SPROD: {
    const Terms *terms;
    if (!collect_walk(walk_env, go, c, s->as.prod.items, &terms, error))
      return 0;
    *out = prod_ty(terms);
    return 1;
  }
  case SURFACE_SPROJ:
    return elab_proj(walk_env, go, c, s->as.proj.term, s->as.proj.key, out,
                     error);
  case SURFACE_SINJ: {
    const Values *tys;
    const Term *tm;
    if (!leg_expectations(c, expected, s->as.inj.size, YES, &tys, error))
      return 0;
    if (!go(walk_env, c, joined_at(s->as.inj.key, tys), s->as.inj.term, &tm,
            error))
      return 0;
    *out = term_in(shape_scoll_term(s->as.inj.size), term_aleg(s->as.inj.key),
                   Terms_cons(tm, NULL));
    return 1;
  }
  case SURFACE_SABSURD: {
    const Term *tm;
    if (!go(walk_env, c, NULL, s->as.absurd.term, &tm, error)) return 0;
    *out = term_elim(term_elimination(shape_scoll_term(int63_zero()), tm,
                                      QUANTITY_ONE, NULL, NULL));
    return 1;
  }
  case SURFACE_SAPP:
    return elab_app(walk_env, go, c, s, expected, out, error);
  case SURFACE_SFUN:
    return elab_fun(walk_env, go, s->as.fun.binders, c, expected,
                    s->as.fun.body, out, error);
  case SURFACE_SARROW:
    return elab_group(walk_env, go, c, s->as.arrow.binder, s->as.arrow.body,
                      NO, out, error);
  case SURFACE_SSTAR:
    return elab_group(walk_env, go, c, s->as.star.binder, s->as.star.body,
                      YES, out, error);
  case SURFACE_SLET: {
    const Term *tyt;
    const Value *tyv;
    const Term *dt;
    const Value *dv;
    const Term *bt;
    if (!go(walk_env, c, NULL, s->as.let.ty, &tyt, error)) return 0;
    if (!eval_in(c, tyt, &tyv, error)) return 0;
    if (!go(walk_env, c, tyv, s->as.let.value, &dt, error)) return 0;
    if (!eval_in(c, dt, &dv, error)) return 0;
    if (!go(walk_env,
            runtime_ops_define(s->as.let.name, QUANTITY_MANY, tyv, dv, c),
            expected, s->as.let.body, &bt, error))
      return 0;
    *out = term_let(s->as.let.name, tyt, dt, bt);
    return 1;
  }
  case SURFACE_SANN: {
    const Term *tyt;
    const Value *tyv;
    const Term *tm;
    if (!go(walk_env, c, NULL, s->as.ann.ty, &tyt, error)) return 0;
    if (!eval_in(c, tyt, &tyv, error)) return 0;
    if (!go(walk_env, c, tyv, s->as.ann.term, &tm, error)) return 0;
    *out = term_ann(tm, tyt);
    return 1;
  }
  case SURFACE_SCASE:
    return elab_case(walk_env, go, c, expected, s->as.elim.scrutinee,
                     s->as.elim.motive, s->as.elim.branches, NO, out, error);
  case SURFACE_SMATCH:
    return elab_case(walk_env, go, c, expected, s->as.elim.scrutinee,
                     s->as.elim.motive, s->as.elim.branches, YES, out, error);
  }
  return 0; /* unreachable: the switch is exhaustive */
}

/* node_walk: the drive of line 632 (@unsafe, no twin). Its effect is direct
   C recursion through request, surface_elab_program_request, node_walk and
   elab_node (depth = term nesting). */
static int node_walk(const void *walk_env, const CheckCtx *c,
                     const Value *expected, const SurfaceTerm *s,
                     const Term **out, const KError **error) {
  return elab_node(walk_env, request, c, expected, s, out, error);
}

/* elab (line 638). */
int surface_elab_elab(const CheckCtx *c, const Value *expected,
                      const SurfaceTerm *s, const Term **out,
                      const KError **error) {
  return request(NULL, c, expected, s, out, error);
}

/* elab_decl (line 639). */
int surface_elab_elab_decl(const CheckCtx *c, const SurfaceDecl *d,
                           CheckDecl *out, const KError **error) {
  return elab_decl_program(c, d, out, error);
}

/* elab_mu_group (line 640). */
int surface_elab_elab_mu_group(Budget budget, const Globals *globals,
                               const SurfaceFams *fams, const Globals **out,
                               const KError **error) {
  return elab_mu_group_program(budget, globals, fams, out, error);
}
