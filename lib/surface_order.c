/* lib/surface_order.c: twin of surface/order.bend (S5 session 2).
   Structural certificates and translation at Kanon 69f3be51 lib/order.ml.
   R.result(A) is an int return (1 = Done with *out, 0 = Fail with *error).
   A Maybe<A> answer is an int return (1 = Some with *out, 0 = None{}). maybe
   and choose (lines 34 and 38) have no C twin: they are C if statements.
   The @unsafe defs peel, spine and walk are loops. Each right fold is a
   loop plus a reverse. */
#include "sole.h"

/* status, List<S.t<T.t>>, peeled and job: SurfaceOrderStatus(es),
   SurfaceOrderShapes, SurfaceOrderPeeled(s) and SurfaceOrderJob(s) in
   sole.h. */

/* work = Work{calls, jobs}. */
typedef struct {
  const SurfaceOrderCalls *calls;
  const SurfaceOrderJobs *jobs;
} OrderWork;

/* spine_view = Spine{head, args, shapes}. */
typedef struct {
  const Term *head;
  const Terms *args;
  const SurfaceOrderShapes *shapes;
} OrderSpine;

static const char *const one_elim_msg =
    "a structurally recursive definition eliminates its recursive argument "
    "at the head of its body";
static const char *const motive_msg =
    "the elimination of a structurally recursive definition carries its "
    "motive";
static const char *const ctor_key_msg =
    "every branch of the elimination of a structurally recursive definition "
    "keys a constructor";

/* index (line 33). */
static Int63 index_of(Nat n) { return int63_raw(bignum_from_nat(n)); }

static SurfaceOrderJob visit(const SurfaceOrderStatuses *st, const SurfaceOrderSteps *chain,
                      const Term *tm) {
  SurfaceOrderJob job;
  job.statuses = st;
  job.chain = chain;
  job.term = tm;
  return job;
}

static OrderWork work_of(const SurfaceOrderCalls *cs, const SurfaceOrderJobs *js) {
  OrderWork w;
  w.calls = cs;
  w.jobs = js;
  return w;
}

/* mentions (line 42). */
Bool surface_order_mentions(const Texts *group, const Term *tm) {
  return term_exists_name(NO, group, tm);
}

/* as_var (line 43): 1 = Some with *out. */
static int as_var(const Term *tm, Int63 *out) {
  switch (tm->tag) {
    case TERM_VAR: *out = tm->as.var.index; return 1;
    case TERM_UNIV: case TERM_LAN: case TERM_RAN: case TERM_IN:
    case TERM_ELIM: case TERM_SEC: case TERM_OUT: case TERM_LET:
    case TERM_ANN: case TERM_GLOBAL: case TERM_LIT: case TERM_AUTO:
      return 0;
  }
  return 0;
}

/* lambda_leg (line 58): leg NULL = None{}; the answer NULL = None{}. */
static const Term *lambda_leg(const TermLeg *leg) {
  if (leg == NULL) return NULL;
  if (rules_one_of_binder(leg->binders) == NULL) return NULL;
  return leg->body;
}

/* as_lam (line 63): NULL = None{}. */
static const Term *as_lam(const Term *tm) {
  switch (tm->tag) {
    case TERM_SEC:
      if (shape_point_dom_term(tm->as.sec.shape) == NULL) return NULL;
      return lambda_leg(rules_one_of_leg(tm->as.sec.legs));
    case TERM_VAR: case TERM_UNIV: case TERM_LAN: case TERM_RAN:
    case TERM_IN: case TERM_ELIM: case TERM_OUT: case TERM_LET:
    case TERM_ANN: case TERM_GLOBAL: case TERM_LIT: case TERM_AUTO:
      return NULL;
  }
  return NULL;
}

/* peel (@unsafe, line 81): the view returns the strict body subterm of a
   single-binder section. A loop. */
