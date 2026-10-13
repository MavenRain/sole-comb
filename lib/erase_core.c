/* lib/erase_core.c: twin of erase/core.bend (S4 session 3).
   Bounded ordinary-definition erasure from Kanon 69f3be51 lib/erase.ml.
   The Bend computation is the int return (1 = Pure with *out, 0 = Abort
   with *error). A Request (lines 337, 339, 341) is a direct call of
   term_step, chain_step or jobs_step: C recursion, the depth is the nesting
   of the term. drive (line 685) spends no fuel at a Request, thus no host
   is necessary. bind, map_result, lift_result, the three output views,
   task, output and dispatch have no C twin. The fuel is spent only by tick
   (line 342). Self tail calls and right folds are loops. */
#include "sole.h"

/* application (line 52). */
typedef struct {
  const Term *head;
  const Terms *args;
} Application;

/* found (line 34). */
typedef enum { LOOKUP_FOUND, LOOKUP_GONE, LOOKUP_MISSING } Found;

/* String.concat of a fixed list of parts (the cat idiom of kernel_rules). */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

/* An Abort with E.Mismatch. */
static int refuse(const char *message, const KError **error) {
  *error = error_mismatch(text_of_cstring(message));
  return 0;
}

/* count (line 48) at K.repr: a right fold of succ is a left loop. */
static Int63 count_reprs(const Reprs *xs) {
  Int63 n = int63_zero();
  for (; xs != NULL; xs = xs->tail) n = int63_succ(n);
  return n;
}

/* count (line 48) at the binders of a leg. */
static Int63 count_binders(const TermBinders *xs) {
  Int63 n = int63_zero();
  for (; xs != NULL; xs = xs->tail) n = int63_succ(n);
  return n;
}

/* point_application (line 54): 1 = Some{F.Pair2{head, arg}}. */
static int point_application(const TermShape *shape, const TermAddr *address,
                             const Term *head, const Term **fn,
                             const Term **arg) {
  TermPoint point;
  switch (shape->tag) {
    case SHAPE_SPI:
      if (!term_as_apt(address, &point)) return 0;
      *fn = head;
      *arg = point.arg;
      return 1;
    case SHAPE_SCOLL:
    case SHAPE_SPAR:
    case SHAPE_SMU:
    case SHAPE_SNU:
      break;
  }
  return 0;
}

/* var_view (line 76): 1 = Some{ix}. */
static int var_view(const Term *tm, Int63 *ix) {
  switch (tm->tag) {
    case TERM_VAR:
      *ix = tm->as.var.index;
      return 1;
    case TERM_UNIV:
    case TERM_LAN:
    case TERM_RAN:
    case TERM_IN:
    case TERM_ELIM:
    case TERM_SEC:
    case TERM_OUT:
    case TERM_LET:
    case TERM_ANN:
    case TERM_GLOBAL:
    case TERM_LIT:
    case TERM_AUTO:
      break;
  }
  return 0;
}

/* application_view (line 138): 1 = Some{F.Pair2{head, arg}}. */
static int application_view(const Term *tm, const Term **fn,
                            const Term **arg) {
  switch (tm->tag) {
    case TERM_OUT:
      return point_application(tm->as.out.shape, tm->as.out.address,
                               tm->as.out.head, fn, arg);
    case TERM_VAR:
    case TERM_UNIV:
    case TERM_LAN:
    case TERM_RAN:
    case TERM_IN:
    case TERM_ELIM:
    case TERM_SEC:
    case TERM_LET:
    case TERM_ANN:
    case TERM_GLOBAL:
    case TERM_LIT:
    case TERM_AUTO:
      break;
  }
  return 0;
}

/* eta_vars (line 94, @unsafe): a right fold, thus a loop and a reverse. */
static const KTms *eta_vars(Int63 n, Int63 i) {
  const KTms *rev = NULL;
  for (; erase_runtime_lt(i, n) == YES; i = int63_succ(i))
    rev = KTms_cons(eterm_kvar(int63_pred(int63_sub(n, i))), rev);
  return KTms_reverse(rev);
}

/* repr_tids (line 98), one repr: push its tid onto a reversed list. */
static const Tids *repr_tid(const Repr *r, const Tids *rev) {
  switch (r->tag) {
    case REPR_RI31:
      return rev;
    case REPR_RSTRUCT:
    case REPR_RUNION:
    case REPR_RFUNC:
    case REPR_RTHUNK:
      break;
  }
  return Tids_cons(r->name, rev);
}

/* Push each tid of ts onto a reversed list. */
static const Tids *push_tids(const Tids *ts, const Tids *rev) {
  for (; ts != NULL; ts = ts->tail) rev = Tids_cons(ts->head, rev);
  return rev;
}

/* has_tid (line 106). */
static Bool has_tid(Tid tid, const Tids *ts) {
  for (; ts != NULL; ts = ts->tail)
    if (text_same(eterm_tid_text(tid), eterm_tid_text(ts->head)) == YES)
      return YES;
  return NO;
}

/* print_decls (line 110): String.concat is associative, thus a left loop. */
static Text print_decls(const KDecls *ds) {
  Text acc = text_of_cstring("");
  for (; ds != NULL; ds = ds->tail)
    acc = text_append(text_append(acc, eterm_print_decl(ds->head)),
                      text_of_cstring("\n"));
  return acc;
}

/* lookup (line 126, @unsafe): a loop. *index is set for LOOKUP_FOUND. */
static Found lookup(Int63 ix, const EraseCoreSlots *slots, Int63 seen,
                    Int63 *index) {
  for (; slots != NULL; slots = slots->tail) {
    switch (slots->head) {
      case ERASE_CORE_SLOT_EXTRA:
        seen = int63_succ(seen);
        break;
      case ERASE_CORE_SLOT_KEEP:
        if (int63_equal(ix, int63_zero()) == YES) {
          *index = seen;
          return LOOKUP_FOUND;
        }
        ix = int63_pred(ix);
        seen = int63_succ(seen);
        break;
      case ERASE_CORE_SLOT_DROP:
        if (int63_equal(ix, int63_zero()) == YES) return LOOKUP_GONE;
        ix = int63_pred(ix);
        break;
    }
  }
  return LOOKUP_MISSING;
}

/* free_legs (line 155), one leg. */
static EraseCoreTodo leg_todo(Int63 depth, const TermLeg *leg) {
  return erase_core_todo(int63_add(depth, count_binders(leg->binders)),
                         leg->body);
}

/* framed (line 159): a right fold, thus a loop and a reverse. */
static const EraseCoreSlots *framed(const EraseCoreSlots *slots,
                                    const Int63s *caps, Int63 ix) {
  const EraseCoreSlots *rev = NULL;
  for (; slots != NULL; slots = slots->tail) {
    switch (slots->head) {
      case ERASE_CORE_SLOT_EXTRA:
        break;
      case ERASE_CORE_SLOT_DROP:
        rev = EraseCoreSlots_cons(ERASE_CORE_SLOT_DROP, rev);
        ix = int63_succ(ix);
        break;
      case ERASE_CORE_SLOT_KEEP:
        rev = EraseCoreSlots_cons(erase_runtime_member(ix, caps) == YES
                                      ? ERASE_CORE_SLOT_KEEP
                                      : ERASE_CORE_SLOT_DROP,
                                  rev);
        ix = int63_succ(ix);
        break;
    }
  }
  return EraseCoreSlots_reverse(rev);
}

