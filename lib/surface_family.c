/* lib/surface_family.c: twin of surface/family.bend (S5 session 3c).
   L.result(A) and M.result(A) are the int return (1 = Done or Pure with
   *out, 0 = Fail or Abort with *error); parsed(A) adds *rest. L.bind,
   L.choose, M.bind and M.map_result are C if statements. P.ctor is
   const Ctor * and Maybe<P.ctor> is NULL for None{}. */
#include "sole.h"

/* S.cat: String.concat of a fixed list of parts. */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

/* want (line 13). */
static int want(const char *what, const SurfaceTokens *ts,
                const KError **error) {
  if (ts == NULL) {
    *error = surface_lexer_error(
        surface_token_start(),
        text_of_cstring("unexpected end of family declaration"));
    return 0;
  }
  {
    Text parts[4];
    parts[0] = text_of_cstring("expected ");
    parts[1] = text_of_cstring(what);
    parts[2] = text_of_cstring(", found ");
    parts[3] = surface_token_describe(ts->head.kind);
    *error = surface_lexer_error(ts->head.loc, cat(parts, 4));
  }
  return 0;
}

/* The head token tag of ts is tag. */
static int head_is(const SurfaceTokens *ts, SurfaceTokenTag tag) {
  return ts != NULL && ts->head.kind.tag == tag;
}

/* known (line 24): Bool.or is eager in Bend; a pure scan, so the loop stops
   at the first match with the same answer. */
Bool surface_family_known(Text name, const Texts *seen) {
  for (; seen != NULL; seen = seen->tail)
    if (text_same(name, seen->head)) return YES;
  return NO;
}

/* group_names (line 32) and ctor_names (line 28): right folds, so one loop
   plus a reverse. */
const Texts *surface_family_group_names(const SurfaceFams *fams) {
  const Texts *acc = NULL;
  for (; fams != NULL; fams = fams->tail) {
    const SurfaceFamCtors *cs;
    acc = Texts_cons(fams->head.name, acc);
    for (cs = fams->head.ctors; cs != NULL; cs = cs->tail)
      acc = Texts_cons(cs->head.name, acc);
  }
  return Texts_reverse(acc);
}

/* finished (line 37). */
static int finished(const SurfaceFamCtors *ctors, const SurfaceTokens *ts,
                    const SurfaceFamCtors **out,
                    const SurfaceTokens **rest) {
  *out = SurfaceFamCtors_reverse(ctors);
  *rest = ts;
  return 1;
}

/* constructors (@unsafe, line 41): the callers build only Read{fuel, ts,
   acc, seen}, thus the Read fields are parameters. The ctor_step states
   Read, AfterType and AfterTail are one loop. Each pass of the loop is one
   Read step; AfterType and AfterTail follow in the same pass. */
int surface_family_constructors(const void *env, SurfaceParseTerm parse,
                                Nat fuel, const SurfaceTokens *ts,
                                const SurfaceFamCtors *acc,
                                const Texts *seen,
                                const SurfaceFamCtors **out,
                                const SurfaceTokens **rest,
                                const KError **error) {
  for (;;) {
    Nat remaining;
    Text name;
    const SurfaceTerm *ty;
    const SurfaceTokens *tail;
    if (fuel == 0) return want("'}'", ts, error);
    remaining = fuel - 1;
    if (head_is(ts, TOKEN_RBRACE)) return finished(acc, ts->tail, out, rest);
    if (!(head_is(ts, TOKEN_IDENT) && head_is(ts->tail, TOKEN_COLON)))
      return want("a constructor 'name : type' or '}'", ts, error);
    name = ts->head.kind.as.ident;
    if (surface_family_known(name, seen)) {
      Text parts[2];
      parts[0] = text_of_cstring("E-FAMILY-CTOR: duplicate constructor ");
      parts[1] = name;
      *error = surface_lexer_error(ts->head.loc, cat(parts, 2));
      return 0;
    }
    if (!parse(env, ts->tail->tail, &ty, &tail, error)) return 0;
    /* AfterType (line 54). */
    acc = SurfaceFamCtors_cons(surface_syntax_fam_ctor(name, ty), acc);
    seen = Texts_cons(name, seen);
    /* AfterTail (line 57). */
    if (head_is(tail, TOKEN_RBRACE))
      return finished(acc, tail->tail, out, rest);
    if (!head_is(tail, TOKEN_SEMICOLON))
      return want("';' or '}' after a constructor", tail, error);
    ts = tail->tail;
    fuel = remaining;
  }
}

/* body (line 63). */
static const SurfaceTerm *body(const SurfaceBinders *bs,
                               const SurfaceTerm *value) {
  return bs == NULL ? value : surface_syntax_sfun(bs, value);
}

/* field (line 67): a binder type is always present, so ty is Some{ty}. */
static SurfaceField field(SurfaceBinder b) {
  return surface_syntax_field(b.q, b.name, b.ty);
}

/* fields (@unsafe, line 74): a loop. The F.Pair2 answer field_body is the
   two out parameters *fs and *result. */
static int fields(Text ctor, const Telescope *args, const SurfaceBinders *bs,
                  const SurfaceTerm *value, const SurfaceFields **fs,
                  const SurfaceTerm **result, const KError **error) {
  const SurfaceFields *acc = NULL;
  while (args != NULL) {
    if (bs != NULL) {
      acc = SurfaceFields_cons(field(bs->head), acc);
      args = args->tail;
      bs = bs->tail;
      continue;
    }
    if (value->tag != SURFACE_SFUN) {
      Text parts[3];
      parts[0] = text_of_cstring("the elim arm for ");
      parts[1] = ctor;
      parts[2] = text_of_cstring(" must bind each of its fields");
      *error = error_missing_branch(cat(parts, 3));
      return 0;
    }
    bs = value->as.fun.binders;
    value = value->as.fun.body;
  }
  *fs = SurfaceFields_reverse(acc);
  *result = body(bs, value);
  return 1;
}

