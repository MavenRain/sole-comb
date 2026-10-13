/* sole.h: shared types of the sole-comb C host (TinyCC port).

   The C host is a one-to-one port of the Bend 2 host (bin/, lib/, surface/,
   erase/). Each C file stands beside its .bend twin (lib/kernel_eval.c next
   to lib/kernel_eval.bend) and keeps the twin's definition names (with the
   module prefix: kernel_eval.bend `leg_closures` -> `eval_leg_closures`),
   argument order and case order, so a reviewer reads the two side by side.
   Each C function names the Bend definitions that it covers when it covers
   more than one (small Bend helpers that exist only because Bend has no
   `if` go into their caller).

   Rules for every C file:
   - A value never changes after a constructor returns it. Constructors
     allocate from the arena and return const pointers. A cell that is still
     being built (for example a buffer under construction) can change until
     the function returns it.
   - The arena is the only global mutable state. It lives until exit.
   - A function that can fail returns int: 1 = done, 0 = fail. It writes its
     answer to an out parameter on done, and the error to a `const KError **`
     out parameter on fail. This is Bend `R.result(T)` (Done{v} | Fail{e}).
   - A Bend Maybe<T> is an int return (1 = Some) with an out parameter, or a
     NULL pointer when T is a boxed value (pointer).
   - A Bend lambda that goes to a helper (L.maybe, List.map, folds) becomes
     direct code at the call site (if / switch / loop). No function pointers
     unless a stage records the reason.
   - A Bend `match` becomes a `switch (x->tag)` with one case per
     constructor and no `default:` label, so -Wall reports a missing case.
     A match on Bool becomes if / else.
   - No recursion over the length of an input list (tokens, source bytes,
     definitions, spines, limbs). Write a loop. Recursion over term depth
     stays, on the big stack (run_on_big_stack).
   - F.choose(A, yes, left, right) becomes `yes ? left : right`.
     F.maybe_when(A, valid, value) becomes an int return.
   - Each use of the generic F.Pair2<A, B> becomes a named struct (by value)
     in the section of the module that uses it.

   The header grows stage by stage. Each stage appends the sections of its
   modules in DAG order (K/INVENTORY.md). */
#ifndef SOLE_H
#define SOLE_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------- arena */

/* All host memory comes from one arena. The arena lives until exit.
   arena_alloc never returns NULL: on exhaustion it prints
   "sole-comb: out of memory" to stderr and exits 70.
   A mark gives back the bytes allocated after it: arena_release(mark)
   rewinds to the mark and frees the blocks taken after it (S8 session 1:
   the `test` verb releases after each row, so the resident size is that
   of the largest row). The caller must not touch a released byte again. */
typedef struct ArenaBlock ArenaBlock;
typedef struct {
  ArenaBlock *block;
  unsigned char *next;
  size_t left;
} ArenaMark;
void *arena_alloc(size_t size);
ArenaMark arena_mark(void);
void arena_release(ArenaMark mark);

/* ---------------------------------------------------------------- base */

/* Bend Base Bool (True{} | False{}). */
typedef enum { NO = 0, YES = 1 } Bool;

/* Bend Base Cmp, in the Bend constructor order (LT{} | EQ{} | GT{}). */
typedef enum { CMP_LT, CMP_EQ, CMP_GT } Cmp;

/* Bend U32. Arithmetic wraps modulo 2^32, as Bend U32 does and as unsigned
   C arithmetic does. S1 has no division by a variable: each divisor in
   lib/foundation.bend and lib/kernel_pp.bend is a literal. The first stage
   with a variable divisor records the Bend result of x / 0 and x % 0 and
   adds u32_div and u32_mod here. */
typedef uint32_t U32;
Cmp u32_cmp(U32 a, U32 b); /* Base U32.cmp */

/* Bend Nat (Base unary Zero | Succ; literals such as 18n). It has no upper
   bound in Bend, but the host uses it only for budgets, line and column
   numbers and small counts. The host keeps 64 bits. nat_add returns 0 on
   overflow; each stage records its Nat arithmetic sites in PROGRESS. */
typedef uint64_t Nat;
int nat_add(Nat a, Nat b, Nat *out);
Nat nat_sub(Nat a, Nat b); /* stops at zero, as Bend Nat subtraction */

/* ---------------------------------------------------------------- lists */

/* A Bend List<T> is an immutable cons list. NULL is the empty list ([]).
   SOLE_LIST(Name, Elem) declares the cell type and its constructor:

     SOLE_LIST(Terms, const Term *)
     const Terms *xs = Terms_cons(t, NULL);

   SOLE_LIST_OPS(Name) adds the helpers that the Bend code calls on that
   list type (L.reverse, List.append, length). Each helper is a loop. */
#define SOLE_LIST(Name, Elem)                                               \
  typedef struct Name Name;                                                 \
  struct Name {                                                             \
    Elem head;                                                              \
    const Name *tail;                                                       \
  };                                                                        \
  static inline const Name *Name##_cons(Elem head, const Name *tail) {      \
    Name *cell = (Name *)arena_alloc(sizeof *cell);                         \
    cell->head = head;                                                      \
    cell->tail = tail;                                                      \
    return cell;                                                            \
  }

#define SOLE_LIST_OPS(Name)                                                 \
  static inline const Name *Name##_reverse_onto(const Name *xs,             \
                                                const Name *acc) {          \
    for (; xs != NULL; xs = xs->tail) acc = Name##_cons(xs->head, acc);     \
    return acc;                                                             \
  }                                                                         \
  static inline const Name *Name##_reverse(const Name *xs) {                \
    return Name##_reverse_onto(xs, NULL);                                   \
  }                                                                         \
  static inline const Name *Name##_append(const Name *xs, const Name *ys) { \
    return Name##_reverse_onto(Name##_reverse(xs), ys);                     \
  }                                                                         \
  static inline Nat Name##_length(const Name *xs) {                         \
    Nat count = 0;                                                          \
    for (; xs != NULL; xs = xs->tail) count++;                              \
    return count;                                                           \
  }

/* ---------------------------------------------------------------- text */

/* A Bend String is a list of Chr (Unicode scalar values). The host keeps a
   String as an immutable slice of U32 items. A slice never changes after it
   is made, so text_tail, text_take and text_drop share items in O(1).
   Source bytes (Bend List<U32> of bytes) use the same type. */
typedef struct {
  const U32 *items;
  U32 size;
} Text;

SOLE_LIST(U32s, U32)
SOLE_LIST_OPS(U32s)
SOLE_LIST(Texts, Text)
SOLE_LIST_OPS(Texts)

Text text_end(void);
Text text_of_bytes(const unsigned char *bytes, size_t size);
Text text_of_cstring(const char *ascii); /* string literals of the Bend code */
Bool text_is_end(Text text);
U32 text_head(Text text);  /* precondition: not text_is_end */
Text text_tail(Text text); /* precondition: not text_is_end */
Text text_take(Text text, U32 count);
Text text_drop(Text text, U32 count);
Text text_append(Text a, Text b);       /* Base String.append */
Text text_reverse(Text text);
Bool text_same(Text a, Text b);         /* Base String.eq */
Text text_concat(const Texts *texts);   /* Base String.concat */
Text text_join(const Texts *texts, Text separator); /* Base String.join */
Text text_of_u32(U32 number);           /* decimal, as the Bend show of U32 */
Text text_of_nat(Nat number);           /* decimal, as the Bend show of Nat */

/* Bend Maybe<String>: some = NO is None{}. */
typedef struct {
  Bool some;
  Text text;
} MaybeText;
static inline MaybeText maybe_text_none(void) {
  MaybeText none = {NO, {NULL, 0}};
  return none;
}
static inline MaybeText maybe_text_some(Text text) {
  MaybeText some = {YES, text};
  return some;
}

/* A growable text that collects output in order. Bend builds output with
   S.cat, String.concat and String.append (non-tail over the first
   argument); the host uses a TextBuilder instead. builder_text returns a
   slice that later pushes do not change. */
typedef struct {
  U32 *items;
  U32 size;
  U32 capacity;
} TextBuilder;

TextBuilder builder_new(void);
void builder_push(TextBuilder *builder, U32 item);
void builder_append(TextBuilder *builder, Text text);
void builder_append_cstring(TextBuilder *builder, const char *ascii);
Text builder_text(const TextBuilder *builder);

/* ---------------------------------------------------------------- errors */

/* The kernel error value (lib/kernel_error.bend, E.t). Declared in the
   kernel_error section below. */
typedef struct KError KError;

/* ---------------------------------------------------------------- host */

/* Runs job(argument) on a thread with a 1 GiB stack (ledger-lang
   compiler/main.c pattern). Returns 1 when the thread ran to the end. The
   Bend toolchain pin used stack_kib 65500 for the js endpoint; S6 measures
   the deepest examples/ and corpus/ input and records the margin. */
#define SOLE_STACK_SIZE ((size_t)1 << 30)
int run_on_big_stack(void *(*job)(void *), void *argument);

/* Source limit of the public CLI (dev/cli.py MAX_SOURCE_BYTES). */
#define SOLE_SOURCE_LIMIT 32768

/* Writes a Text as UTF-8 bytes to a FILE * (Bend IO.print encodes each Chr
   as UTF-8). Returns 1 on success. */
int write_utf8(void *stream, Text text);

/* The UTF-8 bytes of one Chr (the write_utf8 encoding): 1 to 4 bytes into
   bytes, the count returned. */
size_t utf8_encode(U32 item, unsigned char *bytes);

/* ================================================= lib/foundation.bend */

/* Bignum.t = Int{negative, limbs}. The limbs are little endian (the first
   limb is the least significant) and each is below 32768, as in
   foundation.bend. Every value that these functions return is normal
   (Bignum.make): no high zero limb, and zero is not negative. The limbs
   array is immutable and can be shared. */
typedef struct {
  Bool negative;
  const U32 *limbs;
  U32 size;
} Bignum;

/* Int63.t: a signed 63-bit value. foundation.bend keeps it as Int{negative,
   limbs} in [-2^62, 2^62 - 1]; the host keeps the same value in an int64_t
   and applies the same wrap (Int63.wrap_sum) after add and sub. */
typedef int64_t Int63;

Bignum bignum_make(Bool negative, const U32 *limbs, U32 size);
Bignum bignum_zero(void);
Bignum bignum_one(void);
Bignum bignum_from_u32(U32 n);
Bignum bignum_from_nat(Nat n);
Bignum bignum_of_int(Int63 n);
Bignum bignum_of_nat(Nat n);
Cmp bignum_compare(Bignum a, Bignum b);
Bool bignum_equal(Bignum a, Bignum b);
Cmp bignum_sign(Bignum a);
Bignum bignum_negate(Bignum a);
Bignum bignum_add(Bignum a, Bignum b);
Bignum bignum_sub(Bignum a, Bignum b); /* stops at zero, as foundation */
Bignum bignum_mul(Bignum a, Bignum b);
int bignum_of_decimal(Text source, Bignum *out);
Text bignum_to_string(Bignum value);
int bignum_to_nat(Bignum value, Nat *out);
int bignum_to_i31(Bignum value, Nat *out);
int bignum_to_int(Bignum value, Int63 *out);
int bignum_limbs15(Bignum value, const U32 **limbs, U32 *size);
Bignum bignum_succ_int63(Bignum value);

Bool order_le(Cmp order);
Cmp order_first(Cmp order, Cmp next);
Cmp text_compare(Text a, Text b); /* foundation Text.compare */

/* Map.t<A>: the persistent String-keyed ternary trie of foundation.bend
   (Empty | End{value, rest} | Char{code, less, same, greater}). The host
   keeps the trie shape, not a hash table, because Map.bindings order
   reaches the printed output (kernel_global G.t). The value is a const
   pointer; a stage that stores a non-pointer value boxes it. */
typedef enum { MAP_EMPTY, MAP_END, MAP_CHAR } MapTag;
typedef struct Map Map;
struct Map {
  MapTag tag;
  union {
    struct {
      const void *value;
      const Map *rest;
    } end;
    struct {
      U32 code;
      const Map *less;
      const Map *same;
      const Map *greater;
    } chr;
  } as;
};

/* Map.bindings element: F.Pair2<String, A>. */
typedef struct {
  Text key;
  const void *value;
} MapBinding;
SOLE_LIST(MapBindings, MapBinding)
SOLE_LIST_OPS(MapBindings)

const Map *map_empty(void);
const Map *map_singleton(Text key, const void *value);
int map_get(Text key, const Map *map, const void **out);
const Map *map_insert(Text key, const void *value, const Map *map);
const Map *map_remove(Text key, const Map *map);
const MapBindings *map_bindings(const Map *map);
Nat map_size(const Map *map);

Int63 int63_zero(void);
Int63 int63_one(void);
Bool int63_equal(Int63 a, Int63 b);
Cmp int63_compare(Int63 a, Int63 b);
Int63 int63_raw(Bignum value); /* precondition: value in the Int63 range */
Text int63_show(Int63 value);
Int63 int63_add(Int63 a, Int63 b);
Int63 int63_sub(Int63 a, Int63 b);
Int63 int63_succ(Int63 value);
Int63 int63_pred(Int63 value);

/* =============================================== lib/kernel_error.bend */

typedef enum {
  ERROR_NOT_YET,
  ERROR_PARSE,
  ERROR_CARRY,
  ERROR_UNBOUND,
  ERROR_MISMATCH,
  ERROR_UNIVERSE,
  ERROR_QUANTITY,
  ERROR_WRONG_LEG,
  ERROR_MISSING_BRANCH,
  ERROR_OVERFLOW,
  ERROR_CANNOT_INFER,
  ERROR_BUDGET_EXHAUSTED,
  ERROR_INDEX_NOT_ZERO,
  ERROR_INDEX_ABOVE_UNIVERSE,
  ERROR_TERMINATION
} KErrorTag;

/* The 13 constructors with one `message` field share the `plain` member. */
struct KError {
  KErrorTag tag;
  union {
    struct {
      Text message;
    } plain;
    struct {
      Text message;
      Nat line;
      Nat column;
    } parse;
    struct {
      Text name;
    } termination;
  } as;
};

const KError *error_not_yet(Text message);
const KError *error_parse(Text message, Nat line, Nat column);
const KError *error_carry(Text message);
const KError *error_unbound(Text message);
const KError *error_mismatch(Text message);
const KError *error_universe(Text message);
const KError *error_quantity(Text message);
const KError *error_wrong_leg(Text message);
const KError *error_missing_branch(Text message);
const KError *error_overflow(Text message);
const KError *error_cannot_infer(Text message);
const KError *error_budget_exhausted(Text message);
const KError *error_index_not_zero(Text message);
const KError *error_index_above_universe(Text message);
const KError *error_termination(Text name);

Text error_termination_msg(Text name);
Text error_message(const KError *error);
Text error_to_string(const KError *error);

/* ============================================== lib/kernel_budget.bend */

typedef struct {
  Nat remaining;
} Budget;

Budget budget_create(Nat remaining);
Bool budget_exhausted(Budget budget);
int budget_spend(Text context, Budget budget, Budget *out,
                 const KError **error);

/* =============================================== lib/kernel_level.bend */

/* L.t is an alias of F.Bignum.t. */
typedef Bignum Level;

Level level_zero(void);
Level level_one(void);
Level level_succ(Level level);
Level level_max(Level a, Level b);
Bool level_equal(Level a, Level b);
Bool level_le(Level a, Level b);
int level_of_int(Int63 n, Level *out);
Level level_of_nat(Nat n);
Text level_to_string(Level level);

/* ============================================= lib/kernel_literal.bend */

typedef enum { LITERAL_LSTRING, LITERAL_LINT } LiteralTag;
typedef struct Literal Literal;
struct Literal {
  LiteralTag tag;
  union {
    struct {
      Text value;
    } lstring;
    struct {
      Bignum value;
    } lint;
  } as;
};

const Literal *literal_lstring(Text value);
const Literal *literal_lint(Bignum value);
Bool literal_equal(const Literal *a, const Literal *b);

/* ============================================ lib/kernel_quantity.bend */

/* Q.t: only nullary constructors, so a C enum by value. */
typedef enum { QUANTITY_ZERO, QUANTITY_ONE, QUANTITY_MANY } Quantity;

/* interval_t = F.Pair2<t, t> (lo, hi). */
typedef struct {
  Quantity lo;
  Quantity hi;
} QuantityInterval;

/* One row of uses = F.Pair2<F.Int63.t, interval_t>. Rows are in ascending
   level order. */
typedef struct {
  Int63 level;
  QuantityInterval interval;
} QuantityUse;
SOLE_LIST(QuantityUses, QuantityUse)
SOLE_LIST_OPS(QuantityUses)

/* Maybe<uses>: some = NO is None{}. */
typedef struct {
  Bool some;
  const QuantityUses *rows;
} QuantityPaths;

/* usage = Usage{paths: Maybe<uses>, reads: uses}. */
typedef struct {
  QuantityPaths paths;
  const QuantityUses *reads;
} QuantityUsage;

/* The ~f and ~g template arguments of combine, one per distinct Bend
   lambda: add, minimum, maximum, and `lo => _ => lo` (LEFT). */
typedef enum {
  QUANTITY_JOIN_ADD,
  QUANTITY_JOIN_MINIMUM,
  QUANTITY_JOIN_MAXIMUM,
  QUANTITY_JOIN_LEFT
} QuantityJoin;

Bool quantity_equal(Quantity a, Quantity b);
Quantity quantity_mul(Quantity a, Quantity b);
Text quantity_to_string(Quantity q);
Quantity quantity_runtime(Quantity q);
Quantity quantity_add(Quantity a, Quantity b);
Quantity quantity_minimum(Quantity a, Quantity b);
Quantity quantity_maximum(Quantity a, Quantity b);
QuantityUsage quantity_empty(void);
QuantityUsage quantity_unreachable(void);
QuantityInterval quantity_interval(Int63 level, const QuantityUses *rows);
QuantityUsage quantity_returning(const QuantityUses *rows);
QuantityUsage quantity_occurrence(Int63 level, Quantity q);
QuantityInterval quantity_join_interval(QuantityJoin f, QuantityJoin g,
                                        QuantityInterval a,
                                        QuantityInterval b);
const QuantityUses *quantity_combine(QuantityJoin f, QuantityJoin g,
                                     const QuantityUses *a,
                                     const QuantityUses *b);
QuantityPaths quantity_sequence_paths(QuantityPaths a, QuantityPaths b);
QuantityUsage quantity_sequence(QuantityUsage a, QuantityUsage b);
QuantityPaths quantity_alternative_paths(QuantityPaths a, QuantityPaths b);
QuantityUsage quantity_alternative(QuantityUsage a, QuantityUsage b);
const QuantityUses *quantity_scale_map(Quantity lo, Quantity hi,
                                       const QuantityUses *rows);
QuantityUsage quantity_scale_range(QuantityInterval range,
                                   QuantityUsage value);
QuantityUsage quantity_scale(Quantity q, QuantityUsage value);
QuantityInterval quantity_unreachable_interval(QuantityInterval value);
QuantityInterval quantity_get(Int63 level, QuantityUsage value);
const QuantityUses *quantity_remove_map(Int63 level,
                                        const QuantityUses *rows);
QuantityUsage quantity_remove(Int63 level, QuantityUsage value);
Bool quantity_interval_once(QuantityInterval value);
Bool quantity_exactly_once(Int63 level, QuantityUsage value);
Bool quantity_interval_used(QuantityInterval value);
Bool quantity_used(Int63 level, QuantityUsage value);
Bool quantity_interval_at_most_once(QuantityInterval value);
Bool quantity_at_most_once(Int63 level, QuantityUsage value);
const QuantityUses *quantity_capture_paths(QuantityPaths paths,
                                           const QuantityUses *reads);
QuantityUsage quantity_captures(QuantityUsage value);

/* ============================================== lib/kernel_shape.bend */

/* S.t<A> is generic over the payload A. SOLE_SHAPE(Name, Elem, List,
   suffix) declares one instance: the tagged struct, the constructors and
   payload, point_dom, family and name. List is the cons list of Elem. The
   host has two instances: TermShape (shape_*_term) and ValueShape
   (shape_*_value). lib/kernel_shape.c defines both. point_dom returns NULL
   for None{}. */
typedef enum { SHAPE_SPI, SHAPE_SCOLL, SHAPE_SPAR, SHAPE_SMU, SHAPE_SNU } ShapeTag;

