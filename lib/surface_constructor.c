/* lib/surface_constructor.c: twin of surface/constructor.bend (S5 session
   3b). M.result(A) is the int return (1 = Pure with *out, 0 = Abort with
   *error); a Unit result has no out. ~go is the (walk_env, go) pair of
   SurfaceElabWalk; go(c, e, s) is a direct call. M.bind and M.map_result
   are C if statements and M.lift is the identity. choose is a C if (no
   twin). */
#include "sole.h"

/* Syn.cat: String.concat of a fixed list of parts. */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

/* reverse (line 20). */
const Values *surface_constructor_reverse(const Values *xs,
                                          const Values *acc) {
  return Values_reverse_onto(xs, acc);
}

/* eval_in (line 24). */
int surface_constructor_eval_in(const CheckCtx *c, const Term *tm,
                                const Value **out, const KError **error) {
  return eval_eval(c->globals, c->env, tm, out, error);
}

/* values (line 25): a right fold, so a loop plus a reverse. The terms
   evaluate in list order, so the first refusal is the same. */
static int values(const CheckCtx *c, const Terms *terms, const Values **out,
                  const KError **error) {
  const Values *acc = NULL;
  for (; terms != NULL; terms = terms->tail) {
    const Value *v;
    if (!surface_constructor_eval_in(c, terms->head, &v, error)) return 0;
    acc = Values_cons(v, acc);
  }
  *out = Values_reverse(acc);
  return 1;
}

/* same_family (line 32): the family-name check of both paths. */
static int same_family(Text family, Text name, Text n, const KError **error) {
  if (text_same(n, family)) return 1;
  {
    Text parts[6];
    parts[0] = text_of_cstring("the constructor ");
    parts[1] = name;
    parts[2] = text_of_cstring(" of ");
    parts[3] = family;
    parts[4] = text_of_cstring(" cannot have the expected family ");
    parts[5] = n;
    *error = error_mismatch(cat(parts, 6));
  }
  return 0;
}

/* The T.Lan{S.SMu{n, ix}, diagram} pattern of parameters and owner_shape. */
static Bool smu_lan(const Term *tm) {
  return (tm->tag == TERM_LAN && tm->as.lan.shape->tag == SHAPE_SMU) ? YES
                                                                     : NO;
}

/* parameters (line 35): the F.Pair2 answer is the two out parameters
   params and indices. */
static int parameters(const CheckCtx *c, Text family, Text name,
                      const Value *ty, const Term *tm, const Terms **params,
                      const Terms **indices, const KError **error) {
  if (smu_lan(tm)) {
    if (!same_family(family, name, tm->as.lan.shape->as.smu.family, error))
      return 0;
    if (!rules_mu_params_of(tm->as.lan.diagram, params, error)) return 0;
    *indices = tm->as.lan.shape->as.smu.indices;
    return 1;
  }
  {
    Text parts[8];
    parts[0] = text_of_cstring("the constructor ");
    parts[1] = name;
    parts[2] = text_of_cstring(" of ");
    parts[3] = family;
    parts[4] = text_of_cstring(" needs the family ");
    parts[5] = family;
    parts[6] = text_of_cstring(
        " as its expected type, but the expected type is ");
    parts[7] = check_ops_pp(c, ty);
    *error = error_mismatch(cat(parts, 8));
  }
  return 0;
}

/* next_env (line 43): only a later field type reads a field value, so the
   last field is not evaluated. */
static int next_env(const CheckCtx *c, const Telescope *rest, const Term *tm,
                    const Values *env, const Values **out,
                    const KError **error) {
  const Value *value;
  if (rest == NULL) {
    *out = env;
    return 1;
  }
  if (!surface_constructor_eval_in(c, tm, &value, error)) return 0;
  *out = Values_cons(value, env);
  return 1;
}

