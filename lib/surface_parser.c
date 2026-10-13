/* lib/surface_parser.c: twin of surface/parser.bend (S6 sessions 1 to
   1c). parsed(A) = L.result(F.Pair2<A, tokens>) is an int return (1 = Done
   with *out and *rest, 0 = Fail with *error). done, next, mark_apply,
   mark_then, L.bind and L.choose have no C twin: they are C if statements
   and returns. The @unsafe def dispatch (a task machine) is one static
   function per task in direct style; the answer type and read_0 to read_12
   have no C twin, because each task function has its own typed *out. The
   task functions recurse on the term depth (C recursion on the big stack of
   lib/runtime.c, PORT-PLAN item b.4). */
#include "sole.h"

/* String.concat of a fixed list of parts (the cat idiom of kernel_rules). */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

/* L.error at loc with a C string message. */
static int fail(SurfaceLoc loc, const char *message, const KError **error) {
  *error = surface_lexer_error(loc, text_of_cstring(message));
  return 0;
}

/* want (line 21): want_text takes the String what as a Text (ParseItems
   builds it), want takes a C string. */
static int want_text(Text what, const SurfaceTokens *ts,
                     const KError **error) {
  if (ts == NULL)
    return fail(surface_token_start(), "unexpected end of input", error);
  {
    Text parts[4];
    parts[0] = text_of_cstring("expected ");
    parts[1] = what;
    parts[2] = text_of_cstring(", found ");
    parts[3] = surface_token_describe(ts->head.kind);
    *error = surface_lexer_error(ts->head.loc, cat(parts, 4));
  }
  return 0;
}

static int want(const char *what, const SurfaceTokens *ts,
                const KError **error) {
  return want_text(text_of_cstring(what), ts, error);
}

/* bounded (line 25): Bool.or is eager in Bend; both tests are pure. The
   test is: not (0 <= n), or the decimal text of n has more than 18
   digits. */
static int bounded(SurfaceLoc loc, Bignum n, Int63 *out,
                   const KError **error) {
  Cmp sign = bignum_compare(bignum_zero(), n);
  Bool nonnegative = sign == CMP_LT || sign == CMP_EQ;
  if (!nonnegative || bignum_to_string(n).size > 18)
    return fail(loc, "numeric literal too long", error);
  if (!bignum_to_int(n, out))
    return fail(loc, "numeric literal too long", error);
  return 1;
}

/* mark_prefix (line 30): the F.Pair2 answer is the Quantity and *rest.
   mark_apply and mark_then are the caller code after this call. */
static Quantity mark_prefix(const SurfaceTokens *ts,
                            const SurfaceTokens **rest) {
  *rest = ts;
  if (ts == NULL || ts->head.kind.tag != TOKEN_NUMBER) return QUANTITY_MANY;
  if (bignum_equal(ts->head.kind.as.number, bignum_zero())) {
    *rest = ts->tail;
    return QUANTITY_ZERO;
  }
  if (bignum_equal(ts->head.kind.as.number, bignum_one())) {
    *rest = ts->tail;
    return QUANTITY_ONE;
  }
  return QUANTITY_MANY;
}

/* bytes_term (line 41): a right fold over the bytes, so a loop from the
   last byte to the first. */
static const SurfaceTerm *bytes_term(Text bytes) {
  const SurfaceTerm *acc = surface_syntax_svar(text_of_cstring("bytesNil"));
  U32 i = bytes.size;
  for (; i > 0; i--)
    acc = surface_syntax_sapp(
        surface_syntax_sapp(surface_syntax_svar(text_of_cstring("bytesCons")),
                            surface_syntax_snat(
                                bignum_from_u32(bytes.items[i - 1]))),
        acc);
  return acc;
}

/* elimination (line 45). */
typedef enum { ELIM_LEGACY_CASE, ELIM_FIBERED_MATCH } Elimination;

/* elimination_node (line 48). */
static const SurfaceTerm *elimination_node(Elimination form,
                                           const SurfaceTerm *scrut,
                                           const SurfaceMotive *mo,
                                           const SurfaceBranches *brs) {
  switch (form) {
    case ELIM_LEGACY_CASE:
      return surface_syntax_scase(scrut, mo, brs);
    case ELIM_FIBERED_MATCH:
      return surface_syntax_smatch(scrut, mo, brs);
  }
  return surface_syntax_scase(scrut, mo, brs);
}

/* numeric_key (line 52) has no C twin: no def in parser.bend calls it. */

/* index_clause = IndexClause{ind, idx} (line 56). */
typedef struct {
  MaybeText ind;
  const Texts *idx;
} IndexClause;

/* kind_starts_atom (line 59): exhaustive, grouped by the answer. */
static Bool kind_starts_atom(SurfaceTokenTag k) {
  switch (k) {
    case TOKEN_LPAREN:
    case TOKEN_UNIT:
    case TOKEN_KTUPLE:
    case TOKEN_KSUM:
    case TOKEN_KPROD:
    case TOKEN_KPROP:
    case TOKEN_KTYPE:
    case TOKEN_KAUTO:
    case TOKEN_KNU:
    case TOKEN_KNAT_ADD:
    case TOKEN_KNAT_SUB:
    case TOKEN_KNAT_MUL:
    case TOKEN_KNAT_EQ:
    case TOKEN_KNAT_LT:
    case TOKEN_IDENT:
    case TOKEN_NUMBER:
    case TOKEN_BYTES:
      return YES;
    case TOKEN_LBRACE:
    case TOKEN_RBRACE:
    case TOKEN_SEMICOLON:
    case TOKEN_KELIM:
    case TOKEN_KELSE:
    case TOKEN_KRECORD:
    case TOKEN_RPAREN:
    case TOKEN_COLON:
    case TOKEN_COLON_EQ:
    case TOKEN_ARROW:
    case TOKEN_DARROW:
    case TOKEN_STAR:
    case TOKEN_COMMA:
    case TOKEN_DOT:
    case TOKEN_DOT1:
    case TOKEN_DOT2:
    case TOKEN_PIPE:
    case TOKEN_KDEF:
    case TOKEN_KAXIOM:
    case TOKEN_KFUN:
    case TOKEN_KINJ:
    case TOKEN_KOF:
    case TOKEN_KCASE:
    case TOKEN_KMATCH:
    case TOKEN_KAS:
    case TOKEN_KRETURN:
    case TOKEN_KWITH:
    case TOKEN_KABSURD:
    case TOKEN_KLET:
    case TOKEN_KIN:
    case TOKEN_KMU:
    case TOKEN_KMUTUAL:
    case TOKEN_KEND:
    case TOKEN_KAND:
    case TOKEN_KREC:
    case TOKEN_EOF:
      return NO;
  }
  return NO;
}

/* starts_atom (line 114). */
static Bool starts_atom(const SurfaceTokens *ts) {
  return ts != NULL && kind_starts_atom(ts->head.kind.tag);
}

/* collection (line 118). */
static const SurfaceTerm *collection(Text word, const SurfaceTerms *items) {
  if (text_same(word, text_of_cstring("tuple")))
    return surface_syntax_stuple(items);
  if (text_same(word, text_of_cstring("sum")))
    return surface_syntax_ssum(items);
  return surface_syntax_sprod(items);
}

/* arm_count (line 374): the right fold 1n + arm_count(rest) is the list
   length. */
static Nat arm_count(const SurfaceBranches *bs) {
  return SurfaceBranches_length(bs);
}