#define SOLE_SHAPE(Name, Elem, List, suffix)                                 \
  typedef struct Name Name;                                                  \
  struct Name {                                                              \
    ShapeTag tag;                                                            \
    union {                                                                  \
      struct { Quantity quantity; Text binder; Elem domain; } spi;           \
      struct { Int63 size; } scoll;                                          \
      struct { Elem left; Elem right; } spar;                                \
      struct { Text family; const List *indices; } smu;                      \
      struct { Text family; const List *indices; } snu;                      \
    } as;                                                                    \
  };                                                                         \
  const Name *shape_spi_##suffix(Quantity quantity, Text binder,            \
                                 Elem domain);                               \
  const Name *shape_scoll_##suffix(Int63 size);                              \
  const Name *shape_spar_##suffix(Elem left, Elem right);                    \
  const Name *shape_smu_##suffix(Text family, const List *indices);          \
  const Name *shape_snu_##suffix(Text family, const List *indices);          \
  const List *shape_payload_##suffix(const Name *shape);                     \
  Elem shape_point_dom_##suffix(const Name *shape);                          \
  MaybeText shape_family_##suffix(const Name *shape);                        \
  Text shape_name_##suffix(const Name *shape);

const Texts *shape_declared(void);

/* =============================================== lib/kernel_term.bend */

typedef struct Term Term;
SOLE_LIST(Terms, const Term *)
SOLE_LIST_OPS(Terms)
SOLE_SHAPE(TermShape, const Term *, Terms, term)

/* AddrData<t> = T.addr. */
typedef enum { TERM_APT, TERM_ALEG, TERM_ACTOR } TermAddrTag;
typedef struct {
  TermAddrTag tag;
  union {
    struct { Quantity quantity; const Term *arg; } apt;
    struct { Int63 index; } aleg;
    struct { Text name; } actor;
  } as;
} TermAddr;

/* F.Pair2<Q.t, String>: one leg binder (kernel_value VLeg uses it too). */
typedef struct {
  Quantity quantity;
  Text name;
} TermBinder;
SOLE_LIST(TermBinders, TermBinder)
SOLE_LIST_OPS(TermBinders)

/* LegData<t> = T.leg. */
typedef struct {
  const TermBinders *binders;
  const Term *body;
} TermLeg;
SOLE_LIST(TermLegs, const TermLeg *)
SOLE_LIST_OPS(TermLegs)

/* MotiveData<t> = T.motive. Indices are outermost first; self is index zero
   in body. Maybe<T.motive> is a NULL pointer. */
typedef struct {
  MaybeText ind;
  const Texts *idx;
  Text self;
  const Term *body;
} TermMotive;

/* F.Pair2<T.addr, T.leg>: one branch. */
typedef struct {
  const TermAddr *address;
  const TermLeg *leg;
} TermBranch;
SOLE_LIST(TermBranches, TermBranch)
SOLE_LIST_OPS(TermBranches)

/* ElimData<t> = T.elim. */
typedef struct {
  const TermShape *shape;
  const Term *scrut;
  Quantity scrut_q;
  const TermMotive *motive;
  const TermBranches *branches;
} TermElim;

typedef enum {
  TERM_VAR, TERM_UNIV, TERM_LAN, TERM_RAN, TERM_IN, TERM_ELIM, TERM_SEC,
  TERM_OUT, TERM_LET, TERM_ANN, TERM_GLOBAL, TERM_LIT, TERM_AUTO
} TermTag;
struct Term {
  TermTag tag;
  union {
    struct { Int63 index; } var;
    struct { Level level; } univ;
    struct { const TermShape *shape; const Term *diagram; } lan;
    struct { const TermShape *shape; const Term *diagram; } ran;
    struct { const TermShape *shape; const TermAddr *address; const Terms *args; } in;
    struct { const TermElim *elim; } elim;
    struct { const TermShape *shape; const TermLegs *legs; } sec;
    struct { const TermShape *shape; const TermAddr *address; const Term *head; } out;
    struct { Text name; const Term *ty; const Term *value; const Term *body; } let;
    struct { const Term *term; const Term *ty; } ann;
    struct { Text name; } global;
    struct { const Literal *literal; } lit;
  } as;
};

const Term *term_var(Int63 index);
const Term *term_univ(Level level);
const Term *term_lan(const TermShape *shape, const Term *diagram);
const Term *term_ran(const TermShape *shape, const Term *diagram);
const Term *term_in(const TermShape *shape, const TermAddr *address,
                    const Terms *args);
const Term *term_elim(const TermElim *elim);
const Term *term_sec(const TermShape *shape, const TermLegs *legs);
const Term *term_out(const TermShape *shape, const TermAddr *address,
                     const Term *head);
const Term *term_let(Text name, const Term *ty, const Term *value,
                     const Term *body);
const Term *term_ann(const Term *term, const Term *ty);
const Term *term_global(Text name);
const Term *term_lit(const Literal *literal);
const Term *term_auto(void);
const TermAddr *term_apt(Quantity quantity, const Term *arg);
const TermAddr *term_aleg(Int63 index);
const TermAddr *term_actor(Text name);
const TermLeg *term_leg(const TermBinders *binders, const Term *body);
const TermMotive *term_motive(MaybeText ind, const Texts *idx, Text self,
                              const Term *body);
const TermElim *term_elimination(const TermShape *shape, const Term *scrut,
                                 Quantity scrut_q, const TermMotive *motive,
                                 const TermBranches *branches);

/* as_apt answer: F.Pair2<Q.t, t>. */
typedef struct {
  Quantity quantity;
  const Term *arg;
} TermPoint;

/* Scan{found, pending}. */
typedef struct {
  Bool found;
  const Terms *pending;
} TermScan;

const Texts *term_formers(void);
const Texts *term_schema(void);
int term_as_apt(const TermAddr *address, TermPoint *out);
int term_as_aleg(const TermAddr *address, Int63 *out);
MaybeText term_as_actor(const TermAddr *address);
Bool term_member(Text name, const Texts *names);
Bool term_family_occurs(Bool include_families, const Texts *names,
                        MaybeText family);
const Terms *term_addr_terms(const TermAddr *address, const Terms *rest);
const Terms *term_leg_terms(const TermLegs *legs);
const Terms *term_motive_terms(const TermMotive *motive, const Terms *rest);
const Terms *term_branch_terms(const TermBranches *branches);
TermScan term_scan_shape(Bool include_families, const Texts *names,
                         const TermShape *shape, const Terms *rest);
TermScan term_scan_term(Bool include_families, const Texts *names,
                        const Term *term, const Terms *rest);
Bool term_scan(Bool include_families, const Texts *names, TermScan state);
Bool term_exists_name(Bool include_families, const Texts *names,
                      const Term *term);

/* ========================================== lib/kernel_positivity.bend */

/* telescope = List<F.Pair2<Q.t, F.Pair2<String, T.t>>>. */
typedef struct {
  Text name;
  const Term *type;
} TeleBinding;
typedef struct {
  Quantity quantity;
  TeleBinding binding;
} TeleEntry;
SOLE_LIST(Telescope, TeleEntry)
SOLE_LIST_OPS(Telescope)

typedef enum { CTOR_PROVISIONAL, CTOR_BUILTIN, CTOR_COMPLETE } CtorStatusTag;
typedef struct {
  CtorStatusTag tag;
  union {
    struct { const Texts *names; } complete;
  } as;
} CtorStatus;

/* Ctor{c_name, c_args, c_res_idx, c_full_arity, c_self_rec}. */
typedef struct {
  Text name;
  const Telescope *args;
  const Terms *res_idx;
  Int63 full_arity;
  Bool self_rec;
} Ctor;
SOLE_LIST(Ctors, const Ctor *)
SOLE_LIST_OPS(Ctors)

/* Family{f_name, f_params, f_indices, f_level, f_status, f_ctors,
   f_positive}. */
typedef struct {
  Text name;
  const Telescope *params;
  const Telescope *indices;
  Level level;
  const CtorStatus *status;
  const Ctors *ctors;
  Bool positive;
} Family;

const CtorStatus *positivity_provisional(void);
const CtorStatus *positivity_builtin(void);
const CtorStatus *positivity_complete(const Texts *names);
const Ctor *positivity_ctor(Text name, const Telescope *args,
                            const Terms *res_idx, Int63 full_arity,
                            Bool self_rec);
const Family *positivity_family(Text name, const Telescope *params,
                                const Telescope *indices, Level level,
                                const CtorStatus *status, const Ctors *ctors,
                                Bool positive);
Text positivity_ctor_name(const Ctor *value);
const Ctor *positivity_find_ctor(Text name, const Ctors *ctors); /* NULL = None */
const Ctor *positivity_ctor_of(Text name, const Family *fam);   /* NULL = None */
Text positivity_nonpositive_word(void);
Bool positivity_occurs(const Texts *names, const Term *term);
Bool positivity_is_member(const Texts *names, MaybeText name);
int positivity_absent(const Texts *names, const Term *term,
                      const KError **error);
int positivity_absent_list(const Texts *names, const Terms *terms,
                           const KError **error);
int positivity_absent_address(const Texts *names, TermPoint address,
                              const KError **error);
int positivity_positive(const Texts *names, const Term *term,
                        const KError **error);
int positivity_ctor_fields(const Texts *names, const Telescope *args,
                           const KError **error);
Bool positivity_self_rec(const Texts *names, const Telescope *args);

/* ================================================= lib/kernel_pp.bend */

Text pp_escape(Text s);
Text pp_literal(const Literal *lit);
Text pp_term(const Texts *ns, const Term *tm);
Text pp_shape(const Texts *ns, const TermShape *s);
Text pp_addr(const Texts *ns, const TermAddr *a);
Text pp_leg(const Texts *ns, const TermLeg *l);
Text pp_elim(const Texts *ns, const TermElim *e);

/* ============================================== lib/kernel_value.bend */

/* Spines are newest first. */
typedef struct Value Value;
SOLE_LIST(Values, const Value *)
SOLE_LIST_OPS(Values)
SOLE_SHAPE(ValueShape, const Value *, Values, value)

/* ClosureData<t> = V.closure. */
typedef struct {
  const Values *env;
  const Term *body;
} Closure;

/* AddrData<t> = V.vaddr. */
typedef enum { VALUE_VAPT, VALUE_VALEG, VALUE_VACTOR } VAddrTag;
typedef struct {
  VAddrTag tag;
  union {
    struct { Quantity quantity; const Value *arg; } vapt;
    struct { Int63 index; } valeg;
    struct { Text name; } vactor;
  } as;
} VAddr;

/* LegData<t> = V.vleg. */
typedef struct {
  const TermBinders *binders;
  const Closure *clo;
} VLeg;
SOLE_LIST(VLegs, const VLeg *)
SOLE_LIST_OPS(VLegs)

/* V.head. */
typedef enum { VALUE_HLOCAL, VALUE_HGLOBAL } HeadTag;
typedef struct {
  HeadTag tag;
  union {
    struct { Int63 level; } hlocal;
    struct { Text name; } hglobal;
  } as;
} Head;

/* StuckData<t> = V.stuck_elim. */
typedef struct {
  const ValueShape *shape;
  Quantity scrut_q;
  const TermMotive *motive;
  const TermBranches *branches;
  const Values *env;
} Stuck;

/* SpineData<t> = V.spine. */
typedef enum { VALUE_SOUT, VALUE_SELIM } SpineTag;
typedef struct {
  SpineTag tag;
  union {
    struct { const ValueShape *shape; const VAddr *address; } sout;
    struct { const Stuck *elim; } selim;
  } as;
} Spine;
SOLE_LIST(Spines, const Spine *)
SOLE_LIST_OPS(Spines)

/* V.t. universe: Maybe<L.t> is a NULL pointer for None{}. */
typedef enum {
  VALUE_VUNIV, VALUE_VLAN, VALUE_VRAN, VALUE_VIN, VALUE_VSEC, VALUE_VLIT,
  VALUE_VNEUTRAL
} ValueTag;
struct Value {
  ValueTag tag;
  union {
    struct { Level level; } vuniv;
    struct { const ValueShape *shape; const Closure *diagram; const Level *universe; } vlan;
    struct { const ValueShape *shape; const Closure *diagram; const Level *universe; } vran;
    struct { const ValueShape *shape; const VAddr *address; const Values *args; } vin;
    struct { const ValueShape *shape; const VLegs *legs; } vsec;
    struct { const Literal *literal; } vlit;
    struct { const Head *head; const Spines *spine; } vneutral;
  } as;
};

/* Former and Intro (named products), and the F.Pair2 answers of as_sec,
   as_neutral and as_pt, by value. */
typedef struct {
  const ValueShape *shape;
  const Closure *diagram;
  const Level *universe;
} ValueFormer;
typedef struct {
  const ValueShape *shape;
  const VAddr *address;
  const Values *args;
} ValueIntro;
typedef struct {
  const ValueShape *shape;
  const VLegs *legs;
} ValueSec;
typedef struct {
  const Head *head;
  const Spines *spine;
} ValueNeutral;
typedef struct {
  Quantity quantity;
  const Value *arg;
} ValuePoint;

const Closure *value_closure(const Values *env, const Term *body);
const VAddr *value_vapt(Quantity quantity, const Value *arg);
const VAddr *value_valeg(Int63 index);
const VAddr *value_vactor(Text name);
const VLeg *value_vleg(const TermBinders *binders, const Closure *clo);
const Head *value_hlocal(Int63 level);
const Head *value_hglobal(Text name);
const Stuck *value_stuck(const ValueShape *shape, Quantity scrut_q,
                         const TermMotive *motive,
                         const TermBranches *branches, const Values *env);
const Spine *value_sout(const ValueShape *shape, const VAddr *address);
const Spine *value_selim(const Stuck *elim);
const Value *value_vuniv(Level level);
const Value *value_vlan(const ValueShape *shape, const Closure *diagram,
                        const Level *universe);
const Value *value_vran(const ValueShape *shape, const Closure *diagram,
                        const Level *universe);
const Value *value_vin(const ValueShape *shape, const VAddr *address,
                       const Values *args);
const Value *value_vsec(const ValueShape *shape, const VLegs *legs);
const Value *value_vlit(const Literal *literal);
const Value *value_vneutral(const Head *head, const Spines *spine);

const Value *value_var(Int63 level);
const Closure *value_close(const Values *env, const Term *body);
const Level *value_as_univ(const Value *value); /* NULL = None */
int value_as_lan(const Value *value, ValueFormer *out);
int value_as_ran(const Value *value, ValueFormer *out);
int value_as_in(const Value *value, ValueIntro *out);
int value_as_sec(const Value *value, ValueSec *out);
const Literal *value_as_lit(const Value *value); /* NULL = None */
int value_as_neutral(const Value *value, ValueNeutral *out);
int value_as_pt(const VAddr *address, ValuePoint *out);
int value_as_leg(const VAddr *address, Int63 *out);
MaybeText value_as_ctor(const VAddr *address);
Bool value_head_equal(const Head *a, const Head *b);

/* ====================================== lib/kernel_recursor_group.bend */

/* F.Pair2<String, P.family>: one declared family. */
typedef struct {
  Text name;
  const Family *family;
} NamedFamily;
SOLE_LIST(NamedFamilies, NamedFamily)
SOLE_LIST_OPS(NamedFamilies)

Bool recursor_group_reaches(const Texts *names, const Ctors *ctors);
const Texts *recursor_group_joining(const Texts *names,
                                    const NamedFamilies *fams);
const Texts *recursor_group_reach(const NamedFamilies *fuel,
                                  const NamedFamilies *fams,
                                  const Texts *names);
const Texts *recursor_group_members(const NamedFamilies *fams, Text owner);

/* ================================================ lib/kernel_rules.bend */

/* Stage S2, DESIGN D1 in direct style (K/PROGRESS.md `## S2 checkpoint`).
   The Bend module is a free monad: R.program<C, A> = Pure | Abort |
   Request{task, resume}, run by Run.drive with one of three dispatchers
   (kernel_eval dispatch, kernel_conv dispatch, kernel_check dispatch with
   kernel_check drive). In C a program is a function that returns int
   (1 = Pure with the out parameter, 0 = Abort with *error). A Request is
   the call rules_request(host, &task, &answer, error); the host tells which
   dispatcher serves it. task and answer stay ADTs, so each "internal ...
   response kind" message stays reachable byte for byte. */

/* The context of a request (Bend type parameter C): G.t for the
   evaluation host, K.ctx for the conversion and checking hosts. Both
   structs are defined in stage S3 (lib/kernel_global, lib/kernel_check). */
typedef struct Globals Globals;
typedef struct CheckCtx CheckCtx;
typedef union {
  const Globals *globals; /* RULES_HOST_EVAL */
  const CheckCtx *check;  /* RULES_HOST_CONV and RULES_HOST_CHECK */
} RulesCtx;

/* The dispatcher that serves a Request (the ~dispatch of Run.drive). */
typedef enum {
  RULES_HOST_EVAL,  /* kernel_eval dispatch (line 335) */
  RULES_HOST_CONV,  /* kernel_conv dispatch (line 225), ops = runtime_ops */
  RULES_HOST_CHECK  /* kernel_check dispatch (line 389), drive (line 400) */
} RulesHostKind;

typedef struct {
  RulesHostKind kind;
  /* RULES_HOST_CHECK: the fuel that kernel_check drive threads
     (Fuelled.remaining). One cell per checking run; NULL for the other
     hosts. It is the only mutable cell other than the arena (S2
     deviation). */
  Budget *fuel;
} RulesHost;

/* Point{quantity, name, domain} */
typedef struct {
  Quantity quantity;
  Text name;
  const Value *domain;
} RulesPoint;

/* pack = PiPack{} | CollPack{} | MuPack{} */
typedef enum { RULES_PI_PACK, RULES_COLL_PACK, RULES_MU_PACK } RulesPack;

/* task<C>, in the Bend constructor order. */
typedef enum {
  RULES_EVALUATE,
  RULES_WEAK,
  RULES_READBACK,
  RULES_COMPARE,
  RULES_INFER_USES,
  RULES_CHECK_USES,
  RULES_INFER_UNIVERSE
} RulesTaskTag;
typedef struct {
  RulesTaskTag tag;
  RulesCtx context;
  union {
    struct { const Values *env; const Term *term; } evaluate;
    struct { const Value *value; } weak;
    struct { Int63 size; const Value *value; } readback;
    struct { const Value *ty; const Value *left; const Value *right; } compare;
    struct { Quantity mode; const Term *term; } infer_uses;
    struct {
      Quantity mode;
      const Term *term;
      const Value *expected;
    } check_uses;
    struct { const Term *term; } infer_universe;
  } as;
} RulesTask;

/* answer, in the Bend constructor order. */
typedef enum {
  RULES_VALUE_ANSWER,
  RULES_TERM_ANSWER,
  RULES_BOOL_ANSWER,
  RULES_INFERRED_ANSWER,
  RULES_USAGE_ANSWER,
  RULES_LEVEL_ANSWER
} RulesAnswerTag;
typedef struct {
  RulesAnswerTag tag;
  union {
    const Value *value;
    const Term *term;
    Bool equal;
    struct { const Value *ty; QuantityUsage uses; } inferred;
    QuantityUsage uses;
    Level level;
  } as;
} RulesAnswer;

/* SpineStep{argument: Maybe<V.t>, result: V.t}; argument NULL = None. */
typedef struct {
  const Value *argument;
  const Value *result;
} RulesSpineStep;

/* Request{task, resume} without the resume: serve one task on the host
   (Run.drive with its dispatcher). Defined in lib/kernel_rules.c (S2). */
int rules_request(const RulesHost *host, const RulesTask *task,
                  RulesAnswer *answer, const KError **error);

/* evaluator(C) (line 260): List<V.t> -> T.t -> program<C, V.t>. Two
   closures make one: ops_ev (kernel_rules line 441) and ev (kernel_eval
   line 84). A closed enum, no function pointer (S2 session 2). */
typedef enum {
  RULES_EV_OPS, /* ops_ev: lift(o_eval_env(ops, ctx, env, term)) */
  RULES_EV_ASK  /* kernel_eval ev: ask_eval(globals, env, term) */
} RulesEvaluatorKind;
typedef struct {
  RulesEvaluatorKind kind;
  RulesCtx ctx; /* RULES_EV_OPS: ctx.check; RULES_EV_ASK: ctx.globals */
} RulesEvaluator;

/* The three dispatchers of rules_request. Each runs the program of the
   task on the same host, so a nested Request goes back to rules_request. */
/* Defined in stage S3 (lib/kernel_eval.c, dispatch line 335). */
int eval_dispatch(const RulesHost *host, const RulesTask *task,
                  RulesAnswer *answer, const KError **error);
/* Defined in lib/kernel_conv.c (stage S2, session 6; dispatch line 225). */
int conv_dispatch(const RulesHost *host, const RulesTask *task,
                  RulesAnswer *answer, const KError **error);
/* Defined in stage S3 (lib/kernel_check.c): poll on host->fuel (drive
   lines 404-405), then the check dispatch (line 389). */
int check_request(const RulesHost *host, const RulesTask *task,
                  RulesAnswer *answer, const KError **error);

/* The fields of runtime_ops (kernel_check line 96). The o_* accessors of
   kernel_rules (lines 357-399) become these direct calls. Defined in stage
   S3 (lib/kernel_check.c). */