/* lambda_body (line 170): NULL = None{}. */
static const Term *lambda_body(const Term *tm) {
  const TermShape *shape;
  const TermLegs *legs;
  const TermLeg *leg;
  if (!rules_as_tsec(tm, &shape, &legs)) return NULL;
  switch (shape->tag) {
    case SHAPE_SPI:
      leg = rules_one_of_leg(legs);
      return leg == NULL ? NULL : leg->body;
    case SHAPE_SCOLL:
    case SHAPE_SPAR:
    case SHAPE_SMU:
    case SHAPE_SNU:
      break;
  }
  return NULL;
}

/* decl_tids (line 183): the List.append chain in one reversed list. */
static const Tids *decl_tids(const KDecls *ds) {
  const Tids *rev = NULL;
  const Reprs *ps;
  for (; ds != NULL; ds = ds->tail) {
    const KDecl *d = ds->head;
    switch (d->tag) {
      case KDECL_KFUN:
        for (ps = d->as.kfun.params; ps != NULL; ps = ps->tail)
          rev = repr_tid(ps->head, rev);
        rev = repr_tid(d->as.kfun.result, rev);
        rev = push_tids(erase_dependencies_term(d->as.kfun.body), rev);
        break;
      case KDECL_KREC:
        rev = push_tids(d->as.krec.names, rev);
        break;
    }
  }
  return Tids_reverse(rev);
}

/* unique_tids (line 188): acc is kept reversed (has_tid ignores order). */
static const Tids *unique_tids(const Tids *ts, const Tids *acc) {
  const Tids *rev = Tids_reverse(acc);
  for (; ts != NULL; ts = ts->tail)
    if (has_tid(ts->head, rev) == NO) rev = Tids_cons(ts->head, rev);
  return Tids_reverse(rev);
}

/* print_entry (line 196). */
static Text print_entry(Text name, EraseCoreEntry en) {
  Text parts[5];
  switch (en.tag) {
    case ERASE_CORE_DROPPED:
      parts[0] = text_of_cstring("erased ");
      parts[1] = name;
      parts[2] = text_of_cstring("\n");
      return cat(parts, 3);
    case ERASE_CORE_POSTULATE:
      parts[0] = text_of_cstring("axiom ");
      parts[1] = name;
      parts[2] = text_of_cstring(" : ");
      parts[3] = eterm_print_repr(en.repr);
      parts[4] = text_of_cstring("\n");
      return cat(parts, 5);
    case ERASE_CORE_CODE:
      break;
  }
  return print_decls(en.decls);
}

/* added (line 210). C.bind is runtime_ops_bind. */
static EraseCoreCtx added(EraseCoreCtx c, Quantity q, Text x,
                          const Value *dom, Bool keep) {
  EraseCoreCtx inner;
  inner.check = runtime_ops_bind(x, q, dom, c.check);
  inner.slots = EraseCoreSlots_cons(
      keep == YES ? ERASE_CORE_SLOT_KEEP : ERASE_CORE_SLOT_DROP, c.slots);
  inner.self = c.self;
  return inner;
}

/* spine (line 214, @unsafe): a tail call, thus a loop. */
static Application spine(const Term *tm, const Terms *args) {
  Application out;
  const Term *fn;
  const Term *arg;
  while (application_view(tm, &fn, &arg)) {
    args = Terms_cons(arg, args);
    tm = fn;
  }
  out.head = tm;
  out.args = args;
  return out;
}

/* call_result (line 222). */
static EraseCoreAnswer call_result(const KTm *head, const KTms *args,
                                   EraseCoreState s, Bool tail, Bool bare) {
  EraseCoreAnswer out;
  out.state = s;
  out.term = head;
  if (bare == YES) return out;
  out.term = tail == YES ? eterm_ktail(head, args) : eterm_kapp(head, args);
  return out;
}

/* captures (line 230): a filter, thus a loop and a reverse. */
static const Int63s *captures(const EraseCoreSlots *slots, const Int63s *xs) {
  const Int63s *rev = NULL;
  Int63 index;
  for (; xs != NULL; xs = xs->tail)
    if (lookup(xs->head, slots, int63_zero(), &index) == LOOKUP_FOUND)
      rev = Int63s_cons(xs->head, rev);
  return Int63s_reverse(rev);
}

/* code (line 241). */
static EraseCoreEntry code(Text name, EraseCoreChain result) {
  EraseCoreEntry out;
  Fid fid;
  const KDecls *all;
  fid.name = name;
  all = KDecls_append(
      result.state.lifted,
      KDecls_cons(eterm_kfun(fid, result.params, result.result, result.body),
                  NULL));
  out.tag = ERASE_CORE_CODE;
  out.repr = NULL;
  out.decls = KDecls_cons(
      eterm_krec(unique_tids(
          Tids_append(decl_tids(all), result.state.groups), NULL)),
      all);
  return out;
}

/* print (line 246): String.append is associative, thus a left loop. */
Text erase_core_print(const EraseCoreRows *rows) {
  Text acc = text_of_cstring("");
  for (; rows != NULL; rows = rows->tail)
    acc = text_append(acc, print_entry(rows->head.name, rows->head.entry));
  return acc;
}

/* free_children (line 258) with the List.append of free_loop (line 300):
   the children of tm in the Bend order, then rest. free_addr, free_terms,
   free_legs and free_branches are inline. */
static const EraseCoreTodos *free_children(Int63 depth, const Term *tm,
                                           const EraseCoreTodos *rest) {
  const EraseCoreTodos *rev = NULL;
  const Terms *args;
  const TermLegs *legs;
  const TermBranches *branches;
  TermPoint point;
  switch (tm->tag) {
    case TERM_VAR:
    case TERM_UNIV:
    case TERM_GLOBAL:
    case TERM_LIT:
    case TERM_AUTO:
    case TERM_LAN:
    case TERM_RAN:
      break;
    case TERM_IN:
      if (term_as_apt(tm->as.in.address, &point))
        rev = EraseCoreTodos_cons(erase_core_todo(depth, point.arg), rev);
      for (args = tm->as.in.args; args != NULL; args = args->tail)
        rev = EraseCoreTodos_cons(erase_core_todo(depth, args->head), rev);
      break;
    case TERM_OUT:
      if (term_as_apt(tm->as.out.address, &point))
        rev = EraseCoreTodos_cons(erase_core_todo(depth, point.arg), rev);
      rev = EraseCoreTodos_cons(erase_core_todo(depth, tm->as.out.head), rev);
      break;
    case TERM_SEC:
      for (legs = tm->as.sec.legs; legs != NULL; legs = legs->tail)
        rev = EraseCoreTodos_cons(leg_todo(depth, legs->head), rev);
      break;
    case TERM_ELIM:
      rev = EraseCoreTodos_cons(erase_core_todo(depth, tm->as.elim.elim->scrut),
                                rev);
      for (branches = tm->as.elim.elim->branches; branches != NULL;
           branches = branches->tail)
        rev = EraseCoreTodos_cons(leg_todo(depth, branches->head.leg), rev);
      break;
    case TERM_LET:
      rev = EraseCoreTodos_cons(erase_core_todo(depth, tm->as.let.value), rev);
      rev = EraseCoreTodos_cons(
          erase_core_todo(int63_succ(depth), tm->as.let.body), rev);
      break;
    case TERM_ANN:
      rev = EraseCoreTodos_cons(erase_core_todo(depth, tm->as.ann.term), rev);
      break;
  }
  return EraseCoreTodos_reverse_onto(rev, rest);
}