static SurfaceOrderPeeled peel(Text name, Nat n, const Term *tm) {
  SurfaceOrderPeeled p;
  const Term *body;
  while ((body = as_lam(tm)) != NULL) {
    n = 1 + n;
    tm = body;
  }
  p.name = name;
  p.formals = n;
  p.body = tm;
  return p;
}

/* spine (@unsafe, line 85): each point projection removes one Out node
   from the application spine. A loop. The Bend code rebuilds the stopped
   node with the same fields; the twin keeps the same pointer. */
static OrderSpine spine(const Term *tm, const Terms *args,
                        const SurfaceOrderShapes *shapes) {
  OrderSpine view;
  for (;;) {
    switch (tm->tag) {
      case TERM_OUT:
        switch (tm->as.out.address->tag) {
          case TERM_APT:
            args = Terms_cons(tm->as.out.address->as.apt.arg, args);
            shapes = SurfaceOrderShapes_cons(tm->as.out.shape, shapes);
            tm = tm->as.out.head;
            continue;
          case TERM_ALEG: case TERM_ACTOR: break;
        }
        break;
      case TERM_VAR: case TERM_UNIV: case TERM_LAN: case TERM_RAN:
      case TERM_IN: case TERM_ELIM: case TERM_SEC: case TERM_LET:
      case TERM_ANN: case TERM_GLOBAL: case TERM_LIT: case TERM_AUTO:
        break;
    }
    view.head = tm;
    view.args = args;
    view.shapes = shapes;
    return view;
  }
}

/* under (line 104) at F.Pair2<Q.t, String> and at String: one s per item. */
static const SurfaceOrderStatuses *under_binders(SurfaceOrderStatus s,
                                          const TermBinders *bs,
                                          const SurfaceOrderStatuses *st) {
  for (; bs != NULL; bs = bs->tail) st = SurfaceOrderStatuses_cons(s, st);
  return st;
}

static const SurfaceOrderStatuses *under_texts(SurfaceOrderStatus s, const Texts *bs,
                                        const SurfaceOrderStatuses *st) {
  for (; bs != NULL; bs = bs->tail) st = SurfaceOrderStatuses_cons(s, st);
  return st;
}

/* under_point (line 108). */
static const SurfaceOrderStatuses *under_point(const TermShape *s,
                                        const SurfaceOrderStatuses *st) {
  if (shape_point_dom_term(s) == NULL) return st;
  return SurfaceOrderStatuses_cons(SURFACE_ORDER_OTHER, st);
}

/* R.at (kernel_rules line 258) at status and at T.t: 1 = Some. */
static int status_at(Int63 index, const SurfaceOrderStatuses *xs, SurfaceOrderStatus *out) {
  for (; xs != NULL; xs = xs->tail) {
    if (int63_equal(index, int63_zero())) {
      *out = xs->head;
      return 1;
    }
    index = int63_pred(index);
  }
  return 0;
}

static const Term *term_at(Int63 index, const Terms *xs) {
  for (; xs != NULL; xs = xs->tail) {
    if (int63_equal(index, int63_zero())) return xs->head;
    index = int63_pred(index);
  }
  return NULL;
}

/* smaller and special (lines 110 and 116) of R.at(status, ix, st). */
static Bool smaller(Int63 ix, const SurfaceOrderStatuses *st) {
  SurfaceOrderStatus s;
  if (!status_at(ix, st, &s)) return NO;
  switch (s) {
    case SURFACE_ORDER_SMALLER: return YES;
    case SURFACE_ORDER_PRINCIPAL: return NO;
    case SURFACE_ORDER_OTHER: return NO;
  }
  return NO;
}

static Bool special(Int63 ix, const SurfaceOrderStatuses *st) {
  SurfaceOrderStatus s;
  if (!status_at(ix, st, &s)) return NO;
  switch (s) {
    case SURFACE_ORDER_SMALLER: return YES;
    case SURFACE_ORDER_PRINCIPAL: return YES;
    case SURFACE_ORDER_OTHER: return NO;
  }
  return NO;
}

