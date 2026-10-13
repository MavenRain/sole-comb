/* kernel_recursor_body.c: C twin of lib/kernel_recursor_body.bend (stage
   S3, session 2). Checked branch bodies for direct structural recursor
   layouts. Conventions of kernel_recursor.c. */
#include "sole.h"

static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

Text recursor_body_source(RecursorKind k) {
  switch (k.tag) {
  case RECURSOR_FIELD_BINDER: return text_of_cstring("field");
  case RECURSOR_HYPOTHESIS_BINDER: return text_of_cstring("hypothesis");
  }
  return text_of_cstring("field"); /* each tag returns above */
}

/* The Binder arm is a self tail call: a loop. */
int recursor_body_scope(const RulesHost *host, const TermBinders *marks,
                        const RecursorBinders *bs, RulesCtx c, RulesCtx *out,
                        const KError **error) {
  for (; marks != NULL; marks = marks->tail, bs = bs->tail) {
    const RecursorBinder *b;
    if (bs == NULL) {
      *error = error_mismatch(
          text_of_cstring("a recursor branch binder has no plan binder"));
      return 0;
    }
    b = bs->head;
    if (!rules_binder_quantity(text_of_cstring("recursor branch binder"),
                               recursor_body_source(b->kind),
                               marks->head.name, marks->head.quantity,
                               b->quantity, error))
      return 0;
    c = rules_c_bind(c, marks->head.name, b->quantity, b->domain);
  }
  *out = c;
  return 1;
}

int recursor_body_body(const RulesHost *host, RulesCtx origin, Quantity mode,
                       const Term *tm, const Value *target, RulesCtx inner,
                       QuantityUsage *out, const KError **error) {
  QuantityUsage uses;
  if (!rules_c_check(host, inner, quantity_runtime(mode), tm, target, &uses,
                     error))
    return 0;
  return rules_c_close(inner, rules_c_size(origin), mode, uses, out, error);
}

int recursor_body_arity(Text key, Int63 got, Int63 want,
                        const KError **error) {
  Text parts[9];
  if (int63_equal(got, want)) return 1;
  parts[0] = text_of_cstring("the recursor branch at ");
  parts[1] = key;
  parts[2] = text_of_cstring(" binds ");
  parts[3] = int63_show(got);
  parts[4] = text_of_cstring(" binders and ");
  parts[5] = key;
  parts[6] = text_of_cstring(" takes ");
  parts[7] = int63_show(want);
  parts[8] = text_of_cstring(" fields and hypotheses");
  *error = error_missing_branch(cat(parts, 9));
  return 0;
}

int recursor_body_leg(const RulesHost *host, RulesCtx origin, Quantity mode,
                      Text key, RecursorBranchPlan value, const TermLeg *lg,
                      QuantityUsage *out, const KError **error) {
  RulesCtx inner;
  if (!recursor_body_arity(
          key, rules_advance(int63_zero(), TermBinders_length(lg->binders)),
          rules_advance(int63_zero(), RecursorBinders_length(value.binders)),
          error))
    return 0;
  if (!recursor_body_scope(host, lg->binders, value.binders, origin, &inner,
                           error))
    return 0;
  return recursor_body_body(host, origin, mode, lg->body, value.target, inner,
                            out, error);
}

int recursor_body_check(const RulesHost *host, RulesCtx c,
                        RecursorScope scope, Quantity mode, Text owner,
                        Text key, const TermMotive *mo, const TermLeg *lg,
                        QuantityUsage *out, const KError **error) {
  RecursorBranchPlan value;
  const Family *fam;
  RulesCtx origin;
  if (!recursor_branch_layout(host, c, scope, owner, key, mo, &value, error))
    return 0;
  if (!rules_mu_family(c, owner, &fam, error)) return 0;
  if (!recursor_open(host, scope, rules_family_params(fam), c, &origin,
                     error))
    return 0;
  return recursor_body_leg(host, origin, mode, key, value, lg, out, error);
}
