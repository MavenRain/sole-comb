# Global and family maps at Kanon 69f3be51 global.ml.
# add/add_family are kernel-internal writes; checking must precede them.
import Base
import ./foundation.bend as F
import ./kernel_level.bend as L
import ./kernel_positivity.bend as Pos
import ./kernel_prim.bend as P
import ./kernel_term.bend as T

def telescope() -> Data: Pos.telescope
type def_entry is Data:
  Definition{ty: T.t, body: T.t, reducible: Bool, rec_arg: Maybe<&2, F.Int63.t>, partial: Bool}
type axiom_entry is Data:
  Postulate{ax_ty: T.t}
type prim_entry is Data:
  Primitive{p_ty: T.t, prim: P.t}
type entry is Data:
  Def{definition: def_entry}
  Axiom{postulate: axiom_entry}
  Prim{primitive: prim_entry}
type t is Data:
  Globals{entries: F.Map.t<entry>, families: F.Map.t<Pos.family>}

def empty() -> t: Globals{F.Map.empty(entry), F.Map.empty(Pos.family)}

def find(name: String, globals: t) -> Maybe<&2, entry>:
  match globals:
    case Globals{entries, _}: F.Map.get(entry, name, entries)

def add(name: String, value: entry, globals: t) -> t:
  match globals:
    case Globals{entries, families}: Globals{F.Map.insert(entry, name, value, entries), families}

def find_family(name: String, globals: t) -> Maybe<&2, Pos.family>:
  match globals:
    case Globals{_, families}: F.Map.get(Pos.family, name, families)

def add_family(name: String, fam: Pos.family, globals: t) -> t:
  match globals:
    case Globals{entries, families}: Globals{entries, F.Map.insert(Pos.family, name, fam, families)}

def entry_ty(value: entry) -> T.t:
  match value:
    case Def{Definition{ty, _, _, _, _}}: ty
    case Axiom{Postulate{ty}}: ty
    case Prim{Primitive{ty, _}}: ty

def def_of(value: entry) -> Maybe<&2, def_entry>:
  match value:
    case Def{d}: Some{d}
    case Axiom{_}: None{}
    case Prim{_}: None{}

def axiom_of(value: entry) -> Maybe<&2, axiom_entry>:
  match value:
    case Def{_}: None{}
    case Axiom{a}: Some{a}
    case Prim{_}: None{}

def prim_of(value: entry) -> Maybe<&2, prim_entry>:
  match value:
    case Def{_}: None{}
    case Axiom{_}: None{}
    case Prim{p}: Some{p}

def found_def(value: Maybe<&2, entry>) -> Maybe<&2, def_entry>:
  match value:
    case None{}: None{}
    case Some{e}: def_of(e)

def found_axiom(value: Maybe<&2, entry>) -> Maybe<&2, axiom_entry>:
  match value:
    case None{}: None{}
    case Some{e}: axiom_of(e)

def found_prim(value: Maybe<&2, entry>) -> Maybe<&2, prim_entry>:
  match value:
    case None{}: None{}
    case Some{e}: prim_of(e)

def find_def(name: String, globals: t) -> Maybe<&2, def_entry>: found_def(find(name, globals))
def find_axiom(name: String, globals: t) -> Maybe<&2, axiom_entry>: found_axiom(find(name, globals))
def find_prim(name: String, globals: t) -> Maybe<&2, prim_entry>: found_prim(find(name, globals))

def add_prims(prims: List<&2, P.t>, globals: t) -> t:
  match prims:
    case []: globals
    case +p <> rest: add_prims(rest, add(P.name(p), Prim{Primitive{P.ty(p), p}}, globals))

def initial() -> t:
  add_prims(P.catalog, add(P.nat_name, Axiom{Postulate{T.Univ{L.one}}}, empty))
