/* lib/surface_syntax.c: twin of surface/syntax.bend (S5 session 1b).
   The surface syntax and its canonical printer (Kanon 69f3be51
   surface/syntax.ml). The Bend printer `render` (@unsafe, line 150) takes
   a render_job and builds each text with S.cat. The C printer writes each
   text to one TextBuilder in the same order: one static writer per
   render_job constructor (put_at, put_binder, put_field, put_branch,
   put_motive, put_optional_motive, put_raw). The writers recurse over the
   term depth (C recursion, PORT-PLAN hazard 4). A RenderJoin over a list
   is a loop, and each jobs(~A, ~pack, xs) site is that loop with the pack
   written in place. Each texts(~A, ~show, "", xs) site (all have sep "")
   is a loop that writes show(x) in list order. */
#include "sole.h"

/* ---------------------------------------------------------------- make */

static SurfaceTerm *term_new(SurfaceTermTag tag) {
  SurfaceTerm *t = (SurfaceTerm *)arena_alloc(sizeof *t);
  t->tag = tag;
  return t;
}

const SurfaceTerm *surface_syntax_svar(Text name) {
  SurfaceTerm *t = term_new(SURFACE_SVAR);
  t->as.var.name = name;
  return t;
}

const SurfaceTerm *surface_syntax_snat(Bignum value) {
  SurfaceTerm *t = term_new(SURFACE_SNAT);
  t->as.nat.value = value;
  return t;
}

const SurfaceTerm *surface_syntax_sprop(void) {
  return term_new(SURFACE_SPROP);
}

const SurfaceTerm *surface_syntax_stype(Int63 level) {
  SurfaceTerm *t = term_new(SURFACE_STYPE);
  t->as.type.level = level;
  return t;
}

const SurfaceTerm *surface_syntax_sprim(SurfacePrim prim) {
  SurfaceTerm *t = term_new(SURFACE_SPRIM);
  t->as.prim.prim = prim;
  return t;
}

const SurfaceTerm *surface_syntax_sunit(void) {
  return term_new(SURFACE_SUNIT);
}

const SurfaceTerm *surface_syntax_sauto(void) {
  return term_new(SURFACE_SAUTO);
}

const SurfaceTerm *surface_syntax_spair(const SurfaceTerm *fst,
                                       const SurfaceTerm *snd) {
  SurfaceTerm *t = term_new(SURFACE_SPAIR);
  t->as.pair.fst = fst;
  t->as.pair.snd = snd;
  return t;
}

const SurfaceTerm *surface_syntax_stuple(const SurfaceTerms *items) {
  SurfaceTerm *t = term_new(SURFACE_STUPLE);
  t->as.tuple.items = items;
  return t;
}

const SurfaceTerm *surface_syntax_ssum(const SurfaceTerms *items) {
  SurfaceTerm *t = term_new(SURFACE_SSUM);
  t->as.sum.items = items;
  return t;
}

const SurfaceTerm *surface_syntax_sprod(const SurfaceTerms *items) {
  SurfaceTerm *t = term_new(SURFACE_SPROD);
  t->as.prod.items = items;
  return t;
}

const SurfaceTerm *surface_syntax_sproj(const SurfaceTerm *term, Int63 key) {
  SurfaceTerm *t = term_new(SURFACE_SPROJ);
  t->as.proj.term = term;
  t->as.proj.key = key;
  return t;
}

const SurfaceTerm *surface_syntax_sinj(Int63 key, Int63 size,
                                      const SurfaceTerm *term) {
  SurfaceTerm *t = term_new(SURFACE_SINJ);
  t->as.inj.key = key;
  t->as.inj.size = size;
  t->as.inj.term = term;
  return t;
}

const SurfaceTerm *surface_syntax_sabsurd(const SurfaceTerm *term) {
  SurfaceTerm *t = term_new(SURFACE_SABSURD);
  t->as.absurd.term = term;
  return t;
}

const SurfaceTerm *surface_syntax_sapp(const SurfaceTerm *function,
                                      const SurfaceTerm *argument) {
  SurfaceTerm *t = term_new(SURFACE_SAPP);
  t->as.app.function = function;
  t->as.app.argument = argument;
  return t;
}