/* ctor_at (line 85): NULL = None{}. */
static const Ctor *ctor_at(const Ctors *ctors, Int63 key) {
  for (; ctors != NULL; ctors = ctors->tail) {
    if (int63_equal(key, int63_zero())) return ctors->head;
    key = int63_pred(key);
  }
  return NULL;
}

/* branch (line 93) and made (line 90). */
static int branch(const Ctor *ct, const SurfaceBinders *binders,
                  const SurfaceTerm *value, SurfaceBranch *out,
                  const KError **error) {
  Text name = positivity_ctor_name(ct);
  const SurfaceFields *fs;
  const SurfaceTerm *result;
  if (!fields(name, ct->args, binders, value, &fs, &result, error)) return 0;
  *out = surface_syntax_br_ctor(name, fs, result);
  return 1;
}

/* selected (line 97). */
static int selected(Text owner, Int63 key, const SurfaceBinders *binders,
                    const SurfaceTerm *value, const Ctor *found,
                    SurfaceBranch *out, const KError **error) {
  if (found == NULL) {
    Text parts[5];
    parts[0] = text_of_cstring("family elim arm ");
    parts[1] = int63_show(key);
    parts[2] = text_of_cstring(" of ");
    parts[3] = owner;
    parts[4] = text_of_cstring(" has no constructor");
    *error = error_wrong_leg(cat(parts, 5));
    return 0;
  }
  return branch(found, binders, value, out, error);
}

/* arm (line 102). */
static int arm(Text owner, const Ctors *ctors, SurfaceBranch br,
               SurfaceBranch *out, const KError **error) {
  if (br.tag == SURFACE_BR_LEG)
    return selected(owner, br.as.leg.key, br.as.leg.binders, br.as.leg.body,
                    ctor_at(ctors, br.as.leg.key), out, error);
  if (br.tag == SURFACE_BR_DEFAULT) {
    *error = error_wrong_leg(text_of_cstring(
        "a family default arm must be expanded before constructor "
        "selection"));
    return 0;
  }
  *out = br;
  return 1;
}

/* without (line 108): a loop plus a reverse onto the rest. With no match
   the answer is cs (the same list; Bend makes a copy). */
static const Ctors *without(Text name, const Ctors *cs) {
  const Ctors *acc = NULL;
  const Ctors *p;
  for (p = cs; p != NULL; p = p->tail) {
    if (text_same(name, positivity_ctor_name(p->head)))
      return Ctors_reverse_onto(acc, p->tail);
    acc = Ctors_cons(p->head, acc);
  }
  return cs;
}

/* after (line 115). */
static const Ctors *after(SurfaceBranch br, const Ctors *cs) {
  return br.tag == SURFACE_BR_CTOR ? without(br.as.ctor.name, cs) : cs;
}

/* fill (line 122): a right fold, so a loop plus a reverse. The
   constructors go in list order, so the first refusal is the same. */
static int fill(const Ctors *cs, const SurfaceTerm *value,
                const SurfaceBranches **out, const KError **error) {
  const SurfaceBranches *acc = NULL;
  for (; cs != NULL; cs = cs->tail) {
    SurfaceBranch br;
    if (!branch(cs->head, NULL, value, &br, error)) return 0;
    acc = SurfaceBranches_cons(br, acc);
  }
  *out = SurfaceBranches_reverse(acc);
  return 1;
}

/* default (line 128); default is a C keyword. */
static int default_arms(const Ctors *cs, const SurfaceTerm *value,
                        const SurfaceBranches **out, const KError **error) {
  if (cs == NULL) {
    *error = error_wrong_leg(text_of_cstring(
        "a default elim arm must cover at least one remaining constructor"));
    return 0;
  }
  return fill(cs, value, out, error);
}

/* arms (line 135): a right fold over bs, so a loop plus a reverse, with
   remaining updated at each step. The arms go in list order, so the first
   refusal is the same. */
static int arms(Text owner, const Ctors *ctors, const SurfaceBranches *bs,
                const Ctors *remaining, const SurfaceBranches **out,
                const KError **error) {
  const SurfaceBranches *acc = NULL;
  for (; bs != NULL; bs = bs->tail) {
    SurfaceBranch br = bs->head;
    switch (br.tag) {
    case SURFACE_BR_DEFAULT: {
      const SurfaceBranches *tail;
      if (bs->tail != NULL) {
        *error =
            error_wrong_leg(text_of_cstring("a default elim arm must be last"));
        return 0;
      }
      if (!default_arms(remaining, br.as.other.body, &tail, error)) return 0;
      *out = SurfaceBranches_reverse_onto(acc, tail);
      return 1;
    }
    case SURFACE_BR_LEG: {
      SurfaceBranch made;
      if (!arm(owner, ctors, br, &made, error)) return 0;
      acc = SurfaceBranches_cons(made, acc);
      remaining = after(made, remaining);
      break;
    }
    case SURFACE_BR_CTOR:
      acc = SurfaceBranches_cons(br, acc);
      remaining = without(br.as.ctor.name, remaining);
      break;
    }
  }
  *out = SurfaceBranches_reverse(acc);
  return 1;
}

/* expanded (line 145). */
int surface_family_expanded(const Family *fam, const SurfaceBranches *bs,
                            const SurfaceBranches **out,
                            const KError **error) {
  return arms(fam->name, fam->ctors, bs, fam->ctors, out, error);
}
