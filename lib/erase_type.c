/* lib/erase_type.c: twin of erase/type.bend (S4 session 1b).
   Type-directed runtime admission and function layouts (Kanon 69f3be51).
   Collection products omit erased fields; sums retain every tag and layout
   leg. Dependent pairs omit erased components; recursive families have
   nominal layouts.
   The named eliminators (inspect, maybe, boolean, pair, has, empty,
   with_signature) are C if and switch statements. Each ~loop argument has
   one instance (repr_visit, arity or signature_of), so a direct call
   replaces it (no function pointer). */
#include "sole.h"

/* String.concat of a fixed list of parts (the cat idiom of kernel_rules). */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

static RulesCtx ctx_of(const CheckCtx *c) {
  RulesCtx context;
  context.check = c;
  return context;
}

/* K.Tid{String.concat([open, inner, ">"])}. */
static Tid wrapped(const char *open, Text inner) {
  Text parts[3];
  Tid t;
  parts[0] = text_of_cstring(open);
  parts[1] = inner;
  parts[2] = text_of_cstring(">");
  t.name = cat(parts, 3);
  return t;
}

static Tid tid_of_cstring(const char *ascii) {
  Tid t;
  t.name = text_of_cstring(ascii);
  return t;
}

static int repr_visit(const CheckCtx *c, const Value *ty, Budget b,
                      const Repr **out, const KError **error);

/* V.as_lan and V.as_ran as a pointer into slot: NULL = None. */
static const ValueFormer *lan_of(const Value *ty, ValueFormer *slot) {
  if (!value_as_lan(ty, slot)) return NULL;
  return slot;
}

static const ValueFormer *ran_of(const Value *ty, ValueFormer *slot) {
  if (!value_as_ran(ty, slot)) return NULL;
  return slot;
}

/* empty_former (line 58) with empty_collection (line 41): NULL = None. */
static Bool empty_former(const ValueFormer *former) {
  Int63 n;
  if (former == NULL) return NO;
  if (!rules_collection_view(former->shape, &n)) return NO;
  return int63_equal(n, int63_zero());
}

/* proof_free (line 45): the universe of the quoted type is not zero.
   C.infer_univ runs on the budget of c (check_infer_univ, local fuel). */
static int proof_free(const CheckCtx *c, const Value *ty, Bool *out,
                      const KError **error) {
  const Term *tm;
  Level l;
  if (!eval_quote(check_globals(c), c->size, ty, &tm, error)) return 0;
  if (!check_infer_univ(c, tm, &l, error)) return 0;
  *out = level_equal(l, level_zero()) ? NO : YES;
  return 1;
}

/* opened (line 48): R.run with EV.dispatch, so an evaluation host on the
   stack (no fuel), as kernel_eval eval_whnf. */
int erase_type_opened(const CheckCtx *c, const Closure *diagram,
                      const Values *args, const Value **out,
                      const KError **error) {
  RulesHost host;
  RulesCtx ctx;
  RulesEvaluator ev;
  host.kind = RULES_HOST_EVAL;
  host.fuel = NULL;
  ctx.globals = check_globals(c);
  ev = eval_ev(ctx);
  return rules_open_closure(&host, &ev, diagram, args, out, error);
}

static const Repr *any_repr(void) {
  return eterm_repr(REPR_RUNION, tid_of_cstring("any"));
}

Tid erase_type_mu_tid(Text name) { return wrapped("mu<", name); }

/* point_arrow (line 54) of R.point_view(shape): 1 = Some with *out. */
static int point_arrow(const Closure *diagram, const ValueShape *shape,
                       EraseArrow *out) {
  RulesPoint point;
  if (!rules_point_view(shape, &point)) return 0;
  out->quantity = point.quantity;
  out->name = point.name;
  out->domain = point.domain;
  out->diagram = diagram;
  return 1;
}

/* under (line 62): F.Pair2{C.bind(x, q, dom, c), v} is *inner and *out.
   C.bind is runtime_ops_bind (the same body). */