const SurfaceTerm *surface_syntax_sfun(const SurfaceBinders *binders,
                                      const SurfaceTerm *body) {
  SurfaceTerm *t = term_new(SURFACE_SFUN);
  t->as.fun.binders = binders;
  t->as.fun.body = body;
  return t;
}

const SurfaceTerm *surface_syntax_sarrow(SurfaceBinder binder,
                                        const SurfaceTerm *body) {
  SurfaceTerm *t = term_new(SURFACE_SARROW);
  t->as.arrow.binder = binder;
  t->as.arrow.body = body;
  return t;
}

const SurfaceTerm *surface_syntax_sstar(SurfaceBinder binder,
                                       const SurfaceTerm *body) {
  SurfaceTerm *t = term_new(SURFACE_SSTAR);
  t->as.star.binder = binder;
  t->as.star.body = body;
  return t;
}

const SurfaceTerm *surface_syntax_slet(Text name, const SurfaceTerm *ty,
                                      const SurfaceTerm *value,
                                      const SurfaceTerm *body) {
  SurfaceTerm *t = term_new(SURFACE_SLET);
  t->as.let.name = name;
  t->as.let.ty = ty;
  t->as.let.value = value;
  t->as.let.body = body;
  return t;
}

const SurfaceTerm *surface_syntax_sann(const SurfaceTerm *term,
                                      const SurfaceTerm *ty) {
  SurfaceTerm *t = term_new(SURFACE_SANN);
  t->as.ann.term = term;
  t->as.ann.ty = ty;
  return t;
}

static const SurfaceTerm *elim_new(SurfaceTermTag tag,
                                   const SurfaceTerm *scrutinee,
                                   const SurfaceMotive *motive,
                                   const SurfaceBranches *branches) {
  SurfaceTerm *t = term_new(tag);
  t->as.elim.scrutinee = scrutinee;
  t->as.elim.motive = motive;
  t->as.elim.branches = branches;
  return t;
}

const SurfaceTerm *surface_syntax_scase(const SurfaceTerm *scrutinee,
                                       const SurfaceMotive *motive,
                                       const SurfaceBranches *branches) {
  return elim_new(SURFACE_SCASE, scrutinee, motive, branches);
}

const SurfaceTerm *surface_syntax_smatch(const SurfaceTerm *scrutinee,
                                        const SurfaceMotive *motive,
                                        const SurfaceBranches *branches) {
  return elim_new(SURFACE_SMATCH, scrutinee, motive, branches);
}

SurfaceBinder surface_syntax_binder(Quantity q, Text name,
                                    const SurfaceTerm *ty) {
  SurfaceBinder b = {q, name, ty};
  return b;
}

const SurfaceMotive *surface_syntax_motive(Text self, MaybeText ind,
                                           const Texts *idx,
                                           const SurfaceTerm *body) {
  SurfaceMotive *mo = (SurfaceMotive *)arena_alloc(sizeof *mo);
  mo->self = self;
  mo->ind = ind;
  mo->idx = idx;
  mo->body = body;
  return mo;
}

SurfaceField surface_syntax_field(Quantity q, Text name,
                                  const SurfaceTerm *ty) {
  SurfaceField f = {q, name, ty};
  return f;
}

SurfaceBranch surface_syntax_br_leg(Int63 key, const SurfaceBinders *binders,
                                    const SurfaceTerm *body) {
  SurfaceBranch br;
  br.tag = SURFACE_BR_LEG;
  br.as.leg.key = key;
  br.as.leg.binders = binders;
  br.as.leg.body = body;
  return br;
}

SurfaceBranch surface_syntax_br_ctor(Text name, const SurfaceFields *fields,
                                     const SurfaceTerm *body) {
  SurfaceBranch br;
  br.tag = SURFACE_BR_CTOR;
  br.as.ctor.name = name;
  br.as.ctor.fields = fields;
  br.as.ctor.body = body;
  return br;
}

SurfaceBranch surface_syntax_br_default(const SurfaceTerm *body) {
  SurfaceBranch br;
  br.tag = SURFACE_BR_DEFAULT;
  br.as.other.body = body;
  return br;
}

SurfaceFamCtor surface_syntax_fam_ctor(Text name, const SurfaceTerm *ty) {
  SurfaceFamCtor fc = {name, ty};
  return fc;
}

