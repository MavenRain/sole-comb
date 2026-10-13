/* lib/surface_constructor_infer.c: twin of surface/constructor_infer.bend
   (S5 session 3b). M.result(A) is the int return (1 = Pure with *out, 0 =
   Abort with *error). ~go is the (walk_env, go) pair of SurfaceElabWalk;
   go(c, e, s) is a direct call. slots (List<Maybe<V.t>>) is a Values list
   with a NULL head for None{}. constraints is SurfaceConstructorInferPairs.
   state is the private struct State. An F.Pair2 answer is two out
   parameters. optional and C.choose are C if statements. */
#include "sole.h"

/* Syn.cat: String.concat of a fixed list of parts. */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

/* The RulesCtx of the ops calls (as erase_mu.c ctx_of). */
static RulesCtx ctx_of(const CheckCtx *c) {
  RulesCtx context;
  context.check = c;
  return context;
}

/* state = State{parameters, values, terms} (line 23). */
typedef struct {
  const Values *parameters;
  const Values *values;
  const Terms *terms;
} State;

/* empty (line 25): one None{} slot per parameter. */
static const Values *empty(const Telescope *params) {
  const Values *acc = NULL;
  for (; params != NULL; params = params->tail) acc = Values_cons(NULL, acc);
  return acc;
}

/* count (line 29). */
static Int63 count(const Telescope *params) {
  Int63 n = int63_zero();
  for (; params != NULL; params = params->tail) n = int63_succ(n);
  return n;
}

/* slot_base (line 38): Int63.Int{False{}, [0, 0, 4]} = 4 * 32768^2 = 2^32.
   A slot level is far above every context level. A slot never aliases a
   local and quotes to a negative index. */
static Int63 slot_base(void) { return (Int63)4294967296LL; }

/* environment (line 39): a self tail call, so a loop. */
static const Values *environment(const Values *ps, Int63 base, Int63 i,
                                 const Values *acc) {
  for (; ps != NULL; ps = ps->tail) {
    const Value *p = ps->head;
    acc = Values_cons(p == NULL ? value_var(int63_add(base, i)) : p, acc);
    i = int63_succ(i);
  }
  return acc;
}

/* parameter (line 46): 1 = Some with *out (base <= level < base + np). */
static int parameter(Int63 base, Int63 np, const Value *v, Int63 *out) {
  Int63 level;
  if (v->tag != VALUE_VNEUTRAL || v->as.vneutral.head->tag != VALUE_HLOCAL ||
      v->as.vneutral.spine != NULL)
    return 0;
  level = v->as.vneutral.head->as.hlocal.level;
  if (order_le(int63_compare(base, level)) &&
      !order_le(int63_compare(int63_add(base, np), level))) {
    *out = int63_sub(level, base);
    return 1;
  }
  return 0;
}

/* field_site (line 52). */
static Text field_site(Text name, Text k) {
  Text parts[4];
  parts[0] = text_of_cstring("field ");
  parts[1] = k;
  parts[2] = text_of_cstring(" of the constructor ");
  parts[3] = name;
  return cat(parts, 4);
}

/* mismatch (line 53). */
static const KError *mismatch(const CheckCtx *c, Text site, Text what,
                              const Value *given, const Value *wanted) {
  Text parts[8];
  parts[0] = site;
  parts[1] = text_of_cstring(" ");
  parts[2] = what;
  parts[3] = text_of_cstring(": the argument gives ");
  parts[4] = check_ops_pp(c, given);
  parts[5] = text_of_cstring(" where ");
  parts[6] = check_ops_pp(c, wanted);
  parts[7] = text_of_cstring(" is required");
  return error_mismatch(cat(parts, 8));
}

/* equal (line 55). */
static int equal(const CheckCtx *c, const Value *a, const Value *b,
                 const Values *ps, Text site, Text what, const Values **out,
                 const KError **error) {
  Bool yes;
  if (!conv_conv_type(c, a, b, &yes, error)) return 0;
  if (yes) {
    *out = ps;
    return 1;
  }
  *error = mismatch(c, site, what, b, a);
  return 0;
}

/* assign (line 58): the map_result recursion is a loop over the prefix
   plus a reverse onto the new tail. */