/* guarded_call (line 122). */
static Bool guarded_call(Int63 k, const SurfaceOrderStatuses *st, const Terms *args) {
  const Term *tm = term_at(k, args);
  Int63 ix;
  if (tm == NULL) return NO;
  if (!as_var(tm, &ix)) return NO;
  return smaller(ix, st);
}

/* terms (line 125). */
static const SurfaceOrderJobs *terms(const SurfaceOrderStatuses *st,
                              const SurfaceOrderSteps *chain,
                              const Terms *ts) {
  const SurfaceOrderJobs *acc = NULL;
  for (; ts != NULL; ts = ts->tail)
    acc = SurfaceOrderJobs_cons(visit(st, chain, ts->head), acc);
  return SurfaceOrderJobs_reverse(acc);
}

/* shape (line 129). */
static const SurfaceOrderJobs *shape(const SurfaceOrderStatuses *st,
                              const SurfaceOrderSteps *chain,
                              const TermShape *s) {
  return terms(st, chain, shape_payload_term(s));
}

/* shapes (line 130): List.append of each shape, in order. */
static const SurfaceOrderJobs *shapes(const SurfaceOrderStatuses *st,
                               const SurfaceOrderSteps *chain,
                               const SurfaceOrderShapes *ss) {
  const SurfaceOrderJobs *acc = NULL;
  for (; ss != NULL; ss = ss->tail)
    acc = SurfaceOrderJobs_reverse_onto(shape(st, chain, ss->head), acc);
  return SurfaceOrderJobs_reverse(acc);
}

/* address (line 134). */
static const SurfaceOrderJobs *address(const SurfaceOrderStatuses *st,
                                const SurfaceOrderSteps *chain,
                                const TermAddr *a) {
  switch (a->tag) {
    case TERM_APT: return SurfaceOrderJobs_cons(visit(st, chain, a->as.apt.arg), NULL);
    case TERM_ALEG: return NULL;
    case TERM_ACTOR: return NULL;
  }
  return NULL;
}

/* legs (line 139). */
static const SurfaceOrderJobs *legs(const SurfaceOrderStatuses *st,
                             const SurfaceOrderSteps *chain,
                             const TermLegs *ls) {
  const SurfaceOrderJobs *acc = NULL;
  for (; ls != NULL; ls = ls->tail)
    acc = SurfaceOrderJobs_cons(
        visit(under_binders(SURFACE_ORDER_OTHER, ls->head->binders, st), chain,
              ls->head->body),
        acc);
  return SurfaceOrderJobs_reverse(acc);
}

/* motive (line 144): mo NULL = None{}. */
static const SurfaceOrderJobs *motive(const SurfaceOrderStatuses *st,
                               const SurfaceOrderSteps *chain,
                               const TermMotive *mo) {
  if (mo == NULL) return NULL;
  return SurfaceOrderJobs_cons(
      visit(under_texts(SURFACE_ORDER_OTHER, mo->idx,
                        SurfaceOrderStatuses_cons(SURFACE_ORDER_OTHER, st)),
            chain, mo->body),
      NULL);
}

/* chain_at (line 148): has_sv = 0 is None{}. */
static const SurfaceOrderSteps *chain_at(const SurfaceOrderStatuses *st,
                                         const SurfaceOrderSteps *chain,
                                         int has_sv, Int63 ix,
                                         const TermAddr *a) {
  SurfaceOrderStep step;
  if (!has_sv) return chain;
  if (!special(ix, st)) return chain;
  step.scrut = ix;
  step.ctor = term_as_actor(a);
  return SurfaceOrderSteps_cons(step, chain);
}