/* native_branch (line 378). */
static int native_branch(SurfaceLoc loc, const SurfaceTerm *body,
                         const SurfaceBranches *acc, SurfaceBranch *out,
                         const KError **error) {
  Int63 key;
  if (!bounded(loc, bignum_from_nat(arm_count(acc)), &key, error)) return 0;
  *out = surface_sugar_arm(key, body);
  return 1;
}

/* dispatch (line 382): one function per task. Each one is named p_ plus
   the task name without Parse, and takes the task fields in order. */
static int p_term(const SurfaceTokens *ts, const SurfaceTerm **out,
                  const SurfaceTokens **rest, const KError **error);
static int p_fun_term(const SurfaceTokens *ts, const SurfaceTerm **out,
                      const SurfaceTokens **rest, const KError **error);
static int p_fun_body(const SurfaceBinders *bs, const SurfaceTokens *ts,
                      const SurfaceTerm **out, const SurfaceTokens **rest,
                      const KError **error);
static int p_binders(const SurfaceTokens *ts, const SurfaceBinders *acc,
                     const SurfaceBinders **out, const SurfaceTokens **rest,
                     const KError **error);
static int p_binder(const SurfaceTokens *ts, SurfaceBinder *out,
                    const SurfaceTokens **rest, const KError **error);
static int p_let_term(const SurfaceTokens *ts, const SurfaceTerm **out,
                      const SurfaceTokens **rest, const KError **error);
static int p_case_term(Elimination form, const SurfaceTokens *ts,
                       const SurfaceTerm **out, const SurfaceTokens **rest,
                       const KError **error);
static int p_arrow(const SurfaceTokens *ts, const SurfaceTerm **out,
                   const SurfaceTokens **rest, const KError **error);

/* ParseTerm (line 384). */
static int p_term(const SurfaceTokens *ts, const SurfaceTerm **out,
                  const SurfaceTokens **rest, const KError **error) {
  if (ts != NULL) {
    switch (ts->head.kind.tag) {
      case TOKEN_KFUN:
        return p_fun_term(ts->tail, out, rest, error);
      case TOKEN_KLET:
        return p_let_term(ts->tail, out, rest, error);
      case TOKEN_KELIM:
        return p_case_term(ELIM_LEGACY_CASE, ts->tail, out, rest, error);
      case TOKEN_KMATCH:
        return p_case_term(ELIM_FIBERED_MATCH, ts->tail, out, rest, error);
      case TOKEN_LPAREN:
      case TOKEN_RPAREN:
      case TOKEN_LBRACE:
      case TOKEN_RBRACE:
      case TOKEN_SEMICOLON:
      case TOKEN_KELSE:
      case TOKEN_KRECORD:
      case TOKEN_COLON:
      case TOKEN_COLON_EQ:
      case TOKEN_ARROW:
      case TOKEN_DARROW:
      case TOKEN_STAR:
      case TOKEN_COMMA:
      case TOKEN_DOT:
      case TOKEN_DOT1:
      case TOKEN_DOT2:
      case TOKEN_PIPE:
      case TOKEN_UNIT:
      case TOKEN_KDEF:
      case TOKEN_KAXIOM:
      case TOKEN_KINJ:
      case TOKEN_KOF:
      case TOKEN_KCASE:
      case TOKEN_KAS:
      case TOKEN_KRETURN:
      case TOKEN_KWITH:
      case TOKEN_KTUPLE:
      case TOKEN_KSUM:
      case TOKEN_KPROD:
      case TOKEN_KABSURD:
      case TOKEN_KPROP:
      case TOKEN_KTYPE:
      case TOKEN_KIN:
      case TOKEN_KAUTO:
      case TOKEN_KMU:
      case TOKEN_KMUTUAL:
      case TOKEN_KEND:
      case TOKEN_KNU:
      case TOKEN_KAND:
      case TOKEN_KREC:
      case TOKEN_KNAT_ADD:
      case TOKEN_KNAT_SUB:
      case TOKEN_KNAT_MUL:
      case TOKEN_KNAT_EQ:
      case TOKEN_KNAT_LT:
      case TOKEN_IDENT:
      case TOKEN_NUMBER:
      case TOKEN_BYTES:
      case TOKEN_EOF:
        break;
    }
  }
  return p_arrow(ts, out, rest, error);
}

/* ParseFunTerm (line 395): one binder, then the binder list from [first],
   then the body. */
static int p_fun_term(const SurfaceTokens *ts, const SurfaceTerm **out,
                      const SurfaceTokens **rest, const KError **error) {
  SurfaceBinder first;
  const SurfaceBinders *bs;
  const SurfaceTokens *after_first;
  const SurfaceTokens *tail;
  if (!p_binder(ts, &first, &after_first, error)) return 0;
  if (!p_binders(after_first, SurfaceBinders_cons(first, NULL), &bs, &tail,
                 error))
    return 0;
  return p_fun_body(bs, tail, out, rest, error);
}

/* ParseFunBody (line 399). */
static int p_fun_body(const SurfaceBinders *bs, const SurfaceTokens *ts,
                      const SurfaceTerm **out, const SurfaceTokens **rest,
                      const KError **error) {
  const SurfaceTerm *body;
  if (ts == NULL || ts->head.kind.tag != TOKEN_DARROW)
    return want("'=>'", ts, error);
  if (!p_term(ts->tail, &body, rest, error)) return 0;
  *out = surface_syntax_sfun(bs, body);
  return 1;
}

/* ParseBinders (line 405): the self tail call is a loop. */
static int p_binders(const SurfaceTokens *ts, const SurfaceBinders *acc,
                     const SurfaceBinders **out, const SurfaceTokens **rest,
                     const KError **error) {
  while (ts != NULL && ts->head.kind.tag == TOKEN_LPAREN) {
    SurfaceBinder b;
    if (!p_binder(ts, &b, &ts, error)) return 0;
    acc = SurfaceBinders_cons(b, acc);
  }
  *out = SurfaceBinders_reverse(acc);
  *rest = ts;
  return 1;
}

static int p_binder_head(Quantity q, const SurfaceTokens *ts,
                         SurfaceBinder *out, const SurfaceTokens **rest,
                         const KError **error);
static int p_binder_close(SurfaceBinder b, const SurfaceTokens *ts,
                          SurfaceBinder *out, const SurfaceTokens **rest,
                          const KError **error);

/* ParseBinder (line 411): mark_then is mark_prefix, then the head. */
static int p_binder(const SurfaceTokens *ts, SurfaceBinder *out,
                    const SurfaceTokens **rest, const KError **error) {
  const SurfaceTokens *tail;
  Quantity q;
  if (ts == NULL || ts->head.kind.tag != TOKEN_LPAREN)
    return want("'('", ts, error);
  q = mark_prefix(ts->tail, &tail);
  return p_binder_head(q, tail, out, rest, error);
}

/* ParseBinderHead (line 417). */
static int p_binder_head(Quantity q, const SurfaceTokens *ts,
                         SurfaceBinder *out, const SurfaceTokens **rest,
                         const KError **error) {
  const SurfaceTerm *ty;
  const SurfaceTokens *tail;
  if (ts == NULL || ts->head.kind.tag != TOKEN_IDENT || ts->tail == NULL ||
      ts->tail->head.kind.tag != TOKEN_COLON)
    return want("a binder name and ':'", ts, error);
  if (!p_term(ts->tail->tail, &ty, &tail, error)) return 0;
  return p_binder_close(surface_syntax_binder(q, ts->head.kind.as.ident, ty),
                        tail, out, rest, error);
}

