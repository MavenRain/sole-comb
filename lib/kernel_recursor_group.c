/* kernel_recursor_group.c: C twin of lib/kernel_recursor_group.bend
   (stage S1b).

   The group holds the families whose constructor fields reach the owner.
   reaches and joining are loops over their lists. extend and its lambda
   (grown => reach(...)) fold into the reach loop: a round that adds no
   family stops it, and the fuel is the length of the family list. */
#include "sole.h"

/* reaches: Bool.or over the ctors (pure values, so the loop stops at the
   first YES). */
Bool recursor_group_reaches(const Texts *names, const Ctors *ctors) {
  for (; ctors != NULL; ctors = ctors->tail)
    if (positivity_self_rec(names, ctors->head->args)) return YES;
  return NO;
}

/* joining (and admit): the names of the families that are not yet in names
   and that reach names, in the order of fams. */
const Texts *recursor_group_joining(const Texts *names,
                                    const NamedFamilies *fams) {
  const Texts *added = NULL;
  for (; fams != NULL; fams = fams->tail) {
    Text n = fams->head.name;
    if (!term_member(n, names) &&
        recursor_group_reaches(names, fams->head.family->ctors))
      added = Texts_cons(n, added);
  }
  return Texts_reverse(added);
}

/* reach (and extend): one round per fuel item; a round that adds nothing
   returns names. extend puts the added names before names. */
const Texts *recursor_group_reach(const NamedFamilies *fuel,
                                  const NamedFamilies *fams,
                                  const Texts *names) {
  for (; fuel != NULL; fuel = fuel->tail) {
    const Texts *added = recursor_group_joining(names, fams);
    if (added == NULL) return names;
    names = Texts_append(added, names);
  }
  return names;
}

const Texts *recursor_group_members(const NamedFamilies *fams, Text owner) {
  return recursor_group_reach(fams, fams, Texts_cons(owner, NULL));
}