int erase_type_under(const CheckCtx *c, Quantity q, Text x, const Value *dom,
                     const Closure *d, const CheckCtx **inner,
                     const Value **out, const KError **error) {
  if (!erase_type_opened(c, d, Values_cons(value_var(c->size), NULL), out,
                         error))
    return 0;
  *inner = runtime_ops_bind(x, q, dom, c);
  return 1;
}

static const Repr *neutral_repr(const Head *head, const Spines *spine) {
  if (value_head_equal(head, value_hglobal(text_of_cstring("Nat"))) &&
      spine == NULL)
    return eterm_repr(REPR_RUNION, tid_of_cstring("nat"));
  return any_repr();
}

int erase_type_former_arrow(const ValueFormer *former, EraseArrow *out) {
  return point_arrow(former->diagram, former->shape, out);
}

/* runtime_weak (line 76): a universe is not runtime; an empty collection
   former is not runtime; else proof_free. */
static int runtime_weak(const CheckCtx *c, const Value *ty, Bool *out,
                        const KError **error) {
  ValueFormer lan;
  ValueFormer ran;
  if (value_as_univ(ty) != NULL) {
    *out = NO;
    return 1;
  }
  if (empty_former(lan_of(ty, &lan)) || empty_former(ran_of(ty, &ran))) {
    *out = NO;
    return 1;
  }
  return proof_free(c, ty, out, error);
}

int erase_type_runtime_ty(const CheckCtx *c, const Value *ty, Bool *out,
                          const KError **error) {
  const Value *w;
  if (!eval_whnf(check_globals(c), ty, &w, error)) return 0;
  return runtime_weak(c, w, out, error);
}

int erase_type_point_runtime(const CheckCtx *c, Quantity q, const Value *ty,
                             Bool *out, const KError **error) {
  if (quantity_equal(q, QUANTITY_ZERO)) {
    *out = NO;
    return 1;
  }
  return erase_type_runtime_ty(c, ty, out, error);
}

/* count (line 84) over VLegs: a right fold of succ is a left loop (the
   wrapping succ gives the same result in each order). */
static Int63 count_legs(const VLegs *xs) {
  Int63 n = int63_zero();
  for (; xs != NULL; xs = xs->tail) n = int63_succ(n);
  return n;
}

/* leg_types (line 89). C.result_of(C.drive(program, C.budget(c))): the
   programs coll_legs_of and c_open_leg only lift ops calls (no Request), so
   drive spends no fuel, and their C twins take no host. result_of drops
   the fuel. R.map_list is a left-to-right loop (cons, then reverse). */
int erase_type_leg_types(const CheckCtx *c, Int63 n, const Closure *diagram,
                         const Values **out, const KError **error) {
  const VLegs *legs;
  const Values *acc = NULL;
  const Value *v;
  if (!rules_coll_legs_of(ctx_of(c), diagram, &legs, error)) return 0;
  if (!int63_equal(count_legs(legs), n)) {
    *error = error_mismatch(
        text_of_cstring("a collection diagram has the wrong number of legs"));
    return 0;
  }
  for (; legs != NULL; legs = legs->tail) {
    if (!rules_c_open_leg(ctx_of(c), legs->head, &v, error)) return 0;
    acc = Values_cons(v, acc);
  }
  *out = Values_reverse(acc);
  return 1;
}

/* tuple_texts (line 95, @unsafe): the self call is a loop. A kept leg adds
   its printed repr; an erased leg adds nothing. */
static int tuple_texts(const CheckCtx *c, const Values *tys, Budget b,
                       const Texts **out, const KError **error) {
  const Texts *acc = NULL;
  Bool keep;
  const Repr *r;
  for (; tys != NULL; tys = tys->tail) {
    if (!erase_type_runtime_ty(c, tys->head, &keep, error)) return 0;
    if (keep && !repr_visit(c, tys->head, b, &r, error)) return 0;
    if (keep) acc = Texts_cons(eterm_print_repr(r), acc);
  }
  *out = Texts_reverse(acc);
  return 1;
}