/* variable (line 277). */
static int variable(EraseCoreCtx c, EraseCoreState s, Int63 ix,
                    EraseCoreAnswer *out, const KError **error) {
  Int63 index;
  out->state = s;
  switch (lookup(ix, c.slots, int63_zero(), &index)) {
    case LOOKUP_FOUND:
      out->term = eterm_kvar(index);
      return 1;
    case LOOKUP_GONE:
      out->term = eterm_kerased();
      return 1;
    case LOOKUP_MISSING:
      break;
  }
  *error = error_unbound(
      text_append(text_of_cstring("the erasure context has no binder at index "),
                  int63_show(ix)));
  return 0;
}

/* shifted (line 284): done and head are the RT.result<K.ktm>
   (erase_runtime_shift_runtime: 1 = Done, 0 = InvalidStack). */
static int shifted(const Reprs *params, const Repr *ret, EraseCoreState s,
                   Int63 n, int done, const KTm *head, EraseCoreChain *out,
                   const KError **error) {
  if (!done) return refuse("invalid erasure runtime stack", error);
  out->params = params;
  out->result = ret;
  out->body = eterm_ktail(head, eta_vars(n, int63_zero()));
  out->state = s;
  return 1;
}

/* lifted (line 288): done and pruned are the RT.result<RT.pruned>
   (erase_runtime_prune_captures: 1 = Done, 0 = InvalidStack). */
static int lifted(Fid fid, const Reprs *params, const Repr *ret,
                  EraseCoreState s, int done, const Pruned *pruned,
                  EraseCoreAnswer *out, const KError **error) {
  if (!done) return refuse("invalid erasure capture stack", error);
  out->term = eterm_kclos(fid, count_reprs(params), pruned->args);
  out->state = s;
  out->state.lifted = KDecls_append(
      s.lifted,
      KDecls_cons(eterm_kfun(fid, Reprs_append(pruned->captures, params), ret,
                             pruned->body),
                  NULL));
  return 1;
}

/* free_loop (line 295, @unsafe): a loop over the todo list. */
static const Int63s *free_loop(const EraseCoreTodos *todo, const Int63s *acc) {
  Int63 ix;
  while (todo != NULL) {
    EraseCoreTodo item = todo->head;
    if (!var_view(item.term, &ix)) {
      todo = free_children(item.depth, item.term, todo->tail);
      continue;
    }
    if (erase_runtime_ge(ix, item.depth) == YES)
      acc = erase_runtime_insert(int63_sub(ix, item.depth), acc);
    todo = todo->tail;
  }
  return acc;
}

/* inferred (line 330): expected NULL = None{}. */
static int inferred(EraseCoreCtx c, const Value *expected, const Term *tm,
                    const Value **out, const KError **error) {
  if (expected != NULL) {
    *out = expected;
    return 1;
  }
  return check_infer(c.check, QUANTITY_MANY, tm, out, error);
}

/* tick (line 342): the one budget point of core.bend. */
static int tick(EraseCoreState s, EraseCoreState *out, const KError **error) {
  Budget rest;
  if (!budget_spend(text_of_cstring("type-directed erasure"), s.fuel, &rest,
                    error))
    return 0;
  *out = s;
  out->fuel = rest;
  return 1;
}

/* Part 2 (S4 session 3b): Bend lines 336-698. */

/* capture (line 91): Capture{reprs, args}. */
typedef struct {
  const Reprs *reprs;
  const KTms *args;
} Capture;

/* arguments (line 153): Arguments{terms, state}. */
typedef struct {
  const KTms *terms;
  EraseCoreState state;
} Arguments;

/* branches_answer (line 575): Branches{branches, state}. */
typedef struct {
  const KBranches *branches;
  EraseCoreState state;
} BranchesAnswer;

/* F.Pair2<K.kbranch, state>. */
typedef struct {
  const KBranch *branch;
  EraseCoreState state;
} BranchAnswer;

/* An Abort with E.Not_yet. */
static int not_yet(const char *message, const KError **error) {
  *error = error_not_yet(text_of_cstring(message));
  return 0;
}

/* State{0n, [], [], b}. */
static EraseCoreState initial(Budget b) {
  EraseCoreState s;
  s.next = 0;
  s.lifted = NULL;
  s.groups = NULL;
  s.fuel = b;
  return s;
}

/* Dropped{}. */
static EraseCoreEntry dropped(void) {
  EraseCoreEntry out;
  out.tag = ERASE_CORE_DROPPED;
  out.repr = NULL;
  out.decls = NULL;
  return out;
}

static int term_step(EraseCoreCtx c, EraseCoreState s, Bool tail,
                     const Value *expected, const Term *tm,
                     EraseCoreAnswer *out, const KError **error);
static int chain_step(EraseCoreCtx c, EraseCoreState s, const Reprs *params,
                      const Value *ty, const Term *tm, EraseCoreChain *out,
                      const KError **error);
static int jobs_step(EraseCoreCtx c, const Value *ty, const Terms *args,
                     const EraseCoreJobs *acc, Budget b,
                     const EraseCoreJobs **out, const KError **error);

/* term_request (line 336): the Request is a direct call of term_step. */
static int term_request(EraseCoreCtx c, EraseCoreState s, Bool tail,
                        const Value *expected, const Term *tm,
                        EraseCoreAnswer *out, const KError **error) {
  return term_step(c, s, tail, expected, tm, out, error);
}

/* chain_request (line 338): a direct call of chain_step. */
static int chain_request(EraseCoreCtx c, EraseCoreState s,
                         const Reprs *params, const Value *ty, const Term *tm,
                         EraseCoreChain *out, const KError **error) {
  return chain_step(c, s, params, ty, tm, out, error);
}

/* jobs_request (line 340): a direct call of jobs_step. */
static int jobs_request(EraseCoreCtx c, const Value *ty, const Terms *args,
                        const EraseCoreJobs *acc, Budget b,
                        const EraseCoreJobs **out, const KError **error) {
  return jobs_step(c, ty, args, acc, b, out, error);
}

/* jobs_arrow (line 345). */
static int jobs_arrow(EraseCoreCtx c, const Term *arg, const Terms *rest,
                      EraseArrow a, const EraseCoreJobs *acc, Budget b,
                      const EraseCoreJobs **out, const KError **error) {
  Bool keep;
  const Value *av;
  const Value *cod;
  EraseCoreJob job;
  if (!erase_type_point_runtime(c.check, a.quantity, a.domain, &keep, error))
    return 0;
  if (!eval_eval(check_globals(c.check), c.check->env, arg, &av, error))
    return 0;
  if (!erase_type_opened(c.check, a.diagram, Values_cons(av, NULL), &cod,
                         error))
    return 0;
  if (keep == YES) {
    job.ty = a.domain;
    job.term = arg;
    acc = EraseCoreJobs_cons(job, acc);
  }
  return jobs_request(c, cod, rest, acc, b, out, error);
}

