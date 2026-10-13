/* lib/erase_mu.c: twin of erase/mu.bend (S4 session 2b).
   Recursive-family plans from Kanon 69f3be51 lib/erase.ml:942-1134.
   rules (line 34) is C.result_of(C.drive(p, C.budget(c))). Each R program
   here only lifts ops calls (no Request), so the C twins take no host;
   result_of drops the fuel. Pair.require is a C if with error_mismatch
   (refuse). position and find_branch are tail calls (loops); fields,
   binders and indices are right folds (loop, then reverse). */
#include "sole.h"

/* view (line 21). */
typedef struct {
  Text name;
  const Values *indices;
  const Closure *diagram;
} View;

/* binding (line 29). */
typedef struct {
  const CheckCtx *check;
  const Values *env;
  const Values *values;
  Int63 arity;
  const EraseMuBinders *binders;
} Binding;

/* String.concat of a fixed list of parts (the cat idiom of kernel_rules). */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

/* The RulesCtx of the ops calls (as erase_type.c ctx_of). */
static RulesCtx ctx_of(const CheckCtx *c) {
  RulesCtx context;
  context.check = c;
  return context;
}

/* Pair.require: the None case. */
static int refuse(const char *message, const KError **error) {
  *error = error_mismatch(text_of_cstring(message));
  return 0;
}

/* former (line 36). */
static int former(Text action, const ValueFormer *value, View *out,
                  const KError **error) {
  const ValueShape *shape = value->shape;
  switch (shape->tag) {
    case SHAPE_SMU:
      out->name = shape->as.smu.family;
      out->indices = shape->as.smu.indices;
      out->diagram = value->diagram;
      return 1;
    case SHAPE_SPI:
    case SHAPE_SCOLL:
    case SHAPE_SPAR:
    case SHAPE_SNU:
      break;
  }
  *error = error_mismatch(
      text_append(action, text_of_cstring(" at a shape past M0")));
  return 0;
}

/* of_type (line 45). */
static int of_type(const CheckCtx *c, const Value *ty, const char *action,
                   const char *stray, View *out, const KError **error) {
  const Value *w;
  ValueFormer value;
  if (!eval_whnf(check_globals(c), ty, &w, error)) return 0;
  if (!value_as_lan(w, &value)) return refuse(stray, error);
  return former(text_of_cstring(action), &value, out, error);
}

/* position (line 48): *out = the index of n in names; 0 = None{}. */
static int position(Text n, const Texts *names, Int63 *out) {
  Int63 index = int63_zero();
  for (; names != NULL; names = names->tail) {
    if (text_same(n, names->head)) {
      *out = index;
      return 1;
    }
    index = int63_succ(index);
  }
  return 0;
}

/* ctor (line 53). */
static int ctor(Text n, const Family *fam, Text key, const Ctor **out,
                const KError **error) {
  const Ctor *ct = positivity_ctor_of(key, fam);
  if (ct == NULL) {
    *error = erase_family_not_ctor(key, n);
    return 0;
  }
  *out = ct;
  return 1;
}

/* arity_word (line 56). */
static Text arity_word(void) {
  return text_of_cstring(
      "a branch binds a different number of fields than its constructor "
      "takes");
}

/* fields (line 57): the fields with a Some layout entry, in order. */
static int fields(const CheckCtx *c, const Telescope *tele,
                  const Values *env, const Terms *args,
                  const MaybeReprs *layout, const ErasePairFields **out,
                  const KError **error) {
  const ErasePairFields *acc = NULL;
  for (; tele != NULL; tele = tele->tail) {
    const Value *tyv;
    const Value *vv;
    if (args == NULL || layout == NULL) {
      *error = error_mismatch(arity_word());
      return 0;
    }
    if (!eval_eval(check_globals(c), env, tele->head.binding.type, &tyv,
                   error))
      return 0;
    if (!eval_eval(check_globals(c), c->env, args->head, &vv, error))
      return 0;
    if (layout->head != NULL) {
      ErasePairField f;
      f.ty = tyv;
      f.term = args->head;
      acc = ErasePairFields_cons(f, acc);
    }
    env = Values_cons(vv, env);
    args = args->tail;
    layout = layout->tail;
  }
  if (args != NULL || layout != NULL) {
    *error = error_mismatch(arity_word());
    return 0;
  }
  *out = ErasePairFields_reverse(acc);
  return 1;
}