/* ParseBinderClose (line 423). */
static int p_binder_close(SurfaceBinder b, const SurfaceTokens *ts,
                          SurfaceBinder *out, const SurfaceTokens **rest,
                          const KError **error) {
  if (ts == NULL || ts->head.kind.tag != TOKEN_RPAREN)
    return want("')'", ts, error);
  *out = b;
  *rest = ts->tail;
  return 1;
}

static int p_let_value(Text name, const SurfaceTerm *ty,
                       const SurfaceTokens *ts, const SurfaceTerm **out,
                       const SurfaceTokens **rest, const KError **error);
static int p_let_body(Text name, const SurfaceTerm *ty,
                      const SurfaceTerm *value, const SurfaceTokens *ts,
                      const SurfaceTerm **out, const SurfaceTokens **rest,
                      const KError **error);

/* ParseLetTerm (line 429). */
static int p_let_term(const SurfaceTokens *ts, const SurfaceTerm **out,
                      const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *ty;
  const SurfaceTokens *tail;
  if (ts == NULL || ts->head.kind.tag != TOKEN_IDENT || ts->tail == NULL ||
      ts->tail->head.kind.tag != TOKEN_COLON)
    return want("a name and ':' after 'let'", ts, error);
  if (!p_term(ts->tail->tail, &ty, &tail, error)) return 0;
  return p_let_value(ts->head.kind.as.ident, ty, tail, out, rest, error);
}

/* ParseLetValue (line 435). */
static int p_let_value(Text name, const SurfaceTerm *ty,
                       const SurfaceTokens *ts, const SurfaceTerm **out,
                       const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *value;
  const SurfaceTokens *tail;
  if (ts == NULL || ts->head.kind.tag != TOKEN_COLON_EQ)
    return want("':='", ts, error);
  if (!p_term(ts->tail, &value, &tail, error)) return 0;
  return p_let_body(name, ty, value, tail, out, rest, error);
}

/* ParseLetBody (line 441). */
static int p_let_body(Text name, const SurfaceTerm *ty,
                      const SurfaceTerm *value, const SurfaceTokens *ts,
                      const SurfaceTerm **out, const SurfaceTokens **rest,
                      const KError **error) {
  const SurfaceTerm *body;
  if (ts == NULL || ts->head.kind.tag != TOKEN_KIN)
    return want("'in'", ts, error);
  if (!p_term(ts->tail, &body, rest, error)) return 0;
  *out = surface_syntax_slet(name, ty, value, body);
  return 1;
}

static int p_case_motive(Elimination form, const SurfaceTerm *scrut,
                         const SurfaceTokens *ts, const SurfaceTerm **out,
                         const SurfaceTokens **rest, const KError **error);

/* ParseCaseTerm (line 447). */
static int p_case_term(Elimination form, const SurfaceTokens *ts,
                       const SurfaceTerm **out, const SurfaceTokens **rest,
                       const KError **error) {
  const SurfaceTerm *scrut;
  const SurfaceTokens *tail;
  if (!p_term(ts, &scrut, &tail, error)) return 0;
  return p_case_motive(form, scrut, tail, out, rest, error);
}

static int p_index(const SurfaceTokens *ts, IndexClause *out,
                   const SurfaceTokens **rest, const KError **error);
static int p_motive_return(Elimination form, const SurfaceTerm *scrut,
                           Text name, IndexClause clause,
                           const SurfaceTokens *ts, const SurfaceTerm **out,
                           const SurfaceTokens **rest, const KError **error);
static int p_branches(Elimination form, const SurfaceTokens *ts,
                      const SurfaceBranches *acc, const SurfaceBranches **out,
                      const SurfaceTokens **rest, const KError **error);

/* ParseCaseMotive (line 449). */
static int p_case_motive(Elimination form, const SurfaceTerm *scrut,
                         const SurfaceTokens *ts, const SurfaceTerm **out,
                         const SurfaceTokens **rest, const KError **error) {
  if (ts != NULL && ts->head.kind.tag == TOKEN_KAS) {
    if (ts->tail != NULL && ts->tail->head.kind.tag == TOKEN_IDENT) {
      IndexClause clause;
      const SurfaceTokens *tail;
      if (!p_index(ts->tail->tail, &clause, &tail, error)) return 0;
      return p_motive_return(form, scrut, ts->tail->head.kind.as.ident,
                             clause, tail, out, rest, error);
    }
    return fail(ts->head.loc,
                "expected 'NAME [in FAMILY IDX..] return TYPE' after 'as'",
                error);
  }
  if (ts != NULL && ts->head.kind.tag == TOKEN_LBRACE) {
    const SurfaceBranches *bs;
    if (!p_branches(form, ts->tail, NULL, &bs, rest, error)) return 0;
    *out = elimination_node(form, scrut, NULL, bs);
    return 1;
  }
  return want("'as' or '{'", ts, error);
}

static int p_names(const SurfaceTokens *ts, const Texts *acc,
                   const Texts **out, const SurfaceTokens **rest,
                   const KError **error);

/* ParseIndex (line 459). */
static int p_index(const SurfaceTokens *ts, IndexClause *out,
                   const SurfaceTokens **rest, const KError **error) {
  if (ts != NULL && ts->head.kind.tag == TOKEN_KIN && ts->tail != NULL &&
      ts->tail->head.kind.tag == TOKEN_IDENT) {
    const Texts *ns;
    if (!p_names(ts->tail->tail, NULL, &ns, rest, error)) return 0;
    out->ind = maybe_text_some(ts->tail->head.kind.as.ident);
    out->idx = ns;
    return 1;
  }
  out->ind = maybe_text_none();
  out->idx = NULL;
  *rest = ts;
  return 1;
}

/* ParseNames (line 465): the self tail call is a loop. */
static int p_names(const SurfaceTokens *ts, const Texts *acc,
                   const Texts **out, const SurfaceTokens **rest,
                   const KError **error) {
  for (; ts != NULL && ts->head.kind.tag == TOKEN_IDENT; ts = ts->tail)
    acc = Texts_cons(ts->head.kind.as.ident, acc);
  *out = Texts_reverse(acc);
  *rest = ts;
  return 1;
}

static int p_motive_with(Elimination form, const SurfaceTerm *scrut,
                         Text name, IndexClause clause,
                         const SurfaceTerm *body, const SurfaceTokens *ts,
                         const SurfaceTerm **out, const SurfaceTokens **rest,
                         const KError **error);

/* ParseMotiveReturn (line 471). */
static int p_motive_return(Elimination form, const SurfaceTerm *scrut,
                           Text name, IndexClause clause,
                           const SurfaceTokens *ts, const SurfaceTerm **out,
                           const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *body;
  const SurfaceTokens *tail;
  if (ts == NULL || ts->head.kind.tag != TOKEN_KRETURN)
    return want("'return'", ts, error);
  if (!p_term(ts->tail, &body, &tail, error)) return 0;
  return p_motive_with(form, scrut, name, clause, body, tail, out, rest,
                       error);
}

/* ParseMotiveWith (line 477). */
static int p_motive_with(Elimination form, const SurfaceTerm *scrut,
                         Text name, IndexClause clause,
                         const SurfaceTerm *body, const SurfaceTokens *ts,
                         const SurfaceTerm **out, const SurfaceTokens **rest,
                         const KError **error) {
  const SurfaceBranches *bs;
  if (ts == NULL || ts->head.kind.tag != TOKEN_LBRACE)
    return want("'{'", ts, error);
  if (!p_branches(form, ts->tail, NULL, &bs, rest, error)) return 0;
  *out = elimination_node(
      form, scrut, surface_syntax_motive(name, clause.ind, clause.idx, body),
      bs);
  return 1;
}

