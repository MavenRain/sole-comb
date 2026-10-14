/* kernel_term.c: C twin of lib/kernel_term.bend (stage S1b). */
#include "sole.h"

/* ---------------------------------------------------------------- t */

static Term *term_cell(TermTag tag) {
  Term *term = arena_alloc(sizeof *term);
  term->tag = tag;
  term->memo_state = MEMO_UNSEEN;
  term->memo.globals = NULL;
  term->memo.value = NULL;
  return term;
}

static const Term term_auto_cell = {TERM_AUTO, MEMO_UNSEEN, {{0}},
                                    {NULL, NULL}};

const Term *term_var(Int63 index) {
  Term *term = term_cell(TERM_VAR);
  term->as.var.index = index;
  return term;
}

const Term *term_univ(Level level) {
  Term *term = term_cell(TERM_UNIV);
  term->as.univ.level = level;
  return term;
}

const Term *term_lan(const TermShape *shape, const Term *diagram) {
  Term *term = term_cell(TERM_LAN);
  term->as.lan.shape = shape;
  term->as.lan.diagram = diagram;
  return term;
}

const Term *term_ran(const TermShape *shape, const Term *diagram) {
  Term *term = term_cell(TERM_RAN);
  term->as.ran.shape = shape;
  term->as.ran.diagram = diagram;
  return term;
}

const Term *term_in(const TermShape *shape, const TermAddr *address,
                    const Terms *args) {
  Term *term = term_cell(TERM_IN);
  term->as.in.shape = shape;
  term->as.in.address = address;
  term->as.in.args = args;
  return term;
}

const Term *term_elim(const TermElim *elim) {
  Term *term = term_cell(TERM_ELIM);
  term->as.elim.elim = elim;
  return term;
}

const Term *term_sec(const TermShape *shape, const TermLegs *legs) {
  Term *term = term_cell(TERM_SEC);
  term->as.sec.shape = shape;
  term->as.sec.legs = legs;
  return term;
}

const Term *term_out(const TermShape *shape, const TermAddr *address,
                     const Term *head) {
  Term *term = term_cell(TERM_OUT);
  term->as.out.shape = shape;
  term->as.out.address = address;
  term->as.out.head = head;
  return term;
}

const Term *term_let(Text name, const Term *ty, const Term *value,
                     const Term *body) {
  Term *term = term_cell(TERM_LET);
  term->as.let.name = name;
  term->as.let.ty = ty;
  term->as.let.value = value;
  term->as.let.body = body;
  return term;
}

const Term *term_ann(const Term *tm, const Term *ty) {
  Term *term = term_cell(TERM_ANN);
  term->as.ann.term = tm;
  term->as.ann.ty = ty;
  return term;
}

const Term *term_global(Text name) {
  Term *term = term_cell(TERM_GLOBAL);
  term->as.global.name = name;
  term->as.global.seen.globals = NULL;
  term->as.global.seen.entry = NULL;
  return term;
}

const Term *term_lit(const Literal *literal) {
  Term *term = term_cell(TERM_LIT);
  term->as.lit.literal = literal;
  return term;
}

const Term *term_auto(void) { return &term_auto_cell; }

/* ------------------------------------------- AddrData, LegData, ... */

static TermAddr *term_addr_cell(TermAddrTag tag) {
  TermAddr *address = arena_alloc(sizeof *address);
  address->tag = tag;
  return address;
}

const TermAddr *term_apt(Quantity quantity, const Term *arg) {
  TermAddr *address = term_addr_cell(TERM_APT);
  address->as.apt.quantity = quantity;
  address->as.apt.arg = arg;
  return address;
}

const TermAddr *term_aleg(Int63 index) {
  TermAddr *address = term_addr_cell(TERM_ALEG);
  address->as.aleg.index = index;
  return address;
}

const TermAddr *term_actor(Text name) {
  TermAddr *address = term_addr_cell(TERM_ACTOR);
  address->as.actor.name = name;
  return address;
}

const TermLeg *term_leg(const TermBinders *binders, const Term *body) {
  TermLeg *leg = arena_alloc(sizeof *leg);
  leg->binders = binders;
  leg->body = body;
  return leg;
}

