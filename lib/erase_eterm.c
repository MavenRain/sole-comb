/* lib/erase_eterm.c: twin of erase/eterm.bend (S4 session 1).
   The erased representation and its printer (Kanon 69f3be51 lib/eterm.ml).
   The Bend printer `render` keeps a work list of pieces and an accumulator
   of texts. The C printer keeps the same work list and writes each text to
   one TextBuilder in the same order (String.concat of the reversed
   accumulator = the builder text). */
#include "sole.h"

/* ---------------------------------------------------------------- make */

const Repr *eterm_repr(ReprTag tag, Tid name) {
  Repr *r = (Repr *)arena_alloc(sizeof *r);
  r->tag = tag;
  r->name = name;
  return r;
}

const KBranch *eterm_branch(Int63 tag, Int63 arity, const KTm *body) {
  KBranch *b = (KBranch *)arena_alloc(sizeof *b);
  b->tag = tag;
  b->arity = arity;
  b->body = body;
  return b;
}

static KTm *ktm_new(KTmTag tag) {
  KTm *t = (KTm *)arena_alloc(sizeof *t);
  t->tag = tag;
  return t;
}

const KTm *eterm_kvar(Int63 index) {
  KTm *t = ktm_new(KTM_KVAR);
  t->as.kvar.index = index;
  return t;
}

const KTm *eterm_klit(const Literal *literal) {
  KTm *t = ktm_new(KTM_KLIT);
  t->as.klit.literal = literal;
  return t;
}

const KTm *eterm_kglobal(Text name) {
  KTm *t = ktm_new(KTM_KGLOBAL);
  t->as.kglobal.name = name;
  return t;
}

const KTm *eterm_kerased(void) { return ktm_new(KTM_KERASED); }

const KTm *eterm_klet(Text name, const KTm *value, const KTm *body) {
  KTm *t = ktm_new(KTM_KLET);
  t->as.klet.name = name;
  t->as.klet.value = value;
  t->as.klet.body = body;
  return t;
}

const KTm *eterm_kclos(Fid name, Int63 arity, const KTms *captures) {
  KTm *t = ktm_new(KTM_KCLOS);
  t->as.kclos.name = name;
  t->as.kclos.arity = arity;
  t->as.kclos.captures = captures;
  return t;
}

const KTm *eterm_kapp(const KTm *head, const KTms *args) {
  KTm *t = ktm_new(KTM_KAPP);
  t->as.kapp.head = head;
  t->as.kapp.args = args;
  return t;
}

const KTm *eterm_ktail(const KTm *head, const KTms *args) {
  KTm *t = ktm_new(KTM_KTAIL);
  t->as.ktail.head = head;
  t->as.ktail.args = args;
  return t;
}

const KTm *eterm_kstruct(Tid name, const KTms *fields) {
  KTm *t = ktm_new(KTM_KSTRUCT);
  t->as.kstruct.name = name;
  t->as.kstruct.fields = fields;
  return t;
}

const KTm *eterm_kproj(Tid name, Int63 field, const KTm *value) {
  KTm *t = ktm_new(KTM_KPROJ);
  t->as.kproj.name = name;
  t->as.kproj.field = field;
  t->as.kproj.value = value;
  return t;
}

const KTm *eterm_ktag(Tid name, Int63 tag, const KTms *fields) {
  KTm *t = ktm_new(KTM_KTAG);
  t->as.ktag.name = name;
  t->as.ktag.tag = tag;
  t->as.ktag.fields = fields;
  return t;
}

const KTm *eterm_kcase(Tid name, const KTm *scrutinee,
                       const KBranches *branches) {
  KTm *t = ktm_new(KTM_KCASE);
  t->as.kcase.name = name;
  t->as.kcase.scrutinee = scrutinee;
  t->as.kcase.branches = branches;
  return t;
}

const KTm *eterm_kdelay(Fid name, const KTms *captures) {
  KTm *t = ktm_new(KTM_KDELAY);
  t->as.kdelay.name = name;
  t->as.kdelay.captures = captures;
  return t;
}

const KTm *eterm_kforce(const KTm *value) {
  KTm *t = ktm_new(KTM_KFORCE);
  t->as.kforce.value = value;
  return t;
}

const KDecl *eterm_kfun(Fid name, const Reprs *params, const Repr *result,
                        const KTm *body) {
  KDecl *d = (KDecl *)arena_alloc(sizeof *d);
  d->tag = KDECL_KFUN;
  d->as.kfun.name = name;
  d->as.kfun.params = params;
  d->as.kfun.result = result;
  d->as.kfun.body = body;
  return d;
}