/* app_applied (line 352). */
static int app_applied(EraseCoreCtx c, const KTm *head, const KTms *args,
                       EraseCoreState s, Bool tail, const Value *ty,
                       EraseCoreAnswer *out, const KError **error) {
  const Value *w;
  EraseArrow arrow;
  Int63 n;
  if (!eval_whnf(check_globals(c.check), ty, &w, error)) return 0;
  if (!erase_type_arrow_of(w, &arrow)) {
    *out = call_result(head, args, s, tail, NO);
    return 1;
  }
  if (!erase_type_arity(c.check, w, int63_zero(), check_budget(c.check), &n,
                        error))
    return 0;
  if (int63_equal(n, int63_zero()) == YES)
    return not_yet("applications whose remaining parameters all erase in "
                   "type-directed erasure",
                   error);
  *out = call_result(head, args, s, tail, NO);
  return 1;
}

/* app_finish (line 360): args and s are the Arguments of erase_jobs. */
static int app_finish(EraseCoreCtx c, const KTm *head, const KTms *args,
                      EraseCoreState s, Bool tail, const Value *ty,
                      EraseCoreAnswer *out, const KError **error) {
  const Value *w;
  EraseArrow arrow;
  if (args != NULL) return app_applied(c, head, args, s, tail, ty, out, error);
  if (!eval_whnf(check_globals(c.check), ty, &w, error)) return 0;
  *out = call_result(head, args, s, tail,
                     erase_type_arrow_of(w, &arrow) ? YES : NO);
  return 1;
}

/* cap_values (line 367): a right fold, thus a loop and two reverses. */
static int cap_values(EraseCoreCtx c, const Int63s *caps, Capture *out,
                      const KError **error) {
  const Reprs *ps = NULL;
  const KTms *args = NULL;
  const Value *ty;
  const Repr *r;
  EraseCoreAnswer pair;
  for (; caps != NULL; caps = caps->tail) {
    if (!check_infer(c.check, QUANTITY_MANY, term_var(caps->head), &ty, error))
      return 0;
    if (!erase_type_repr_of(c.check, ty, &r, error)) return 0;
    if (!variable(c, initial(budget_create(0)), caps->head, &pair, error))
      return 0;
    ps = Reprs_cons(r, ps);
    args = KTms_cons(pair.term, args);
  }
  out->reprs = Reprs_reverse(ps);
  out->args = KTms_reverse(args);
  return 1;
}

/* erase_jobs (line 378, @unsafe): a tail call, thus a loop. */
static int erase_jobs(EraseCoreCtx c, EraseCoreState s,
                      const EraseCoreJobs *js, const KTms *acc,
                      Arguments *out, const KError **error) {
  EraseCoreAnswer pair;
  for (; js != NULL; js = js->tail) {
    if (!term_request(c, s, NO, js->head.ty, js->head.term, &pair, error))
      return 0;
    acc = KTms_cons(pair.term, acc);
    s = pair.state;
  }
  out->terms = KTms_reverse(acc);
  out->state = s;
  return 1;
}

/* let_body (line 384). */
static int let_body(EraseCoreCtx c, EraseCoreState s, Bool tail,
                    const Value *ty, Text x, const KTm *value,
                    const Term *body, EraseCoreAnswer *out,
                    const KError **error) {
  EraseCoreAnswer pair;
  if (!term_request(c, s, tail, ty, body, &pair, error)) return 0;
  out->term = eterm_klet(x, value, pair.term);
  out->state = pair.state;
  return 1;
}

/* chain_stop (line 387). */
static int chain_stop(EraseCoreCtx c, EraseCoreState s, const Reprs *params,
                      const Value *ty, const Term *tm, EraseCoreChain *out,
                      const KError **error) {
  const Repr *ret;
  EraseCoreAnswer pair;
  if (!erase_type_repr_of(c.check, ty, &ret, error)) return 0;
  if (!term_request(c, s, YES, ty, tm, &pair, error)) return 0;
  out->params = params;
  out->result = ret;
  out->body = pair.term;
  out->state = pair.state;
  return 1;
}

/* eta (line 391): Ty.with_signature gives extra = params, ret = result. */
static int eta(EraseCoreCtx c, EraseCoreState s, const Reprs *params,
               const Value *ty, const Term *tm, EraseCoreChain *out,
               const KError **error) {
  EraseCoreAnswer pair;
  EraseSignature sig;
  const KTm *head = NULL;
  Int63 n;
  int done;
  if (!term_request(c, s, NO, ty, tm, &pair, error)) return 0;
  if (!erase_type_signature_of(c.check, ty, check_budget(c.check), &sig,
                               error))
    return 0;
  n = count_reprs(sig.params);
  done = erase_runtime_shift_runtime(n, int63_zero(), pair.term, &head);
  return shifted(Reprs_append(params, sig.params), sig.result, pair.state, n,
                 done, head, out, error);
}

/* chain_arrow (line 398). */
static int chain_arrow(EraseCoreCtx c, EraseCoreState s, const Reprs *params,
                       EraseArrow a, const Term *body, EraseCoreChain *out,
                       const KError **error) {
  Bool keep;
  const Reprs *here;
  const Value *cod;
  if (!erase_type_point_runtime(c.check, a.quantity, a.domain, &keep, error))
    return 0;
  if (!erase_type_parameter(c.check, a.quantity, a.domain, &here, error))
    return 0;
  if (!erase_type_opened(c.check, a.diagram,
                         Values_cons(value_var(c.check->size), NULL), &cod,
                         error))
    return 0;
  return chain_request(added(c, a.quantity, a.name, a.domain, keep), s,
                       Reprs_append(params, here), cod, body, out, error);
}

/* decl_value (line 405): body NULL = None{}. */
static int decl_value(const CheckCtx *c, Text name, const Term *body,
                      const Value *ty, Budget b, Bool recursive,
                      EraseCoreEntry *out, const KError **error) {
  Bool keep;
  const Repr *r;
  EraseCoreCtx ctx;
  EraseCoreChain result;
  if (!erase_type_runtime_ty(c, ty, &keep, error)) return 0;
  *out = dropped();
  if (keep == NO) return 1;
  if (recursive == YES)
    return not_yet("recursive definitions in type-directed erasure", error);
  if (body == NULL) {
    if (!erase_type_repr_of(c, ty, &r, error)) return 0;
    out->tag = ERASE_CORE_POSTULATE;
    out->repr = r;
    return 1;
  }
  ctx.check = c;
  ctx.slots = NULL;
  ctx.self = name;
  if (!chain_request(ctx, initial(b), NULL, ty, body, &result, error))
    return 0;
  *out = code(name, result);
  return 1;
}

/* jobs_weak (line 411). */
static int jobs_weak(EraseCoreCtx c, const Value *ty, const Term *arg,
                     const Terms *rest, const EraseCoreJobs *acc, Budget b,
                     const EraseCoreJobs **out, const KError **error) {
  EraseArrow a;
  if (!erase_type_arrow_of(ty, &a))
    return refuse("an application needs a point function type", error);
  return jobs_arrow(c, arg, rest, a, acc, b, out, error);
}

/* app (line 415). */
static int app(EraseCoreCtx c, EraseCoreState s, Bool tail, const Value *ty,
               Application a, EraseCoreAnswer *out, const KError **error) {
  const Value *head_ty;
  const EraseCoreJobs *js;
  EraseCoreAnswer pair;
  Arguments value;
  if (!check_infer(c.check, QUANTITY_MANY, a.head, &head_ty, error)) return 0;
  if (!jobs_request(c, head_ty, a.args, NULL, check_budget(c.check), &js,
                    error))
    return 0;
  if (!term_request(c, s, NO, head_ty, a.head, &pair, error)) return 0;
  if (!erase_jobs(c, pair.state, js, NULL, &value, error)) return 0;
  return app_finish(c, pair.term, value.terms, value.state, tail, ty, out,
                    error);
}