/* tag_fields (line 73). */
static int tag_fields(const CheckCtx *c, Text n, const Family *fam,
                      Int63 index, const Terms *args, const Values *env,
                      const MaybeReprs *layout, const Ctor *ct,
                      EraseMuTag *out, const KError **error) {
  const ErasePairFields *fs;
  const Tids *group;
  if (!fields(c, ct->args, env, args, layout, &fs, error)) return 0;
  if (!erase_family_group_tids(c, n, fam, &group, error)) return 0;
  out->tid = erase_type_mu_tid(n);
  out->index = index;
  out->fields = fs;
  out->group = group;
  return 1;
}

/* tag_ctor (line 78). */
static int tag_ctor(const CheckCtx *c, Text n, const Closure *d,
                    const Family *fam, Text key, const Terms *args,
                    const Texts *names, EraseMuTag *out,
                    const KError **error) {
  Int63 index;
  const Ctor *ct;
  const Values *env;
  const MaybeReprs *layout;
  if (!position(key, names, &index)) {
    *error = erase_family_not_ctor(key, n);
    return 0;
  }
  if (!ctor(n, fam, key, &ct, error)) return 0;
  if (!rules_mu_param_env(ctx_of(c), d, &env, error)) return 0;
  if (!erase_family_layout(c, fam, ct, &layout, error)) return 0;
  return tag_fields(c, n, fam, index, args, env, layout, ct, out, error);
}

/* injection_view (line 85). */
static int injection_view(const CheckCtx *c, const TermAddr *address,
                          const Terms *args, const View *value,
                          EraseMuTag *out, const KError **error) {
  MaybeText key = term_as_actor(address);
  const Family *fam;
  const Texts *names;
  if (!key.some)
    return refuse("a constructor takes a constructor address", error);
  if (!rules_mu_family(ctx_of(c), value->name, &fam, error)) return 0;
  if (!rules_mu_ctor_names(value->name, fam, &names, error)) return 0;
  return tag_ctor(c, value->name, value->diagram, fam, key.text, args, names,
                  out, error);
}

/* injection (line 92). */
int erase_mu_injection(const CheckCtx *c, const Value *ty,
                       const TermAddr *address, const Terms *args,
                       EraseMuTag *out, const KError **error) {
  View value;
  if (!of_type(c, ty, "an introduction",
               "an introduction stands at a type that is not a left former",
               &value, error))
    return 0;
  return injection_view(c, address, args, &value, out, error);
}

/* case_view (line 94). */
static int case_view(const CheckCtx *c, const Value *sty,
                     const TermMotive *motive, const View *value,
                     EraseMuCase *out, const KError **error) {
  const Family *fam;
  const TermMotive *mo;
  const Texts *names;
  const Values *env;
  const Tids *group;
  if (!rules_mu_family(ctx_of(c), value->name, &fam, error)) return 0;
  if (!rules_mu_motive_of(value->name, fam, value->indices, motive, &mo,
                          error))
    return 0;
  if (!rules_mu_ctor_names(value->name, fam, &names, error)) return 0;
  if (!rules_mu_param_env(ctx_of(c), value->diagram, &env, error)) return 0;
  if (!erase_family_group_tids(c, value->name, fam, &group, error)) return 0;
  out->sty = sty;
  out->name = value->name;
  out->family = fam;
  out->motive = mo;
  out->env = env;
  out->names = names;
  out->group = group;
  return 1;
}

/* case_type (line 102). */
static int case_type(const CheckCtx *c, const Value *sty,
                     const TermMotive *motive, EraseMuCase *out,
                     const KError **error) {
  View value;
  if (!of_type(c, sty, "an elimination",
               "an elimination stands on a value that is not a left former",
               &value, error))
    return 0;
  return case_view(c, sty, motive, &value, out, error);
}

/* case_of (line 104). */
int erase_mu_case_of(const CheckCtx *c, const Term *scrut,
                     const TermMotive *motive, EraseMuCase *out,
                     const KError **error) {
  const Value *sty;
  if (!check_infer(c, QUANTITY_MANY, scrut, &sty, error)) return 0;
  return case_type(c, sty, motive, out, error);
}

/* find_branch (line 106): the first branch at constructor key. */
static int find_branch(Text n, Text key, const TermBranches *branches,
                       const TermLeg **out, const KError **error) {
  Text parts[4];
  for (; branches != NULL; branches = branches->tail) {
    MaybeText x = term_as_actor(branches->head.address);
    if (x.some && text_same(key, x.text)) {
      *out = branches->head.leg;
      return 1;
    }
  }
  parts[0] = text_of_cstring("the elimination of ");
  parts[1] = n;
  parts[2] = text_of_cstring(" has no branch at ");
  parts[3] = key;
  *error = error_missing_branch(cat(parts, 4));
  return 0;
}

