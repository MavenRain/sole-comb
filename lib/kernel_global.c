/* kernel_global.c: C twin of lib/kernel_global.bend (stage S3, session 1).
   Global and family maps at Kanon 69f3be51 global.ml. add and add_family
   are kernel-internal writes; checking must precede them.

   The maps are the F.Map tries of foundation.c. An entries value is a
   const GlobalEntry *; a families value is a const Family *. A Maybe of a
   pointer is NULL for None{}. */
#include "sole.h"

static const GlobalEntry *entry_new(GlobalEntryTag tag) {
  GlobalEntry *entry = (GlobalEntry *)arena_alloc(sizeof *entry);
  entry->tag = tag;
  return entry;
}

static const Globals *globals_new(const Map *entries, const Map *families) {
  Globals *globals = (Globals *)arena_alloc(sizeof *globals);
  globals->entries = entries;
  globals->families = families;
  return globals;
}

/* Def{Definition{ty, body, reducible, rec_arg, partial}} */
const GlobalEntry *global_definition(const Term *ty, const Term *body,
                                     Bool reducible, const Int63 *rec_arg,
                                     Bool partial) {
  GlobalEntry *entry = (GlobalEntry *)entry_new(GLOBAL_DEF);
  GlobalMemo *memo = (GlobalMemo *)arena_alloc(sizeof *memo);
  memo->globals = NULL;
  memo->value = NULL;
  entry->as.def.memo = memo;
  entry->as.def.ty = ty;
  entry->as.def.body = body;
  entry->as.def.reducible = reducible;
  entry->as.def.rec_arg = rec_arg;
  entry->as.def.partial = partial;
  return entry;
}

/* Axiom{Postulate{ax_ty}} */
const GlobalEntry *global_postulate(const Term *ty) {
  GlobalEntry *entry = (GlobalEntry *)entry_new(GLOBAL_AXIOM);
  entry->as.axiom.ty = ty;
  return entry;
}

/* Prim{Primitive{p_ty, prim}} */
const GlobalEntry *global_primitive(const Term *ty, Prim prim) {
  GlobalEntry *entry = (GlobalEntry *)entry_new(GLOBAL_PRIM);
  entry->as.prim.ty = ty;
  entry->as.prim.prim = prim;
  return entry;
}

const Globals *global_empty(void) {
  return globals_new(map_empty(), map_empty());
}

const GlobalEntry *global_find(Text name, const Globals *globals) {
  const void *value;
  return map_get(name, globals->entries, &value) ? value : NULL;
}

const Globals *global_add(Text name, const GlobalEntry *value,
                          const Globals *globals) {
  return globals_new(map_insert(name, value, globals->entries),
                     globals->families);
}

const Family *global_find_family(Text name, const Globals *globals) {
  const void *value;
  return map_get(name, globals->families, &value) ? value : NULL;
}

/* F.Map.bindings at P.family: the binding order of map_bindings, each
   value as a NamedFamily. */
const NamedFamilies *global_family_list(const Globals *globals) {
  const MapBindings *bindings = map_bindings(globals->families);
  const NamedFamilies *acc = NULL;
  for (; bindings != NULL; bindings = bindings->tail) {
    NamedFamily named;
    named.name = bindings->head.key;
    named.family = bindings->head.value;
    acc = NamedFamilies_cons(named, acc);
  }
  return NamedFamilies_reverse(acc);
}

const Globals *global_add_family(Text name, const Family *fam,
                                 const Globals *globals) {
  return globals_new(globals->entries,
                     map_insert(name, fam, globals->families));
}

const Term *global_entry_ty(const GlobalEntry *value) {
  switch (value->tag) {
  case GLOBAL_DEF: return value->as.def.ty;
  case GLOBAL_AXIOM: return value->as.axiom.ty;
  case GLOBAL_PRIM: return value->as.prim.ty;
  }
  return NULL; /* each tag returns above */
}

const GlobalDef *global_def_of(const GlobalEntry *value) {
  switch (value->tag) {
  case GLOBAL_DEF: return &value->as.def;
  case GLOBAL_AXIOM: return NULL;
  case GLOBAL_PRIM: return NULL;
  }
  return NULL; /* each tag returns above */
}

const GlobalAxiom *global_axiom_of(const GlobalEntry *value) {
  switch (value->tag) {
  case GLOBAL_DEF: return NULL;
  case GLOBAL_AXIOM: return &value->as.axiom;
  case GLOBAL_PRIM: return NULL;
  }
  return NULL; /* each tag returns above */
}

const GlobalPrim *global_prim_of(const GlobalEntry *value) {
  switch (value->tag) {
  case GLOBAL_DEF: return NULL;
  case GLOBAL_AXIOM: return NULL;
  case GLOBAL_PRIM: return &value->as.prim;
  }
  return NULL; /* each tag returns above */
}

const GlobalDef *global_found_def(const GlobalEntry *value) {
  return value == NULL ? NULL : global_def_of(value);
}

const GlobalAxiom *global_found_axiom(const GlobalEntry *value) {
  return value == NULL ? NULL : global_axiom_of(value);
}

const GlobalPrim *global_found_prim(const GlobalEntry *value) {
  return value == NULL ? NULL : global_prim_of(value);
}

const GlobalDef *global_find_def(Text name, const Globals *globals) {
  return global_found_def(global_find(name, globals));
}

const GlobalAxiom *global_find_axiom(Text name, const Globals *globals) {
  return global_found_axiom(global_find(name, globals));
}

const GlobalPrim *global_find_prim(Text name, const Globals *globals) {
  return global_found_prim(global_find(name, globals));
}

/* The self tail call is a loop; each add runs in the list order. */
const Globals *global_add_prims(const Prims *prims, const Globals *globals) {
  for (; prims != NULL; prims = prims->tail)
    globals = global_add(prim_name(prims->head),
                         global_primitive(prim_ty(prims->head), prims->head),
                         globals);
  return globals;
}

const Globals *global_initial(void) {
  return global_add_prims(
      prim_catalog(),
      global_add(prim_nat_name(), global_postulate(term_univ(level_one())),
                 global_empty()));
}