/* arguments (line 49): a right fold, so a loop plus a reverse. Each field
   evaluates its type, walks its argument and extends the environment
   before the next field, as in the Bend recursion. */
int surface_constructor_arguments(const void *walk_env, SurfaceElabWalk go,
                                  const Telescope *fields, const CheckCtx *c,
                                  const Values *env,
                                  const SurfaceTerms *args, const Terms **out,
                                  const KError **error) {
  const Terms *acc = NULL;
  for (; fields != NULL; fields = fields->tail) {
    const Value *dom;
    const Term *tm;
    if (args == NULL) {
      *error = error_mismatch(
          text_of_cstring("a constructor has missing fields"));
      return 0;
    }
    if (!eval_eval(c->globals, env, fields->head.binding.type, &dom, error))
      return 0;
    if (!go(walk_env, c, dom, args->head, &tm, error)) return 0;
    if (!next_env(c, fields->tail, tm, env, &env, error)) return 0;
    acc = Terms_cons(tm, acc);
    args = args->tail;
  }
  if (args != NULL) {
    *error = error_mismatch(text_of_cstring("a constructor has excess fields"));
    return 0;
  }
  *out = Terms_reverse(acc);
  return 1;
}

/* finish (line 63): pt and ix are the F.Pair2 of parameters. */
static int finish(const void *walk_env, SurfaceElabWalk go, const CheckCtx *c,
                  Text family, Text name, const Telescope *fields,
                  const SurfaceTerms *args, const Terms *pt, const Terms *ix,
                  const Term **out, const KError **error) {
  const Values *vs;
  const Terms *ts;
  if (!values(c, pt, &vs, error)) return 0;
  if (!surface_constructor_arguments(walk_env, go, fields, c,
                                     surface_constructor_reverse(vs, NULL),
                                     args, &ts, error))
    return 0;
  *out = term_in(shape_smu_term(family, ix), term_actor(name), ts);
  return 1;
}

/* index_terms (line 69): index terms read the field values, so a family
   without indices evaluates no field again. */
int surface_constructor_index_terms(const Terms *ix, const void *run_env,
                                    SurfaceConstructorIndexRun run,
                                    const Terms **out, const KError **error) {
  if (ix == NULL) {
    *out = NULL;
    return 1;
  }
  return run(run_env, ix, out, error);
}

/* whnf_target (line 73). */
static int whnf_target(const CheckCtx *c, const Value *ty, const Term **out,
                       const KError **error) {
  const Value *w;
  if (!eval_whnf(c->globals, ty, &w, error)) return 0;
  return eval_quote(c->globals, c->size, w, out, error);
}

/* expected (line 76): the caller checks the field count (elab.bend
   arity_is) before both paths. */
int surface_constructor_expected(const void *walk_env, SurfaceElabWalk go,
                                 const CheckCtx *c, const Family *fam,
                                 const Ctor *ctor, const SurfaceTerms *args,
                                 const Value *ty, const Term **out,
                                 const KError **error) {
  const Term *tm;
  const Terms *params;
  const Terms *indices;
  if (!whnf_target(c, ty, &tm, error)) return 0;
  if (!parameters(c, fam->name, ctor->name, ty, tm, &params, &indices, error))
    return 0;
  return finish(walk_env, go, c, fam->name, ctor->name, ctor->args, args,
                params, indices, out, error);
}

/* owner_shape (line 82). */
static int owner_shape(Text family, Text name, const Term *tm,
                       const KError **error) {
  if (smu_lan(tm))
    return same_family(family, name, tm->as.lan.shape->as.smu.family, error);
  return 1;
}

/* owner (line 87): a parameterless constructor with an expected family gets
   the same name check. A Unit result: no out. */
int surface_constructor_owner(const CheckCtx *c, Text family, Text name,
                              const Value *ty, const KError **error) {
  const Term *tm;
  if (!whnf_target(c, ty, &tm, error)) return 0;
  return owner_shape(family, name, tm, error);
}
