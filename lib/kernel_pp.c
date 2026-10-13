/* kernel_pp.c: C twin of lib/kernel_pp.bend (stage S1b). Display only.

   render (@unsafe in Bend) is a worklist loop over a cons list of pieces.
   Each step appends the text of one expansion to a TextBuilder and puts
   its children before the rest of the list, as render(append(children,
   rest), text <> acc) does. The strings are those of expand_term,
   expand_shape, expand_leg and expand, byte for byte. */
#include "sole.h"

/* cat over a fixed list of texts. */
static Text pp_cat(const Text *parts, U32 count) {
  TextBuilder out = builder_new();
  U32 at;
  for (at = 0; at < count; at++) builder_append(&out, parts[at]);
  return builder_text(&out);
}
#define PP_CAT(...)                                                          \
  pp_cat((const Text[]){__VA_ARGS__},                                        \
         (U32)(sizeof((const Text[]){__VA_ARGS__}) / sizeof(Text)))
#define PP_LIT(ascii) text_of_cstring(ascii)

/* ------------------------------------------------------------- escape */

/* decimal_byte: \ and three decimal digits. U32 arithmetic, as Bend. */
static void pp_decimal_byte(TextBuilder *out, U32 c) {
  builder_push(out, 92);
  builder_push(out, 48 + c / 100);
  builder_push(out, 48 + (c / 10) % 10);
  builder_push(out, 48 + c % 10);
}

static void pp_escape_ascii(TextBuilder *out, U32 c) {
  switch (c) {
  case 34: builder_append_cstring(out, "\\\""); return;
  case 92: builder_append_cstring(out, "\\\\"); return;
  case 10: builder_append_cstring(out, "\\n"); return;
  case 13: builder_append_cstring(out, "\\r"); return;
  case 9: builder_append_cstring(out, "\\t"); return;
  case 8: builder_append_cstring(out, "\\b"); return;
  default:
    if (c >= 32 && c < 127) builder_push(out, c);
    else pp_decimal_byte(out, c);
    return;
  }
}

/* escape_code: the UTF-8 bytes of c as decimal escapes. */
static void pp_escape_code(TextBuilder *out, U32 c) {
  if (c < 128) {
    pp_escape_ascii(out, c);
    return;
  }
  if (c < 2048) {
    pp_decimal_byte(out, 192 + c / 64);
    pp_decimal_byte(out, 128 + c % 64);
    return;
  }
  if (c < 65536) {
    pp_decimal_byte(out, 224 + c / 4096);
    pp_decimal_byte(out, 128 + (c / 64) % 64);
    pp_decimal_byte(out, 128 + c % 64);
    return;
  }
  pp_decimal_byte(out, 240 + c / 262144);
  pp_decimal_byte(out, 128 + (c / 4096) % 64);
  pp_decimal_byte(out, 128 + (c / 64) % 64);
  pp_decimal_byte(out, 128 + c % 64);
}

/* escape: a loop over the text. */
Text pp_escape(Text s) {
  TextBuilder out = builder_new();
  U32 at;
  for (at = 0; at < s.size; at++) pp_escape_code(&out, s.items[at]);
  return builder_text(&out);
}

Text pp_literal(const Literal *lit) {
  switch (lit->tag) {
  case LITERAL_LINT: return bignum_to_string(lit->as.lint.value);
  case LITERAL_LSTRING:
    return PP_CAT(PP_LIT("\""), pp_escape(lit->as.lstring.value), PP_LIT("\""));
  }
  return text_end();
}

/* ------------------------------------------------------------- names */

/* push (and push_loop): each added name goes on top, in order. */
static const Texts *pp_push(const Texts *names, const Texts *added) {
  return Texts_reverse_onto(added, names);
}

static const Texts *pp_binder_names(const TermBinders *bs) {
  const Texts *names = NULL;
  for (; bs != NULL; bs = bs->tail) names = Texts_cons(bs->head.name, names);
  return Texts_reverse(names);
}

