/* foundation.c: C twin of lib/foundation.bend (stage S1).

   Each function names its foundation.bend definitions and lines. The Bend
   limb functions recurse over the limb list; the twins are loops over the
   limb array with the same results. Every Bignum result goes through
   Bignum.make (trim), so its limbs are the unique normal form. */
#include "sole.h"

static U32 *limb_cells(U32 size) {
  return arena_alloc((size_t)(size == 0 ? 1 : size) * sizeof(U32));
}

/* Bignum.trim_zero 18, trim_cons 25, trim 32, make_norm 39, make 46. */
Bignum bignum_make(Bool negative, const U32 *limbs, U32 size) {
  Bignum value;
  while (size > 0 && limbs[size - 1] == 0) size--;
  value.negative = size == 0 ? NO : negative;
  value.limbs = limbs;
  value.size = size;
  return value;
}

/* Bignum.zero 49. */
Bignum bignum_zero(void) { return bignum_make(NO, NULL, 0); }

/* Bignum.one 52. */
Bignum bignum_one(void) {
  static const U32 one[1] = {1};
  return bignum_make(NO, one, 1);
}

/* Bignum.from_u32 62. */
Bignum bignum_from_u32(U32 n) {
  U32 *limbs = limb_cells(3);
  limbs[0] = n % 32768;
  limbs[1] = (n / 32768) % 32768;
  limbs[2] = n / 1073741824;
  return bignum_make(NO, limbs, 3);
}

/* Limbs of a 64-bit magnitude, little endian, no high zero limb. */
static Bignum bignum_of_u64(Bool negative, uint64_t n) {
  U32 *limbs = limb_cells(5);
  U32 size = 0;
  while (n > 0) {
    limbs[size++] = (U32)(n % 32768);
    n /= 32768;
  }
  return bignum_make(negative, limbs, size);
}

/* Bignum.nat_limbs_step 55, nat_limbs 66 (@unsafe), from_nat 73. */
Bignum bignum_from_nat(Nat n) { return bignum_of_u64(NO, n); }

/* Bignum.of_int 76. */
Bignum bignum_of_int(Int63 n) {
  uint64_t magnitude = n < 0 ? (uint64_t)0 - (uint64_t)n : (uint64_t)n;
  return bignum_of_u64(n < 0 ? YES : NO, magnitude);
}

/* Bignum.of_nat 81. */
Bignum bignum_of_nat(Nat n) { return bignum_from_nat(n); }

/* Bignum.cmp_head 84, cmp_abs 93. The Bend recursion decides on length
   first (the shorter list reaches Nil first), then on the highest limb
   that differs. */
static Cmp bignum_cmp_abs(const U32 *xs, U32 xn, const U32 *ys, U32 yn) {
  U32 at;
  if (xn != yn) return xn < yn ? CMP_LT : CMP_GT;
  for (at = xn; at > 0; at--)
    if (xs[at - 1] != ys[at - 1]) return u32_cmp(xs[at - 1], ys[at - 1]);
  return CMP_EQ;
}

/* Bignum.cmp_flip 108. */
static Cmp bignum_cmp_flip(Cmp c) {
  switch (c) {
  case CMP_LT: return CMP_GT;
  case CMP_EQ: return CMP_EQ;
  case CMP_GT: return CMP_LT;
  }
  return CMP_EQ;
}

/* Bignum.compare 117. */
Cmp bignum_compare(Bignum a, Bignum b) {
  if (!a.negative && !b.negative)
    return bignum_cmp_abs(a.limbs, a.size, b.limbs, b.size);
  if (a.negative && b.negative)
    return bignum_cmp_flip(bignum_cmp_abs(a.limbs, a.size, b.limbs, b.size));
  return a.negative ? CMP_LT : CMP_GT;
}

/* Bignum.is_equal 128, equal 137. */
Bool bignum_equal(Bignum a, Bignum b) {
  return bignum_compare(a, b) == CMP_EQ ? YES : NO;
}

