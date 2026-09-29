# Whole-program entry points at Kanon 69f3be51 surface/elab.ml:994-1179.
import Base
import ../../lib/foundation.bend as F
import ../../lib/kernel_budget.bend as B
import ../../lib/kernel_error.bend as E
import ../../lib/kernel_global.bend as G
import ../../lib/kernel_term.bend as T
import ../../lib/kernel_value.bend as V
import ../../lib/kernel_rules.bend as R
import ../../lib/kernel_check.bend as K
import ../../lib/kernel_eval.bend as Ev
import ../../lib/kernel_pp.bend as PP
import ./syntax.bend as S
import ./elab.bend as Elab
import ./order.bend as O
import ./totality.bend as Totality

def entries() -> Data: List<&2, F.Pair2<String, G.entry>>
type checked is Data:
  Checked{globals: G.t, rows: entries}
type typed is Data:
  Typed{name: String, ty: T.t, body: S.t}
type body is Data:
  Body{name: String, ty: T.t, tyv: V.t, body: T.t}
def with_checked(-A: Type, state: checked, next: G.t -> entries -> A) -> A:
  match state:
    case Checked{g, rows}: next(g, rows)
def with_entry(-A: Type, row: F.Pair2<String, G.entry>, next: String -> G.entry -> A) -> A:
  match row:
    case F.Pair2{n, e}: next(n, e)

def types(+budget: B.t, +globals: G.t, ms: List<&2, S.rec_def_of<S.t>>) -> R.result(List<&2, typed>):
  match ms:
    case []: Done{[]}
    case S.RecDef{n, ty, b} <> rest:
      R.bind(T.t, List<&2, typed>, Elab.elab(K.make(globals, budget), None{}, ty), t =>
        R.map_result(List<&2, typed>, List<&2, typed>, ts => Typed{n, t, b} <> ts, types(budget, globals, rest)))
def postulates(ms: List<&2, typed>) -> List<&2, K.decl>:
  match ms:
    case []: []
    case Typed{n, ty, _} <> rest: K.Declaration{n, K.Postulate{}, ty, None{}} <> postulates(rest)
def install(rows: entries, g: G.t) -> G.t:
  match rows:
    case []: g
    case F.Pair2{n, e} <> rest: install(rest, G.add(n, e, g))
def bodies(+budget: B.t, +g: G.t, ms: List<&2, typed>) -> R.result(List<&2, body>):
  match ms:
    case []: Done{[]}
    case Typed{n, +ty, b} <> rest:
      R.bind(V.t, List<&2, body>, Ev.eval(g, [], ty), +tyv =>
        R.bind(T.t, List<&2, body>, Elab.elab(K.make(g, budget), Some{tyv}, b), tm =>
          R.map_result(List<&2, body>, List<&2, body>, bs => Body{n, ty, tyv, tm} <> bs, bodies(budget, g, rest))))
def members(bs: List<&2, body>) -> List<&2, F.Pair2<String, T.t>>:
  match bs:
    case []: []
    case Body{n, _, _, tm} <> rest: F.Pair2{n, tm} <> members(rest)
def declarations(bs: List<&2, body>) -> List<&2, K.decl>:
  match bs:
    case []: []
    case Body{n, ty, _, tm} <> rest: K.Declaration{n, K.Definition{}, ty, Some{tm}} <> declarations(rest)
def rec_arg(cert: O.t, tm: T.t) -> Maybe<&2, F.Int63.t>:
  match cert:
    case O.Certificate{group, k, _}: F.choose(Maybe<&2, F.Int63.t>, O.mentions(group, tm), Some{k}, None{})
def guarded(+budget: B.t, +g: G.t, +cert: O.t, bs: List<&2, body>) -> R.result(entries):
  match bs:
    case []: Done{[]}
    case Body{n, ty, tyv, +b} <> rest:
      R.bind(T.t, entries, O.translate(cert, b), +tm =>
        R.bind(Unit, entries, K.check_term(g, budget, tm, tyv), _ =>
          R.map_result(entries, entries, rs => F.Pair2{n, G.Def{G.Definition{ty, tm, True{}, rec_arg(cert, b), False{}}}} <> rs, guarded(budget, g, cert, rest))))