static int p_default_body(const SurfaceTokens *ts,
                          const SurfaceBranches *acc,
                          const SurfaceBranches **out,
                          const SurfaceTokens **rest, const KError **error);
static int p_native_arm_close(Elimination form, const SurfaceTerm *body,
                              const SurfaceTokens *ts,
                              const SurfaceBranches *acc,
                              const SurfaceBranches **out,
                              const SurfaceTokens **rest,
                              const KError **error);

/* ParseBranches (line 485): with ParseNativeArmClose, C recursion on the
   arm count. */
static int p_branches(Elimination form, const SurfaceTokens *ts,
                      const SurfaceBranches *acc, const SurfaceBranches **out,
                      const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *body;
  const SurfaceTokens *tail;
  if (ts != NULL && ts->head.kind.tag == TOKEN_RBRACE) {
    *out = SurfaceBranches_reverse(acc);
    *rest = ts->tail;
    return 1;
  }
  if (ts != NULL && ts->head.kind.tag == TOKEN_KELSE)
    return p_default_body(ts->tail, acc, out, rest, error);
  if (!p_term(ts, &body, &tail, error)) return 0;
  return p_native_arm_close(form, body, tail, acc, out, rest, error);
}

static int p_default_arm_close(const SurfaceTerm *body,
                               const SurfaceTokens *ts,
                               const SurfaceBranches *acc,
                               const SurfaceBranches **out,
                               const SurfaceTokens **rest,
                               const KError **error);

/* ParseDefaultBody (line 493). */
static int p_default_body(const SurfaceTokens *ts,
                          const SurfaceBranches *acc,
                          const SurfaceBranches **out,
                          const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *body;
  const SurfaceTokens *tail;
  if (ts == NULL || ts->head.kind.tag != TOKEN_COLON)
    return want("':' after 'else'", ts, error);
  if (!p_term(ts->tail, &body, &tail, error)) return 0;
  return p_default_arm_close(body, tail, acc, out, rest, error);
}

/* ParseDefaultArmClose (line 498). */
static int p_default_arm_close(const SurfaceTerm *body,
                               const SurfaceTokens *ts,
                               const SurfaceBranches *acc,
                               const SurfaceBranches **out,
                               const SurfaceTokens **rest,
                               const KError **error) {
  const SurfaceBranches *all =
      SurfaceBranches_cons(surface_syntax_br_default(body), acc);
  if (ts != NULL && ts->head.kind.tag == TOKEN_SEMICOLON) {
    if (ts->tail == NULL || ts->tail->head.kind.tag != TOKEN_RBRACE)
      return want("'}' after the final default elim arm", ts->tail, error);
    *out = SurfaceBranches_reverse(all);
    *rest = ts->tail->tail;
    return 1;
  }
  if (ts != NULL && ts->head.kind.tag == TOKEN_RBRACE) {
    *out = SurfaceBranches_reverse(all);
    *rest = ts->tail;
    return 1;
  }
  return want("';' or '}' after the final default elim arm", ts, error);
}

/* ParseNativeArmClose (line 508). */
static int p_native_arm_close(Elimination form, const SurfaceTerm *body,
                              const SurfaceTokens *ts,
                              const SurfaceBranches *acc,
                              const SurfaceBranches **out,
                              const SurfaceTokens **rest,
                              const KError **error) {
  SurfaceBranch br;
  if (ts != NULL && ts->head.kind.tag == TOKEN_SEMICOLON) {
    if (!native_branch(ts->head.loc, body, acc, &br, error)) return 0;
    return p_branches(form, ts->tail, SurfaceBranches_cons(br, acc), out,
                      rest, error);
  }
  if (ts != NULL && ts->head.kind.tag == TOKEN_RBRACE) {
    if (!native_branch(ts->head.loc, body, acc, &br, error)) return 0;
    *out = SurfaceBranches_reverse(SurfaceBranches_cons(br, acc));
    *rest = ts->tail;
    return 1;
  }
  return want("';' or '}' after an elim arm", ts, error);
}

/* ParseLegBody, ParseCtorBody (lines 516 and 522), ParseFields,
   ParseTypedField and ParseUntypedField (lines 528 to 543) have no C twin:
   no dispatch site in parser.bend reaches them (ParseFields is called only
   from the two field tasks). */

static int p_attempt_arrow(const SurfaceTokens *original,
                           const SurfaceBinder *b,
                           const SurfaceTokens *b_rest,
                           const SurfaceTerm **out,
                           const SurfaceTokens **rest, const KError **error);
static int p_arrow_plain(const SurfaceTokens *ts, const SurfaceTerm **out,
                         const SurfaceTokens **rest, const KError **error);

/* ParseArrow (line 544): the binder attempt is strict, as in Bend; its
   error is dropped. */
static int p_arrow(const SurfaceTokens *ts, const SurfaceTerm **out,
                   const SurfaceTokens **rest, const KError **error) {
  if (ts != NULL && ts->head.kind.tag == TOKEN_LPAREN) {
    SurfaceBinder b;
    const SurfaceTokens *b_rest = NULL;
    const KError *dropped = NULL;
    if (p_binder(ts, &b, &b_rest, &dropped))
      return p_attempt_arrow(ts, &b, b_rest, out, rest, error);
    return p_attempt_arrow(ts, NULL, NULL, out, rest, error);
  }
  return p_arrow_plain(ts, out, rest, error);
}

static int p_group_op(SurfaceBinder b, const SurfaceTokens *ts,
                      const SurfaceTerm **out, const SurfaceTokens **rest,
                      const KError **error);

/* ParseAttemptArrow (line 550): value = parsed(S.binder) is b (NULL =
   Fail) and b_rest. */
static int p_attempt_arrow(const SurfaceTokens *original,
                           const SurfaceBinder *b,
                           const SurfaceTokens *b_rest,
                           const SurfaceTerm **out,
                           const SurfaceTokens **rest, const KError **error) {
  if (b != NULL && b_rest != NULL &&
      (b_rest->head.kind.tag == TOKEN_ARROW ||
       b_rest->head.kind.tag == TOKEN_STAR))
    return p_group_op(*b, b_rest, out, rest, error);
  return p_arrow_plain(original, out, rest, error);
}

/* ParseGroupOp (line 562). */
static int p_group_op(SurfaceBinder b, const SurfaceTokens *ts,
                      const SurfaceTerm **out, const SurfaceTokens **rest,
                      const KError **error) {
  const SurfaceTerm *body;
  if (ts != NULL && ts->head.kind.tag == TOKEN_ARROW) {
    if (!p_term(ts->tail, &body, rest, error)) return 0;
    *out = surface_syntax_sarrow(b, body);
    return 1;
  }
  if (ts != NULL && ts->head.kind.tag == TOKEN_STAR) {
    if (!p_term(ts->tail, &body, rest, error)) return 0;
    *out = surface_syntax_sstar(b, body);
    return 1;
  }
  return want("'->' or '*'", ts, error);
}

static int p_app(const SurfaceTokens *ts, const SurfaceTerm **out,
                 const SurfaceTokens **rest, const KError **error);
static int p_arrow_tail(const SurfaceTerm *lhs, const SurfaceTokens *ts,
                        const SurfaceTerm **out, const SurfaceTokens **rest,
                        const KError **error);

/* ParseArrowPlain (line 570). */
static int p_arrow_plain(const SurfaceTokens *ts, const SurfaceTerm **out,
                         const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *lhs;
  const SurfaceTokens *tail;
  if (!p_app(ts, &lhs, &tail, error)) return 0;
  return p_arrow_tail(lhs, tail, out, rest, error);
}