static int assign(const CheckCtx *c, const Values *ps, Int63 i, const Value *v,
                  Text site, const Values **out, const KError **error) {
  const Values *prefix = NULL;
  const Values *tail;
  for (;;) {
    if (ps == NULL) {
      *error = error_cannot_infer(
          text_of_cstring("a constructor parameter has no slot"));
      return 0;
    }
    if (int63_equal(i, int63_zero())) break;
    prefix = Values_cons(ps->head, prefix);
    ps = ps->tail;
    i = int63_pred(i);
  }
  if (ps->head == NULL) {
    tail = Values_cons(v, ps->tail);
  } else if (!equal(c, ps->head, v, Values_cons(ps->head, ps->tail), site,
                    text_of_cstring("gives inconsistent parameters"), &tail,
                    error)) {
    return 0;
  }
  *out = Values_reverse_onto(prefix, tail);
  return 1;
}

/* nominal (line 66): the parameter environment holds the last parameter
   first. The parameters are solved in declaration order. */
static int nominal(const CheckCtx *c, const Closure *d, const Values **out,
                   const KError **error) {
  const Values *xs;
  if (!rules_mu_param_env(ctx_of(c), d, &xs, error)) return 0;
  *out = surface_constructor_reverse(xs, NULL);
  return 1;
}

/* negative (line 68): a variable with a negative index. */
static Bool negative(const Term *tm) {
  if (tm->tag == TERM_VAR &&
      !order_le(int63_compare(int63_zero(), tm->as.var.index)))
    return YES;
  return NO;
}

/* open_term (line 76, @unsafe): a loop over the work list of every
   subterm of one quoted type. */
static Bool open_term(const Terms *terms) {
  while (terms != NULL) {
    const Term *tm = terms->head;
    if (negative(tm)) return YES;
    terms = term_scan_term(NO, NULL, tm, terms->tail).pending;
  }
  return NO;
}

/* has_hole (line 81). */
static int has_hole(const CheckCtx *c, const Value *v, Bool *out,
                    const KError **error) {
  const Term *tm;
  if (!eval_quote(c->globals, c->size, v, &tm, error)) return 0;
  *out = open_term(Terms_cons(tm, NULL));
  return 1;
}

/* One F.Pair2{a, b} of constraints. */
static SurfaceConstructorInferPair pair_of(const Value *a, const Value *b) {
  SurfaceConstructorInferPair pair;
  pair.a = a;
  pair.b = b;
  return pair;
}

/* zip (line 84): a right fold, so a loop plus a reverse. */
static int zip(Text site, const Values *xs, const Values *ys,
               const SurfaceConstructorInferPairs **out,
               const KError **error) {
  const SurfaceConstructorInferPairs *acc = NULL;
  for (; xs != NULL && ys != NULL; xs = xs->tail, ys = ys->tail)
    acc = SurfaceConstructorInferPairs_cons(pair_of(xs->head, ys->head), acc);
  if (xs != NULL || ys != NULL) {
    *error = error_mismatch(text_append(
        site,
        text_of_cstring(": constructor field types have different arities")));
    return 0;
  }
  *out = SurfaceConstructorInferPairs_reverse(acc);
  return 1;
}

/* family_error (line 95). */
static const KError *family_error(const CheckCtx *c, Text site, Text n,
                                  const Value *b) {
  Text parts[5];
  parts[0] = site;
  parts[1] = text_of_cstring(" needs the family ");
  parts[2] = n;
  parts[3] = text_of_cstring(" and the argument has type ");
  parts[4] = check_ops_pp(c, b);
  return error_mismatch(cat(parts, 5));
}

/* solve_family (line 97). */
static int solve_family(const CheckCtx *c, Text n, const Values *ix,
                        const Closure *d, const Value *b, const Values *ps,
                        Text site, const Values **ps_out,
                        const SurfaceConstructorInferPairs **pairs,
                        const KError **error) {
  const Values *xs;
  const Values *ys;
  if (b->tag != VALUE_VLAN || b->as.vlan.shape->tag != SHAPE_SMU ||
      !text_same(n, b->as.vlan.shape->as.smu.family)) {
    *error = family_error(c, site, n, b);
    return 0;
  }
  if (!nominal(c, d, &xs, error)) return 0;
  if (!nominal(c, b->as.vlan.diagram, &ys, error)) return 0;
  if (!zip(site, Values_append(xs, ix),
           Values_append(ys, b->as.vlan.shape->as.smu.indices), pairs, error))
    return 0;
  *ps_out = ps;
  return 1;
}