const TermMotive *term_motive(MaybeText ind, const Texts *idx, Text self,
                              const Term *body) {
  TermMotive *motive = arena_alloc(sizeof *motive);
  motive->ind = ind;
  motive->idx = idx;
  motive->self = self;
  motive->body = body;
  return motive;
}

const TermElim *term_elimination(const TermShape *shape, const Term *scrut,
                                 Quantity scrut_q, const TermMotive *motive,
                                 const TermBranches *branches) {
  TermElim *elim = arena_alloc(sizeof *elim);
  elim->shape = shape;
  elim->scrut = scrut;
  elim->scrut_q = scrut_q;
  elim->motive = motive;
  elim->branches = branches;
  return elim;
}

/* ---------------------------------------------------------------- defs */

const Texts *term_formers(void) {
  return Texts_cons(text_of_cstring("Lan"),
         Texts_cons(text_of_cstring("Ran"), NULL));
}

const Texts *term_schema(void) {
  return Texts_cons(text_of_cstring("In"),
         Texts_cons(text_of_cstring("Elim"),
         Texts_cons(text_of_cstring("Sec"),
         Texts_cons(text_of_cstring("Out"), NULL))));
}

int term_as_apt(const TermAddr *address, TermPoint *out) {
  switch (address->tag) {
  case TERM_APT:
    out->quantity = address->as.apt.quantity;
    out->arg = address->as.apt.arg;
    return 1;
  case TERM_ALEG: return 0;
  case TERM_ACTOR: return 0;
  }
  return 0;
}

int term_as_aleg(const TermAddr *address, Int63 *out) {
  switch (address->tag) {
  case TERM_APT: return 0;
  case TERM_ALEG:
    *out = address->as.aleg.index;
    return 1;
  case TERM_ACTOR: return 0;
  }
  return 0;
}

MaybeText term_as_actor(const TermAddr *address) {
  switch (address->tag) {
  case TERM_APT: return maybe_text_none();
  case TERM_ALEG: return maybe_text_none();
  case TERM_ACTOR: return maybe_text_some(address->as.actor.name);
  }
  return maybe_text_none();
}

/* member: a loop over names. */
Bool term_member(Text name, const Texts *names) {
  for (; names != NULL; names = names->tail)
    if (text_same(name, names->head)) return YES;
  return NO;
}

Bool term_family_occurs(Bool include_families, const Texts *names,
                        MaybeText family) {
  if (include_families == NO) return NO;
  if (family.some == NO) return NO;
  return term_member(family.text, names);
}

const Terms *term_addr_terms(const TermAddr *address, const Terms *rest) {
  switch (address->tag) {
  case TERM_APT: return Terms_cons(address->as.apt.arg, rest);
  case TERM_ALEG: return rest;
  case TERM_ACTOR: return rest;
  }
  return rest;
}

/* leg_terms: the leg bodies in order (a loop, then a reverse). */
const Terms *term_leg_terms(const TermLegs *legs) {
  const Terms *bodies = NULL;
  for (; legs != NULL; legs = legs->tail)
    bodies = Terms_cons(legs->head->body, bodies);
  return Terms_reverse(bodies);
}

const Terms *term_motive_terms(const TermMotive *motive, const Terms *rest) {
  if (motive == NULL) return rest;
  return Terms_cons(motive->body, rest);
}

/* branch_terms: for each branch, the APt argument (if any), then the leg
   body (addr_terms(address, body <> next)). */
const Terms *term_branch_terms(const TermBranches *branches) {
  const Terms *terms = NULL;
  for (; branches != NULL; branches = branches->tail) {
    TermPoint point;
    if (term_as_apt(branches->head.address, &point))
      terms = Terms_cons(point.arg, terms);
    terms = Terms_cons(branches->head.leg->body, terms);
  }
  return Terms_reverse(terms);
}

TermScan term_scan_shape(Bool include_families, const Texts *names,
                         const TermShape *shape, const Terms *rest) {
  TermScan scan;
  scan.found = term_family_occurs(include_families, names,
                                  shape_family_term(shape));
  scan.pending = Terms_append(shape_payload_term(shape), rest);
  return scan;
}

