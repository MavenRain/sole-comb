/* kernel_positivity.c: C twin of lib/kernel_positivity.bend (stage S1b).

   result = Result<E.t, Unit> is an int (1 = Done{Unit{}}, 0 = Fail{e} with
   the error in *error). bind_result is `&&`; option_case is an if on NULL
   or on the as_apt int. */
#include "sole.h"

/* ------------------------------------------------------ ctor_status */

static const CtorStatus provisional_cell = {CTOR_PROVISIONAL, {{NULL}}};
static const CtorStatus builtin_cell = {CTOR_BUILTIN, {{NULL}}};

const CtorStatus *positivity_provisional(void) { return &provisional_cell; }

const CtorStatus *positivity_builtin(void) { return &builtin_cell; }

const CtorStatus *positivity_complete(const Texts *names) {
  CtorStatus *status = arena_alloc(sizeof *status);
  status->tag = CTOR_COMPLETE;
  status->as.complete.names = names;
  return status;
}

/* ------------------------------------------------------ ctor, family */

const Ctor *positivity_ctor(Text name, const Telescope *args,
                            const Terms *res_idx, Int63 full_arity,
                            Bool self_rec) {
  Ctor *ctor = arena_alloc(sizeof *ctor);
  ctor->name = name;
  ctor->args = args;
  ctor->res_idx = res_idx;
  ctor->full_arity = full_arity;
  ctor->self_rec = self_rec;
  return ctor;
}

const Family *positivity_family(Text name, const Telescope *params,
                                const Telescope *indices, Level level,
                                const CtorStatus *status, const Ctors *ctors,
                                Bool positive) {
  Family *family = arena_alloc(sizeof *family);
  family->name = name;
  family->params = params;
  family->indices = indices;
  family->level = level;
  family->status = status;
  family->ctors = ctors;
  family->positive = positive;
  return family;
}

/* ---------------------------------------------------------------- defs */

Text positivity_ctor_name(const Ctor *value) { return value->name; }

/* find_ctor: a loop over ctors. */
const Ctor *positivity_find_ctor(Text name, const Ctors *ctors) {
  for (; ctors != NULL; ctors = ctors->tail)
    if (text_same(name, positivity_ctor_name(ctors->head))) return ctors->head;
  return NULL;
}

const Ctor *positivity_ctor_of(Text name, const Family *fam) {
  return positivity_find_ctor(name, fam->ctors);
}

Text positivity_nonpositive_word(void) {
  return text_of_cstring(
      "a family that is not strictly positive arrives at M2");
}

Bool positivity_occurs(const Texts *names, const Term *term) {
  return term_exists_name(YES, names, term);
}

Bool positivity_is_member(const Texts *names, MaybeText name) {
  if (name.some == NO) return NO;
  return term_member(name.text, names);
}

int positivity_absent(const Texts *names, const Term *term,
                      const KError **error) {
  if (positivity_occurs(names, term)) {
    *error = error_not_yet(positivity_nonpositive_word());
    return 0;
  }
  return 1;
}

/* absent_list: a loop over terms. */
int positivity_absent_list(const Texts *names, const Terms *terms,
                           const KError **error) {
  for (; terms != NULL; terms = terms->tail)
    if (!positivity_absent(names, terms->head, error)) return 0;
  return 1;
}

int positivity_absent_address(const Texts *names, TermPoint address,
                              const KError **error) {
  return positivity_absent(names, address.arg, error);
}

/* positive: recursion over term depth (Ran codomain, Out head). Where the
   Bend rebuilds the matched term for absent (T.Lan{s, d}), the C code
   passes the same term. */
int positivity_positive(const Texts *names, const Term *term,
                        const KError **error) {
  switch (term->tag) {
  case TERM_LAN: {
    const TermShape *s = term->as.lan.shape;
    if (positivity_is_member(names, shape_family_term(s)))
      return positivity_absent_list(names, shape_payload_term(s), error) &&
             positivity_absent(names, term->as.lan.diagram, error);
    return positivity_absent(names, term, error);
  }
  case TERM_RAN: {
    const Term *dom = shape_point_dom_term(term->as.ran.shape);
    if (dom == NULL) return positivity_absent(names, term, error);
    return positivity_absent(names, dom, error) &&
           positivity_positive(names, term->as.ran.diagram, error);
  }
  case TERM_OUT: {
    TermPoint arg;
    if (shape_point_dom_term(term->as.out.shape) == NULL)
      return positivity_absent(names, term, error);
    if (!term_as_apt(term->as.out.address, &arg))
      return positivity_absent(names, term, error);
    return positivity_absent_address(names, arg, error) &&
           positivity_positive(names, term->as.out.head, error);
  }
  case TERM_GLOBAL: return 1;
  case TERM_VAR: return positivity_absent(names, term, error);
  case TERM_UNIV: return positivity_absent(names, term, error);
  case TERM_IN: return positivity_absent(names, term, error);
  case TERM_ELIM: return positivity_absent(names, term, error);
  case TERM_SEC: return positivity_absent(names, term, error);
  case TERM_LET: return positivity_absent(names, term, error);
  case TERM_ANN: return positivity_absent(names, term, error);
  case TERM_LIT: return positivity_absent(names, term, error);
  case TERM_AUTO: return positivity_absent(names, term, error);
  }
  return positivity_absent(names, term, error);
}

/* ctor_fields: a loop over the telescope. */
int positivity_ctor_fields(const Texts *names, const Telescope *args,
                           const KError **error) {
  for (; args != NULL; args = args->tail)
    if (!positivity_positive(names, args->head.binding.type, error)) return 0;
  return 1;
}

/* self_rec: a loop over the telescope (Bool.or of pure values). */
Bool positivity_self_rec(const Texts *names, const Telescope *args) {
  for (; args != NULL; args = args->tail)
    if (positivity_occurs(names, args->head.binding.type)) return YES;
  return NO;
}