/* Bignum.sign 140. */
Cmp bignum_sign(Bignum a) { return bignum_compare(a, bignum_zero()); }

/* Bignum.negate 143. */
Bignum bignum_negate(Bignum a) {
  return bignum_make(a.negative ? NO : YES, a.limbs, a.size);
}

/* Bignum.carry_end 148, add_carry 155, add_abs 162 (carry 0). */
static Bignum bignum_add_abs(Bool negative, Bignum a, Bignum b) {
  U32 size = (a.size > b.size ? a.size : b.size) + 1;
  U32 *limbs = limb_cells(size);
  U32 carry = 0;
  U32 at;
  for (at = 0; at + 1 < size; at++) {
    U32 h = at < a.size ? a.limbs[at] : 0;
    U32 k = at < b.size ? b.limbs[at] : 0;
    limbs[at] = (h + k + carry) % 32768;
    carry = (h + k + carry) / 32768;
  }
  limbs[size - 1] = carry;
  return bignum_make(negative, limbs, size);
}

/* Bignum.borrow 173, sub_abs 176 (borrow 0). Precondition: |a| > |b|. */
static Bignum bignum_sub_abs(Bool negative, Bignum a, Bignum b) {
  U32 *limbs = limb_cells(a.size);
  U32 borrow = 0;
  U32 at;
  for (at = 0; at < a.size; at++) {
    U32 h = a.limbs[at];
    U32 k = at < b.size ? b.limbs[at] : 0;
    limbs[at] = (32768 + h - k - borrow) % 32768;
    borrow = h < k + borrow ? 1 : 0;
  }
  return bignum_make(negative, limbs, a.size);
}

/* Bignum.add_different 187. */
static Bignum bignum_add_different(Bool negative, Bignum a, Bignum b,
                                   Cmp order) {
  switch (order) {
  case CMP_EQ: return bignum_zero();
  case CMP_GT: return bignum_sub_abs(negative, a, b);
  case CMP_LT: return bignum_sub_abs(negative ? NO : YES, b, a);
  }
  return bignum_zero();
}

/* Bignum.add 196. */
Bignum bignum_add(Bignum a, Bignum b) {
  if (a.negative == b.negative) return bignum_add_abs(a.negative, a, b);
  return bignum_add_different(a.negative, a, b,
                              bignum_cmp_abs(a.limbs, a.size, b.limbs, b.size));
}

/* Bignum.sub_order 207, sub 216: a - b when a > b, else zero. */
Bignum bignum_sub(Bignum a, Bignum b) {
  switch (bignum_compare(a, b)) {
  case CMP_GT: return bignum_add(a, bignum_negate(b));
  case CMP_EQ: return bignum_zero();
  case CMP_LT: return bignum_zero();
  }
  return bignum_zero();
}

/* Bignum.mul_limb 219, mul_abs 226, mul 233. Schoolbook rows; a row carry
   stays below 32768, so each step is U32 as in mul_limb. */
Bignum bignum_mul(Bignum a, Bignum b) {
  U32 size = a.size + b.size;
  U32 *limbs = limb_cells(size);
  U32 i;
  U32 j;
  for (i = 0; i < size; i++) limbs[i] = 0;
  for (i = 0; i < a.size; i++) {
    U32 carry = 0;
    for (j = 0; j < b.size; j++) {
      U32 cur = limbs[i + j] + a.limbs[i] * b.limbs[j] + carry;
      limbs[i + j] = cur % 32768;
      carry = cur / 32768;
    }
    limbs[i + b.size] = carry;
  }
  return bignum_make(a.negative != b.negative ? YES : NO, limbs, size);
}

/* Bignum.carry_lo 242, carry_out 249, limbs_onto 252, muladd_go 259,
   muladd 266: limbs = limbs * factor + chunk, in place on a buffer that is
   still being built. A limb is below 2^15 and factor is at most 10^5, so
   each step stays below 2^32. carry_out pushes carry % 32768 when carry is
   not zero, then carry / 32768 when that is not zero. */
