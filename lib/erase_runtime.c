/* lib/erase_runtime.c: twin of erase/runtime.bend (S4 session 1).
   Runtime scope operations (Kanon 69f3be51 lib/erase.ml:497-555). The Bend
   code keeps explicit work stacks in place of mutual recursion; the C code
   keeps the same stacks as loops. Bend result<A> (Done{v} | InvalidStack{})
   is an int return: 1 = Done with *out, 0 = InvalidStack. */
#include "sole.h"

/* ---------------------------------------------------------------- order */

static Bool is_lt(Cmp order) {
  switch (order) {
    case CMP_LT: return YES;
    case CMP_EQ: return NO;
    case CMP_GT: return NO;
  }
  return NO; /* unreachable: the switch is exhaustive */
}

Bool erase_runtime_lt(Int63 a, Int63 b) { return is_lt(int63_compare(a, b)); }

Bool erase_runtime_ge(Int63 a, Int63 b) {
  return erase_runtime_lt(a, b) ? NO : YES;
}

/* ---------------------------------------------------------------- scope */

static const ScopedTerms *scope_terms(Int63 depth, const KTms *ts) {
  const ScopedTerms *acc = NULL;
  for (; ts != NULL; ts = ts->tail)
    acc = ScopedTerms_cons(scoped_term(depth, ts->head), acc);
  return ScopedTerms_reverse(acc);
}

static const ScopedTerms *scope_branches(Int63 depth, const KBranches *bs) {
  const ScopedTerms *acc = NULL;
  for (; bs != NULL; bs = bs->tail)
    acc = ScopedTerms_cons(
        scoped_term(int63_add(depth, bs->head->arity), bs->head->body), acc);
  return ScopedTerms_reverse(acc);
}

const ScopedTerms *erase_runtime_children(Int63 depth, const KTm *t) {
  switch (t->tag) {
    case KTM_KVAR: return NULL;
    case KTM_KLIT: return NULL;
    case KTM_KGLOBAL: return NULL;
    case KTM_KERASED: return NULL;
    case KTM_KLET:
      return ScopedTerms_cons(
          scoped_term(depth, t->as.klet.value),
          ScopedTerms_cons(scoped_term(int63_succ(depth), t->as.klet.body),
                           NULL));
    case KTM_KCLOS: return scope_terms(depth, t->as.kclos.captures);
    case KTM_KAPP:
      return ScopedTerms_cons(scoped_term(depth, t->as.kapp.head),
                              scope_terms(depth, t->as.kapp.args));
    case KTM_KTAIL:
      return ScopedTerms_cons(scoped_term(depth, t->as.ktail.head),
                              scope_terms(depth, t->as.ktail.args));
    case KTM_KSTRUCT: return scope_terms(depth, t->as.kstruct.fields);
    case KTM_KPROJ:
      return ScopedTerms_cons(scoped_term(depth, t->as.kproj.value), NULL);
    case KTM_KTAG: return scope_terms(depth, t->as.ktag.fields);
    case KTM_KCASE:
      return ScopedTerms_cons(scoped_term(depth, t->as.kcase.scrutinee),
                              scope_branches(depth, t->as.kcase.branches));
    case KTM_KDELAY: return scope_terms(depth, t->as.kdelay.captures);
    case KTM_KFORCE:
      return ScopedTerms_cons(scoped_term(depth, t->as.kforce.value), NULL);
  }
  return NULL; /* unreachable: the switch is exhaustive */
}

/* ---------------------------------------------------------------- live */

/* member (Bool.or over a self recursion) is a loop. */
Bool erase_runtime_member(Int63 x, const Int63s *xs) {
  for (; xs != NULL; xs = xs->tail)
    if (int63_equal(x, xs->head)) return YES;
  return NO;
}

/* smaller: the count of y in xs with y < x (a right fold of Int63 adds;
   the wrapping add does not depend on the order). */