SurfaceFam surface_syntax_fam(Text name, const SurfaceBinders *params,
                              const SurfaceTerm *ty,
                              const SurfaceFamCtors *ctors) {
  SurfaceFam fm = {name, params, ty, ctors};
  return fm;
}

SurfaceRecDef surface_syntax_rec_def(Text name, const SurfaceTerm *ty,
                                     const SurfaceTerm *body) {
  SurfaceRecDef r = {name, ty, body};
  return r;
}

static SurfaceDecl *decl_new(SurfaceDeclTag tag) {
  SurfaceDecl *d = (SurfaceDecl *)arena_alloc(sizeof *d);
  d->tag = tag;
  return d;
}

const SurfaceDecl *surface_syntax_ddef(Text name, const SurfaceTerm *ty,
                                       const SurfaceTerm *body) {
  SurfaceDecl *d = decl_new(SURFACE_DDEF);
  d->as.def.name = name;
  d->as.def.ty = ty;
  d->as.def.body = body;
  return d;
}

const SurfaceDecl *surface_syntax_daxiom(Text name, const SurfaceTerm *ty) {
  SurfaceDecl *d = decl_new(SURFACE_DAXIOM);
  d->as.axiom.name = name;
  d->as.axiom.ty = ty;
  return d;
}

const SurfaceDecl *surface_syntax_dmu(const SurfaceFams *families) {
  SurfaceDecl *d = decl_new(SURFACE_DMU);
  d->as.mu.families = families;
  return d;
}

const SurfaceDecl *surface_syntax_drec(const SurfaceRecDefs *members) {
  SurfaceDecl *d = decl_new(SURFACE_DREC);
  d->as.rec.members = members;
  return d;
}

/* ---------------------------------------------------------------- helpers */

static void put(TextBuilder *b, const char *ascii) {
  builder_append_cstring(b, ascii);
}

Text surface_syntax_prim_name(SurfacePrim p) {
  switch (p) {
    case SURFACE_PRIM_ADD: return text_of_cstring("natAdd");
    case SURFACE_PRIM_SUB: return text_of_cstring("natSub");
    case SURFACE_PRIM_MUL: return text_of_cstring("natMul");
    case SURFACE_PRIM_EQ: return text_of_cstring("natEq");
    case SURFACE_PRIM_LT: return text_of_cstring("natLt");
  }
  return text_end(); /* unreachable: the switch is exhaustive */
}

/* level_of (line 89): 2 = atom, 1 = application, 0 = binder form. */
static Nat level_of(const SurfaceTerm *s) {
  switch (s->tag) {
    case SURFACE_SVAR: return 2;
    case SURFACE_SNAT: return 2;
    case SURFACE_SPROP: return 2;
    case SURFACE_STYPE: return 2;
    case SURFACE_SPRIM: return 2;
    case SURFACE_SUNIT: return 2;
    case SURFACE_SAUTO: return 2;
    case SURFACE_SPAIR: return 2;
    case SURFACE_STUPLE: return 2;
    case SURFACE_SSUM: return 2;
    case SURFACE_SPROD: return 2;
    case SURFACE_SPROJ: return 2;
    case SURFACE_SANN: return 2;
    case SURFACE_SINJ: return 1;
    case SURFACE_SABSURD: return 1;
    case SURFACE_SAPP: return 1;
    case SURFACE_SFUN: return 0;
    case SURFACE_SARROW: return 0;
    case SURFACE_SSTAR: return 0;
    case SURFACE_SLET: return 0;
    case SURFACE_SCASE: return 0;
    case SURFACE_SMATCH: return 0;
  }
  return 0; /* unreachable: the switch is exhaustive */
}

/* mark (line 113). */
static const char *mark(Quantity q) {
  switch (q) {
    case QUANTITY_ZERO: return "0 ";
    case QUANTITY_ONE: return "1 ";
    case QUANTITY_MANY: return "";
  }
  return ""; /* unreachable: the switch is exhaustive */
}

/* ---------------------------------------------------------------- render */

static void put_raw(TextBuilder *b, const SurfaceTerm *s);

