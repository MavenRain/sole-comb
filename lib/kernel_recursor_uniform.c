/* kernel_recursor_uniform.c: C twin of lib/kernel_recursor_uniform.bend
   (stage S3, session 2). Uniform-parameter validation for direct
   recursive-field layouts. Conventions of kernel_recursor.c. A Unit
   program returns int and has no *out. */
#include "sole.h"

const KError *recursor_uniform_mismatch(void) {
  return error_not_yet(text_of_cstring(
      "recursor layout requires uniform recursive parameters"));
}

/* The E.Mismatch text of the three arity arms (lines 20, 21, 24). */
static const KError *arity_differs(void) {
  return error_mismatch(text_of_cstring(
      "recursive parameter arity differs from the family telescope"));
}

/* The pchoose True arm is a self tail call: a loop. */
int recursor_uniform_arguments(const RulesHost *host, RulesCtx c,
                               RecursorScope scope, const Telescope *tele,
                               const Values *values, const Values *expected,
                               RulesCtx prefix, const KError **error) {
  for (; tele != NULL && values != NULL;
       tele = tele->tail, values = values->tail, expected = expected->tail) {
    const Value *tyv;
    Bool eq;
    if (expected == NULL) {
      *error = arity_differs();
      return 0;
    }
    if (!rules_c_eval(prefix, tele->head.binding.type, &tyv, error)) return 0;
    if (!rules_c_conv(host, c, tyv, values->head, expected->head, &eq, error))
      return 0;
    if (!eq) {
      *error = recursor_uniform_mismatch();
      return 0;
    }
    prefix = recursor_install(scope, tele->head.binding.name,
                              tele->head.quantity, tyv, expected->head,
                              prefix);
  }
  if (tele != NULL || values != NULL) {
    *error = arity_differs();
    return 0;
  }
  return 1;
}

int recursor_uniform_diagram(const RulesHost *host, RulesCtx base,
                             RecursorScope scope, RulesCtx c,
                             const Telescope *params, const Values *expected,
                             const ValueFormer *view, const KError **error) {
  const Values *env;
  if (view == NULL) {
    *error = error_mismatch(
        text_of_cstring("a direct recursive field has no family former"));
    return 0;
  }
  if (!rules_mu_param_env(c, view->diagram, &env, error)) return 0;
  return recursor_uniform_arguments(host, c, scope, params,
                                    Values_reverse(env), expected, base,
                                    error);
}

/* Both arms are self tail calls: a loop. */
int recursor_uniform_fields(const RulesHost *host, RulesCtx base,
                            RecursorScope scope, RulesCtx c,
                            const Telescope *params, const Values *expected,
                            const RecursorFields *values,
                            const KError **error) {
  for (; values != NULL; values = values->tail) {
    ValueFormer former;
    switch (values->head->role.tag) {
    case RECURSOR_ORDINARY: break;
    case RECURSOR_DIRECT:
      if (!recursor_uniform_diagram(
              host, base, scope, c, params, expected,
              value_as_lan(values->head->domain, &former) ? &former : NULL,
              error))
        return 0;
      break;
    }
  }
  return 1;
}

const Values *recursor_uniform_levels(const Telescope *tele, Int63 level) {
  if (tele == NULL) return NULL;
  return Values_cons(value_var(level),
                     recursor_uniform_levels(tele->tail, int63_succ(level)));
}

const Values *recursor_uniform_targets(RecursorScope scope,
                                       const Telescope *tele, RulesCtx c) {
  switch (scope.tag) {
  case RECURSOR_ABSTRACT:
    return recursor_uniform_levels(tele, rules_c_size(c));
  case RECURSOR_INSTANCE: return scope.values;
  }
  return NULL; /* each tag returns above */
}

int recursor_uniform_validate(const RulesHost *host, RulesCtx base,
                              RecursorScope scope, const Telescope *params,
                              RecursorPlan value, RecursorPlan *out,
                              const KError **error) {
  if (!recursor_uniform_fields(host, base, scope, value.context, params,
                               recursor_uniform_targets(scope, params, base),
                               value.fields, error))
    return 0;
  *out = value;
  return 1;
}

int recursor_uniform_layout(const RulesHost *host, RulesCtx c,
                            RecursorScope scope, Text owner, Text key,
                            RecursorPlan *out, const KError **error) {
  const Family *fam;
  RecursorPlan plan;
  if (!rules_mu_family(c, owner, &fam, error)) return 0;
  if (!recursor_layout(host, c, scope, owner, key, &plan, error)) return 0;
  return recursor_uniform_validate(host, c, scope, rules_family_params(fam),
                                   plan, out, error);
}