const KDecl *eterm_krec(const Tids *names) {
  KDecl *d = (KDecl *)arena_alloc(sizeof *d);
  d->tag = KDECL_KREC;
  d->as.krec.names = names;
  return d;
}

/* ---------------------------------------------------------------- text */

Text eterm_tid_text(Tid t) { return t.name; }

Text eterm_fid_text(Fid f) { return f.name; }

/* decimal_byte: `\ddd`. Each divisor is a literal (100, 10). */
static void decimal_byte(TextBuilder *b, U32 c) {
  builder_push(b, 92);
  builder_push(b, 48 + c / 100);
  builder_push(b, 48 + (c / 10) % 10);
  builder_push(b, 48 + c % 10);
}

static void escape_ascii(TextBuilder *b, U32 c) {
  switch (c) {
    case 34: builder_append_cstring(b, "\\\""); return;
    case 92: builder_append_cstring(b, "\\\\"); return;
    case 10: builder_append_cstring(b, "\\n"); return;
    case 13: builder_append_cstring(b, "\\r"); return;
    case 9: builder_append_cstring(b, "\\t"); return;
    case 8: builder_append_cstring(b, "\\b"); return;
  }
  if (c >= 32 && c < 127) {
    builder_push(b, c);
    return;
  }
  decimal_byte(b, c);
}

/* escape_code: the UTF-8 bytes of c, each as `\ddd`. Each divisor is a
   literal (64, 4096, 262144). */
static void escape_code(TextBuilder *b, U32 c) {
  if (c < 128) {
    escape_ascii(b, c);
    return;
  }
  if (c < 2048) {
    decimal_byte(b, 192 + c / 64);
    decimal_byte(b, 128 + c % 64);
    return;
  }
  if (c < 65536) {
    decimal_byte(b, 224 + c / 4096);
    decimal_byte(b, 128 + (c / 64) % 64);
    decimal_byte(b, 128 + c % 64);
    return;
  }
  decimal_byte(b, 240 + c / 262144);
  decimal_byte(b, 128 + (c / 4096) % 64);
  decimal_byte(b, 128 + (c / 64) % 64);
  decimal_byte(b, 128 + c % 64);
}

/* escape (a self recursion over the string) is a loop. */
static void escape_into(TextBuilder *b, Text s) {
  U32 i;
  for (i = 0; i < s.size; i++) escape_code(b, s.items[i]);
}

Text eterm_escape(Text s) {
  TextBuilder b = builder_new();
  escape_into(&b, s);
  return builder_text(&b);
}

Text eterm_literal_text(const Literal *lit) {
  TextBuilder b = builder_new();
  switch (lit->tag) {
    case LITERAL_LINT: return bignum_to_string(lit->as.lint.value);
    case LITERAL_LSTRING:
      builder_push(&b, 34);
      escape_into(&b, lit->as.lstring.value);
      builder_push(&b, 34);
      return builder_text(&b);
  }
  return builder_text(&b); /* unreachable: the switch is exhaustive */
}

/* join and join_more: the first text, then sep and each next text. */
Text eterm_join(Text sep, const Texts *xs) {
  TextBuilder b = builder_new();
  if (xs == NULL) return builder_text(&b);
  builder_append(&b, xs->head);
  for (xs = xs->tail; xs != NULL; xs = xs->tail) {
    builder_append(&b, sep);
    builder_append(&b, xs->head);
  }
  return builder_text(&b);
}

static Text prefixed(const char *head, Text name) {
  return text_append(text_of_cstring(head), name);
}

Text eterm_print_repr(const Repr *r) {
  switch (r->tag) {
    case REPR_RI31: return text_of_cstring("i31");
    case REPR_RSTRUCT: return prefixed("struct ", eterm_tid_text(r->name));
    case REPR_RUNION: return prefixed("union ", eterm_tid_text(r->name));
    case REPR_RFUNC: return prefixed("func ", eterm_tid_text(r->name));
    case REPR_RTHUNK: return prefixed("thunk ", eterm_tid_text(r->name));
  }
  return text_end(); /* unreachable: the switch is exhaustive */
}

const Texts *eterm_repr_texts(const Reprs *rs) {
  const Texts *acc = NULL;
  for (; rs != NULL; rs = rs->tail)
    acc = Texts_cons(eterm_print_repr(rs->head), acc);
  return Texts_reverse(acc);
}

static const Texts *tid_texts(const Tids *ts) {
  const Texts *acc = NULL;
  for (; ts != NULL; ts = ts->tail) acc = Texts_cons(eterm_tid_text(ts->head), acc);
  return Texts_reverse(acc);
}

/* ---------------------------------------------------------------- render */

