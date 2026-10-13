/* kernel_recursor.c: C twin of lib/kernel_recursor.bend (stage S3,
   session 2). Direct recursive-field layouts: metadata, not a recursor
   certificate.

   C = K.ctx, so each context argument is a RulesCtx (ctx.check). The
   check_ops record (op) has no C twin (kernel_rules.c, c_runtime): c_eval,
   c_bind, c_define and c_size are the rules_c_* calls, and o_quote, o_whnf,
   o_env and o_families are the runtime_ops_* calls on ctx.check. A program
   takes the host first and returns int (1 = Pure with *out, 0 = Abort with
   *error). Self tail calls are loops; the other recursion stays. */
#include "sole.h"

/* String.concat of a fixed list of parts (the cat idiom of kernel_rules). */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

int recursor_abstract(const RulesHost *host, const Telescope *tele,
                      RulesCtx c, RulesCtx *out, const KError **error) {
  for (; tele != NULL; tele = tele->tail) {
    const Value *tyv;
    if (!rules_c_eval(c, tele->head.binding.type, &tyv, error)) return 0;
    c = rules_c_bind(c, tele->head.binding.name, tele->head.quantity, tyv);
  }
  *out = c;
  return 1;
}

int recursor_instance(const RulesHost *host, const Telescope *tele,
                      const Values *values, RulesCtx c, RulesCtx *out,
                      const KError **error) {
  for (; tele != NULL && values != NULL;
       tele = tele->tail, values = values->tail) {
    const Value *tyv;
    if (!rules_c_eval(c, tele->head.binding.type, &tyv, error)) return 0;
    c = rules_c_define(c, tele->head.binding.name, QUANTITY_ZERO, tyv,
                       values->head);
  }
  if (tele != NULL || values != NULL) {
    *error = error_mismatch(rules_wrong_pack());
    return 0;
  }
  *out = c;
  return 1;
}

int recursor_open(const RulesHost *host, RecursorScope scope,
                  const Telescope *tele, RulesCtx c, RulesCtx *out,
                  const KError **error) {
  switch (scope.tag) {
  case RECURSOR_ABSTRACT: return recursor_abstract(host, tele, c, out, error);
  case RECURSOR_INSTANCE:
    return recursor_instance(host, tele, scope.values, c, out, error);
  }
  return 0; /* each tag returns above */
}

int recursor_parameters(const RulesHost *host, const Telescope *tele,
                        RulesCtx c, RulesCtx *out, const KError **error) {
  RecursorScope scope;
  scope.tag = RECURSOR_ABSTRACT;
  scope.values = NULL;
  return recursor_open(host, scope, tele, c, out, error);
}

RulesCtx recursor_install(RecursorScope scope, Text x, Quantity q,
                          const Value *tyv, const Value *v, RulesCtx c) {
  switch (scope.tag) {
  case RECURSOR_ABSTRACT: return rules_c_bind(c, x, q, tyv);
  case RECURSOR_INSTANCE:
    return rules_c_define(c, x, QUANTITY_ZERO, tyv, v);
  }
  return c; /* each tag returns above */
}

const KError *recursor_refusal(Text owner, const Term *tm) {
  return error_not_yet(text_of_cstring(
      positivity_occurs(Texts_cons(owner, NULL), tm)
          ? "recursor layout supports direct recursive fields only"
          : "recursor layout does not support mutual families"));
}

int recursor_ordinary(const RulesHost *host, RulesCtx c, Text owner,
                      const Texts *group, const Value *ty, RecursorRole *out,
                      const KError **error) {
  const Term *tm;
  if (!runtime_ops_quote(c.check, ty, &tm, error)) return 0;
  if (positivity_occurs(group, tm)) {
    *error = recursor_refusal(owner, tm);
    return 0;
  }
  out->tag = RECURSOR_ORDINARY;
  out->indices = NULL;
  return 1;
}

int recursor_family_role(const RulesHost *host, RulesCtx c, Text owner,
                         const Texts *group, const Value *ty, Bool found,
                         Text name, const Values *indices, RecursorRole *out,
                         const KError **error) {
  if (found && text_same(owner, name)) {
    out->tag = RECURSOR_DIRECT;
    out->indices = indices;
    return 1;
  }
  return recursor_ordinary(host, c, owner, group, ty, out, error);
}