int runtime_ops_infer_univ(const CheckCtx *ctx, const Term *term, Level *out,
                           const KError **error);
int runtime_ops_whnf(const CheckCtx *ctx, const Value *value,
                     const Value **out, const KError **error);
const CheckCtx *runtime_ops_bind(Text name, Quantity quantity,
                                 const Value *ty, const CheckCtx *ctx);
const CheckCtx *runtime_ops_define(Text name, Quantity quantity,
                                   const Value *ty, const Value *value,
                                   const CheckCtx *ctx);
Int63 runtime_ops_size(const CheckCtx *ctx);
const Values *runtime_ops_env(const CheckCtx *ctx);
int runtime_ops_eval_env(const CheckCtx *ctx, const Values *env,
                         const Term *term, const Value **out,
                         const KError **error);
int runtime_ops_quote(const CheckCtx *ctx, const Value *value,
                      const Term **out, const KError **error);
int runtime_ops_head_ty(const CheckCtx *ctx, const Head *head,
                        const Value **out, const KError **error);
const Family *runtime_ops_family(const CheckCtx *ctx, Text name); /* NULL = None */
const NamedFamilies *runtime_ops_families(const CheckCtx *ctx);

/* The close and pp fields of check_ops (kernel_rules line 722). The one
   maker is kernel_check ops (line 98): close = close(False{}, ...) and
   pp = pp_value. Defined in stage S3 (lib/kernel_check.c). */
int check_ops_close(const CheckCtx *ctx, Int63 lower, Quantity mode,
                    QuantityUsage uses, QuantityUsage *out,
                    const KError **error);
Text check_ops_pp(const CheckCtx *ctx, const Value *value);

/* lib/kernel_rules.c, Bend lines 1-450 (S2 session 2). A Maybe of a
   pointer is NULL for None{}; a Maybe of a value is an int return (1 =
   Some, with *out). */
const Value *rules_placeholder(void);
Level rules_level_default(const Level *level);
int rules_collection_view(const ValueShape *shape, Int63 *out);
const TermBranches *rules_proj_branch(Quantity q, Bool second);
Bool rules_nat_equal(Nat a, Nat b);
const Term *rules_arrow(Quantity q, Text name, const Term *domain,
                        const Term *codomain);
const Term *rules_unit_ty(Level level);
const Term *rules_unit_val(void);
const Value *rules_bool_value(Bool b);
const Values *rules_fresh(Int63 size, Nat count);
Int63 rules_advance(Int63 size, Nat count);
Text rules_spar_word(void);
Text rules_snu_word(void);
Text rules_auto_word(void);
Text rules_mu_ran_word(void);
Text rules_wrong_pack(void);
Text rules_stuck_msg(void);
Text rules_diagram_msg(void);
Text rules_leg_msg(void);
Bool rules_branch_key(const VAddr *key, const TermAddr *address);
const VAddr *rules_collection_key(const VAddr *address);
const VAddr *rules_constructor_key(const VAddr *address);
Bool rules_ann_lvl_eq(const ValueShape *shape, const Level *u1,
                      const Level *u2);
int rules_point_view(const ValueShape *shape, RulesPoint *out);
int rules_empty_section(const Value *value, const VLegs **out,
                        const KError **error);
Bool rules_erased_fields(const Telescope *args);
Bool rules_has_eta(RulesPack p, Bool right);
const Term *rules_bool_ty(void);
/* rules(A, shape) reads only the constructor of S.t<A>, so the C twin
   takes the tag and serves TermShape and ValueShape. */
int rules_rules(ShapeTag tag, RulesPack *out, const KError **error);
Nat rules_diagram_arity(RulesPack p);
const TermLeg *rules_branch_leg(const VAddr *key,
                                const TermBranches *branches);
Bool rules_zero_constructor(const Ctor *ctor);
int rules_point_equal(const RulesHost *host, RulesCtx ctx, const Value *a,
                      const Value *b, Bool same, Bool *out,
                      const KError **error);
int rules_freeze_neutral(const ValueNeutral *neutral, const Spine *frame,
                         const Value **out, const KError **error);
int rules_value_answer(const RulesAnswer *answer, const Value **out,
                       const KError **error);
int rules_term_answer(const RulesAnswer *answer, const Term **out,
                      const KError **error);
int rules_bool_answer(const RulesAnswer *answer, Bool *out,
                      const KError **error);
Bool rules_zero_complete(const Texts *names, const Family *family);
int rules_payload_eq(const RulesHost *host, RulesCtx ctx, const Values *xs,
                     const Values *ys, Bool *out, const KError **error);
int rules_evaluate(const RulesHost *host, const RulesEvaluator *ev,
                   const Values *env, const Term *term, const Value **out,
                   const KError **error);
int rules_open_closure(const RulesHost *host, const RulesEvaluator *ev,
                       const Closure *clo, const Values *args,
                       const Value **out, const KError **error);
int rules_freeze_option(const Value *value, const Spine *frame,
                        const Value *option, const Value **out,
                        const KError **error);
int rules_ask_eval(const RulesHost *host, RulesCtx ctx, const Values *env,
                   const Term *term, const Value **out, const KError **error);
int rules_ask_whnf(const RulesHost *host, RulesCtx ctx, const Value *value,
                   const Value **out, const KError **error);
int rules_ask_quote(const RulesHost *host, RulesCtx ctx, Int63 size,
                    const Value *value, const Term **out,
                    const KError **error);
int rules_ask_conv(const RulesHost *host, RulesCtx ctx, const Value *ty,
                   const Value *a, const Value *b, Bool *out,
                   const KError **error);
RulesEvaluator rules_ops_ev(RulesCtx ctx);
int rules_ask_type(const RulesHost *host, RulesCtx ctx, const Value *a,
                   const Value *b, Bool *out, const KError **error);
int rules_open_leg(const RulesHost *host, const RulesEvaluator *ev,
                   const VLeg *leg, const Value **out, const KError **error);

/* lib/kernel_rules.c, Bend lines 451-720 (S2 session 3). map_list,
   map_shape and freeze have no twin (K/PROGRESS.md `### S2 session 3`).
   A Maybe<SpineStep> answer is a RulesSpineStep with result NULL for
   None{}. A Maybe<F.Int63.t> parameter is a pointer, NULL for None{}. */
Bool rules_zero_family_view(const Family *family, const Family *view);
RulesCtx rules_grow(Nat count, RulesCtx ctx);
int rules_family_equal(const RulesHost *host, RulesCtx ctx, const Values *a,
                       const Values *b, Bool same, Bool *out,
                       const KError **error);
int rules_beta_closure(const RulesHost *host, const RulesEvaluator *ev,
                       const Values *args, const VLeg *leg, const Value **out,
                       const KError **error);
int rules_beta_pair(const RulesHost *host, const RulesEvaluator *ev,
                    const Values *env, const ValuePoint *point,
                    const Value *arg, const TermBranch *branch,
                    const Value **out, const KError **error);
int rules_beta_body(const RulesHost *host, const RulesEvaluator *ev,
                    const Values *env, const Values *args,
                    const TermLeg *branch, const Value **out,
                    const KError **error);
int rules_coll_legs(const RulesHost *host, RulesCtx ctx,
                    const Closure *diagram, const VLegs **out,
                    const KError **error);
/* at / at_loop (lines 258, 208) and one_of (line 54): one instance per
   element type that a caller needs. */
const VLeg *rules_at_vleg(Int63 index, const VLegs *xs);
const VLeg *rules_one_of_vleg(const VLegs *xs);
const Value *rules_one_of_value(const Values *xs);
const TermBranch *rules_one_of_branch(const TermBranches *xs);
int rules_leg_type(const RulesHost *host, RulesCtx ctx, const VLegs *legs,
                   Int63 index, const Value **out, const KError **error);
Bool rules_zero_family(const Family *family);
int rules_spine_point(const RulesHost *host, RulesCtx ctx,
                      const Closure *diagram, const RulesPoint *info,
                      const ValuePoint *address, RulesSpineStep *out,
                      const KError **error);
int rules_spine_leg(const RulesHost *host, RulesCtx ctx, const VLeg *leg,
                    RulesSpineStep *out, const KError **error);
int rules_point_diagrams(const RulesHost *host, RulesCtx ctx,
                         const Closure *d1, const Closure *d2,
                         const RulesPoint *info, Bool *out,
                         const KError **error);
int rules_shape_eq(const RulesHost *host, RulesCtx ctx, const ValueShape *a,
                   const ValueShape *b, Bool *out, const KError **error);
int rules_beta_point(const RulesHost *host, const RulesEvaluator *ev,
                     const ValuePoint *point, const ValueSec *section,
                     const Value **out, const KError **error);
int rules_beta_collection(const RulesHost *host, const RulesEvaluator *ev,
                          const Int63 *index, const ValueSec *section,
                          const Value **out, const KError **error);
int rules_beta_branch(const RulesHost *host, const RulesEvaluator *ev,
                      const VAddr *key, const TermBranches *branches,
                      const Values *env, const Values *args,
                      const Value **out, const KError **error);
Bool rules_subsingleton_found(const Family *family);
int rules_spine_collection(const RulesHost *host, RulesCtx ctx,
                           const Closure *diagram, const Int63 *address,
                           RulesSpineStep *out, const KError **error);
int rules_conv_diagram(const RulesHost *host, RulesCtx ctx, RulesPack p,
                       const ValueShape *shape, const Closure *d1,
                       const Closure *d2, Bool *out, const KError **error);
int rules_beta_out(const RulesHost *host, RulesPack p,
                   const RulesEvaluator *ev, const VAddr *address,
                   const Value *value, const Value **out,
                   const KError **error);
int rules_beta_intro(const RulesHost *host, RulesPack p,
                     const RulesEvaluator *ev, const TermBranches *branches,
                     const Values *env, const ValueIntro *intro,
                     const Value **out, const KError **error);
Bool rules_subsingleton(RulesCtx ctx, const ValueShape *shape);
int rules_spine_ty(const RulesHost *host, RulesCtx ctx, RulesPack p,
                   const ValueShape *shape, const Closure *diagram,
                   const VAddr *address, RulesSpineStep *out,
                   const KError **error);
int rules_beta_elim(const RulesHost *host, RulesPack p,
                    const RulesEvaluator *ev, const TermBranches *branches,
                    const Values *env, const Value *value, const Value **out,
                    const KError **error);
int rules_out_with(const RulesHost *host, RulesPack p,
                   const RulesEvaluator *ev, const ValueShape *shape,
                   const VAddr *address, const Value *value,
                   const Value **out, const KError **error);
int rules_eta_right(const RulesHost *host, RulesCtx ctx,
                    const ValueShape *shape, const Closure *diagram,
                    const Value *a, const Value *b, Quantity q, Text name,
                    const Value *domain, Bool *out, const KError **error);
int rules_coll_eta_loop(const RulesHost *host, RulesCtx ctx,
                        const ValueShape *shape, const VLegs *legs,
                        const Value *a, const Value *b, Int63 index,
                        Int63 remaining, Bool *out, const KError **error);
int rules_out_value(const RulesHost *host, const RulesEvaluator *ev,
                    const ValueShape *shape, const VAddr *address,
                    const Value *value, const Value **out,
                    const KError **error);
int rules_elim_with(const RulesHost *host, RulesPack p,
                    const RulesEvaluator *ev, const ValueShape *shape,
                    Quantity q, const TermMotive *motive,
                    const TermBranches *branches, const Values *env,
                    const Value *value, const Value **out,
                    const KError **error);
int rules_motive_body(const RulesHost *host, RulesCtx ctx,
                      const ValueShape *shape, const Closure *diagram,
                      Quantity q, const Value *domain, Bool second,
                      const Value **out, const KError **error);
int rules_elim_value(const RulesHost *host, const RulesEvaluator *ev,
                     const ValueShape *shape, Quantity q,
                     const TermMotive *motive, const TermBranches *branches,
                     const Values *env, const Value *value,
                     const Value **out, const KError **error);
int rules_proj_motive(const RulesHost *host, RulesCtx ctx,
                      const ValueShape *shape, const Closure *diagram,
                      Quantity q, Text name, const Value *domain, Bool second,
                      const TermMotive **out, const KError **error);
int rules_eta_second(const RulesHost *host, RulesCtx ctx,
                     const ValueShape *shape, const Closure *diagram,
                     const Value *a, const Value *b, Quantity q, Text name,
                     const Value *domain, const Value *point, Bool *out,
                     const KError **error);
int rules_eta_left(const RulesHost *host, RulesCtx ctx,
                   const ValueShape *shape, const Closure *diagram,
                   const Value *a, const Value *b, Quantity q, Text name,
                   const Value *domain, Bool *out, const KError **error);
int rules_eta_point(const RulesHost *host, RulesCtx ctx,
                    const ValueShape *shape, const Closure *diagram,
                    const Value *a, const Value *b, Bool right,
                    const RulesPoint *info, Bool *out, const KError **error);
int rules_expand(const RulesHost *host, RulesCtx ctx, RulesPack p,
                 const ValueShape *shape, const Closure *diagram,
                 const Value *a, const Value *b, Bool right, Bool *out,
                 const KError **error);

/* lib/kernel_rules.c, Bend lines 721-1002 (S2 session 4). */

/* inferred = F.Pair2<V.t, Q.usage> (line 719). */
typedef struct {
  const Value *ty;
  QuantityUsage uses;
} RulesInferred;

/* List<S.t<T.t>> of samples (line 873). */
SOLE_LIST(RulesShapes, const TermShape *)
SOLE_LIST_OPS(RulesShapes)

/* F.Pair2<String, pack>: one row of packs (line 886). */
typedef struct {
  Text name;
  RulesPack pack;
} RulesNamedPack;
SOLE_LIST(RulesNamedPacks, RulesNamedPack)

/* binding_state<C> = BindingState{context, env, values} (line 979). */
typedef struct {
  RulesCtx context;
  const Values *env;
  const Values *values;
} RulesBindingState;

int rules_c_close(RulesCtx ctx, Int63 size, Quantity q, QuantityUsage uses,
                  QuantityUsage *out, const KError **error);
Text rules_c_pp(RulesCtx ctx, const Value *value);
int rules_c_conv(const RulesHost *host, RulesCtx ctx, const Value *ty,
                 const Value *a, const Value *b, Bool *out,
                 const KError **error);
int rules_c_conv_type(const RulesHost *host, RulesCtx ctx, const Value *a,
                      const Value *b, Bool *out, const KError **error);
int rules_c_eval(RulesCtx ctx, const Term *tm, const Value **out,
                 const KError **error);
int rules_c_open(RulesCtx ctx, const Closure *clo, const Values *args,
                 const Value **out, const KError **error);
RulesCtx rules_c_bind(RulesCtx ctx, Text x, Quantity q, const Value *ty);
RulesCtx rules_c_define(RulesCtx ctx, Text x, Quantity q, const Value *ty,
                        const Value *value);
Int63 rules_c_size(RulesCtx ctx);
int rules_inferred_answer(const RulesAnswer *answer, RulesInferred *out,
                          const KError **error);
int rules_usage_answer(const RulesAnswer *answer, QuantityUsage *out,
                       const KError **error);
int rules_with_point(const ValueShape *s, Text message, RulesPoint *out,
                     const KError **error);
int rules_binder_quantity(Text subject, Text source, Text name,
                          Quantity marked, Quantity declared,
                          const KError **error);
int rules_elim_result(RulesCtx ctx, const TermMotive *mo,
                      const Value *expected, const Value *self,
                      const Value **out, const KError **error);
int rules_as_tsec(const Term *tm, const TermShape **shape,
                  const TermLegs **legs);
Bool rules_diagram_unbound(const TermLegs *legs);
int rules_coll_legs_of(RulesCtx ctx, const Closure *d, const VLegs **out,
                       const KError **error);
int rules_c_open_leg(RulesCtx ctx, const VLeg *leg, const Value **out,
                     const KError **error);
int rules_coll_leg_ty(RulesCtx ctx, const VLegs *legs, Int63 k,
                      const Value **out, const KError **error);
Bool rules_nonnegative(Cmp order);
Bool rules_less_than(Cmp order);
int rules_leg_bound(Int63 k, Int63 n, const KError **error);
const RulesShapes *rules_samples(void);
const RulesNamedPacks *rules_present_pack(const RulesPack *value, Text name,
                                          const RulesNamedPacks *tail);
const RulesNamedPacks *rules_collect_packs(const RulesShapes *xs);
const RulesNamedPacks *rules_packs(void);
const Texts *rules_named_declared(void);
const Texts *rules_named_present(void);
const Telescope *rules_family_params(const Family *f);
const Telescope *rules_family_indices(const Family *f);
Level rules_family_level(const Family *f);
const Telescope *rules_ctor_args(const Ctor *c);
const Terms *rules_ctor_indices(const Ctor *c);
int rules_mu_family_status(const Family *f, const Family *view,
                           const Family **out, const KError **error);
int rules_mu_family(RulesCtx ctx, Text n, const Family **out,
                    const KError **error);
const Terms *rules_leg_bodies(const TermLegs *legs);
int rules_mu_params_of(const Term *diagram, const Terms **out,
                       const KError **error);
int rules_c_eval_env(RulesCtx ctx, const Values *env, const Term *tm,
                     const Value **out, const KError **error);
int rules_mu_param_legs(RulesCtx ctx, const VLegs *legs, const Values *env,
                        const Values **out, const KError **error);
int rules_mu_param_env(RulesCtx ctx, const Closure *d, const Values **out,
                       const KError **error);
int rules_mu_index_walk(const RulesHost *host, RulesCtx ctx, Text n,
                        Text ctor, const Terms *indices, const Values *wants,
                        const Values *env, const KError **error);
int rules_mu_indices(const RulesHost *host, RulesCtx ctx, Text n,
                     const Ctor *ct, const Values *ixv, const Values *env,
                     const KError **error);
int rules_mu_motive_view(Text n, Int63 want, Int63 applied,
                         const TermMotive *mo, const TermMotive *view,
                         const TermMotive **out, const KError **error);
int rules_mu_motive_of(Text n, const Family *fam, const Values *ixv,
                       const TermMotive *mo, const TermMotive **out,
                       const KError **error);
int rules_mu_result(RulesCtx ctx, const TermMotive *mo, const Values *idx,
                    const Value *self, const Value **out,
                    const KError **error);
RulesBindingState rules_bound_state(RulesCtx ctx, const Values *env,
                                    const Values *vals, Text name,
                                    Quantity q, const Value *ty);
int rules_mu_bind_indices(RulesCtx origin, const Telescope *tele,
                          const Texts *names, RulesBindingState state,
                          RulesBindingState *out, const KError **error);
int rules_mu_large(Text n, const Family *fam, Level mlvl,
                   const KError **error);
int rules_mu_ctor_names(Text n, const Family *fam, const Texts **out,
                        const KError **error);

/* lib/kernel_rules.c, Bend lines 1004-1376 (S2 session 5). */
int rules_coll_width_term(const TermShape *s, Int63 *out);
int rules_as_mu_term(const TermShape *s, Text *name, const Terms **indices);
int rules_as_mu_value(const ValueShape *s, Text *name,
                      const Values **indices);
int rules_two_of_binders(const TermBinders *xs, TermBinder *x, TermBinder *y);
const Term *rules_one_of_term(const Terms *xs);
const TermBinder *rules_one_of_binder(const TermBinders *xs);
int rules_mu_keys(Text n, const TermBranches *branches, const Texts **out,
                  const KError **error);
Int63 rules_mu_count(Text key, const Texts *names);
int rules_mu_cover_names(Text n, const Texts *names, const Texts *keys,
                         const KError **error);
int rules_mu_cover_keys(Text n, const Texts *names, const Texts *keys,
                        const KError **error);
int rules_mu_cover(Text n, const Texts *names, const TermBranches *branches,
                   const KError **error);
int rules_mu_bind_fields(RulesCtx origin, const Telescope *tele,
                         const TermBinders *binders, RulesBindingState state,
                         RulesBindingState *out, const KError **error);
int rules_mu_values(RulesCtx ctx, const Values *env, const Terms *terms,
                    const Values **out, const KError **error);
const Level *rules_annotation(const Value *value);
int rules_level_answer(const RulesAnswer *answer, Level *out,
                       const KError **error);
int rules_ask_univ(const RulesHost *host, RulesCtx ctx, const Term *tm,
                   Level *out, const KError **error);
int rules_c_infer(const RulesHost *host, RulesCtx ctx, Quantity q,
                  const Term *tm, RulesInferred *out, const KError **error);
int rules_c_check(const RulesHost *host, RulesCtx ctx, Quantity q,
                  const Term *tm, const Value *ty, QuantityUsage *out,
                  const KError **error);
int rules_c_univ(const RulesHost *host, RulesCtx ctx, const Term *tm,
                 Level *out, const KError **error);
int rules_with_former(RulesCtx ctx, Bool right, Text message, const Value *ty,
                      const Value **w, ValueFormer *former,
                      const KError **error);