static U32 bignum_muladd(U32 *limbs, U32 size, U32 factor, U32 chunk) {
  U32 carry = chunk;
  U32 at;
  for (at = 0; at < size; at++) {
    U32 cur = limbs[at] * factor + carry;
    limbs[at] = cur % 32768;
    carry = cur / 32768;
  }
  if (carry != 0) limbs[size++] = carry % 32768;
  if (carry / 32768 != 0) limbs[size++] = carry / 32768;
  return size;
}

/* Bignum.all_digits 269, dec_limbs 276, dec_value 283, dec_factor 290,
   dec_count 297, decimal_go 306, of_decimal_valid 313, of_decimal 320.
   Five digits per chunk; the fifth digit flushes the chunk. */
int bignum_of_decimal(Text source, Bignum *out) {
  U32 *limbs;
  U32 size = 0;
  U32 value = 0;
  U32 factor = 1;
  Nat count = 0;
  U32 at;
  if (source.size == 0) return 0;
  for (at = 0; at < source.size; at++)
    if (source.items[at] < 48 || source.items[at] > 57) return 0;
  limbs = limb_cells(source.size / 4 + 3);
  for (at = 0; at < source.size; at++) {
    value = value * 10 + (source.items[at] - 48);
    factor = factor * 10;
    if (count == 4) {
      size = bignum_muladd(limbs, size, factor, value);
      value = 0;
      factor = 1;
      count = 0;
    } else {
      count = count + 1;
    }
  }
  size = bignum_muladd(limbs, size, factor, value);
  out->negative = NO;
  out->limbs = limbs;
  out->size = size;
  return 1;
}

/* Bignum.div_ten 327, show_digit 334, show_loop 340 (@unsafe),
   to_string 347. Each pass divides the limbs by ten from the highest limb
   down and keeps the remainder as the next lower digit. */
Text bignum_to_string(Bignum value) {
  U32 *work;
  U32 size = value.size;
  U32 *digits;
  U32 count = 0;
  TextBuilder builder = builder_new();
  U32 at;
  if (size == 0) return text_of_cstring("0");
  work = limb_cells(size);
  for (at = 0; at < size; at++) work[at] = value.limbs[at];
  digits = limb_cells(size * 5 + 1);
  while (size > 0) {
    U32 carry = 0;
    for (at = size; at > 0; at--) {
      U32 cur = carry * 32768 + work[at - 1];
      work[at - 1] = cur / 10;
      carry = cur % 10;
    }
    while (size > 0 && work[size - 1] == 0) size--;
    digits[count++] = 48 + carry;
  }
  if (value.negative) builder_push(&builder, 45);
  for (at = count; at > 0; at--) builder_push(&builder, digits[at - 1]);
  return builder_text(&builder);
}

/* Bignum.narrow_ok 356, narrow_step 363, narrow 370, to_nat 377. narrow
   reads from the highest limb down and refuses when the prefix is above
   2^33 - 1 before a step, so the result is below 2^48. */
int bignum_to_nat(Bignum value, Nat *out) {
  const Nat max = (Nat)4294967295u * 2 + 1;
  Nat n = 0;
  U32 at;
  if (value.negative) return 0;
  for (at = value.size; at > 0; at--) {
    if (n > max) return 0;
    n = n * 32768 + value.limbs[at - 1];
  }
  *out = n;
  return 1;
}

/* Bignum.i31_check 384, to_i31 393. */
int bignum_to_i31(Bignum value, Nat *out) {
  switch (bignum_compare(value, bignum_from_nat(1073741823))) {
  case CMP_GT: return 0;
  case CMP_EQ: return bignum_to_nat(value, out);
  case CMP_LT: return bignum_to_nat(value, out);
  }
  return 0;
}