/* ParseArrowTail (line 572). */
static int p_arrow_tail(const SurfaceTerm *lhs, const SurfaceTokens *ts,
                        const SurfaceTerm **out, const SurfaceTokens **rest,
                        const KError **error) {
  const SurfaceTerm *rhs;
  SurfaceBinder anon =
      surface_syntax_binder(QUANTITY_MANY, text_of_cstring("_"), lhs);
  if (ts != NULL && ts->head.kind.tag == TOKEN_ARROW) {
    if (!p_term(ts->tail, &rhs, rest, error)) return 0;
    *out = surface_syntax_sarrow(anon, rhs);
    return 1;
  }
  if (ts != NULL && ts->head.kind.tag == TOKEN_STAR) {
    if (!p_term(ts->tail, &rhs, rest, error)) return 0;
    *out = surface_syntax_sstar(anon, rhs);
    return 1;
  }
  *out = lhs;
  *rest = ts;
  return 1;
}

static int p_inj(const SurfaceTokens *ts, const SurfaceTerm **out,
                 const SurfaceTokens **rest, const KError **error);
static int p_atom(const SurfaceTokens *ts, const SurfaceTerm **out,
                  const SurfaceTokens **rest, const KError **error);
static int p_app_rest(const SurfaceTerm *head, const SurfaceTokens *ts,
                      const SurfaceTerm **out, const SurfaceTokens **rest,
                      const KError **error);

/* ParseApp (line 580). */
static int p_app(const SurfaceTokens *ts, const SurfaceTerm **out,
                 const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *a;
  const SurfaceTokens *tail;
  if (ts != NULL && ts->head.kind.tag == TOKEN_KINJ)
    return p_inj(ts->tail, out, rest, error);
  if (ts != NULL && ts->head.kind.tag == TOKEN_KABSURD) {
    if (!p_app(ts->tail, &a, rest, error)) return 0;
    *out = surface_syntax_sabsurd(a);
    return 1;
  }
  if (!p_atom(ts, &a, &tail, error)) return 0;
  return p_app_rest(a, tail, out, rest, error);
}

/* ParseInj (line 588): T.Number{k} kloc, KOf, T.Number{n} nloc. */
static int p_inj(const SurfaceTokens *ts, const SurfaceTerm **out,
                 const SurfaceTokens **rest, const KError **error) {
  Int63 key;
  Int63 size;
  const SurfaceTerm *a;
  if (ts == NULL || ts->head.kind.tag != TOKEN_NUMBER || ts->tail == NULL ||
      ts->tail->head.kind.tag != TOKEN_KOF || ts->tail->tail == NULL ||
      ts->tail->tail->head.kind.tag != TOKEN_NUMBER)
    return want("'K of N' after 'inj'", ts, error);
  if (!bounded(ts->head.loc, ts->head.kind.as.number, &key, error)) return 0;
  if (!bounded(ts->tail->tail->head.loc, ts->tail->tail->head.kind.as.number,
               &size, error))
    return 0;
  if (!p_app(ts->tail->tail->tail, &a, rest, error)) return 0;
  *out = surface_syntax_sinj(key, size, a);
  return 1;
}

/* ParseAppRest (line 596): the self tail call is a loop. */
static int p_app_rest(const SurfaceTerm *head, const SurfaceTokens *ts,
                      const SurfaceTerm **out, const SurfaceTokens **rest,
                      const KError **error) {
  while (starts_atom(ts)) {
    const SurfaceTerm *arg;
    if (!p_atom(ts, &arg, &ts, error)) return 0;
    head = surface_syntax_sapp(head, arg);
  }
  *out = head;
  *rest = ts;
  return 1;
}

static int p_atom_head(const SurfaceTokens *ts, const SurfaceTerm **out,
                       const SurfaceTokens **rest, const KError **error);
static int p_postfix(const SurfaceTerm *a, const SurfaceTokens *ts,
                     const SurfaceTerm **out, const SurfaceTokens **rest,
                     const KError **error);

/* ParseAtom (line 600). */
static int p_atom(const SurfaceTokens *ts, const SurfaceTerm **out,
                  const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *a;
  const SurfaceTokens *tail;
  if (!p_atom_head(ts, &a, &tail, error)) return 0;
  return p_postfix(a, tail, out, rest, error);
}

/* ParsePostfix (line 602): the self tail call is a loop. The Dot2 key is
   F.Int63.succ(F.Int63.one) = 2. */
static int p_postfix(const SurfaceTerm *a, const SurfaceTokens *ts,
                     const SurfaceTerm **out, const SurfaceTokens **rest,
                     const KError **error) {
  for (;;) {
    const SurfaceTokens *next;
    Int63 key;
    if (ts == NULL || (ts->head.kind.tag != TOKEN_DOT1 &&
                       ts->head.kind.tag != TOKEN_DOT2 &&
                       ts->head.kind.tag != TOKEN_DOT)) {
      *out = a;
      *rest = ts;
      return 1;
    }
    next = ts->tail;
    if (ts->head.kind.tag == TOKEN_DOT1) {
      a = surface_syntax_sproj(a, int63_one());
      ts = next;
      continue;
    }
    if (ts->head.kind.tag == TOKEN_DOT2) {
      a = surface_syntax_sproj(a, int63_succ(int63_one()));
      ts = next;
      continue;
    }
    if (next != NULL && next->head.kind.tag == TOKEN_NUMBER) {
      if (!bounded(next->head.loc, next->head.kind.as.number, &key, error))
        return 0;
      a = surface_syntax_sproj(a, key);
      ts = next->tail;
      continue;
    }
    if (next == NULL || next->head.kind.tag != TOKEN_IDENT)
      return fail(ts->head.loc,
                  "expected a field name or leg number after '.'", error);
    if (!surface_record_qualify(a, next->head.kind.as.ident, ts->head.loc,
                                &a, error))
      return 0;
    ts = next->tail;
  }
}

static int p_items(Text word, const SurfaceTokens *ts,
                   const SurfaceTerm **out, const SurfaceTokens **rest,
                   const KError **error);
static int p_paren(const SurfaceTokens *ts, const SurfaceTerm **out,
                   const SurfaceTokens **rest, const KError **error);

