# Display-only kernel printer at Kanon 69f3be51 pp.ml.
import Base
import ./foundation.bend as F
import ./kernel_level.bend as L
import ./kernel_quantity.bend as Q
import ./kernel_literal.bend as Lit
import ./kernel_term.bend as T
import ./kernel_shape.bend as S

def cat(xs: List<&2, String>) -> String: String.concat(xs)
def decimal_byte(+c: U32) -> String:
  SCon{Chr{92}, SCon{Chr{(48 + c / 100 : U32)}, SCon{Chr{(48 + (c / 10) % 10 : U32)}, SCon{Chr{(48 + c % 10 : U32)}, SNil{}}}}}
def escape_ascii(+c: U32) -> String:
  F.choose(String, U32.is_eq(c, 34), "\\\"",
    F.choose(String, U32.is_eq(c, 92), "\\\\",
    F.choose(String, U32.is_eq(c, 10), "\\n",
    F.choose(String, U32.is_eq(c, 13), "\\r",
    F.choose(String, U32.is_eq(c, 9), "\\t",
    F.choose(String, U32.is_eq(c, 8), "\\b",
    F.choose(String, Bool.and(U32.is_ge(c, 32), U32.is_lt(c, 127)), SCon{Chr{c}, SNil{}}, decimal_byte(c))))))))
def escape_code(+c: U32) -> String:
  Q.if_else(String, U32.is_lt(c, 128), _ => escape_ascii(c), _ =>
    Q.if_else(String, U32.is_lt(c, 2048), _ => cat([decimal_byte((192 + c / 64 : U32)), decimal_byte((128 + c % 64 : U32))]), _ =>
    Q.if_else(String, U32.is_lt(c, 65536), _ => cat([decimal_byte((224 + c / 4096 : U32)), decimal_byte((128 + (c / 64) % 64 : U32)), decimal_byte((128 + c % 64 : U32))]), _ =>
      cat([decimal_byte((240 + c / 262144 : U32)), decimal_byte((128 + (c / 4096) % 64 : U32)), decimal_byte((128 + (c / 64) % 64 : U32)), decimal_byte((128 + c % 64 : U32))]))))
def escape(s: String) -> String:
  match s:
    case SNil{}: ""
    case SCon{Chr{c}, rest}: cat([escape_code(c), escape(rest)])
def literal(lit: Lit.t) -> String:
  match lit:
    case Lit.LInt{n}: F.Bignum.to_string(n)
    case Lit.LString{s}: cat(["\"", escape(s), "\""])
def push_loop(added: List<&2, String>, names: List<&2, String>) -> List<&2, String>:
  match added:
    case []: names
    case x <> rest: push_loop(rest, x <> names)
def push(names: List<&2, String>, added: List<&2, String>) -> List<&2, String>: push_loop(added, names)
def binder_names(bs: List<&2, F.Pair2<Q.t, String>>) -> List<&2, String>:
  match bs:
    case []: []
    case F.Pair2{_, x} <> rest: x <> binder_names(rest)
def under(s: S.t<T.t>, names: List<&2, String>) -> List<&2, String>:
  match s:
    case S.SPi{_, x, _}: x <> names
    case S.SColl{_}: names
    case S.SPar{_, _}: names
    case S.SMu{_, _}: names
    case S.SNu{_, _}: names
def nth_loop(names: List<&2, String>, +ix: F.Int63.t) -> Maybe<&2, String>:
  match names:
    case []: None{}
    case x <> rest: Q.if_else(Maybe<&2, String>, F.Int63.equal(ix, F.Int63.zero), _ => Some{x}, _ => nth_loop(rest, F.Int63.pred(ix)))
def nth_name(ix: F.Int63.t, names: List<&2, String>) -> Maybe<&2, String>: nth_loop(names, ix)
def lookup_name(original: F.Int63.t, name: Maybe<&2, String>) -> String:
  match name:
    case None{}: cat(["#", F.Int63.show(original)])
    case Some{x}: x
# Callbacks print strict subterms held by shapes, legs, and motives.
# Explicit tasks avoid recursive specialization of higher-order printers.
type piece is Data:
  Text{text: String}
  Term{names: List<&2, String>, tm: T.t}
  Shape{names: List<&2, String>, shape: S.t<T.t>}
  Addr{names: List<&2, String>, addr: T.addr}
  Leg{names: List<&2, String>, leg: T.leg}
  Elim{names: List<&2, String>, elim: T.elim}
  Motive{names: List<&2, String>, motive: Maybe<&2, T.motive>}
  Terms{first: Bool, names: List<&2, String>, terms: List<&2, T.t>}
  Legs{first: Bool, names: List<&2, String>, legs: List<&2, T.leg>}
  Branches{names: List<&2, String>, branches: List<&2, F.Pair2<T.addr, T.leg>>}
type expansion is Data:
  Expanded{text: String, children: List<&2, piece>}
