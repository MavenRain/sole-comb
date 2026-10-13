/* kernel_recursor_cover.c: C twin of lib/kernel_recursor_cover.bend (stage
   S3, session 2). Checked constructor coverage for direct structural
   recursor branches. Conventions of kernel_recursor.c. */
#include "sole.h"

static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

int recursor_cover_inhabited(const Texts *names, const Texts **out,
                             const KError **error) {
  if (names == NULL) {
    *error = error_not_yet(text_of_cstring(
        "recursor coverage for a family without constructors is not "
        "supported"));
    return 0;
  }
  *out = names;
  return 1;
}

/* Not a tail call (pmap after the recursion): the recursion stays, so the
   join is alternative(uses, tail) in the Bend order. */
int recursor_cover_walk(const RulesHost *host, const Texts *names, RulesCtx c,
                        RecursorScope scope, Quantity mode, Text owner,
                        const TermMotive *mo, const TermBranches *branches,
                        QuantityUsage *out, const KError **error) {
  const TermLeg *lg;
  QuantityUsage uses;
  QuantityUsage tail;
  if (names == NULL) {
    *out = quantity_unreachable();
    return 1;
  }
  lg = rules_branch_leg(value_vactor(names->head), branches);
  if (lg == NULL) {
    Text parts[4];
    parts[0] = text_of_cstring("the elimination of ");
    parts[1] = owner;
    parts[2] = text_of_cstring(" has no branch at ");
    parts[3] = names->head;
    *error = error_missing_branch(cat(parts, 4));
    return 0;
  }
  if (!recursor_body_check(host, c, scope, mode, owner, names->head, mo, lg,
                           &uses, error))
    return 0;
  if (!recursor_cover_walk(host, names->tail, c, scope, mode, owner, mo,
                           branches, &tail, error))
    return 0;
  *out = quantity_alternative(uses, tail);
  return 1;
}

int recursor_cover_check(const RulesHost *host, RulesCtx c,
                         RecursorScope scope, Quantity mode, Text owner,
                         const TermMotive *mo, const TermBranches *branches,
                         QuantityUsage *out, const KError **error) {
  const Family *fam;
  const Texts *all;
  const Texts *names;
  if (!rules_mu_family(c, owner, &fam, error)) return 0;
  if (!rules_mu_ctor_names(owner, fam, &all, error)) return 0;
  if (!recursor_cover_inhabited(all, &names, error)) return 0;
  if (!rules_mu_cover(owner, names, branches, error)) return 0;
  return recursor_cover_walk(host, names, c, scope, mode, owner, mo, branches,
                             out, error);
}