static const Texts *pp_under(const TermShape *s, const Texts *names) {
  switch (s->tag) {
  case SHAPE_SPI: return Texts_cons(s->as.spi.binder, names);
  case SHAPE_SCOLL: return names;
  case SHAPE_SPAR: return names;
  case SHAPE_SMU: return names;
  case SHAPE_SNU: return names;
  }
  return names;
}

/* nth_name (and nth_loop): a loop; ix steps down with Int63 pred. */
static MaybeText pp_nth_name(Int63 ix, const Texts *names) {
  for (; names != NULL; names = names->tail) {
    if (int63_equal(ix, int63_zero())) return maybe_text_some(names->head);
    ix = int63_pred(ix);
  }
  return maybe_text_none();
}

static Text pp_lookup_name(Int63 original, MaybeText name) {
  if (name.some == NO) return PP_CAT(PP_LIT("#"), int63_show(original));
  return name.text;
}

/* ------------------------------------------------------------- pieces */

typedef enum {
  PIECE_TEXT, PIECE_TERM, PIECE_SHAPE, PIECE_ADDR, PIECE_LEG, PIECE_ELIM,
  PIECE_MOTIVE, PIECE_TERMS, PIECE_LEGS, PIECE_BRANCHES
} PieceTag;
typedef struct Piece Piece;
struct Piece {
  PieceTag tag;
  union {
    struct { Text text; } text;
    struct { const Texts *names; const Term *tm; } term;
    struct { const Texts *names; const TermShape *shape; } shape;
    struct { const Texts *names; const TermAddr *addr; } addr;
    struct { const Texts *names; const TermLeg *leg; } leg;
    struct { const Texts *names; const TermElim *elim; } elim;
    struct { const Texts *names; const TermMotive *motive; } motive;
    struct { Bool first; const Texts *names; const Terms *terms; } terms;
    struct { Bool first; const Texts *names; const TermLegs *legs; } legs;
    struct { const Texts *names; const TermBranches *branches; } branches;
  } as;
};
SOLE_LIST(Pieces, const Piece *)

static Piece *piece_cell(PieceTag tag) {
  Piece *piece = arena_alloc(sizeof *piece);
  piece->tag = tag;
  return piece;
}

static const Piece *piece_text(Text text) {
  Piece *piece = piece_cell(PIECE_TEXT);
  piece->as.text.text = text;
  return piece;
}

static const Piece *piece_term(const Texts *names, const Term *tm) {
  Piece *piece = piece_cell(PIECE_TERM);
  piece->as.term.names = names;
  piece->as.term.tm = tm;
  return piece;
}

static const Piece *piece_shape(const Texts *names, const TermShape *shape) {
  Piece *piece = piece_cell(PIECE_SHAPE);
  piece->as.shape.names = names;
  piece->as.shape.shape = shape;
  return piece;
}

static const Piece *piece_addr(const Texts *names, const TermAddr *addr) {
  Piece *piece = piece_cell(PIECE_ADDR);
  piece->as.addr.names = names;
  piece->as.addr.addr = addr;
  return piece;
}

static const Piece *piece_leg(const Texts *names, const TermLeg *leg) {
  Piece *piece = piece_cell(PIECE_LEG);
  piece->as.leg.names = names;
  piece->as.leg.leg = leg;
  return piece;
}

static const Piece *piece_elim(const Texts *names, const TermElim *elim) {
  Piece *piece = piece_cell(PIECE_ELIM);
  piece->as.elim.names = names;
  piece->as.elim.elim = elim;
  return piece;
}

static const Piece *piece_motive(const Texts *names,
                                 const TermMotive *motive) {
  Piece *piece = piece_cell(PIECE_MOTIVE);
  piece->as.motive.names = names;
  piece->as.motive.motive = motive;
  return piece;
}