def pieces(children: List<&2, piece>) -> expansion: Expanded{"", children}
def expand_term(+ns: List<&2, String>, tm: T.t) -> expansion:
  match tm:
    case T.Var{+ix}: Expanded{lookup_name(ix, nth_name(ix, ns)), []}
    case T.Univ{l}: Expanded{cat(["Type ", L.to_string(l)]), []}
    case T.Lan{+s, d}: pieces([Text{"(Lan "}, Shape{ns, s}, Text{" "}, Term{under(s, ns), d}, Text{")"}])
    case T.Ran{+s, d}: pieces([Text{"(Ran "}, Shape{ns, s}, Text{" "}, Term{under(s, ns), d}, Text{")"}])
    case T.In{s, a, args}: pieces([Text{"(In "}, Shape{ns, s}, Text{" "}, Addr{ns, a}, Text{" ["}, Terms{True{}, ns, args}, Text{"])"}])
    case T.Elim{e}: pieces([Elim{ns, e}])
    case T.Sec{s, ls}: pieces([Text{"(Sec "}, Shape{ns, s}, Text{" ["}, Legs{True{}, ns, ls}, Text{"])"}])
    case T.Out{s, a, scrut}: pieces([Text{"(Out "}, Shape{ns, s}, Text{" "}, Addr{ns, a}, Text{" "}, Term{ns, scrut}, Text{")"}])
    case T.Let{+x, ty, d, body}: pieces([Text{cat(["let ", x, " : "])}, Term{ns, ty}, Text{" := "}, Term{ns, d}, Text{" in "}, Term{x <> ns, body}])
    case T.Ann{t, ty}: pieces([Text{"("}, Term{ns, t}, Text{" : "}, Term{ns, ty}, Text{")"}])
    case T.Global{n}: Expanded{n, []}
    case T.Lit{l}: Expanded{literal(l), []}
    case T.Auto{}: Expanded{"auto", []}
def expand_shape(+ns: List<&2, String>, s: S.t<T.t>) -> expansion:
  match s:
    case S.SPi{q, x, dom}: pieces([Text{cat(["SPi ", Q.to_string(q), " ", x, " "])}, Term{ns, dom}])
    case S.SColl{n}: Expanded{cat(["SColl ", F.Int63.show(n)]), []}
    case S.SPar{a, b}: pieces([Text{"SPar "}, Term{ns, a}, Text{" "}, Term{ns, b}])
    case S.SMu{n, ts}: pieces([Text{cat(["SMu ", n, " ["])}, Terms{True{}, ns, ts}, Text{"]"}])
    case S.SNu{n, ts}: pieces([Text{cat(["SNu ", n, " ["])}, Terms{True{}, ns, ts}, Text{"]"}])
def expand_leg(ns: List<&2, String>, +bs: List<&2, String>, body: T.t) -> expansion:
  pieces([Text{cat([String.join(bs, " "), " => "])}, Term{push(ns, bs), body}])
def expand(item: piece) -> expansion:
  match item:
    case Text{text}: Expanded{text, []}
    case Term{ns, tm}: expand_term(ns, tm)
    case Shape{ns, s}: expand_shape(ns, s)
    case Addr{ns, a}:
      match a:
        case T.APt{q, arg}: pieces([Text{cat(["(APt ", Q.to_string(q), " "])}, Term{ns, arg}, Text{")"}])
        case T.ALeg{k}: Expanded{cat(["(ALeg ", F.Int63.show(k), ")"]), []}
        case T.ACtor{c}: Expanded{cat(["(ACtor ", c, ")"]), []}
    case Leg{ns, T.Leg{bs, body}}: expand_leg(ns, binder_names(bs), body)
    case Elim{+ns, T.Elimination{s, scrut, _, mo, bs}}: pieces([Text{"(Elim "}, Shape{ns, s}, Text{" "}, Term{ns, scrut}, Motive{ns, mo}, Text{" with"}, Branches{ns, bs}, Text{")"}])
    case Motive{ns, mo}:
      match mo:
        case None{}: pieces([])
        case Some{T.Motive{_, idx, +self, body}}: pieces([Text{cat([" as ", self, " return "])}, Term{self <> push(ns, idx), body}])
    case Terms{first, +ns, ts}:
      match ts:
        case []: pieces([])
        case t <> rest: pieces([Text{F.choose(String, first, "", "; ")}, Term{ns, t}, Terms{False{}, ns, rest}])
    case Legs{first, +ns, ls}:
      match ls:
        case []: pieces([])
        case l <> rest: pieces([Text{F.choose(String, first, "", "; ")}, Leg{ns, l}, Legs{False{}, ns, rest}])
    case Branches{+ns, bs}:
      match bs:
        case []: pieces([])
        case F.Pair2{a, lg} <> rest: pieces([Text{" | "}, Addr{ns, a}, Text{" "}, Leg{ns, lg}, Branches{ns, rest}])
def append(xs: List<&2, piece>, tail: List<&2, piece>) -> List<&2, piece>:
  match xs:
    case []: tail
    case x <> rest: x <> append(rest, tail)
def reverse(xs: List<&2, String>, tail: List<&2, String>) -> List<&2, String>:
  match xs:
    case []: tail
    case x <> rest: reverse(rest, x <> tail)
def apply_expansion(value: expansion, next: String -> List<&2, piece> -> String) -> String:
  match value:
    case Expanded{text, children}: next(text, children)
# Expansion visits each finite syntax node and each list element once.
@unsafe
def render(todo: List<&2, piece>, acc: List<&2, String>) -> String:
  match todo:
    case []: String.concat(reverse(acc, []))
    case item <> rest: apply_expansion(expand(item), text => children => render(append(children, rest), text <> acc))
def term(ns: List<&2, String>, tm: T.t) -> String: render([Term{ns, tm}], [])
def shape(ns: List<&2, String>, s: S.t<T.t>) -> String: render([Shape{ns, s}], [])
def addr(ns: List<&2, String>, a: T.addr) -> String: render([Addr{ns, a}], [])
def leg(ns: List<&2, String>, l: T.leg) -> String: render([Leg{ns, l}], [])
def elim(ns: List<&2, String>, e: T.elim) -> String: render([Elim{ns, e}], [])
