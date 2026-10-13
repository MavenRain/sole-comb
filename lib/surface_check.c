/* lib/surface_check.c: twin of surface/check.bend (S6 session 3). A
   R.result(Program.checked) or R.result(Erase.rows) argument is the pair
   (done, value) with the error: done = 1 is Done{value}, done = 0 is
   Fail{error}. R.bind is a C if statement. */
#include "sole.h"

/* Syn.cat: String.concat of a fixed list of parts (surface_elab.c). */
static Text cat(const Text *parts, U32 count) {
  const Texts *list = NULL;
  for (; count > 0; count--) list = Texts_cons(parts[count - 1], list);
  return text_concat(list);
}

/* The Fail arm of report, erased_rows and erased_report. */
static Text failed(const KError *error) {
  Text parts[2];
  parts[0] = text_of_cstring("FAIL\n");
  parts[1] = error_to_string(error);
  return cat(parts, 2);
}

/* The Done arm of report and erased_rows: "OK\n" count "\n" form. */
static Text answered(Nat count, Text form) {
  Text parts[4];
  parts[0] = text_of_cstring("OK\n");
  parts[1] = bignum_to_string(bignum_from_nat(count));
  parts[2] = text_of_cstring("\n");
  parts[3] = form;
  return cat(parts, 4);
}

/* count (line 13). */
static Nat count(const CheckNamedEntries *rows) {
  return CheckNamedEntries_length(rows);
}

/* report (line 17). */
static Text report(int done, SurfaceProgramChecked value,
                   const KError *error) {
  if (!done) return failed(error);
  return answered(count(value.rows),
                  surface_program_checked_form(value.rows));
}

/* The bind of P.parse_bytes and Program.elab_program_in (lines 23, 34). */
static int checked(Text bytes, SurfaceProgramChecked *out,
                   const KError **error) {
  const SurfaceDecls *ds;
  if (!surface_parser_parse_bytes(bytes, &ds, error)) return 0;
  return surface_program_elab_program_in(budget_create(16384),
                                         global_initial(), ds, out, error);
}

/* source (line 22). */
Text surface_check_source(Text bytes) {
  SurfaceProgramChecked value;
  const KError *error = NULL;
  int done = checked(bytes, &value, &error);
  return report(done, value, error);
}

/* erased_rows (line 24). */
static Text erased_rows(Nat n, int done, const EraseCoreRows *erased,
                        const KError *error) {
  if (!done) return failed(error);
  return answered(n, erase_core_print(erased));
}

/* erased_report (line 28). */
static Text erased_report(int done, SurfaceProgramChecked value,
                          const KError *error) {
  const EraseCoreRows *erased = NULL;
  int ok;
  if (!done) return failed(error);
  ok = erase_core_program(value.globals, budget_create(16384), value.rows,
                          &erased, &error);
  return erased_rows(count(value.rows), ok, erased, error);
}

/* erased_source (line 33). */
Text surface_check_erased_source(Text bytes) {
  SurfaceProgramChecked value;
  const KError *error = NULL;
  int done = checked(bytes, &value, &error);
  return erased_report(done, value, error);
}
