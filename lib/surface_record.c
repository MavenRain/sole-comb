/* lib/surface_record.c: twin of surface/record.bend (S5 session 2).
   Closed record declarations expand to a Ran product and qualified Out
   getters. parsed(A) = L.result(F.Pair2<A, tokens>) is an int return (1 =
   Done with *out and *rest, 0 = Fail with *error). next, L.bind and
   L.choose have no C twin: they are C if statements and returns. The
   @unsafe defs fields (a field_step state machine) and declarations are
   loops. */
#include "sole.h"

/* String.concat of a fixed list of parts (the cat idiom of kernel_rules). */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

/* field = Field{name, ty} (line 16): SurfaceRecordField(s) in sole.h. */

/* want (line 19). */
static int want(const char *what, const SurfaceTokens *ts,
                const KError **error) {
  if (ts == NULL) {
    *error = surface_lexer_error(
        surface_token_start(),
        text_of_cstring("unexpected end of record declaration"));
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

/* length (line 24): the right fold 1n + length(rest) is a loop. */
Nat surface_record_length(const SurfaceTokens *ts) {
  return SurfaceTokens_length(ts);
}

/* known (line 29): Bool.or is eager in Bend; a pure scan, so the loop stops
   at the first match with the same answer. */
static Bool known(Text name, const SurfaceRecordFields *fs) {
  for (; fs != NULL; fs = fs->tail)
    if (text_same(name, fs->head.name)) return YES;
  return NO;
}

/* types (line 34): a right fold, so a loop plus a reverse. */
static const SurfaceTerms *types(const SurfaceRecordFields *fs) {
  const SurfaceTerms *acc = NULL;
  for (; fs != NULL; fs = fs->tail) acc = SurfaceTerms_cons(fs->head.ty, acc);
  return SurfaceTerms_reverse(acc);
}

/* getter (line 39). */
static const SurfaceDecl *getter(Text name, Text label,
                                 const SurfaceTerm *ty, Int63 key,
                                 SurfaceBinder binder) {
  Text parts[3];
  parts[0] = name;
  parts[1] = text_of_cstring(".");
  parts[2] = label;
  return surface_syntax_ddef(
      cat(parts, 3), surface_syntax_sarrow(binder, ty),
      surface_syntax_sfun(
          SurfaceBinders_cons(binder, NULL),
          surface_syntax_sproj(
              surface_syntax_svar(text_of_cstring("#record")), key)));
}

/* getters (line 42): a right fold, so a loop plus a reverse. '#' cannot
   occur in a source identifier, so the binder cannot capture a field
   type's free name, including an ordinary global called value. */
static const SurfaceDecls *getters(Text name, const SurfaceRecordFields *fs,
                                   Int63 key) {
  const SurfaceDecls *acc = NULL;
  for (; fs != NULL; fs = fs->tail) {
    SurfaceBinder binder = surface_syntax_binder(
        QUANTITY_ONE, text_of_cstring("#record"), surface_syntax_svar(name));
    acc = SurfaceDecls_cons(
        getter(name, fs->head.name, fs->head.ty, key, binder), acc);
    key = int63_succ(key);
  }
  return SurfaceDecls_reverse(acc);
}

/* expand (line 50). */
static const SurfaceDecls *expand(Text name, const SurfaceTerm *sort,
                                  const SurfaceRecordFields *fs) {
  return SurfaceDecls_cons(
      surface_syntax_ddef(name, sort, surface_syntax_sprod(types(fs))),
      getters(name, fs, int63_zero()));
}

/* fields (@unsafe, line 59): the field_step states ReadFields, AfterType
   and AfterTail are one loop. Each pass of the loop is one ReadFields
   step; AfterType and AfterTail follow in the same pass. */
static int fields(const void *env, SurfaceParseTerm parse, Nat fuel,
                  const SurfaceTokens *ts, const SurfaceRecordFields *acc,
                  const SurfaceRecordFields **out, const SurfaceTokens **rest,
                  const KError **error) {
  for (;;) {
    Nat remaining;
    Text name;
    SurfaceLoc loc;
    const SurfaceTerm *ty;
    const SurfaceTokens *tail;
    SurfaceRecordField field;
    if (fuel == 0) return want("'}'", ts, error);
    remaining = fuel - 1;
    if (head_is(ts, TOKEN_RBRACE)) {
      *out = SurfaceRecordFields_reverse(acc);
      *rest = ts->tail;
      return 1;
    }
    if (!(head_is(ts, TOKEN_IDENT) && head_is(ts->tail, TOKEN_COLON)))
      return want("a record field 'name : type' or '}'", ts, error);
    name = ts->head.kind.as.ident;
    loc = ts->head.loc;
    if (known(name, acc)) {
      Text parts[2];
      parts[0] = text_of_cstring("E-RECORD-FIELD: duplicate field ");
      parts[1] = name;
      *error = surface_lexer_error(loc, cat(parts, 2));
      return 0;
    }
    if (!parse(env, ts->tail->tail, &ty, &tail, error)) return 0;
    /* AfterType{remaining, name, acc, F.Pair2{ty, tail}}. */
    field.name = name;
    field.ty = ty;
    acc = SurfaceRecordFields_cons(field, acc);
    /* AfterTail{remaining, acc, tail}. */
    if (head_is(tail, TOKEN_SEMICOLON)) {
      fuel = remaining;
      ts = tail->tail;
      continue;
    }
    if (head_is(tail, TOKEN_RBRACE)) {
      *out = SurfaceRecordFields_reverse(acc);
      *rest = tail->tail;
      return 1;
    }
    return want("';' or '}' after a record field", tail, error);
  }
}

/* declaration_sort and declared (lines 81 and 85). */
static int declaration_sort(const void *env, SurfaceParseTerm parse,
                            Text name, const SurfaceTerm *sort,
                            const SurfaceTokens *tail,
                            const SurfaceDecls **out,
                            const SurfaceTokens **rest,
                            const KError **error) {
  const SurfaceRecordFields *fs;
  const SurfaceTokens *suffix;
  if (!head_is(tail, TOKEN_LBRACE))
    return want("'{' after a record's type annotation", tail, error);
  if (!fields(env, parse, surface_record_length(tail->tail), tail->tail,
              NULL, &fs, &suffix, error))
    return 0;
  *out = expand(name, sort, fs);
  *rest = suffix;
  return 1;
}

/* declaration (line 93). */
static int declaration(const void *env, SurfaceParseTerm parse,
                       const SurfaceTokens *ts, const SurfaceDecls **out,
                       const SurfaceTokens **rest, const KError **error) {
  const SurfaceTerm *sort;
  const SurfaceTokens *tail;
  if (!(head_is(ts, TOKEN_IDENT) && head_is(ts->tail, TOKEN_COLON)))
    return want("'name : Type' after 'record'", ts, error);
  if (!parse(env, ts->tail->tail, &sort, &tail, error)) return 0;
  return declaration_sort(env, parse, ts->head.kind.as.ident, sort, tail,
                          out, rest, error);
}

/* declarations (@unsafe, line 105): a loop. prepend (line 99) is
   SurfaceDecls_reverse_onto. */
int surface_record_declarations(const void *env, SurfaceParseTerm parse,
                                SurfaceParseDecl parse_decl, Nat fuel,
                                const SurfaceTokens *ts,
                                const SurfaceDecls *acc,
                                const SurfaceDecls **out,
                                const SurfaceTokens **rest,
                                const KError **error) {
  for (;;) {
    const SurfaceTokens *tail;
    if (fuel == 0) return want("end of input", ts, error);
    if (head_is(ts, TOKEN_EOF)) {
      *out = SurfaceDecls_reverse(acc);
      *rest = ts->tail;
      return 1;
    }
    if (head_is(ts, TOKEN_KRECORD)) {
      const SurfaceDecls *ds;
      if (!declaration(env, parse, ts->tail, &ds, &tail, error)) return 0;
      acc = SurfaceDecls_reverse_onto(ds, acc);
    } else {
      const SurfaceDecl *d;
      if (!parse_decl(env, ts, &d, &tail, error)) return 0;
      acc = SurfaceDecls_cons(d, acc);
    }
    fuel = fuel - 1;
    ts = tail;
  }
}

/* qualify (line 115). */
int surface_record_qualify(const SurfaceTerm *term, Text field,
                           SurfaceLoc loc, const SurfaceTerm **out,
                           const KError **error) {
  if (term->tag == SURFACE_SVAR) {
    Text parts[3];
    parts[0] = term->as.var.name;
    parts[1] = text_of_cstring(".");
    parts[2] = field;
    *out = surface_syntax_svar(cat(parts, 3));
    return 1;
  }
  *error = surface_lexer_error(
      loc, text_of_cstring("E-RECORD-PROJECTION: use a qualified getter "
                           "such as Pair.first value"));
  return 0;
}