static int tuple_repr(const CheckCtx *c, Int63 n, const Closure *diagram,
                      Budget b, const Repr **out, const KError **error) {
  const Values *tys;
  const Texts *texts;
  if (int63_equal(n, int63_zero())) {
    *out = any_repr();
    return 1;
  }
  if (!erase_type_leg_types(c, n, diagram, &tys, error)) return 0;
  if (!tuple_texts(c, tys, b, &texts, error)) return 0;
  *out = eterm_repr(REPR_RSTRUCT,
                    wrapped("tuple<", eterm_join(text_of_cstring(","), texts)));
  return 1;
}

/* sum_texts (line 108, @unsafe): the self call is a loop. A kept leg adds
   its printed repr; an erased leg adds "unit". */
static int sum_texts(const CheckCtx *c, const Values *tys, Budget b,
                     const Texts **out, const KError **error) {
  const Texts *acc = NULL;
  Bool keep;
  const Repr *r;
  Text text;
  for (; tys != NULL; tys = tys->tail) {
    if (!erase_type_runtime_ty(c, tys->head, &keep, error)) return 0;
    if (keep && !repr_visit(c, tys->head, b, &r, error)) return 0;
    text = text_of_cstring("unit");
    if (keep) text = eterm_print_repr(r);
    acc = Texts_cons(text, acc);
  }
  *out = Texts_reverse(acc);
  return 1;
}

static int sum_repr(const CheckCtx *c, Int63 n, const Closure *diagram,
                    Budget b, const Repr **out, const KError **error) {
  const Values *tys;
  const Texts *texts;
  if (int63_equal(n, int63_zero())) {
    *out = any_repr();
    return 1;
  }
  if (!erase_type_leg_types(c, n, diagram, &tys, error)) return 0;
  if (!sum_texts(c, tys, b, &texts, error)) return 0;
  *out = eterm_repr(REPR_RUNION,
                    wrapped("sum<", eterm_join(text_of_cstring("|"), texts)));
  return 1;
}

/* refused_repr (line 120): a former that is not an empty collection fails;
   a type with no former is any. */
static int refused_repr(const Value *ty, const Repr **out,
                        const KError **error) {
  ValueFormer lan;
  ValueFormer ran;
  const ValueFormer *l = lan_of(ty, &lan);
  const ValueFormer *r = ran_of(ty, &ran);
  if ((l != NULL || r != NULL) && !(empty_former(l) || empty_former(r))) {
    *error = error_not_yet(
        text_of_cstring("structural layouts in type-directed erasure"));
    return 0;
  }
  *out = any_repr();
  return 1;
}

int erase_type_arrow_of(const Value *value, EraseArrow *out) {
  ValueFormer former;
  if (!value_as_ran(value, &former)) return 0;
  return erase_type_former_arrow(&former, out);
}

static int product_repr(const CheckCtx *c, const Value *ty,
                        const ValueFormer *former, Budget b,
                        const Repr **out, const KError **error) {
  Int63 n;
  if (!rules_collection_view(former->shape, &n))
    return refused_repr(ty, out, error);
  return tuple_repr(c, n, former->diagram, b, out, error);
}

/* repr_text (line 132, @unsafe): no self call, so a plain function. */
static int repr_text(const CheckCtx *c, const Value *ty, Bool keep, Budget b,
                     const Texts **out, const KError **error) {
  const Repr *r;
  if (!keep) {
    *out = NULL;
    return 1;
  }
  if (!repr_visit(c, ty, b, &r, error)) return 0;
  *out = Texts_cons(eterm_print_repr(r), NULL);
  return 1;
}

