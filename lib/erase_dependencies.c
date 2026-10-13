/* lib/erase_dependencies.c: twin of erase/dependencies.bend (S4 session 1).
   Layout dependencies (Kanon 69f3be51 lib/erase.ml:tids_ktm). */
#include "sole.h"

static const Tids *own(const KTm *tm, const Tids *acc) {
  switch (tm->tag) {
    case KTM_KVAR: return acc;
    case KTM_KLIT: return acc;
    case KTM_KGLOBAL: return acc;
    case KTM_KERASED: return acc;
    case KTM_KLET: return acc;
    case KTM_KCLOS: return acc;
    case KTM_KAPP: return acc;
    case KTM_KTAIL: return acc;
    case KTM_KSTRUCT: return Tids_cons(tm->as.kstruct.name, acc);
    case KTM_KPROJ: return Tids_cons(tm->as.kproj.name, acc);
    case KTM_KTAG: return Tids_cons(tm->as.ktag.name, acc);
    case KTM_KCASE: return Tids_cons(tm->as.kcase.name, acc);
    case KTM_KDELAY: return acc;
    case KTM_KFORCE: return acc;
  }
  return acc; /* unreachable: the switch is exhaustive */
}

/* term + loop (@unsafe in Bend; a self tail call) is a loop.
   R.rev_append(acc, []) = Tids_reverse. */
const Tids *erase_dependencies_term(const KTm *tm) {
  const ScopedTerms *pending = ScopedTerms_cons(scoped_term(int63_zero(), tm),
                                                NULL);
  const Tids *acc = NULL;
  ScopedTerm item;
  while (pending != NULL) {
    item = pending->head;
    pending = ScopedTerms_append(erase_runtime_children(item.depth, item.term),
                                 pending->tail);
    acc = own(item.term, acc);
  }
  return Tids_reverse(acc);
}