def admitted(budget: B.t, globals: G.t, provisional: G.t, bs: List<&2, body>, cert: Maybe<&2, O.t>) -> R.result(entries):
  match cert:
    case None{}: K.check_decls(globals, budget, declarations(bs))
    case Some{c}: guarded(budget, provisional, c, bs)
def in_provisional(+budget: B.t, +globals: G.t, ts: List<&2, typed>, +provisional: G.t) -> R.result(checked):
  R.bind(List<&2, body>, checked, bodies(budget, provisional, ts), +bs =>
    R.bind(Maybe<&2, O.t>, checked, Totality.guard_group(budget, provisional, members(bs)), cert =>
      R.map_result(entries, checked, +rows => Checked{install(rows, globals), rows}, admitted(budget, globals, provisional, bs, cert))))
def elab_rec_group(+budget: B.t, +globals: G.t, ms: List<&2, S.rec_def_of<S.t>>) -> R.result(checked):
  R.bind(List<&2, typed>, checked, types(budget, globals, ms), +ts =>
    R.bind(entries, checked, K.check_decls(globals, budget, postulates(ts)), ds =>
      in_provisional(budget, globals, ts, install(ds, globals))))
def ordinary(+budget: B.t, +g: G.t, d: S.decl) -> R.result(checked):
  R.bind(K.decl, checked, Elab.elab_decl(K.make(g, budget), d), row =>
    R.bind(entries, checked, K.check_decls(g, budget, [row]), rows =>
      O.maybe(F.Pair2<String, G.entry>, R.result(checked), R.one_of(F.Pair2<String, G.entry>, rows), _ => Fail{E.Cannot_infer{"the checker answered no entry for a declaration"}}, entry =>
        with_entry(R.result(checked), entry, +n => +e => Done{Checked{G.add(n, e, g), [F.Pair2{n, e}]}}))))
def declaration(budget: B.t, g: G.t, d: S.decl) -> R.result(checked):
  match d:
    case S.DMu{fams}: R.map_result(G.t, checked, next => Checked{next, []}, Elab.elab_mu_group(budget, g, fams))
    case S.DRec{ms}: elab_rec_group(budget, g, ms)
    case S.DDef{n, ty, b}: ordinary(budget, g, S.DDef{n, ty, b})
    case S.DAxiom{n, ty}: ordinary(budget, g, S.DAxiom{n, ty})
def program_loop(ds: List<&2, S.decl>, +budget: B.t, g: G.t) -> R.result(checked):
  match ds:
    case []: Done{Checked{g, []}}
    case d <> rest:
      R.bind(checked, checked, declaration(budget, g, d), state =>
        with_checked(R.result(checked), state, next => rows =>
          R.map_result(checked, checked, tail => with_checked(checked, tail, last => out => Checked{last, List.append(&2, F.Pair2<String, G.entry>, rows, out)}), program_loop(rest, budget, next))))
def elab_program_in(budget: B.t, g: G.t, ds: List<&2, S.decl>) -> R.result(checked): program_loop(ds, budget, g)
def entry_text(row: F.Pair2<String, G.entry>) -> String:
  match row:
    case F.Pair2{name, entry}:
      match entry:
        case G.Def{G.Definition{ty, body, _, _, _}}: S.cat(["def ", name, " : ", PP.term([], ty), " := ", PP.term([], body), "\n"])
        case G.Axiom{G.Postulate{ty}}: S.cat(["axiom ", name, " : ", PP.term([], ty), "\n"])
        case G.Prim{G.Primitive{ty, _}}: S.cat(["prim ", name, " : ", PP.term([], ty), "\n"])
def checked_form(rows: entries) -> String:
  match rows:
    case []: ""
    case row <> rest: String.append(entry_text(row), checked_form(rest))
def axiom_names(rows: entries) -> List<&2, String>:
  match rows:
    case []: []
    case F.Pair2{n, e} <> +rest:
      O.maybe(G.axiom_entry, List<&2, String>, G.axiom_of(e), _ => axiom_names(rest), _ => n <> axiom_names(rest))
