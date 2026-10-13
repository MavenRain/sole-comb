/* kernel_quantity.c: C twin of lib/kernel_quantity.bend (stage S1).

   if_else, interval_order, combine_order and map_paths take Bend thunks or
   lambdas; their twins are the if / switch at each call site. The lists of
   uses are in ascending level order; the list functions are loops that
   build the result in reverse and then reverse it. */
#include "sole.h"

static QuantityInterval quantity_pair(Quantity lo, Quantity hi) {
  QuantityInterval interval;
  interval.lo = lo;
  interval.hi = hi;
  return interval;
}

static QuantityUse quantity_row(Int63 level, QuantityInterval interval) {
  QuantityUse row;
  row.level = level;
  row.interval = interval;
  return row;
}

static QuantityPaths quantity_some(const QuantityUses *rows) {
  QuantityPaths paths;
  paths.some = YES;
  paths.rows = rows;
  return paths;
}

static QuantityPaths quantity_none(void) {
  QuantityPaths paths;
  paths.some = NO;
  paths.rows = NULL;
  return paths;
}

static QuantityUsage quantity_usage(QuantityPaths paths,
                                    const QuantityUses *reads) {
  QuantityUsage usage;
  usage.paths = paths;
  usage.reads = reads;
  return usage;
}

/* equal */
Bool quantity_equal(Quantity a, Quantity b) { return a == b ? YES : NO; }

/* mul */
Quantity quantity_mul(Quantity a, Quantity b) {
  switch (a) {
  case QUANTITY_ZERO: return QUANTITY_ZERO;
  case QUANTITY_ONE: return b;
  case QUANTITY_MANY:
    switch (b) {
    case QUANTITY_ZERO: return QUANTITY_ZERO;
    case QUANTITY_ONE: return QUANTITY_MANY;
    case QUANTITY_MANY: return QUANTITY_MANY;
    }
    break;
  }
  return QUANTITY_MANY;
}

/* to_string */
Text quantity_to_string(Quantity q) {
  switch (q) {
  case QUANTITY_ZERO: return text_of_cstring("0");
  case QUANTITY_ONE: return text_of_cstring("1");
  case QUANTITY_MANY: return text_of_cstring("w");
  }
  return text_of_cstring("w");
}

/* runtime */
Quantity quantity_runtime(Quantity q) {
  return quantity_equal(q, QUANTITY_ZERO) ? QUANTITY_ZERO : QUANTITY_ONE;
}

/* add */
Quantity quantity_add(Quantity a, Quantity b) {
  if (quantity_equal(a, QUANTITY_ZERO)) return b;
  return quantity_equal(b, QUANTITY_ZERO) ? a : QUANTITY_MANY;
}

/* minimum */
Quantity quantity_minimum(Quantity a, Quantity b) {
  switch (a) {
  case QUANTITY_ZERO: return QUANTITY_ZERO;
  case QUANTITY_ONE:
    return quantity_equal(b, QUANTITY_ZERO) ? QUANTITY_ZERO : QUANTITY_ONE;
  case QUANTITY_MANY: return b;
  }
  return b;
}

/* maximum */
Quantity quantity_maximum(Quantity a, Quantity b) {
  switch (a) {
  case QUANTITY_ZERO: return b;
  case QUANTITY_ONE:
    return quantity_equal(b, QUANTITY_MANY) ? QUANTITY_MANY : QUANTITY_ONE;
  case QUANTITY_MANY: return QUANTITY_MANY;
  }
  return QUANTITY_MANY;
}

/* empty */
QuantityUsage quantity_empty(void) {
  return quantity_usage(quantity_some(NULL), NULL);
}

/* unreachable */
QuantityUsage quantity_unreachable(void) {
  return quantity_usage(quantity_none(), NULL);
}

/* interval_order, interval: rows are ascending, so a lower level stops. */
QuantityInterval quantity_interval(Int63 level, const QuantityUses *rows) {
  for (; rows != NULL; rows = rows->tail) {
    switch (int63_compare(level, rows->head.level)) {
    case CMP_LT: return quantity_pair(QUANTITY_ZERO, QUANTITY_ZERO);
    case CMP_EQ: return rows->head.interval;
    case CMP_GT: break;
    }
  }
  return quantity_pair(QUANTITY_ZERO, QUANTITY_ZERO);
}