/* branch_status (line 152). */
static SurfaceOrderStatus branch_status(const SurfaceOrderStatuses *st, int has_sv,
                                 Int63 ix) {
  if (!has_sv) return SURFACE_ORDER_OTHER;
  return special(ix, st) ? SURFACE_ORDER_SMALLER : SURFACE_ORDER_OTHER;
}

/* branches (line 156). */
static const SurfaceOrderJobs *branches(const SurfaceOrderStatuses *st,
                                 const SurfaceOrderSteps *chain, int has_sv,
                                 Int63 ix, const TermBranches *bs) {
  const SurfaceOrderJobs *acc = NULL;
  for (; bs != NULL; bs = bs->tail) {
    const TermAddr *a = bs->head.address;
    const TermLeg *leg = bs->head.leg;
    acc = SurfaceOrderJobs_reverse_onto(address(st, chain, a), acc);
    acc = SurfaceOrderJobs_cons(
        visit(under_binders(branch_status(st, has_sv, ix), leg->binders, st),
              chain_at(st, chain, has_sv, ix, a), leg->body),
        acc);
  }
  return SurfaceOrderJobs_reverse(acc);
}

/* projection (line 161). */
static const SurfaceOrderJobs *projection(const SurfaceOrderStatuses *st,
                                   const SurfaceOrderSteps *chain,
                                   const TermShape *s, const TermAddr *a,
                                   const Term *hd) {
  return SurfaceOrderJobs_append(
      shape(st, chain, s),
      SurfaceOrderJobs_append(address(st, chain, a),
                       SurfaceOrderJobs_cons(visit(st, chain, hd), NULL)));
}

/* spine_head (line 163): 1 = Some with *out. The Bend code rebuilds the
   head node of a one-job answer with the same fields; the twin keeps the
   same pointer. */
static int spine_head(const Texts *group, Int63 k, const SurfaceOrderStatuses *st,
                      const SurfaceOrderSteps *chain, const Terms *args,
                      const Term *hd, OrderWork *out) {
  switch (hd->tag) {
    case TERM_GLOBAL:
      if (term_member(hd->as.global.name, group)) {
        SurfaceOrderCall c;
        if (!guarded_call(k, st, args)) return 0;
        c.callee = hd->as.global.name;
        c.chain = chain;
        *out = work_of(SurfaceOrderCalls_cons(c, NULL), NULL);
        return 1;
      }
      *out = work_of(NULL, NULL);
      return 1;
    case TERM_VAR: case TERM_UNIV: case TERM_LIT: case TERM_AUTO:
      *out = work_of(NULL, NULL);
      return 1;
    case TERM_OUT:
      *out = work_of(NULL, projection(st, chain, hd->as.out.shape,
                                      hd->as.out.address, hd->as.out.head));
      return 1;
    case TERM_LAN: case TERM_RAN: case TERM_IN: case TERM_ELIM:
    case TERM_SEC: case TERM_LET: case TERM_ANN:
      *out = work_of(NULL, SurfaceOrderJobs_cons(visit(st, chain, hd), NULL));
      return 1;
  }
  return 0;
}

/* spine_jobs and extend (lines 180 and 184). */
static int spine_jobs(const Texts *group, Int63 k, const SurfaceOrderStatuses *st,
                      const SurfaceOrderSteps *chain, OrderSpine view,
                      OrderWork *out) {
  OrderWork w;
  if (!spine_head(group, k, st, chain, view.args, view.head, &w)) return 0;
  *out = work_of(w.calls,
                 SurfaceOrderJobs_append(w.jobs,
                                  SurfaceOrderJobs_append(shapes(st, chain,
                                                          view.shapes),
                                                   terms(st, chain,
                                                         view.args))));
  return 1;
}

