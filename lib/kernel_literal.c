/* kernel_literal.c: C twin of lib/kernel_literal.bend (stage S1). */
#include "sole.h"

const Literal *literal_lstring(Text value) {
  Literal *literal = arena_alloc(sizeof *literal);
  literal->tag = LITERAL_LSTRING;
  literal->as.lstring.value = value;
  return literal;
}

const Literal *literal_lint(Bignum value) {
  Literal *literal = arena_alloc(sizeof *literal);
  literal->tag = LITERAL_LINT;
  literal->as.lint.value = value;
  return literal;
}

/* equal */
Bool literal_equal(const Literal *a, const Literal *b) {
  switch (a->tag) {
  case LITERAL_LSTRING:
    switch (b->tag) {
    case LITERAL_LSTRING:
      return text_same(a->as.lstring.value, b->as.lstring.value);
    case LITERAL_LINT: return NO;
    }
    break;
  case LITERAL_LINT:
    switch (b->tag) {
    case LITERAL_LSTRING: return NO;
    case LITERAL_LINT: return bignum_equal(a->as.lint.value, b->as.lint.value);
    }
    break;
  }
  return NO;
}
