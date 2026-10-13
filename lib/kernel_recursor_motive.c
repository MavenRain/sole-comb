/* kernel_recursor_motive.c: C twin of lib/kernel_recursor_motive.bend
   (stage S3, session 2). Checked motive and induction-hypothesis types for
   direct recursive layouts. Conventions of kernel_recursor.c. */
#include "sole.h"

const TermLegs *recursor_motive_legs(const Telescope *tele) {
  if (tele == NULL) return NULL;
  return TermLegs_cons(
      term_leg(NULL, term_var(rules_advance(int63_zero(),
                                            Telescope_length(tele->tail)))),
      recursor_motive_legs(tele->tail));
}

const Closure *recursor_motive_diagram(const Telescope *params, RulesCtx c) {
  return value_closure(
      runtime_ops_env(c.check),
      term_sec(shape_scoll_term(
                   rules_advance(int63_zero(), Telescope_length(params))),
               recursor_motive_legs(params)));
}

/* The Ordinary arm is a self tail call (a loop); the Direct arm recurses
   under pmap. */
int recursor_motive_hypotheses(const RulesHost *host, RulesCtx origin,
                               const TermMotive *mo,
                               const RecursorFields *fs,
                               const RecursorHypotheses **out,
                               const KError **error) {
  for (; fs != NULL; fs = fs->tail) {
    const RecursorField *f = fs->head;
    const Value *ty;
    const RecursorHypotheses *tail;
    RecursorHypothesis *h;
    switch (f->role.tag) {
    case RECURSOR_ORDINARY: continue;
    case RECURSOR_DIRECT:
      if (!rules_mu_result(origin, mo, f->role.indices, f->value, &ty, error))
        return 0;
      if (!recursor_motive_hypotheses(host, origin, mo, fs->tail, &tail,
                                      error))
        return 0;
      h = (RecursorHypothesis *)arena_alloc(sizeof *h);
      h->position = f->position;
      h->name = f->name;
      h->domain = ty;
      *out = RecursorHypotheses_cons(h, tail);
      return 1;
    }
  }
  *out = NULL;
  return 1;
}

int recursor_motive_checked(const RulesHost *host, RulesCtx origin,
                            const TermMotive *mo, Level lvl, RecursorPlan raw,
                            RecursorMotivePlan *out, const KError **error) {
  const RecursorHypotheses *hs;
  if (!recursor_motive_hypotheses(host, origin, mo, raw.fields, &hs, error))
    return 0;
  out->layout = raw;
  out->level = lvl;
  out->hypotheses = hs;
  return 1;
}

int recursor_motive_motive(const RulesHost *host, RulesCtx origin,
                           Text owner, const Family *fam,
                           const TermMotive *mo, RecursorPlan raw,
                           RecursorMotivePlan *out, const KError **error) {
  Int63 count = rules_advance(int63_zero(),
                              Telescope_length(rules_family_indices(fam)));
  Level declared = rules_family_level(fam);
  const TermMotive *valid;
  Level lvl;
  if (!rules_mu_motive_view(owner, count, count, mo, mo, &valid, error))
    return 0;
  if (!rules_mu_motive_lvl(
          host, origin, owner, fam, valid,
          recursor_motive_diagram(rules_family_params(fam), origin), &declared,
          runtime_ops_env(origin.check), &lvl, error))
    return 0;
  if (!rules_mu_large(owner, fam, lvl, error)) return 0;
  return recursor_motive_checked(host, origin, mo, lvl, raw, out, error);
}

int recursor_motive_layout(const RulesHost *host, RulesCtx c,
                           RecursorScope scope, Text owner, Text key,
                           const TermMotive *mo, RecursorMotivePlan *out,
                           const KError **error) {
  const Family *fam;
  RecursorPlan raw;
  RulesCtx origin;
  if (!rules_mu_family(c, owner, &fam, error)) return 0;
  if (!recursor_uniform_layout(host, c, scope, owner, key, &raw, error))
    return 0;
  if (!recursor_open(host, scope, rules_family_params(fam), c, &origin,
                     error))
    return 0;
  return recursor_motive_motive(host, origin, owner, fam, mo, raw, out,
                                error);
}
