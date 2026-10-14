/* kernel_value.c: C twin of lib/kernel_value.bend (stage S1b). NbE domain
   at Kanon 69f3be51 value.ml. Spines are newest first. */
#include "sole.h"

/* --------------------------------------------------------- records */

const Closure *value_closure(const Values *env, const Term *body) {
  Closure *closure = arena_alloc(sizeof *closure);
  closure->env = env;
  closure->body = body;
  return closure;
}

const VLeg *value_vleg(const TermBinders *binders, const Closure *clo) {
  VLeg *leg = arena_alloc(sizeof *leg);
  leg->binders = binders;
  leg->clo = clo;
  return leg;
}

const Stuck *value_stuck(const ValueShape *shape, Quantity scrut_q,
                         const TermMotive *motive,
                         const TermBranches *branches, const Values *env) {
  Stuck *stuck = arena_alloc(sizeof *stuck);
  stuck->shape = shape;
  stuck->scrut_q = scrut_q;
  stuck->motive = motive;
  stuck->branches = branches;
  stuck->env = env;
  return stuck;
}

/* ----------------------------------------------------------- vaddr */

static VAddr *value_vaddr_cell(VAddrTag tag) {
  VAddr *address = arena_alloc(sizeof *address);
  address->tag = tag;
  return address;
}

const VAddr *value_vapt(Quantity quantity, const Value *arg) {
  VAddr *address = value_vaddr_cell(VALUE_VAPT);
  address->as.vapt.quantity = quantity;
  address->as.vapt.arg = arg;
  return address;
}

const VAddr *value_valeg(Int63 index) {
  VAddr *address = value_vaddr_cell(VALUE_VALEG);
  address->as.valeg.index = index;
  return address;
}

const VAddr *value_vactor(Text name) {
  VAddr *address = value_vaddr_cell(VALUE_VACTOR);
  address->as.vactor.name = name;
  return address;
}

/* ------------------------------------------------------------ head */

const Head *value_hlocal(Int63 level) {
  Head *head = arena_alloc(sizeof *head);
  head->tag = VALUE_HLOCAL;
  head->as.hlocal.level = level;
  return head;
}

const Head *value_hglobal(Text name) {
  Head *head = arena_alloc(sizeof *head);
  head->tag = VALUE_HGLOBAL;
  head->as.hglobal.name = name;
  head->as.hglobal.seen.globals = NULL;
  head->as.hglobal.seen.entry = NULL;
  return head;
}

/* ----------------------------------------------------------- spine */

const Spine *value_sout(const ValueShape *shape, const VAddr *address) {
  Spine *spine = arena_alloc(sizeof *spine);
  spine->tag = VALUE_SOUT;
  spine->as.sout.shape = shape;
  spine->as.sout.address = address;
  return spine;
}

const Spine *value_selim(const Stuck *elim) {
  Spine *spine = arena_alloc(sizeof *spine);
  spine->tag = VALUE_SELIM;
  spine->as.selim.elim = elim;
  return spine;
}

/* --------------------------------------------------------------- t */

static Value *value_cell(ValueTag tag) {
  Value *value = arena_alloc(sizeof *value);
  value->tag = tag;
  return value;
}

const Value *value_vuniv(Level level) {
  Value *value = value_cell(VALUE_VUNIV);
  value->as.vuniv.level = level;
  return value;
}

const Value *value_vlan(const ValueShape *shape, const Closure *diagram,
                        const Level *universe) {
  Value *value = value_cell(VALUE_VLAN);
  value->as.vlan.shape = shape;
  value->as.vlan.diagram = diagram;
  value->as.vlan.universe = universe;
  return value;
}

const Value *value_vran(const ValueShape *shape, const Closure *diagram,
                        const Level *universe) {
  Value *value = value_cell(VALUE_VRAN);
  value->as.vran.shape = shape;
  value->as.vran.diagram = diagram;
  value->as.vran.universe = universe;
  return value;
}

const Value *value_vin(const ValueShape *shape, const VAddr *address,
                       const Values *args) {
  Value *value = value_cell(VALUE_VIN);
  value->as.vin.shape = shape;
  value->as.vin.address = address;
  value->as.vin.args = args;
  return value;
}

const Value *value_vsec(const ValueShape *shape, const VLegs *legs) {
  Value *value = value_cell(VALUE_VSEC);
  value->as.vsec.shape = shape;
  value->as.vsec.legs = legs;
  return value;
}

const Value *value_vlit(const Literal *literal) {
  Value *value = value_cell(VALUE_VLIT);
  value->as.vlit.literal = literal;
  return value;
}

const Value *value_vneutral(const Head *head, const Spines *spine) {
  Value *value = value_cell(VALUE_VNEUTRAL);
  value->as.vneutral.head = head;
  value->as.vneutral.spine = spine;
  return value;
}

/* ------------------------------------------------------- var, close */

const Value *value_var(Int63 level) {
  return value_vneutral(value_hlocal(level), NULL);
}

const Closure *value_close(const Values *env, const Term *body) {
  return value_closure(env, body);
}

/* ----------------------------------------------------------- views */