static Int63 smaller(Int63 x, const Int63s *xs) {
  Int63 count = int63_zero();
  for (; xs != NULL; xs = xs->tail)
    count = int63_add(erase_runtime_lt(xs->head, x) ? int63_one() : int63_zero(),
                      count);
  return count;
}

static Int63 compact_index(Int63 params, const Int63s *live, Int63 i) {
  if (erase_runtime_lt(i, params)) return i;
  return int63_add(params, smaller(int63_sub(i, params), live));
}

/* insert: the sorted insert of x (no duplicate). */
const Int63s *erase_runtime_insert(Int63 x, const Int63s *xs) {
  const Int63s *prefix = NULL;
  for (; xs != NULL; xs = xs->tail) {
    if (erase_runtime_lt(x, xs->head))
      return Int63s_reverse_onto(prefix, Int63s_cons(x, xs));
    if (int63_equal(x, xs->head)) return Int63s_reverse_onto(prefix, xs);
    prefix = Int63s_cons(xs->head, prefix);
  }
  return Int63s_reverse_onto(prefix, Int63s_cons(x, NULL));
}

static const Int63s *live_step(Int63 params, Int63 count, Int63 i,
                               const Int63s *rest) {
  if (erase_runtime_ge(i, params) &&
      erase_runtime_lt(i, int63_add(params, count)))
    return erase_runtime_insert(int63_sub(i, params), rest);
  return rest;
}

/* live_indices (a right fold) walks xs from the end. */
static const Int63s *live_indices(Int63 params, Int63 count,
                                  const Int63s *xs) {
  const Int63s *rev = Int63s_reverse(xs);
  const Int63s *acc = NULL;
  for (; rev != NULL; rev = rev->tail)
    acc = live_step(params, count, rev->head, acc);
  return acc;
}

/* ---------------------------------------------------------------- reindex */

/* The Bend `index` argument of map_loop (A -> Int63 -> Int63, with its
   state A) has two instances: shift_runtime (state = the offset) and
   capture_index (state = F.Pair2{params, live}). C keeps them as a closed
   sum (no function pointer). */
typedef enum { REINDEX_SHIFT, REINDEX_CAPTURE } ReindexKind;

typedef struct {
  ReindexKind kind;
  Int63 offset;       /* REINDEX_SHIFT */
  Int63 params;       /* REINDEX_CAPTURE */
  const Int63s *live; /* REINDEX_CAPTURE */
} Reindex;

static Int63 reindex_index(const Reindex *ix, Int63 i) {
  switch (ix->kind) {
    case REINDEX_SHIFT: return int63_add(i, ix->offset);
    case REINDEX_CAPTURE: return compact_index(ix->params, ix->live, i);
  }
  return i; /* unreachable: the switch is exhaustive */
}

typedef enum {
  FRAME_LEAF,
  FRAME_LET,
  FRAME_CLOS,
  FRAME_APP,
  FRAME_TAIL,
  FRAME_STRUCT,
  FRAME_PROJ,
  FRAME_TAG,
  FRAME_CASE,
  FRAME_DELAY,
  FRAME_FORCE
} FrameTag;

typedef struct {
  FrameTag tag;
  const KTm *term;           /* Leaf */
  Text name;                 /* Let */
  Fid fid;                   /* Clos, Delay */
  Tid tid;                   /* Struct, Proj, Tag, Case */
  Int63 number;              /* Clos arity, Proj field, Tag tag */
  Nat size;                  /* Clos, App, Tail, Struct, Tag, Delay */
  const KBranches *branches; /* Case */
} Frame;

static Frame frame(FrameTag tag) {
  Frame f;
  f.tag = tag;
  f.term = NULL;
  f.name = text_end();
  f.fid.name = text_end();
  f.tid.name = text_end();
  f.number = int63_zero();
  f.size = 0;
  f.branches = NULL;
  return f;
}

static Frame leaf(const KTm *t) {
  Frame f = frame(FRAME_LEAF);
  f.term = t;
  return f;
}