/* ParseAtomHead (line 616): exhaustive; the want arm is the last group. */
static int p_atom_head(const SurfaceTokens *ts, const SurfaceTerm **out,
                       const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *t;
  Int63 level;
  if (ts == NULL) return want("a term", ts, error);
  switch (ts->head.kind.tag) {
    case TOKEN_IDENT:
      t = surface_syntax_svar(ts->head.kind.as.ident);
      break;
    case TOKEN_NUMBER:
      t = surface_syntax_snat(ts->head.kind.as.number);
      break;
    case TOKEN_BYTES:
      t = bytes_term(ts->head.kind.as.bytes);
      break;
    case TOKEN_KPROP:
      t = surface_syntax_sprop();
      break;
    case TOKEN_KTYPE:
      if (ts->tail == NULL || ts->tail->head.kind.tag != TOKEN_NUMBER) {
        t = surface_syntax_stype(int63_zero());
        break;
      }
      if (!bounded(ts->tail->head.loc, ts->tail->head.kind.as.number, &level,
                   error))
        return 0;
      *out = surface_syntax_stype(level);
      *rest = ts->tail->tail;
      return 1;
    case TOKEN_KAUTO:
      t = surface_syntax_sauto();
      break;
    case TOKEN_UNIT:
      t = surface_syntax_sunit();
      break;
    case TOKEN_KNAT_ADD:
      t = surface_syntax_sprim(SURFACE_PRIM_ADD);
      break;
    case TOKEN_KNAT_SUB:
      t = surface_syntax_sprim(SURFACE_PRIM_SUB);
      break;
    case TOKEN_KNAT_MUL:
      t = surface_syntax_sprim(SURFACE_PRIM_MUL);
      break;
    case TOKEN_KNAT_EQ:
      t = surface_syntax_sprim(SURFACE_PRIM_EQ);
      break;
    case TOKEN_KNAT_LT:
      t = surface_syntax_sprim(SURFACE_PRIM_LT);
      break;
    case TOKEN_KTUPLE:
      return p_items(text_of_cstring("tuple"), ts->tail, out, rest, error);
    case TOKEN_KSUM:
      return p_items(text_of_cstring("sum"), ts->tail, out, rest, error);
    case TOKEN_KPROD:
      return p_items(text_of_cstring("prod"), ts->tail, out, rest, error);
    case TOKEN_KMU:
      return fail(ts->head.loc, "a mu group is a declaration and not a term",
                  error);
    case TOKEN_KAND:
      return fail(ts->head.loc,
                  "'and' joins two members of a mu group and is not a term",
                  error);
    case TOKEN_KNU:
      return fail(ts->head.loc, "nu arrives at M2", error);
    case TOKEN_LPAREN:
      return p_paren(ts->tail, out, rest, error);
    case TOKEN_KFUN:
    case TOKEN_KLET:
    case TOKEN_KELIM:
    case TOKEN_KMATCH:
    case TOKEN_RPAREN:
    case TOKEN_LBRACE:
    case TOKEN_RBRACE:
    case TOKEN_SEMICOLON:
    case TOKEN_KELSE:
    case TOKEN_KRECORD:
    case TOKEN_COLON:
    case TOKEN_COLON_EQ:
    case TOKEN_ARROW:
    case TOKEN_DARROW:
    case TOKEN_STAR:
    case TOKEN_COMMA:
    case TOKEN_DOT:
    case TOKEN_DOT1:
    case TOKEN_DOT2:
    case TOKEN_PIPE:
    case TOKEN_KDEF:
    case TOKEN_KAXIOM:
    case TOKEN_KINJ:
    case TOKEN_KOF:
    case TOKEN_KCASE:
    case TOKEN_KAS:
    case TOKEN_KRETURN:
    case TOKEN_KWITH:
    case TOKEN_KABSURD:
    case TOKEN_KIN:
    case TOKEN_KMUTUAL:
    case TOKEN_KEND:
    case TOKEN_KREC:
    case TOKEN_EOF:
      return want("a term", ts, error);
  }
  *out = t;
  *rest = ts->tail;
  return 1;
}

static int p_paren_tail(const SurfaceTerm *inner, const SurfaceTokens *ts,
                        const SurfaceTerm **out, const SurfaceTokens **rest,
                        const KError **error);
static int p_close_term(const SurfaceTerm *value, const SurfaceTokens *ts,
                        const SurfaceTerm **out, const SurfaceTokens **rest,
                        const KError **error);

/* ParseParen (line 660). */
static int p_paren(const SurfaceTokens *ts, const SurfaceTerm **out,
                   const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *inner;
  const SurfaceTokens *tail;
  if (!p_term(ts, &inner, &tail, error)) return 0;
  return p_paren_tail(inner, tail, out, rest, error);
}

/* ParseParenTail (line 662). */
static int p_paren_tail(const SurfaceTerm *inner, const SurfaceTokens *ts,
                        const SurfaceTerm **out, const SurfaceTokens **rest,
                        const KError **error) {
  const SurfaceTerm *t;
  const SurfaceTokens *tail;
  if (ts != NULL && ts->head.kind.tag == TOKEN_RPAREN) {
    *out = inner;
    *rest = ts->tail;
    return 1;
  }
  if (ts != NULL && ts->head.kind.tag == TOKEN_COMMA) {
    if (!p_term(ts->tail, &t, &tail, error)) return 0;
    return p_close_term(surface_syntax_spair(inner, t), tail, out, rest,
                        error);
  }
  if (ts != NULL && ts->head.kind.tag == TOKEN_COLON) {
    if (!p_term(ts->tail, &t, &tail, error)) return 0;
    return p_close_term(surface_syntax_sann(inner, t), tail, out, rest, error);
  }
  return want("')', ',' or ':'", ts, error);
}

/* ParseCloseTerm (line 672). */
static int p_close_term(const SurfaceTerm *value, const SurfaceTokens *ts,
                        const SurfaceTerm **out, const SurfaceTokens **rest,
                        const KError **error) {
  if (ts == NULL || ts->head.kind.tag != TOKEN_RPAREN)
    return want("')'", ts, error);
  *out = value;
  *rest = ts->tail;
  return 1;
}

static int p_items_body(Text word, const SurfaceTokens *ts,
                        const SurfaceTerm **out, const SurfaceTokens **rest,
                        const KError **error);
static int p_items_rest(Text word, const SurfaceTokens *ts,
                        const SurfaceTerms *acc, const SurfaceTerm **out,
                        const SurfaceTokens **rest, const KError **error);
static int p_items_tail(Text word, const SurfaceTokens *ts,
                        const SurfaceTerms *acc, const SurfaceTerm **out,
                        const SurfaceTokens **rest, const KError **error);

/* ParseItems (line 678). */
static int p_items(Text word, const SurfaceTokens *ts,
                   const SurfaceTerm **out, const SurfaceTokens **rest,
                   const KError **error) {
  if (ts != NULL && ts->head.kind.tag == TOKEN_UNIT) {
    *out = collection(word, NULL);
    *rest = ts->tail;
    return 1;
  }
  if (ts != NULL && ts->head.kind.tag == TOKEN_LPAREN)
    return p_items_body(word, ts->tail, out, rest, error);
  {
    Text parts[3];
    parts[0] = text_of_cstring("'(' after '");
    parts[1] = word;
    parts[2] = text_of_cstring("'");
    return want_text(cat(parts, 3), ts, error);
  }
}

/* ParseItemsBody (line 686). */
static int p_items_body(Text word, const SurfaceTokens *ts,
                        const SurfaceTerm **out, const SurfaceTokens **rest,
                        const KError **error) {
  if (ts != NULL && ts->head.kind.tag == TOKEN_RPAREN) {
    *out = collection(word, NULL);
    *rest = ts->tail;
    return 1;
  }
  return p_items_rest(word, ts, NULL, out, rest, error);
}

/* ParseItemsRest (line 692): with ParseItemsTail, C recursion on the item
   count. */
static int p_items_rest(Text word, const SurfaceTokens *ts,
                        const SurfaceTerms *acc, const SurfaceTerm **out,
                        const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *item;
  const SurfaceTokens *tail;
  if (!p_term(ts, &item, &tail, error)) return 0;
  return p_items_tail(word, tail, SurfaceTerms_cons(item, acc), out, rest,
                      error);
}

/* ParseItemsTail (line 694). */
static int p_items_tail(Text word, const SurfaceTokens *ts,
                        const SurfaceTerms *acc, const SurfaceTerm **out,
                        const SurfaceTokens **rest, const KError **error) {
  if (ts != NULL && ts->head.kind.tag == TOKEN_COMMA)
    return p_items_rest(word, ts->tail, acc, out, rest, error);
  if (ts != NULL && ts->head.kind.tag == TOKEN_RPAREN) {
    *out = collection(word, SurfaceTerms_reverse(acc));
    *rest = ts->tail;
    return 1;
  }
  return want("',' or ')'", ts, error);
}

