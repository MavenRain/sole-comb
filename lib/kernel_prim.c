/* kernel_prim.c: C twin of lib/kernel_prim.bend (stage S2, session 6).
   Five Nat primitives from Kanon 69f3be51 prim.ml, before the Stage B
   delta. A Maybe of a pointer is NULL for None{}; a Maybe of a value is an
   int return (1 = Some, with *out). */
#include "sole.h"

Text prim_name(Prim prim) {
  switch (prim) {
  case PRIM_NAT_ADD: return text_of_cstring("natAdd");
  case PRIM_NAT_SUB: return text_of_cstring("natSub");
  case PRIM_NAT_MUL: return text_of_cstring("natMul");
  case PRIM_NAT_EQ: return text_of_cstring("natEq");
  case PRIM_NAT_LT: return text_of_cstring("natLt");
  }
  return text_end(); /* each constructor returns above */
}

Nat prim_arity(Prim prim) {
  switch (prim) {
  case PRIM_NAT_ADD:
  case PRIM_NAT_SUB:
  case PRIM_NAT_MUL:
  case PRIM_NAT_EQ:
  case PRIM_NAT_LT:
    return 2;
  }
  return 0; /* each constructor returns above */
}

const Prims *prim_catalog(void) {
  return Prims_cons(
      PRIM_NAT_ADD,
      Prims_cons(PRIM_NAT_SUB,
                 Prims_cons(PRIM_NAT_MUL,
                            Prims_cons(PRIM_NAT_EQ,
                                       Prims_cons(PRIM_NAT_LT, NULL)))));
}

/* The self tail call is the loop. */
int prim_lookup(Text source, const Prims *prims, Prim *out) {
  for (; prims != NULL; prims = prims->tail) {
    if (text_same(source, prim_name(prims->head))) {
      *out = prims->head;
      return 1;
    }
  }
  return 0;
}

int prim_of_name(Text source, Prim *out) {
  return prim_lookup(source, prim_catalog(), out);
}

Text prim_nat_name(void) { return text_of_cstring("Nat"); }

const Term *prim_nat_ty(void) { return term_global(prim_nat_name()); }

const Term *prim_binary(const Term *result) {
  return rules_arrow(QUANTITY_MANY, text_of_cstring("a"), prim_nat_ty(),
                     rules_arrow(QUANTITY_MANY, text_of_cstring("b"),
                                 prim_nat_ty(), result));
}

const Term *prim_ty(Prim prim) {
  switch (prim) {
  case PRIM_NAT_ADD:
  case PRIM_NAT_SUB:
  case PRIM_NAT_MUL:
    return prim_binary(prim_nat_ty());
  case PRIM_NAT_EQ:
  case PRIM_NAT_LT:
    return prim_binary(rules_bool_ty());
  }
  return NULL; /* each constructor returns above */
}

int prim_nonnegative(Cmp sign, Bignum value, Bignum *out) {
  switch (sign) {
  case CMP_LT: return 0;
  case CMP_EQ:
  case CMP_GT:
    *out = value;
    return 1;
  }
  return 0; /* each constructor returns above */
}

int prim_as_nat(const Literal *literal, Bignum *out) {
  switch (literal->tag) {
  case LITERAL_LINT:
    return prim_nonnegative(bignum_sign(literal->as.lint.value),
                            literal->as.lint.value, out);
  case LITERAL_LSTRING: return 0;
  }
  return 0; /* each constructor returns above */
}

/* add, sub and mul are Done{...}: they cannot fail. */
int prim_add(Bignum a, Bignum b, Bignum *out, const KError **error) {
  (void)error;
  *out = bignum_add(a, b);
  return 1;
}

int prim_sub(Bignum a, Bignum b, Bignum *out, const KError **error) {
  (void)error;
  *out = bignum_sub(a, b);
  return 1;
}

int prim_mul(Bignum a, Bignum b, Bignum *out, const KError **error) {
  (void)error;
  *out = bignum_mul(a, b);
  return 1;
}

/* a, b NULL = None{}. */
int prim_natural_pair(const Bignum *a, const Bignum *b, PrimNats *out) {
  if (a == NULL || b == NULL)
    return 0;
  out->first = *a;
  out->second = *b;
  return 1;
}

int prim_two_nats(const Literals *args, PrimNats *out) {
  Bignum a;
  Bignum b;
  int has_a;
  int has_b;
  if (args == NULL || args->tail == NULL || args->tail->tail != NULL)
    return 0;
  /* Both as_nat calls run, as the Bend natural_pair arguments. */
  has_a = prim_as_nat(args->head, &a);
  has_b = prim_as_nat(args->tail->head, &b);
  return prim_natural_pair(has_a ? &a : NULL, has_b ? &b : NULL, out);
}

/* Defunctionalized arithmetic and comparison rows retain the pinned split. */
const Literal *prim_reduce_nats(Prim prim, Bignum a, Bignum b) {
  switch (prim) {
  case PRIM_NAT_ADD: return literal_lint(bignum_add(a, b));
  case PRIM_NAT_SUB: return literal_lint(bignum_sub(a, b));
  case PRIM_NAT_MUL: return literal_lint(bignum_mul(a, b));
  case PRIM_NAT_EQ:
  case PRIM_NAT_LT:
    return NULL;
  }
  return NULL; /* each constructor returns above */
}

/* pair NULL = None{}. */
const Literal *prim_reduce_pair(Prim prim, const PrimNats *pair) {
  return pair == NULL ? NULL
                      : prim_reduce_nats(prim, pair->first, pair->second);
}

/* Done{...}: *out NULL = None{}. */
int prim_reduce(Prim prim, const Literals *args, const Literal **out,
                const KError **error) {
  PrimNats pair;
  (void)error;
  *out = prim_reduce_pair(prim, prim_two_nats(args, &pair) ? &pair : NULL);
  return 1;
}

Bool prim_less(Cmp order) {
  switch (order) {
  case CMP_LT: return YES;
  case CMP_EQ:
  case CMP_GT:
    return NO;
  }
  return NO; /* each constructor returns above */
}

const Value *prim_apply_nats(Prim prim, Bignum a, Bignum b) {
  switch (prim) {
  case PRIM_NAT_ADD: return value_vlit(literal_lint(bignum_add(a, b)));
  case PRIM_NAT_SUB: return value_vlit(literal_lint(bignum_sub(a, b)));
  case PRIM_NAT_MUL: return value_vlit(literal_lint(bignum_mul(a, b)));
  case PRIM_NAT_EQ: return rules_bool_value(bignum_equal(a, b));
  case PRIM_NAT_LT: return rules_bool_value(prim_less(bignum_compare(a, b)));
  }
  return NULL; /* each constructor returns above */
}

/* pair NULL = None{}. */
const Value *prim_apply_pair(Prim prim, const PrimNats *pair) {
  return pair == NULL ? NULL
                      : prim_apply_nats(prim, pair->first, pair->second);
}

/* Done{...}: *out NULL = None{}. */
int prim_apply(Prim prim, const Literals *args, const Value **out,
               const KError **error) {
  PrimNats pair;
  (void)error;
  *out = prim_apply_pair(prim, prim_two_nats(args, &pair) ? &pair : NULL);
  return 1;
}
