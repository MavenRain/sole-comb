/* lib/erase_sum.c: twin of erase/sum.bend (S4 session 2b).
   Finite sum plans from Kanon 69f3be51 lib/erase.ml: tag_intro, case_elim
   and branch_of. require (line 25) is a C if with error_mismatch
   (refuse). find_branch (@unsafe) is a loop. R.elim_result only lifts ops
   calls (no Request), so the C twin takes no host; result_of drops the
   fuel. */
#include "sole.h"

/* collection (line 15). */
typedef struct {
  Int63 width;
  const Closure *diagram;
} Collection;

/* The RulesCtx of the ops calls (as erase_type.c ctx_of). */
static RulesCtx ctx_of(const CheckCtx *c) {
  RulesCtx context;
  context.check = c;
  return context;
}

/* require (line 25): the None case. */
static int refuse(const char *message, const KError **error) {
  *error = error_mismatch(text_of_cstring(message));
  return 0;
}

/* R.at at V.t (the body of rules_at_vleg); NULL = None{}. */
static const Value *at_value(Int63 index, const Values *xs) {
  for (; xs != NULL; xs = xs->tail) {
    if (int63_equal(index, int63_zero())) return xs->head;
    index = int63_pred(index);
  }
  return NULL;
}

/* Ty.count at V.t. */
static Int63 count(const Values *xs) {
  Int63 n = int63_zero();
  for (; xs != NULL; xs = xs->tail) n = int63_succ(n);
  return n;
}

/* collection_former (line 29). */
static int collection_former(const ValueFormer *former, Collection *out,
                             const KError **error) {
  Int63 n;
  if (!rules_collection_view(former->shape, &n))
    return refuse("erasure needs a finite collection former", error);
  out->width = n;
  out->diagram = former->diagram;
  return 1;
}

/* collection_of (line 33). */
static int collection_of(const CheckCtx *c, const Value *ty,
                         Collection *out, const KError **error) {
  const Value *w;
  ValueFormer former;
  if (!eval_whnf(check_globals(c), ty, &w, error)) return 0;
  if (!value_as_lan(w, &former))
    return refuse("a sum stands at a type that is not a left former", error);
  return collection_former(&former, out, error);
}

/* legs_of (line 36). */
static int legs_of(const CheckCtx *c, const Collection *value,
                   const Values **out, const KError **error) {
  return erase_type_leg_types(c, value->width, value->diagram, out, error);
}

/* tag_payload (line 39). */
static int tag_payload(const CheckCtx *c, const Value *ty, Int63 index,
                       const Term *body, const Value *lty, EraseSumTag *out,
                       const KError **error) {
  Bool keep;
  Tid tid;
  if (!erase_type_runtime_ty(c, lty, &keep, error)) return 0;
  if (!erase_type_tid_of(c, ty, &tid, error)) return 0;
  out->tid = tid;
  out->index = index;
  out->payload = NULL;
  if (keep) {
    EraseSumPayload *p = (EraseSumPayload *)arena_alloc(sizeof *p);
    p->ty = lty;
    p->body = body;
    out->payload = p;
  }
  return 1;
}

/* injection (line 42). */
int erase_sum_injection(const CheckCtx *c, const Value *ty,
                        const TermAddr *address, const Terms *args,
                        EraseSumTag *out, const KError **error) {
  Int63 index;
  Collection coll;
  const Values *tys;
  const Value *lty;
  const Term *body;
  if (!term_as_aleg(address, &index))
    return refuse("an injection takes a leg address", error);
  if (!collection_of(c, ty, &coll, error)) return 0;
  if (!legs_of(c, &coll, &tys, error)) return 0;
  lty = at_value(index, tys);
  if (lty == NULL)
    return refuse("the leg address is outside the collection", error);
  body = rules_one_of_term(args);
  if (body == NULL) return refuse("an injection takes one payload", error);
  return tag_payload(c, ty, index, body, lty, out, error);
}

/* case_type (line 48). */
static int case_type(const CheckCtx *c, const Value *ty, EraseSumCase *out,
                     const KError **error) {
  Collection coll;
  const Values *tys;
  Tid tid;
  if (!collection_of(c, ty, &coll, error)) return 0;
  if (!legs_of(c, &coll, &tys, error)) return 0;
  if (!erase_type_tid_of(c, ty, &tid, error)) return 0;
  out->ty = ty;
  out->tid = tid;
  out->legs = tys;
  return 1;
}

/* case_of (line 54). */
int erase_sum_case_of(const CheckCtx *c, const Term *scrut, EraseSumCase *out,
                      const KError **error) {
  const Value *ty;
  if (!check_infer(c, QUANTITY_MANY, scrut, &ty, error)) return 0;
  return case_type(c, ty, out, error);
}

/* find_branch (line 56, @unsafe): the first branch at leg index. */
static int find_branch(Int63 index, const TermBranches *branches,
                       const TermLeg **out, const KError **error) {
  for (; branches != NULL; branches = branches->tail) {
    Int63 k;
    if (term_as_aleg(branches->head.address, &k) && int63_equal(k, index)) {
      *out = branches->head.leg;
      return 1;
    }
  }
  return refuse("a case is missing a branch", error);
}

/* branch_binder (line 63). */
static int branch_binder(const CheckCtx *c, const Value *ty,
                         const TermMotive *motive, const Values *tys,
                         Int63 index, const Value *lty, const Term *body,
                         const TermBinder *binder, EraseSumBranch *out,
                         const KError **error) {
  Bool keep;
  const Value *self;
  const Value *target;
  if (!erase_type_point_runtime(c, binder->quantity, lty, &keep, error))
    return 0;
  self = value_vin(shape_scoll_value(count(tys)), value_valeg(index),
                   Values_cons(value_var(c->size), NULL));
  if (!rules_elim_result(ctx_of(c), motive, ty, self, &target, error))
    return 0;
  out->quantity = binder->quantity;
  out->name = binder->name;
  out->domain = lty;
  out->keep = keep;
  out->target = target;
  out->body = body;
  return 1;
}

/* branch_leg (line 69). */
static int branch_leg(const CheckCtx *c, const Value *ty,
                      const TermMotive *motive, const Values *tys,
                      Int63 index, const Value *lty, const TermLeg *leg,
                      EraseSumBranch *out, const KError **error) {
  const TermBinder *binder = rules_one_of_binder(leg->binders);
  if (binder == NULL) return refuse("a case branch takes one binder", error);
  return branch_binder(c, ty, motive, tys, index, lty, leg->body, binder,
                       out, error);
}

/* branch_of (line 73). */
int erase_sum_branch_of(const CheckCtx *c, const Value *ty,
                        const TermMotive *motive, const Values *tys,
                        Int63 index, const TermBranches *branches,
                        EraseSumBranch *out, const KError **error) {
  const Value *lty = at_value(index, tys);
  const TermLeg *leg;
  if (lty == NULL)
    return refuse("a leg number is outside the collection", error);
  if (!find_branch(index, branches, &leg, error)) return 0;
  return branch_leg(c, ty, motive, tys, index, lty, leg, out, error);
}
