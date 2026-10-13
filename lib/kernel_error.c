/* kernel_error.c: C twin of lib/kernel_error.bend (stage S1). The message
   strings are byte for byte the Bend strings. */
#include "sole.h"

static const KError *error_plain(KErrorTag tag, Text message) {
  KError *error = arena_alloc(sizeof *error);
  error->tag = tag;
  error->as.plain.message = message;
  return error;
}

const KError *error_not_yet(Text message) {
  return error_plain(ERROR_NOT_YET, message);
}

const KError *error_parse(Text message, Nat line, Nat column) {
  KError *error = arena_alloc(sizeof *error);
  error->tag = ERROR_PARSE;
  error->as.parse.message = message;
  error->as.parse.line = line;
  error->as.parse.column = column;
  return error;
}

const KError *error_carry(Text message) {
  return error_plain(ERROR_CARRY, message);
}

const KError *error_unbound(Text message) {
  return error_plain(ERROR_UNBOUND, message);
}

const KError *error_mismatch(Text message) {
  return error_plain(ERROR_MISMATCH, message);
}

const KError *error_universe(Text message) {
  return error_plain(ERROR_UNIVERSE, message);
}

const KError *error_quantity(Text message) {
  return error_plain(ERROR_QUANTITY, message);
}

const KError *error_wrong_leg(Text message) {
  return error_plain(ERROR_WRONG_LEG, message);
}

const KError *error_missing_branch(Text message) {
  return error_plain(ERROR_MISSING_BRANCH, message);
}

const KError *error_overflow(Text message) {
  return error_plain(ERROR_OVERFLOW, message);
}

const KError *error_cannot_infer(Text message) {
  return error_plain(ERROR_CANNOT_INFER, message);
}

const KError *error_budget_exhausted(Text message) {
  return error_plain(ERROR_BUDGET_EXHAUSTED, message);
}

const KError *error_index_not_zero(Text message) {
  return error_plain(ERROR_INDEX_NOT_ZERO, message);
}

const KError *error_index_above_universe(Text message) {
  return error_plain(ERROR_INDEX_ABOVE_UNIVERSE, message);
}

const KError *error_termination(Text name) {
  KError *error = arena_alloc(sizeof *error);
  error->tag = ERROR_TERMINATION;
  error->as.termination.name = name;
  return error;
}

/* termination_msg */
Text error_termination_msg(Text name) {
  TextBuilder builder = builder_new();
  builder_append_cstring(&builder, "recursive definition ");
  builder_append(&builder, name);
  builder_append_cstring(&builder, " failed the structural termination guard");
  return builder_text(&builder);
}

/* message */
Text error_message(const KError *error) {
  switch (error->tag) {
  case ERROR_NOT_YET: return error->as.plain.message;
  case ERROR_PARSE: return error->as.parse.message;
  case ERROR_CARRY: return error->as.plain.message;
  case ERROR_UNBOUND: return error->as.plain.message;
  case ERROR_MISMATCH: return error->as.plain.message;
  case ERROR_UNIVERSE: return error->as.plain.message;
  case ERROR_QUANTITY: return error->as.plain.message;
  case ERROR_WRONG_LEG: return error->as.plain.message;
  case ERROR_MISSING_BRANCH: return error->as.plain.message;
  case ERROR_OVERFLOW: return error->as.plain.message;
  case ERROR_CANNOT_INFER: return error->as.plain.message;
  case ERROR_BUDGET_EXHAUSTED: return error->as.plain.message;
  case ERROR_INDEX_NOT_ZERO: return error->as.plain.message;
  case ERROR_INDEX_ABOVE_UNIVERSE: return error->as.plain.message;
  case ERROR_TERMINATION:
    return error_termination_msg(error->as.termination.name);
  }
  return error->as.plain.message;
}

static Text error_prefixed(const char *prefix, Text rest) {
  TextBuilder builder = builder_new();
  builder_append_cstring(&builder, prefix);
  builder_append(&builder, rest);
  return builder_text(&builder);
}

/* to_string */
Text error_to_string(const KError *error) {
  switch (error->tag) {
  case ERROR_NOT_YET:
    return error_prefixed("not yet: ", error->as.plain.message);
  case ERROR_PARSE: {
    TextBuilder builder = builder_new();
    builder_append_cstring(&builder, "line ");
    builder_append(&builder,
                   bignum_to_string(bignum_from_nat(error->as.parse.line)));
    builder_append_cstring(&builder, ", column ");
    builder_append(&builder,
                   bignum_to_string(bignum_from_nat(error->as.parse.column)));
    builder_append_cstring(&builder, ": ");
    builder_append(&builder, error->as.parse.message);
    return builder_text(&builder);
  }
  case ERROR_CARRY:
    return error_prefixed("carry: ", error->as.plain.message);
  case ERROR_UNBOUND:
    return error_prefixed("unbound: ", error->as.plain.message);
  case ERROR_MISMATCH:
    return error_prefixed("mismatch: ", error->as.plain.message);
  case ERROR_UNIVERSE:
    return error_prefixed("universe: ", error->as.plain.message);
  case ERROR_QUANTITY:
    return error_prefixed("quantity: ", error->as.plain.message);
  case ERROR_WRONG_LEG:
    return error_prefixed("wrong leg: ", error->as.plain.message);
  case ERROR_MISSING_BRANCH:
    return error_prefixed("missing branch: ", error->as.plain.message);
  case ERROR_OVERFLOW:
    return error_prefixed("overflow: ", error->as.plain.message);
  case ERROR_CANNOT_INFER:
    return error_prefixed("cannot infer: ", error->as.plain.message);
  case ERROR_BUDGET_EXHAUSTED:
    return error_prefixed("budget: ", error->as.plain.message);
  case ERROR_INDEX_NOT_ZERO:
    return error_prefixed("index not zero: ", error->as.plain.message);
  case ERROR_INDEX_ABOVE_UNIVERSE:
    return error_prefixed("index above universe: ", error->as.plain.message);
  case ERROR_TERMINATION:
    return error_prefixed("termination: ",
                          error_termination_msg(error->as.termination.name));
  }
  return error->as.plain.message;
}