int rules_infer_scrutinee(const RulesHost *host, RulesCtx ctx, Quantity mode,
                          Quantity q, const Term *scrut, RulesInferred *out,
                          const KError **error);
int rules_check_motive(const RulesHost *host, RulesCtx ctx,
                       const TermMotive *mo, const Value *ty,
                       const KError **error);
int rules_form_pi(const RulesHost *host, RulesCtx ctx, const TermShape *shape,
                  const Term *diagram, Bool right, Level *out,
                  const KError **error);
int rules_pi_sec_body(const RulesHost *host, RulesCtx ctx, RulesCtx inner,
                      Quantity mode, const Term *body, const Value *cod,
                      QuantityUsage *out, const KError **error);
int rules_pi_intro_in(const RulesHost *host, RulesCtx ctx, Quantity mode,
                      const TermAddr *addr, const Terms *args,
                      const Value *expected, QuantityUsage *out,
                      const KError **error);
int rules_pi_elim_out(const RulesHost *host, RulesCtx ctx, Quantity mode,
                      const TermAddr *addr, const Term *head,
                      RulesInferred *out, const KError **error);
int rules_pi_branch_body(const RulesHost *host, RulesCtx ctx, RulesCtx inner,
                         Quantity mode, const Term *body, const TermMotive *mo,
                         const Value *expected, const Value *self,
                         QuantityUsage *out, const KError **error);
int rules_coll_levels(const RulesHost *host, RulesCtx ctx,
                      const TermLegs *legs, Level *out, const KError **error);
int rules_form_coll(const RulesHost *host, RulesCtx ctx,
                    const TermShape *shape, const Term *diagram,
                    const Level *expected, Level *out, const KError **error);
int rules_coll_tuple(const RulesHost *host, RulesCtx ctx, Quantity mode,
                     const TermLegs *legs, const VLegs *dlegs,
                     QuantityUsage *out, const KError **error);
int rules_coll_intro_sec(const RulesHost *host, RulesCtx ctx, Quantity mode,
                         const TermShape *s, const TermLegs *legs,
                         const Value *expected, QuantityUsage *out,
                         const KError **error);
int rules_coll_intro_in(const RulesHost *host, RulesCtx ctx, Quantity mode,
                        const TermAddr *addr, const Terms *args,
                        const Value *expected, QuantityUsage *out,
                        const KError **error);
int rules_coll_elim_out(const RulesHost *host, RulesCtx ctx, Quantity mode,
                        const TermAddr *addr, const Term *head,
                        RulesInferred *out, const KError **error);
int rules_coll_branch(const RulesHost *host, RulesCtx ctx, Quantity mode,
                      const ValueShape *vs, const VLegs *dlegs,
                      const TermMotive *mo, const Value *expected,
                      const TermBranches *branches, Int63 k,
                      QuantityUsage *out, const KError **error);
int rules_coll_branches(const RulesHost *host, RulesCtx ctx, Quantity mode,
                        const ValueShape *vs, const VLegs *dlegs,
                        const TermMotive *mo, const Value *expected,
                        const TermBranches *branches,
                        const TermBranches *steps, Int63 k,
                        QuantityUsage *out, const KError **error);
int rules_coll_elim_elim(const RulesHost *host, RulesCtx ctx, Quantity mode,
                         const TermElim *e, const Value *expected,
                         RulesInferred *out, const KError **error);
int rules_elim_out_pack(const RulesHost *host, RulesCtx ctx, Quantity mode,
                        RulesPack p, const TermAddr *addr, const Term *head,
                        RulesInferred *out, const KError **error);
int rules_elim_out(const RulesHost *host, RulesCtx ctx, Quantity mode,
                   const TermShape *s, const TermAddr *addr, const Term *head,
                   RulesInferred *out, const KError **error);
int rules_tele_walk(const RulesHost *host, RulesCtx ctx, Quantity mode,
                    const Telescope *tele, const Terms *args,
                    const Values *env, QuantityUsage uses,
                    const Values **env_out, QuantityUsage *uses_out,
                    const KError **error);
int rules_mu_telescope_uses(const RulesHost *host, RulesCtx ctx, Quantity mode,
                            Text what, const Telescope *tele,
                            const Terms *args, const Values *env,
                            const Values **env_out, QuantityUsage *uses_out,
                            const KError **error);
int rules_mu_telescope(const RulesHost *host, RulesCtx ctx, Quantity mode,
                       Text what, const Telescope *tele, const Terms *args,
                       const Values *env, const Values **out,
                       const KError **error);
int rules_form_mu(const RulesHost *host, RulesCtx ctx, const TermShape *s,
                  const Term *diagram, Level *out, const KError **error);
int rules_mu_intro_in(const RulesHost *host, RulesCtx ctx, Quantity mode,
                      const TermAddr *addr, const Terms *args,
                      const Value *expected, QuantityUsage *out,
                      const KError **error);
int rules_mu_motive_body(const RulesHost *host, Text n, Text self,
                         const Term *body, const Closure *d, const Level *u,
                         RulesBindingState state, Level *out,
                         const KError **error);
int rules_mu_motive_lvl(const RulesHost *host, RulesCtx ctx, Text n,
                        const Family *fam, const TermMotive *mo,
                        const Closure *d, const Level *u, const Values *penv,
                        Level *out, const KError **error);
int rules_mu_branch_body(const RulesHost *host, RulesCtx ctx, Quantity mode,
                         Text n, Text ctor, const TermMotive *mo,
                         const Terms *indices, const Term *body,
                         RulesBindingState state, QuantityUsage *out,
                         const KError **error);
int rules_mu_branch(const RulesHost *host, RulesCtx ctx, Quantity mode, Text n,
                    const Family *fam, const TermMotive *mo,
                    const Values *penv, const TermBranches *branches, Text c,
                    QuantityUsage *out, const KError **error);
int rules_mu_branches(const RulesHost *host, RulesCtx ctx, Quantity mode,
                      Text n, const Family *fam, const TermMotive *mo,
                      const Values *penv, const TermBranches *branches,
                      const Texts *names, QuantityUsage *out,
                      const KError **error);
const TermLeg *rules_one_of_leg(const TermLegs *xs);
int rules_mu_elim_elim(const RulesHost *host, RulesCtx ctx, Quantity mode,
                       const TermElim *e, RulesInferred *out,
                       const KError **error);
int rules_pi_intro_sec(const RulesHost *host, RulesCtx ctx, Quantity mode,
                       const TermLegs *legs, const Value *expected,
                       QuantityUsage *out, const KError **error);
int rules_pi_branch(const RulesHost *host, RulesCtx ctx, Quantity mode,
                    const ValueShape *vs, const Closure *d, Quantity q,
                    const Value *dom, const TermMotive *mo,
                    const Value *expected, const TermAddr *key,
                    const TermLeg *leg, QuantityUsage *out,
                    const KError **error);
int rules_pi_elim_elim(const RulesHost *host, RulesCtx ctx, Quantity mode,
                       const TermElim *e, const Value *expected,
                       RulesInferred *out, const KError **error);
int rules_form_pack(const RulesHost *host, RulesCtx ctx, RulesPack p,
                    const TermShape *s, const Term *d, const Level *expected,
                    Bool right, Level *out, const KError **error);
int rules_form(const RulesHost *host, RulesCtx ctx, const TermShape *s,
               const Term *d, const Level *expected, Bool right, Level *out,
               const KError **error);
int rules_intro_sec_pack(const RulesHost *host, RulesCtx ctx, Quantity mode,
                         RulesPack p, const TermShape *s, const TermLegs *legs,
                         const Value *expected, QuantityUsage *out,
                         const KError **error);
int rules_intro_sec(const RulesHost *host, RulesCtx ctx, Quantity mode,
                    const TermShape *s, const TermLegs *legs,
                    const Value *expected, QuantityUsage *out,
                    const KError **error);
int rules_intro_in_pack(const RulesHost *host, RulesCtx ctx, Quantity mode,
                        RulesPack p, const TermAddr *addr, const Terms *args,
                        const Value *expected, QuantityUsage *out,
                        const KError **error);
int rules_intro_in(const RulesHost *host, RulesCtx ctx, Quantity mode,
                   const TermShape *s, const TermAddr *addr, const Terms *args,
                   const Value *expected, QuantityUsage *out,
                   const KError **error);
int rules_elim_elim_pack(const RulesHost *host, RulesCtx ctx, Quantity mode,
                         RulesPack p, const TermElim *e, const Value *expected,
                         RulesInferred *out, const KError **error);
int rules_elim_elim(const RulesHost *host, RulesCtx ctx, Quantity mode,
                    const TermElim *e, const Value *expected,
                    RulesInferred *out, const KError **error);

/* ---------------------------------------------------------------- sections

   Filled stage by stage, in this order (one section per .bend twin):

   S1 lib/foundation, lib/kernel_error, lib/kernel_budget, lib/kernel_level,
      lib/kernel_literal, lib/kernel_quantity (above); S1b: lib/kernel_shape,
      lib/kernel_term, lib/kernel_positivity, lib/kernel_pp,
      lib/kernel_value, lib/kernel_recursor_group
   S2 lib/kernel_rules, lib/kernel_conv, lib/kernel_prim
   S3 lib/kernel_global, lib/kernel_eval, lib/kernel_recursor*,
      lib/kernel_check
   S4 erase/eterm, erase/runtime, erase/dependencies, erase/type,
      erase/family, erase/pair, erase/mu, erase/sum, erase/core
   S5 surface/token, surface/lexer, surface/syntax, surface/record,
      surface/source, surface/order, surface/totality, surface/elab_program,
      surface/constructor, surface/constructor_infer, surface/sugar,
      surface/family
   S6 surface/parser, surface/elab, surface/program, surface/check,
      bin/sole-comb (main.c twin: the public CLI, no hex protocol)

   Shape of one ADT section (example, kernel_term):

     typedef enum { TERM_VAR, TERM_UNIV, ... } TermTag;   one per constructor
     typedef struct Term Term;
     struct Term {
       TermTag tag;
       union {
         struct { Int63 index; } var;
         struct { Level level; } univ;
         ...
       } as;
     };
     const Term *term_var(Int63 index);                   one per constructor
     SOLE_LIST(Terms, const Term *)

   Nullary constructors are static const values, not arena cells. */

/* ================================================= lib/kernel_conv.bend */

/* lib/kernel_conv.c (S2 session 6). C = K.ctx (ops = runtime_ops at each
   caller). A Maybe of a pointer is NULL for None{}. BranchContext has no
   twin: branches_equal and branch_equal take ctx, env1 and env2. */
Bool conv_prop_level(int done, Level level);
int conv_shape_equal(const RulesHost *host, RulesCtx ctx,
                     const ValueShape *a, const ValueShape *b, Bool *out,
                     const KError **error);
int conv_values_equal(const RulesHost *host, RulesCtx ctx, const Values *a,
                      const Values *b, Bool *out, const KError **error);
int conv_point_equal(const RulesHost *host, RulesCtx ctx, const Value *ty,
                     const Value *a, const Value *b, Bool *out,
                     const KError **error);
int conv_open_pair(const RulesHost *host, RulesCtx ctx, Nat count,
                   const Closure *a, const Closure *b, Bool *out,
                   const KError **error);
int conv_branch_addr(const RulesHost *host, RulesCtx ctx, const Values *env1,
                     const Values *env2, const TermAddr *a, const TermAddr *b,
                     Bool *out, const KError **error);
const Values *conv_motive_env(Int63 size, Nat count, const Values *env);
int conv_spine_step_ran(const RulesHost *host, RulesCtx ctx,
                        const ValueShape *shape, const VAddr *addr,
                        const ValueFormer *former, RulesSpineStep *out,
                        const KError **error);
const Value *conv_step_arg(const RulesSpineStep *step);
const Value *conv_step_next(const RulesSpineStep *step);
int conv_subsingleton(RulesCtx ctx, const Value *ty, Bool *out,
                      const KError **error);
int conv_is_prop(const RulesHost *host, RulesCtx ctx, const Value *ty,
                 Bool *out, const KError **error);
int conv_conv_addr(const RulesHost *host, RulesCtx ctx, const Value *ty,
                   const VAddr *a, const VAddr *b, Bool *out,
                   const KError **error);
int conv_closures_equal(const RulesHost *host, RulesCtx ctx, Nat count,
                        const Closure *a, Nat other, const Closure *b,
                        Bool *out, const KError **error);
int conv_motive_bodies(const RulesHost *host, RulesCtx ctx,
                       const Values *env1, const Values *env2,
                       const Term *body1, const Term *body2, Nat count,
                       Bool *out, const KError **error);
int conv_spine_step(const RulesHost *host, RulesCtx ctx, const Value *hty,
                    const ValueShape *shape, const VAddr *addr,
                    RulesSpineStep *out, const KError **error);
int conv_former_pair(const RulesHost *host, RulesCtx ctx,
                     const ValueShape *s1, const Closure *d1,
                     const Level *u1, const ValueFormer *other, Bool *out,
                     const KError **error);
int conv_leg_equal(const RulesHost *host, RulesCtx ctx, const VLeg *a,
                   const VLeg *b, Bool *out, const KError **error);
int conv_branch_equal(const RulesHost *host, RulesCtx ctx,
                      const Values *env1, const Values *env2,
                      const TermBranch *a, const TermBranch *b, Bool *out,
                      const KError **error);
int conv_motive_counts(const RulesHost *host, RulesCtx ctx,
                       const Values *env1, const Values *env2,
                       const Term *body1, const Term *body2, Nat count,
                       Nat other, Bool *out, const KError **error);
int conv_in_pair(const RulesHost *host, RulesCtx ctx, const ValueShape *s1,
                 const VAddr *a1, const Values *args1,
                 const ValueIntro *other, Bool *out, const KError **error);
int conv_legs_equal(const RulesHost *host, RulesCtx ctx, const VLegs *a,
                    const VLegs *b, Bool *out, const KError **error);
int conv_branches_equal(const RulesHost *host, RulesCtx ctx,
                        const Values *env1, const Values *env2,
                        const TermBranches *a, const TermBranches *b,
                        Bool *out, const KError **error);
int conv_motives_equal(const RulesHost *host, RulesCtx ctx,
                       const Values *env1, const Values *env2,
                       const TermMotive *a, const TermMotive *b, Bool *out,
                       const KError **error);
int conv_stuck_equal(const RulesHost *host, RulesCtx ctx, const Stuck *a,
                     const Stuck *b, Bool *out, const KError **error);
int conv_sec_pair(const RulesHost *host, RulesCtx ctx, const ValueShape *s1,
                  const VLegs *legs1, const ValueSec *other, Bool *out,
                  const KError **error);
int conv_spine_equal(const RulesHost *host, RulesCtx ctx, const Spines *a,
                     const Spines *b, const Value *hty, Bool *out,
                     const KError **error);
int conv_neutral_pair(const RulesHost *host, RulesCtx ctx, const Head *h1,
                      const Spines *sp1, const ValueNeutral *other,
                      Bool *out, const KError **error);
int conv_structural_values(const RulesHost *host, RulesCtx ctx,
                           const Value *a, const Value *b, Bool *out,
                           const KError **error);
int conv_structural(const RulesHost *host, RulesCtx ctx, const Value *a,
                    const Value *b, Bool *out, const KError **error);
int conv_eta_former(const RulesHost *host, RulesCtx ctx,
                    const ValueShape *shape, const Closure *diagram,
                    const Value *a, const Value *b, Bool right, Bool *out,
                    const KError **error);
int conv_eta_by_type(const RulesHost *host, RulesCtx ctx, const Value *ty,
                     const Value *a, const Value *b, Bool *out,
                     const KError **error);
int conv_conv_step(const RulesHost *host, RulesCtx ctx, const Value *ty,
                   const Value *a, const Value *b, Bool *out,
                   const KError **error);
int conv_conv(const CheckCtx *ctx, const Value *ty, const Value *a,
              const Value *b, Bool *out, const KError **error);
int conv_conv_type(const CheckCtx *ctx, const Value *a, const Value *b,
                   Bool *out, const KError **error);

/* ================================================= lib/kernel_prim.bend */

/* t = Nat_add | Nat_sub | Nat_mul | Nat_eq | Nat_lt. */
typedef enum {
  PRIM_NAT_ADD,
  PRIM_NAT_SUB,
  PRIM_NAT_MUL,
  PRIM_NAT_EQ,
  PRIM_NAT_LT
} Prim;
SOLE_LIST(Prims, Prim)
SOLE_LIST_OPS(Prims)
/* List<Lit.t>: the literal arguments of a primitive. */
SOLE_LIST(Literals, const Literal *)
SOLE_LIST_OPS(Literals)
/* F.Pair2<F.Bignum.t, F.Bignum.t>. */
typedef struct {
  Bignum first;
  Bignum second;
} PrimNats;

/* lib/kernel_prim.c (S2 session 6). */
Text prim_name(Prim prim);
Nat prim_arity(Prim prim);
const Prims *prim_catalog(void);
int prim_lookup(Text source, const Prims *prims, Prim *out);
int prim_of_name(Text source, Prim *out);
Text prim_nat_name(void);
const Term *prim_nat_ty(void);
const Term *prim_binary(const Term *result);
const Term *prim_ty(Prim prim);
int prim_nonnegative(Cmp sign, Bignum value, Bignum *out);
int prim_as_nat(const Literal *literal, Bignum *out);
int prim_add(Bignum a, Bignum b, Bignum *out, const KError **error);
int prim_sub(Bignum a, Bignum b, Bignum *out, const KError **error);
int prim_mul(Bignum a, Bignum b, Bignum *out, const KError **error);
int prim_natural_pair(const Bignum *a, const Bignum *b, PrimNats *out);
int prim_two_nats(const Literals *args, PrimNats *out);
const Literal *prim_reduce_nats(Prim prim, Bignum a, Bignum b);
const Literal *prim_reduce_pair(Prim prim, const PrimNats *pair);
int prim_reduce(Prim prim, const Literals *args, const Literal **out,
                const KError **error);
Bool prim_less(Cmp order);
const Value *prim_apply_nats(Prim prim, Bignum a, Bignum b);
const Value *prim_apply_pair(Prim prim, const PrimNats *pair);
int prim_apply(Prim prim, const Literals *args, const Value **out,
               const KError **error);

/* =============================================== lib/kernel_global.bend */

/* def_entry = Definition{ty, body, reducible, rec_arg, partial}. rec_arg
   NULL = None{}. */
typedef struct {
  const Term *ty;
  const Term *body;
  Bool reducible;
  const Int63 *rec_arg;
  Bool partial;
} GlobalDef;
/* axiom_entry = Postulate{ax_ty}. */
typedef struct {
  const Term *ty;
} GlobalAxiom;
/* prim_entry = Primitive{p_ty, prim}. */
typedef struct {
  const Term *ty;
  Prim prim;
} GlobalPrim;
/* entry = Def{definition} | Axiom{postulate} | Prim{primitive}. */
typedef enum { GLOBAL_DEF, GLOBAL_AXIOM, GLOBAL_PRIM } GlobalEntryTag;
typedef struct {
  GlobalEntryTag tag;
  union {
    GlobalDef def;
    GlobalAxiom axiom;
    GlobalPrim prim;
  } as;
} GlobalEntry;

/* t = Globals{entries, families}. entries maps a name to a const
   GlobalEntry *; families maps a name to a const Family *. The typedef is
   in the kernel_rules section (RulesCtx). */
struct Globals {
  const Map *entries;
  const Map *families;
};

/* lib/kernel_global.c (S3 session 1). A Maybe of an entry, a def_entry,
   an axiom_entry, a prim_entry or a family is a pointer: NULL = None{}.
   telescope (line 10) is a type alias: no twin. The three makers build
   the entry constructors (Def, Axiom, Prim). */
const GlobalEntry *global_definition(const Term *ty, const Term *body,
                                     Bool reducible, const Int63 *rec_arg,
                                     Bool partial);
const GlobalEntry *global_postulate(const Term *ty);
const GlobalEntry *global_primitive(const Term *ty, Prim prim);
const Globals *global_empty(void);
const GlobalEntry *global_find(Text name, const Globals *globals);
const Globals *global_add(Text name, const GlobalEntry *value,
                          const Globals *globals);
const Family *global_find_family(Text name, const Globals *globals);
const NamedFamilies *global_family_list(const Globals *globals);
const Globals *global_add_family(Text name, const Family *fam,
                                 const Globals *globals);
const Term *global_entry_ty(const GlobalEntry *value);
const GlobalDef *global_def_of(const GlobalEntry *value);
const GlobalAxiom *global_axiom_of(const GlobalEntry *value);
const GlobalPrim *global_prim_of(const GlobalEntry *value);
const GlobalDef *global_found_def(const GlobalEntry *value);
const GlobalAxiom *global_found_axiom(const GlobalEntry *value);
const GlobalPrim *global_found_prim(const GlobalEntry *value);
const GlobalDef *global_find_def(Text name, const Globals *globals);
const GlobalAxiom *global_find_axiom(Text name, const Globals *globals);
const GlobalPrim *global_find_prim(Text name, const Globals *globals);
const Globals *global_add_prims(const Prims *prims, const Globals *globals);
const Globals *global_initial(void);