static Frame frame_of(const Reindex *ix, Int63 depth, const KTm *t) {
  Frame f = frame(FRAME_FORCE);
  Int63 i;
  switch (t->tag) {
    case KTM_KVAR:
      i = t->as.kvar.index;
      if (erase_runtime_ge(i, depth))
        return leaf(eterm_kvar(
            int63_add(depth, reindex_index(ix, int63_sub(i, depth)))));
      return leaf(eterm_kvar(i));
    case KTM_KLIT: return leaf(t);
    case KTM_KGLOBAL: return leaf(t);
    case KTM_KERASED: return leaf(t);
    case KTM_KLET:
      f = frame(FRAME_LET);
      f.name = t->as.klet.name;
      return f;
    case KTM_KCLOS:
      f = frame(FRAME_CLOS);
      f.fid = t->as.kclos.name;
      f.number = t->as.kclos.arity;
      f.size = KTms_length(t->as.kclos.captures);
      return f;
    case KTM_KAPP:
      f = frame(FRAME_APP);
      f.size = KTms_length(t->as.kapp.args);
      return f;
    case KTM_KTAIL:
      f = frame(FRAME_TAIL);
      f.size = KTms_length(t->as.ktail.args);
      return f;
    case KTM_KSTRUCT:
      f = frame(FRAME_STRUCT);
      f.tid = t->as.kstruct.name;
      f.size = KTms_length(t->as.kstruct.fields);
      return f;
    case KTM_KPROJ:
      f = frame(FRAME_PROJ);
      f.tid = t->as.kproj.name;
      f.number = t->as.kproj.field;
      return f;
    case KTM_KTAG:
      f = frame(FRAME_TAG);
      f.tid = t->as.ktag.name;
      f.number = t->as.ktag.tag;
      f.size = KTms_length(t->as.ktag.fields);
      return f;
    case KTM_KCASE:
      f = frame(FRAME_CASE);
      f.tid = t->as.kcase.name;
      f.branches = t->as.kcase.branches;
      return f;
    case KTM_KDELAY:
      f = frame(FRAME_DELAY);
      f.fid = t->as.kdelay.name;
      f.size = KTms_length(t->as.kdelay.captures);
      return f;
    case KTM_KFORCE: return f;
  }
  return f; /* unreachable: the switch is exhaustive */
}

/* arity. Nat sites: 1 + n (App, Tail, Case); n is a list length. */
static Nat arity(Frame f) {
  switch (f.tag) {
    case FRAME_LEAF: return 0;
    case FRAME_LET: return 2;
    case FRAME_CLOS: return f.size;
    case FRAME_APP: return 1 + f.size;
    case FRAME_TAIL: return 1 + f.size;
    case FRAME_STRUCT: return f.size;
    case FRAME_PROJ: return 1;
    case FRAME_TAG: return f.size;
    case FRAME_CASE: return 1 + KBranches_length(f.branches);
    case FRAME_DELAY: return f.size;
    case FRAME_FORCE: return 1;
  }
  return 0; /* unreachable: the switch is exhaustive */
}

/* pop (a self tail call) is a loop: *items = the popped values in stack
   order reversed (Bend acc), *rest = the remaining values. */
static int pop(Nat count, const KTms *values, const KTms **items,
               const KTms **rest) {
  const KTms *acc = NULL;
  for (; count > 0; count--) {
    if (values == NULL) return 0;
    acc = KTms_cons(values->head, acc);
    values = values->tail;
  }
  *items = acc;
  *rest = values;
  return 1;
}

/* pack_one: exactly one value. */
static int pack_one(const KTms *xs, const KTm **x) {
  if (KTms_length(xs) != 1) return 0;
  *x = xs->head;
  return 1;
}

/* zip_branches + branch_cons: equal lengths, else InvalidStack. */
static int zip_branches(const KBranches *bs, const KTms *ts,
                        const KBranches **out) {
  const KBranches *acc = NULL;
  for (; bs != NULL && ts != NULL; bs = bs->tail, ts = ts->tail)
    acc = KBranches_cons(eterm_branch(bs->head->tag, bs->head->arity, ts->head),
                         acc);
  if (bs != NULL || ts != NULL) return 0;
  *out = KBranches_reverse(acc);
  return 1;
}