const Level *value_as_univ(const Value *value) {
  switch (value->tag) {
  case VALUE_VUNIV: return &value->as.vuniv.level;
  case VALUE_VLAN: return NULL;
  case VALUE_VRAN: return NULL;
  case VALUE_VIN: return NULL;
  case VALUE_VSEC: return NULL;
  case VALUE_VLIT: return NULL;
  case VALUE_VNEUTRAL: return NULL;
  }
  return NULL;
}

int value_as_lan(const Value *value, ValueFormer *out) {
  switch (value->tag) {
  case VALUE_VUNIV: return 0;
  case VALUE_VLAN:
    out->shape = value->as.vlan.shape;
    out->diagram = value->as.vlan.diagram;
    out->universe = value->as.vlan.universe;
    return 1;
  case VALUE_VRAN: return 0;
  case VALUE_VIN: return 0;
  case VALUE_VSEC: return 0;
  case VALUE_VLIT: return 0;
  case VALUE_VNEUTRAL: return 0;
  }
  return 0;
}

int value_as_ran(const Value *value, ValueFormer *out) {
  switch (value->tag) {
  case VALUE_VUNIV: return 0;
  case VALUE_VLAN: return 0;
  case VALUE_VRAN:
    out->shape = value->as.vran.shape;
    out->diagram = value->as.vran.diagram;
    out->universe = value->as.vran.universe;
    return 1;
  case VALUE_VIN: return 0;
  case VALUE_VSEC: return 0;
  case VALUE_VLIT: return 0;
  case VALUE_VNEUTRAL: return 0;
  }
  return 0;
}

int value_as_in(const Value *value, ValueIntro *out) {
  switch (value->tag) {
  case VALUE_VUNIV: return 0;
  case VALUE_VLAN: return 0;
  case VALUE_VRAN: return 0;
  case VALUE_VIN:
    out->shape = value->as.vin.shape;
    out->address = value->as.vin.address;
    out->args = value->as.vin.args;
    return 1;
  case VALUE_VSEC: return 0;
  case VALUE_VLIT: return 0;
  case VALUE_VNEUTRAL: return 0;
  }
  return 0;
}

int value_as_sec(const Value *value, ValueSec *out) {
  switch (value->tag) {
  case VALUE_VUNIV: return 0;
  case VALUE_VLAN: return 0;
  case VALUE_VRAN: return 0;
  case VALUE_VIN: return 0;
  case VALUE_VSEC:
    out->shape = value->as.vsec.shape;
    out->legs = value->as.vsec.legs;
    return 1;
  case VALUE_VLIT: return 0;
  case VALUE_VNEUTRAL: return 0;
  }
  return 0;
}

const Literal *value_as_lit(const Value *value) {
  switch (value->tag) {
  case VALUE_VUNIV: return NULL;
  case VALUE_VLAN: return NULL;
  case VALUE_VRAN: return NULL;
  case VALUE_VIN: return NULL;
  case VALUE_VSEC: return NULL;
  case VALUE_VLIT: return value->as.vlit.literal;
  case VALUE_VNEUTRAL: return NULL;
  }
  return NULL;
}

int value_as_neutral(const Value *value, ValueNeutral *out) {
  switch (value->tag) {
  case VALUE_VUNIV: return 0;
  case VALUE_VLAN: return 0;
  case VALUE_VRAN: return 0;
  case VALUE_VIN: return 0;
  case VALUE_VSEC: return 0;
  case VALUE_VLIT: return 0;
  case VALUE_VNEUTRAL:
    out->head = value->as.vneutral.head;
    out->spine = value->as.vneutral.spine;
    return 1;
  }
  return 0;
}

int value_as_pt(const VAddr *address, ValuePoint *out) {
  switch (address->tag) {
  case VALUE_VAPT:
    out->quantity = address->as.vapt.quantity;
    out->arg = address->as.vapt.arg;
    return 1;
  case VALUE_VALEG: return 0;
  case VALUE_VACTOR: return 0;
  }
  return 0;
}

int value_as_leg(const VAddr *address, Int63 *out) {
  switch (address->tag) {
  case VALUE_VAPT: return 0;
  case VALUE_VALEG:
    *out = address->as.valeg.index;
    return 1;
  case VALUE_VACTOR: return 0;
  }
  return 0;
}

MaybeText value_as_ctor(const VAddr *address) {
  switch (address->tag) {
  case VALUE_VAPT: return maybe_text_none();
  case VALUE_VALEG: return maybe_text_none();
  case VALUE_VACTOR: return maybe_text_some(address->as.vactor.name);
  }
  return maybe_text_none();
}

/* head_equal: HLocal with HLocal compares through Bignum.of_int, as the
   Bend does. */
Bool value_head_equal(const Head *a, const Head *b) {
  switch (a->tag) {
  case VALUE_HLOCAL:
    switch (b->tag) {
    case VALUE_HLOCAL:
      return bignum_equal(bignum_of_int(a->as.hlocal.level),
                          bignum_of_int(b->as.hlocal.level));
    case VALUE_HGLOBAL: return NO;
    }
    return NO;
  case VALUE_HGLOBAL:
    switch (b->tag) {
    case VALUE_HLOCAL: return NO;
    case VALUE_HGLOBAL:
      return text_same(a->as.hglobal.name, b->as.hglobal.name);
    }
    return NO;
  }
  return NO;
}