/* RenderAt{lvl, s} = at_text(level_of(s) >= lvl, raw(s)) (line 156). */
static void put_at(TextBuilder *b, Nat lvl, const SurfaceTerm *s) {
  if (level_of(s) >= lvl) {
    put_raw(b, s);
    return;
  }
  put(b, "(");
  put_raw(b, s);
  put(b, ")");
}

/* RenderBinderText{b} (line 158). */
static void put_binder(TextBuilder *b, SurfaceBinder x) {
  put(b, "(");
  put(b, mark(x.q));
  builder_append(b, x.name);
  put(b, " : ");
  put_at(b, 0, x.ty);
  put(b, ")");
}

/* RenderFieldText{f} (line 161). */
static void put_field(TextBuilder *b, SurfaceField f) {
  put(b, " ");
  if (f.ty == NULL) {
    put(b, mark(f.q));
    builder_append(b, f.name);
    return;
  }
  put_binder(b, surface_syntax_binder(f.q, f.name, f.ty));
}

/* RenderBranchText{br} (line 165). Both RenderJoin sites have sep "". */
static void put_branch(TextBuilder *b, SurfaceBranch br) {
  switch (br.tag) {
    case SURFACE_BR_LEG:
      put(b, " | ");
      builder_append(b, int63_show(br.as.leg.key));
      for (const SurfaceBinders *bs = br.as.leg.binders; bs != NULL;
           bs = bs->tail) {
        put(b, " ");
        put_binder(b, bs->head);
      }
      put(b, " => ");
      put_at(b, 1, br.as.leg.body);
      return;
    case SURFACE_BR_CTOR:
      put(b, " | ");
      builder_append(b, br.as.ctor.name);
      for (const SurfaceFields *fs = br.as.ctor.fields; fs != NULL;
           fs = fs->tail)
        put_field(b, fs->head);
      put(b, " => ");
      put_at(b, 1, br.as.ctor.body);
      return;
    case SURFACE_BR_DEFAULT:
      put(b, " | else: ");
      put_at(b, 0, br.as.other.body);
      return;
  }
}

/* ind_text (line 127). */
static void put_ind(TextBuilder *b, MaybeText ind, const Texts *idx) {
  if (ind.some == NO) return;
  put(b, " in ");
  builder_append(b, ind.text);
  for (; idx != NULL; idx = idx->tail) {
    put(b, " ");
    builder_append(b, idx->head);
  }
}

/* RenderMotiveText{mo} (line 172). */
static void put_motive(TextBuilder *b, const SurfaceMotive *mo) {
  put(b, " as ");
  builder_append(b, mo->self);
  put_ind(b, mo->ind, mo->idx);
  put(b, " return ");
  put_at(b, 1, mo->body);
}

/* RenderOptionalMotive{mo} (line 175): NULL = None. */
static void put_optional_motive(TextBuilder *b, const SurfaceMotive *mo) {
  if (mo == NULL) return;
  put_motive(b, mo);
}

/* STuple, SSum, SProd (lines 189 to 191): RenderJoin{", ", RenderAt{0n}}. */
static void put_items(TextBuilder *b, const char *word,
                      const SurfaceTerms *xs) {
  put(b, word);
  for (const SurfaceTerms *it = xs; it != NULL; it = it->tail) {
    if (it != xs) put(b, ", ");
    put_at(b, 0, it->head);
  }
  put(b, ")");
}

/* SCase, SMatch (lines 201 and 202): RenderJoin{"", RenderBranchText}. */
static void put_elim(TextBuilder *b, const char *word, const SurfaceTerm *s) {
  put(b, word);
  put_at(b, 1, s->as.elim.scrutinee);
  put_optional_motive(b, s->as.elim.motive);
  put(b, " with");
  for (const SurfaceBranches *brs = s->as.elim.branches; brs != NULL;
       brs = brs->tail)
    put_branch(b, brs->head);
}

