/* kernel_recursor_instance.c: C twin of lib/kernel_recursor_instance.bend
   (stage S3, session 2). Validate the parameter diagram before the values
   of a recursor instance are extracted. Conventions of kernel_recursor.c.
   The pair F.Pair2<S.t<V.t>, List<V.vleg>> of V.as_sec is a ValueSec. */
#include "sole.h"

/* The first arm is a self tail call: a loop. */
Bool recursor_instance_nullary(const VLegs *legs) {
  for (; legs != NULL; legs = legs->tail)
    if (legs->head->binders != NULL) return NO;
  return YES;
}

/* coll_width at V.t is rules_collection_view (kernel_rules.c). */
int recursor_instance_checked(const RulesHost *host, RulesCtx c, Int63 width,
                              const ValueSec *pair, const Values **out,
                              const KError **error) {
  Int63 actual;
  if (!rules_collection_view(pair->shape, &actual)) {
    *error = error_mismatch(rules_diagram_msg());
    return 0;
  }
  if (int63_equal(actual, width) &&
      int63_equal(rules_advance(int63_zero(), VLegs_length(pair->legs)),
                  width) &&
      recursor_instance_nullary(pair->legs))
    return rules_mu_param_legs(c, pair->legs, NULL, out, error);
  *error = error_mismatch(rules_diagram_msg());
  return 0;
}

int recursor_instance_section(const RulesHost *host, RulesCtx c, Int63 width,
                              const Value *value, const Values **out,
                              const KError **error) {
  ValueSec pair;
  if (!value_as_sec(value, &pair)) {
    *error = error_mismatch(rules_diagram_msg());
    return 0;
  }
  return recursor_instance_checked(host, c, width, &pair, out, error);
}

int recursor_instance_environment(const RulesHost *host, RulesCtx c,
                                  const Telescope *params, const Closure *d,
                                  const Values **out, const KError **error) {
  const Value *value;
  if (!rules_c_open(c, d, NULL, &value, error)) return 0;
  return recursor_instance_section(
      host, c, rules_advance(int63_zero(), Telescope_length(params)), value,
      out, error);
}