/* The list patterns of ParseDecl and ParseRecMember: the first count
   tokens of ts have the tags in order. */
static Bool has_tags(const SurfaceTokens *ts, const SurfaceTokenTag *tags,
                     U32 count) {
  for (; count > 0; count--, tags++, ts = ts->tail)
    if (ts == NULL || ts->head.kind.tag != *tags) return NO;
  return YES;
}

/* The tokens of ts after the first n (has_tags gave YES for n tags). */
static const SurfaceTokens *drop(const SurfaceTokens *ts, U32 n) {
  for (; n > 0; n--) ts = ts->tail;
  return ts;
}

/* S.cat([namespace, ".", field]) of the pattern Ident Dot Ident at ts. */
static Text field_name(const SurfaceTokens *ts) {
  Text parts[3];
  parts[0] = ts->head.kind.as.ident;
  parts[1] = text_of_cstring(".");
  parts[2] = ts->tail->tail->head.kind.as.ident;
  return cat(parts, 3);
}

static int p_def_body(Text name, const SurfaceTerm *ty,
                      const SurfaceTokens *ts, const SurfaceDecl **out,
                      const SurfaceTokens **rest, const KError **error);
static int p_rec_group(const SurfaceTokens *ts, const SurfaceRecDefs *acc,
                       const SurfaceRecDefs **out,
                       const SurfaceTokens **rest, const KError **error);
static int p_fam_group(const SurfaceTokens *ts, const SurfaceFams *acc,
                       const SurfaceFams **out, const SurfaceTokens **rest,
                       const KError **error);
static int p_mutual_group(const SurfaceTokens *ts, const SurfaceFams *acc,
                          const SurfaceFams **out,
                          const SurfaceTokens **rest, const KError **error);

/* ParseDecl (line 702): the Bend arms in order. */
static int p_decl(const SurfaceTokens *ts, const SurfaceDecl **out,
                  const SurfaceTokens **rest, const KError **error) {
  static const SurfaceTokenTag def_field[5] = {
      TOKEN_KDEF, TOKEN_IDENT, TOKEN_DOT, TOKEN_IDENT, TOKEN_COLON};
  static const SurfaceTokenTag axiom_field[5] = {
      TOKEN_KAXIOM, TOKEN_IDENT, TOKEN_DOT, TOKEN_IDENT, TOKEN_COLON};
  static const SurfaceTokenTag def_rec[2] = {TOKEN_KDEF, TOKEN_KREC};
  static const SurfaceTokenTag def_name[3] = {TOKEN_KDEF, TOKEN_IDENT,
                                              TOKEN_COLON};
  static const SurfaceTokenTag axiom_name[3] = {TOKEN_KAXIOM, TOKEN_IDENT,
                                                TOKEN_COLON};
  const SurfaceTerm *ty;
  const SurfaceTokens *tail;
  const SurfaceRecDefs *ms;
  const SurfaceFams *fs;
  if (has_tags(ts, def_field, 5)) {
    if (!p_term(drop(ts, 5), &ty, &tail, error)) return 0;
    return p_def_body(field_name(ts->tail), ty, tail, out, rest, error);
  }
  if (has_tags(ts, axiom_field, 5)) {
    if (!p_term(drop(ts, 5), &ty, rest, error)) return 0;
    *out = surface_syntax_daxiom(field_name(ts->tail), ty);
    return 1;
  }
  if (has_tags(ts, def_rec, 2)) {
    if (!p_rec_group(drop(ts, 2), NULL, &ms, rest, error)) return 0;
    *out = surface_syntax_drec(ms);
    return 1;
  }
  if (has_tags(ts, def_name, 3)) {
    if (!p_term(drop(ts, 3), &ty, &tail, error)) return 0;
    return p_def_body(ts->tail->head.kind.as.ident, ty, tail, out, rest,
                      error);
  }
  if (has_tags(ts, axiom_name, 3)) {
    if (!p_term(drop(ts, 3), &ty, rest, error)) return 0;
    *out = surface_syntax_daxiom(ts->tail->head.kind.as.ident, ty);
    return 1;
  }
  if (ts != NULL && ts->head.kind.tag == TOKEN_KMU) {
    if (!p_fam_group(ts->tail, NULL, &fs, rest, error)) return 0;
    *out = surface_syntax_dmu(fs);
    return 1;
  }
  if (ts != NULL && ts->head.kind.tag == TOKEN_KMUTUAL) {
    if (!p_mutual_group(ts->tail, NULL, &fs, rest, error)) return 0;
    *out = surface_syntax_dmu(fs);
    return 1;
  }
  if (ts != NULL && ts->head.kind.tag == TOKEN_KNU)
    return fail(ts->head.loc, "nu arrives at M2", error);
  return want("'def NAME :', 'def NAME.FIELD :', 'def rec NAME :', "
              "'axiom NAME :', 'axiom NAME.FIELD :', 'record NAME :', "
              "'mu NAME' or 'mutual'",
              ts, error);
}

/* ParseDefBody (line 722). */
static int p_def_body(Text name, const SurfaceTerm *ty,
                      const SurfaceTokens *ts, const SurfaceDecl **out,
                      const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *body;
  if (ts == NULL || ts->head.kind.tag != TOKEN_COLON_EQ)
    return want("':='", ts, error);
  if (!p_term(ts->tail, &body, rest, error)) return 0;
  *out = surface_syntax_ddef(name, ty, body);
  return 1;
}

static int p_rec_tail(const SurfaceTokens *ts, const SurfaceRecDefs *acc,
                      const SurfaceRecDefs **out, const SurfaceTokens **rest,
                      const KError **error);
static int p_rec_member(const SurfaceTokens *ts, SurfaceRecDef *out,
                        const SurfaceTokens **rest, const KError **error);

/* ParseRecGroup (line 728): with ParseRecTail, C recursion on the member
   count. */
static int p_rec_group(const SurfaceTokens *ts, const SurfaceRecDefs *acc,
                       const SurfaceRecDefs **out,
                       const SurfaceTokens **rest, const KError **error) {
  SurfaceRecDef m;
  const SurfaceTokens *tail;
  if (!p_rec_member(ts, &m, &tail, error)) return 0;
  return p_rec_tail(tail, SurfaceRecDefs_cons(m, acc), out, rest, error);
}

/* ParseRecTail (line 730). */
static int p_rec_tail(const SurfaceTokens *ts, const SurfaceRecDefs *acc,
                      const SurfaceRecDefs **out, const SurfaceTokens **rest,
                      const KError **error) {
  if (ts != NULL && ts->head.kind.tag == TOKEN_KAND)
    return p_rec_group(ts->tail, acc, out, rest, error);
  *out = SurfaceRecDefs_reverse(acc);
  *rest = ts;
  return 1;
}

static int p_rec_body(Text name, const SurfaceTerm *ty,
                      const SurfaceTokens *ts, SurfaceRecDef *out,
                      const SurfaceTokens **rest, const KError **error);

/* ParseRecMember (line 736). */
static int p_rec_member(const SurfaceTokens *ts, SurfaceRecDef *out,
                        const SurfaceTokens **rest, const KError **error) {
  static const SurfaceTokenTag name_colon[2] = {TOKEN_IDENT, TOKEN_COLON};
  const SurfaceTerm *ty;
  const SurfaceTokens *tail;
  if (!has_tags(ts, name_colon, 2))
    return want("a definition name and ':'", ts, error);
  if (!p_term(drop(ts, 2), &ty, &tail, error)) return 0;
  return p_rec_body(ts->head.kind.as.ident, ty, tail, out, rest, error);
}