static TermScan term_scan_of(Bool found, const Terms *pending) {
  TermScan scan;
  scan.found = found;
  scan.pending = pending;
  return scan;
}

/* scan_term: replaces a node by its strict subterms, in the pin's
   traversal order. */
TermScan term_scan_term(Bool include_families, const Texts *names,
                        const Term *term, const Terms *rest) {
  switch (term->tag) {
  case TERM_VAR: return term_scan_of(NO, rest);
  case TERM_UNIV: return term_scan_of(NO, rest);
  case TERM_LIT: return term_scan_of(NO, rest);
  case TERM_AUTO: return term_scan_of(NO, rest);
  case TERM_GLOBAL:
    return term_scan_of(term_member(term->as.global.name, names), rest);
  case TERM_LAN:
    return term_scan_shape(include_families, names, term->as.lan.shape,
                           Terms_cons(term->as.lan.diagram, rest));
  case TERM_RAN:
    return term_scan_shape(include_families, names, term->as.ran.shape,
                           Terms_cons(term->as.ran.diagram, rest));
  case TERM_IN:
    return term_scan_shape(
        include_families, names, term->as.in.shape,
        term_addr_terms(term->as.in.address,
                        Terms_append(term->as.in.args, rest)));
  case TERM_SEC:
    return term_scan_shape(
        include_families, names, term->as.sec.shape,
        Terms_append(term_leg_terms(term->as.sec.legs), rest));
  case TERM_OUT:
    return term_scan_shape(
        include_families, names, term->as.out.shape,
        term_addr_terms(term->as.out.address,
                        Terms_cons(term->as.out.head, rest)));
  case TERM_ELIM: {
    const TermElim *elim = term->as.elim.elim;
    return term_scan_shape(
        include_families, names, elim->shape,
        Terms_cons(elim->scrut,
                   term_motive_terms(
                       elim->motive,
                       Terms_append(term_branch_terms(elim->branches),
                                    rest))));
  }
  case TERM_LET:
    return term_scan_of(
        NO, Terms_cons(term->as.let.ty,
                       Terms_cons(term->as.let.value,
                                  Terms_cons(term->as.let.body, rest))));
  case TERM_ANN:
    return term_scan_of(
        NO, Terms_cons(term->as.ann.term, Terms_cons(term->as.ann.ty, rest)));
  }
  return term_scan_of(NO, rest);
}

/* scan (@unsafe in Bend): a loop over the pending list (worklist). */
Bool term_scan(Bool include_families, const Texts *names, TermScan state) {
  while (state.found == NO && state.pending != NULL)
    state = term_scan_term(include_families, names, state.pending->head,
                           state.pending->tail);
  return state.found;
}

Bool term_exists_name(Bool include_families, const Texts *names,
                      const Term *term) {
  return term_scan(include_families, names,
                   term_scan_of(NO, Terms_cons(term, NULL)));
}

/* exists_var (S14): YES if a T.Var occurs in term, at any binder depth. A
   term without a T.Var does not read its environment. The scan does not
   count binders, thus a bound variable also gives YES (a safe answer). */
Bool term_exists_var(const Term *term) {
  TermScan state = term_scan_of(NO, Terms_cons(term, NULL));
  while (state.found == NO && state.pending != NULL)
    state = state.pending->head->tag == TERM_VAR
                ? term_scan_of(YES, NULL)
                : term_scan_term(NO, NULL, state.pending->head,
                                 state.pending->tail);
  return state.found;
}

/* One pending term of term_closed: the term and the number of binders
   above it. */
typedef struct {
  const Term *term;
  Int63 depth;
} TermAt;
SOLE_LIST(TermsAt, TermAt)

static const TermsAt *closed_push(const Term *term, Int63 depth,
                                  const TermsAt *rest) {
  TermAt at = {term, depth};
  return TermsAt_cons(at, rest);
}

static const TermsAt *closed_push_all(const Terms *terms, Int63 depth,
                                      const TermsAt *rest) {
  for (; terms != NULL; terms = terms->tail)
    rest = closed_push(terms->head, depth, rest);
  return rest;
}

static const TermsAt *closed_push_shape(const TermShape *shape, Int63 depth,
                                        const TermsAt *rest) {
  return closed_push_all(shape_payload_term(shape), depth, rest);
}

