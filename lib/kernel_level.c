/* kernel_level.c: C twin of lib/kernel_level.bend (stage S1). */
#include "sole.h"

/* zero */
Level level_zero(void) { return bignum_zero(); }

/* one */
Level level_one(void) { return bignum_one(); }

/* succ: Stage A succ keeps the signed-int wrap of the pin. */
Level level_succ(Level level) { return bignum_succ_int63(level); }

/* max */
Level level_max(Level a, Level b) {
  return order_le(bignum_compare(a, b)) ? b : a;
}

/* equal */
Bool level_equal(Level a, Level b) { return bignum_equal(a, b); }

/* le */
Bool level_le(Level a, Level b) { return order_le(bignum_compare(a, b)); }

/* of_int */
int level_of_int(Int63 n, Level *out) {
  if (!level_le(level_zero(), bignum_of_int(n))) return 0;
  *out = bignum_of_int(n);
  return 1;
}

/* of_nat */
Level level_of_nat(Nat n) { return bignum_from_nat(n); }

/* to_string */
Text level_to_string(Level level) { return bignum_to_string(level); }