/* binders (line 115) with bound_cons (line 112). c, env, vals and arity go
   down; the Binder of each entry joins on the way up, so the result
   binders are the telescope order, then the initial binders. The new
   variable is V.var(C.size(c)) of the context before the bind. */
static int binders(const Telescope *tele, const TermBinders *names,
                   const MaybeReprs *layout, const Binding *value,
                   Binding *out, const KError **error) {
  const CheckCtx *c = value->check;
  const Values *env = value->env;
  const Values *vals = value->values;
  Int63 arity = value->arity;
  const EraseMuBinders *acc = NULL;
  for (; tele != NULL; tele = tele->tail) {
    Quantity q = tele->head.quantity;
    EraseMuBinder b;
    const Value *v;
    if (names == NULL || layout == NULL) {
      *error = error_missing_branch(arity_word());
      return 0;
    }
    b.quantity = q;
    b.name = names->head.name;
    if (!eval_eval(check_globals(c), env, tele->head.binding.type,
                   &b.domain, error))
      return 0;
    if (layout->head == NULL)
      b.keep = NO;
    else
      b.keep = YES;
    v = value_var(c->size);
    acc = EraseMuBinders_cons(b, acc);
    c = runtime_ops_bind(b.name, q, b.domain, c);
    env = Values_cons(v, env);
    vals = Values_cons(v, vals);
    if (b.keep) arity = int63_succ(arity);
    names = names->tail;
    layout = layout->tail;
  }
  if (names != NULL || layout != NULL) {
    *error = error_missing_branch(arity_word());
    return 0;
  }
  out->check = c;
  out->env = env;
  out->values = vals;
  out->arity = arity;
  out->binders = EraseMuBinders_reverse_onto(acc, value->binders);
  return 1;
}

/* indices (line 134). */
static int indices(const CheckCtx *c, const Values *env, const Terms *terms,
                   const Values **out, const KError **error) {
  const Values *acc = NULL;
  for (; terms != NULL; terms = terms->tail) {
    const Value *v;
    if (!eval_eval(check_globals(c), env, terms->head, &v, error)) return 0;
    acc = Values_cons(v, acc);
  }
  *out = Values_reverse(acc);
  return 1;
}

/* branch_target (line 139). */
static int branch_target(const CheckCtx *origin, Text n, Text key,
                         const TermMotive *mo, const Terms *results,
                         const Term *body, const Binding *value,
                         EraseMuBranch *out, const KError **error) {
  const Values *idx;
  const Value *self;
  const Value *target;
  if (!indices(value->check, value->env, results, &idx, error)) return 0;
  self = value_vin(shape_smu_value(n, idx), value_vactor(key),
                   Values_reverse(value->values));
  if (!rules_mu_result(ctx_of(origin), mo, idx, self, &target, error))
    return 0;
  out->binders = value->binders;
  out->arity = value->arity;
  out->target = target;
  out->body = body;
  return 1;
}

/* branch_leg (line 146). */
static int branch_leg(const CheckCtx *c, Text n, Text key,
                      const TermMotive *mo, const Values *env,
                      const Ctor *ct, const TermLeg *leg,
                      const MaybeReprs *layout, EraseMuBranch *out,
                      const KError **error) {
  Binding start;
  Binding value;
  start.check = c;
  start.env = env;
  start.values = NULL;
  start.arity = int63_zero();
  start.binders = NULL;
  if (!binders(ct->args, leg->binders, layout, &start, &value, error))
    return 0;
  return branch_target(c, n, key, mo, ct->res_idx, leg->body, &value, out,
                       error);
}

/* branch_of (line 153). */
int erase_mu_branch_of(const CheckCtx *c, Text n, const Family *fam,
                       const TermMotive *mo, const Values *env, Text key,
                       const TermBranches *branches, EraseMuBranch *out,
                       const KError **error) {
  const Ctor *ct;
  const TermLeg *leg;
  const MaybeReprs *layout;
  if (!ctor(n, fam, key, &ct, error)) return 0;
  if (!find_branch(n, key, branches, &leg, error)) return 0;
  if (!erase_family_layout(c, fam, ct, &layout, error)) return 0;
  return branch_leg(c, n, key, mo, env, ct, leg, layout, out, error);
}