static int pair_repr(const CheckCtx *c, const EraseArrow *point, Budget b,
                     const Repr **out, const KError **error) {
  Bool keep1;
  Bool keep2;
  const Texts *first;
  const Texts *second;
  const CheckCtx *c1;
  const Value *cod;
  if (!erase_type_point_runtime(c, point->quantity, point->domain, &keep1,
                                error))
    return 0;
  if (!repr_text(c, point->domain, keep1, b, &first, error)) return 0;
  if (!erase_type_under(c, point->quantity, point->name, point->domain,
                        point->diagram, &c1, &cod, error))
    return 0;
  if (!erase_type_runtime_ty(c1, cod, &keep2, error)) return 0;
  if (!repr_text(c1, cod, keep2, b, &second, error)) return 0;
  *out = eterm_repr(REPR_RSTRUCT,
                    wrapped("pair<", eterm_join(text_of_cstring(","),
                                                Texts_append(first, second))));
  return 1;
}

/* sum_former_repr (line 145): a mu shape, then a point shape, then a
   collection shape. */
static int sum_former_repr(const CheckCtx *c, const Value *ty,
                           const ValueFormer *former, Budget b,
                           const Repr **out, const KError **error) {
  Text name;
  const Values *indices;
  EraseArrow point;
  Int63 n;
  if (rules_as_mu_value(former->shape, &name, &indices)) {
    *out = eterm_repr(REPR_RUNION, erase_type_mu_tid(name));
    return 1;
  }
  if (point_arrow(former->diagram, former->shape, &point))
    return pair_repr(c, &point, b, out, error);
  if (!rules_collection_view(former->shape, &n))
    return refused_repr(ty, out, error);
  return sum_repr(c, n, former->diagram, b, out, error);
}

static int structural_repr(const CheckCtx *c, const Value *ty, Budget b,
                           const Repr **out, const KError **error) {
  ValueFormer former;
  if (value_as_ran(ty, &former))
    return product_repr(c, ty, &former, b, out, error);
  if (value_as_lan(ty, &former))
    return sum_former_repr(c, ty, &former, b, out, error);
  return refused_repr(ty, out, error);
}

static int nonfunction_repr(const CheckCtx *c, const Value *ty, Budget b,
                            const Repr **out, const KError **error) {
  ValueNeutral neutral;
  if (!value_as_neutral(ty, &neutral))
    return structural_repr(c, ty, b, out, error);
  *out = neutral_repr(neutral.head, neutral.spine);
  return 1;
}

/* arity (line 170, @unsafe) with arity_weak (line 167) and arity_arrow
   (line 160): the loop argument is arity itself in tail position, so one C
   loop. Each turn spends one unit of b (budget point). */
int erase_type_arity(const CheckCtx *c, const Value *ty, Int63 n, Budget b,
                     Int63 *out, const KError **error) {
  Budget rest;
  const Value *w;
  EraseArrow a;
  Bool keep;
  const CheckCtx *c1;
  const Value *cod;
  for (;;) {
    if (!budget_spend(text_of_cstring("erasure function arity"), b, &rest,
                      error))
      return 0;
    if (!eval_whnf(check_globals(c), ty, &w, error)) return 0;
    if (!erase_type_arrow_of(w, &a)) {
      *out = n;
      return 1;
    }
    if (!erase_type_point_runtime(c, a.quantity, a.domain, &keep, error))
      return 0;
    if (!erase_type_under(c, a.quantity, a.name, a.domain, a.diagram, &c1,
                          &cod, error))
      return 0;
    if (keep) n = int63_succ(n);
    c = c1;
    ty = cod;
    b = rest;
  }
}

static int ordinary_repr(const CheckCtx *c, const Value *ty, Budget b,
                         const Repr **out, const KError **error) {
  EraseArrow a;
  Int63 n;
  if (!erase_type_arrow_of(ty, &a))
    return nonfunction_repr(c, ty, b, out, error);
  if (!erase_type_arity(c, ty, int63_zero(), b, &n, error)) return 0;
  *out = eterm_repr(REPR_RFUNC, wrapped("fn<", int63_show(n)));
  return 1;
}

