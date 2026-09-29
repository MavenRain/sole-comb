# Observations are test data, separate from the canonical syntax printer.
import Base
import ../../lib/foundation.bend as F
import ../../lib/kernel_quantity.bend as Q
import ../../lib/kernel_error.bend as E
import token.bend as T
import lexer.bend as L
import syntax.bend as S
import parser.bend as P

def nat(n: Nat) -> String: F.Bignum.to_string(F.Bignum.from_nat(n))
def string(+s: String) -> String: S.cat([nat(String.length(s)), ":", s])
def quantity(q: Q.t) -> String:
  match q:
    case Q.Zero{}: "0"
    case Q.One{}: "1"
    case Q.Many{}: "many"
type job is Data:
  Text{value: String}
  Atom{value: String}
  Node{tag: String, fields: List<&2, job>}
  Seq{items: List<&2, job>}
  Term{value: S.t}
  Binder{value: S.binder}
  Field{value: S.field}
  Motive{value: S.motive}
  Branch{value: S.branch}
  Family{value: S.fam}
  Constructor{value: S.fam_ctor}
  Recursive{value: S.rec_def}
  Decl{value: S.decl}
  MaybeTerm{value: Maybe<&2, S.t>}
  MaybeText{value: Maybe<&2, String>}
  MaybeMotive{value: Maybe<&2, S.motive>}
def jobs(~A: Data, ~pack: A -> job, xs: List<&2, A>) -> List<&2, job>:
  match xs:
    case []: []
    case x <> rest: pack(x) <> jobs(~A, ~pack, rest)
@unsafe
def tree(value: job) -> String:
  match value:
    case Text{s}: string(s)
    case Atom{s}: s
    case Node{tag, fields}: S.cat(["(", tag, " ", tree(Seq{fields}), ")"])
    case Seq{[]}: ""
    case Seq{x <> rest}: S.cat([tree(x), ";", tree(Seq{rest})])
    case MaybeTerm{None{}}: tree(Node{"None", []})
    case MaybeTerm{Some{t}}: tree(Node{"Some", [Term{t}]})
    case MaybeText{None{}}: tree(Node{"None", []})
    case MaybeText{Some{t}}: tree(Node{"Some", [Text{t}]})
    case MaybeMotive{None{}}: tree(Node{"None", []})
    case MaybeMotive{Some{t}}: tree(Node{"Some", [Motive{t}]})
    case Binder{S.Binder{q, n, t}}: tree(Node{"Binder", [Atom{quantity(q)}, Text{n}, Term{t}]})
    case Field{S.Field{q, n, t}}: tree(Node{"Field", [Atom{quantity(q)}, Text{n}, MaybeTerm{t}]})
    case Motive{S.Motive{self, ind, idx, body}}: tree(Node{"Motive", [Text{self}, MaybeText{ind}, Node{"List", jobs(~String, ~(x => Text{x}), idx)}, Term{body}]})
    case Branch{S.BrLeg{k, bs, body}}: tree(Node{"BrLeg", [Atom{F.Int63.show(k)}, Node{"List", jobs(~S.binder, ~(b => Binder{b}), bs)}, Term{body}]})
    case Branch{S.BrCtor{c, fs, body}}: tree(Node{"BrCtor", [Text{c}, Node{"List", jobs(~S.field, ~(f => Field{f}), fs)}, Term{body}]})
    case Family{S.Fam{name, params, ty, ctors}}: tree(Node{"Fam", [Text{name}, Node{"List", jobs(~S.binder, ~(b => Binder{b}), params)}, Term{ty}, Node{"List", jobs(~S.fam_ctor, ~(c => Constructor{c}), ctors)}]})
    case Constructor{S.FamCtor{name, ty}}: tree(Node{"Ctor", [Text{name}, Term{ty}]})
    case Recursive{S.RecDef{name, ty, body}}: tree(Node{"Rec", [Text{name}, Term{ty}, Term{body}]})
    case Decl{S.DDef{name, ty, body}}: tree(Node{"DDef", [Text{name}, Term{ty}, Term{body}]})
    case Decl{S.DAxiom{name, ty}}: tree(Node{"DAxiom", [Text{name}, Term{ty}]})
    case Decl{S.DMu{fs}}: tree(Node{"DMu", jobs(~S.fam, ~(f => Family{f}), fs)})
    case Decl{S.DRec{ms}}: tree(Node{"DRec", jobs(~S.rec_def, ~(m => Recursive{m}), ms)})
    case Term{S.SVar{x}}: tree(Node{"SVar", [Text{x}]})
    case Term{S.SNat{n}}: tree(Node{"SNat", [Atom{F.Bignum.to_string(n)}]})
    case Term{S.SProp{}}: tree(Node{"SProp", []})
    case Term{S.SType{n}}: tree(Node{"SType", [Atom{F.Int63.show(n)}]})
    case Term{S.SPrim{p}}: tree(Node{"SPrim", [Text{S.prim_name(p)}]})
    case Term{S.SUnit{}}: tree(Node{"SUnit", []})
    case Term{S.SAuto{}}: tree(Node{"SAuto", []})
    case Term{S.SPair{a, b}}: tree(Node{"SPair", [Term{a}, Term{b}]})
    case Term{S.STuple{xs}}: tree(Node{"STuple", jobs(~S.t, ~(x => Term{x}), xs)})
    case Term{S.SSum{xs}}: tree(Node{"SSum", jobs(~S.t, ~(x => Term{x}), xs)})
    case Term{S.SProd{xs}}: tree(Node{"SProd", jobs(~S.t, ~(x => Term{x}), xs)})
    case Term{S.SProj{a, k}}: tree(Node{"SProj", [Term{a}, Atom{F.Int63.show(k)}]})
    case Term{S.SInj{k, n, a}}: tree(Node{"SInj", [Atom{F.Int63.show(k)}, Atom{F.Int63.show(n)}, Term{a}]})
    case Term{S.SAbsurd{a}}: tree(Node{"SAbsurd", [Term{a}]})
    case Term{S.SApp{f, a}}: tree(Node{"SApp", [Term{f}, Term{a}]})
    case Term{S.SFun{bs, body}}: tree(Node{"SFun", [Node{"List", jobs(~S.binder, ~(b => Binder{b}), bs)}, Term{body}]})
    case Term{S.SArrow{b, body}}: tree(Node{"SArrow", [Binder{b}, Term{body}]})
    case Term{S.SStar{b, body}}: tree(Node{"SStar", [Binder{b}, Term{body}]})
    case Term{S.SLet{name, ty, value, body}}: tree(Node{"SLet", [Text{name}, Term{ty}, Term{value}, Term{body}]})
    case Term{S.SAnn{a, ty}}: tree(Node{"SAnn", [Term{a}, Term{ty}]})
    case Term{S.SCase{scrut, mo, bs}}: tree(Node{"SCase", [Term{scrut}, MaybeMotive{mo}, Node{"List", jobs(~S.branch, ~(b => Branch{b}), bs)}]})
    case Term{S.SMatch{scrut, mo, bs}}: tree(Node{"SMatch", [Term{scrut}, MaybeMotive{mo}, Node{"List", jobs(~S.branch, ~(b => Branch{b}), bs)}]})

