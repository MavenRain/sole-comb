/* lib/surface_sugar.c: twin of surface/sugar.bend (S5 session 3c).
   M.result(A) is the int return (1 = Pure with *out, 0 = Abort with
   *error). M.map_result is a C if statement. ~A slots is a Bend list of
   any type and only its length counts, thus slots is a Nat count. */
#include "sole.h"

/* S.cat: String.concat of a fixed list of parts. */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

/* arm (line 9): an SFun with an empty binder list goes to the last row. */
SurfaceBranch surface_sugar_arm(Int63 k, const SurfaceTerm *body) {
  if (body->tag == SURFACE_SFUN && body->as.fun.binders != NULL) {
    const SurfaceBinders *more = body->as.fun.binders->tail;
    const SurfaceBinders *one =
        SurfaceBinders_cons(body->as.fun.binders->head, NULL);
    if (more == NULL) return surface_syntax_br_leg(k, one, body->as.fun.body);
    return surface_syntax_br_leg(k, one,
                                 surface_syntax_sfun(more, body->as.fun.body));
  }
  return surface_syntax_br_leg(k, NULL, body);
}

/* fill (line 15): a right fold over slots, so a loop plus a reverse. */
static const SurfaceBranches *fill(Nat slots, Int63 k,
                                   const SurfaceTerm *body) {
  const SurfaceBranches *acc = NULL;
  for (; slots > 0; slots--) {
    acc = SurfaceBranches_cons(surface_sugar_arm(k, body), acc);
    k = int63_succ(k);
  }
  return SurfaceBranches_reverse(acc);
}

/* default (line 20); default is a C keyword. */
static int default_arms(Int63 k, const SurfaceTerm *body, Nat slots,
                        const SurfaceBranches **out, const KError **error) {
  if (slots == 0) {
    *error = error_wrong_leg(text_of_cstring(
        "a default elim arm must cover at least one remaining leg"));
    return 0;
  }
  *out = fill(slots, k, body);
  return 1;
}

/* expand (line 28): a right fold over bs, so a loop plus a reverse. The
   arms go in list order, so the first refusal is the same. A leg with no
   slot left gives the rest of bs unchanged. */
int surface_sugar_expand(const SurfaceBranches *bs, Int63 k, Nat slots,
                         const SurfaceBranches **out, const KError **error) {
  const SurfaceBranches *acc = NULL;
  for (; bs != NULL; bs = bs->tail) {
    SurfaceBranch br = bs->head;
    switch (br.tag) {
    case SURFACE_BR_LEG:
      if (slots == 0) {
        *out = SurfaceBranches_reverse_onto(acc, bs);
        return 1;
      }
      acc = SurfaceBranches_cons(br, acc);
      k = int63_succ(k);
      slots = slots - 1;
      break;
    case SURFACE_BR_CTOR: {
      Text parts[3];
      parts[0] = text_of_cstring("the branch at ");
      parts[1] = br.as.ctor.name;
      parts[2] = text_of_cstring(
          " keys a constructor and a case at a collection keys a leg");
      *error = error_wrong_leg(cat(parts, 3));
      return 0;
    }
    case SURFACE_BR_DEFAULT: {
      const SurfaceBranches *tail;
      if (bs->tail != NULL) {
        *error =
            error_wrong_leg(text_of_cstring("a default elim arm must be last"));
        return 0;
      }
      if (!default_arms(k, br.as.other.body, slots, &tail, error)) return 0;
      *out = SurfaceBranches_reverse_onto(acc, tail);
      return 1;
    }
    }
  }
  *out = SurfaceBranches_reverse(acc);
  return 1;
}