static const Piece *piece_terms(Bool first, const Texts *names,
                                const Terms *terms) {
  Piece *piece = piece_cell(PIECE_TERMS);
  piece->as.terms.first = first;
  piece->as.terms.names = names;
  piece->as.terms.terms = terms;
  return piece;
}

static const Piece *piece_legs(Bool first, const Texts *names,
                               const TermLegs *legs) {
  Piece *piece = piece_cell(PIECE_LEGS);
  piece->as.legs.first = first;
  piece->as.legs.names = names;
  piece->as.legs.legs = legs;
  return piece;
}

static const Piece *piece_branches(const Texts *names,
                                   const TermBranches *branches) {
  Piece *piece = piece_cell(PIECE_BRANCHES);
  piece->as.branches.names = names;
  piece->as.branches.branches = branches;
  return piece;
}

/* pieces(children) followed by rest: append(children, rest). */
static const Pieces *pp_pieces(const Pieces *rest, const Piece *const *items,
                               U32 count) {
  for (; count > 0; count--) rest = Pieces_cons(items[count - 1], rest);
  return rest;
}
#define PP_PIECES(rest, ...)                                                 \
  pp_pieces((rest), (const Piece *const[]){__VA_ARGS__},                     \
            (U32)(sizeof((const Piece *const[]){__VA_ARGS__}) /              \
                  sizeof(const Piece *)))

/* Expanded{text, []}: append the text; no children. */
static const Pieces *pp_expanded(TextBuilder *out, Text text,
                                 const Pieces *rest) {
  builder_append(out, text);
  return rest;
}

/* ------------------------------------------------------------- expand */

static const Pieces *pp_expand_term(TextBuilder *out, const Texts *ns,
                                    const Term *tm, const Pieces *rest) {
  switch (tm->tag) {
  case TERM_VAR:
    return pp_expanded(out, pp_lookup_name(tm->as.var.index,
                                           pp_nth_name(tm->as.var.index, ns)),
                       rest);
  case TERM_UNIV:
    return pp_expanded(out,
                       PP_CAT(PP_LIT("Type "), level_to_string(tm->as.univ.level)),
                       rest);
  case TERM_LAN:
    return PP_PIECES(rest, piece_text(PP_LIT("(Lan ")),
                     piece_shape(ns, tm->as.lan.shape), piece_text(PP_LIT(" ")),
                     piece_term(pp_under(tm->as.lan.shape, ns), tm->as.lan.diagram),
                     piece_text(PP_LIT(")")));
  case TERM_RAN:
    return PP_PIECES(rest, piece_text(PP_LIT("(Ran ")),
                     piece_shape(ns, tm->as.ran.shape), piece_text(PP_LIT(" ")),
                     piece_term(pp_under(tm->as.ran.shape, ns), tm->as.ran.diagram),
                     piece_text(PP_LIT(")")));
  case TERM_IN:
    return PP_PIECES(rest, piece_text(PP_LIT("(In ")),
                     piece_shape(ns, tm->as.in.shape), piece_text(PP_LIT(" ")),
                     piece_addr(ns, tm->as.in.address), piece_text(PP_LIT(" [")),
                     piece_terms(YES, ns, tm->as.in.args),
                     piece_text(PP_LIT("])")));
  case TERM_ELIM:
    return PP_PIECES(rest, piece_elim(ns, tm->as.elim.elim));
  case TERM_SEC:
    return PP_PIECES(rest, piece_text(PP_LIT("(Sec ")),
                     piece_shape(ns, tm->as.sec.shape), piece_text(PP_LIT(" [")),
                     piece_legs(YES, ns, tm->as.sec.legs),
                     piece_text(PP_LIT("])")));
  case TERM_OUT:
    return PP_PIECES(rest, piece_text(PP_LIT("(Out ")),
                     piece_shape(ns, tm->as.out.shape), piece_text(PP_LIT(" ")),
                     piece_addr(ns, tm->as.out.address), piece_text(PP_LIT(" ")),
                     piece_term(ns, tm->as.out.head), piece_text(PP_LIT(")")));
  case TERM_LET:
    return PP_PIECES(rest,
                     piece_text(PP_CAT(PP_LIT("let "), tm->as.let.name, PP_LIT(" : "))),
                     piece_term(ns, tm->as.let.ty), piece_text(PP_LIT(" := ")),
                     piece_term(ns, tm->as.let.value), piece_text(PP_LIT(" in ")),
                     piece_term(Texts_cons(tm->as.let.name, ns), tm->as.let.body));
  case TERM_ANN:
    return PP_PIECES(rest, piece_text(PP_LIT("(")),
                     piece_term(ns, tm->as.ann.term), piece_text(PP_LIT(" : ")),
                     piece_term(ns, tm->as.ann.ty), piece_text(PP_LIT(")")));
  case TERM_GLOBAL: return pp_expanded(out, tm->as.global.name, rest);
  case TERM_LIT: return pp_expanded(out, pp_literal(tm->as.lit.literal), rest);
  case TERM_AUTO: return pp_expanded(out, PP_LIT("auto"), rest);
  }
  return rest;
}