/* ParseRecBody (line 742). */
static int p_rec_body(Text name, const SurfaceTerm *ty,
                      const SurfaceTokens *ts, SurfaceRecDef *out,
                      const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *body;
  if (ts == NULL || ts->head.kind.tag != TOKEN_COLON_EQ)
    return want("':='", ts, error);
  if (!p_term(ts->tail, &body, rest, error)) return 0;
  *out = surface_syntax_rec_def(name, ty, body);
  return 1;
}

static int p_fam_tail(const SurfaceTokens *ts, const SurfaceFams *acc,
                      const SurfaceFams **out, const SurfaceTokens **rest,
                      const KError **error);
static int p_fam(const SurfaceTokens *ts, const Texts *seen, SurfaceFam *out,
                 const SurfaceTokens **rest, const KError **error);

/* ParseFamGroup (line 748): with ParseFamTail, C recursion on the family
   count. Family.group_names is surface_family_group_names. */
static int p_fam_group(const SurfaceTokens *ts, const SurfaceFams *acc,
                       const SurfaceFams **out, const SurfaceTokens **rest,
                       const KError **error) {
  SurfaceFam fm;
  const SurfaceTokens *tail;
  if (!p_fam(ts, surface_family_group_names(acc), &fm, &tail, error))
    return 0;
  return p_fam_tail(tail, SurfaceFams_cons(fm, acc), out, rest, error);
}

/* ParseFamTail (line 750). */
static int p_fam_tail(const SurfaceTokens *ts, const SurfaceFams *acc,
                      const SurfaceFams **out, const SurfaceTokens **rest,
                      const KError **error) {
  if (ts != NULL && ts->head.kind.tag == TOKEN_KAND)
    return p_fam_group(ts->tail, acc, out, rest, error);
  *out = SurfaceFams_reverse(acc);
  *rest = ts;
  return 1;
}

/* ParseMutualGroup (line 756): the self tail call is a loop. The two
   short arms of match acc (line 761) give the same error: fewer than two
   members. */
static int p_mutual_group(const SurfaceTokens *ts, const SurfaceFams *acc,
                          const SurfaceFams **out,
                          const SurfaceTokens **rest, const KError **error) {
  while (ts != NULL && ts->head.kind.tag == TOKEN_KMU) {
    SurfaceFam fm;
    if (!p_fam(ts->tail, surface_family_group_names(acc), &fm, &ts, error))
      return 0;
    acc = SurfaceFams_cons(fm, acc);
  }
  if (ts == NULL || ts->head.kind.tag != TOKEN_KEND)
    return want("'mu' or 'end' in a mutual group", ts, error);
  if (acc == NULL || acc->tail == NULL)
    return fail(ts->head.loc,
                "a mutual group needs at least two mu declarations", error);
  *out = SurfaceFams_reverse(acc);
  *rest = ts->tail;
  return 1;
}

static int p_fam_type(Text name, const SurfaceBinders *params,
                      const SurfaceTokens *ts, const Texts *seen,
                      SurfaceFam *out, const SurfaceTokens **rest,
                      const KError **error);

/* ParseFam (line 770). */
static int p_fam(const SurfaceTokens *ts, const Texts *seen, SurfaceFam *out,
                 const SurfaceTokens **rest, const KError **error) {
  const SurfaceBinders *bs;
  const SurfaceTokens *tail;
  if (ts == NULL || ts->head.kind.tag != TOKEN_IDENT)
    return want("a family name", ts, error);
  if (!p_binders(ts->tail, NULL, &bs, &tail, error)) return 0;
  return p_fam_type(ts->head.kind.as.ident, bs, tail, seen, out, rest, error);
}

static int p_fam_constructors(Text name, const SurfaceBinders *params,
                              const SurfaceTerm *ty, const SurfaceTokens *ts,
                              const Texts *seen, SurfaceFam *out,
                              const SurfaceTokens **rest,
                              const KError **error);

/* ParseFamType (line 776). */
static int p_fam_type(Text name, const SurfaceBinders *params,
                      const SurfaceTokens *ts, const Texts *seen,
                      SurfaceFam *out, const SurfaceTokens **rest,
                      const KError **error) {
  const SurfaceTerm *ty;
  const SurfaceTokens *tail;
  if (ts == NULL || ts->head.kind.tag != TOKEN_COLON)
    return want("':'", ts, error);
  if (!p_term(ts->tail, &ty, &tail, error)) return 0;
  return p_fam_constructors(name, params, ty, tail, seen, out, rest, error);
}

/* The closure ~(tail => read_0(dispatch(ParseTerm{tail}))) of lines 785
   and 794 as a SurfaceParseTerm. The parser has no state: env is NULL. */
static int term_cb(const void *env, const SurfaceTokens *ts,
                   const SurfaceTerm **out, const SurfaceTokens **rest,
                   const KError **error) {
  return p_term(ts, out, rest, error);
}

/* ParseFamConstructors (line 782): Family.Read{Record.length(rest), rest,
   [], name <> seen} is the Read fields of surface_family_constructors. */
static int p_fam_constructors(Text name, const SurfaceBinders *params,
                              const SurfaceTerm *ty, const SurfaceTokens *ts,
                              const Texts *seen, SurfaceFam *out,
                              const SurfaceTokens **rest,
                              const KError **error) {
  const SurfaceFamCtors *cs;
  if (ts == NULL || ts->head.kind.tag != TOKEN_LBRACE)
    return want("'{' after a family header", ts, error);
  if (!surface_family_constructors(NULL, term_cb,
                                   surface_record_length(ts->tail), ts->tail,
                                   NULL, Texts_cons(name, seen), &cs, rest,
                                   error))
    return 0;
  *out = surface_syntax_fam(name, params, ty, cs);
  return 1;
}

/* The closure ~(tail => read_8(dispatch(ParseDecl{tail}))) of line 794 as
   a SurfaceParseDecl (env is NULL). */
static int decl_cb(const void *env, const SurfaceTokens *ts,
                   const SurfaceDecl **out, const SurfaceTokens **rest,
                   const KError **error) {
  return p_decl(ts, out, rest, error);
}

/* parse_admitted (line 793). finish (line 789) has no C twin: the tokens
   after the declarations are dropped here. */
int surface_parser_parse_admitted(const SurfaceTokens *ts,
                                  const SurfaceDecls **out,
                                  const KError **error) {
  const SurfaceTokens *rest;
  return surface_record_declarations(NULL, term_cb, decl_cb,
                                     surface_record_length(ts), ts, NULL, out,
                                     &rest, error);
}

/* parse_tokens (line 795). */
int surface_parser_parse_tokens(const SurfaceTokens *ts,
                                const SurfaceDecls **out,
                                const KError **error) {
  const SurfaceTokens *admitted;
  if (!surface_source_tokens(ts, &admitted, error)) return 0;
  return surface_parser_parse_admitted(admitted, out, error);
}

/* parse_bytes (line 797): the bytes are a Text slice. */
int surface_parser_parse_bytes(Text source, const SurfaceDecls **out,
                               const KError **error) {
  const SurfaceTokens *ts;
  if (!surface_lexer_lex_bytes(source, &ts, error)) return 0;
  return surface_parser_parse_tokens(ts, out, error);
}

/* parse (line 799). */
int surface_parser_parse(Text source, const SurfaceDecls **out,
                         const KError **error) {
  const SurfaceTokens *ts;
  if (!surface_lexer_lex(source, &ts, error)) return 0;
  return surface_parser_parse_tokens(ts, out, error);
}