/* solve_nominal (line 106). */
static int solve_nominal(const CheckCtx *c, const Value *a, const Value *b,
                         const Values *ps, Text site, const Values **ps_out,
                         const SurfaceConstructorInferPairs **pairs,
                         const KError **error) {
  Bool yes;
  if (a->tag == VALUE_VLAN && a->as.vlan.shape->tag == SHAPE_SMU)
    return solve_family(c, a->as.vlan.shape->as.smu.family,
                        a->as.vlan.shape->as.smu.indices, a->as.vlan.diagram,
                        b, ps, site, ps_out, pairs, error);
  if (!has_hole(c, a, &yes, error)) return 0;
  *pairs = NULL;
  if (yes) {
    *ps_out = ps;
    return 1;
  }
  return equal(c, a, b, ps, site,
               text_of_cstring("does not match its field type"), ps_out,
               error);
}

/* solve_step (line 113). */
static int solve_step(const CheckCtx *c, Int63 base, Int63 np, const Value *a,
                      const Value *b, const Values *ps, Text site,
                      const Values **ps_out,
                      const SurfaceConstructorInferPairs **pairs,
                      const KError **error) {
  const Value *w;
  const Value *z;
  Int63 i;
  if (!eval_whnf(c->globals, a, &w, error)) return 0;
  if (parameter(base, np, w, &i)) {
    *pairs = NULL;
    return assign(c, ps, i, b, site, ps_out, error);
  }
  if (!eval_whnf(c->globals, b, &z, error)) return 0;
  return solve_nominal(c, w, z, ps, site, ps_out, pairs, error);
}

/* solve (line 128, @unsafe): a loop over the work list of constraints;
   join (line 118) is SurfaceConstructorInferPairs_append. */
static int solve(const CheckCtx *c, Int63 base, Int63 np,
                 const SurfaceConstructorInferPairs *pairs, const Values *ps,
                 Text site, const Values **out, const KError **error) {
  while (pairs != NULL) {
    const SurfaceConstructorInferPairs *more;
    if (!solve_step(c, base, np, pairs->head.a, pairs->head.b, ps, site, &ps,
                    &more, error))
      return 0;
    pairs = SurfaceConstructorInferPairs_append(more, pairs->tail);
  }
  *out = ps;
  return 1;
}

/* located (line 136). */
static const KError *located(Text name, Text k, const KError *error) {
  Text parts[7];
  if (error->tag != ERROR_CANNOT_INFER) return error;
  parts[0] = text_of_cstring("the constructor ");
  parts[1] = name;
  parts[2] = text_of_cstring(
      " needs an expected type or an inferable field: field ");
  parts[3] = k;
  parts[4] = text_of_cstring(" has no inferable type (");
  parts[5] = error->as.plain.message;
  parts[6] = text_of_cstring(")");
  return error_cannot_infer(cat(parts, 7));
}

/* field (line 149). A field type with a slot is not used as an expected
   type: the argument is inferred and its type is solved. inferable (line
   141, @unsafe) is the located rewrite of *error after a go or check_infer
   refusal. */
static int field(const void *walk_env, SurfaceElabWalk go, const CheckCtx *c,
                 Text name, Text k, Int63 base, Int63 np, const Values *ps,
                 const Value *dom, const SurfaceTerm *arg,
                 const Values **ps_out, const Term **tm_out,
                 const KError **error) {
  Bool yes;
  const Value *actual;
  if (!has_hole(c, dom, &yes, error)) return 0;
  if (!yes) {
    *ps_out = ps;
    return go(walk_env, c, dom, arg, tm_out, error);
  }
  if (!go(walk_env, c, NULL, arg, tm_out, error)) {
    *error = located(name, k, *error);
    return 0;
  }
  if (!check_infer(c, QUANTITY_ZERO, *tm_out, &actual, error)) {
    *error = located(name, k, *error);
    return 0;
  }
  return solve(c, base, np,
               SurfaceConstructorInferPairs_cons(pair_of(dom, actual), NULL),
               ps, field_site(name, k), ps_out, error);
}

/* fields (line 159): a self tail call in the last continuation, so a loop.
   With no field left, excess arguments stay (as the Bend []-row). */
static int fields(const void *walk_env, SurfaceElabWalk go, const CheckCtx *c,
                  Text name, Int63 np, const Telescope *tele, Int63 k,
                  const SurfaceTerms *args, State st, State *out,
                  const KError **error) {
  for (; tele != NULL; tele = tele->tail) {
    const Value *dom;
    const Values *next;
    const Term *tm;
    const Value *v;
    if (args == NULL) {
      *error = error_mismatch(
          text_of_cstring("a constructor has missing fields"));
      return 0;
    }
    if (!eval_eval(c->globals,
                   Values_append(st.values,
                                 environment(st.parameters, slot_base(),
                                             int63_zero(), NULL)),
                   tele->head.binding.type, &dom, error))
      return 0;
    if (!field(walk_env, go, c, name, int63_show(int63_succ(k)), slot_base(),
               np, st.parameters, dom, args->head, &next, &tm, error))
      return 0;
    if (!surface_constructor_eval_in(c, tm, &v, error)) return 0;
    st.parameters = next;
    st.values = Values_cons(v, st.values);
    st.terms = Terms_cons(tm, st.terms);
    k = int63_succ(k);
    args = args->tail;
  }
  *out = st;
  return 1;
}