/* Order.le 396. */
Bool order_le(Cmp order) {
  switch (order) {
  case CMP_LT: return YES;
  case CMP_EQ: return YES;
  case CMP_GT: return NO;
  }
  return NO;
}

/* Order.first 405. */
Cmp order_first(Cmp order, Cmp next) {
  switch (order) {
  case CMP_LT: return CMP_LT;
  case CMP_EQ: return next;
  case CMP_GT: return CMP_GT;
  }
  return next;
}

/* Bignum.to_int 428: |value| <= 2^62 when negative, <= 2^62 - 1 else. */
int bignum_to_int(Bignum value, Int63 *out) {
  static const U32 low[5] = {0, 0, 0, 0, 4};
  static const U32 high[5] = {32767, 32767, 32767, 32767, 3};
  const U32 *bound = value.negative ? low : high;
  if (!order_le(bignum_cmp_abs(value.limbs, value.size, bound, 5))) return 0;
  *out = int63_raw(value);
  return 1;
}

/* Bignum.limbs15 433. */
int bignum_limbs15(Bignum value, const U32 **limbs, U32 *size) {
  if (value.negative) return 0;
  *limbs = value.limbs;
  *size = value.size;
  return 1;
}

/* Bignum.succ_int63 439: 2^62 - 1 wraps to -2^62. */
Bignum bignum_succ_int63(Bignum value) {
  static const U32 high[5] = {32767, 32767, 32767, 32767, 3};
  static const U32 low[5] = {0, 0, 0, 0, 4};
  if (bignum_equal(value, bignum_make(NO, high, 5)))
    return bignum_make(YES, low, 5);
  return bignum_add(value, bignum_one());
}

/* Text.compare 442. */
Cmp text_compare(Text a, Text b) {
  U32 at;
  for (at = 0; at < a.size && at < b.size; at++)
    if (a.items[at] != b.items[at]) return u32_cmp(a.items[at], b.items[at]);
  if (a.size == b.size) return CMP_EQ;
  return a.size < b.size ? CMP_LT : CMP_GT;
}

/* ---------------------------------------------------------------- Map 459 */

static const Map map_empty_cell = {MAP_EMPTY, {{NULL, NULL}}};

/* Map.empty 464. */
const Map *map_empty(void) { return &map_empty_cell; }

static const Map *map_end(const void *value, const Map *rest) {
  Map *map = arena_alloc(sizeof *map);
  map->tag = MAP_END;
  map->as.end.value = value;
  map->as.end.rest = rest;
  return map;
}

static const Map *map_char(U32 code, const Map *less, const Map *same,
                           const Map *greater) {
  Map *map = arena_alloc(sizeof *map);
  map->tag = MAP_CHAR;
  map->as.chr.code = code;
  map->as.chr.less = less;
  map->as.chr.same = same;
  map->as.chr.greater = greater;
  return map;
}

/* Map.singleton 478. */
const Map *map_singleton(Text key, const void *value) {
  const Map *map = map_end(value, map_empty());
  U32 at;
  for (at = key.size; at > 0; at--)
    map = map_char(key.items[at - 1], map_empty(), map, map_empty());
  return map;
}

/* Map.select 467, with_key 473, get_at 483, get 495. get_at is a tail
   walk: End and Char with key left go down; `at` is the key position. */
int map_get(Text key, const Map *map, const void **out) {
  U32 at = 0;
  for (;;) {
    switch (map->tag) {
    case MAP_EMPTY:
      return 0;
    case MAP_END:
      if (at == key.size) {
        *out = map->as.end.value;
        return 1;
      }
      map = map->as.end.rest;
      break;
    case MAP_CHAR:
      if (at == key.size) return 0;
      switch (u32_cmp(key.items[at], map->as.chr.code)) {
      case CMP_LT: map = map->as.chr.less; break;
      case CMP_EQ: map = map->as.chr.same; at++; break;
      case CMP_GT: map = map->as.chr.greater; break;
      }
      break;
    }
  }
}