/* RenderRaw{s} (line 179). */
static void put_raw(TextBuilder *b, const SurfaceTerm *s) {
  switch (s->tag) {
    case SURFACE_SVAR:
      builder_append(b, s->as.var.name);
      return;
    case SURFACE_SNAT:
      builder_append(b, bignum_to_string(s->as.nat.value));
      return;
    case SURFACE_SPROP:
      put(b, "Prop");
      return;
    case SURFACE_STYPE:
      put(b, "Type ");
      builder_append(b, int63_show(s->as.type.level));
      return;
    case SURFACE_SPRIM:
      builder_append(b, surface_syntax_prim_name(s->as.prim.prim));
      return;
    case SURFACE_SUNIT:
      put(b, "()");
      return;
    case SURFACE_SAUTO:
      put(b, "auto");
      return;
    case SURFACE_SPAIR:
      put(b, "(");
      put_at(b, 0, s->as.pair.fst);
      put(b, ", ");
      put_at(b, 0, s->as.pair.snd);
      put(b, ")");
      return;
    case SURFACE_STUPLE:
      put_items(b, "tuple (", s->as.tuple.items);
      return;
    case SURFACE_SSUM:
      put_items(b, "sum (", s->as.sum.items);
      return;
    case SURFACE_SPROD:
      put_items(b, "prod (", s->as.prod.items);
      return;
    case SURFACE_SPROJ:
      put_at(b, 2, s->as.proj.term);
      put(b, ".");
      builder_append(b, int63_show(s->as.proj.key));
      return;
    case SURFACE_SINJ:
      put(b, "inj ");
      builder_append(b, int63_show(s->as.inj.key));
      put(b, " of ");
      builder_append(b, int63_show(s->as.inj.size));
      put(b, " ");
      put_at(b, 1, s->as.inj.term);
      return;
    case SURFACE_SABSURD:
      put(b, "absurd ");
      put_at(b, 1, s->as.absurd.term);
      return;
    case SURFACE_SAPP:
      put_at(b, 1, s->as.app.function);
      put(b, " ");
      put_at(b, 2, s->as.app.argument);
      return;
    case SURFACE_SFUN:
      put(b, "fun ");
      for (const SurfaceBinders *bs = s->as.fun.binders; bs != NULL;
           bs = bs->tail) {
        if (bs != s->as.fun.binders) put(b, " ");
        put_binder(b, bs->head);
      }
      put(b, " => ");
      put_at(b, 0, s->as.fun.body);
      return;
    case SURFACE_SARROW:
      put_binder(b, s->as.arrow.binder);
      put(b, " -> ");
      put_at(b, 0, s->as.arrow.body);
      return;
    case SURFACE_SSTAR:
      put_binder(b, s->as.star.binder);
      put(b, " * ");
      put_at(b, 0, s->as.star.body);
      return;
    case SURFACE_SLET:
      put(b, "let ");
      builder_append(b, s->as.let.name);
      put(b, " : ");
      put_at(b, 1, s->as.let.ty);
      put(b, " := ");
      put_at(b, 1, s->as.let.value);
      put(b, " in ");
      put_at(b, 0, s->as.let.body);
      return;
    case SURFACE_SANN:
      put(b, "(");
      put_at(b, 0, s->as.ann.term);
      put(b, " : ");
      put_at(b, 0, s->as.ann.ty);
      put(b, ")");
      return;
    case SURFACE_SCASE:
      put_elim(b, "case ", s);
      return;
    case SURFACE_SMATCH:
      put_elim(b, "match ", s);
      return;
  }
}

/* ---------------------------------------------------------------- decls */

/* fam_ctor_text (line 225). */
static void put_fam_ctor(TextBuilder *b, SurfaceFamCtor fc) {
  put(b, "| ");
  builder_append(b, fc.name);
  put(b, " : ");
  put_at(b, 0, fc.ty);
  put(b, "\n");
}

/* fam_text (line 228): two texts(..., "", xs) sites. */
static void put_fam(TextBuilder *b, Text word, SurfaceFam fm) {
  builder_append(b, word);
  put(b, " ");
  builder_append(b, fm.name);
  for (const SurfaceBinders *ps = fm.params; ps != NULL; ps = ps->tail) {
    put(b, " ");
    put_binder(b, ps->head);
  }
  put(b, " : ");
  put_at(b, 0, fm.ty);
  put(b, " :=\n");
  for (const SurfaceFamCtors *cs = fm.ctors; cs != NULL; cs = cs->tail)
    put_fam_ctor(b, cs->head);
}

/* rec_text (line 232): the non-tail self call is a loop (the output order
   is the list order); word is "and" after the first member. */
