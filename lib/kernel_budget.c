/* kernel_budget.c: C twin of lib/kernel_budget.bend (stage S1). */
#include "sole.h"

/* create */
Budget budget_create(Nat remaining) {
  Budget budget;
  budget.remaining = remaining;
  return budget;
}

/* exhausted */
Bool budget_exhausted(Budget budget) {
  return budget.remaining == 0 ? YES : NO;
}

/* spend: 0n fails with Budget_exhausted{context}; 1n+rest is Done{rest}. */
int budget_spend(Text context, Budget budget, Budget *out,
                 const KError **error) {
  if (budget.remaining == 0) {
    *error = error_budget_exhausted(context);
    return 0;
  }
  *out = budget_create(budget.remaining - 1);
  return 1;
}