/* insert_at and remove_at rebuild the path from the root to the node where
   the key stops. A frame records a node on that path and the child that
   the walk took. */
typedef enum { MAP_REST, MAP_LESS, MAP_SAME, MAP_GREATER } MapStep;
typedef struct {
  const Map *node;
  MapStep step;
} MapFrame;
SOLE_LIST(MapFrames, MapFrame)

static const Map *map_descend(Text key, const Map *map,
                              const MapFrames **path, U32 *at) {
  MapFrame frame;
  for (;;) {
    frame.node = map;
    switch (map->tag) {
    case MAP_EMPTY:
      return map;
    case MAP_END:
      if (*at == key.size) return map;
      frame.step = MAP_REST;
      map = map->as.end.rest;
      break;
    case MAP_CHAR:
      if (*at == key.size) return map;
      switch (u32_cmp(key.items[*at], map->as.chr.code)) {
      case CMP_LT: frame.step = MAP_LESS; map = map->as.chr.less; break;
      case CMP_EQ:
        frame.step = MAP_SAME;
        map = map->as.chr.same;
        *at = *at + 1;
        break;
      case CMP_GT: frame.step = MAP_GREATER; map = map->as.chr.greater; break;
      }
      break;
    }
    *path = MapFrames_cons(frame, *path);
  }
}

/* The innermost frame is the head of the path. */
static const Map *map_rebuild(const MapFrames *path, const Map *map) {
  for (; path != NULL; path = path->tail) {
    const Map *node = path->head.node;
    switch (path->head.step) {
    case MAP_REST:
      map = map_end(node->as.end.value, map);
      break;
    case MAP_LESS:
      map = map_char(node->as.chr.code, map, node->as.chr.same,
                     node->as.chr.greater);
      break;
    case MAP_SAME:
      map = map_char(node->as.chr.code, node->as.chr.less, map,
                     node->as.chr.greater);
      break;
    case MAP_GREATER:
      map = map_char(node->as.chr.code, node->as.chr.less, node->as.chr.same,
                     map);
      break;
    }
  }
  return map;
}

/* Map.insert_at 498, insert 511. */
const Map *map_insert(Text key, const void *value, const Map *map) {
  const MapFrames *path = NULL;
  U32 at = 0;
  const Map *stop = map_descend(key, map, &path, &at);
  const Map *fresh = stop;
  switch (stop->tag) {
  case MAP_EMPTY: fresh = map_singleton(text_drop(key, at), value); break;
  case MAP_END: fresh = map_end(value, stop->as.end.rest); break;
  case MAP_CHAR: fresh = map_end(value, stop); break;
  }
  return map_rebuild(path, fresh);
}

/* Map.remove_at 514, remove 526. */
const Map *map_remove(Text key, const Map *map) {
  const MapFrames *path = NULL;
  U32 at = 0;
  const Map *stop = map_descend(key, map, &path, &at);
  const Map *fresh = stop;
  switch (stop->tag) {
  case MAP_EMPTY: fresh = map_empty(); break;
  case MAP_END: fresh = stop->as.end.rest; break;
  case MAP_CHAR: fresh = stop; break;
  }
  return map_rebuild(path, fresh);
}

/* A pending node of a trie walk and the key prefix above it. */
typedef struct {
  const Map *node;
  Text prefix;
} MapWork;
SOLE_LIST(MapWorks, MapWork)

static const MapWorks *map_work(const Map *node, Text prefix,
                                const MapWorks *rest) {
  MapWork work;
  work.node = node;
  work.prefix = prefix;
  return MapWorks_cons(work, rest);
}

/* Map.reverse 529, bindings_prefix 534, bindings 543. Order: an End
   binding, then its rest; at a Char, less, then same (prefix + code), then
   greater. The work stack keeps that order. */