/* returning */
QuantityUsage quantity_returning(const QuantityUses *rows) {
  return quantity_usage(quantity_some(rows), rows);
}

/* occurrence */
QuantityUsage quantity_occurrence(Int63 level, Quantity q) {
  return quantity_returning(
      QuantityUses_cons(quantity_row(level, quantity_pair(q, q)), NULL));
}

static Quantity quantity_join(QuantityJoin op, Quantity x, Quantity y) {
  switch (op) {
  case QUANTITY_JOIN_ADD: return quantity_add(x, y);
  case QUANTITY_JOIN_MINIMUM: return quantity_minimum(x, y);
  case QUANTITY_JOIN_MAXIMUM: return quantity_maximum(x, y);
  case QUANTITY_JOIN_LEFT: return x;
  }
  return x;
}

/* join_interval */
QuantityInterval quantity_join_interval(QuantityJoin f, QuantityJoin g,
                                        QuantityInterval a,
                                        QuantityInterval b) {
  return quantity_pair(quantity_join(f, a.lo, b.lo),
                       quantity_join(g, a.hi, b.hi));
}

/* combine_order, combine: a merge of two ascending lists; a level that is
   in one list only joins with (Zero, Zero) on the missing side. */
const QuantityUses *quantity_combine(QuantityJoin f, QuantityJoin g,
                                     const QuantityUses *a,
                                     const QuantityUses *b) {
  const QuantityInterval none = quantity_pair(QUANTITY_ZERO, QUANTITY_ZERO);
  const QuantityUses *out = NULL;
  while (a != NULL || b != NULL) {
    if (a == NULL) {
      out = QuantityUses_cons(
          quantity_row(b->head.level,
                       quantity_join_interval(f, g, none, b->head.interval)),
          out);
      b = b->tail;
      continue;
    }
    if (b == NULL) {
      out = QuantityUses_cons(
          quantity_row(a->head.level,
                       quantity_join_interval(f, g, a->head.interval, none)),
          out);
      a = a->tail;
      continue;
    }
    switch (int63_compare(a->head.level, b->head.level)) {
    case CMP_LT:
      out = QuantityUses_cons(
          quantity_row(a->head.level,
                       quantity_join_interval(f, g, a->head.interval, none)),
          out);
      a = a->tail;
      break;
    case CMP_EQ:
      out = QuantityUses_cons(
          quantity_row(a->head.level,
                       quantity_join_interval(f, g, a->head.interval,
                                              b->head.interval)),
          out);
      a = a->tail;
      b = b->tail;
      break;
    case CMP_GT:
      out = QuantityUses_cons(
          quantity_row(b->head.level,
                       quantity_join_interval(f, g, none, b->head.interval)),
          out);
      b = b->tail;
      break;
    }
  }
  return QuantityUses_reverse(out);
}

/* sequence_paths */
QuantityPaths quantity_sequence_paths(QuantityPaths a, QuantityPaths b) {
  if (!a.some) return quantity_none();
  if (!b.some) return quantity_none();
  return quantity_some(
      quantity_combine(QUANTITY_JOIN_ADD, QUANTITY_JOIN_ADD, a.rows, b.rows));
}

/* sequence */
QuantityUsage quantity_sequence(QuantityUsage a, QuantityUsage b) {
  return quantity_usage(
      quantity_sequence_paths(a.paths, b.paths),
      quantity_combine(QUANTITY_JOIN_ADD, QUANTITY_JOIN_ADD, a.reads, b.reads));
}

/* alternative_paths */
QuantityPaths quantity_alternative_paths(QuantityPaths a, QuantityPaths b) {
  if (!a.some) return b;
  if (!b.some) return quantity_some(a.rows);
  return quantity_some(quantity_combine(QUANTITY_JOIN_MINIMUM,
                                        QUANTITY_JOIN_MAXIMUM, a.rows, b.rows));
}

/* alternative */
QuantityUsage quantity_alternative(QuantityUsage a, QuantityUsage b) {
  return quantity_usage(quantity_alternative_paths(a.paths, b.paths),
                        quantity_combine(QUANTITY_JOIN_MAXIMUM,
                                         QUANTITY_JOIN_MAXIMUM, a.reads,
                                         b.reads));
}

