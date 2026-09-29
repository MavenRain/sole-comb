# Test-only declaration observations. Recursive function groups are not driven.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_error.bend as E
import ../lib/kernel_global.bend as G
import ../lib/kernel_check.bend as K
import ../lib/kernel_rules.bend as R
import ../lib/kernel_term.bend as T
import ../lib/kernel_pp.bend as PP
import pinfront/syntax.bend as S
import pinfront/token.bend as Tok
import pinfront/lexer.bend as L
import pinfront/parser.bend as P
import pinfront/elab.bend as Elab

def nat(n: Nat) -> String: F.Bignum.to_string(F.Bignum.from_nat(n))
def budget() -> B.t: B.create(16384n)
def body_text(body: Maybe<&2, T.t>) -> String:
  match body:
    case None{}: ""
    case Some{tm}: String.append(" = ", PP.term([], tm))
def kind_text(kind: K.kind) -> String:
  match kind:
    case K.Definition{}: "def "
    case K.Postulate{}: "axiom "
def declaration_text(d: K.decl) -> String:
  match d:
    case K.Declaration{name, kind, ty, body}: S.cat([kind_text(kind), name, " : ", PP.term([], ty), body_text(body), ";\n"])
def family_text(fams: List<&2, S.fam>) -> String:
  match fams:
    case []: ""
    case fm <> rest: S.cat(["mu ", Elab.fam_name(fm), ";\n", family_text(rest)])
def admit(+g: G.t, d: S.decl) -> R.result(F.Pair2<G.t, String>):
  match d:
    case S.DMu{+families}:
      R.map_result(G.t, F.Pair2<G.t, String>, next => F.Pair2{next, family_text(families)}, Elab.elab_mu_group(budget, g, families))
    case other:
      R.bind(K.decl, F.Pair2<G.t, String>, Elab.elab_decl(K.make(g, budget), other), +decl =>
        R.map_result(G.entry, F.Pair2<G.t, String>, entry => F.Pair2{G.add(K.declaration_name(decl), entry, g), declaration_text(decl)}, K.check_decl(g, budget, decl)))
def with_row(-A: Type, row: F.Pair2<G.t, String>, next: G.t -> String -> A) -> A:
  match row:
    case F.Pair2{g, text}: next(g, text)
def declarations(ds: List<&2, S.decl>, g: G.t) -> R.result(String):
  match ds:
    case []: Done{""}
    case d <> rest:
      R.bind(F.Pair2<G.t, String>, String, admit(g, d), row => with_row(R.result(String), row, next => text =>
        R.map_result(String, String, tail => String.append(text, tail), declarations(rest, next))))
def result_text(value: R.result(String)) -> String:
  match value:
    case Fail{error}: String.append("error:", E.to_string(error))
    case Done{text}: String.append("ok:", text)
def parsed(value: R.result(List<&2, S.decl>)) -> String:
  result_text(R.bind(List<&2, S.decl>, String, value, ds => declarations(ds, G.initial)))
def observe(source: String) -> String: parsed(P.parse(source))
def observe_bytes(bytes: List<&2, U32>) -> String:
  parsed(R.bind(List<&2, Tok.t>, List<&2, S.decl>, L.lex_bytes(bytes), ts => P.parse_tokens(ts)))