/* let_defined (line 423). C.define is runtime_ops_define. */
static int let_defined(EraseCoreCtx c, EraseCoreState s, Bool tail,
                       const Value *ty, Text x, const Value *lty,
                       const Term *value, const Term *body, Bool keep,
                       const Value *vv, EraseCoreAnswer *out,
                       const KError **error) {
  EraseCoreCtx next;
  EraseCoreAnswer pair;
  next.check = runtime_ops_define(x, QUANTITY_MANY, lty, vv, c.check);
  next.slots = EraseCoreSlots_cons(
      keep == YES ? ERASE_CORE_SLOT_KEEP : ERASE_CORE_SLOT_DROP, c.slots);
  next.self = c.self;
  if (keep == NO) return term_request(next, s, tail, ty, body, out, error);
  if (!term_request(c, s, NO, lty, value, &pair, error)) return 0;
  return let_body(next, pair.state, tail, ty, x, pair.term, body, out, error);
}

/* chain_weak (line 431). */
static int chain_weak(EraseCoreCtx c, EraseCoreState s, const Reprs *params,
                      const Value *ty, const Term *tm, const Value *weak,
                      EraseCoreChain *out, const KError **error) {
  EraseArrow a;
  const Term *body;
  if (!erase_type_arrow_of(weak, &a))
    return chain_stop(c, s, params, ty, tm, out, error);
  body = lambda_body(tm);
  if (body == NULL) return eta(c, s, params, ty, tm, out, error);
  return chain_arrow(c, s, params, a, body, out, error);
}

/* decl_type (line 438). */
static int decl_type(const Globals *g, Budget b, Text name, const Term *ty,
                     const Term *body, Bool recursive, EraseCoreEntry *out,
                     const KError **error) {
  const Value *tyv;
  if (!eval_eval(g, NULL, ty, &tyv, error)) return 0;
  return decl_value(check_make(g, b), name, body, tyv, b, recursive, out,
                    error);
}

/* lift_closure (line 440). The Fid is self ++ "$" ++ the decimal of next.
   next + 1 is a uint64 add: each lift spends fuel at tick first. */
static int lift_closure(EraseCoreCtx c, EraseCoreState s, const Value *ty,
                        const Term *tm, EraseCoreAnswer *out,
                        const KError **error) {
  const Int63s *caps;
  Capture value;
  Text parts[3];
  Fid fid;
  EraseCoreCtx inner;
  EraseCoreState start;
  EraseCoreChain result;
  Pruned pruned;
  int done;
  caps = captures(c.slots,
                  free_loop(EraseCoreTodos_cons(
                                erase_core_todo(int63_zero(), tm), NULL),
                            NULL));
  if (!cap_values(c, Int63s_reverse(caps), &value, error)) return 0;
  parts[0] = c.self;
  parts[1] = text_of_cstring("$");
  parts[2] = text_of_nat(s.next);
  fid.name = cat(parts, 3);
  inner.check = c.check;
  inner.slots = framed(c.slots, caps, int63_zero());
  inner.self = c.self;
  start = s;
  start.next = s.next + 1;
  if (!chain_request(inner, start, NULL, ty, tm, &result, error)) return 0;
  done = erase_runtime_prune_captures(value.reprs, value.args,
                                      count_reprs(result.params), result.body,
                                      &pruned);
  return lifted(fid, result.params, result.result, result.state, done,
                &pruned, out, error);
}

/* tuple_jobs (line 452, @unsafe): a right fold, thus a loop and a reverse.
   The legs are walked in order, so the effect order is the Bend order. */
static int tuple_jobs(const CheckCtx *c, const TermLegs *legs,
                      const Values *tys, const EraseCoreJobs **out,
                      const KError **error) {
  const EraseCoreJobs *rev = NULL;
  Bool keep;
  EraseCoreJob job;
  while (legs != NULL && tys != NULL) {
    if (!erase_type_runtime_ty(c, tys->head, &keep, error)) return 0;
    if (keep == YES) {
      job.ty = tys->head;
      job.term = legs->head->body;
      rev = EraseCoreJobs_cons(job, rev);
    }
    legs = legs->tail;
    tys = tys->tail;
  }
  if (legs != NULL || tys != NULL)
    return refuse("a tuple has the wrong number of legs", error);
  *out = EraseCoreJobs_reverse(rev);
  return 1;
}

/* tuple_finish (line 461). */
static int tuple_finish(EraseCoreCtx c, const Value *ty, Arguments value,
                        EraseCoreAnswer *out, const KError **error) {
  Tid tid;
  out->state = value.state;
  if (value.terms == NULL) {
    out->term = eterm_kerased();
    return 1;
  }
  if (!erase_type_tid_of(c.check, ty, &tid, error)) return 0;
  out->term = eterm_kstruct(tid, value.terms);
  return 1;
}

/* tuple_former (line 466). */
static int tuple_former(EraseCoreCtx c, EraseCoreState s, const Value *ty,
                        Int63 n, const TermLegs *legs, ValueFormer former,
                        EraseCoreAnswer *out, const KError **error) {
  const Values *tys;
  const EraseCoreJobs *jobs;
  Arguments value;
  if (!erase_type_leg_types(c.check, n, former.diagram, &tys, error)) return 0;
  if (!tuple_jobs(c.check, legs, tys, &jobs, error)) return 0;
  if (!erase_jobs(c, s, jobs, NULL, &value, error)) return 0;
  return tuple_finish(c, ty, value, out, error);
}

/* tuple (line 472). */
static int tuple(EraseCoreCtx c, EraseCoreState s, const Value *ty, Int63 n,
                 const TermLegs *legs, EraseCoreAnswer *out,
                 const KError **error) {
  const Value *w;
  ValueFormer former;
  if (!eval_whnf(check_globals(c.check), ty, &w, error)) return 0;
  if (!value_as_ran(w, &former))
    return refuse("a tuple stands at a type that is not a right former",
                  error);
  return tuple_former(c, s, ty, n, legs, former, out, error);
}

/* projection_flags (line 476, @unsafe): a loop. */
static int projection_flags(const CheckCtx *c, const Values *tys,
                            Int63 before, Int63 *out, const KError **error) {
  Bool keep;
  for (; tys != NULL; tys = tys->tail)
    if (!erase_type_runtime_ty(c, tys->head, &keep, error)) return 0;
  *out = before;
  return 1;
}

/* projection_offset (line 482, @unsafe): a tail call, thus a loop. */
static int projection_offset(const CheckCtx *c, const Values *tys,
                             Int63 index, Int63 before, Int63 *out,
                             const KError **error) {
  Bool keep;
  for (; tys != NULL; tys = tys->tail) {
    if (!erase_type_runtime_ty(c, tys->head, &keep, error)) return 0;
    if (int63_equal(index, int63_zero()) == YES)
      return projection_flags(c, tys->tail, before, out, error);
    index = int63_pred(index);
    before = keep == YES ? int63_succ(before) : before;
  }
  *error = error_wrong_leg(rules_leg_msg());
  return 0;
}

