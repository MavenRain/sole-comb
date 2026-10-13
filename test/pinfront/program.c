/* test/pinfront/program.c: twin of test/pinfront/program.bend (S7 session
   4). A copy of lib/surface_program.c (S6 session 3) with the pinfront
   delta. test/pinfront/program.bend is surface/program.bend with other
   import paths; its elab import is the COPIED test/pinfront/elab.bend, so
   each elab call goes to pinfront_elab_elab (types, bodies),
   pinfront_elab_elab_decl (ordinary) or pinfront_elab_elab_mu_group
   (declaration). Its order and totality imports are SHARED
   (test/pinfront/order.bend and test/pinfront/totality.bend are the
   surface files with other import paths and import no COPIED module): the
   surface_order_* and surface_totality_* calls stay. entry_text,
   checked_form and axiom_names do not reach elab: no copy here, a caller
   uses surface_program_checked_form and surface_program_axiom_names (cat
   went with them). "(line N)" is a surface/program.bend line, which is also
   the test/pinfront/program.bend line (the import blocks have the same
   size). R.result(A) is the int return (1 = Done with *out, 0 = Fail with
   *error). R.bind and R.map_result are C if statements; with_checked and
   with_entry are C code at the use site (no twin). Each walk over a list
   (types, postulates, install, bodies, members, declarations, guarded,
   program_loop) is a loop in the Bend order: the first Fail in list order
   is the answer. K.make(g, budget) is a check_make call at each use, as in
   the Bend code. */
#include "sole.h"

/* types (line 32). */
static int types(Budget budget, const Globals *globals,
                 const SurfaceRecDefs *ms, const SurfaceProgramTypeds **out,
                 const KError **error) {
  const SurfaceProgramTypeds *acc = NULL;
  for (; ms != NULL; ms = ms->tail) {
    SurfaceProgramTyped row;
    if (!pinfront_elab_elab(check_make(globals, budget), NULL, ms->head.ty,
                           &row.ty, error))
      return 0;
    row.name = ms->head.name;
    row.body = ms->head.body;
    acc = SurfaceProgramTypeds_cons(row, acc);
  }
  *out = SurfaceProgramTypeds_reverse(acc);
  return 1;
}

/* postulates (line 38). */
static const CheckDecls *postulates(const SurfaceProgramTypeds *ms) {
  const CheckDecls *acc = NULL;
  for (; ms != NULL; ms = ms->tail) {
    CheckDecl d;
    d.name = ms->head.name;
    d.kind = CHECK_POSTULATE;
    d.ty = ms->head.ty;
    d.body = NULL;
    acc = CheckDecls_cons(d, acc);
  }
  return CheckDecls_reverse(acc);
}

/* install (line 42). */
static const Globals *install(const CheckNamedEntries *rows,
                              const Globals *g) {
  for (; rows != NULL; rows = rows->tail)
    g = global_add(rows->head.name, rows->head.entry, g);
  return g;
}

/* bodies (line 46): the type value, then the body against it. */
static int bodies(Budget budget, const Globals *g,
                  const SurfaceProgramTypeds *ms,
                  const SurfaceProgramBodies **out, const KError **error) {
  const SurfaceProgramBodies *acc = NULL;
  for (; ms != NULL; ms = ms->tail) {
    SurfaceProgramBody row;
    if (!eval_eval(g, NULL, ms->head.ty, &row.tyv, error)) return 0;
    if (!pinfront_elab_elab(check_make(g, budget), row.tyv, ms->head.body,
                           &row.body, error))
      return 0;
    row.name = ms->head.name;
    row.ty = ms->head.ty;
    acc = SurfaceProgramBodies_cons(row, acc);
  }
  *out = SurfaceProgramBodies_reverse(acc);
  return 1;
}

/* members (line 53). */
static const SurfaceOrderMembers *members(const SurfaceProgramBodies *bs) {
  const SurfaceOrderMembers *acc = NULL;
  for (; bs != NULL; bs = bs->tail) {
    SurfaceOrderMember m;
    m.name = bs->head.name;
    m.body = bs->head.body;
    acc = SurfaceOrderMembers_cons(m, acc);
  }
  return SurfaceOrderMembers_reverse(acc);
}

/* declarations (line 57). */
static const CheckDecls *declarations(const SurfaceProgramBodies *bs) {
  const CheckDecls *acc = NULL;
  for (; bs != NULL; bs = bs->tail) {
    CheckDecl d;
    d.name = bs->head.name;
    d.kind = CHECK_DEFINITION;
    d.ty = bs->head.ty;
    d.body = bs->head.body;
    acc = CheckDecls_cons(d, acc);
  }
  return CheckDecls_reverse(acc);
}

/* rec_arg (line 61): Some{k} is a pointer to the arg cell of the
   certificate (an arena value that does not change). */
static const Int63 *rec_arg(const SurfaceOrderCertificate *cert,
                            const Term *tm) {
  return surface_order_mentions(cert->group, tm) ? &cert->arg : NULL;
}