/* ================================================= lib/kernel_eval.bend */

/* recursive = Recursive{body, index, frames}. */
typedef struct {
  const Term *body;
  Int63 index;
  const Spines *frames;
} EvalRecursive;

/* lib/kernel_eval.c (S3 session 1). C = G.t: each program takes the host
   and ctx (ctx.globals). A Maybe of a level, a literal, an intro, a
   neutral, a prim (in eval_prim_arguments) or a recursive (in
   eval_whnf_candidate) is a pointer: NULL = None{}. A Maybe that a def
   returns is an int return with an out parameter (1 = Some{}).
   eval_prim_arguments takes Maybe<List<Lit.t>> as has_literals and
   literals. No twin: literal_next, guarded_index and guarded_point (folded
   into the loops of eval_spine_literals and eval_guarded_frames), and
   mapped_eval, mapped_quote and mapped_leg (S2 session 3: each map_list
   or map_shape caller is a loop). */
const VLegs *eval_leg_closures(const Values *env, const TermLegs *legs);
const Level *eval_fill(const Level *old, const Level *inferred);
int eval_found_prim(const GlobalPrim *found, Prim *out);
const Value *eval_prim_answer(const Value *value, const Value *found);
Bool eval_guarded_intro(const ValueIntro *intro);
const Term *eval_annotate_former(const Term *core, const Level *universe);
const Term *eval_quote_head(Int63 size, const Head *head);
const Literal *eval_frame_literal(const Spine *frame);
int eval_request_eval(const RulesHost *host, RulesCtx ctx, const Values *env,
                      const Term *term, const Value **out,
                      const KError **error);
int eval_request_whnf(const RulesHost *host, RulesCtx ctx, const Value *value,
                      const Value **out, const KError **error);
int eval_request_quote(const RulesHost *host, RulesCtx ctx, Int63 size,
                       const Value *value, const Term **out,
                       const KError **error);
RulesEvaluator eval_ev(RulesCtx ctx);
int eval_eval_addr(const RulesHost *host, RulesCtx ctx, const Values *env,
                   const TermAddr *address, const VAddr **out,
                   const KError **error);
int eval_eval_entry(const RulesHost *host, RulesCtx ctx, Text name,
                    const GlobalEntry *found, const Value **out,
                    const KError **error);
int eval_level_of_ann(const RulesHost *host, RulesCtx ctx, const Values *env,
                      const Term *ty, const Level **out,
                      const KError **error);
int eval_head_prim(RulesCtx ctx, const Head *head, Prim *out);
int eval_spine_literals(const Spines *spine, const Literals *acc,
                        const Literals **out);
int eval_prim_arity(const Value *value, Prim prim, const Literals *args,
                    Bool saturated, const Value **out, const KError **error);
int eval_candidate_def(const Spines *spine, const GlobalDef *found,
                       EvalRecursive *out);
const Term *eval_former(Bool right, const TermShape *shape,
                        const Term *diagram, const Level *universe);
int eval_quote_addr(const RulesHost *host, RulesCtx ctx, Int63 size,
                    const VAddr *address, const TermAddr **out,
                    const KError **error);
int eval_eval_global(const RulesHost *host, RulesCtx ctx, Text name,
                     const Value **out, const KError **error);
int eval_annotate(const RulesHost *host, RulesCtx ctx, const Values *env,
                  const Value *value, const Term *ty, const Value **out,
                  const KError **error);
int eval_prim_arguments(const Value *value, const Prim *prim,
                        int has_literals, const Literals *literals,
                        const Value **out, const KError **error);
int eval_candidate_neutral(RulesCtx ctx, const ValueNeutral *neutral,
                           EvalRecursive *out);
int eval_replay_frames(const RulesHost *host, RulesCtx ctx,
                       const Spines *frames, const Value *value,
                       const Value **out, const KError **error);
int eval_quote_diagram(const RulesHost *host, RulesCtx ctx, Int63 size,
                       const TermShape *shape, const Closure *diagram,
                       const Level *universe, Bool right, Nat arity,
                       const Term **out, const KError **error);
int eval_quote_leg_arity(const RulesHost *host, RulesCtx ctx, Int63 size,
                         const TermBinders *binders, const Closure *clo,
                         Nat arity, const TermLeg **out,
                         const KError **error);
int eval_eval_shape(const RulesHost *host, RulesCtx ctx, const Values *env,
                    const TermShape *shape, const ValueShape **out,
                    const KError **error);
int eval_eval_list(const RulesHost *host, RulesCtx ctx, const Values *env,
                   const Terms *args, const Values **out,
                   const KError **error);
int eval_quote_shape(const RulesHost *host, RulesCtx ctx, Int63 size,
                     const ValueShape *shape, const TermShape **out,
                     const KError **error);
int eval_quote_list(const RulesHost *host, RulesCtx ctx, Int63 size,
                    const Values *args, const Terms **out,
                    const KError **error);
int eval_replay(const RulesHost *host, RulesCtx ctx, const Value *value,
                const Spines *frames, const Value **out,
                const KError **error);
int eval_prim_neutral(RulesCtx ctx, const Value *value,
                      const ValueNeutral *neutral, const Value **out,
                      const KError **error);
int eval_candidate(RulesCtx ctx, const Value *value, EvalRecursive *out);
Bool eval_guarded_frames(const Spines *frames, Int63 index);
int eval_quote_former(const RulesHost *host, RulesCtx ctx, Int63 size,
                      const ValueShape *shape, const Closure *diagram,
                      const Level *universe, Bool right, const Term **out,
                      const KError **error);
int eval_quote_leg(const RulesHost *host, RulesCtx ctx, Int63 size,
                   const VLeg *leg, const TermLeg **out,
                   const KError **error);
int eval_eval_step(const RulesHost *host, RulesCtx ctx, const Values *env,
                   const Term *term, const Value **out, const KError **error);
int eval_quote_spine(const RulesHost *host, RulesCtx ctx, Int63 size,
                     const Spines *frames, const Term *term,
                     const Term **out, const KError **error);
int eval_quote_frames(const RulesHost *host, RulesCtx ctx, Int63 size,
                      const Term *term, const Spines *frames,
                      const Term **out, const KError **error);
Bool eval_guarded_frame(Int63 index, const Spines *frames);
int eval_whnf_guard(const RulesHost *host, RulesCtx ctx, const Value *value,
                    const Term *body, const Spines *frames, Bool guarded,
                    const Value **out, const KError **error);
int eval_prim_step(RulesCtx ctx, const Value *value, const Value **out,
                   const KError **error);
int eval_whnf_candidate(const RulesHost *host, RulesCtx ctx,
                        const Value *value, const EvalRecursive *recursion,
                        const Value **out, const KError **error);
int eval_quote_legs(const RulesHost *host, RulesCtx ctx, Int63 size,
                    const VLegs *legs, const TermLegs **out,
                    const KError **error);
int eval_quote_step(const RulesHost *host, RulesCtx ctx, Int63 size,
                    const Value *value, const Term **out,
                    const KError **error);
int eval_whnf_value(const RulesHost *host, RulesCtx ctx, const Value *value,
                    const Value **out, const KError **error);
int eval_whnf_step(const RulesHost *host, RulesCtx ctx, const Value *value,
                   const Value **out, const KError **error);
/* eval, whnf and quote (lines 345-352): R.run with dispatch, so an
   evaluation host on the stack (no fuel). */
int eval_eval(const Globals *globals, const Values *env, const Term *term,
              const Value **out, const KError **error);
int eval_whnf(const Globals *globals, const Value *value, const Value **out,
              const KError **error);
int eval_quote(const Globals *globals, Int63 size, const Value *value,
               const Term **out, const KError **error);

/* ============================================ lib/kernel_recursor.bend */
/* S3 session 2. C = K.ctx: each context argument is a RulesCtx (ctx.check).
   A program takes the host first and returns int (1 = Pure with *out,
   0 = Abort with *error). A Unit program has no *out. */

/* role = Ordinary{} | Direct{indices}. */
typedef enum { RECURSOR_ORDINARY, RECURSOR_DIRECT } RecursorRoleTag;
typedef struct {
  RecursorRoleTag tag;
  const Values *indices; /* RECURSOR_DIRECT */
} RecursorRole;
/* field = Field{position, quantity, name, domain, value, role}. */
typedef struct {
  Int63 position;
  Quantity quantity;
  Text name;
  const Value *domain;
  const Value *value;
  RecursorRole role;
} RecursorField;
SOLE_LIST(RecursorFields, const RecursorField *)
SOLE_LIST_OPS(RecursorFields)
/* plan<C> = Plan{context, fields}. */
typedef struct {
  RulesCtx context;
  const RecursorFields *fields;
} RecursorPlan;
/* scope = Abstract{} | Instance{values}. */
typedef enum { RECURSOR_ABSTRACT, RECURSOR_INSTANCE } RecursorScopeTag;
typedef struct {
  RecursorScopeTag tag;
  const Values *values; /* RECURSOR_INSTANCE */
} RecursorScope;

int recursor_abstract(const RulesHost *host, const Telescope *tele,
                      RulesCtx c, RulesCtx *out, const KError **error);
int recursor_instance(const RulesHost *host, const Telescope *tele,
                      const Values *values, RulesCtx c, RulesCtx *out,
                      const KError **error);
int recursor_open(const RulesHost *host, RecursorScope scope,
                  const Telescope *tele, RulesCtx c, RulesCtx *out,
                  const KError **error);
int recursor_parameters(const RulesHost *host, const Telescope *tele,
                        RulesCtx c, RulesCtx *out, const KError **error);
RulesCtx recursor_install(RecursorScope scope, Text x, Quantity q,
                          const Value *tyv, const Value *v, RulesCtx c);
const KError *recursor_refusal(Text owner, const Term *tm);
int recursor_ordinary(const RulesHost *host, RulesCtx c, Text owner,
                      const Texts *group, const Value *ty, RecursorRole *out,
                      const KError **error);
/* view = Maybe<F.Pair2<String, List<V.t>>>: found NO = None{}. */
int recursor_family_role(const RulesHost *host, RulesCtx c, Text owner,
                         const Texts *group, const Value *ty, Bool found,
                         Text name, const Values *indices, RecursorRole *out,
                         const KError **error);
/* view NULL = None{}. */
int recursor_former_role(const RulesHost *host, RulesCtx c, Text owner,
                         const Texts *group, const Value *ty,
                         const ValueFormer *view, RecursorRole *out,
                         const KError **error);
RecursorPlan recursor_prepend(const RecursorField *field, RecursorPlan value);
int recursor_fields(const RulesHost *host, Text owner, const Texts *group,
                    const Telescope *tele, RulesCtx c, Int63 position,
                    RecursorPlan *out, const KError **error);
int recursor_constructor(const RulesHost *host, RulesCtx c,
                         RecursorScope scope, Text owner, const Texts *group,
                         const Telescope *params, const Ctor *value,
                         RecursorPlan *out, const KError **error);
int recursor_family(const RulesHost *host, RulesCtx c, RecursorScope scope,
                    Text owner, const Texts *group, Text key,
                    const Family *value, RecursorPlan *out,
                    const KError **error);
int recursor_layout(const RulesHost *host, RulesCtx c, RecursorScope scope,
                    Text owner, Text key, RecursorPlan *out,
                    const KError **error);
const RecursorFields *recursor_hypotheses(const RecursorFields *fs);
Int63 recursor_arity(RecursorPlan value);

/* ==================================== lib/kernel_recursor_uniform.bend */
const KError *recursor_uniform_mismatch(void);
int recursor_uniform_arguments(const RulesHost *host, RulesCtx c,
                               RecursorScope scope, const Telescope *tele,
                               const Values *values, const Values *expected,
                               RulesCtx prefix, const KError **error);
int recursor_uniform_diagram(const RulesHost *host, RulesCtx base,
                             RecursorScope scope, RulesCtx c,
                             const Telescope *params, const Values *expected,
                             const ValueFormer *view, const KError **error);
int recursor_uniform_fields(const RulesHost *host, RulesCtx base,
                            RecursorScope scope, RulesCtx c,
                            const Telescope *params, const Values *expected,
                            const RecursorFields *values,
                            const KError **error);
const Values *recursor_uniform_levels(const Telescope *tele, Int63 level);
const Values *recursor_uniform_targets(RecursorScope scope,
                                       const Telescope *tele, RulesCtx c);
int recursor_uniform_validate(const RulesHost *host, RulesCtx base,
                              RecursorScope scope, const Telescope *params,
                              RecursorPlan value, RecursorPlan *out,
                              const KError **error);
int recursor_uniform_layout(const RulesHost *host, RulesCtx c,
                            RecursorScope scope, Text owner, Text key,
                            RecursorPlan *out, const KError **error);

/* =================================== lib/kernel_recursor_instance.bend */
Bool recursor_instance_nullary(const VLegs *legs);
int recursor_instance_checked(const RulesHost *host, RulesCtx c, Int63 width,
                              const ValueSec *pair, const Values **out,
                              const KError **error);
int recursor_instance_section(const RulesHost *host, RulesCtx c, Int63 width,
                              const Value *value, const Values **out,
                              const KError **error);
int recursor_instance_environment(const RulesHost *host, RulesCtx c,
                                  const Telescope *params, const Closure *d,
                                  const Values **out, const KError **error);

/* ====================================== lib/kernel_recursor_route.bend */
Int63 recursor_route_binders(const TermLeg *lg);
Bool recursor_route_fields(const TermLeg *lg, const Ctor *ct); /* NULL = None */
Bool recursor_route_at(const Family *fam, const TermLeg *lg, MaybeText name);
Bool recursor_route_legs(const Family *fam, const TermBranches *branches);
Bool recursor_route_routes(const Family *fam, const TermBranches *branches);

/* ===================================== lib/kernel_recursor_motive.bend */
/* hypothesis = Hypothesis{position, name, domain}. */
typedef struct {
  Int63 position;
  Text name;
  const Value *domain;
} RecursorHypothesis;
SOLE_LIST(RecursorHypotheses, const RecursorHypothesis *)
SOLE_LIST_OPS(RecursorHypotheses)
/* plan<C> = Plan{layout, level, hypotheses}. */
typedef struct {
  RecursorPlan layout;
  Level level;
  const RecursorHypotheses *hypotheses;
} RecursorMotivePlan;

const TermLegs *recursor_motive_legs(const Telescope *tele);
const Closure *recursor_motive_diagram(const Telescope *params, RulesCtx c);
int recursor_motive_hypotheses(const RulesHost *host, RulesCtx origin,
                               const TermMotive *mo,
                               const RecursorFields *fs,
                               const RecursorHypotheses **out,
                               const KError **error);
int recursor_motive_checked(const RulesHost *host, RulesCtx origin,
                            const TermMotive *mo, Level lvl, RecursorPlan raw,
                            RecursorMotivePlan *out, const KError **error);
int recursor_motive_motive(const RulesHost *host, RulesCtx origin,
                           Text owner, const Family *fam,
                           const TermMotive *mo, RecursorPlan raw,
                           RecursorMotivePlan *out, const KError **error);
int recursor_motive_layout(const RulesHost *host, RulesCtx c,
                           RecursorScope scope, Text owner, Text key,
                           const TermMotive *mo, RecursorMotivePlan *out,
                           const KError **error);

/* ===================================== lib/kernel_recursor_branch.bend */
/* kind = FieldBinder{} | HypothesisBinder{child}. */
typedef enum {
  RECURSOR_FIELD_BINDER,
  RECURSOR_HYPOTHESIS_BINDER
} RecursorKindTag;
typedef struct {
  RecursorKindTag tag;
  Int63 child; /* RECURSOR_HYPOTHESIS_BINDER */
} RecursorKind;
/* binder = Binder{position, quantity, name, domain, kind}. */
typedef struct {
  Int63 position;
  Quantity quantity;
  Text name;
  const Value *domain;
  RecursorKind kind;
} RecursorBinder;
SOLE_LIST(RecursorBinders, const RecursorBinder *)
SOLE_LIST_OPS(RecursorBinders)
/* plan<C> = Plan{motive, context, binders, target}. */
typedef struct {
  RecursorMotivePlan motive;
  RulesCtx context;
  const RecursorBinders *binders;
  const Value *target;
} RecursorBranchPlan;

int recursor_branch_grade(Quantity q, Quantity *out, const KError **error);
const RecursorBinders *recursor_branch_fields(const RecursorFields *fs);
const Values *recursor_branch_values(const RecursorFields *fs);
int recursor_branch_child(Int63 position, const RecursorFields *fs,
                          Quantity *out, const KError **error);
int recursor_branch_hypotheses(const RecursorHypotheses *hs,
                               const RecursorFields *fs, Int63 count,
                               const RecursorBinders **out,
                               const KError **error);
int recursor_branch_target(const RulesHost *host, RulesCtx origin, Text owner,
                           Text key, const TermMotive *mo, const Ctor *ct,
                           RulesCtx c, const RecursorFields *fs,
                           const Value **out, const KError **error);
int recursor_branch_formation(const RulesHost *host, RulesCtx c,
                              const Value *value, const KError **error);
int recursor_branch_certify(const RulesHost *host, const RecursorBinders *bs,
                            RulesCtx c, const Value *ty, RulesCtx *out,
                            const KError **error);
int recursor_branch_finish(const RulesHost *host, RulesCtx origin,
                           RecursorMotivePlan value,
                           const RecursorBinders *bs, const Value *ty,
                           RecursorBranchPlan *out, const KError **error);
int recursor_branch_assemble(const RulesHost *host, RulesCtx origin,
                             Text owner, Text key, const TermMotive *mo,
                             const Ctor *ct, RecursorMotivePlan value,
                             RecursorBranchPlan *out, const KError **error);
int recursor_branch_layout(const RulesHost *host, RulesCtx c,
                           RecursorScope scope, Text owner, Text key,
                           const TermMotive *mo, RecursorBranchPlan *out,
                           const KError **error);

/* ======================================= lib/kernel_recursor_body.bend */
Text recursor_body_source(RecursorKind k);
int recursor_body_scope(const RulesHost *host, const TermBinders *marks,
                        const RecursorBinders *bs, RulesCtx c, RulesCtx *out,
                        const KError **error);
int recursor_body_body(const RulesHost *host, RulesCtx origin, Quantity mode,
                       const Term *tm, const Value *target, RulesCtx inner,
                       QuantityUsage *out, const KError **error);
int recursor_body_arity(Text key, Int63 got, Int63 want,
                        const KError **error);
int recursor_body_leg(const RulesHost *host, RulesCtx origin, Quantity mode,
                      Text key, RecursorBranchPlan value, const TermLeg *lg,
                      QuantityUsage *out, const KError **error);
int recursor_body_check(const RulesHost *host, RulesCtx c,
                        RecursorScope scope, Quantity mode, Text owner,
                        Text key, const TermMotive *mo, const TermLeg *lg,
                        QuantityUsage *out, const KError **error);

/* ====================================== lib/kernel_recursor_cover.bend */
int recursor_cover_inhabited(const Texts *names, const Texts **out,
                             const KError **error);
int recursor_cover_walk(const RulesHost *host, const Texts *names, RulesCtx c,
                        RecursorScope scope, Quantity mode, Text owner,
                        const TermMotive *mo, const TermBranches *branches,
                        QuantityUsage *out, const KError **error);
int recursor_cover_check(const RulesHost *host, RulesCtx c,
                         RecursorScope scope, Quantity mode, Text owner,
                         const TermMotive *mo, const TermBranches *branches,
                         QuantityUsage *out, const KError **error);

/* ======================================= lib/kernel_recursor_elim.bend */
/* view = F.Pair2<String, F.Pair2<List<V.t>, V.closure>> (line 15). */
typedef struct {
  Text name;
  const Values *indices;
  const Closure *diagram;
} RecursorElimView;

/* pair = F.Pair2<String, List<V.t>>: name and ixv. */
RecursorElimView recursor_elim_closed(const Closure *d, Text name,
                                      const Values *ixv);
int recursor_elim_former(const RulesHost *host, RulesCtx c, const Value *ty,
                         RecursorElimView *out, const KError **error);
int recursor_elim_named(Text owner, Text n, const KError **error);
int recursor_elim_values(const RulesHost *host, RulesCtx c,
                         const Telescope *params, const Closure *d,
                         const Values **out, const KError **error);
int recursor_elim_result(const RulesHost *host, RulesCtx c, RulesCtx origin,
                         Quantity mode, const TermMotive *mo,
                         const Term *scrut, const Values *ixv,
                         QuantityUsage su, QuantityUsage bu,
                         RulesInferred *out, const KError **error);