/* projection_former (line 489). */
static int projection_former(EraseCoreCtx c, EraseCoreState s,
                             const Value *sty, Int63 n, Int63 index,
                             const Term *head, ValueFormer former,
                             EraseCoreAnswer *out, const KError **error) {
  const Values *tys;
  Int63 before;
  EraseCoreAnswer pair;
  Tid tid;
  if (!erase_type_leg_types(c.check, n, former.diagram, &tys, error)) return 0;
  if (!projection_offset(c.check, tys, index, int63_zero(), &before, error))
    return 0;
  if (!term_request(c, s, NO, sty, head, &pair, error)) return 0;
  if (!erase_type_tid_of(c.check, sty, &tid, error)) return 0;
  out->term = eterm_kproj(tid, before, pair.term);
  out->state = pair.state;
  return 1;
}

/* projection (line 497). */
static int projection(EraseCoreCtx c, EraseCoreState s, Int63 n,
                      const TermAddr *address, const Term *head,
                      EraseCoreAnswer *out, const KError **error) {
  Int63 index;
  const Value *sty;
  const Value *w;
  ValueFormer former;
  if (!term_as_aleg(address, &index))
    return refuse("a projection takes a leg address", error);
  if (!check_infer(c.check, QUANTITY_MANY, head, &sty, error)) return 0;
  if (!eval_whnf(check_globals(c.check), sty, &w, error)) return 0;
  if (!value_as_ran(w, &former))
    return refuse(
        "a projection stands on a value that is not a right former", error);
  return projection_former(c, s, sty, n, index, head, former, out, error);
}

/* section (line 502). The SPi case rebuilds T.Sec with the same shape. */
static int section(EraseCoreCtx c, EraseCoreState s, const Value *ty,
                   const TermShape *shape, const TermLegs *legs,
                   EraseCoreAnswer *out, const KError **error) {
  switch (shape->tag) {
    case SHAPE_SPI:
      return lift_closure(c, s, ty, term_sec(shape, legs), out, error);
    case SHAPE_SCOLL:
      return tuple(c, s, ty, shape->as.scoll.size, legs, out, error);
    case SHAPE_SPAR:
      break;
    case SHAPE_SMU:
      return not_yet("recursive families in type-directed erasure", error);
    case SHAPE_SNU:
      return not_yet("coinductive shapes in type-directed erasure", error);
  }
  return not_yet("parallel shapes in type-directed erasure", error);
}

/* application_shape (line 509). The SPi case rebuilds T.Out with the same
   shape. */
static int application_shape(EraseCoreCtx c, EraseCoreState s, Bool tail,
                             const Value *ty, const TermShape *shape,
                             const TermAddr *address, const Term *head,
                             EraseCoreAnswer *out, const KError **error) {
  switch (shape->tag) {
    case SHAPE_SPI:
      return app(c, s, tail, ty, spine(term_out(shape, address, head), NULL),
                 out, error);
    case SHAPE_SCOLL:
      return projection(c, s, shape->as.scoll.size, address, head, out,
                        error);
    case SHAPE_SPAR:
      break;
    case SHAPE_SMU:
      return not_yet("recursive families in type-directed erasure", error);
    case SHAPE_SNU:
      return not_yet("coinductive shapes in type-directed erasure", error);
  }
  return not_yet("parallel shapes in type-directed erasure", error);
}

/* let_value (line 516). */
static int let_value(EraseCoreCtx c, EraseCoreState s, Bool tail,
                     const Value *ty, Text x, const Value *lty,
                     const Term *value, const Term *body, Bool keep,
                     EraseCoreAnswer *out, const KError **error) {
  const Value *vv;
  if (!eval_eval(check_globals(c.check), c.check->env, value, &vv, error))
    return 0;
  return let_defined(c, s, tail, ty, x, lty, value, body, keep, vv, out,
                     error);
}

/* chain_step (line 518). */
static int chain_step(EraseCoreCtx c, EraseCoreState s, const Reprs *params,
                      const Value *ty, const Term *tm, EraseCoreChain *out,
                      const KError **error) {
  EraseCoreState s1;
  const Value *w;
  if (!tick(s, &s1, error)) return 0;
  if (!eval_whnf(check_globals(c.check), ty, &w, error)) return 0;
  return chain_weak(c, s1, params, ty, tm, w, out, error);
}

/* jobs_step (line 521): the budget point "erasure application spine". */
static int jobs_step(EraseCoreCtx c, const Value *ty, const Terms *args,
                     const EraseCoreJobs *acc, Budget b,
                     const EraseCoreJobs **out, const KError **error) {
  Budget fuel;
  const Value *w;
  if (args == NULL) {
    *out = EraseCoreJobs_reverse(acc);
    return 1;
  }
  if (!budget_spend(text_of_cstring("erasure application spine"), b, &fuel,
                    error))
    return 0;
  if (!eval_whnf(check_globals(c.check), ty, &w, error)) return 0;
  return jobs_weak(c, w, args->head, args->tail, acc, fuel, out, error);
}

/* declaration_program (line 527). */
static int declaration_program(const Globals *g, Budget b, Text name,
                               const GlobalEntry *en, EraseCoreEntry *out,
                               const KError **error) {
  const GlobalDef *def;
  switch (en->tag) {
    case GLOBAL_PRIM:
      *out = dropped();
      return 1;
    case GLOBAL_AXIOM:
      return decl_type(g, b, name, en->as.axiom.ty, NULL, NO, out, error);
    case GLOBAL_DEF:
      break;
  }
  def = &en->as.def;
  return decl_type(g, b, name, def->ty, def->body,
                   def->rec_arg != NULL || def->partial == YES ? YES : NO,
                   out, error);
}

/* let_term (line 533). */
static int let_term(EraseCoreCtx c, EraseCoreState s, Bool tail,
                    const Value *ty, Text x, const Term *lty,
                    const Term *value, const Term *body,
                    EraseCoreAnswer *out, const KError **error) {
  const Value *ltyv;
  Bool keep;
  if (!eval_eval(check_globals(c.check), c.check->env, lty, &ltyv, error))
    return 0;
  if (!erase_type_runtime_ty(c.check, ltyv, &keep, error)) return 0;
  return let_value(c, s, tail, ty, x, ltyv, value, body, keep, out, error);
}

/* program_loop (line 539, @unsafe): a tail call, thus a loop. */
static int program_loop(const Globals *g, Budget b,
                        const CheckNamedEntries *entries,
                        const EraseCoreRows *acc, const EraseCoreRows **out,
                        const KError **error) {
  EraseCoreRow row;
  for (; entries != NULL; entries = entries->tail) {
    row.name = entries->head.name;
    if (!declaration_program(g, b, row.name, entries->head.entry, &row.entry,
                             error))
      return 0;
    g = global_add(row.name, entries->head.entry, g);
    acc = EraseCoreRows_cons(row, acc);
  }
  *out = EraseCoreRows_reverse(acc);
  return 1;
}

/* tag_body (line 544). */
static int tag_body(EraseCoreCtx c, EraseCoreState s, Tid tid, Int63 index,
                    const EraseSumPayload *value, EraseCoreAnswer *out,
                    const KError **error) {
  EraseCoreAnswer pair;
  if (!term_request(c, s, NO, value->ty, value->body, &pair, error)) return 0;
  out->term = eterm_ktag(tid, index, KTms_cons(pair.term, NULL));
  out->state = pair.state;
  return 1;
}

/* tag_plan (line 548): payload NULL = None{}. */
static int tag_plan(EraseCoreCtx c, EraseCoreState s, EraseSumTag plan,
                    EraseCoreAnswer *out, const KError **error) {
  if (plan.payload != NULL)
    return tag_body(c, s, plan.tid, plan.index, plan.payload, out, error);
  out->term = eterm_ktag(plan.tid, plan.index, NULL);
  out->state = s;
  return 1;
}

