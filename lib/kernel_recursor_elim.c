/* kernel_recursor_elim.c: C twin of lib/kernel_recursor_elim.bend (stage
   S3, session 2). Checked recursor scrutinee at instantiated parameters:
   the kernel Elim term routes here when a leg binds hypotheses.
   Conventions of kernel_recursor.c. view (line 15) is RecursorElimView. */
#include "sole.h"

static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

RecursorElimView recursor_elim_closed(const Closure *d, Text name,
                                      const Values *ixv) {
  RecursorElimView v;
  v.name = name;
  v.indices = ixv;
  v.diagram = d;
  return v;
}

/* with_former (kernel_rules line 1074) gives the whnf and the former; the
   lambda reads its shape (vs) and its diagram (d). */
int recursor_elim_former(const RulesHost *host, RulesCtx c, const Value *ty,
                         RecursorElimView *out, const KError **error) {
  const Value *w;
  ValueFormer former;
  Text n;
  const Values *ixv;
  if (!rules_with_former(c, NO,
                         text_of_cstring("the scrutinee is not a left former"),
                         ty, &w, &former, error))
    return 0;
  if (!rules_as_mu_value(former.shape, &n, &ixv)) {
    *error = error_mismatch(rules_wrong_pack());
    return 0;
  }
  *out = recursor_elim_closed(former.diagram, n, ixv);
  return 1;
}

int recursor_elim_named(Text owner, Text n, const KError **error) {
  Text parts[4];
  if (text_same(n, owner)) return 1;
  parts[0] = text_of_cstring("the scrutinee is in the family ");
  parts[1] = n;
  parts[2] = text_of_cstring(" and the recursor eliminates ");
  parts[3] = owner;
  *error = error_mismatch(cat(parts, 4));
  return 0;
}

int recursor_elim_values(const RulesHost *host, RulesCtx c,
                         const Telescope *params, const Closure *d,
                         const Values **out, const KError **error) {
  const Values *env;
  if (!recursor_instance_environment(host, c, params, d, &env, error))
    return 0;
  *out = Values_reverse(env);
  return 1;
}

int recursor_elim_result(const RulesHost *host, RulesCtx c, RulesCtx origin,
                         Quantity mode, const TermMotive *mo,
                         const Term *scrut, const Values *ixv,
                         QuantityUsage su, QuantityUsage bu,
                         RulesInferred *out, const KError **error) {
  const Value *sv;
  const Value *ty;
  QuantityUsage bu2;
  if (!rules_c_eval(c, scrut, &sv, error)) return 0;
  if (!rules_c_close(origin, rules_c_size(c), mode, bu, &bu2, error))
    return 0;
  if (!rules_mu_result(origin, mo, ixv, sv, &ty, error)) return 0;
  out->ty = ty;
  out->uses = quantity_sequence(su, quantity_scale(mode, bu2));
  return 1;
}

int recursor_elim_scoped(const RulesHost *host, RulesCtx c, Quantity mode,
                         Text owner, const Term *scrut, const TermMotive *mo,
                         const TermBranches *branches, const Family *fam,
                         QuantityUsage su, const Values *ixv,
                         RecursorScope scope, RulesInferred *out,
                         const KError **error) {
  RulesCtx origin;
  QuantityUsage bu;
  if (!recursor_open(host, scope, rules_family_params(fam), c, &origin,
                     error))
    return 0;
  if (!recursor_cover_check(host, c, scope, mode, owner, mo, branches, &bu,
                            error))
    return 0;
  return recursor_elim_result(host, c, origin, mode, mo, scrut, ixv, su, bu,
                              out, error);
}

int recursor_elim_viewed(const RulesHost *host, RulesCtx c, Quantity mode,
                         Text owner, const Term *scrut, const TermMotive *mo,
                         const TermBranches *branches, const Family *fam,
                         QuantityUsage su, RecursorElimView v,
                         RulesInferred *out, const KError **error) {
  const Values *vals;
  RecursorScope scope;
  if (!recursor_elim_named(owner, v.name, error)) return 0;
  if (!recursor_elim_values(host, c, rules_family_params(fam), v.diagram,
                            &vals, error))
    return 0;
  scope.tag = RECURSOR_INSTANCE;
  scope.values = vals;
  return recursor_elim_scoped(host, c, mode, owner, scrut, mo, branches, fam,
                              su, v.indices, scope, out, error);
}

int recursor_elim_typed(const RulesHost *host, RulesCtx c, Quantity mode,
                        Text owner, const Term *scrut, const TermMotive *mo,
                        const TermBranches *branches, const Family *fam,
                        RulesInferred inf, RulesInferred *out,
                        const KError **error) {
  RecursorElimView v;
  if (!recursor_elim_former(host, c, inf.ty, &v, error)) return 0;
  return recursor_elim_viewed(host, c, mode, owner, scrut, mo, branches, fam,
                              inf.uses, v, out, error);
}

int recursor_elim_check(const RulesHost *host, RulesCtx c, Quantity mode,
                        Text owner, Quantity sq, const Term *scrut,
                        const TermMotive *mo, const TermBranches *branches,
                        RulesInferred *out, const KError **error) {
  const Family *fam;
  RulesInferred inf;
  if (!rules_mu_family(c, owner, &fam, error)) return 0;
  if (!rules_infer_scrutinee(host, c, mode, sq, scrut, &inf, error)) return 0;
  return recursor_elim_typed(host, c, mode, owner, scrut, mo, branches, fam,
                             inf, out, error);
}
