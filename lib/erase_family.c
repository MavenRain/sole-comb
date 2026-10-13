/* lib/erase_family.c: twin of erase/family.bend (S4 session 2).
   Nominal family layouts from Kanon 69f3be51 lib/erase.ml: mu_layout and
   mu_group_tids. A layout (List<Maybe<K.repr>>) is a MaybeReprs: a NULL
   head is None{}. The right folds (kept, leg_reprs, group_loop) are left
   loops with a reverse at the end; parameters is a tail call (a loop). */
#include "sole.h"

/* String.concat of a fixed list of parts (the cat idiom of kernel_rules). */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

/* not_ctor (line 14). */
const KError *erase_family_not_ctor(Text key, Text name) {
  Text parts[3];
  parts[0] = key;
  parts[1] = text_of_cstring(" is not a constructor of ");
  parts[2] = name;
  return error_unbound(cat(parts, 3));
}

/* kept (line 16): the Some entries, in order. */
static const Reprs *kept(const MaybeReprs *fields) {
  const Reprs *acc = NULL;
  for (; fields != NULL; fields = fields->tail)
    if (fields->head != NULL) acc = Reprs_cons(fields->head, acc);
  return Reprs_reverse(acc);
}

/* leg_tid (line 21). */
static Tid leg_tid(Text name, Int63 index, const Reprs *fields) {
  Text parts[6];
  Tid t;
  parts[0] = text_of_cstring("leg<");
  parts[1] = eterm_tid_text(erase_type_mu_tid(name));
  parts[2] = text_of_cstring(",");
  parts[3] = int63_show(index);
  if (fields == NULL)
    parts[4] = text_of_cstring("");
  else
    parts[4] = text_append(text_of_cstring(","),
                           eterm_join(text_of_cstring(","),
                                      eterm_repr_texts(fields)));
  parts[5] = text_of_cstring(">");
  t.name = cat(parts, 6);
  return t;
}

/* parameters (line 24): binds each parameter in c. */
static int parameters(const Telescope *tele, const CheckCtx *c,
                      const CheckCtx **out, const KError **error) {
  for (; tele != NULL; tele = tele->tail) {
    const Value *tyv;
    if (!eval_eval(check_globals(c), c->env, tele->head.binding.type, &tyv,
                   error))
      return 0;
    c = runtime_ops_bind(tele->head.binding.name, tele->head.quantity, tyv,
                         c);
  }
  *out = c;
  return 1;
}

/* field_repr (line 29): *out = NULL is None{}. */
static int field_repr(const CheckCtx *c, const Value *ty, Bool keep,
                      const Repr **out, const KError **error) {
  if (!keep) {
    *out = NULL;
    return 1;
  }
  return erase_type_repr_of(c, ty, out, error);
}

/* leg_reprs (line 32). The new variable is V.var(C.size(c)) of the context
   before the bind. */
static int leg_reprs(const Telescope *tele, const CheckCtx *c,
                     const Values *env, const MaybeReprs **out,
                     const KError **error) {
  const MaybeReprs *acc = NULL;
  for (; tele != NULL; tele = tele->tail) {
    Quantity q = tele->head.quantity;
    const Value *tyv;
    const Value *v;
    const Repr *here;
    Bool keep;
    if (!eval_eval(check_globals(c), env, tele->head.binding.type, &tyv,
                   error))
      return 0;
    if (!erase_type_point_runtime(c, q, tyv, &keep, error)) return 0;
    if (!field_repr(c, tyv, keep, &here, error)) return 0;
    acc = MaybeReprs_cons(here, acc);
    v = value_var(c->size);
    c = runtime_ops_bind(tele->head.binding.name, q, tyv, c);
    env = Values_cons(v, env);
  }
  *out = MaybeReprs_reverse(acc);
  return 1;
}

/* layout (line 42): the parameters bind in a fresh context. */
int erase_family_layout(const CheckCtx *c, const Family *fam,
                        const Ctor *ctor, const MaybeReprs **out,
                        const KError **error) {
  const CheckCtx *fresh;
  if (!parameters(fam->params, check_make(check_globals(c), check_budget(c)),
                  &fresh, error))
    return 0;
  return leg_reprs(ctor->args, fresh, fresh->env, out, error);
}

/* group_loop (line 49). */
static int group_loop(const Texts *xs, const CheckCtx *c, Text name,
                      const Family *fam, Int63 index, const Tids **out,
                      const KError **error) {
  const Tids *acc = NULL;
  for (; xs != NULL; xs = xs->tail) {
    const Ctor *ctor = positivity_ctor_of(xs->head, fam);
    const MaybeReprs *fields;
    if (ctor == NULL) {
      *error = erase_family_not_ctor(xs->head, name);
      return 0;
    }
    if (!erase_family_layout(c, fam, ctor, &fields, error)) return 0;
    acc = Tids_cons(leg_tid(name, index, kept(fields)), acc);
    index = int63_succ(index);
  }
  *out = Tids_reverse(acc);
  return 1;
}

/* group_tids (line 58): R.mu_ctor_names only lifts ops calls (no Request),
   so the C twin takes no host; result_of drops the fuel. */
int erase_family_group_tids(const CheckCtx *c, Text name, const Family *fam,
                            const Tids **out, const KError **error) {
  const Texts *xs;
  if (!rules_mu_ctor_names(name, fam, &xs, error)) return 0;
  return group_loop(xs, c, name, fam, int63_zero(), out, error);
}