int recursor_elim_scoped(const RulesHost *host, RulesCtx c, Quantity mode,
                         Text owner, const Term *scrut, const TermMotive *mo,
                         const TermBranches *branches, const Family *fam,
                         QuantityUsage su, const Values *ixv,
                         RecursorScope scope, RulesInferred *out,
                         const KError **error);
int recursor_elim_viewed(const RulesHost *host, RulesCtx c, Quantity mode,
                         Text owner, const Term *scrut, const TermMotive *mo,
                         const TermBranches *branches, const Family *fam,
                         QuantityUsage su, RecursorElimView v,
                         RulesInferred *out, const KError **error);
int recursor_elim_typed(const RulesHost *host, RulesCtx c, Quantity mode,
                        Text owner, const Term *scrut, const TermMotive *mo,
                        const TermBranches *branches, const Family *fam,
                        RulesInferred inf, RulesInferred *out,
                        const KError **error);
int recursor_elim_check(const RulesHost *host, RulesCtx c, Quantity mode,
                        Text owner, Quantity sq, const Term *scrut,
                        const TermMotive *mo, const TermBranches *branches,
                        RulesInferred *out, const KError **error);

/* lib/kernel_check.c, Bend lines 1-435 (S3 session 3). Programs take the
   host first and a RulesCtx (ctx.check) for each context argument, then
   the Bend arguments in Bend order, then out, then error. A program<ctx,
   Unit> has no out. Pure defs take const CheckCtx *. A Maybe of a pointer
   is NULL for None{}. The records runtime_ops and ops have no C twin (the
   runtime_ops_* and check_ops_* functions above are their fields).
   fuelled, with_fuel, polled, result_of, drive and rules have no C twin:
   the fuel is one Budget cell (host->fuel). */
/* local = Local{name, quantity, ty} */
typedef struct {
  Text name;
  Quantity quantity;
  const Value *ty;
} CheckLocal;
SOLE_LIST(CheckLocals, CheckLocal)
/* ctx = Context{globals, env, locals, size, budget} */
struct CheckCtx {
  const Globals *globals;
  const Values *env;
  const CheckLocals *locals;
  Int63 size;
  Budget budget;
};
/* The let_bound body closure (ctx -> program<ctx, inferred>), closed:
   CHECK_BODY_INFER = inner => infer_request(inner, mode, body) (line 365);
   CHECK_BODY_CHECK = inner => pmap(uses => (expected, uses),
   check_request(inner, mode, body, expected)) (line 380). */
typedef enum { CHECK_BODY_INFER, CHECK_BODY_CHECK } CheckBodyKind;
typedef struct {
  CheckBodyKind kind;
  Quantity mode;
  const Term *body;
  const Value *expected; /* CHECK_BODY_CHECK only */
} CheckBody;
/* kind = Definition{} | Postulate{} */
typedef enum { CHECK_DEFINITION, CHECK_POSTULATE } CheckKind;
/* decl = Declaration{name, kind, ty, body} */
typedef struct {
  Text name;
  CheckKind kind;
  const Term *ty;
  const Term *body; /* NULL = None{} */
} CheckDecl;
SOLE_LIST(CheckDecls, CheckDecl)
/* F.Pair2<String, G.entry> */
typedef struct {
  Text name;
  const GlobalEntry *entry;
} CheckNamedEntry;
SOLE_LIST(CheckNamedEntries, CheckNamedEntry)
/* family_decl = FamilyDecl{name, params, indices, level} */
typedef struct {
  Text name;
  const Telescope *params;
  const Telescope *indices;
  Level level;
} CheckFamilyDecl;
/* ctor_decl = CtorDecl{name, args, params, indices} */
typedef struct {
  Text name;
  const Telescope *args;
  const Terms *params;
  const Terms *indices;
} CheckCtorDecl;
SOLE_LIST(CheckCtorDecls, CheckCtorDecl)

const CheckCtx *check_make(const Globals *globals, Budget budget);
const Globals *check_globals(const CheckCtx *c);
const CheckLocals *check_locals(const CheckCtx *c);
Budget check_budget(const CheckCtx *c);
const Texts *check_local_names(const CheckLocals *ls);
Text check_pp_result(const Texts *names, const Term *value); /* NULL = Fail */
const Value *check_local_ty(const CheckLocal *value);
Bool check_readable(Quantity mode, Quantity q);
int check_close_loop(const CheckLocals *ls, Int63 level, Int63 lower,
                     Quantity mode, Bool affine, QuantityUsage uses,
                     QuantityUsage *out, const KError **error);
int check_close(Bool affine, const CheckCtx *c, Int63 lower, Quantity mode,
                QuantityUsage uses, QuantityUsage *out, const KError **error);
int check_infer_request(const RulesHost *host, RulesCtx c, Quantity q,
                        const Term *tm, RulesInferred *out,
                        const KError **error);
int check_check_request(const RulesHost *host, RulesCtx c, Quantity q,
                        const Term *tm, const Value *ty, QuantityUsage *out,
                        const KError **error);
int check_univ_request(const RulesHost *host, RulesCtx c, const Term *tm,
                       Level *out, const KError **error);
int check_ensure(const RulesHost *host, RulesCtx c, const Value *got,
                 const Value *expected, const KError **error);
int check_infer_local(const RulesHost *host, RulesCtx c, Quantity mode,
                      Int63 ix, const CheckLocal *entry, RulesInferred *out,
                      const KError **error);
const KError *check_no_infer(Text what);
int check_infer_literal(const RulesHost *host, RulesCtx c,
                        const Literal *lit, RulesInferred *out,
                        const KError **error);
Bool check_linear_definition(const CheckLocals *ls, Int63 level,
                             QuantityUsage uses);
int check_let_finish(const RulesHost *host, RulesCtx outer, RulesCtx inner,
                     Quantity mode, Bool linear, QuantityUsage def_uses,
                     RulesInferred answer, RulesInferred *out,
                     const KError **error);
int check_let_bound(const RulesHost *host, RulesCtx outer, Quantity mode,
                    RulesCtx inner, QuantityUsage uses, Bool linear,
                    const CheckBody *body, RulesInferred *out,
                    const KError **error);
int check_inferred_universe(const RulesHost *host, RulesCtx c,
                            RulesInferred value, Level *out,
                            const KError **error);
int check_check_former(const RulesHost *host, RulesCtx c, const TermShape *s,
                       const Term *d, const Value *expected, Bool right,
                       QuantityUsage *out, const KError **error);
int check_inferred_check(const RulesHost *host, RulesCtx c,
                         const Value *expected, RulesInferred value,
                         QuantityUsage *out, const KError **error);
QuantityUsage check_inferred_uses(RulesInferred value);
int check_infer_then_check(const RulesHost *host, RulesCtx c, Quantity mode,
                           const Term *tm, const Value *expected,
                           QuantityUsage *out, const KError **error);
RulesAnswer check_inferred_answer(RulesInferred value);
/* poll (line 166): the drive Request arm (check_request) calls it. */
int check_poll(const RulesTask *task, Budget budget, Budget *out,
               const KError **error);
const Value *check_inferred_type(RulesInferred value);
Text check_declaration_name(const CheckDecl *d);
int check_declaration_body(const RulesHost *host, RulesCtx c, Text n,
                           CheckKind kind, const Term *ty, const Term *body,
                           const Value *tyv, const GlobalEntry **out,
                           const KError **error);
int check_check_decl_program(const RulesHost *host, RulesCtx c,
                             const CheckDecl *d, const GlobalEntry **out,
                             const KError **error);
int check_declarations_program(const RulesHost *host, const CheckDecls *ds,
                               const Globals *g, Budget b,
                               const CheckNamedEntries **out,
                               const KError **error);
int check_check_tele_loop(const RulesHost *host, const Telescope *tele,
                          RulesCtx c, RulesCtx *out, const KError **error);
int check_check_telescope(const RulesHost *host, RulesCtx c,
                          const Telescope *tele, RulesCtx *out,
                          const KError **error);
int check_index_rules(const RulesHost *host, RulesCtx c, Text name,
                      Level level, TeleEntry entry, const KError **error);
int check_check_index_loop(const RulesHost *host, const Telescope *tele,
                           RulesCtx c, Text name, Level level, RulesCtx *out,
                           const KError **error);
int check_check_index_telescope(const RulesHost *host, RulesCtx c, Text name,
                                Level level, const Telescope *tele,
                                RulesCtx *out, const KError **error);
Bool check_absent_family(const Family *family);
int check_declare_family_program(const RulesHost *host, const Globals *g,
                                 Budget b, const CheckFamilyDecl *d,
                                 const Globals **out, const KError **error);
Bool check_parameter_at(Int63 index, const Term *tm);
Bool check_parameters_at(const Terms *params, Int63 index);
int check_check_field_loop(const RulesHost *host, const Telescope *tele,
                           RulesCtx c, Text name, Level level, RulesCtx *out,
                           const KError **error);
int check_check_ctor(const RulesHost *host, RulesCtx c, const Family *fam,
                     const Texts *group, const CheckCtorDecl *cd,
                     const Ctor **out, const KError **error);
int check_ctors_program(const RulesHost *host, const CheckCtorDecls *cds,
                        RulesCtx c, const Family *fam, const Texts *group,
                        const Ctors **out, const KError **error);
Bool check_provisional(const Family *f);
const Texts *check_ctor_names(const Ctors *ctors);
const Family *check_complete_family(const Family *f, const Ctors *ctors);
int check_define_ctors_program(const RulesHost *host, const Globals *g,
                               Budget b, const Texts *group, Text name,
                               const CheckCtorDecls *cds, const Globals **out,
                               const KError **error);
int check_let_continue(const RulesHost *host, RulesCtx c, Quantity mode,
                       Text x, const Value *ty, const Value *value,
                       QuantityUsage uses, Bool linear, const CheckBody *body,
                       RulesInferred *out, const KError **error);
int check_let_body(const RulesHost *host, RulesCtx c, Quantity mode, Text x,
                   const Term *ty, const Term *definition,
                   const CheckBody *body, RulesInferred *out,
                   const KError **error);
MaybeText check_routed(const CheckCtx *c, const TermShape *s,
                       const TermBranches *bs);
int check_recursor(const RulesHost *host, RulesCtx c, Quantity mode,
                   MaybeText owner, const TermShape *s, const Term *scrut,
                   Quantity sq, const TermMotive *mo, const TermBranches *bs,
                   const Value *expected, RulesInferred *out,
                   const KError **error);
int check_route(const RulesHost *host, RulesCtx c, Quantity mode,
                const TermElim *e, const Value *expected, RulesInferred *out,
                const KError **error);
int check_infer_node(const RulesHost *host, RulesCtx c, Quantity mode,
                     const Term *tm, RulesInferred *out,
                     const KError **error);
int check_check_node(const RulesHost *host, RulesCtx c, Quantity mode,
                     const Term *tm, const Value *expected,
                     QuantityUsage *out, const KError **error);
int check_dispatch(const RulesHost *host, const RulesTask *task,
                   RulesAnswer *out, const KError **error);
/* Entry points: the fuel is one local Budget cell; *remaining is set on
   both returns (Fuelled.remaining). */
int check_infer_with_budget(const CheckCtx *c, Quantity mode, const Term *tm,
                            const Value **out, Budget *remaining,
                            const KError **error);
int check_check_with_budget(const CheckCtx *c, Quantity mode, const Term *tm,
                            const Value *expected, Budget *remaining,
                            const KError **error);
int check_infer(const CheckCtx *c, Quantity mode, const Term *tm,
                const Value **out, const KError **error);
int check_check(const CheckCtx *c, Quantity mode, const Term *tm,
                const Value *expected, const KError **error);
int check_infer_univ(const CheckCtx *c, const Term *tm, Level *out,
                     const KError **error);
int check_conv_type(const CheckCtx *c, const Value *a, const Value *b,
                    Bool *out, const KError **error);
int check_infer_term(const Globals *globals, Budget budget, const Term *tm,
                     const Value **out, const KError **error);
int check_check_term(const Globals *globals, Budget budget, const Term *tm,
                     const Value *ty, const KError **error);
int check_check_decl(const Globals *g, Budget b, const CheckDecl *d,
                     const GlobalEntry **out, const KError **error);
int check_check_decls(const Globals *g, Budget b, const CheckDecls *ds,
                      const CheckNamedEntries **out, const KError **error);
int check_declare_family(const Globals *g, Budget b, const CheckFamilyDecl *d,
                         const Globals **out, const KError **error);
int check_define_ctors(const Globals *g, Budget b, const Texts *group,
                       Text name, const CheckCtorDecls *cds,
                       const Globals **out, const KError **error);


/* ================================================== erase/eterm.bend */

/* S4 twins sit in lib/ as lib/erase_<module>.c (erase/eterm.bend ->
   lib/erase_eterm.c). Prefixes: eterm_ for erase/eterm; erase_<module>_ for
   the other erase files (lib/runtime.c and the runtime_ops_* functions
   already use the name `runtime`). */

/* Bend tid and fid: symbolic names that stay distinct types (by value). */
typedef struct {
  Text name;
} Tid;
typedef struct {
  Text name;
} Fid;
SOLE_LIST(Tids, Tid)
SOLE_LIST_OPS(Tids)

/* Bend repr. RI31 does not use name. */
typedef enum {
  REPR_RI31,
  REPR_RSTRUCT,
  REPR_RUNION,
  REPR_RFUNC,
  REPR_RTHUNK
} ReprTag;
typedef struct Repr Repr;
struct Repr {
  ReprTag tag;
  Tid name;
};
SOLE_LIST(Reprs, const Repr *)
SOLE_LIST_OPS(Reprs)

/* Bend ktm, in the Bend constructor order. */
typedef enum {
  KTM_KVAR,
  KTM_KLIT,
  KTM_KGLOBAL,
  KTM_KERASED,
  KTM_KLET,
  KTM_KCLOS,
  KTM_KAPP,
  KTM_KTAIL,
  KTM_KSTRUCT,
  KTM_KPROJ,
  KTM_KTAG,
  KTM_KCASE,
  KTM_KDELAY,
  KTM_KFORCE
} KTmTag;
typedef struct KTm KTm;
SOLE_LIST(KTms, const KTm *)
SOLE_LIST_OPS(KTms)

/* Bend kbranch = BranchData<ktm> (the only instance of BranchData). */
typedef struct {
  Int63 tag;
  Int63 arity;
  const KTm *body;
} KBranch;
SOLE_LIST(KBranches, const KBranch *)
SOLE_LIST_OPS(KBranches)

struct KTm {
  KTmTag tag;
  union {
    struct {
      Int63 index;
    } kvar;
    struct {
      const Literal *literal;
    } klit;
    struct {
      Text name;
    } kglobal;
    struct {
      Text name;
      const KTm *value;
      const KTm *body;
    } klet;
    struct {
      Fid name;
      Int63 arity;
      const KTms *captures;
    } kclos;
    struct {
      const KTm *head;
      const KTms *args;
    } kapp;
    struct {
      const KTm *head;
      const KTms *args;
    } ktail;
    struct {
      Tid name;
      const KTms *fields;
    } kstruct;
    struct {
      Tid name;
      Int63 field;
      const KTm *value;
    } kproj;
    struct {
      Tid name;
      Int63 tag;
      const KTms *fields;
    } ktag;
    struct {
      Tid name;
      const KTm *scrutinee;
      const KBranches *branches;
    } kcase;
    struct {
      Fid name;
      const KTms *captures;
    } kdelay;
    struct {
      const KTm *value;
    } kforce;
  } as;
};

/* Bend kdecl. */
typedef enum { KDECL_KFUN, KDECL_KREC } KDeclTag;
typedef struct KDecl KDecl;
struct KDecl {
  KDeclTag tag;
  union {
    struct {
      Fid name;
      const Reprs *params;
      const Repr *result;
      const KTm *body;
    } kfun;
    struct {
      const Tids *names;
    } krec;
  } as;
};

const Repr *eterm_repr(ReprTag tag, Tid name);
const KBranch *eterm_branch(Int63 tag, Int63 arity, const KTm *body);
const KTm *eterm_kvar(Int63 index);
const KTm *eterm_klit(const Literal *literal);
const KTm *eterm_kglobal(Text name);
const KTm *eterm_kerased(void);
const KTm *eterm_klet(Text name, const KTm *value, const KTm *body);
const KTm *eterm_kclos(Fid name, Int63 arity, const KTms *captures);
const KTm *eterm_kapp(const KTm *head, const KTms *args);
const KTm *eterm_ktail(const KTm *head, const KTms *args);
const KTm *eterm_kstruct(Tid name, const KTms *fields);
const KTm *eterm_kproj(Tid name, Int63 field, const KTm *value);
const KTm *eterm_ktag(Tid name, Int63 tag, const KTms *fields);
const KTm *eterm_kcase(Tid name, const KTm *scrutinee,
                       const KBranches *branches);
const KTm *eterm_kdelay(Fid name, const KTms *captures);
const KTm *eterm_kforce(const KTm *value);
const KDecl *eterm_kfun(Fid name, const Reprs *params, const Repr *result,
                        const KTm *body);
const KDecl *eterm_krec(const Tids *names);
Text eterm_tid_text(Tid t);
Text eterm_fid_text(Fid f);
Text eterm_escape(Text s);
Text eterm_literal_text(const Literal *lit);
Text eterm_join(Text sep, const Texts *xs);
Text eterm_print_repr(const Repr *r);
const Texts *eterm_repr_texts(const Reprs *rs);
Text eterm_print_ktm(const KTm *t);
Text eterm_ktm_list(const KTms *ts);
Text eterm_print_branch(const KBranch *b);
Text eterm_print_decl(const KDecl *d);

/* ================================================ erase/runtime.bend */

/* Bend RT.result<A> (Done{v} | InvalidStack{}) is an int return: 1 = Done
   with *out, 0 = InvalidStack (no error value). */
SOLE_LIST(Int63s, Int63)
SOLE_LIST_OPS(Int63s)

/* Bend RT.scoped = F.Pair2<F.Int63.t, K.ktm>. */
typedef struct {
  Int63 depth;
  const KTm *term;
} ScopedTerm;
SOLE_LIST(ScopedTerms, ScopedTerm)
SOLE_LIST_OPS(ScopedTerms)
static inline ScopedTerm scoped_term(Int63 depth, const KTm *term) {
  ScopedTerm item;
  item.depth = depth;
  item.term = term;
  return item;
}

/* Bend RT.pruned. */
typedef struct {
  const Reprs *captures;
  const KTms *args;
  const KTm *body;
} Pruned;

Bool erase_runtime_lt(Int63 a, Int63 b);
Bool erase_runtime_ge(Int63 a, Int63 b);
const ScopedTerms *erase_runtime_children(Int63 depth, const KTm *t);
Bool erase_runtime_member(Int63 x, const Int63s *xs);
const Int63s *erase_runtime_insert(Int63 x, const Int63s *xs);
int erase_runtime_shift_runtime(Int63 by, Int63 depth, const KTm *t,
                                const KTm **out);
int erase_runtime_prune_captures(const Reprs *ps, const KTms *args,
                                 Int63 params, const KTm *body, Pruned *out);

/* =========================================== erase/dependencies.bend */

const Tids *erase_dependencies_term(const KTm *tm);


/* ================================================== erase/type.bend */

/* C = K.ctx: each context argument is a const CheckCtx *. A Maybe<arrow>
   answer is an int return: 1 = Some with *out. A ~loop argument has one
   instance in each family (repr_visit, arity, signature_of): a direct call. */

/* Bend arrow = Arrow{quantity, name, domain, diagram} (line 39). */
typedef struct {
  Quantity quantity;
  Text name;
  const Value *domain;
  const Closure *diagram;
} EraseArrow;

/* Bend signature = Signature{params, result} (line 52). */
typedef struct {
  const Reprs *params;
  const Repr *result;
} EraseSignature;

Tid erase_type_mu_tid(Text name);
int erase_type_opened(const CheckCtx *c, const Closure *diagram,
                      const Values *args, const Value **out,
                      const KError **error);
int erase_type_under(const CheckCtx *c, Quantity q, Text x, const Value *dom,
                     const Closure *d, const CheckCtx **inner,
                     const Value **out, const KError **error);
int erase_type_former_arrow(const ValueFormer *former, EraseArrow *out);
int erase_type_arrow_of(const Value *value, EraseArrow *out);
int erase_type_runtime_ty(const CheckCtx *c, const Value *ty, Bool *out,
                          const KError **error);
int erase_type_point_runtime(const CheckCtx *c, Quantity q, const Value *ty,
                             Bool *out, const KError **error);
int erase_type_leg_types(const CheckCtx *c, Int63 n, const Closure *diagram,
                         const Values **out, const KError **error);
int erase_type_arity(const CheckCtx *c, const Value *ty, Int63 n, Budget b,
                     Int63 *out, const KError **error);