/* guarded (line 64). */
static int guarded(Budget budget, const Globals *g,
                   const SurfaceOrderCertificate *cert,
                   const SurfaceProgramBodies *bs,
                   const CheckNamedEntries **out, const KError **error) {
  const CheckNamedEntries *acc = NULL;
  for (; bs != NULL; bs = bs->tail) {
    const Term *tm;
    CheckNamedEntry row;
    if (!surface_order_translate(cert, bs->head.body, &tm, error)) return 0;
    if (!check_check_term(g, budget, tm, bs->head.tyv, error)) return 0;
    row.name = bs->head.name;
    row.entry = global_definition(bs->head.ty, tm, YES,
                                  rec_arg(cert, bs->head.body), NO);
    acc = CheckNamedEntries_cons(row, acc);
  }
  *out = CheckNamedEntries_reverse(acc);
  return 1;
}

/* admitted (line 71): cert NULL = None{}. */
static int admitted(Budget budget, const Globals *globals,
                    const Globals *provisional,
                    const SurfaceProgramBodies *bs,
                    const SurfaceOrderCertificate *cert,
                    const CheckNamedEntries **out, const KError **error) {
  if (cert == NULL)
    return check_check_decls(globals, budget, declarations(bs), out, error);
  return guarded(budget, provisional, cert, bs, out, error);
}

/* in_provisional (line 75). */
static int in_provisional(Budget budget, const Globals *globals,
                          const SurfaceProgramTypeds *ts,
                          const Globals *provisional,
                          SurfaceProgramChecked *out, const KError **error) {
  const SurfaceProgramBodies *bs;
  const SurfaceOrderCertificate *cert;
  const CheckNamedEntries *rows;
  if (!bodies(budget, provisional, ts, &bs, error)) return 0;
  if (!surface_totality_guard_group(budget, provisional, members(bs), &cert,
                                    error))
    return 0;
  if (!admitted(budget, globals, provisional, bs, cert, &rows, error))
    return 0;
  out->globals = install(rows, globals);
  out->rows = rows;
  return 1;
}

/* elab_rec_group (line 79). */
static int elab_rec_group(Budget budget, const Globals *globals,
                          const SurfaceRecDefs *ms,
                          SurfaceProgramChecked *out, const KError **error) {
  const SurfaceProgramTypeds *ts;
  const CheckNamedEntries *ds;
  if (!types(budget, globals, ms, &ts, error)) return 0;
  if (!check_check_decls(globals, budget, postulates(ts), &ds, error))
    return 0;
  return in_provisional(budget, globals, ts, install(ds, globals), out,
                        error);
}

/* ordinary (line 83). The Bend code builds the same DDef or DAxiom again;
   the C code gives d. R.one_of is Some only for a list of one entry. */
static int ordinary(Budget budget, const Globals *g, const SurfaceDecl *d,
                    SurfaceProgramChecked *out, const KError **error) {
  CheckDecl row;
  const CheckNamedEntries *rows;
  if (!pinfront_elab_elab_decl(check_make(g, budget), d, &row, error))
    return 0;
  if (!check_check_decls(g, budget, CheckDecls_cons(row, NULL), &rows,
                         error))
    return 0;
  if (rows == NULL || rows->tail != NULL) {
    *error = error_cannot_infer(text_of_cstring(
        "the checker answered no entry for a declaration"));
    return 0;
  }
  out->globals = global_add(rows->head.name, rows->head.entry, g);
  out->rows = CheckNamedEntries_cons(rows->head, NULL);
  return 1;
}

/* declaration (line 88). */
static int declaration(Budget budget, const Globals *g, const SurfaceDecl *d,
                       SurfaceProgramChecked *out, const KError **error) {
  switch (d->tag) {
  case SURFACE_DDEF:
  case SURFACE_DAXIOM:
    return ordinary(budget, g, d, out, error);
  case SURFACE_DMU: {
    const Globals *next;
    if (!pinfront_elab_elab_mu_group(budget, g, d->as.mu.families, &next,
                                    error))
      return 0;
    out->globals = next;
    out->rows = NULL;
    return 1;
  }
  case SURFACE_DREC:
    return elab_rec_group(budget, g, d->as.rec.members, out, error);
  }
  return 0;
}

/* program_loop (line 94): the rows of each declaration in order, the
   globals of the last one. */
static int program_loop(const SurfaceDecls *ds, Budget budget,
                        const Globals *g, SurfaceProgramChecked *out,
                        const KError **error) {
  const CheckNamedEntries *acc = NULL;
  for (; ds != NULL; ds = ds->tail) {
    SurfaceProgramChecked state;
    const CheckNamedEntries *rows;
    if (!declaration(budget, g, ds->head, &state, error)) return 0;
    for (rows = state.rows; rows != NULL; rows = rows->tail)
      acc = CheckNamedEntries_cons(rows->head, acc);
    g = state.globals;
  }
  out->globals = g;
  out->rows = CheckNamedEntries_reverse(acc);
  return 1;
}

/* elab_program_in (line 101). */
int pinfront_program_elab_program_in(Budget budget, const Globals *g,
                                    const SurfaceDecls *ds,
                                    SurfaceProgramChecked *out,
                                    const KError **error) {
  return program_loop(ds, budget, g, out, error);
}