/* binder (line 171). */
static Text binder(const Telescope *tele) {
  return tele == NULL ? text_of_cstring("?") : tele->head.binding.name;
}

/* resolved (line 179): a right fold, so a loop plus a reverse. The first
   None{} slot in declaration order gives the refusal. after (line 175) is
   the tail step of the telescope. */
static int resolved(Text name, const Values *ps, const Telescope *tele,
                    const Values **out, const KError **error) {
  const Values *acc = NULL;
  for (; ps != NULL; ps = ps->tail) {
    if (ps->head == NULL) {
      Text parts[4];
      parts[0] = text_of_cstring("the constructor ");
      parts[1] = name;
      parts[2] = text_of_cstring(
          " needs an expected type or an inferable field for the parameter ");
      parts[3] = binder(tele);
      *error = error_cannot_infer(cat(parts, 4));
      return 0;
    }
    acc = Values_cons(ps->head, acc);
    tele = tele == NULL ? NULL : tele->tail;
  }
  *out = Values_reverse(acc);
  return 1;
}

/* quote (line 186): a right fold, so a loop plus a reverse. */
static int quote(const CheckCtx *c, const Values *vs, const Terms **out,
                 const KError **error) {
  const Terms *acc = NULL;
  for (; vs != NULL; vs = vs->tail) {
    const Term *tm;
    if (!eval_quote(c->globals, c->size, vs->head, &tm, error)) return 0;
    acc = Terms_cons(tm, acc);
  }
  *out = Terms_reverse(acc);
  return 1;
}

/* indices (line 192): a right fold, so a loop plus a reverse. */
static int indices(const CheckCtx *c, const Values *env, const Terms *ix,
                   const Values **out, const KError **error) {
  const Values *acc = NULL;
  for (; ix != NULL; ix = ix->tail) {
    const Value *v;
    if (!eval_eval(c->globals, env, ix->head, &v, error)) return 0;
    acc = Values_cons(v, acc);
  }
  *out = Values_reverse(acc);
  return 1;
}

/* legs (line 198): a right fold with no effect, so a loop over the
   reversed list. */
static const TermLegs *legs(const Terms *ts) {
  const TermLegs *acc = NULL;
  for (ts = Terms_reverse(ts); ts != NULL; ts = ts->tail)
    acc = TermLegs_cons(term_leg(NULL, ts->head), acc);
  return acc;
}

/* finish (line 206). terms (line 202) is Terms_reverse_onto. */
static int finish(const CheckCtx *c, Text family, Text name, Int63 np,
                  const Telescope *params, const Terms *ix, State st,
                  const Term **out, const KError **error) {
  const Values *pv;
  const Terms *pt;
  const Values *iv;
  const Terms *it;
  if (!resolved(name, st.parameters, params, &pv, error)) return 0;
  if (!quote(c, pv, &pt, error)) return 0;
  if (!indices(c,
               Values_append(st.values, surface_constructor_reverse(pv, NULL)),
               ix, &iv, error))
    return 0;
  if (!quote(c, iv, &it, error)) return 0;
  *out = term_ann(
      term_in(shape_smu_term(family, it), term_actor(name),
              Terms_reverse_onto(st.terms, NULL)),
      term_lan(shape_smu_term(family, it),
               term_sec(shape_scoll_term(np), legs(pt))));
  return 1;
}

/* infer (line 214). */
int surface_constructor_infer_infer(const void *walk_env, SurfaceElabWalk go,
                                    const CheckCtx *c, const Family *fam,
                                    const Ctor *ctor,
                                    const SurfaceTerms *args,
                                    const Term **out, const KError **error) {
  State st;
  st.parameters = empty(fam->params);
  st.values = NULL;
  st.terms = NULL;
  if (!fields(walk_env, go, c, ctor->name, count(fam->params), ctor->args,
              int63_zero(), args, st, &st, error))
    return 0;
  return finish(c, fam->name, ctor->name, count(fam->params), fam->params,
                ctor->res_idx, st, out, error);
}