static const Pieces *pp_expand_shape(TextBuilder *out, const Texts *ns,
                                     const TermShape *s, const Pieces *rest) {
  switch (s->tag) {
  case SHAPE_SPI:
    return PP_PIECES(rest,
                     piece_text(PP_CAT(PP_LIT("SPi "),
                                       quantity_to_string(s->as.spi.quantity),
                                       PP_LIT(" "), s->as.spi.binder, PP_LIT(" "))),
                     piece_term(ns, s->as.spi.domain));
  case SHAPE_SCOLL:
    return pp_expanded(out, PP_CAT(PP_LIT("SColl "), int63_show(s->as.scoll.size)),
                       rest);
  case SHAPE_SPAR:
    return PP_PIECES(rest, piece_text(PP_LIT("SPar ")),
                     piece_term(ns, s->as.spar.left), piece_text(PP_LIT(" ")),
                     piece_term(ns, s->as.spar.right));
  case SHAPE_SMU:
    return PP_PIECES(rest,
                     piece_text(PP_CAT(PP_LIT("SMu "), s->as.smu.family, PP_LIT(" ["))),
                     piece_terms(YES, ns, s->as.smu.indices),
                     piece_text(PP_LIT("]")));
  case SHAPE_SNU:
    return PP_PIECES(rest,
                     piece_text(PP_CAT(PP_LIT("SNu "), s->as.snu.family, PP_LIT(" ["))),
                     piece_terms(YES, ns, s->as.snu.indices),
                     piece_text(PP_LIT("]")));
  }
  return rest;
}

static const Pieces *pp_expand_leg(const Texts *ns, const Texts *bs,
                                   const Term *body, const Pieces *rest) {
  return PP_PIECES(rest,
                   piece_text(PP_CAT(text_join(bs, PP_LIT(" ")), PP_LIT(" => "))),
                   piece_term(pp_push(ns, bs), body));
}

static const Pieces *pp_expand_addr(TextBuilder *out, const Texts *ns,
                                    const TermAddr *a, const Pieces *rest) {
  switch (a->tag) {
  case TERM_APT:
    return PP_PIECES(rest,
                     piece_text(PP_CAT(PP_LIT("(APt "),
                                       quantity_to_string(a->as.apt.quantity),
                                       PP_LIT(" "))),
                     piece_term(ns, a->as.apt.arg), piece_text(PP_LIT(")")));
  case TERM_ALEG:
    return pp_expanded(out,
                       PP_CAT(PP_LIT("(ALeg "), int63_show(a->as.aleg.index), PP_LIT(")")),
                       rest);
  case TERM_ACTOR:
    return pp_expanded(out, PP_CAT(PP_LIT("(ACtor "), a->as.actor.name, PP_LIT(")")),
                       rest);
  }
  return rest;
}

