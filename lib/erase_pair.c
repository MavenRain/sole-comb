/* lib/erase_pair.c: twin of erase/pair.bend (S4 session 2b).
   Dependent-pair plans from Kanon 69f3be51 lib/erase.ml: pair_intro and
   pair_elim. require (line 20) is a C if with error_mismatch (refuse).
   The R programs only lift ops calls (no Request), so the C twins take no
   host; result_of drops the fuel. */
#include "sole.h"

/* The RulesCtx of the ops calls (as erase_type.c ctx_of). */
static RulesCtx ctx_of(const CheckCtx *c) {
  RulesCtx context;
  context.check = c;
  return context;
}

/* require (line 20): the None case. */
static int refuse(const char *message, const KError **error) {
  *error = error_mismatch(text_of_cstring(message));
  return 0;
}

/* point_of (line 23). */
static int point_of(const CheckCtx *c, const Value *ty, EraseArrow *out,
                    const KError **error) {
  const Value *w;
  ValueFormer former;
  if (!eval_whnf(check_globals(c), ty, &w, error)) return 0;
  if (!value_as_lan(w, &former))
    return refuse("a pair stands at a type that is not a left former", error);
  if (!erase_type_former_arrow(&former, out))
    return refuse("a pair needs a point former", error);
  return 1;
}

/* fields (line 27): the point field, then the fibre field. */
static int fields(const CheckCtx *c, const EraseArrow *point, const Term *p,
                  const Term *fibre, const ErasePairFields **out,
                  const KError **error) {
  Bool keep1;
  Bool keep2;
  const CheckCtx *c1;
  const Value *cod;
  const Value *pv;
  const Value *fty;
  const ErasePairFields *xs = NULL;
  ErasePairField f;
  if (!erase_type_point_runtime(c, point->quantity, point->domain, &keep1,
                                error))
    return 0;
  if (!erase_type_under(c, point->quantity, point->name, point->domain,
                        point->diagram, &c1, &cod, error))
    return 0;
  if (!erase_type_runtime_ty(c1, cod, &keep2, error)) return 0;
  if (!eval_eval(check_globals(c), c->env, p, &pv, error)) return 0;
  if (!erase_type_opened(c, point->diagram, Values_cons(pv, NULL), &fty,
                         error))
    return 0;
  if (keep2) {
    f.ty = fty;
    f.term = fibre;
    xs = ErasePairFields_cons(f, xs);
  }
  if (keep1) {
    f.ty = point->domain;
    f.term = p;
    xs = ErasePairFields_cons(f, xs);
  }
  *out = xs;
  return 1;
}

/* introduction (line 37). */
int erase_pair_introduction(const CheckCtx *c, const Value *ty,
                            const TermAddr *address, const Terms *args,
                            const ErasePairFields **out,
                            const KError **error) {
  TermPoint pt;
  const Term *fibre;
  EraseArrow point;
  if (!term_as_apt(address, &pt))
    return refuse("a pair takes a point address", error);
  fibre = rules_one_of_term(args);
  if (fibre == NULL) return refuse("a pair takes one fibre", error);
  if (!point_of(c, ty, &point, error)) return 0;
  return fields(c, &point, pt.arg, fibre, out, error);
}

/* mapped_shape (line 42): R.run with EV.dispatch, so an evaluation host on
   the stack (no fuel), as erase_type_opened. */
static int mapped_shape(const CheckCtx *c, const TermShape *shape,
                        const ValueShape **out, const KError **error) {
  RulesHost host;
  RulesCtx ctx;
  host.kind = RULES_HOST_EVAL;
  host.fuel = NULL;
  ctx.globals = check_globals(c);
  return eval_eval_shape(&host, ctx, c->env, shape, out, error);
}

/* branch_target (line 44). */
static int branch_target(const CheckCtx *c, const Value *ty,
                         const TermShape *shape, const TermMotive *motive,
                         Quantity q, const Value **out,
                         const KError **error) {
  const ValueShape *sv;
  const Value *self;
  if (!mapped_shape(c, shape, &sv, error)) return 0;
  self = value_vin(sv, value_vapt(q, value_var(c->size)),
                   Values_cons(value_var(int63_succ(c->size)), NULL));
  return rules_elim_result(ctx_of(c), motive, ty, self, out, error);
}

/* branch_point (line 47). */
static int branch_point(const CheckCtx *c, const Value *sty,
                        const Value *ty, const TermShape *shape,
                        const TermMotive *motive, const Term *body,
                        Text second, const EraseArrow *point,
                        ErasePairBranch *out, const KError **error) {
  Bool keep1;
  Bool keep2;
  const CheckCtx *c1;
  const Value *cod;
  const Value *target;
  Tid tid;
  if (!erase_type_point_runtime(c, point->quantity, point->domain, &keep1,
                                error))
    return 0;
  if (!erase_type_under(c, point->quantity, point->name, point->domain,
                        point->diagram, &c1, &cod, error))
    return 0;
  if (!erase_type_runtime_ty(c1, cod, &keep2, error)) return 0;
  if (!branch_target(c, ty, shape, motive, point->quantity, &target, error))
    return 0;
  if (!erase_type_tid_of(c, sty, &tid, error)) return 0;
  out->scrut_type = sty;
  out->tid = tid;
  out->quantity = point->quantity;
  out->first_name = point->name;
  out->domain = point->domain;
  out->keep_first = keep1;
  out->second_name = second;
  out->fibre = cod;
  out->keep_second = keep2;
  out->target = target;
  out->body = body;
  return 1;
}

/* branch_leg (line 56): the name of the second binder. */
static int branch_leg(const CheckCtx *c, const Value *sty, const Value *ty,
                      const TermShape *shape, const TermMotive *motive,
                      const TermLeg *leg, const EraseArrow *point,
                      ErasePairBranch *out, const KError **error) {
  TermBinder first;
  TermBinder second;
  if (!rules_two_of_binders(leg->binders, &first, &second))
    return refuse("a pair branch takes two binders", error);
  return branch_point(c, sty, ty, shape, motive, leg->body, second.name,
                      point, out, error);
}

/* elimination (line 62). */
int erase_pair_elimination(const CheckCtx *c, const Value *ty,
                           const TermShape *shape, const Term *scrut,
                           const TermMotive *motive,
                           const TermBranches *branches, ErasePairBranch *out,
                           const KError **error) {
  const Value *sty;
  EraseArrow point;
  const TermBranch *br;
  if (!check_infer(c, QUANTITY_MANY, scrut, &sty, error)) return 0;
  if (!point_of(c, sty, &point, error)) return 0;
  br = rules_one_of_branch(branches);
  if (br == NULL)
    return refuse("a pair elimination takes one branch", error);
  return branch_leg(c, sty, ty, shape, motive, br->leg, &point, out, error);
}