static const TermsAt *closed_push_addr(const TermAddr *address, Int63 depth,
                                       const TermsAt *rest) {
  return closed_push_all(term_addr_terms(address, NULL), depth, rest);
}

static const TermsAt *closed_push_legs(const TermLegs *legs, Int63 depth,
                                       const TermsAt *rest) {
  for (; legs != NULL; legs = legs->tail)
    rest = closed_push(
        legs->head->body,
        depth + (Int63)TermBinders_length(legs->head->binders), rest);
  return rest;
}

/* The binders of a T.Lan or T.Ran diagram: the point binder of an SPi
   former (rules_form_pi), none for the other shapes. */
static Int63 closed_diagram_binders(const TermShape *shape) {
  switch (shape->tag) {
  case SHAPE_SPI: return 1;
  case SHAPE_SCOLL:
  case SHAPE_SPAR:
  case SHAPE_SMU:
  case SHAPE_SNU: return 0;
  }
  return 0; /* each tag returns above */
}

static const TermsAt *closed_push_elim(const TermElim *elim, Int63 depth,
                                       const TermsAt *rest) {
  const TermBranches *branches = elim->branches;
  rest = closed_push_shape(elim->shape, depth,
                           closed_push(elim->scrut, depth, rest));
  if (elim->motive != NULL)
    rest = closed_push(elim->motive->body,
                       depth + (Int63)Texts_length(elim->motive->idx) + 1,
                       rest);
  for (; branches != NULL; branches = branches->tail)
    rest = closed_push_addr(
        branches->head.address, depth,
        closed_push(branches->head.leg->body,
                    depth + (Int63)TermBinders_length(
                                branches->head.leg->binders),
                    rest));
  return rest;
}

/* closed (H6): YES if each T.Var of term is bound inside term. A closed
   term does not read its environment. Each binder form adds its binder
   count: a leg its binders, a motive its indices and self, a let body
   one, an SPi diagram one. A loop over the pending list (worklist). */
Bool term_closed(const Term *term) {
  const TermsAt *pending = closed_push(term, 0, NULL);
  while (pending != NULL) {
    TermAt at = pending->head;
    Int63 d = at.depth;
    pending = pending->tail;
    switch (at.term->tag) {
    case TERM_VAR:
      if (at.term->as.var.index >= d) return NO;
      break;
    case TERM_UNIV:
    case TERM_GLOBAL:
    case TERM_LIT:
    case TERM_AUTO: break;
    case TERM_LAN:
      pending = closed_push_shape(
          at.term->as.lan.shape, d,
          closed_push(at.term->as.lan.diagram,
                      d + closed_diagram_binders(at.term->as.lan.shape),
                      pending));
      break;
    case TERM_RAN:
      pending = closed_push_shape(
          at.term->as.ran.shape, d,
          closed_push(at.term->as.ran.diagram,
                      d + closed_diagram_binders(at.term->as.ran.shape),
                      pending));
      break;
    case TERM_IN:
      pending = closed_push_shape(
          at.term->as.in.shape, d,
          closed_push_addr(at.term->as.in.address, d,
                           closed_push_all(at.term->as.in.args, d, pending)));
      break;
    case TERM_SEC:
      pending = closed_push_shape(
          at.term->as.sec.shape, d,
          closed_push_legs(at.term->as.sec.legs, d, pending));
      break;
    case TERM_OUT:
      pending = closed_push_shape(
          at.term->as.out.shape, d,
          closed_push_addr(at.term->as.out.address, d,
                           closed_push(at.term->as.out.head, d, pending)));
      break;
    case TERM_ELIM:
      pending = closed_push_elim(at.term->as.elim.elim, d, pending);
      break;
    case TERM_LET:
      pending = closed_push(
          at.term->as.let.ty, d,
          closed_push(at.term->as.let.value, d,
                      closed_push(at.term->as.let.body, d + 1, pending)));
      break;
    case TERM_ANN:
      pending = closed_push(at.term->as.ann.term, d,
                            closed_push(at.term->as.ann.ty, d, pending));
      break;
    }
  }
  return YES;
}