typedef enum {
  PIECE_TEXT,
  PIECE_TERM,
  PIECE_TERMS,
  PIECE_MORE_TERMS,
  PIECE_BRANCH,
  PIECE_BRANCHES,
  PIECE_MORE_BRANCHES
} PieceTag;

typedef struct {
  PieceTag tag;
  Text text;                  /* PIECE_TEXT */
  const KTm *term;            /* PIECE_TERM */
  const KTms *terms;          /* PIECE_TERMS, PIECE_MORE_TERMS */
  const KBranch *branch;      /* PIECE_BRANCH */
  const KBranches *branches;  /* PIECE_BRANCHES, PIECE_MORE_BRANCHES */
} Piece;

SOLE_LIST(Pieces, Piece)

static Piece piece(PieceTag tag) {
  Piece p;
  p.tag = tag;
  p.text = text_end();
  p.term = NULL;
  p.terms = NULL;
  p.branch = NULL;
  p.branches = NULL;
  return p;
}

static const Pieces *text_then(const char *ascii, const Pieces *rest) {
  Piece p = piece(PIECE_TEXT);
  p.text = text_of_cstring(ascii);
  return Pieces_cons(p, rest);
}

static const Pieces *term_then(const KTm *t, const Pieces *rest) {
  Piece p = piece(PIECE_TERM);
  p.term = t;
  return Pieces_cons(p, rest);
}

static const Pieces *terms_then(PieceTag tag, const KTms *ts,
                                const Pieces *rest) {
  Piece p = piece(tag);
  p.terms = ts;
  return Pieces_cons(p, rest);
}

static const Pieces *branch_then(const KBranch *b, const Pieces *rest) {
  Piece p = piece(PIECE_BRANCH);
  p.branch = b;
  return Pieces_cons(p, rest);
}

static const Pieces *branches_then(PieceTag tag, const KBranches *bs,
                                   const Pieces *rest) {
  Piece p = piece(tag);
  p.branches = bs;
  return Pieces_cons(p, rest);
}

/* expand_term + expanded: writes the Expanded text and returns
   children ++ rest (Bend `append(children, rest)`). */
static const Pieces *expand_term(TextBuilder *b, const KTm *t,
                                 const Pieces *rest) {
  switch (t->tag) {
    case KTM_KVAR:
      builder_append_cstring(b, "KVar ");
      builder_append(b, int63_show(t->as.kvar.index));
      return rest;
    case KTM_KLIT:
      builder_append_cstring(b, "KLit ");
      builder_append(b, eterm_literal_text(t->as.klit.literal));
      return rest;
    case KTM_KGLOBAL:
      builder_append_cstring(b, "KGlobal ");
      builder_append(b, t->as.kglobal.name);
      return rest;
    case KTM_KERASED:
      builder_append_cstring(b, "KErased");
      return rest;
    case KTM_KLET:
      builder_append_cstring(b, "KLet ");
      builder_append(b, t->as.klet.name);
      builder_append_cstring(b, " (");
      return term_then(t->as.klet.value,
                       text_then(") (", term_then(t->as.klet.body,
                                                  text_then(")", rest))));
    case KTM_KCLOS:
      builder_append_cstring(b, "KClos ");
      builder_append(b, eterm_fid_text(t->as.kclos.name));
      builder_append_cstring(b, " ");
      builder_append(b, int63_show(t->as.kclos.arity));
      builder_append_cstring(b, " [");
      return terms_then(PIECE_TERMS, t->as.kclos.captures, text_then("]", rest));
    case KTM_KAPP:
      builder_append_cstring(b, "KApp (");
      return term_then(t->as.kapp.head,
                       text_then(") [", terms_then(PIECE_TERMS, t->as.kapp.args,
                                                   text_then("]", rest))));
    case KTM_KTAIL:
      builder_append_cstring(b, "KTail (");
      return term_then(t->as.ktail.head,
                       text_then(") [", terms_then(PIECE_TERMS, t->as.ktail.args,
                                                   text_then("]", rest))));
    case KTM_KSTRUCT:
      builder_append_cstring(b, "KStruct ");
      builder_append(b, eterm_tid_text(t->as.kstruct.name));
      builder_append_cstring(b, " [");
      return terms_then(PIECE_TERMS, t->as.kstruct.fields, text_then("]", rest));
    case KTM_KPROJ:
      builder_append_cstring(b, "KProj ");
      builder_append(b, eterm_tid_text(t->as.kproj.name));
      builder_append_cstring(b, " ");
      builder_append(b, int63_show(t->as.kproj.field));
      builder_append_cstring(b, " (");
      return term_then(t->as.kproj.value, text_then(")", rest));
    case KTM_KTAG:
      builder_append_cstring(b, "KTag ");
      builder_append(b, eterm_tid_text(t->as.ktag.name));
      builder_append_cstring(b, " ");
      builder_append(b, int63_show(t->as.ktag.tag));
      builder_append_cstring(b, " [");
      return terms_then(PIECE_TERMS, t->as.ktag.fields, text_then("]", rest));
    case KTM_KCASE:
      builder_append_cstring(b, "KCase ");
      builder_append(b, eterm_tid_text(t->as.kcase.name));
      builder_append_cstring(b, " (");
      return term_then(t->as.kcase.scrutinee,
                       text_then(") [", branches_then(PIECE_BRANCHES,
                                                      t->as.kcase.branches,
                                                      text_then("]", rest))));
    case KTM_KDELAY:
      builder_append_cstring(b, "KDelay ");
      builder_append(b, eterm_fid_text(t->as.kdelay.name));
      builder_append_cstring(b, " [");
      return terms_then(PIECE_TERMS, t->as.kdelay.captures, text_then("]", rest));
    case KTM_KFORCE:
      builder_append_cstring(b, "KForce (");
      return term_then(t->as.kforce.value, text_then(")", rest));
  }
  return rest; /* unreachable: the switch is exhaustive */
}