/* pack, with pack_let, pack_app, pack_case and case_body. */
static int pack(Frame f, const KTms *xs, const KTm **out) {
  const KTm *x;
  const KBranches *branches;
  switch (f.tag) {
    case FRAME_LEAF:
      *out = f.term;
      return 1;
    case FRAME_LET:
      if (KTms_length(xs) != 2) return 0;
      *out = eterm_klet(f.name, xs->head, xs->tail->head);
      return 1;
    case FRAME_CLOS:
      *out = eterm_kclos(f.fid, f.number, xs);
      return 1;
    case FRAME_APP:
      if (xs == NULL) return 0;
      *out = eterm_kapp(xs->head, xs->tail);
      return 1;
    case FRAME_TAIL:
      if (xs == NULL) return 0;
      *out = eterm_ktail(xs->head, xs->tail);
      return 1;
    case FRAME_STRUCT:
      *out = eterm_kstruct(f.tid, xs);
      return 1;
    case FRAME_PROJ:
      if (!pack_one(xs, &x)) return 0;
      *out = eterm_kproj(f.tid, f.number, x);
      return 1;
    case FRAME_TAG:
      *out = eterm_ktag(f.tid, f.number, xs);
      return 1;
    case FRAME_CASE:
      if (xs == NULL) return 0;
      if (!zip_branches(f.branches, xs->tail, &branches)) return 0;
      *out = eterm_kcase(f.tid, xs->head, branches);
      return 1;
    case FRAME_DELAY:
      *out = eterm_kdelay(f.fid, xs);
      return 1;
    case FRAME_FORCE:
      if (!pack_one(xs, &x)) return 0;
      *out = eterm_kforce(x);
      return 1;
  }
  return 0; /* unreachable: the switch is exhaustive */
}

typedef enum { TASK_VISIT, TASK_BUILD } TaskTag;

typedef struct {
  TaskTag tag;
  Int63 depth;     /* Visit */
  const KTm *term; /* Visit */
  Frame frame;     /* Build */
} Task;

SOLE_LIST(Tasks, Task)

static Task visit(Int63 depth, const KTm *term) {
  Task task;
  task.tag = TASK_VISIT;
  task.depth = depth;
  task.term = term;
  task.frame = frame(FRAME_FORCE);
  return task;
}

static Task build(Frame f) {
  Task task;
  task.tag = TASK_BUILD;
  task.depth = int63_zero();
  task.term = NULL;
  task.frame = f;
  return task;
}

/* schedule: a Visit for each scoped term, in order, then tail. */
static const Tasks *schedule(const ScopedTerms *xs, const Tasks *tail) {
  const ScopedTerms *rev = ScopedTerms_reverse(xs);
  for (; rev != NULL; rev = rev->tail)
    tail = Tasks_cons(visit(rev->head.depth, rev->head.term), tail);
  return tail;
}

/* map_loop (@unsafe in Bend; self tail calls) is a loop, with rebuild,
   push, resumed and finish. reindex_runtime = map_loop on [Visit]. */
static int map_loop(const Reindex *ix, Int63 depth, const KTm *t,
                    const KTm **out) {
  const Tasks *todo = Tasks_cons(visit(depth, t), NULL);
  const KTms *values = NULL;
  const KTms *items;
  const KTm *built;
  Task task;
  while (todo != NULL) {
    task = todo->head;
    todo = todo->tail;
    switch (task.tag) {
      case TASK_VISIT:
        todo = schedule(erase_runtime_children(task.depth, task.term),
                        Tasks_cons(build(frame_of(ix, task.depth, task.term)),
                                   todo));
        break;
      case TASK_BUILD:
        if (!pop(arity(task.frame), values, &items, &values)) return 0;
        if (!pack(task.frame, items, &built)) return 0;
        values = KTms_cons(built, values);
        break;
    }
  }
  if (KTms_length(values) != 1) return 0;
  *out = values->head;
  return 1;
}

