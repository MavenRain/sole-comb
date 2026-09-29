# Erased representation and printer from Kanon 69f3be51 lib/eterm.ml.
# Symbolic names remain distinct types; all host indices use signed Int63.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_literal.bend as Lit
import ../lib/kernel_quantity.bend as Q

type tid is Data:
  Tid{name: String}
type fid is Data:
  Fid{name: String}
type repr is Data:
  RI31{}
  RStruct{name: tid}
  RUnion{name: tid}
  RFunc{name: tid}
  RThunk{name: tid}
type BranchData<-A: Data> is Data:
  Branch{tag: F.Int63.t, arity: F.Int63.t, body: A}
type ktm is Data:
  KVar{index: F.Int63.t}
  KLit{literal: Lit.t}
  KGlobal{name: String}
  KErased{}
  KLet{name: String, value: ktm, body: ktm}
  KClos{name: fid, arity: F.Int63.t, captures: List<&2, ktm>}
  KApp{head: ktm, args: List<&2, ktm>}
  KTail{head: ktm, args: List<&2, ktm>}
  KStruct{name: tid, fields: List<&2, ktm>}
  KProj{name: tid, field: F.Int63.t, value: ktm}
  KTag{name: tid, tag: F.Int63.t, fields: List<&2, ktm>}
  KCase{name: tid, scrutinee: ktm, branches: List<&2, BranchData<ktm>>}
  KDelay{name: fid, captures: List<&2, ktm>}
  KForce{value: ktm}
def kbranch() -> Data: BranchData<ktm>
type kdecl is Data:
  KFun{name: fid, params: List<&2, repr>, result: repr, body: ktm}
  KRec{names: List<&2, tid>}

def tid_text(t: tid) -> String:
  match t:
    case Tid{name}: name
def fid_text(f: fid) -> String:
  match f:
    case Fid{name}: name

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
# The literal printer uses OCaml byte escaping, independently of kernel syntax.
def literal_text(lit: Lit.t) -> String:
  match lit:
    case Lit.LInt{n}: F.Bignum.to_string(n)
    case Lit.LString{s}: cat(["\"", escape(s), "\""])

def join_more(+sep: String, xs: List<&2, String>) -> String:
  match xs:
    case []: ""
    case h <> t: cat([sep, h, join_more(sep, t)])
def join(+sep: String, xs: List<&2, String>) -> String:
  match xs:
    case []: ""
    case h <> t: String.append(h, join_more(sep, t))
def print_repr(r: repr) -> String:
  match r:
    case RI31{}: "i31"
    case RStruct{t}: String.append("struct ", tid_text(t))
    case RUnion{t}: String.append("union ", tid_text(t))
    case RFunc{t}: String.append("func ", tid_text(t))
    case RThunk{t}: String.append("thunk ", tid_text(t))
def repr_texts(rs: List<&2, repr>) -> List<&2, String>:
  match rs:
    case []: []
    case r <> rs: print_repr(r) <> repr_texts(rs)
def tid_texts(ts: List<&2, tid>) -> List<&2, String>:
  match ts:
    case []: []
    case t <> ts: tid_text(t) <> tid_texts(ts)

type piece is Data:
  Text{text: String}
  Term{term: ktm}
  Terms{terms: List<&2, ktm>}
  MoreTerms{terms: List<&2, ktm>}
  BranchPiece{branch: kbranch}
  Branches{branches: List<&2, kbranch>}
  MoreBranches{branches: List<&2, kbranch>}
type expansion is Data:
  Expanded{text: String, children: List<&2, piece>}