int recursor_former_role(const RulesHost *host, RulesCtx c, Text owner,
                         const Texts *group, const Value *ty,
                         const ValueFormer *view, RecursorRole *out,
                         const KError **error) {
  Text name = owner;
  const Values *indices = NULL;
  Bool found;
  if (view == NULL)
    return recursor_ordinary(host, c, owner, group, ty, out, error);
  found = rules_as_mu_value(view->shape, &name, &indices) ? YES : NO;
  return recursor_family_role(host, c, owner, group, ty, found, name,
                              indices, out, error);
}

RecursorPlan recursor_prepend(const RecursorField *field,
                              RecursorPlan value) {
  value.fields = RecursorFields_cons(field, value.fields);
  return value;
}

/* Not a tail call (pmap after the recursion): the recursion stays. */
int recursor_fields(const RulesHost *host, Text owner, const Texts *group,
                    const Telescope *tele, RulesCtx c, Int63 position,
                    RecursorPlan *out, const KError **error) {
  const Value *tyv;
  const Value *w;
  ValueFormer former;
  RecursorRole role;
  RecursorPlan tail;
  RecursorField *field;
  if (tele == NULL) {
    out->context = c;
    out->fields = NULL;
    return 1;
  }
  if (!rules_c_eval(c, tele->head.binding.type, &tyv, error)) return 0;
  if (!runtime_ops_whnf(c.check, tyv, &w, error)) return 0;
  if (!recursor_former_role(host, c, owner, group, w,
                            value_as_lan(w, &former) ? &former : NULL, &role,
                            error))
    return 0;
  if (!recursor_fields(host, owner, group, tele->tail,
                       rules_c_bind(c, tele->head.binding.name,
                                    tele->head.quantity, w),
                       int63_succ(position), &tail, error))
    return 0;
  field = (RecursorField *)arena_alloc(sizeof *field);
  field->position = position;
  field->quantity = tele->head.quantity;
  field->name = tele->head.binding.name;
  field->domain = w;
  field->value = value_var(rules_c_size(c));
  field->role = role;
  *out = recursor_prepend(field, tail);
  return 1;
}

int recursor_constructor(const RulesHost *host, RulesCtx c,
                         RecursorScope scope, Text owner, const Texts *group,
                         const Telescope *params, const Ctor *value,
                         RecursorPlan *out, const KError **error) {
  RulesCtx fresh;
  if (!recursor_open(host, scope, params, c, &fresh, error)) return 0;
  return recursor_fields(host, owner, group, value->args, fresh, int63_zero(),
                         out, error);
}

int recursor_family(const RulesHost *host, RulesCtx c, RecursorScope scope,
                    Text owner, const Texts *group, Text key,
                    const Family *value, RecursorPlan *out,
                    const KError **error) {
  const Ctor *ct = positivity_ctor_of(key, value);
  if (ct == NULL) {
    Text parts[3];
    parts[0] = key;
    parts[1] = text_of_cstring(" is not a constructor of ");
    parts[2] = owner;
    *error = error_unbound(cat(parts, 3));
    return 0;
  }
  return recursor_constructor(host, c, scope, owner, group, value->params, ct,
                              out, error);
}

int recursor_layout(const RulesHost *host, RulesCtx c, RecursorScope scope,
                    Text owner, Text key, RecursorPlan *out,
                    const KError **error) {
  const Family *fam;
  if (!rules_mu_family(c, owner, &fam, error)) return 0;
  return recursor_family(
      host, c, scope, owner,
      recursor_group_members(runtime_ops_families(c.check), owner), key, fam,
      out, error);
}

/* The Ordinary arm is a self tail call: a loop, then a reverse. */
const RecursorFields *recursor_hypotheses(const RecursorFields *fs) {
  const RecursorFields *acc = NULL;
  for (; fs != NULL; fs = fs->tail) {
    switch (fs->head->role.tag) {
    case RECURSOR_ORDINARY: break;
    case RECURSOR_DIRECT: acc = RecursorFields_cons(fs->head, acc); break;
    }
  }
  return RecursorFields_reverse(acc);
}

Int63 recursor_arity(RecursorPlan value) {
  return int63_add(
      rules_advance(int63_zero(), RecursorFields_length(value.fields)),
      rules_advance(int63_zero(),
                    RecursorFields_length(recursor_hypotheses(value.fields))));
}
