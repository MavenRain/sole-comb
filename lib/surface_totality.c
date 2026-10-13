/* lib/surface_totality.c: twin of surface/totality.bend (S5 session 2b).
   Finite node polling followed by the pinned structural certificate.
   R.result(A) is an int return (1 = Done with *out, 0 = Fail with *error).
   pending, next_terms, guarded_arg and guard_result have no C twin: they
   are inline. O.maybe is a C if statement. spend is a self tail call: a
   loop. */
#include "sole.h"

static const char *const budget_msg = "the totality budget is exhausted";

/* empty (line 15). */
static int empty(const Terms *terms, const KError **error) {
  if (terms == NULL) return 1;
  *error = error_budget_exhausted(text_of_cstring(budget_msg));
  return 0;
}

/* spend (line 23): the budget point of totality. Each step uses one unit
   of fuel and scans the next pending term (next_terms, line 19). */
static int spend(Nat fuel, const Terms *terms, const KError **error) {
  for (; fuel > 0; fuel--) {
    if (terms == NULL) return 1;
    terms = term_scan_term(NO, NULL, terms->head, terms->tail).pending;
  }
  return empty(terms, error);
}

/* bodies (line 27): a right fold, so a loop plus a reverse. */
static const Terms *bodies(const SurfaceOrderMembers *ms) {
  const Terms *acc = NULL;
  for (; ms != NULL; ms = ms->tail) acc = Terms_cons(ms->head.body, acc);
  return Terms_reverse(acc);
}

/* guard_group (line 31). */
int surface_totality_guard_group(Budget budget, const Globals *globals,
                                 const SurfaceOrderMembers *members,
                                 const SurfaceOrderCertificate **out,
                                 const KError **error) {
  if (!spend(budget.remaining, bodies(members), error)) return 0;
  return surface_order_certify(SURFACE_ORDER_STRUCTURAL, members, out, error);
}

/* guard (line 42), with guard_result (line 37) and guarded_arg (line 34)
   inline. */
int surface_totality_guard(Budget budget, const Globals *globals, Text name,
                           const Term *ty, const Term *body, Bool *has_arg,
                           Int63 *arg, const KError **error) {
  SurfaceOrderMember member;
  const SurfaceOrderCertificate *cert = NULL;
  member.name = name;
  member.body = body;
  if (!surface_totality_guard_group(budget, globals,
                                    SurfaceOrderMembers_cons(member, NULL),
                                    &cert, error))
    return 0;
  *has_arg = cert != NULL ? YES : NO;
  if (cert != NULL) *arg = cert->arg;
  return 1;
}