/* repr_visit (line 177, @unsafe): the loop argument is repr_visit itself.
   The calls through tuple_texts, sum_texts and repr_text are not tail
   calls, so this stays C recursion (depth = the nesting of the type). Each
   level spends one unit of b (budget point). */
static int repr_visit(const CheckCtx *c, const Value *ty, Budget b,
                      const Repr **out, const KError **error) {
  Budget rest;
  const Value *w;
  if (!budget_spend(text_of_cstring("erasure type representation"), b, &rest,
                    error))
    return 0;
  if (!eval_whnf(check_globals(c), ty, &w, error)) return 0;
  return ordinary_repr(c, w, rest, out, error);
}

/* repr_of (line 180): the budget of c (budget point). */
int erase_type_repr_of(const CheckCtx *c, const Value *ty, const Repr **out,
                       const KError **error) {
  return repr_visit(c, ty, check_budget(c), out, error);
}

static int repr_tid(const Repr *repr, Tid *out, const KError **error) {
  switch (repr->tag) {
    case REPR_RI31:
      *error = error_mismatch(text_of_cstring(
          "a structured value stands at a type whose repr is i31"));
      return 0;
    case REPR_RSTRUCT: *out = repr->name; return 1;
    case REPR_RUNION: *out = repr->name; return 1;
    case REPR_RFUNC: *out = repr->name; return 1;
    case REPR_RTHUNK: *out = repr->name; return 1;
  }
  *out = repr->name; /* unreachable: the switch is exhaustive */
  return 1;
}

int erase_type_tid_of(const CheckCtx *c, const Value *ty, Tid *out,
                      const KError **error) {
  const Repr *repr;
  if (!erase_type_repr_of(c, ty, &repr, error)) return 0;
  return repr_tid(repr, out, error);
}

int erase_type_parameter(const CheckCtx *c, Quantity q, const Value *dom,
                         const Reprs **out, const KError **error) {
  Bool keep;
  const Repr *r;
  if (!erase_type_point_runtime(c, q, dom, &keep, error)) return 0;
  if (!keep) {
    *out = NULL;
    return 1;
  }
  if (!erase_type_repr_of(c, dom, &r, error)) return 0;
  *out = Reprs_cons(r, NULL);
  return 1;
}

/* signature_weak (line 200) when ty has no arrow: Signature{[], r}, with
   the parameters of the turns before (acc, reversed). */
static int signature_end(const CheckCtx *c, const Value *ty,
                         const Reprs *acc, EraseSignature *out,
                         const KError **error) {
  const Repr *r;
  if (!erase_type_repr_of(c, ty, &r, error)) return 0;
  out->params = Reprs_reverse(acc);
  out->result = r;
  return 1;
}

/* signature_of (line 204, @unsafe) with signature_weak (line 200) and
   signature_arrow (line 193): the loop argument is signature_of itself.
   prepend(here, s) after the call is a list append, so the loop keeps the
   parameters reversed in acc. Each turn spends one unit of b (budget
   point). */
int erase_type_signature_of(const CheckCtx *c, const Value *ty, Budget b,
                            EraseSignature *out, const KError **error) {
  const Reprs *acc = NULL;
  Budget rest;
  const Value *w;
  EraseArrow a;
  const Reprs *here;
  const CheckCtx *c1;
  const Value *cod;
  for (;;) {
    if (!budget_spend(text_of_cstring("erasure function signature"), b, &rest,
                      error))
      return 0;
    if (!eval_whnf(check_globals(c), ty, &w, error)) return 0;
    if (!erase_type_arrow_of(w, &a))
      return signature_end(c, w, acc, out, error);
    if (!erase_type_parameter(c, a.quantity, a.domain, &here, error))
      return 0;
    if (!erase_type_under(c, a.quantity, a.name, a.domain, a.diagram, &c1,
                          &cod, error))
      return 0;
    acc = Reprs_reverse_onto(here, acc);
    c = c1;
    ty = cod;
    b = rest;
  }
}