int erase_type_repr_of(const CheckCtx *c, const Value *ty, const Repr **out,
                       const KError **error);
int erase_type_tid_of(const CheckCtx *c, const Value *ty, Tid *out,
                      const KError **error);
int erase_type_parameter(const CheckCtx *c, Quantity q, const Value *dom,
                         const Reprs **out, const KError **error);
int erase_type_signature_of(const CheckCtx *c, const Value *ty, Budget b,
                            EraseSignature *out, const KError **error);
/* ================================================ erase/family.bend */
/* List<&2, Maybe<&2, K.repr>> (a layout): a NULL head is None{}. */
SOLE_LIST(MaybeReprs, const Repr *)
SOLE_LIST_OPS(MaybeReprs)
const KError *erase_family_not_ctor(Text key, Text name);
int erase_family_layout(const CheckCtx *c, const Family *fam,
                        const Ctor *ctor, const MaybeReprs **out,
                        const KError **error);
int erase_family_group_tids(const CheckCtx *c, Text name, const Family *fam,
                            const Tids **out, const KError **error);

/* erase/pair.bend (lib/erase_pair.c, S4 session 2b): dependent-pair plans.
   Field = Pair.Field (mu uses the list for its tag fields). */
typedef struct {
  const Value *ty;
  const Term *term;
} ErasePairField;
SOLE_LIST(ErasePairFields, ErasePairField)
SOLE_LIST_OPS(ErasePairFields)
typedef struct {
  const Value *scrut_type;
  Tid tid;
  Quantity quantity;
  Text first_name;
  const Value *domain;
  Bool keep_first;
  Text second_name;
  const Value *fibre;
  Bool keep_second;
  const Value *target;
  const Term *body;
} ErasePairBranch;
int erase_pair_introduction(const CheckCtx *c, const Value *ty,
                            const TermAddr *address, const Terms *args,
                            const ErasePairFields **out, const KError **error);
int erase_pair_elimination(const CheckCtx *c, const Value *ty,
                           const TermShape *shape, const Term *scrut,
                           const TermMotive *motive,
                           const TermBranches *branches, ErasePairBranch *out,
                           const KError **error);

/* erase/sum.bend (lib/erase_sum.c, S4 session 2b): finite sum plans.
   Tag.payload: NULL = None{}. */
typedef struct {
  const Value *ty;
  const Term *body;
} EraseSumPayload;
typedef struct {
  Tid tid;
  Int63 index;
  const EraseSumPayload *payload;
} EraseSumTag;
typedef struct {
  const Value *ty;
  Tid tid;
  const Values *legs;
} EraseSumCase;
typedef struct {
  Quantity quantity;
  Text name;
  const Value *domain;
  Bool keep;
  const Value *target;
  const Term *body;
} EraseSumBranch;
int erase_sum_injection(const CheckCtx *c, const Value *ty,
                        const TermAddr *address, const Terms *args,
                        EraseSumTag *out, const KError **error);
int erase_sum_case_of(const CheckCtx *c, const Term *scrut, EraseSumCase *out,
                      const KError **error);
int erase_sum_branch_of(const CheckCtx *c, const Value *ty,
                        const TermMotive *motive, const Values *tys,
                        Int63 index, const TermBranches *branches,
                        EraseSumBranch *out, const KError **error);

/* erase/mu.bend (lib/erase_mu.c, S4 session 2b): recursive-family plans.
   Tag.fields uses the Pair.field list. */
typedef struct {
  Tid tid;
  Int63 index;
  const ErasePairFields *fields;
  const Tids *group;
} EraseMuTag;
typedef struct {
  const Value *sty;
  Text name;
  const Family *family;
  const TermMotive *motive;
  const Values *env;
  const Texts *names;
  const Tids *group;
} EraseMuCase;
typedef struct {
  Quantity quantity;
  Text name;
  const Value *domain;
  Bool keep;
} EraseMuBinder;
SOLE_LIST(EraseMuBinders, EraseMuBinder)
SOLE_LIST_OPS(EraseMuBinders)
typedef struct {
  const EraseMuBinders *binders;
  Int63 arity;
  const Value *target;
  const Term *body;
} EraseMuBranch;
int erase_mu_injection(const CheckCtx *c, const Value *ty,
                       const TermAddr *address, const Terms *args,
                       EraseMuTag *out, const KError **error);
int erase_mu_case_of(const CheckCtx *c, const Term *scrut,
                     const TermMotive *motive, EraseMuCase *out,
                     const KError **error);
int erase_mu_branch_of(const CheckCtx *c, Text n, const Family *fam,
                       const TermMotive *mo, const Values *env, Text key,
                       const TermBranches *branches, EraseMuBranch *out,
                       const KError **error);
/* ================================================== erase/core.bend */
/* erase/core.bend (lib/erase_core.c, S4 session 3): ordinary-definition
   erasure. The Bend computation (Pure, Abort, Request) is the int return:
   1 = Pure with *out, 0 = Abort with *error. A Request (TermTask, ChainTask,
   JobsTask) is a direct call of the step function (C recursion; the depth is
   the nesting of the term). drive and dispatch have no C twin. ctx, state,
   answer and chain_result are structs by value. */
SOLE_LIST(KDecls, const KDecl *)
SOLE_LIST_OPS(KDecls)

typedef enum {
  ERASE_CORE_SLOT_DROP,
  ERASE_CORE_SLOT_KEEP,
  ERASE_CORE_SLOT_EXTRA
} EraseCoreSlot;
SOLE_LIST(EraseCoreSlots, EraseCoreSlot)
SOLE_LIST_OPS(EraseCoreSlots)

typedef struct {
  Nat next;
  const KDecls *lifted;
  const Tids *groups;
  Budget fuel;
} EraseCoreState;

typedef enum {
  ERASE_CORE_DROPPED,
  ERASE_CORE_POSTULATE,
  ERASE_CORE_CODE
} EraseCoreEntryTag;

/* entry: repr is set for Postulate, decls for Code (else NULL). */
typedef struct {
  EraseCoreEntryTag tag;
  const Repr *repr;
  const KDecls *decls;
} EraseCoreEntry;

/* rows: F.Pair2<String, entry>. */
typedef struct {
  Text name;
  EraseCoreEntry entry;
} EraseCoreRow;
SOLE_LIST(EraseCoreRows, EraseCoreRow)
SOLE_LIST_OPS(EraseCoreRows)

typedef struct {
  const CheckCtx *check;
  const EraseCoreSlots *slots;
  Text self;
} EraseCoreCtx;

/* answer: F.Pair2<K.ktm, state>. */
typedef struct {
  const KTm *term;
  EraseCoreState state;
} EraseCoreAnswer;

/* chain_result: Chain. */
typedef struct {
  const Reprs *params;
  const Repr *result;
  const KTm *body;
  EraseCoreState state;
} EraseCoreChain;

/* F.Pair2<F.Int63.t, T.t>: one task of the free-variable walk. */
typedef struct {
  Int63 depth;
  const Term *term;
} EraseCoreTodo;
SOLE_LIST(EraseCoreTodos, EraseCoreTodo)
SOLE_LIST_OPS(EraseCoreTodos)
static inline EraseCoreTodo erase_core_todo(Int63 depth, const Term *term) {
  EraseCoreTodo item;
  item.depth = depth;
  item.term = term;
  return item;
}

/* job: Job{ty, term}. */
typedef struct {
  const Value *ty;
  const Term *term;
} EraseCoreJob;
SOLE_LIST(EraseCoreJobs, EraseCoreJob)
SOLE_LIST_OPS(EraseCoreJobs)

Text erase_core_print(const EraseCoreRows *rows);
int erase_core_term(EraseCoreCtx c, EraseCoreState s, Bool tail,
                    const Value *expected, const Term *tm,
                    EraseCoreAnswer *out, const KError **error);
int erase_core_chain(EraseCoreCtx c, EraseCoreState s, const Reprs *params,
                     const Value *ty, const Term *tm, EraseCoreChain *out,
                     const KError **error);
/* declaration (line 695) and program (line 697): G.t = Globals, G.entry =
   GlobalEntry, the entries = CheckNamedEntries (F.Pair2<String, G.entry>). */
int erase_core_declaration(const Globals *g, Budget b, Text name,
                           const GlobalEntry *en, EraseCoreEntry *out,
                           const KError **error);
int erase_core_program(const Globals *g, Budget b,
                       const CheckNamedEntries *entries,
                       const EraseCoreRows **out, const KError **error);
/* ============================================== surface/token.bend */

/* T.loc = Loc{line, col}: by value. */
typedef struct {
  Nat line;
  Nat col;
} SurfaceLoc;

/* T.kind, in the Bend constructor order. */
typedef enum {
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_LBRACE,
  TOKEN_RBRACE,
  TOKEN_SEMICOLON,
  TOKEN_KELIM,
  TOKEN_KELSE,
  TOKEN_KRECORD,
  TOKEN_COLON,
  TOKEN_COLON_EQ,
  TOKEN_ARROW,
  TOKEN_DARROW,
  TOKEN_STAR,
  TOKEN_COMMA,
  TOKEN_DOT,
  TOKEN_DOT1,
  TOKEN_DOT2,
  TOKEN_PIPE,
  TOKEN_UNIT,
  TOKEN_KDEF,
  TOKEN_KAXIOM,
  TOKEN_KFUN,
  TOKEN_KINJ,
  TOKEN_KOF,
  TOKEN_KCASE,
  TOKEN_KMATCH,
  TOKEN_KAS,
  TOKEN_KRETURN,
  TOKEN_KWITH,
  TOKEN_KTUPLE,
  TOKEN_KSUM,
  TOKEN_KPROD,
  TOKEN_KABSURD,
  TOKEN_KPROP,
  TOKEN_KTYPE,
  TOKEN_KLET,
  TOKEN_KIN,
  TOKEN_KAUTO,
  TOKEN_KMU,
  TOKEN_KMUTUAL,
  TOKEN_KEND,
  TOKEN_KNU,
  TOKEN_KAND,
  TOKEN_KREC,
  TOKEN_KNAT_ADD,
  TOKEN_KNAT_SUB,
  TOKEN_KNAT_MUL,
  TOKEN_KNAT_EQ,
  TOKEN_KNAT_LT,
  TOKEN_IDENT,
  TOKEN_NUMBER,
  TOKEN_BYTES,
  TOKEN_EOF
} SurfaceTokenTag;

/* T.kind by value. ident = Ident.value, number = Number.value, bytes =
   Bytes.value (the Bend List<U32> of bytes as a Text slice). */
typedef struct {
  SurfaceTokenTag tag;
  union {
    Text ident;
    Bignum number;
    Text bytes;
  } as;
} SurfaceKind;

/* T.t = Token{kind, loc}: by value. */
typedef struct {
  SurfaceKind kind;
  SurfaceLoc loc;
} SurfaceToken;

SOLE_LIST(SurfaceTokens, SurfaceToken)
SOLE_LIST_OPS(SurfaceTokens)

SurfaceKind surface_token_kind(SurfaceTokenTag tag); /* a nullary kind */
SurfaceKind surface_token_ident(Text value);
SurfaceKind surface_token_number(Bignum value);
SurfaceKind surface_token_bytes(Text value);
SurfaceToken surface_token(SurfaceKind kind, SurfaceLoc loc);
SurfaceLoc surface_token_start(void);
SurfaceLoc surface_token_advance(SurfaceLoc position, Nat n);
SurfaceLoc surface_token_next_line(SurfaceLoc position);
Text surface_token_describe(SurfaceKind kind);
SurfaceKind surface_token_ident_kind(Text s);

/* ============================================== surface/lexer.bend */

/* L.result(A) = Result<E.t, A>: an int return (1 = Done with *out, 0 = Fail
   with *error). error (line 13) is the E.Parse value at the position. The
   source bytes of lex_bytes (Bend List<U32>) are a Text slice. */
const KError *surface_lexer_error(SurfaceLoc position, Text message);
int surface_lexer_hex_digit(U32 c, U32 *out); /* 1 = Some with *out */
int surface_lexer_lex_bytes(Text source, const SurfaceTokens **out,
                            const KError **error);
int surface_lexer_lex(Text source, const SurfaceTokens **out,
                      const KError **error);

/* ============================================= surface/source.bend */

/* tokens (line 7): 1 = Done with *out, 0 = Fail with *error. */
int surface_source_tokens(const SurfaceTokens *ts, const SurfaceTokens **out,
                          const KError **error);

/* ============================================= surface/syntax.bend */

/* The Bend data types binder_of<A> to rec_def_of<A> are used only at
   A = S.t (syntax and program.bend), thus each one is one C type. */

/* S.prim, in the Bend constructor order. */
typedef enum {
  SURFACE_PRIM_ADD,
  SURFACE_PRIM_SUB,
  SURFACE_PRIM_MUL,
  SURFACE_PRIM_EQ,
  SURFACE_PRIM_LT
} SurfacePrim;

/* S.t: a pointer to an arena node. */
typedef struct SurfaceTerm SurfaceTerm;
SOLE_LIST(SurfaceTerms, const SurfaceTerm *)
SOLE_LIST_OPS(SurfaceTerms)

/* S.binder = Binder{q, name, ty}: by value. */
typedef struct {
  Quantity q;
  Text name;
  const SurfaceTerm *ty;
} SurfaceBinder;
SOLE_LIST(SurfaceBinders, SurfaceBinder)
SOLE_LIST_OPS(SurfaceBinders)

/* S.motive = Motive{self, ind, idx, body}. A Maybe<S.motive> is a pointer
   (NULL = None). */
typedef struct {
  Text self;
  MaybeText ind;
  const Texts *idx;
  const SurfaceTerm *body;
} SurfaceMotive;

/* S.field = Field{q, name, ty}: by value; ty NULL = None. */
typedef struct {
  Quantity q;
  Text name;
  const SurfaceTerm *ty;
} SurfaceField;
SOLE_LIST(SurfaceFields, SurfaceField)
SOLE_LIST_OPS(SurfaceFields)

/* S.branch, by value; other = BrDefault. */
typedef enum {
  SURFACE_BR_LEG,
  SURFACE_BR_CTOR,
  SURFACE_BR_DEFAULT
} SurfaceBranchTag;
typedef struct {
  SurfaceBranchTag tag;
  union {
    struct { Int63 key; const SurfaceBinders *binders; const SurfaceTerm *body; } leg;
    struct { Text name; const SurfaceFields *fields; const SurfaceTerm *body; } ctor;
    struct { const SurfaceTerm *body; } other;
  } as;
} SurfaceBranch;
SOLE_LIST(SurfaceBranches, SurfaceBranch)
SOLE_LIST_OPS(SurfaceBranches)

/* S.fam_ctor = FamCtor{name, ty}: by value. */
typedef struct {
  Text name;
  const SurfaceTerm *ty;
} SurfaceFamCtor;
SOLE_LIST(SurfaceFamCtors, SurfaceFamCtor)
SOLE_LIST_OPS(SurfaceFamCtors)

/* S.fam = Fam{name, params, ty, ctors}: by value. */
typedef struct {
  Text name;
  const SurfaceBinders *params;
  const SurfaceTerm *ty;
  const SurfaceFamCtors *ctors;
} SurfaceFam;
SOLE_LIST(SurfaceFams, SurfaceFam)
SOLE_LIST_OPS(SurfaceFams)

/* S.rec_def = RecDef{name, ty, body}: by value. */
typedef struct {
  Text name;
  const SurfaceTerm *ty;
  const SurfaceTerm *body;
} SurfaceRecDef;
SOLE_LIST(SurfaceRecDefs, SurfaceRecDef)
SOLE_LIST_OPS(SurfaceRecDefs)

/* S.t, in the Bend constructor order. SCase and SMatch share elim. */
typedef enum {
  SURFACE_SVAR,
  SURFACE_SNAT,
  SURFACE_SPROP,
  SURFACE_STYPE,
  SURFACE_SPRIM,
  SURFACE_SUNIT,
  SURFACE_SAUTO,
  SURFACE_SPAIR,
  SURFACE_STUPLE,
  SURFACE_SSUM,
  SURFACE_SPROD,
  SURFACE_SPROJ,
  SURFACE_SINJ,
  SURFACE_SABSURD,
  SURFACE_SAPP,
  SURFACE_SFUN,
  SURFACE_SARROW,
  SURFACE_SSTAR,
  SURFACE_SLET,
  SURFACE_SANN,
  SURFACE_SCASE,
  SURFACE_SMATCH
} SurfaceTermTag;
struct SurfaceTerm {
  SurfaceTermTag tag;
  union {
    struct { Text name; } var;
    struct { Bignum value; } nat;
    struct { Int63 level; } type;
    struct { SurfacePrim prim; } prim;
    struct { const SurfaceTerm *fst; const SurfaceTerm *snd; } pair;
    struct { const SurfaceTerms *items; } tuple;
    struct { const SurfaceTerms *items; } sum;
    struct { const SurfaceTerms *items; } prod;
    struct { const SurfaceTerm *term; Int63 key; } proj;
    struct { Int63 key; Int63 size; const SurfaceTerm *term; } inj;
    struct { const SurfaceTerm *term; } absurd;
    struct { const SurfaceTerm *function; const SurfaceTerm *argument; } app;
    struct { const SurfaceBinders *binders; const SurfaceTerm *body; } fun;
    struct { SurfaceBinder binder; const SurfaceTerm *body; } arrow;
    struct { SurfaceBinder binder; const SurfaceTerm *body; } star;
    struct {
      Text name;
      const SurfaceTerm *ty;
      const SurfaceTerm *value;
      const SurfaceTerm *body;
    } let;
    struct { const SurfaceTerm *term; const SurfaceTerm *ty; } ann;
    struct {
      const SurfaceTerm *scrutinee;
      const SurfaceMotive *motive;
      const SurfaceBranches *branches;
    } elim;
  } as;
};

/* S.decl, in the Bend constructor order: a pointer to an arena node. */
typedef enum {
  SURFACE_DDEF,
  SURFACE_DAXIOM,
  SURFACE_DMU,
  SURFACE_DREC
} SurfaceDeclTag;
typedef struct {
  SurfaceDeclTag tag;
  union {
    struct { Text name; const SurfaceTerm *ty; const SurfaceTerm *body; } def;
    struct { Text name; const SurfaceTerm *ty; } axiom;
    struct { const SurfaceFams *families; } mu;
    struct { const SurfaceRecDefs *members; } rec;
  } as;
} SurfaceDecl;
SOLE_LIST(SurfaceDecls, const SurfaceDecl *)
SOLE_LIST_OPS(SurfaceDecls)

/* Constructors (S.SVar .. S.SMatch, S.Binder .. S.RecDef, S.DDef ..
   S.DRec). */
const SurfaceTerm *surface_syntax_svar(Text name);
const SurfaceTerm *surface_syntax_snat(Bignum value);
const SurfaceTerm *surface_syntax_sprop(void);
const SurfaceTerm *surface_syntax_stype(Int63 level);
const SurfaceTerm *surface_syntax_sprim(SurfacePrim prim);
const SurfaceTerm *surface_syntax_sunit(void);
const SurfaceTerm *surface_syntax_sauto(void);
const SurfaceTerm *surface_syntax_spair(const SurfaceTerm *fst,
                                       const SurfaceTerm *snd);
const SurfaceTerm *surface_syntax_stuple(const SurfaceTerms *items);
const SurfaceTerm *surface_syntax_ssum(const SurfaceTerms *items);
const SurfaceTerm *surface_syntax_sprod(const SurfaceTerms *items);
const SurfaceTerm *surface_syntax_sproj(const SurfaceTerm *term, Int63 key);
const SurfaceTerm *surface_syntax_sinj(Int63 key, Int63 size,
                                      const SurfaceTerm *term);
const SurfaceTerm *surface_syntax_sabsurd(const SurfaceTerm *term);
const SurfaceTerm *surface_syntax_sapp(const SurfaceTerm *function,
                                      const SurfaceTerm *argument);
const SurfaceTerm *surface_syntax_sfun(const SurfaceBinders *binders,
                                      const SurfaceTerm *body);
const SurfaceTerm *surface_syntax_sarrow(SurfaceBinder binder,
                                        const SurfaceTerm *body);
const SurfaceTerm *surface_syntax_sstar(SurfaceBinder binder,
                                       const SurfaceTerm *body);
const SurfaceTerm *surface_syntax_slet(Text name, const SurfaceTerm *ty,
                                      const SurfaceTerm *value,
                                      const SurfaceTerm *body);
const SurfaceTerm *surface_syntax_sann(const SurfaceTerm *term,
                                      const SurfaceTerm *ty);
const SurfaceTerm *surface_syntax_scase(const SurfaceTerm *scrutinee,
                                       const SurfaceMotive *motive,
                                       const SurfaceBranches *branches);
const SurfaceTerm *surface_syntax_smatch(const SurfaceTerm *scrutinee,
                                        const SurfaceMotive *motive,
                                        const SurfaceBranches *branches);