/* pair_jobs (line 554): a right map, thus a loop and a reverse. */
static const EraseCoreJobs *pair_jobs(const ErasePairFields *fields) {
  const EraseCoreJobs *rev = NULL;
  EraseCoreJob job;
  for (; fields != NULL; fields = fields->tail) {
    job.ty = fields->head.ty;
    job.term = fields->head.term;
    rev = EraseCoreJobs_cons(job, rev);
  }
  return EraseCoreJobs_reverse(rev);
}

/* pair_fields (line 558). */
static int pair_fields(EraseCoreCtx c, EraseCoreState s, const Value *ty,
                       const ErasePairFields *fields, EraseCoreAnswer *out,
                       const KError **error) {
  Arguments value;
  if (!erase_jobs(c, s, pair_jobs(fields), NULL, &value, error)) return 0;
  return tuple_finish(c, ty, value, out, error);
}

/* extra (line 560). */
static EraseCoreCtx extra(EraseCoreCtx c) {
  c.slots = EraseCoreSlots_cons(ERASE_CORE_SLOT_EXTRA, c.slots);
  return c;
}

/* pair_lets (line 563). */
static const KTm *pair_lets(Tid tid, Text x, Text second, Bool keep1,
                            Bool keep2, const KTm *body) {
  Int63 offset = keep1 == YES ? int63_one() : int63_zero();
  const KTm *inner =
      keep2 == YES
          ? eterm_klet(second, eterm_kproj(tid, offset, eterm_kvar(offset)),
                       body)
          : body;
  return keep1 == YES
             ? eterm_klet(x,
                          eterm_kproj(tid, int63_zero(),
                                      eterm_kvar(int63_zero())),
                          inner)
             : inner;
}

/* pair_branch (line 567). */
static int pair_branch(EraseCoreCtx c, EraseCoreState s, Bool tail,
                       const Term *scrut, ErasePairBranch plan,
                       EraseCoreAnswer *out, const KError **error) {
  EraseCoreCtx c2 =
      added(added(extra(c), plan.quantity, plan.first_name, plan.domain,
                  plan.keep_first),
            QUANTITY_MANY, plan.second_name, plan.fibre, plan.keep_second);
  EraseCoreAnswer value;
  EraseCoreAnswer result;
  if (!term_request(c, s, NO, plan.scrut_type, scrut, &value, error)) return 0;
  if (!term_request(c2, value.state, tail, plan.target, plan.body, &result,
                    error))
    return 0;
  out->term = eterm_klet(
      text_of_cstring("scrut"), value.term,
      pair_lets(plan.tid, plan.first_name, plan.second_name, plan.keep_first,
                plan.keep_second, result.term));
  out->state = result.state;
  return 1;
}

/* with_groups (line 577). */
static EraseCoreState with_groups(EraseCoreState s, const Tids *more) {
  s.groups = Tids_append(s.groups, more);
  return s;
}

/* mu_tag (line 580). */
static int mu_tag(EraseCoreCtx c, EraseCoreState s, EraseMuTag plan,
                  EraseCoreAnswer *out, const KError **error) {
  Arguments value;
  if (!erase_jobs(c, s, pair_jobs(plan.fields), NULL, &value, error))
    return 0;
  out->term = eterm_ktag(plan.tid, plan.index, value.terms);
  out->state = with_groups(value.state, plan.group);
  return 1;
}

/* mu_context (line 585): a tail call, thus a loop. */
static EraseCoreCtx mu_context(const EraseMuBinders *bs, EraseCoreCtx c) {
  for (; bs != NULL; bs = bs->tail)
    c = added(c, bs->head.quantity, bs->head.name, bs->head.domain,
              bs->head.keep);
  return c;
}

/* mu_body (line 589). */
static int mu_body(EraseCoreCtx c, EraseCoreState s, Bool tail, Int63 index,
                   EraseMuBranch plan, BranchAnswer *out,
                   const KError **error) {
  EraseCoreAnswer value;
  if (!term_request(mu_context(plan.binders, c), s, tail, plan.target,
                    plan.body, &value, error))
    return 0;
  out->branch = eterm_branch(index, plan.arity, value.term);
  out->state = value.state;
  return 1;
}

/* mu_branches (line 594, @unsafe): a tail call, thus a loop. fam is
   passed through unchanged (the Bend rebuilds the same Mu.Case). */
static int mu_branches(EraseCoreCtx c, EraseCoreState s, Bool tail,
                       const Texts *names, Int63 index,
                       const TermBranches *source, const KBranches *acc,
                       EraseMuCase fam, BranchesAnswer *out,
                       const KError **error) {
  EraseMuBranch plan;
  BranchAnswer value;
  for (; names != NULL; names = names->tail) {
    if (!erase_mu_branch_of(c.check, fam.name, fam.family, fam.motive, fam.env,
                            names->head, source, &plan, error))
      return 0;
    if (!mu_body(c, s, tail, index, plan, &value, error)) return 0;
    s = value.state;
    acc = KBranches_cons(value.branch, acc);
    index = int63_succ(index);
  }
  out->branches = KBranches_reverse(acc);
  out->state = s;
  return 1;
}

/* mu_finish (line 606). */
static EraseCoreAnswer mu_finish(Text n, const KTm *scrut, const Tids *group,
                                 BranchesAnswer result) {
  EraseCoreAnswer out;
  out.term = eterm_kcase(erase_type_mu_tid(n), scrut, result.branches);
  out.state = with_groups(result.state, group);
  return out;
}

/* mu_case (line 609). */
static int mu_case(EraseCoreCtx c, EraseCoreState s, Bool tail,
                   const Term *scrut, const TermBranches *branches,
                   EraseMuCase plan, EraseCoreAnswer *out,
                   const KError **error) {
  EraseCoreAnswer value;
  BranchesAnswer result;
  if (!term_request(c, s, NO, plan.sty, scrut, &value, error)) return 0;
  if (!mu_branches(c, value.state, tail, plan.names, int63_zero(), branches,
                   NULL, plan, &result, error))
    return 0;
  *out = mu_finish(plan.name, value.term, plan.group, result);
  return 1;
}

/* injection_shape (line 616). */
static int injection_shape(EraseCoreCtx c, EraseCoreState s, const Value *ty,
                           const TermShape *shape, const TermAddr *address,
                           const Terms *args, EraseCoreAnswer *out,
                           const KError **error) {
  EraseSumTag tag;
  const ErasePairFields *fields;
  EraseMuTag mu;
  switch (shape->tag) {
    case SHAPE_SCOLL:
      if (!erase_sum_injection(c.check, ty, address, args, &tag, error))
        return 0;
      return tag_plan(c, s, tag, out, error);
    case SHAPE_SPI:
      if (!erase_pair_introduction(c.check, ty, address, args, &fields, error))
        return 0;
      return pair_fields(c, s, ty, fields, out, error);
    case SHAPE_SPAR:
      break;
    case SHAPE_SMU:
      if (!erase_mu_injection(c.check, ty, address, args, &mu, error))
        return 0;
      return mu_tag(c, s, mu, out, error);
    case SHAPE_SNU:
      return not_yet("coinductive shapes in type-directed erasure", error);
  }
  return not_yet("parallel shapes in type-directed erasure", error);
}