/* node (line 187): 1 = Some with *out. */
static int node(const Texts *group, Int63 k, SurfaceOrderJob task, OrderWork *out) {
  const SurfaceOrderStatuses *st = task.statuses;
  const SurfaceOrderSteps *chain = task.chain;
  const Term *tm = task.term;
  switch (tm->tag) {
    case TERM_VAR: case TERM_UNIV: case TERM_LIT: case TERM_AUTO:
      *out = work_of(NULL, NULL);
      return 1;
    case TERM_GLOBAL:
      if (term_member(tm->as.global.name, group)) return 0;
      *out = work_of(NULL, NULL);
      return 1;
    case TERM_LAN:
      *out = work_of(NULL,
                     SurfaceOrderJobs_append(
                         shape(st, chain, tm->as.lan.shape),
                         SurfaceOrderJobs_cons(
                             visit(under_point(tm->as.lan.shape, st), chain,
                                   tm->as.lan.diagram),
                             NULL)));
      return 1;
    case TERM_RAN:
      *out = work_of(NULL,
                     SurfaceOrderJobs_append(
                         shape(st, chain, tm->as.ran.shape),
                         SurfaceOrderJobs_cons(
                             visit(under_point(tm->as.ran.shape, st), chain,
                                   tm->as.ran.diagram),
                             NULL)));
      return 1;
    case TERM_IN:
      *out = work_of(NULL,
                     SurfaceOrderJobs_append(
                         shape(st, chain, tm->as.in.shape),
                         SurfaceOrderJobs_append(address(st, chain, tm->as.in.address),
                                          terms(st, chain, tm->as.in.args))));
      return 1;
    case TERM_SEC:
      *out = work_of(NULL, SurfaceOrderJobs_append(shape(st, chain, tm->as.sec.shape),
                                            legs(st, chain, tm->as.sec.legs)));
      return 1;
    case TERM_OUT:
      return spine_jobs(group, k, st, chain, spine(tm, NULL, NULL), out);
    case TERM_ELIM: {
      const TermElim *e = tm->as.elim.elim;
      Int63 ix = int63_zero();
      int has_sv = as_var(e->scrut, &ix);
      *out = work_of(
          NULL,
          SurfaceOrderJobs_cons(
              visit(st, chain, e->scrut),
              SurfaceOrderJobs_append(
                  shape(st, chain, e->shape),
                  SurfaceOrderJobs_append(motive(st, chain, e->motive),
                                   branches(st, chain, has_sv, ix,
                                            e->branches)))));
      return 1;
    }
    case TERM_LET:
      *out = work_of(
          NULL,
          SurfaceOrderJobs_cons(
              visit(st, chain, tm->as.let.ty),
              SurfaceOrderJobs_cons(
                  visit(st, chain, tm->as.let.value),
                  SurfaceOrderJobs_cons(
                      visit(SurfaceOrderStatuses_cons(SURFACE_ORDER_OTHER, st), chain,
                            tm->as.let.body),
                      NULL))));
      return 1;
    case TERM_ANN:
      *out = work_of(NULL,
                     SurfaceOrderJobs_cons(visit(st, chain, tm->as.ann.term),
                                    SurfaceOrderJobs_cons(visit(st, chain,
                                                         tm->as.ann.ty),
                                                   NULL)));
      return 1;
  }
  return 0;
}

/* walk and next_work (@unsafe, lines 206 and 210): each expansion visits
   strict subterms. A stopped non-point Out is expanded once. A loop; 1 =
   Some with *out. */
static int walk(const Texts *group, Int63 k, OrderWork state,
                const SurfaceOrderCalls **out) {
  while (state.jobs != NULL) {
    OrderWork next;
    if (!node(group, k, state.jobs->head, &next)) return 0;
    state = work_of(SurfaceOrderCalls_append(state.calls, next.calls),
                    SurfaceOrderJobs_append(next.jobs, state.jobs->tail));
  }
  *out = state.calls;
  return 1;
}