SurfaceBinder surface_syntax_binder(Quantity q, Text name,
                                    const SurfaceTerm *ty);
const SurfaceMotive *surface_syntax_motive(Text self, MaybeText ind,
                                           const Texts *idx,
                                           const SurfaceTerm *body);
SurfaceField surface_syntax_field(Quantity q, Text name,
                                  const SurfaceTerm *ty); /* NULL = None */
SurfaceBranch surface_syntax_br_leg(Int63 key, const SurfaceBinders *binders,
                                    const SurfaceTerm *body);
SurfaceBranch surface_syntax_br_ctor(Text name, const SurfaceFields *fields,
                                     const SurfaceTerm *body);
SurfaceBranch surface_syntax_br_default(const SurfaceTerm *body);
SurfaceFamCtor surface_syntax_fam_ctor(Text name, const SurfaceTerm *ty);
SurfaceFam surface_syntax_fam(Text name, const SurfaceBinders *params,
                              const SurfaceTerm *ty,
                              const SurfaceFamCtors *ctors);
SurfaceRecDef surface_syntax_rec_def(Text name, const SurfaceTerm *ty,
                                     const SurfaceTerm *body);
const SurfaceDecl *surface_syntax_ddef(Text name, const SurfaceTerm *ty,
                                       const SurfaceTerm *body);
const SurfaceDecl *surface_syntax_daxiom(Text name, const SurfaceTerm *ty);
const SurfaceDecl *surface_syntax_dmu(const SurfaceFams *families);
const SurfaceDecl *surface_syntax_drec(const SurfaceRecDefs *members);

/* The canonical printer (Bend lines 82 and 204 to 244). S.cat is
   String.concat (text_concat); each caller twin uses its static cat. */
Text surface_syntax_prim_name(SurfacePrim p);
Text surface_syntax_at(Nat lvl, const SurfaceTerm *s);
Text surface_syntax_binder_text(SurfaceBinder b);
Text surface_syntax_field_text(SurfaceField f);
Text surface_syntax_branch_text(SurfaceBranch br);
Text surface_syntax_motive_text(const SurfaceMotive *mo); /* not NULL */
Text surface_syntax_optional_motive(const SurfaceMotive *mo); /* NULL = None */
Text surface_syntax_raw(const SurfaceTerm *s);
Text surface_syntax_fam_ctor_text(SurfaceFamCtor fc);
Text surface_syntax_fam_text(Text word, SurfaceFam fm);
Text surface_syntax_rec_text(const SurfaceRecDefs *ms, Text word);
Text surface_syntax_decl_text(const SurfaceDecl *d);
Text surface_syntax_print(const SurfaceDecls *ds);


/* ============================================= surface/record.bend */

/* The ~parse and ~parse_decl arguments of record.bend (family.bend uses
   the first one too): a parser of one surface term or of one declaration,
   with the Bend result parsed(A) = L.result(F.Pair2<A, tokens>). Return 1
   = Done with *out and *rest, 0 = Fail with *error. env is the closure
   data of the caller (the S6 parser twin); the Bend closures capture no
   value, so env can be NULL. */
typedef int (*SurfaceParseTerm)(const void *env, const SurfaceTokens *ts,
                                const SurfaceTerm **out,
                                const SurfaceTokens **rest,
                                const KError **error);
typedef int (*SurfaceParseDecl)(const void *env, const SurfaceTokens *ts,
                                const SurfaceDecl **out,
                                const SurfaceTokens **rest,
                                const KError **error);

/* field = Field{name, ty} (line 16), private to lib/surface_record.c. It
   is here because SOLE_LIST_OPS in a .c file gives -Wunused-function
   warnings. */
typedef struct {
  Text name;
  const SurfaceTerm *ty;
} SurfaceRecordField;
SOLE_LIST(SurfaceRecordFields, SurfaceRecordField)
SOLE_LIST_OPS(SurfaceRecordFields)

Nat surface_record_length(const SurfaceTokens *ts);
/* declarations (line 105): parse and parse_decl share env. */
int surface_record_declarations(const void *env, SurfaceParseTerm parse,
                                SurfaceParseDecl parse_decl, Nat fuel,
                                const SurfaceTokens *ts,
                                const SurfaceDecls *acc,
                                const SurfaceDecls **out,
                                const SurfaceTokens **rest,
                                const KError **error);
int surface_record_qualify(const SurfaceTerm *term, Text field,
                           SurfaceLoc loc, const SurfaceTerm **out,
                           const KError **error);

/* ============================================== surface/order.bend */

/* rule, in the Bend constructor order. */
typedef enum { SURFACE_ORDER_STRUCTURAL } SurfaceOrderRule;

/* step = Step{scrut, ctor}: by value. */
typedef struct {
  Int63 scrut;
  MaybeText ctor;
} SurfaceOrderStep;
SOLE_LIST(SurfaceOrderSteps, SurfaceOrderStep)
SOLE_LIST_OPS(SurfaceOrderSteps)

/* call = Call{callee, chain}: by value. */
typedef struct {
  Text callee;
  const SurfaceOrderSteps *chain;
} SurfaceOrderCall;
SOLE_LIST(SurfaceOrderCalls, SurfaceOrderCall)
SOLE_LIST_OPS(SurfaceOrderCalls)

/* row = Row{member, calls}: by value. */
typedef struct {
  Text member;
  const SurfaceOrderCalls *calls;
} SurfaceOrderRow;
SOLE_LIST(SurfaceOrderRows, SurfaceOrderRow)
SOLE_LIST_OPS(SurfaceOrderRows)

/* O.t = Certificate{group, arg, rows}. Maybe<O.t> is a NULL pointer. */
typedef struct {
  const Texts *group;
  Int63 arg;
  const SurfaceOrderRows *rows;
} SurfaceOrderCertificate;

/* F.Pair2<String, T.t>: one member of a recursive group. */
typedef struct {
  Text name;
  const Term *body;
} SurfaceOrderMember;
SOLE_LIST(SurfaceOrderMembers, SurfaceOrderMember)
SOLE_LIST_OPS(SurfaceOrderMembers)

/* The types below are private to lib/surface_order.c. They are here
   because SOLE_LIST_OPS in a .c file gives -Wunused-function warnings. */
/* status, in the Bend constructor order. */
typedef enum {
  SURFACE_ORDER_PRINCIPAL,
  SURFACE_ORDER_SMALLER,
  SURFACE_ORDER_OTHER
} SurfaceOrderStatus;
SOLE_LIST(SurfaceOrderStatuses, SurfaceOrderStatus)
SOLE_LIST_OPS(SurfaceOrderStatuses)

/* List<S.t<T.t>>. */
SOLE_LIST(SurfaceOrderShapes, const TermShape *)
SOLE_LIST_OPS(SurfaceOrderShapes)

/* peeled = Peeled{name, formals, body}: by value. */
typedef struct {
  Text name;
  Nat formals;
  const Term *body;
} SurfaceOrderPeeled;
SOLE_LIST(SurfaceOrderPeeleds, SurfaceOrderPeeled)
SOLE_LIST_OPS(SurfaceOrderPeeleds)

/* job = Visit{statuses, chain, term}: by value. */
typedef struct {
  const SurfaceOrderStatuses *statuses;
  const SurfaceOrderSteps *chain;
  const Term *term;
} SurfaceOrderJob;
SOLE_LIST(SurfaceOrderJobs, SurfaceOrderJob)
SOLE_LIST_OPS(SurfaceOrderJobs)

/* O.maybe and O.choose have no C twin: they are C if statements. */
Bool surface_order_mentions(const Texts *group, const Term *tm);
/* certify (line 266): 1 = Done with *out (NULL = None{}), 0 = Fail. */
int surface_order_certify(SurfaceOrderRule rule,
                          const SurfaceOrderMembers *ms,
                          const SurfaceOrderCertificate **out,
                          const KError **error);
/* translate (line 316): 1 = Done with *out, 0 = Fail with *error. */
int surface_order_translate(const SurfaceOrderCertificate *cert,
                            const Term *body, const Term **out,
                            const KError **error);

/* =========================================== surface/totality.bend */

/* guard_group (line 31): spend (the budget point) then O.certify. 1 = Done
   with *out (NULL = None{}), 0 = Fail with *error. globals is not used. */
int surface_totality_guard_group(Budget budget, const Globals *globals,
                                 const SurfaceOrderMembers *members,
                                 const SurfaceOrderCertificate **out,
                                 const KError **error);
/* guard (line 42) has no Bend caller. Maybe<F.Int63.t> is (*has_arg, *arg),
   *arg set only when *has_arg = YES. ty is not used. */
int surface_totality_guard(Budget budget, const Globals *globals, Text name,
                           const Term *ty, const Term *body, Bool *has_arg,
                           Int63 *arg, const KError **error);

/* ======================================= surface/elab_program.bend */
/* lib/surface_elab_program.c (S5 session 3). program<A> = Pure{value} |
   Abort{error} | Request{context, expected, syntax, resume}. In C a
   program is a function that returns int: 1 = Pure with *out, 0 = Abort
   with *error. A Request plus its drive is a direct call of the walker:
   SurfaceElabWalk is constructor.bend walker (K.ctx -> Maybe<V.t> ->
   Syn.t -> M.result(T.t)); walk_env is the closure data of the S6 elab
   twin (NULL is OK); expected NULL = None{}. lift is the identity (R.result
   and program have the same C shape). bind and map_result are C if
   statements. result has no C twin. */
typedef int (*SurfaceElabWalk)(const void *walk_env, const CheckCtx *c,
                               const Value *expected, const SurfaceTerm *s,
                               const Term **out, const KError **error);
int surface_elab_program_request(const void *walk_env, SurfaceElabWalk go,
                                 const CheckCtx *c, const Value *expected,
                                 const SurfaceTerm *s, const Term **out,
                                 const KError **error);

/* ======================================== surface/constructor.bend */

/* lib/surface_constructor.c (S5 session 3b). M.result(A) is the int
   return (1 = Pure with *out, 0 = Abort with *error); a Unit result (owner)
   has no out. ~go is the (walk_env, go) pair of SurfaceElabWalk. P.family
   and P.ctor are const Family * and const Ctor *. choose has no C twin (a
   C if). index_terms takes the (run_env, run) pair of
   SurfaceConstructorIndexRun (the Bend run closure). */
typedef int (*SurfaceConstructorIndexRun)(const void *run_env,
                                          const Terms *ix, const Terms **out,
                                          const KError **error);
const Values *surface_constructor_reverse(const Values *xs,
                                          const Values *acc);
int surface_constructor_eval_in(const CheckCtx *c, const Term *tm,
                                const Value **out, const KError **error);
int surface_constructor_arguments(const void *walk_env, SurfaceElabWalk go,
                                  const Telescope *fields, const CheckCtx *c,
                                  const Values *env,
                                  const SurfaceTerms *args, const Terms **out,
                                  const KError **error);
int surface_constructor_index_terms(const Terms *ix, const void *run_env,
                                    SurfaceConstructorIndexRun run,
                                    const Terms **out, const KError **error);
int surface_constructor_expected(const void *walk_env, SurfaceElabWalk go,
                                 const CheckCtx *c, const Family *fam,
                                 const Ctor *ctor, const SurfaceTerms *args,
                                 const Value *ty, const Term **out,
                                 const KError **error);
int surface_constructor_owner(const CheckCtx *c, Text family, Text name,
                              const Value *ty, const KError **error);

/* ================================== surface/constructor_infer.bend */

/* lib/surface_constructor_infer.c (S5 session 3b). M.result(A) is the int
   return (1 = Pure with *out, 0 = Abort with *error). ~go is the
   (walk_env, go) pair of SurfaceElabWalk. slots is a Values list with a
   NULL head for None{}. constraints (List<F.Pair2<V.t, V.t>>) is
   SurfaceConstructorInferPairs (private to the twin). */
typedef struct {
  const Value *a;
  const Value *b;
} SurfaceConstructorInferPair;
SOLE_LIST(SurfaceConstructorInferPairs, SurfaceConstructorInferPair)
SOLE_LIST_OPS(SurfaceConstructorInferPairs)
int surface_constructor_infer_infer(const void *walk_env, SurfaceElabWalk go,
                                    const CheckCtx *c, const Family *fam,
                                    const Ctor *ctor,
                                    const SurfaceTerms *args,
                                    const Term **out, const KError **error);

/* ============================================== surface/sugar.bend */

/* lib/surface_sugar.c (S5 session 3c). M.result(A) is the int return (1 =
   Pure with *out, 0 = Abort with *error). ~A slots is a Bend list of any
   type and only its length counts, thus slots is a Nat count: the elab
   caller (elab.bend line 464) gives the length of its VLegs list. */
SurfaceBranch surface_sugar_arm(Int63 k, const SurfaceTerm *body);
int surface_sugar_expand(const SurfaceBranches *bs, Int63 k, Nat slots,
                         const SurfaceBranches **out, const KError **error);

/* ============================================= surface/family.bend */

/* lib/surface_family.c (S5 session 3c). L.result(A) and M.result(A) are
   the int return (1 = Done or Pure with *out, 0 = Fail or Abort with
   *error); parsed(A) adds *rest. ~parse is the (env, parse) pair of
   SurfaceParseTerm. P.family is const Family *. The callers build only
   the ctor_step Read{fuel, ts, acc, seen}, thus constructors takes the
   Read fields; AfterType and AfterTail are internal states of its loop. */
Bool surface_family_known(Text name, const Texts *seen);
const Texts *surface_family_group_names(const SurfaceFams *fams);
int surface_family_constructors(const void *env, SurfaceParseTerm parse,
                                Nat fuel, const SurfaceTokens *ts,
                                const SurfaceFamCtors *acc,
                                const Texts *seen,
                                const SurfaceFamCtors **out,
                                const SurfaceTokens **rest,
                                const KError **error);
int surface_family_expanded(const Family *fam, const SurfaceBranches *bs,
                            const SurfaceBranches **out,
                            const KError **error);

/* ============================================= surface/parser.bend */

/* lib/surface_parser.c (S6 sessions 1 to 1c). L.result(List<&2, S.decl>)
   is the int return (1 = Done with *out, 0 = Fail with *error), as the
   twin of Record.declarations without *rest: finish (line 789) has no C
   twin, parse_admitted drops the tokens after the declarations. The
   source bytes of parse_bytes (Bend List<U32>) are a Text slice, as in
   surface_lexer_lex_bytes. */
int surface_parser_parse_admitted(const SurfaceTokens *ts,
                                  const SurfaceDecls **out,
                                  const KError **error);
int surface_parser_parse_tokens(const SurfaceTokens *ts,
                                const SurfaceDecls **out,
                                const KError **error);
int surface_parser_parse_bytes(Text source, const SurfaceDecls **out,
                               const KError **error);
int surface_parser_parse(Text source, const SurfaceDecls **out,
                         const KError **error);

/* =============================================== surface/elab.bend */

/* lib/surface_elab.c (S6 sessions 2 to 2e). R.result(T.t), R.result(K.decl)
   and R.result(G.t) are the int return (1 = Done with *out, 0 = Fail with
   *error). drive (line 632, @unsafe) has no C twin: the request walk is
   direct C recursion (depth = term nesting). expected NULL = None. */
int surface_elab_elab(const CheckCtx *c, const Value *expected,
                      const SurfaceTerm *s, const Term **out,
                      const KError **error);
int surface_elab_elab_decl(const CheckCtx *c, const SurfaceDecl *d,
                           CheckDecl *out, const KError **error);
int surface_elab_elab_mu_group(Budget budget, const Globals *globals,
                               const SurfaceFams *fams, const Globals **out,
                               const KError **error);

/* ============================================ surface/program.bend */

/* lib/surface_program.c (S6 session 3). R.result(checked) is the int
   return (1 = Done with *out, 0 = Fail with *error). entries
   (List<F.Pair2<String, G.entry>>) is CheckNamedEntries. */
SOLE_LIST_OPS(CheckNamedEntries)
SOLE_LIST_OPS(CheckDecls)
/* typed = Typed{name, ty, body}: by value. */
typedef struct {
  Text name;
  const Term *ty;
  const SurfaceTerm *body;
} SurfaceProgramTyped;
SOLE_LIST(SurfaceProgramTypeds, SurfaceProgramTyped)
SOLE_LIST_OPS(SurfaceProgramTypeds)
/* body = Body{name, ty, tyv, body}: by value. */
typedef struct {
  Text name;
  const Term *ty;
  const Value *tyv;
  const Term *body;
} SurfaceProgramBody;
SOLE_LIST(SurfaceProgramBodies, SurfaceProgramBody)
SOLE_LIST_OPS(SurfaceProgramBodies)
/* checked = Checked{globals, rows}: by value. */
typedef struct {
  const Globals *globals;
  const CheckNamedEntries *rows;
} SurfaceProgramChecked;
int surface_program_elab_program_in(Budget budget, const Globals *g,
                                    const SurfaceDecls *ds,
                                    SurfaceProgramChecked *out,
                                    const KError **error);
Text surface_program_checked_form(const CheckNamedEntries *rows);
/* axiom_names (line 113) has no Bend caller. */
const Texts *surface_program_axiom_names(const CheckNamedEntries *rows);

/* ============================================== surface/check.bend */

/* lib/surface_check.c (S6 session 3). source (line 22) and erased_source
   (line 33): the source bytes to the answer text ("FAIL\n" error, or
   "OK\n" count "\n" form). Each makes its own budget of 16384. */
Text surface_check_source(Text bytes);
Text surface_check_erased_source(Text bytes);

/* ======================================== test/pinfront/token.bend */
/* Twin: test/pinfront/token.c. The kinds, the token, the location and
   describe are the surface ones (section surface/token.bend). */
SurfaceKind pinfront_token_ident_kind(Text s);

/* ======================================== test/pinfront/lexer.bend */
/* Twin: test/pinfront/lexer.c (a copy of lib/surface_lexer.c with the
   pinfront delta). */
const KError *pinfront_lexer_error(SurfaceLoc position, Text message);
int pinfront_lexer_hex_digit(U32 c, U32 *out); /* 1 = Some with *out */
int pinfront_lexer_lex_bytes(Text source, const SurfaceTokens **out,
                            const KError **error);
int pinfront_lexer_lex(Text source, const SurfaceTokens **out,
                      const KError **error);

/* ======================================= test/pinfront/parser.bend */
/* Twin: test/pinfront/parser.c (a copy of lib/surface_parser.c with the
   pinfront delta). The declarations and the syntax are the surface ones
   (section surface/syntax.bend). */
int pinfront_parser_parse_tokens(const SurfaceTokens *ts,
                                const SurfaceDecls **out,
                                const KError **error);
int pinfront_parser_parse_bytes(Text source, const SurfaceDecls **out,
                               const KError **error);
int pinfront_parser_parse(Text source, const SurfaceDecls **out,
                         const KError **error);

/* ========================================= test/pinfront/elab.bend */

/* test/pinfront/elab.c (S7 sessions 3 and 3b). A copy of lib/surface_elab.c
   with the pinfront delta (the header comment of test/pinfront/elab.c).
   R.result(T.t), R.result(K.decl) and R.result(G.t) are the int return
   (1 = Done with *out, 0 = Fail with *error). drive (pinfront line 533,
   @unsafe) has no C twin: the request walk is direct C recursion (depth =
   term nesting). expected NULL = None. test/pinfront/elab_program.bend is
   surface/elab_program.bend with other import paths: no C twin, the
   surface_elab_program_* functions are SHARED. */
int pinfront_elab_elab(const CheckCtx *c, const Value *expected,
                       const SurfaceTerm *s, const Term **out,
                       const KError **error);
int pinfront_elab_elab_decl(const CheckCtx *c, const SurfaceDecl *d,
                            CheckDecl *out, const KError **error);
int pinfront_elab_elab_mu_group(Budget budget, const Globals *globals,
                                const SurfaceFams *fams, const Globals **out,
                                const KError **error);

/* ====================================== test/pinfront/program.bend */

/* test/pinfront/program.c (S7 session 4). A copy of lib/surface_program.c
   whose elab calls go to the pinfront_elab_* functions (the header comment
   of test/pinfront/program.c). R.result(checked) is the int return (1 =
   Done with *out, 0 = Fail with *error). typed, body, checked and entries
   are the surface types (section surface/program.bend). checked_form and
   axiom_names are SHARED: surface_program_checked_form and
   surface_program_axiom_names. test/pinfront/order.bend and
   test/pinfront/totality.bend are surface/order.bend and
   surface/totality.bend with other import paths and import no COPIED
   module: no C twin, the surface_order_* and surface_totality_* functions
   are SHARED. */
int pinfront_program_elab_program_in(Budget budget, const Globals *g,
                                     const SurfaceDecls *ds,
                                     SurfaceProgramChecked *out,
                                     const KError **error);
#endif