int erase_runtime_shift_runtime(Int63 by, Int63 depth, const KTm *t,
                                const KTm **out) {
  Reindex ix;
  ix.kind = REINDEX_SHIFT;
  ix.offset = by;
  ix.params = int63_zero();
  ix.live = NULL;
  return map_loop(&ix, depth, t, out);
}

/* ---------------------------------------------------------------- vars */

static const Int63s *record_var(Int63 depth, const KTm *t,
                                const Int63s *acc) {
  switch (t->tag) {
    case KTM_KVAR:
      if (erase_runtime_ge(t->as.kvar.index, depth))
        return Int63s_cons(int63_sub(t->as.kvar.index, depth), acc);
      return acc;
    case KTM_KLIT: return acc;
    case KTM_KGLOBAL: return acc;
    case KTM_KERASED: return acc;
    case KTM_KLET: return acc;
    case KTM_KCLOS: return acc;
    case KTM_KAPP: return acc;
    case KTM_KTAIL: return acc;
    case KTM_KSTRUCT: return acc;
    case KTM_KPROJ: return acc;
    case KTM_KTAG: return acc;
    case KTM_KCASE: return acc;
    case KTM_KDELAY: return acc;
    case KTM_KFORCE: return acc;
  }
  return acc; /* unreachable: the switch is exhaustive */
}

/* runtime_vars + vars_loop (@unsafe in Bend; a self tail call) is a loop.
   append_scoped = ScopedTerms_append; reverse_vars = Int63s_reverse. */
static const Int63s *runtime_vars(Int63 depth, const KTm *t) {
  const ScopedTerms *todo = ScopedTerms_cons(scoped_term(depth, t), NULL);
  const Int63s *acc = NULL;
  ScopedTerm item;
  while (todo != NULL) {
    item = todo->head;
    todo = ScopedTerms_append(erase_runtime_children(item.depth, item.term),
                              todo->tail);
    acc = record_var(item.depth, item.term, acc);
  }
  return Int63s_reverse(acc);
}

/* ---------------------------------------------------------------- prune */

static Int63 count_reprs(const Reprs *xs) {
  Int63 count = int63_zero();
  for (; xs != NULL; xs = xs->tail) count = int63_succ(count);
  return count;
}

/* select at A = K.repr and at A = K.ktm (two C twins of the one generic
   Bend def). The first item has the index `index`; each next item has the
   index before it minus one. */
static const Reprs *select_reprs(const Int63s *live, const Reprs *xs,
                                 Int63 index) {
  const Reprs *acc = NULL;
  for (; xs != NULL; xs = xs->tail, index = int63_pred(index))
    if (erase_runtime_member(index, live)) acc = Reprs_cons(xs->head, acc);
  return Reprs_reverse(acc);
}

static const KTms *select_ktms(const Int63s *live, const KTms *xs,
                               Int63 index) {
  const KTms *acc = NULL;
  for (; xs != NULL; xs = xs->tail, index = int63_pred(index))
    if (erase_runtime_member(index, live)) acc = KTms_cons(xs->head, acc);
  return KTms_reverse(acc);
}

/* prune_captures, prune_count, prune_live, pruned_body, capture_index. */
int erase_runtime_prune_captures(const Reprs *ps, const KTms *args,
                                 Int63 params, const KTm *body, Pruned *out) {
  Int63 count = count_reprs(ps);
  const Int63s *live =
      live_indices(params, count, runtime_vars(int63_zero(), body));
  const KTm *reindexed;
  Reindex ix;
  ix.kind = REINDEX_CAPTURE;
  ix.offset = int63_zero();
  ix.params = params;
  ix.live = live;
  if (!map_loop(&ix, int63_zero(), body, &reindexed)) return 0;
  out->captures = select_reprs(live, ps, int63_pred(count));
  out->args = select_ktms(live, args, int63_pred(count));
  out->body = reindexed;
  return 1;
}
