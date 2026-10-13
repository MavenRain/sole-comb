/* kernel_recursor_branch.c: C twin of lib/kernel_recursor_branch.bend
   (stage S3, session 2). Checked branch types for direct structural
   recursor layouts. Conventions of kernel_recursor.c. R.result defs return
   int (1 = Done with *out, 0 = Fail with *error). */
#include "sole.h"

static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

/* The Binder{...} constructor in the arena. */
static const RecursorBinder *binder_new(Int63 position, Quantity q,
                                        Text name, const Value *domain,
                                        RecursorKind kind) {
  RecursorBinder *b = (RecursorBinder *)arena_alloc(sizeof *b);
  b->position = position;
  b->quantity = q;
  b->name = name;
  b->domain = domain;
  b->kind = kind;
  return b;
}

int recursor_branch_grade(Quantity q, Quantity *out, const KError **error) {
  switch (q) {
  case QUANTITY_ZERO: *out = QUANTITY_ZERO; return 1;
  case QUANTITY_ONE:
    *error = error_not_yet(text_of_cstring(
        "recursor hypotheses for affine recursive fields are not supported"));
    return 0;
  case QUANTITY_MANY: *out = QUANTITY_MANY; return 1;
  }
  return 0; /* each tag returns above */
}

const RecursorBinders *recursor_branch_fields(const RecursorFields *fs) {
  RecursorKind kind;
  if (fs == NULL) return NULL;
  kind.tag = RECURSOR_FIELD_BINDER;
  kind.child = int63_zero();
  return RecursorBinders_cons(
      binder_new(fs->head->position, fs->head->quantity, fs->head->name,
                 fs->head->domain, kind),
      recursor_branch_fields(fs->tail));
}

const Values *recursor_branch_values(const RecursorFields *fs) {
  if (fs == NULL) return NULL;
  return Values_cons(fs->head->value, recursor_branch_values(fs->tail));
}

/* The else arm is a self tail call: a loop. */
int recursor_branch_child(Int63 position, const RecursorFields *fs,
                          Quantity *out, const KError **error) {
  for (; fs != NULL; fs = fs->tail)
    if (int63_equal(fs->head->position, position))
      return recursor_branch_grade(fs->head->quantity, out, error);
  *error = error_mismatch(
      text_of_cstring("a recursor hypothesis has no recursive field"));
  return 0;
}

int recursor_branch_hypotheses(const RecursorHypotheses *hs,
                               const RecursorFields *fs, Int63 count,
                               const RecursorBinders **out,
                               const KError **error) {
  Quantity q;
  const RecursorBinders *tail;
  RecursorKind kind;
  if (hs == NULL) {
    *out = NULL;
    return 1;
  }
  if (!recursor_branch_child(hs->head->position, fs, &q, error)) return 0;
  if (!recursor_branch_hypotheses(hs->tail, fs, int63_succ(count), &tail,
                                  error))
    return 0;
  kind.tag = RECURSOR_HYPOTHESIS_BINDER;
  kind.child = hs->head->position;
  *out = RecursorBinders_cons(
      binder_new(count, q, hs->head->name, hs->head->domain, kind), tail);
  return 1;
}

int recursor_branch_target(const RulesHost *host, RulesCtx origin, Text owner,
                           Text key, const TermMotive *mo, const Ctor *ct,
                           RulesCtx c, const RecursorFields *fs,
                           const Value **out, const KError **error) {
  const Values *idx;
  if (!rules_mu_values(c, runtime_ops_env(c.check), rules_ctor_indices(ct),
                       &idx, error))
    return 0;
  return rules_mu_result(origin, mo, idx,
                         value_vin(shape_smu_value(owner, idx),
                                   value_vactor(key),
                                   recursor_branch_values(fs)),
                         out, error);
}

int recursor_branch_formation(const RulesHost *host, RulesCtx c,
                              const Value *value, const KError **error) {
  const Term *tm;
  Level lvl;
  if (!runtime_ops_quote(c.check, value, &tm, error)) return 0;
  return rules_c_univ(host, c, tm, &lvl, error);
}

/* The Binder arm is a self tail call: a loop. */
int recursor_branch_certify(const RulesHost *host, const RecursorBinders *bs,
                            RulesCtx c, const Value *ty, RulesCtx *out,
                            const KError **error) {
  for (; bs != NULL; bs = bs->tail) {
    if (!recursor_branch_formation(host, c, bs->head->domain, error))
      return 0;
    c = rules_c_bind(c, bs->head->name, bs->head->quantity, bs->head->domain);
  }
  if (!recursor_branch_formation(host, c, ty, error)) return 0;
  *out = c;
  return 1;
}

int recursor_branch_finish(const RulesHost *host, RulesCtx origin,
                           RecursorMotivePlan value,
                           const RecursorBinders *bs, const Value *ty,
                           RecursorBranchPlan *out, const KError **error) {
  RulesCtx scope;
  if (!recursor_branch_certify(host, bs, origin, ty, &scope, error)) return 0;
  out->motive = value;
  out->context = scope;
  out->binders = bs;
  out->target = ty;
  return 1;
}

int recursor_branch_assemble(const RulesHost *host, RulesCtx origin,
                             Text owner, Text key, const TermMotive *mo,
                             const Ctor *ct, RecursorMotivePlan value,
                             RecursorBranchPlan *out, const KError **error) {
  RulesCtx c = value.layout.context;
  const RecursorFields *fs = value.layout.fields;
  const RecursorBinders *ihs;
  const Value *ty;
  if (!recursor_branch_hypotheses(
          value.hypotheses, fs,
          rules_advance(int63_zero(), RecursorFields_length(fs)), &ihs,
          error))
    return 0;
  if (!recursor_branch_target(host, origin, owner, key, mo, ct, c, fs, &ty,
                              error))
    return 0;
  return recursor_branch_finish(
      host, origin, value,
      RecursorBinders_append(recursor_branch_fields(fs), ihs), ty, out,
      error);
}

int recursor_branch_layout(const RulesHost *host, RulesCtx c,
                           RecursorScope scope, Text owner, Text key,
                           const TermMotive *mo, RecursorBranchPlan *out,
                           const KError **error) {
  const Family *fam;
  RecursorMotivePlan value;
  const Ctor *ct;
  RulesCtx origin;
  if (!rules_mu_family(c, owner, &fam, error)) return 0;
  if (!recursor_motive_layout(host, c, scope, owner, key, mo, &value, error))
    return 0;
  ct = positivity_ctor_of(key, fam);
  if (ct == NULL) {
    Text parts[3];
    parts[0] = key;
    parts[1] = text_of_cstring(" is not a constructor of ");
    parts[2] = owner;
    *error = error_unbound(cat(parts, 3));
    return 0;
  }
  if (!recursor_open(host, scope, rules_family_params(fam), c, &origin,
                     error))
    return 0;
  return recursor_branch_assemble(host, origin, owner, key, mo, ct, value,
                                  out, error);
}