/* seed (line 215): a right fold, so a loop plus a reverse. */
static const SurfaceOrderStatuses *seed(Nat n, Int63 principal, Int63 ix) {
  const SurfaceOrderStatuses *acc = NULL;
  for (; n > 0; n--) {
    acc = SurfaceOrderStatuses_cons(
        int63_equal(ix, principal) ? SURFACE_ORDER_PRINCIPAL : SURFACE_ORDER_OTHER, acc);
    ix = int63_succ(ix);
  }
  return SurfaceOrderStatuses_reverse(acc);
}

/* passes (line 219): 1 = Some with *out. */
static int passes(SurfaceOrderRule rule, const Texts *group, Int63 k,
                  Nat formals, const Term *body,
                  const SurfaceOrderCalls **out) {
  switch (rule) {
    case SURFACE_ORDER_STRUCTURAL: {
      const SurfaceOrderStatuses *st = seed(
          formals, int63_sub(int63_pred(index_of(formals)), k), int63_zero());
      return walk(group, k,
                  work_of(NULL, SurfaceOrderJobs_cons(visit(st, NULL, body), NULL)),
                  out);
    }
  }
  return 0;
}

/* names (line 222). */
static const Texts *names(const SurfaceOrderMembers *ms) {
  const Texts *acc = NULL;
  for (; ms != NULL; ms = ms->tail) acc = Texts_cons(ms->head.name, acc);
  return Texts_reverse(acc);
}

/* peel_members (line 226). */
static const SurfaceOrderPeeleds *peel_members(const SurfaceOrderMembers *ms) {
  const SurfaceOrderPeeleds *acc = NULL;
  for (; ms != NULL; ms = ms->tail)
    acc = SurfaceOrderPeeleds_cons(peel(ms->head.name, 0, ms->head.body), acc);
  return SurfaceOrderPeeleds_reverse(acc);
}

/* any_calls (line 230): choose is lazy, so the loop stops at a match. */
static Bool any_calls(const Texts *group, const SurfaceOrderMembers *ms) {
  for (; ms != NULL; ms = ms->tail)
    if (surface_order_mentions(group, ms->head.body)) return YES;
  return NO;
}

/* bound and minimum (lines 235 and 239): Maybe<Nat> is (have, least). */
static Nat bound(const Texts *group, const SurfaceOrderPeeleds *ms) {
  int have = 0;
  Nat least = 0;
  for (; ms != NULL; ms = ms->tail) {
    if (!surface_order_mentions(group, ms->head.body)) continue;
    if (!have || ms->head.formals < least) least = ms->head.formals;
    have = 1;
  }
  return have ? least : 0;
}

/* rows_at (line 247): 1 = Some with *out. The first None{} stops it. */
static int rows_at(SurfaceOrderRule rule, const Texts *group, Int63 k,
                   const SurfaceOrderPeeleds *ms, const SurfaceOrderRows **out) {
  const SurfaceOrderRows *acc = NULL;
  for (; ms != NULL; ms = ms->tail) {
    SurfaceOrderRow row;
    const SurfaceOrderCalls *cs;
    if (!passes(rule, group, k, ms->head.formals, ms->head.body, &cs))
      return 0;
    row.member = ms->head.name;
    row.calls = cs;
    acc = SurfaceOrderRows_cons(row, acc);
  }
  *out = SurfaceOrderRows_reverse(acc);
  return 1;
}

/* refusal (line 253). */
static const KError *refusal(const SurfaceOrderMembers *ms) {
  if (ms == NULL) return error_termination(text_of_cstring(""));
  return error_termination(ms->head.name);
}

/* first_fit (line 257): a loop over k. */
static int first_fit(Nat remaining, SurfaceOrderRule rule,
                     const Texts *group, const SurfaceOrderPeeleds *ms, Int63 k,
                     const KError *failure,
                     const SurfaceOrderCertificate **out,
                     const KError **error) {
  for (; remaining > 0; remaining--) {
    const SurfaceOrderRows *rs;
    if (rows_at(rule, group, k, ms, &rs)) {
      SurfaceOrderCertificate *c =
          (SurfaceOrderCertificate *)arena_alloc(sizeof *c);
      c->group = group;
      c->arg = k;
      c->rows = rs;
      *out = c;
      return 1;
    }
    k = int63_succ(k);
  }
  *error = failure;
  return 0;
}