static void put_rec(TextBuilder *b, const SurfaceRecDefs *ms, Text word) {
  for (; ms != NULL; ms = ms->tail) {
    builder_append(b, word);
    put(b, " ");
    builder_append(b, ms->head.name);
    put(b, " : ");
    put_at(b, 0, ms->head.ty);
    put(b, " := ");
    put_at(b, 0, ms->head.body);
    put(b, "\n");
    word = text_of_cstring("and");
  }
}

/* DMu (lines 240 to 242): [] = "", one family = fam_text("mu", fm), two or
   more = "mutual\n", each fam_text("mu", f), "end\n". */
static void put_mu(TextBuilder *b, const SurfaceFams *fs) {
  Text mu = text_of_cstring("mu");
  if (fs == NULL) return;
  if (fs->tail == NULL) {
    put_fam(b, mu, fs->head);
    return;
  }
  put(b, "mutual\n");
  for (; fs != NULL; fs = fs->tail) put_fam(b, mu, fs->head);
  put(b, "end\n");
}

/* decl_text (line 236). */
static void put_decl(TextBuilder *b, const SurfaceDecl *d) {
  switch (d->tag) {
    case SURFACE_DDEF:
      put(b, "def ");
      builder_append(b, d->as.def.name);
      put(b, " : ");
      put_at(b, 0, d->as.def.ty);
      put(b, " := ");
      put_at(b, 0, d->as.def.body);
      put(b, "\n");
      return;
    case SURFACE_DAXIOM:
      put(b, "axiom ");
      builder_append(b, d->as.axiom.name);
      put(b, " : ");
      put_at(b, 0, d->as.axiom.ty);
      put(b, "\n");
      return;
    case SURFACE_DMU:
      put_mu(b, d->as.mu.families);
      return;
    case SURFACE_DREC:
      put_rec(b, d->as.rec.members, text_of_cstring("def rec"));
      return;
  }
}

/* ---------------------------------------------------------------- texts */

Text surface_syntax_at(Nat lvl, const SurfaceTerm *s) {
  TextBuilder b = builder_new();
  put_at(&b, lvl, s);
  return builder_text(&b);
}

Text surface_syntax_binder_text(SurfaceBinder x) {
  TextBuilder b = builder_new();
  put_binder(&b, x);
  return builder_text(&b);
}

Text surface_syntax_field_text(SurfaceField f) {
  TextBuilder b = builder_new();
  put_field(&b, f);
  return builder_text(&b);
}

Text surface_syntax_branch_text(SurfaceBranch br) {
  TextBuilder b = builder_new();
  put_branch(&b, br);
  return builder_text(&b);
}

Text surface_syntax_motive_text(const SurfaceMotive *mo) {
  TextBuilder b = builder_new();
  put_motive(&b, mo);
  return builder_text(&b);
}

Text surface_syntax_optional_motive(const SurfaceMotive *mo) {
  TextBuilder b = builder_new();
  put_optional_motive(&b, mo);
  return builder_text(&b);
}

Text surface_syntax_raw(const SurfaceTerm *s) {
  TextBuilder b = builder_new();
  put_raw(&b, s);
  return builder_text(&b);
}

Text surface_syntax_fam_ctor_text(SurfaceFamCtor fc) {
  TextBuilder b = builder_new();
  put_fam_ctor(&b, fc);
  return builder_text(&b);
}

Text surface_syntax_fam_text(Text word, SurfaceFam fm) {
  TextBuilder b = builder_new();
  put_fam(&b, word, fm);
  return builder_text(&b);
}

Text surface_syntax_rec_text(const SurfaceRecDefs *ms, Text word) {
  TextBuilder b = builder_new();
  put_rec(&b, ms, word);
  return builder_text(&b);
}

Text surface_syntax_decl_text(const SurfaceDecl *d) {
  TextBuilder b = builder_new();
  put_decl(&b, d);
  return builder_text(&b);
}

/* print (line 244): texts(~decl, decl_text, "", ds). */
Text surface_syntax_print(const SurfaceDecls *ds) {
  TextBuilder b = builder_new();
  for (; ds != NULL; ds = ds->tail) put_decl(&b, ds->head);
  return builder_text(&b);
}