def expand_term(t: ktm) -> expansion:
  match t:
    case KVar{i}: Expanded{String.append("KVar ", F.Int63.show(i)), []}
    case KLit{l}: Expanded{String.append("KLit ", literal_text(l)), []}
    case KGlobal{n}: Expanded{String.append("KGlobal ", n), []}
    case KErased{}: Expanded{"KErased", []}
    case KLet{x, v, b}: Expanded{cat(["KLet ", x, " ("]), [Term{v}, Text{") ("}, Term{b}, Text{")"}]}
    case KClos{f, n, cs}: Expanded{cat(["KClos ", fid_text(f), " ", F.Int63.show(n), " ["]), [Terms{cs}, Text{"]"}]}
    case KApp{h, args}: Expanded{"KApp (", [Term{h}, Text{") ["}, Terms{args}, Text{"]"}]}
    case KTail{h, args}: Expanded{"KTail (", [Term{h}, Text{") ["}, Terms{args}, Text{"]"}]}
    case KStruct{t, fs}: Expanded{cat(["KStruct ", tid_text(t), " ["]), [Terms{fs}, Text{"]"}]}
    case KProj{t, k, x}: Expanded{cat(["KProj ", tid_text(t), " ", F.Int63.show(k), " ("]), [Term{x}, Text{")"}]}
    case KTag{t, k, ps}: Expanded{cat(["KTag ", tid_text(t), " ", F.Int63.show(k), " ["]), [Terms{ps}, Text{"]"}]}
    case KCase{t, s, bs}: Expanded{cat(["KCase ", tid_text(t), " ("]), [Term{s}, Text{") ["}, Branches{bs}, Text{"]"}]}
    case KDelay{f, cs}: Expanded{cat(["KDelay ", fid_text(f), " ["]), [Terms{cs}, Text{"]"}]}
    case KForce{x}: Expanded{"KForce (", [Term{x}, Text{")"}]}
def expand(item: piece) -> expansion:
  match item:
    case Text{s}: Expanded{s, []}
    case Term{t}: expand_term(t)
    case Terms{[]}: Expanded{"", []}
    case Terms{t <> ts}: Expanded{"", [Term{t}, MoreTerms{ts}]}
    case MoreTerms{[]}: Expanded{"", []}
    case MoreTerms{t <> ts}: Expanded{"; ", [Term{t}, MoreTerms{ts}]}
    case BranchPiece{Branch{k, n, b}}: Expanded{cat(["{", F.Int63.show(k), " ", F.Int63.show(n), " ("]), [Term{b}, Text{")}"}]}
    case Branches{[]}: Expanded{"", []}
    case Branches{b <> bs}: Expanded{"", [BranchPiece{b}, MoreBranches{bs}]}
    case MoreBranches{[]}: Expanded{"", []}
    case MoreBranches{b <> bs}: Expanded{"; ", [BranchPiece{b}, MoreBranches{bs}]}
def append(xs: List<&2, piece>, ys: List<&2, piece>) -> List<&2, piece>:
  match xs:
    case []: ys
    case x <> xs: x <> append(xs, ys)
def reverse(xs: List<&2, String>, ys: List<&2, String>) -> List<&2, String>:
  match xs:
    case []: ys
    case x <> xs: reverse(xs, x <> ys)
def expanded(value: expansion, rest: List<&2, piece>, acc: List<&2, String>, next: List<&2, piece> -> List<&2, String> -> String) -> String:
  match value:
    case Expanded{text, children}: next(append(children, rest), text <> acc)
@unsafe
def render(todo: List<&2, piece>, acc: List<&2, String>) -> String:
  match todo:
    case []: String.concat(reverse(acc, []))
    case item <> rest: expanded(expand(item), rest, acc, ts => out => render(ts, out))
def print_ktm(t: ktm) -> String: render([Term{t}], [])
def ktm_list(ts: List<&2, ktm>) -> String: cat(["[", render([Terms{ts}], []), "]"])
def print_branch(b: kbranch) -> String: render([BranchPiece{b}], [])
def print_decl(d: kdecl) -> String:
  match d:
    case KFun{f, ps, r, b}: cat(["fun ", fid_text(f), " (", join(", ", repr_texts(ps)), ") : ", print_repr(r), " := ", print_ktm(b)])
    case KRec{ts}: cat(["rec [", join("; ", tid_texts(ts)), "]"])