/* certify_peeled, certify_named and certify (lines 262 to 267). */
int surface_order_certify(SurfaceOrderRule rule,
                          const SurfaceOrderMembers *ms,
                          const SurfaceOrderCertificate **out,
                          const KError **error) {
  const Texts *group = names(ms);
  const SurfaceOrderPeeleds *ps;
  if (!any_calls(group, ms)) {
    *out = NULL;
    return 1;
  }
  ps = peel_members(ms);
  return first_fit(bound(group, ps), rule, group, ps, int63_zero(),
                   refusal(ms), out, error);
}

/* ctor_keys (line 271). */
static int ctor_keys(const TermBranches *bs, const KError **error) {
  for (; bs != NULL; bs = bs->tail) {
    if (!term_as_actor(bs->head.address).some) {
      *error = error_wrong_leg(text_of_cstring(ctor_key_msg));
      return 0;
    }
  }
  return 1;
}

/* call_chains (line 276). */
static int call_chains(Text name, const SurfaceOrderCalls *cs,
                       const KError **error) {
  for (; cs != NULL; cs = cs->tail) {
    const SurfaceOrderSteps *chain = cs->head.chain;
    if (chain == NULL || !chain->head.ctor.some) {
      *error = error_termination(name);
      return 0;
    }
  }
  return 1;
}

/* row_chains (line 286). */
static int row_chains(const SurfaceOrderRows *rs, const KError **error) {
  for (; rs != NULL; rs = rs->tail)
    if (!call_chains(rs->head.member, rs->head.calls, error)) return 0;
  return 1;
}

/* translated_motive (line 290). */
static int translated_motive(const TermMotive *mo, const TermBranches *bs,
                             const SurfaceOrderRows *rs,
                             const KError **error) {
  if (mo == NULL) {
    *error = error_missing_branch(text_of_cstring(motive_msg));
    return 0;
  }
  if (!ctor_keys(bs, error)) return 0;
  return row_chains(rs, error);
}

static int mismatch(const KError **error) {
  *error = error_mismatch(text_of_cstring(one_elim_msg));
  return 0;
}

/* translated_elim (line 294). */
static int translated_elim(Int63 k, const SurfaceOrderRows *rs, Nat n,
                           const TermElim *e, const KError **error) {
  Int63 ix;
  if (!as_var(e->scrut, &ix)) return mismatch(error);
  if (!int63_equal(ix, int63_sub(int63_pred(index_of(n)), k)))
    return mismatch(error);
  return translated_motive(e->motive, e->branches, rs, error);
}

/* translated_inner (line 299). */
static int translated_inner(Int63 k, const SurfaceOrderRows *rs,
                            SurfaceOrderPeeled p, const KError **error) {
  switch (p.body->tag) {
    case TERM_ELIM:
      return translated_elim(k, rs, p.formals, p.body->as.elim.elim, error);
    case TERM_VAR: case TERM_UNIV: case TERM_LAN: case TERM_RAN:
    case TERM_IN: case TERM_SEC: case TERM_OUT: case TERM_LET:
    case TERM_ANN: case TERM_GLOBAL: case TERM_LIT: case TERM_AUTO:
      return mismatch(error);
  }
  return mismatch(error);
}

/* translate (line 316). */
int surface_order_translate(const SurfaceOrderCertificate *cert,
                            const Term *body, const Term **out,
                            const KError **error) {
  if (surface_order_mentions(cert->group, body)) {
    if (!translated_inner(cert->arg, cert->rows,
                          peel(text_of_cstring(""), 0, body), error))
      return 0;
  }
  *out = body;
  return 1;
}