/* branch_body (line 623). */
static int branch_body(EraseCoreCtx c, EraseCoreState s, Bool tail,
                       Int63 index, EraseSumBranch plan, BranchAnswer *out,
                       const KError **error) {
  EraseCoreAnswer pair;
  if (!term_request(added(c, plan.quantity, plan.name, plan.domain, plan.keep),
                    s, tail, plan.target, plan.body, &pair, error))
    return 0;
  out->branch = eterm_branch(
      index, plan.keep == YES ? int63_one() : int63_zero(), pair.term);
  out->state = pair.state;
  return 1;
}

/* case_branches (line 629, @unsafe): a tail call, thus a loop. */
static int case_branches(EraseCoreCtx c, EraseCoreState s, Bool tail,
                         const Value *ty, const TermMotive *motive,
                         const Values *tys, const Values *remaining,
                         Int63 index, const TermBranches *source,
                         const KBranches *acc, BranchesAnswer *out,
                         const KError **error) {
  EraseSumBranch plan;
  BranchAnswer value;
  for (; remaining != NULL; remaining = remaining->tail) {
    if (!erase_sum_branch_of(c.check, ty, motive, tys, index, source, &plan,
                             error))
      return 0;
    if (!branch_body(c, s, tail, index, plan, &value, error)) return 0;
    s = value.state;
    acc = KBranches_cons(value.branch, acc);
    index = int63_succ(index);
  }
  out->branches = KBranches_reverse(acc);
  out->state = s;
  return 1;
}

/* case_finish (line 637). */
static EraseCoreAnswer case_finish(Tid tid, const KTm *scrut,
                                   BranchesAnswer value) {
  EraseCoreAnswer out;
  out.term = eterm_kcase(tid, scrut, value.branches);
  out.state = value.state;
  return out;
}

/* case_plan (line 640). */
static int case_plan(EraseCoreCtx c, EraseCoreState s, Bool tail,
                     const Value *ty, const Term *scrut,
                     const TermMotive *motive, const TermBranches *branches,
                     EraseSumCase plan, EraseCoreAnswer *out,
                     const KError **error) {
  EraseCoreAnswer pair;
  BranchesAnswer value;
  if (!term_request(c, s, NO, plan.ty, scrut, &pair, error)) return 0;
  if (!case_branches(c, pair.state, tail, ty, motive, plan.legs, plan.legs,
                     int63_zero(), branches, NULL, &value, error))
    return 0;
  *out = case_finish(plan.tid, pair.term, value);
  return 1;
}

/* elimination_shape (line 645). */
static int elimination_shape(EraseCoreCtx c, EraseCoreState s, Bool tail,
                             const Value *ty, const TermShape *shape,
                             const Term *scrut, const TermMotive *motive,
                             const TermBranches *branches,
                             EraseCoreAnswer *out, const KError **error) {
  EraseSumCase sum;
  ErasePairBranch pair;
  EraseMuCase mu;
  switch (shape->tag) {
    case SHAPE_SCOLL:
      if (!erase_sum_case_of(c.check, scrut, &sum, error)) return 0;
      return case_plan(c, s, tail, ty, scrut, motive, branches, sum, out,
                       error);
    case SHAPE_SPI:
      if (!erase_pair_elimination(c.check, ty, shape, scrut, motive, branches,
                                  &pair, error))
        return 0;
      return pair_branch(c, s, tail, scrut, pair, out, error);
    case SHAPE_SPAR:
      break;
    case SHAPE_SMU:
      if (!erase_mu_case_of(c.check, scrut, motive, &mu, error)) return 0;
      return mu_case(c, s, tail, scrut, branches, mu, out, error);
    case SHAPE_SNU:
      return not_yet("coinductive shapes in type-directed erasure", error);
  }
  return not_yet("parallel shapes in type-directed erasure", error);
}

/* elimination (line 652). */
static int elimination(EraseCoreCtx c, EraseCoreState s, Bool tail,
                       const Value *ty, const TermElim *value,
                       EraseCoreAnswer *out, const KError **error) {
  return elimination_shape(c, s, tail, ty, value->shape, value->scrut,
                           value->motive, value->branches, out, error);
}

/* node (line 655). */
static int node(EraseCoreCtx c, EraseCoreState s, Bool tail, const Value *ty,
                const Term *tm, EraseCoreAnswer *out, const KError **error) {
  out->state = s;
  switch (tm->tag) {
    case TERM_VAR:
      return variable(c, s, tm->as.var.index, out, error);
    case TERM_UNIV:
    case TERM_LAN:
    case TERM_RAN:
      out->term = eterm_kerased();
      return 1;
    case TERM_GLOBAL:
      out->term = eterm_kglobal(tm->as.global.name);
      return 1;
    case TERM_LIT:
      out->term = eterm_klit(tm->as.lit.literal);
      return 1;
    case TERM_ANN:
      return term_request(c, s, tail, ty, tm->as.ann.term, out, error);
    case TERM_LET:
      return let_term(c, s, tail, ty, tm->as.let.name, tm->as.let.ty,
                      tm->as.let.value, tm->as.let.body, out, error);
    case TERM_SEC:
      return section(c, s, ty, tm->as.sec.shape, tm->as.sec.legs, out, error);
    case TERM_OUT:
      return application_shape(c, s, tail, ty, tm->as.out.shape,
                               tm->as.out.address, tm->as.out.head, out,
                               error);
    case TERM_IN:
      return injection_shape(c, s, ty, tm->as.in.shape, tm->as.in.address,
                             tm->as.in.args, out, error);
    case TERM_ELIM:
      return elimination(c, s, tail, ty, tm->as.elim.elim, out, error);
    case TERM_AUTO:
      break;
  }
  *error = error_not_yet(rules_auto_word());
  return 0;
}

/* term_step (line 670). */
static int term_step(EraseCoreCtx c, EraseCoreState s, Bool tail,
                     const Value *expected, const Term *tm,
                     EraseCoreAnswer *out, const KError **error) {
  EraseCoreState s1;
  const Value *ty;
  Bool keep;
  if (!tick(s, &s1, error)) return 0;
  if (!inferred(c, expected, tm, &ty, error)) return 0;
  if (!erase_type_runtime_ty(c.check, ty, &keep, error)) return 0;
  if (keep == YES) return node(c, s1, tail, ty, tm, out, error);
  out->term = eterm_kerased();
  out->state = s1;
  return 1;
}

/* term (line 691): drive of term_request. */
int erase_core_term(EraseCoreCtx c, EraseCoreState s, Bool tail,
                    const Value *expected, const Term *tm,
                    EraseCoreAnswer *out, const KError **error) {
  return term_request(c, s, tail, expected, tm, out, error);
}

/* chain (line 693): drive of chain_request. */
int erase_core_chain(EraseCoreCtx c, EraseCoreState s, const Reprs *params,
                     const Value *ty, const Term *tm, EraseCoreChain *out,
                     const KError **error) {
  return chain_request(c, s, params, ty, tm, out, error);
}

/* declaration (line 695): drive of declaration_program. */
int erase_core_declaration(const Globals *g, Budget b, Text name,
                           const GlobalEntry *en, EraseCoreEntry *out,
                           const KError **error) {
  return declaration_program(g, b, name, en, out, error);
}

/* program (line 697): drive of program_loop. */
int erase_core_program(const Globals *g, Budget b,
                       const CheckNamedEntries *entries,
                       const EraseCoreRows **out, const KError **error) {
  return program_loop(g, b, entries, NULL, out, error);
}