static const Pieces *pp_expand(TextBuilder *out, const Piece *item,
                               const Pieces *rest) {
  switch (item->tag) {
  case PIECE_TEXT: return pp_expanded(out, item->as.text.text, rest);
  case PIECE_TERM:
    return pp_expand_term(out, item->as.term.names, item->as.term.tm, rest);
  case PIECE_SHAPE:
    return pp_expand_shape(out, item->as.shape.names, item->as.shape.shape,
                           rest);
  case PIECE_ADDR:
    return pp_expand_addr(out, item->as.addr.names, item->as.addr.addr, rest);
  case PIECE_LEG:
    return pp_expand_leg(item->as.leg.names,
                         pp_binder_names(item->as.leg.leg->binders),
                         item->as.leg.leg->body, rest);
  case PIECE_ELIM: {
    const Texts *ns = item->as.elim.names;
    const TermElim *e = item->as.elim.elim;
    return PP_PIECES(rest, piece_text(PP_LIT("(Elim ")), piece_shape(ns, e->shape),
                     piece_text(PP_LIT(" ")), piece_term(ns, e->scrut),
                     piece_motive(ns, e->motive), piece_text(PP_LIT(" with")),
                     piece_branches(ns, e->branches), piece_text(PP_LIT(")")));
  }
  case PIECE_MOTIVE: {
    const Texts *ns = item->as.motive.names;
    const TermMotive *mo = item->as.motive.motive;
    if (mo == NULL) return rest;
    return PP_PIECES(rest,
                     piece_text(PP_CAT(PP_LIT(" as "), mo->self, PP_LIT(" return "))),
                     piece_term(Texts_cons(mo->self, pp_push(ns, mo->idx)), mo->body));
  }
  case PIECE_TERMS: {
    const Texts *ns = item->as.terms.names;
    const Terms *ts = item->as.terms.terms;
    if (ts == NULL) return rest;
    return PP_PIECES(rest,
                     piece_text(item->as.terms.first ? PP_LIT("") : PP_LIT("; ")),
                     piece_term(ns, ts->head), piece_terms(NO, ns, ts->tail));
  }
  case PIECE_LEGS: {
    const Texts *ns = item->as.legs.names;
    const TermLegs *ls = item->as.legs.legs;
    if (ls == NULL) return rest;
    return PP_PIECES(rest,
                     piece_text(item->as.legs.first ? PP_LIT("") : PP_LIT("; ")),
                     piece_leg(ns, ls->head), piece_legs(NO, ns, ls->tail));
  }
  case PIECE_BRANCHES: {
    const Texts *ns = item->as.branches.names;
    const TermBranches *bs = item->as.branches.branches;
    if (bs == NULL) return rest;
    return PP_PIECES(rest, piece_text(PP_LIT(" | ")),
                     piece_addr(ns, bs->head.address), piece_text(PP_LIT(" ")),
                     piece_leg(ns, bs->head.leg), piece_branches(ns, bs->tail));
  }
  }
  return rest;
}

/* render (@unsafe in Bend): a worklist loop. */
static Text pp_render(const Piece *first) {
  TextBuilder out = builder_new();
  const Pieces *todo = Pieces_cons(first, NULL);
  while (todo != NULL) todo = pp_expand(&out, todo->head, todo->tail);
  return builder_text(&out);
}

Text pp_term(const Texts *ns, const Term *tm) {
  return pp_render(piece_term(ns, tm));
}

Text pp_shape(const Texts *ns, const TermShape *s) {
  return pp_render(piece_shape(ns, s));
}

Text pp_addr(const Texts *ns, const TermAddr *a) {
  return pp_render(piece_addr(ns, a));
}

Text pp_leg(const Texts *ns, const TermLeg *l) {
  return pp_render(piece_leg(ns, l));
}

Text pp_elim(const Texts *ns, const TermElim *e) {
  return pp_render(piece_elim(ns, e));
}
