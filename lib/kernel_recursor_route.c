/* kernel_recursor_route.c: C twin of lib/kernel_recursor_route.bend (stage
   S3, session 2). Kernel Elim dispatch: a leg that binds more binders than
   the fields of its recursive constructor routes to the checked recursor.
   All defs are pure. */
#include "sole.h"

Int63 recursor_route_binders(const TermLeg *lg) {
  return rules_advance(int63_zero(), TermBinders_length(lg->binders));
}

Bool recursor_route_fields(const TermLeg *lg, const Ctor *ct) {
  if (ct == NULL) return NO;
  return ct->self_rec &&
                 rules_less_than(int63_compare(
                     rules_advance(int63_zero(), Telescope_length(ct->args)),
                     recursor_route_binders(lg)))
             ? YES
             : NO;
}

Bool recursor_route_at(const Family *fam, const TermLeg *lg, MaybeText name) {
  if (!name.some) return NO;
  return recursor_route_fields(lg, positivity_ctor_of(name.text, fam));
}

/* Bool.or over the branches (pure values, so the loop stops at the first
   YES). */
Bool recursor_route_legs(const Family *fam, const TermBranches *branches) {
  for (; branches != NULL; branches = branches->tail)
    if (recursor_route_at(fam, branches->head.leg,
                          term_as_actor(branches->head.address)))
      return YES;
  return NO;
}

Bool recursor_route_routes(const Family *fam, const TermBranches *branches) {
  if (fam == NULL) return NO;
  return recursor_route_legs(fam, branches);
}