def ast(ds: List<&2, S.decl>) -> String: tree(Seq{jobs(~S.decl, ~(d => Decl{d}), ds)})
def byte_text(b: U32) -> String: F.Bignum.to_string(F.Bignum.from_u32(b))
def token_kind(+k: T.kind) -> String:
  match k:
    case T.Bytes{bs}: S.cat(["byte literal[", S.texts(~U32, ~byte_text, ",", bs), "]"])
    case _: T.describe(k)
def token(t: T.t) -> String:
  match t:
    case T.Token{kind, T.Loc{line, col}}: S.cat([nat(line), ":", nat(col), ":", string(token_kind(kind)), ";"])
def same_text(left: String, right: String) -> Bool:
  match left:
    case SNil{}:
      match right:
        case SNil{}: True{}
        case SCon{_, _}: False{}
    case SCon{Chr{a}, tail}:
      match right:
        case SNil{}: False{}
        case SCon{Chr{b}, rest}: L.choose(Bool, U32.is_eq(a, b), _ => same_text(tail, rest), _ => False{})
def roundtrip(expected: String, value: L.result(List<&2, S.decl>)) -> String:
  match value:
    case Fail{e}: String.append("error:", E.to_string(e))
    case Done{ds}: F.choose(String, same_text(expected, ast(ds)), "true", "false")
def parsed_observation(lexed: String, value: L.result(List<&2, S.decl>)) -> String:
  match value:
    case Fail{e}: S.cat(["LEX\n", lexed, "\nPARSE-ERROR\n", E.to_string(e)])
    case Done{+ds}: S.cat(["LEX\n", lexed, "\nAST\n", ast(ds), "\nPRINT\n", S.print(ds), "\nROUNDTRIP\n", roundtrip(ast(ds), P.parse(S.print(ds)))])
def lexed_observation(value: L.result(List<&2, T.t>)) -> String:
  match value:
    case Fail{e}: String.append("LEX-ERROR\n", E.to_string(e))
    case Done{+ts}: parsed_observation(S.texts(~T.t, ~token, "", ts), P.parse_tokens(ts))
def observe_bytes(bytes: List<&2, U32>) -> String: lexed_observation(L.lex_bytes(bytes))
def observe(source: String) -> String: lexed_observation(L.lex(source))