/* expand + expanded (one work-list step). */
static const Pieces *expand(TextBuilder *b, Piece item, const Pieces *rest) {
  switch (item.tag) {
    case PIECE_TEXT:
      builder_append(b, item.text);
      return rest;
    case PIECE_TERM: return expand_term(b, item.term, rest);
    case PIECE_TERMS:
      if (item.terms == NULL) return rest;
      return term_then(item.terms->head,
                       terms_then(PIECE_MORE_TERMS, item.terms->tail, rest));
    case PIECE_MORE_TERMS:
      if (item.terms == NULL) return rest;
      builder_append_cstring(b, "; ");
      return term_then(item.terms->head,
                       terms_then(PIECE_MORE_TERMS, item.terms->tail, rest));
    case PIECE_BRANCH:
      builder_append_cstring(b, "{");
      builder_append(b, int63_show(item.branch->tag));
      builder_append_cstring(b, " ");
      builder_append(b, int63_show(item.branch->arity));
      builder_append_cstring(b, " (");
      return term_then(item.branch->body, text_then(")}", rest));
    case PIECE_BRANCHES:
      if (item.branches == NULL) return rest;
      return branch_then(item.branches->head,
                         branches_then(PIECE_MORE_BRANCHES,
                                       item.branches->tail, rest));
    case PIECE_MORE_BRANCHES:
      if (item.branches == NULL) return rest;
      builder_append_cstring(b, "; ");
      return branch_then(item.branches->head,
                         branches_then(PIECE_MORE_BRANCHES,
                                       item.branches->tail, rest));
  }
  return rest; /* unreachable: the switch is exhaustive */
}

/* render (@unsafe in Bend; a self tail call) is a loop. */
static void render(TextBuilder *b, const Pieces *todo) {
  while (todo != NULL) todo = expand(b, todo->head, todo->tail);
}

Text eterm_print_ktm(const KTm *t) {
  TextBuilder b = builder_new();
  render(&b, term_then(t, NULL));
  return builder_text(&b);
}

Text eterm_ktm_list(const KTms *ts) {
  TextBuilder b = builder_new();
  builder_append_cstring(&b, "[");
  render(&b, terms_then(PIECE_TERMS, ts, NULL));
  builder_append_cstring(&b, "]");
  return builder_text(&b);
}

Text eterm_print_branch(const KBranch *br) {
  TextBuilder b = builder_new();
  render(&b, branch_then(br, NULL));
  return builder_text(&b);
}

Text eterm_print_decl(const KDecl *d) {
  TextBuilder b = builder_new();
  switch (d->tag) {
    case KDECL_KFUN:
      builder_append_cstring(&b, "fun ");
      builder_append(&b, eterm_fid_text(d->as.kfun.name));
      builder_append_cstring(&b, " (");
      builder_append(&b, eterm_join(text_of_cstring(", "),
                                    eterm_repr_texts(d->as.kfun.params)));
      builder_append_cstring(&b, ") : ");
      builder_append(&b, eterm_print_repr(d->as.kfun.result));
      builder_append_cstring(&b, " := ");
      builder_append(&b, eterm_print_ktm(d->as.kfun.body));
      return builder_text(&b);
    case KDECL_KREC:
      builder_append_cstring(&b, "rec [");
      builder_append(&b, eterm_join(text_of_cstring("; "),
                                    tid_texts(d->as.krec.names)));
      builder_append_cstring(&b, "]");
      return builder_text(&b);
  }
  return builder_text(&b); /* unreachable: the switch is exhaustive */
}