const MapBindings *map_bindings(const Map *map) {
  const MapWorks *todo = map_work(map, text_end(), NULL);
  const MapBindings *out = NULL;
  while (todo != NULL) {
    MapWork work = todo->head;
    todo = todo->tail;
    switch (work.node->tag) {
    case MAP_EMPTY:
      break;
    case MAP_END: {
      MapBinding binding;
      binding.key = work.prefix;
      binding.value = work.node->as.end.value;
      out = MapBindings_cons(binding, out);
      todo = map_work(work.node->as.end.rest, work.prefix, todo);
      break;
    }
    case MAP_CHAR: {
      Text code_text;
      code_text.items = &work.node->as.chr.code;
      code_text.size = 1;
      todo = map_work(work.node->as.chr.greater, work.prefix, todo);
      todo = map_work(work.node->as.chr.same,
                      text_append(work.prefix, code_text), todo);
      todo = map_work(work.node->as.chr.less, work.prefix, todo);
      break;
    }
    }
  }
  return MapBindings_reverse(out);
}

/* Map.size 546: the count of End nodes. */
Nat map_size(const Map *map) {
  const MapWorks *todo = map_work(map, text_end(), NULL);
  Nat count = 0;
  while (todo != NULL) {
    const Map *node = todo->head.node;
    todo = todo->tail;
    switch (node->tag) {
    case MAP_EMPTY:
      break;
    case MAP_END:
      count = count + 1;
      todo = map_work(node->as.end.rest, text_end(), todo);
      break;
    case MAP_CHAR:
      todo = map_work(node->as.chr.less, text_end(), todo);
      todo = map_work(node->as.chr.same, text_end(), todo);
      todo = map_work(node->as.chr.greater, text_end(), todo);
      break;
    }
  }
  return count;
}

/* ---------------------------------------------------------------- Int63 */

/* Int63.zero 554. */
Int63 int63_zero(void) { return 0; }

/* Int63.one 556. */
Int63 int63_one(void) { return 1; }

/* Int63.equal 558: Bignum.equal of the two values. */
Bool int63_equal(Int63 a, Int63 b) { return a == b ? YES : NO; }

/* Int63.compare 561: Bignum.compare of the two values. */
Cmp int63_compare(Int63 a, Int63 b) {
  if (a < b) return CMP_LT;
  return a == b ? CMP_EQ : CMP_GT;
}

/* Int63.raw 564: the value of the limbs, read as a 64-bit magnitude. */
Int63 int63_raw(Bignum value) {
  uint64_t magnitude = 0;
  U32 at;
  for (at = value.size; at > 0; at--)
    magnitude = magnitude * 32768 + value.limbs[at - 1];
  return value.negative ? (Int63)((uint64_t)0 - magnitude) : (Int63)magnitude;
}

/* Int63.show 568. */
Text int63_show(Int63 value) { return bignum_to_string(bignum_of_int(value)); }

/* Int63.wrap_high_order 570, wrap_high 576, wrap_sum_order 579,
   wrap_sum 585. The input is a sum of two Int63 values, so it lies in
   [-2^63, 2^63 - 2]: above 2^62 - 1 subtract 2^63, below -2^62 add 2^63. */
static Int63 int63_wrap_sum(int64_t raw) {
  const int64_t high = ((int64_t)1 << 62) - 1;
  const int64_t low = -((int64_t)1 << 62);
  if (raw > high) return raw + INT64_MIN;
  if (raw < low) return raw - INT64_MIN;
  return raw;
}

/* Int63.add 588. */
Int63 int63_add(Int63 a, Int63 b) { return int63_wrap_sum(a + b); }

/* Int63.sub 591. */
Int63 int63_sub(Int63 a, Int63 b) { return int63_wrap_sum(a - b); }

/* Int63.succ 594. */
Int63 int63_succ(Int63 value) { return int63_add(value, int63_one()); }

/* Int63.pred 596. */
Int63 int63_pred(Int63 value) { return int63_sub(value, int63_one()); }