/* scale_map */
const QuantityUses *quantity_scale_map(Quantity lo, Quantity hi,
                                       const QuantityUses *rows) {
  const QuantityUses *out = NULL;
  for (; rows != NULL; rows = rows->tail)
    out = QuantityUses_cons(
        quantity_row(rows->head.level,
                     quantity_pair(quantity_mul(lo, rows->head.interval.lo),
                                   quantity_mul(hi, rows->head.interval.hi))),
        out);
  return QuantityUses_reverse(out);
}

/* map_paths, scale_range */
QuantityUsage quantity_scale_range(QuantityInterval range,
                                   QuantityUsage value) {
  QuantityPaths paths = value.paths;
  if (paths.some)
    paths = quantity_some(quantity_scale_map(range.lo, range.hi, paths.rows));
  return quantity_usage(paths,
                        quantity_scale_map(range.lo, range.hi, value.reads));
}

/* scale */
QuantityUsage quantity_scale(Quantity q, QuantityUsage value) {
  if (quantity_equal(q, QUANTITY_ZERO)) return quantity_empty();
  return quantity_scale_range(quantity_pair(q, q), value);
}

/* unreachable_interval */
QuantityInterval quantity_unreachable_interval(QuantityInterval value) {
  return quantity_pair(QUANTITY_ONE, quantity_maximum(QUANTITY_ONE, value.hi));
}

/* get */
QuantityInterval quantity_get(Int63 level, QuantityUsage value) {
  if (value.paths.some) return quantity_interval(level, value.paths.rows);
  return quantity_unreachable_interval(quantity_interval(level, value.reads));
}

/* remove_map: drops the first row at level and shares the rest. */
const QuantityUses *quantity_remove_map(Int63 level,
                                        const QuantityUses *rows) {
  const QuantityUses *kept = NULL;
  for (; rows != NULL; rows = rows->tail) {
    if (int63_equal(level, rows->head.level))
      return QuantityUses_reverse_onto(kept, rows->tail);
    kept = QuantityUses_cons(rows->head, kept);
  }
  return QuantityUses_reverse(kept);
}

/* map_paths, remove */
QuantityUsage quantity_remove(Int63 level, QuantityUsage value) {
  QuantityPaths paths = value.paths;
  if (paths.some) paths = quantity_some(quantity_remove_map(level, paths.rows));
  return quantity_usage(paths, quantity_remove_map(level, value.reads));
}

/* interval_once */
Bool quantity_interval_once(QuantityInterval value) {
  return quantity_equal(value.lo, QUANTITY_ONE) &&
                 quantity_equal(value.hi, QUANTITY_ONE)
             ? YES
             : NO;
}

/* exactly_once */
Bool quantity_exactly_once(Int63 level, QuantityUsage value) {
  return quantity_interval_once(quantity_get(level, value));
}

/* interval_used */
Bool quantity_interval_used(QuantityInterval value) {
  return quantity_equal(value.hi, QUANTITY_ZERO) ? NO : YES;
}

/* used: reads only. */
Bool quantity_used(Int63 level, QuantityUsage value) {
  return quantity_interval_used(quantity_interval(level, value.reads));
}

/* interval_at_most_once */
Bool quantity_interval_at_most_once(QuantityInterval value) {
  return quantity_equal(value.hi, QUANTITY_MANY) ? NO : YES;
}

/* at_most_once */
Bool quantity_at_most_once(Int63 level, QuantityUsage value) {
  return quantity_interval_at_most_once(quantity_get(level, value));
}

/* capture_paths */
const QuantityUses *quantity_capture_paths(QuantityPaths paths,
                                           const QuantityUses *reads) {
  if (!paths.some) return reads;
  return quantity_combine(QUANTITY_JOIN_LEFT, QUANTITY_JOIN_MAXIMUM,
                          paths.rows, reads);
}

/* captures */
QuantityUsage quantity_captures(QuantityUsage value) {
  return quantity_usage(
      quantity_some(quantity_capture_paths(value.paths, value.reads)),
      value.reads);
}
