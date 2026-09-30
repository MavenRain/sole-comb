# Parser semantics from Kanon 69f3be51 surface/parser.ml.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_error.bend as E
import token.bend as T
import source.bend as Source
import lexer.bend as L
import syntax.bend as S

def tokens() -> Data: List<&2, T.t>
def parsed(-A: Data) -> Data: L.result(F.Pair2<A, tokens>)
def done(-A: Data, value: A, rest: tokens) -> parsed(A): Done{F.Pair2{value, rest}}
def next(-A: Data, -B: Data, value: parsed(A), k: A -> tokens -> parsed(B)) -> parsed(B):
  match value:
    case Fail{e}: Fail{e}
    case Done{F.Pair2{x, rest}}: k(x, rest)
def want(-A: Data, what: String, ts: tokens) -> parsed(A):
  match ts:
    case []: L.error(F.Pair2<A, tokens>, T.start, "unexpected end of input")
    case T.Token{kind, loc} <> _: L.error(F.Pair2<A, tokens>, loc, S.cat(["expected ", what, ", found ", T.describe(kind)]))
def bounded(+loc: T.loc, +n: F.Bignum.t) -> L.result(F.Int63.t):
  L.choose(L.result(F.Int63.t), Bool.or(Bool.not(F.Order.le(F.Bignum.compare(F.Bignum.zero, n))), (String.length(F.Bignum.to_string(n)) > 18n : Nat)),
    _ => L.error(F.Int63.t, loc, "numeric literal too long"),
    _ => L.maybe(F.Int63.t, L.result(F.Int63.t), F.Bignum.to_int(n),
      _ => L.error(F.Int63.t, loc, "numeric literal too long"), value => Done{value}))
def mark_prefix(+ts: tokens) -> F.Pair2<Q.t, tokens>:
  match ts:
    case T.Token{T.Number{+n}, _} <> +rest:
      F.choose(F.Pair2<Q.t, tokens>, F.Bignum.equal(n, F.Bignum.zero), F.Pair2{Q.Zero{}, rest},
        F.choose(F.Pair2<Q.t, tokens>, F.Bignum.equal(n, F.Bignum.one), F.Pair2{Q.One{}, rest}, F.Pair2{Q.Many{}, ts}))
    case _: F.Pair2{Q.Many{}, ts}
def mark_apply(-A: Data, value: F.Pair2<Q.t, tokens>, k: Q.t -> tokens -> parsed(A)) -> parsed(A):
  match value:
    case F.Pair2{q, rest}: k(q, rest)
def mark_then(-A: Data, ts: tokens, k: Q.t -> tokens -> parsed(A)) -> parsed(A):
  mark_apply(A, mark_prefix(ts), k)
def bytes_term(bytes: List<&2, U32>) -> S.t:
  match bytes:
    case []: S.SVar{"bytesNil"}
    case b <> rest: S.SApp{S.SApp{S.SVar{"bytesCons"}, S.SNat{F.Bignum.from_u32(b)}}, bytes_term(rest)}
def arrows(bs: List<&2, S.binder>, body: S.t) -> S.t:
  match bs:
    case []: body
    case b <> rest: S.SArrow{b, arrows(rest, body)}
type elimination is Data:
  LegacyCase{}
  FiberedMatch{}
def elimination_node(form: elimination, scrut: S.t, mo: Maybe<&2, S.motive>, brs: List<&2, S.branch>) -> S.t:
  match form:
    case LegacyCase{}: S.SCase{scrut, mo, brs}
    case FiberedMatch{}: S.SMatch{scrut, mo, brs}
def numeric_key(form: elimination, loc: T.loc) -> L.result(Unit):
  match form:
    case LegacyCase{}: Done{Unit{}}
    case FiberedMatch{}: L.error(Unit, loc, "a match branch keys a constructor, not a leg number")
type index_clause is Data:
  IndexClause{ind: Maybe<&2, String>, idx: List<&2, String>}

def kind_starts_atom(k: T.kind) -> Bool:
  match k:
    case T.LBrace{}: False{}
    case T.RBrace{}: False{}
    case T.Semicolon{}: False{}
    case T.KElim{}: False{}
    case T.LParen{}: True{}
    case T.RParen{}: False{}
    case T.Colon{}: False{}
    case T.ColonEq{}: False{}
    case T.Arrow{}: False{}
    case T.DArrow{}: False{}
    case T.Star{}: False{}
    case T.Comma{}: False{}
    case T.Dot{}: False{}
    case T.Dot1{}: False{}
    case T.Dot2{}: False{}
    case T.Pipe{}: False{}
    case T.Unit{}: True{}
    case T.KDef{}: False{}
    case T.KAxiom{}: False{}
    case T.KFun{}: False{}
    case T.KInj{}: False{}
    case T.KOf{}: False{}
    case T.KCase{}: False{}
    case T.KMatch{}: False{}
    case T.KAs{}: False{}
    case T.KReturn{}: False{}
    case T.KWith{}: False{}
    case T.KTuple{}: True{}
    case T.KSum{}: True{}
    case T.KProd{}: True{}
    case T.KAbsurd{}: False{}
    case T.KProp{}: True{}
    case T.KType{}: True{}
    case T.KLet{}: False{}
    case T.KIn{}: False{}
    case T.KAuto{}: True{}
    case T.KMu{}: False{}
    case T.KMutual{}: False{}
    case T.KEnd{}: False{}
    case T.KNu{}: True{}
    case T.KAnd{}: False{}
    case T.KRec{}: False{}
    case T.KNatAdd{}: True{}
    case T.KNatSub{}: True{}
    case T.KNatMul{}: True{}
    case T.KNatEq{}: True{}
    case T.KNatLt{}: True{}
    case T.Ident{_}: True{}
    case T.Number{_}: True{}
    case T.Bytes{_}: True{}
    case T.Eof{}: False{}
def starts_atom(ts: tokens) -> Bool:
  match ts:
    case []: False{}
    case T.Token{kind, _} <> _: kind_starts_atom(kind)
def collection(+word: String, +items: List<&2, S.t>) -> S.t:
  F.choose(S.t, String.eq(word, "tuple"), S.STuple{items},
    F.choose(S.t, String.eq(word, "sum"), S.SSum{items}, S.SProd{items}))

# One typed task per parsing operation replaces OCaml mutual recursion.
type task is Data:
  ParseTerm{ts: tokens}
  ParseFunTerm{ts: tokens}
  ParseFunBody{bs: List<&2, S.binder>, ts: tokens}
  ParseBinders{ts: tokens, acc: List<&2, S.binder>}
  ParseBinder{ts: tokens}
  ParseBinderHead{q: Q.t, ts: tokens}
  ParseBinderClose{b: S.binder, ts: tokens}
  ParseLetTerm{ts: tokens}
  ParseLetValue{name: String, ty: S.t, ts: tokens}
  ParseLetBody{name: String, ty: S.t, value: S.t, ts: tokens}
  ParseCaseTerm{form: elimination, ts: tokens}
  ParseCaseMotive{form: elimination, scrut: S.t, ts: tokens}
  ParseIndex{ts: tokens}
  ParseNames{ts: tokens, acc: List<&2, String>}
  ParseMotiveReturn{form: elimination, scrut: S.t, name: String, clause: index_clause, ts: tokens}
  ParseMotiveWith{form: elimination, scrut: S.t, name: String, clause: index_clause, body: S.t, ts: tokens}
  ParseBranches{form: elimination, ts: tokens, acc: List<&2, S.branch>}
  ParseNativeArmClose{form: elimination, body: S.t, ts: tokens, acc: List<&2, S.branch>}
  ParseLegBody{form: elimination, key: F.Int63.t, bs: List<&2, S.binder>, ts: tokens, acc: List<&2, S.branch>}
  ParseCtorBody{form: elimination, name: String, fs: List<&2, S.field>, ts: tokens, acc: List<&2, S.branch>}
  ParseFields{ts: tokens, acc: List<&2, S.field>}
  ParseTypedField{b: S.binder, ts: tokens, acc: List<&2, S.field>}
  ParseUntypedField{q: Q.t, ts: tokens, original: tokens, acc: List<&2, S.field>}
  ParseArrow{ts: tokens}
  ParseAttemptArrow{original: tokens, value: parsed(S.binder)}
  ParseGroupOp{b: S.binder, ts: tokens}
  ParseArrowPlain{ts: tokens}
  ParseArrowTail{lhs: S.t, ts: tokens}
  ParseApp{ts: tokens}
  ParseInj{ts: tokens}
  ParseAppRest{head: S.t, ts: tokens}
  ParseAtom{ts: tokens}
  ParsePostfix{a: S.t, ts: tokens}
  ParseAtomHead{ts: tokens}
  ParseParen{ts: tokens}
  ParseParenTail{inner: S.t, ts: tokens}
  ParseCloseTerm{value: S.t, ts: tokens}
  ParseItems{word: String, ts: tokens}
  ParseItemsBody{word: String, ts: tokens}
  ParseItemsRest{word: String, ts: tokens, acc: List<&2, S.t>}
  ParseItemsTail{word: String, ts: tokens, acc: List<&2, S.t>}
  ParseDecls{ts: tokens, acc: List<&2, S.decl>}
  ParseDecl{ts: tokens}
  ParseDefBody{name: String, ty: S.t, ts: tokens}
  ParseRecGroup{ts: tokens, acc: List<&2, S.rec_def>}
  ParseRecTail{ts: tokens, acc: List<&2, S.rec_def>}
  ParseRecMember{ts: tokens}
  ParseRecBody{name: String, ty: S.t, ts: tokens}
  ParseFamGroup{ts: tokens, acc: List<&2, S.fam>}
  ParseFamTail{ts: tokens, acc: List<&2, S.fam>}
  ParseMutualGroup{ts: tokens, acc: List<&2, S.fam>}
  ParseFam{ts: tokens}
  ParseFamType{name: String, params: List<&2, S.binder>, ts: tokens}
  ParseFamConstructors{name: String, params: List<&2, S.binder>, ty: S.t, ts: tokens}
  ParseFamCtors{ts: tokens, acc: List<&2, S.fam_ctor>}
  ParseFamCtorType{name: String, bs: List<&2, S.binder>, ts: tokens, acc: List<&2, S.fam_ctor>}
type answer is Data:
  Answer0{value: parsed(S.t)}
  Answer1{value: parsed(List<&2, S.binder>)}
  Answer2{value: parsed(S.binder)}
  Answer3{value: parsed(index_clause)}
  Answer4{value: parsed(List<&2, String>)}
  Answer5{value: parsed(List<&2, S.branch>)}
  Answer6{value: parsed(List<&2, S.field>)}
  Answer7{value: parsed(List<&2, S.decl>)}
  Answer8{value: parsed(S.decl)}
  Answer9{value: parsed(List<&2, S.rec_def>)}
  Answer10{value: parsed(S.rec_def)}
  Answer11{value: parsed(List<&2, S.fam>)}
  Answer12{value: parsed(S.fam)}
  Answer13{value: parsed(List<&2, S.fam_ctor>)}

def read_0(value: answer) -> parsed(S.t):
  match value:
    case Answer0{value}: value
    case Answer1{_}: L.error(F.Pair2<S.t, tokens>, T.start, "internal parser result kind")
    case Answer2{_}: L.error(F.Pair2<S.t, tokens>, T.start, "internal parser result kind")
    case Answer3{_}: L.error(F.Pair2<S.t, tokens>, T.start, "internal parser result kind")
    case Answer4{_}: L.error(F.Pair2<S.t, tokens>, T.start, "internal parser result kind")
    case Answer5{_}: L.error(F.Pair2<S.t, tokens>, T.start, "internal parser result kind")
    case Answer6{_}: L.error(F.Pair2<S.t, tokens>, T.start, "internal parser result kind")
    case Answer7{_}: L.error(F.Pair2<S.t, tokens>, T.start, "internal parser result kind")
    case Answer8{_}: L.error(F.Pair2<S.t, tokens>, T.start, "internal parser result kind")
    case Answer9{_}: L.error(F.Pair2<S.t, tokens>, T.start, "internal parser result kind")
    case Answer10{_}: L.error(F.Pair2<S.t, tokens>, T.start, "internal parser result kind")
    case Answer11{_}: L.error(F.Pair2<S.t, tokens>, T.start, "internal parser result kind")
    case Answer12{_}: L.error(F.Pair2<S.t, tokens>, T.start, "internal parser result kind")
    case Answer13{_}: L.error(F.Pair2<S.t, tokens>, T.start, "internal parser result kind")

def read_1(value: answer) -> parsed(List<&2, S.binder>):
  match value:
    case Answer0{_}: L.error(F.Pair2<List<&2, S.binder>, tokens>, T.start, "internal parser result kind")
    case Answer1{value}: value
    case Answer2{_}: L.error(F.Pair2<List<&2, S.binder>, tokens>, T.start, "internal parser result kind")
    case Answer3{_}: L.error(F.Pair2<List<&2, S.binder>, tokens>, T.start, "internal parser result kind")
    case Answer4{_}: L.error(F.Pair2<List<&2, S.binder>, tokens>, T.start, "internal parser result kind")
    case Answer5{_}: L.error(F.Pair2<List<&2, S.binder>, tokens>, T.start, "internal parser result kind")
    case Answer6{_}: L.error(F.Pair2<List<&2, S.binder>, tokens>, T.start, "internal parser result kind")
    case Answer7{_}: L.error(F.Pair2<List<&2, S.binder>, tokens>, T.start, "internal parser result kind")
    case Answer8{_}: L.error(F.Pair2<List<&2, S.binder>, tokens>, T.start, "internal parser result kind")
    case Answer9{_}: L.error(F.Pair2<List<&2, S.binder>, tokens>, T.start, "internal parser result kind")
    case Answer10{_}: L.error(F.Pair2<List<&2, S.binder>, tokens>, T.start, "internal parser result kind")
    case Answer11{_}: L.error(F.Pair2<List<&2, S.binder>, tokens>, T.start, "internal parser result kind")
    case Answer12{_}: L.error(F.Pair2<List<&2, S.binder>, tokens>, T.start, "internal parser result kind")
    case Answer13{_}: L.error(F.Pair2<List<&2, S.binder>, tokens>, T.start, "internal parser result kind")

def read_2(value: answer) -> parsed(S.binder):
  match value:
    case Answer0{_}: L.error(F.Pair2<S.binder, tokens>, T.start, "internal parser result kind")
    case Answer1{_}: L.error(F.Pair2<S.binder, tokens>, T.start, "internal parser result kind")
    case Answer2{value}: value
    case Answer3{_}: L.error(F.Pair2<S.binder, tokens>, T.start, "internal parser result kind")
    case Answer4{_}: L.error(F.Pair2<S.binder, tokens>, T.start, "internal parser result kind")
    case Answer5{_}: L.error(F.Pair2<S.binder, tokens>, T.start, "internal parser result kind")
    case Answer6{_}: L.error(F.Pair2<S.binder, tokens>, T.start, "internal parser result kind")
    case Answer7{_}: L.error(F.Pair2<S.binder, tokens>, T.start, "internal parser result kind")
    case Answer8{_}: L.error(F.Pair2<S.binder, tokens>, T.start, "internal parser result kind")
    case Answer9{_}: L.error(F.Pair2<S.binder, tokens>, T.start, "internal parser result kind")
    case Answer10{_}: L.error(F.Pair2<S.binder, tokens>, T.start, "internal parser result kind")
    case Answer11{_}: L.error(F.Pair2<S.binder, tokens>, T.start, "internal parser result kind")
    case Answer12{_}: L.error(F.Pair2<S.binder, tokens>, T.start, "internal parser result kind")
    case Answer13{_}: L.error(F.Pair2<S.binder, tokens>, T.start, "internal parser result kind")

def read_3(value: answer) -> parsed(index_clause):
  match value:
    case Answer0{_}: L.error(F.Pair2<index_clause, tokens>, T.start, "internal parser result kind")
    case Answer1{_}: L.error(F.Pair2<index_clause, tokens>, T.start, "internal parser result kind")
    case Answer2{_}: L.error(F.Pair2<index_clause, tokens>, T.start, "internal parser result kind")
    case Answer3{value}: value
    case Answer4{_}: L.error(F.Pair2<index_clause, tokens>, T.start, "internal parser result kind")
    case Answer5{_}: L.error(F.Pair2<index_clause, tokens>, T.start, "internal parser result kind")
    case Answer6{_}: L.error(F.Pair2<index_clause, tokens>, T.start, "internal parser result kind")
    case Answer7{_}: L.error(F.Pair2<index_clause, tokens>, T.start, "internal parser result kind")
    case Answer8{_}: L.error(F.Pair2<index_clause, tokens>, T.start, "internal parser result kind")
    case Answer9{_}: L.error(F.Pair2<index_clause, tokens>, T.start, "internal parser result kind")
    case Answer10{_}: L.error(F.Pair2<index_clause, tokens>, T.start, "internal parser result kind")
    case Answer11{_}: L.error(F.Pair2<index_clause, tokens>, T.start, "internal parser result kind")
    case Answer12{_}: L.error(F.Pair2<index_clause, tokens>, T.start, "internal parser result kind")
    case Answer13{_}: L.error(F.Pair2<index_clause, tokens>, T.start, "internal parser result kind")

def read_4(value: answer) -> parsed(List<&2, String>):
  match value:
    case Answer0{_}: L.error(F.Pair2<List<&2, String>, tokens>, T.start, "internal parser result kind")
    case Answer1{_}: L.error(F.Pair2<List<&2, String>, tokens>, T.start, "internal parser result kind")
    case Answer2{_}: L.error(F.Pair2<List<&2, String>, tokens>, T.start, "internal parser result kind")
    case Answer3{_}: L.error(F.Pair2<List<&2, String>, tokens>, T.start, "internal parser result kind")
    case Answer4{value}: value
    case Answer5{_}: L.error(F.Pair2<List<&2, String>, tokens>, T.start, "internal parser result kind")
    case Answer6{_}: L.error(F.Pair2<List<&2, String>, tokens>, T.start, "internal parser result kind")
    case Answer7{_}: L.error(F.Pair2<List<&2, String>, tokens>, T.start, "internal parser result kind")
    case Answer8{_}: L.error(F.Pair2<List<&2, String>, tokens>, T.start, "internal parser result kind")
    case Answer9{_}: L.error(F.Pair2<List<&2, String>, tokens>, T.start, "internal parser result kind")
    case Answer10{_}: L.error(F.Pair2<List<&2, String>, tokens>, T.start, "internal parser result kind")
    case Answer11{_}: L.error(F.Pair2<List<&2, String>, tokens>, T.start, "internal parser result kind")
    case Answer12{_}: L.error(F.Pair2<List<&2, String>, tokens>, T.start, "internal parser result kind")
    case Answer13{_}: L.error(F.Pair2<List<&2, String>, tokens>, T.start, "internal parser result kind")

def read_5(value: answer) -> parsed(List<&2, S.branch>):
  match value:
    case Answer0{_}: L.error(F.Pair2<List<&2, S.branch>, tokens>, T.start, "internal parser result kind")
    case Answer1{_}: L.error(F.Pair2<List<&2, S.branch>, tokens>, T.start, "internal parser result kind")
    case Answer2{_}: L.error(F.Pair2<List<&2, S.branch>, tokens>, T.start, "internal parser result kind")
    case Answer3{_}: L.error(F.Pair2<List<&2, S.branch>, tokens>, T.start, "internal parser result kind")
    case Answer4{_}: L.error(F.Pair2<List<&2, S.branch>, tokens>, T.start, "internal parser result kind")
    case Answer5{value}: value
    case Answer6{_}: L.error(F.Pair2<List<&2, S.branch>, tokens>, T.start, "internal parser result kind")
    case Answer7{_}: L.error(F.Pair2<List<&2, S.branch>, tokens>, T.start, "internal parser result kind")
    case Answer8{_}: L.error(F.Pair2<List<&2, S.branch>, tokens>, T.start, "internal parser result kind")
    case Answer9{_}: L.error(F.Pair2<List<&2, S.branch>, tokens>, T.start, "internal parser result kind")
    case Answer10{_}: L.error(F.Pair2<List<&2, S.branch>, tokens>, T.start, "internal parser result kind")
    case Answer11{_}: L.error(F.Pair2<List<&2, S.branch>, tokens>, T.start, "internal parser result kind")
    case Answer12{_}: L.error(F.Pair2<List<&2, S.branch>, tokens>, T.start, "internal parser result kind")
    case Answer13{_}: L.error(F.Pair2<List<&2, S.branch>, tokens>, T.start, "internal parser result kind")

def read_6(value: answer) -> parsed(List<&2, S.field>):
  match value:
    case Answer0{_}: L.error(F.Pair2<List<&2, S.field>, tokens>, T.start, "internal parser result kind")
    case Answer1{_}: L.error(F.Pair2<List<&2, S.field>, tokens>, T.start, "internal parser result kind")
    case Answer2{_}: L.error(F.Pair2<List<&2, S.field>, tokens>, T.start, "internal parser result kind")
    case Answer3{_}: L.error(F.Pair2<List<&2, S.field>, tokens>, T.start, "internal parser result kind")
    case Answer4{_}: L.error(F.Pair2<List<&2, S.field>, tokens>, T.start, "internal parser result kind")
    case Answer5{_}: L.error(F.Pair2<List<&2, S.field>, tokens>, T.start, "internal parser result kind")
    case Answer6{value}: value
    case Answer7{_}: L.error(F.Pair2<List<&2, S.field>, tokens>, T.start, "internal parser result kind")
    case Answer8{_}: L.error(F.Pair2<List<&2, S.field>, tokens>, T.start, "internal parser result kind")
    case Answer9{_}: L.error(F.Pair2<List<&2, S.field>, tokens>, T.start, "internal parser result kind")
    case Answer10{_}: L.error(F.Pair2<List<&2, S.field>, tokens>, T.start, "internal parser result kind")
    case Answer11{_}: L.error(F.Pair2<List<&2, S.field>, tokens>, T.start, "internal parser result kind")
    case Answer12{_}: L.error(F.Pair2<List<&2, S.field>, tokens>, T.start, "internal parser result kind")
    case Answer13{_}: L.error(F.Pair2<List<&2, S.field>, tokens>, T.start, "internal parser result kind")

def read_7(value: answer) -> parsed(List<&2, S.decl>):
  match value:
    case Answer0{_}: L.error(F.Pair2<List<&2, S.decl>, tokens>, T.start, "internal parser result kind")
    case Answer1{_}: L.error(F.Pair2<List<&2, S.decl>, tokens>, T.start, "internal parser result kind")
    case Answer2{_}: L.error(F.Pair2<List<&2, S.decl>, tokens>, T.start, "internal parser result kind")
    case Answer3{_}: L.error(F.Pair2<List<&2, S.decl>, tokens>, T.start, "internal parser result kind")
    case Answer4{_}: L.error(F.Pair2<List<&2, S.decl>, tokens>, T.start, "internal parser result kind")
    case Answer5{_}: L.error(F.Pair2<List<&2, S.decl>, tokens>, T.start, "internal parser result kind")
    case Answer6{_}: L.error(F.Pair2<List<&2, S.decl>, tokens>, T.start, "internal parser result kind")
    case Answer7{value}: value
    case Answer8{_}: L.error(F.Pair2<List<&2, S.decl>, tokens>, T.start, "internal parser result kind")
    case Answer9{_}: L.error(F.Pair2<List<&2, S.decl>, tokens>, T.start, "internal parser result kind")
    case Answer10{_}: L.error(F.Pair2<List<&2, S.decl>, tokens>, T.start, "internal parser result kind")
    case Answer11{_}: L.error(F.Pair2<List<&2, S.decl>, tokens>, T.start, "internal parser result kind")
    case Answer12{_}: L.error(F.Pair2<List<&2, S.decl>, tokens>, T.start, "internal parser result kind")
    case Answer13{_}: L.error(F.Pair2<List<&2, S.decl>, tokens>, T.start, "internal parser result kind")

def read_8(value: answer) -> parsed(S.decl):
  match value:
    case Answer0{_}: L.error(F.Pair2<S.decl, tokens>, T.start, "internal parser result kind")
    case Answer1{_}: L.error(F.Pair2<S.decl, tokens>, T.start, "internal parser result kind")
    case Answer2{_}: L.error(F.Pair2<S.decl, tokens>, T.start, "internal parser result kind")
    case Answer3{_}: L.error(F.Pair2<S.decl, tokens>, T.start, "internal parser result kind")
    case Answer4{_}: L.error(F.Pair2<S.decl, tokens>, T.start, "internal parser result kind")
    case Answer5{_}: L.error(F.Pair2<S.decl, tokens>, T.start, "internal parser result kind")
    case Answer6{_}: L.error(F.Pair2<S.decl, tokens>, T.start, "internal parser result kind")
    case Answer7{_}: L.error(F.Pair2<S.decl, tokens>, T.start, "internal parser result kind")
    case Answer8{value}: value
    case Answer9{_}: L.error(F.Pair2<S.decl, tokens>, T.start, "internal parser result kind")
    case Answer10{_}: L.error(F.Pair2<S.decl, tokens>, T.start, "internal parser result kind")
    case Answer11{_}: L.error(F.Pair2<S.decl, tokens>, T.start, "internal parser result kind")
    case Answer12{_}: L.error(F.Pair2<S.decl, tokens>, T.start, "internal parser result kind")
    case Answer13{_}: L.error(F.Pair2<S.decl, tokens>, T.start, "internal parser result kind")

def read_9(value: answer) -> parsed(List<&2, S.rec_def>):
  match value:
    case Answer0{_}: L.error(F.Pair2<List<&2, S.rec_def>, tokens>, T.start, "internal parser result kind")
    case Answer1{_}: L.error(F.Pair2<List<&2, S.rec_def>, tokens>, T.start, "internal parser result kind")
    case Answer2{_}: L.error(F.Pair2<List<&2, S.rec_def>, tokens>, T.start, "internal parser result kind")
    case Answer3{_}: L.error(F.Pair2<List<&2, S.rec_def>, tokens>, T.start, "internal parser result kind")
    case Answer4{_}: L.error(F.Pair2<List<&2, S.rec_def>, tokens>, T.start, "internal parser result kind")
    case Answer5{_}: L.error(F.Pair2<List<&2, S.rec_def>, tokens>, T.start, "internal parser result kind")
    case Answer6{_}: L.error(F.Pair2<List<&2, S.rec_def>, tokens>, T.start, "internal parser result kind")
    case Answer7{_}: L.error(F.Pair2<List<&2, S.rec_def>, tokens>, T.start, "internal parser result kind")
    case Answer8{_}: L.error(F.Pair2<List<&2, S.rec_def>, tokens>, T.start, "internal parser result kind")
    case Answer9{value}: value
    case Answer10{_}: L.error(F.Pair2<List<&2, S.rec_def>, tokens>, T.start, "internal parser result kind")
    case Answer11{_}: L.error(F.Pair2<List<&2, S.rec_def>, tokens>, T.start, "internal parser result kind")
    case Answer12{_}: L.error(F.Pair2<List<&2, S.rec_def>, tokens>, T.start, "internal parser result kind")
    case Answer13{_}: L.error(F.Pair2<List<&2, S.rec_def>, tokens>, T.start, "internal parser result kind")

def read_10(value: answer) -> parsed(S.rec_def):
  match value:
    case Answer0{_}: L.error(F.Pair2<S.rec_def, tokens>, T.start, "internal parser result kind")
    case Answer1{_}: L.error(F.Pair2<S.rec_def, tokens>, T.start, "internal parser result kind")
    case Answer2{_}: L.error(F.Pair2<S.rec_def, tokens>, T.start, "internal parser result kind")
    case Answer3{_}: L.error(F.Pair2<S.rec_def, tokens>, T.start, "internal parser result kind")
    case Answer4{_}: L.error(F.Pair2<S.rec_def, tokens>, T.start, "internal parser result kind")
    case Answer5{_}: L.error(F.Pair2<S.rec_def, tokens>, T.start, "internal parser result kind")
    case Answer6{_}: L.error(F.Pair2<S.rec_def, tokens>, T.start, "internal parser result kind")
    case Answer7{_}: L.error(F.Pair2<S.rec_def, tokens>, T.start, "internal parser result kind")
    case Answer8{_}: L.error(F.Pair2<S.rec_def, tokens>, T.start, "internal parser result kind")
    case Answer9{_}: L.error(F.Pair2<S.rec_def, tokens>, T.start, "internal parser result kind")
    case Answer10{value}: value
    case Answer11{_}: L.error(F.Pair2<S.rec_def, tokens>, T.start, "internal parser result kind")
    case Answer12{_}: L.error(F.Pair2<S.rec_def, tokens>, T.start, "internal parser result kind")
    case Answer13{_}: L.error(F.Pair2<S.rec_def, tokens>, T.start, "internal parser result kind")

def read_11(value: answer) -> parsed(List<&2, S.fam>):
  match value:
    case Answer0{_}: L.error(F.Pair2<List<&2, S.fam>, tokens>, T.start, "internal parser result kind")
    case Answer1{_}: L.error(F.Pair2<List<&2, S.fam>, tokens>, T.start, "internal parser result kind")
    case Answer2{_}: L.error(F.Pair2<List<&2, S.fam>, tokens>, T.start, "internal parser result kind")
    case Answer3{_}: L.error(F.Pair2<List<&2, S.fam>, tokens>, T.start, "internal parser result kind")
    case Answer4{_}: L.error(F.Pair2<List<&2, S.fam>, tokens>, T.start, "internal parser result kind")
    case Answer5{_}: L.error(F.Pair2<List<&2, S.fam>, tokens>, T.start, "internal parser result kind")
    case Answer6{_}: L.error(F.Pair2<List<&2, S.fam>, tokens>, T.start, "internal parser result kind")
    case Answer7{_}: L.error(F.Pair2<List<&2, S.fam>, tokens>, T.start, "internal parser result kind")
    case Answer8{_}: L.error(F.Pair2<List<&2, S.fam>, tokens>, T.start, "internal parser result kind")
    case Answer9{_}: L.error(F.Pair2<List<&2, S.fam>, tokens>, T.start, "internal parser result kind")
    case Answer10{_}: L.error(F.Pair2<List<&2, S.fam>, tokens>, T.start, "internal parser result kind")
    case Answer11{value}: value
    case Answer12{_}: L.error(F.Pair2<List<&2, S.fam>, tokens>, T.start, "internal parser result kind")
    case Answer13{_}: L.error(F.Pair2<List<&2, S.fam>, tokens>, T.start, "internal parser result kind")

def read_12(value: answer) -> parsed(S.fam):
  match value:
    case Answer0{_}: L.error(F.Pair2<S.fam, tokens>, T.start, "internal parser result kind")
    case Answer1{_}: L.error(F.Pair2<S.fam, tokens>, T.start, "internal parser result kind")
    case Answer2{_}: L.error(F.Pair2<S.fam, tokens>, T.start, "internal parser result kind")
    case Answer3{_}: L.error(F.Pair2<S.fam, tokens>, T.start, "internal parser result kind")
    case Answer4{_}: L.error(F.Pair2<S.fam, tokens>, T.start, "internal parser result kind")
    case Answer5{_}: L.error(F.Pair2<S.fam, tokens>, T.start, "internal parser result kind")
    case Answer6{_}: L.error(F.Pair2<S.fam, tokens>, T.start, "internal parser result kind")
    case Answer7{_}: L.error(F.Pair2<S.fam, tokens>, T.start, "internal parser result kind")
    case Answer8{_}: L.error(F.Pair2<S.fam, tokens>, T.start, "internal parser result kind")
    case Answer9{_}: L.error(F.Pair2<S.fam, tokens>, T.start, "internal parser result kind")
    case Answer10{_}: L.error(F.Pair2<S.fam, tokens>, T.start, "internal parser result kind")
    case Answer11{_}: L.error(F.Pair2<S.fam, tokens>, T.start, "internal parser result kind")
    case Answer12{value}: value
    case Answer13{_}: L.error(F.Pair2<S.fam, tokens>, T.start, "internal parser result kind")

def read_13(value: answer) -> parsed(List<&2, S.fam_ctor>):
  match value:
    case Answer0{_}: L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, T.start, "internal parser result kind")
    case Answer1{_}: L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, T.start, "internal parser result kind")
    case Answer2{_}: L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, T.start, "internal parser result kind")
    case Answer3{_}: L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, T.start, "internal parser result kind")
    case Answer4{_}: L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, T.start, "internal parser result kind")
    case Answer5{_}: L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, T.start, "internal parser result kind")
    case Answer6{_}: L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, T.start, "internal parser result kind")
    case Answer7{_}: L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, T.start, "internal parser result kind")
    case Answer8{_}: L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, T.start, "internal parser result kind")
    case Answer9{_}: L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, T.start, "internal parser result kind")
    case Answer10{_}: L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, T.start, "internal parser result kind")
    case Answer11{_}: L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, T.start, "internal parser result kind")
    case Answer12{_}: L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, T.start, "internal parser result kind")
    case Answer13{value}: value

# Backtracking retries one binder probe; successful loops consume tokens.
def arm_count(bs: List<&2, S.branch>) -> Nat:
  match bs:
    case []: 0n
    case _ <> rest: (1n + arm_count(rest) : Nat)
def native_branch(+loc: T.loc, +body: S.t, acc: List<&2, S.branch>) -> L.result(S.branch):
  match body:
    case S.SFun{[binder], arm_body}:
      L.bind(F.Int63.t, S.branch, bounded(loc, F.Bignum.from_nat(arm_count(acc))), key =>
        Done{S.BrLeg{key, [binder], arm_body}})
    case S.SFun{binder <> rest, arm_body}:
      L.bind(F.Int63.t, S.branch, bounded(loc, F.Bignum.from_nat(arm_count(acc))), key =>
        Done{S.BrLeg{key, [binder], S.SFun{rest, arm_body}}})
    case _:
      L.bind(F.Int63.t, S.branch, bounded(loc, F.Bignum.from_nat(arm_count(acc))), key =>
        Done{S.BrLeg{key, [], body}})

@unsafe
def dispatch(job: task) -> answer:
  match job:
    case ParseTerm{+ts}:
      match ts:
        case T.Token{T.KFun{}, _} <> rest:
          Answer0{read_0(dispatch(ParseFunTerm{rest}))}
        case T.Token{T.KLet{}, _} <> rest:
          Answer0{read_0(dispatch(ParseLetTerm{rest}))}
        case T.Token{T.KElim{}, _} <> rest:
          Answer0{read_0(dispatch(ParseCaseTerm{LegacyCase{}, rest}))}
        case T.Token{T.KMatch{}, _} <> rest:
          Answer0{read_0(dispatch(ParseCaseTerm{FiberedMatch{}, rest}))}
        case _:
          Answer0{read_0(dispatch(ParseArrow{ts}))}
    case ParseFunTerm{+ts}:
      Answer0{next(S.binder, S.t, read_2(dispatch(ParseBinder{ts})), first => rest =>
        next(List<&2, S.binder>, S.t, read_1(dispatch(ParseBinders{rest, [first]})), bs => tail => read_0(dispatch(ParseFunBody{bs, tail}))))}
    case ParseFunBody{+bs, +ts}:
      match ts:
        case T.Token{T.DArrow{}, _} <> rest:
          Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{rest})), body => tail => done(S.t, S.SFun{bs, body}, tail))}
        case _:
          Answer0{want(S.t, "'=>'", ts)}
    case ParseBinders{+ts, +acc}:
      match ts:
        case T.Token{T.LParen{}, _} <> _:
          Answer1{next(S.binder, List<&2, S.binder>, read_2(dispatch(ParseBinder{ts})), b => rest => read_1(dispatch(ParseBinders{rest, b <> acc})))}
        case _:
          Answer1{done(List<&2, S.binder>, L.reverse(S.binder, acc, []), ts)}
    case ParseBinder{+ts}:
      match ts:
        case T.Token{T.LParen{}, _} <> rest:
          Answer2{mark_then(S.binder, rest, q => tail => read_2(dispatch(ParseBinderHead{q, tail})))}
        case _:
          Answer2{want(S.binder, "'('", ts)}
    case ParseBinderHead{+q, +ts}:
      match ts:
        case T.Token{T.Ident{name}, _} <> T.Token{T.Colon{}, _} <> rest:
          Answer2{next(S.t, S.binder, read_0(dispatch(ParseTerm{rest})), ty => tail => read_2(dispatch(ParseBinderClose{S.Binder{q, name, ty}, tail})))}
        case _:
          Answer2{want(S.binder, "a binder name and ':'", ts)}
    case ParseBinderClose{+b, +ts}:
      match ts:
        case T.Token{T.RParen{}, _} <> rest:
          Answer2{done(S.binder, b, rest)}
        case _:
          Answer2{want(S.binder, "')'", ts)}
    case ParseLetTerm{+ts}:
      match ts:
        case T.Token{T.Ident{name}, _} <> T.Token{T.Colon{}, _} <> rest:
          Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{rest})), ty => tail => read_0(dispatch(ParseLetValue{name, ty, tail})))}
        case _:
          Answer0{want(S.t, "a name and ':' after 'let'", ts)}
    case ParseLetValue{+name, +ty, +ts}:
      match ts:
        case T.Token{T.ColonEq{}, _} <> rest:
          Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{rest})), value => tail => read_0(dispatch(ParseLetBody{name, ty, value, tail})))}
        case _:
          Answer0{want(S.t, "':='", ts)}
    case ParseLetBody{+name, +ty, +value, +ts}:
      match ts:
        case T.Token{T.KIn{}, _} <> rest:
          Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{rest})), body => tail => done(S.t, S.SLet{name, ty, value, body}, tail))}
        case _:
          Answer0{want(S.t, "'in'", ts)}
    case ParseCaseTerm{+form, +ts}:
      Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{ts})), scrut => rest => read_0(dispatch(ParseCaseMotive{form, scrut, rest})))}
    case ParseCaseMotive{+form, +scrut, +ts}:
      match ts:
        case T.Token{T.KAs{}, _} <> T.Token{T.Ident{name}, _} <> rest:
          Answer0{next(index_clause, S.t, read_3(dispatch(ParseIndex{rest})), clause => tail => read_0(dispatch(ParseMotiveReturn{form, scrut, name, clause, tail})))}
        case T.Token{T.KAs{}, loc} <> _:
          Answer0{L.error(F.Pair2<S.t, tokens>, loc, "expected 'NAME [in FAMILY IDX..] return TYPE' after 'as'")}
        case T.Token{T.LBrace{}, _} <> rest:
          Answer0{next(List<&2, S.branch>, S.t, read_5(dispatch(ParseBranches{form, rest, []})), bs => tail => done(S.t, elimination_node(form, scrut, None{}, bs), tail))}
        case _:
          Answer0{want(S.t, "'as' or '{'", ts)}
    case ParseIndex{+ts}:
      match ts:
        case T.Token{T.KIn{}, _} <> T.Token{T.Ident{name}, _} <> rest:
          Answer3{next(List<&2, String>, index_clause, read_4(dispatch(ParseNames{rest, []})), ns => tail => done(index_clause, IndexClause{Some{name}, ns}, tail))}
        case _:
          Answer3{done(index_clause, IndexClause{None{}, []}, ts)}
    case ParseNames{+ts, +acc}:
      match ts:
        case T.Token{T.Ident{name}, _} <> rest:
          Answer4{read_4(dispatch(ParseNames{rest, name <> acc}))}
        case _:
          Answer4{done(List<&2, String>, L.reverse(String, acc, []), ts)}
    case ParseMotiveReturn{+form, +scrut, +name, +clause, +ts}:
      match ts:
        case T.Token{T.KReturn{}, _} <> rest:
          Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{rest})), body => tail => read_0(dispatch(ParseMotiveWith{form, scrut, name, clause, body, tail})))}
        case _:
          Answer0{want(S.t, "'return'", ts)}
    case ParseMotiveWith{+form, +scrut, +name, +clause, +body, +ts}:
      match clause:
        case IndexClause{ind, idx}:
          match ts:
            case T.Token{T.LBrace{}, _} <> rest:
              Answer0{next(List<&2, S.branch>, S.t, read_5(dispatch(ParseBranches{form, rest, []})), bs => tail => done(S.t, elimination_node(form, scrut, Some{S.Motive{name, ind, idx, body}}, bs), tail))}
            case _:
              Answer0{want(S.t, "'{'", ts)}
    case ParseBranches{+form, +ts, +acc}:
      match ts:
        case T.Token{T.RBrace{}, _} <> rest:
          Answer5{done(List<&2, S.branch>, L.reverse(S.branch, acc, []), rest)}
        case _:
          Answer5{next(S.t, List<&2, S.branch>, read_0(dispatch(ParseTerm{ts})), body => tail => read_5(dispatch(ParseNativeArmClose{form, body, tail, acc})))}
    case ParseNativeArmClose{+form, body, +ts, +acc}:
      match ts:
        case T.Token{T.Semicolon{}, +loc} <> rest:
          Answer5{L.bind(S.branch, F.Pair2<List<&2, S.branch>, tokens>, native_branch(loc, body, acc), br => read_5(dispatch(ParseBranches{form, rest, br <> acc})))}
        case T.Token{T.RBrace{}, +loc} <> rest:
          Answer5{L.bind(S.branch, F.Pair2<List<&2, S.branch>, tokens>, native_branch(loc, body, acc), br => done(List<&2, S.branch>, L.reverse(S.branch, br <> acc, []), rest))}
        case _:
          Answer5{want(List<&2, S.branch>, "';' or '}' after an elim arm", ts)}
    case ParseLegBody{+form, +key, +bs, +ts, +acc}:
      match ts:
        case T.Token{T.DArrow{}, _} <> rest:
          Answer5{next(S.t, List<&2, S.branch>, read_0(dispatch(ParseTerm{rest})), body => tail => read_5(dispatch(ParseBranches{form, tail, S.BrLeg{key, bs, body} <> acc})))}
        case _:
          Answer5{want(List<&2, S.branch>, "'=>'", ts)}
    case ParseCtorBody{+form, +name, +fs, +ts, +acc}:
      match ts:
        case T.Token{T.DArrow{}, _} <> rest:
          Answer5{next(S.t, List<&2, S.branch>, read_0(dispatch(ParseTerm{rest})), body => tail => read_5(dispatch(ParseBranches{form, tail, S.BrCtor{name, fs, body} <> acc})))}
        case _:
          Answer5{want(List<&2, S.branch>, "'=>'", ts)}
    case ParseFields{+ts, +acc}:
      match ts:
        case T.Token{T.LParen{}, _} <> _:
          Answer6{next(S.binder, List<&2, S.field>, read_2(dispatch(ParseBinder{ts})), b => rest => read_6(dispatch(ParseTypedField{b, rest, acc})))}
        case _:
          Answer6{mark_then(List<&2, S.field>, ts, q => rest => read_6(dispatch(ParseUntypedField{q, rest, ts, acc})))}
    case ParseTypedField{+b, +ts, +acc}:
      match b:
        case S.Binder{q, name, ty}:
          Answer6{read_6(dispatch(ParseFields{ts, S.Field{q, name, Some{ty}} <> acc}))}
    case ParseUntypedField{+q, +ts, +original, +acc}:
      match ts:
        case T.Token{T.Ident{name}, _} <> rest:
          Answer6{read_6(dispatch(ParseFields{rest, S.Field{q, name, None{}} <> acc}))}
        case _:
          Answer6{done(List<&2, S.field>, L.reverse(S.field, acc, []), original)}
    case ParseArrow{+ts}:
      match ts:
        case T.Token{T.LParen{}, _} <> _:
          Answer0{read_0(dispatch(ParseAttemptArrow{ts, read_2(dispatch(ParseBinder{ts}))}))}
        case _:
          Answer0{read_0(dispatch(ParseArrowPlain{ts}))}
    case ParseAttemptArrow{+original, +value}:
      match value:
        case Fail{_}:
          Answer0{read_0(dispatch(ParseArrowPlain{original}))}
        case Done{F.Pair2{b, +rest}}:
          match rest:
            case T.Token{T.Arrow{}, _} <> _:
              Answer0{read_0(dispatch(ParseGroupOp{b, rest}))}
            case T.Token{T.Star{}, _} <> _:
              Answer0{read_0(dispatch(ParseGroupOp{b, rest}))}
            case _:
              Answer0{read_0(dispatch(ParseArrowPlain{original}))}
    case ParseGroupOp{+b, +ts}:
      match ts:
        case T.Token{T.Arrow{}, _} <> rest:
          Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{rest})), body => tail => done(S.t, S.SArrow{b, body}, tail))}
        case T.Token{T.Star{}, _} <> rest:
          Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{rest})), body => tail => done(S.t, S.SStar{b, body}, tail))}
        case _:
          Answer0{want(S.t, "'->' or '*'", ts)}
    case ParseArrowPlain{+ts}:
      Answer0{next(S.t, S.t, read_0(dispatch(ParseApp{ts})), lhs => rest => read_0(dispatch(ParseArrowTail{lhs, rest})))}
    case ParseArrowTail{+lhs, +ts}:
      match ts:
        case T.Token{T.Arrow{}, _} <> rest:
          Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{rest})), rhs => tail => done(S.t, S.SArrow{S.Binder{Q.Many{}, "_", lhs}, rhs}, tail))}
        case T.Token{T.Star{}, _} <> rest:
          Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{rest})), rhs => tail => done(S.t, S.SStar{S.Binder{Q.Many{}, "_", lhs}, rhs}, tail))}
        case _:
          Answer0{done(S.t, lhs, ts)}
    case ParseApp{+ts}:
      match ts:
        case T.Token{T.KInj{}, _} <> rest:
          Answer0{read_0(dispatch(ParseInj{rest}))}
        case T.Token{T.KAbsurd{}, _} <> rest:
          Answer0{next(S.t, S.t, read_0(dispatch(ParseApp{rest})), a => tail => done(S.t, S.SAbsurd{a}, tail))}
        case _:
          Answer0{next(S.t, S.t, read_0(dispatch(ParseAtom{ts})), head => rest => read_0(dispatch(ParseAppRest{head, rest})))}
    case ParseInj{+ts}:
      match ts:
        case T.Token{T.Number{k}, kloc} <> T.Token{T.KOf{}, _} <> T.Token{T.Number{n}, nloc} <> rest:
          Answer0{L.bind(F.Int63.t, F.Pair2<S.t, tokens>, bounded(kloc, k), key =>
            L.bind(F.Int63.t, F.Pair2<S.t, tokens>, bounded(nloc, n), size =>
              next(S.t, S.t, read_0(dispatch(ParseApp{rest})), a => tail => done(S.t, S.SInj{key, size, a}, tail))))}
        case _:
          Answer0{want(S.t, "'K of N' after 'inj'", ts)}
    case ParseAppRest{+head, +ts}:
      Answer0{L.choose(parsed(S.t), starts_atom(ts),
        _ => next(S.t, S.t, read_0(dispatch(ParseAtom{ts})), arg => rest => read_0(dispatch(ParseAppRest{S.SApp{head, arg}, rest}))),
        _ => done(S.t, head, ts))}
    case ParseAtom{+ts}:
      Answer0{next(S.t, S.t, read_0(dispatch(ParseAtomHead{ts})), a => rest => read_0(dispatch(ParsePostfix{a, rest})))}
    case ParsePostfix{+a, +ts}:
      match ts:
        case T.Token{T.Dot1{}, _} <> rest:
          Answer0{read_0(dispatch(ParsePostfix{S.SProj{a, F.Int63.one}, rest}))}
        case T.Token{T.Dot2{}, _} <> rest:
          Answer0{read_0(dispatch(ParsePostfix{S.SProj{a, F.Int63.succ(F.Int63.one)}, rest}))}
        case T.Token{T.Dot{}, _} <> T.Token{T.Number{k}, loc} <> rest:
          Answer0{L.bind(F.Int63.t, F.Pair2<S.t, tokens>, bounded(loc, k), key => read_0(dispatch(ParsePostfix{S.SProj{a, key}, rest})))}
        case T.Token{T.Dot{}, loc} <> _:
          Answer0{L.error(F.Pair2<S.t, tokens>, loc, "expected a leg number after '.'")}
        case _:
          Answer0{done(S.t, a, ts)}
    case ParseAtomHead{+ts}:
      match ts:
        case T.Token{T.Ident{x}, _} <> rest:
          Answer0{done(S.t, S.SVar{x}, rest)}
        case T.Token{T.Number{n}, _} <> rest:
          Answer0{done(S.t, S.SNat{n}, rest)}
        case T.Token{T.Bytes{bytes}, _} <> rest:
          Answer0{done(S.t, bytes_term(bytes), rest)}
        case T.Token{T.KProp{}, _} <> rest:
          Answer0{done(S.t, S.SProp{}, rest)}
        case T.Token{T.KType{}, _} <> T.Token{T.Number{n}, loc} <> rest:
          Answer0{L.bind(F.Int63.t, F.Pair2<S.t, tokens>, bounded(loc, n), level => done(S.t, S.SType{level}, rest))}
        case T.Token{T.KType{}, _} <> rest:
          Answer0{done(S.t, S.SType{F.Int63.zero}, rest)}
        case T.Token{T.KAuto{}, _} <> rest:
          Answer0{done(S.t, S.SAuto{}, rest)}
        case T.Token{T.Unit{}, _} <> rest:
          Answer0{done(S.t, S.SUnit{}, rest)}
        case T.Token{T.KNatAdd{}, _} <> rest:
          Answer0{done(S.t, S.SPrim{S.PAdd{}}, rest)}
        case T.Token{T.KNatSub{}, _} <> rest:
          Answer0{done(S.t, S.SPrim{S.PSub{}}, rest)}
        case T.Token{T.KNatMul{}, _} <> rest:
          Answer0{done(S.t, S.SPrim{S.PMul{}}, rest)}
        case T.Token{T.KNatEq{}, _} <> rest:
          Answer0{done(S.t, S.SPrim{S.PEq{}}, rest)}
        case T.Token{T.KNatLt{}, _} <> rest:
          Answer0{done(S.t, S.SPrim{S.PLt{}}, rest)}
        case T.Token{T.KTuple{}, _} <> rest:
          Answer0{read_0(dispatch(ParseItems{"tuple", rest}))}
        case T.Token{T.KSum{}, _} <> rest:
          Answer0{read_0(dispatch(ParseItems{"sum", rest}))}
        case T.Token{T.KProd{}, _} <> rest:
          Answer0{read_0(dispatch(ParseItems{"prod", rest}))}
        case T.Token{T.KMu{}, loc} <> _:
          Answer0{L.error(F.Pair2<S.t, tokens>, loc, "a mu group is a declaration and not a term")}
        case T.Token{T.KAnd{}, loc} <> _:
          Answer0{L.error(F.Pair2<S.t, tokens>, loc, "'and' joins two members of a mu group and is not a term")}
        case T.Token{T.KNu{}, loc} <> _:
          Answer0{L.error(F.Pair2<S.t, tokens>, loc, "nu arrives at M2")}
        case T.Token{T.LParen{}, _} <> rest:
          Answer0{read_0(dispatch(ParseParen{rest}))}
        case _:
          Answer0{want(S.t, "a term", ts)}
    case ParseParen{+ts}:
      Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{ts})), inner => rest => read_0(dispatch(ParseParenTail{inner, rest})))}
    case ParseParenTail{+inner, +ts}:
      match ts:
        case T.Token{T.RParen{}, _} <> rest:
          Answer0{done(S.t, inner, rest)}
        case T.Token{T.Comma{}, _} <> rest:
          Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{rest})), second => tail => read_0(dispatch(ParseCloseTerm{S.SPair{inner, second}, tail})))}
        case T.Token{T.Colon{}, _} <> rest:
          Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{rest})), ty => tail => read_0(dispatch(ParseCloseTerm{S.SAnn{inner, ty}, tail})))}
        case _:
          Answer0{want(S.t, "')', ',' or ':'", ts)}
    case ParseCloseTerm{+value, +ts}:
      match ts:
        case T.Token{T.RParen{}, _} <> rest:
          Answer0{done(S.t, value, rest)}
        case _:
          Answer0{want(S.t, "')'", ts)}
    case ParseItems{+word, +ts}:
      match ts:
        case T.Token{T.Unit{}, _} <> rest:
          Answer0{done(S.t, collection(word, []), rest)}
        case T.Token{T.LParen{}, _} <> rest:
          Answer0{read_0(dispatch(ParseItemsBody{word, rest}))}
        case _:
          Answer0{want(S.t, S.cat(["'(' after '", word, "'"]), ts)}
    case ParseItemsBody{+word, +ts}:
      match ts:
        case T.Token{T.RParen{}, _} <> rest:
          Answer0{done(S.t, collection(word, []), rest)}
        case _:
          Answer0{read_0(dispatch(ParseItemsRest{word, ts, []}))}
    case ParseItemsRest{+word, +ts, +acc}:
      Answer0{next(S.t, S.t, read_0(dispatch(ParseTerm{ts})), item => rest => read_0(dispatch(ParseItemsTail{word, rest, item <> acc})))}
    case ParseItemsTail{+word, +ts, +acc}:
      match ts:
        case T.Token{T.Comma{}, _} <> rest:
          Answer0{read_0(dispatch(ParseItemsRest{word, rest, acc}))}
        case T.Token{T.RParen{}, _} <> rest:
          Answer0{done(S.t, collection(word, L.reverse(S.t, acc, [])), rest)}
        case _:
          Answer0{want(S.t, "',' or ')'", ts)}
    case ParseDecls{+ts, +acc}:
      match ts:
        case T.Token{T.Eof{}, _} <> rest:
          Answer7{done(List<&2, S.decl>, L.reverse(S.decl, acc, []), rest)}
        case _:
          Answer7{next(S.decl, List<&2, S.decl>, read_8(dispatch(ParseDecl{ts})), d => rest => read_7(dispatch(ParseDecls{rest, d <> acc})))}
    case ParseDecl{+ts}:
      match ts:
        case T.Token{T.KDef{}, _} <> T.Token{T.KRec{}, _} <> rest:
          Answer8{next(List<&2, S.rec_def>, S.decl, read_9(dispatch(ParseRecGroup{rest, []})), ms => tail => done(S.decl, S.DRec{ms}, tail))}
        case T.Token{T.KDef{}, _} <> T.Token{T.Ident{name}, _} <> T.Token{T.Colon{}, _} <> rest:
          Answer8{next(S.t, S.decl, read_0(dispatch(ParseTerm{rest})), ty => tail => read_8(dispatch(ParseDefBody{name, ty, tail})))}
        case T.Token{T.KAxiom{}, _} <> T.Token{T.Ident{name}, _} <> T.Token{T.Colon{}, _} <> rest:
          Answer8{next(S.t, S.decl, read_0(dispatch(ParseTerm{rest})), ty => tail => done(S.decl, S.DAxiom{name, ty}, tail))}
        case T.Token{T.KMu{}, _} <> rest:
          Answer8{next(List<&2, S.fam>, S.decl, read_11(dispatch(ParseFamGroup{rest, []})), fs => tail => done(S.decl, S.DMu{fs}, tail))}
        case T.Token{T.KMutual{}, _} <> rest:
          Answer8{next(List<&2, S.fam>, S.decl, read_11(dispatch(ParseMutualGroup{rest, []})), fs => tail => done(S.decl, S.DMu{fs}, tail))}
        case T.Token{T.KNu{}, loc} <> _:
          Answer8{L.error(F.Pair2<S.decl, tokens>, loc, "nu arrives at M2")}
        case _:
          Answer8{want(S.decl, "'def NAME :', 'def rec NAME :', 'axiom NAME :', 'mu NAME' or 'mutual'", ts)}
    case ParseDefBody{+name, +ty, +ts}:
      match ts:
        case T.Token{T.ColonEq{}, _} <> rest:
          Answer8{next(S.t, S.decl, read_0(dispatch(ParseTerm{rest})), body => tail => done(S.decl, S.DDef{name, ty, body}, tail))}
        case _:
          Answer8{want(S.decl, "':='", ts)}
    case ParseRecGroup{+ts, +acc}:
      Answer9{next(S.rec_def, List<&2, S.rec_def>, read_10(dispatch(ParseRecMember{ts})), m => rest => read_9(dispatch(ParseRecTail{rest, m <> acc})))}
    case ParseRecTail{+ts, +acc}:
      match ts:
        case T.Token{T.KAnd{}, _} <> rest:
          Answer9{read_9(dispatch(ParseRecGroup{rest, acc}))}
        case _:
          Answer9{done(List<&2, S.rec_def>, L.reverse(S.rec_def, acc, []), ts)}
    case ParseRecMember{+ts}:
      match ts:
        case T.Token{T.Ident{name}, _} <> T.Token{T.Colon{}, _} <> rest:
          Answer10{next(S.t, S.rec_def, read_0(dispatch(ParseTerm{rest})), ty => tail => read_10(dispatch(ParseRecBody{name, ty, tail})))}
        case _:
          Answer10{want(S.rec_def, "a definition name and ':'", ts)}
    case ParseRecBody{+name, +ty, +ts}:
      match ts:
        case T.Token{T.ColonEq{}, _} <> rest:
          Answer10{next(S.t, S.rec_def, read_0(dispatch(ParseTerm{rest})), body => tail => done(S.rec_def, S.RecDef{name, ty, body}, tail))}
        case _:
          Answer10{want(S.rec_def, "':='", ts)}
    case ParseFamGroup{+ts, +acc}:
      Answer11{next(S.fam, List<&2, S.fam>, read_12(dispatch(ParseFam{ts})), fm => rest => read_11(dispatch(ParseFamTail{rest, fm <> acc})))}
    case ParseFamTail{+ts, +acc}:
      match ts:
        case T.Token{T.KAnd{}, _} <> rest:
          Answer11{read_11(dispatch(ParseFamGroup{rest, acc}))}
        case _:
          Answer11{done(List<&2, S.fam>, L.reverse(S.fam, acc, []), ts)}
    case ParseMutualGroup{+ts, +acc}:
      match ts:
        case T.Token{T.KMu{}, _} <> rest:
          Answer11{next(S.fam, List<&2, S.fam>, read_12(dispatch(ParseFam{rest})), fm => tail => read_11(dispatch(ParseMutualGroup{tail, fm <> acc})))}
        case T.Token{T.KEnd{}, loc} <> rest:
          match acc:
            case []:
              Answer11{L.error(F.Pair2<List<&2, S.fam>, tokens>, loc, "a mutual group needs at least two mu declarations")}
            case _ <> []:
              Answer11{L.error(F.Pair2<List<&2, S.fam>, tokens>, loc, "a mutual group needs at least two mu declarations")}
            case a <> b <> others:
              Answer11{done(List<&2, S.fam>, L.reverse(S.fam, a <> b <> others, []), rest)}
        case _:
          Answer11{want(List<&2, S.fam>, "'mu' or 'end' in a mutual group", ts)}
    case ParseFam{+ts}:
      match ts:
        case T.Token{T.Ident{name}, _} <> rest:
          Answer12{next(List<&2, S.binder>, S.fam, read_1(dispatch(ParseBinders{rest, []})), bs => tail => read_12(dispatch(ParseFamType{name, bs, tail})))}
        case _:
          Answer12{want(S.fam, "a family name", ts)}
    case ParseFamType{+name, +params, +ts}:
      match ts:
        case T.Token{T.Colon{}, _} <> rest:
          Answer12{next(S.t, S.fam, read_0(dispatch(ParseTerm{rest})), ty => tail => read_12(dispatch(ParseFamConstructors{name, params, ty, tail})))}
        case _:
          Answer12{want(S.fam, "':'", ts)}
    case ParseFamConstructors{+name, +params, +ty, +ts}:
      match ts:
        case T.Token{T.KWith{}, _} <> rest:
          Answer12{next(List<&2, S.fam_ctor>, S.fam, read_13(dispatch(ParseFamCtors{rest, []})), cs => tail => done(S.fam, S.Fam{name, params, ty, cs}, tail))}
        case T.Token{T.ColonEq{}, _} <> rest:
          Answer12{next(List<&2, S.fam_ctor>, S.fam, read_13(dispatch(ParseFamCtors{rest, []})), cs => tail => done(S.fam, S.Fam{name, params, ty, cs}, tail))}
        case _:
          Answer12{want(S.fam, "':=' or 'with'", ts)}
    case ParseFamCtors{+ts, +acc}:
      match ts:
        case T.Token{T.Pipe{}, _} <> T.Token{T.Ident{name}, _} <> rest:
          Answer13{next(List<&2, S.binder>, List<&2, S.fam_ctor>, read_1(dispatch(ParseBinders{rest, []})), bs => tail => read_13(dispatch(ParseFamCtorType{name, bs, tail, acc})))}
        case T.Token{T.Pipe{}, loc} <> _:
          Answer13{L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, loc, "expected a constructor name and ':' after '|'")}
        case _:
          Answer13{done(List<&2, S.fam_ctor>, L.reverse(S.fam_ctor, acc, []), ts)}
    case ParseFamCtorType{+name, +bs, +ts, +acc}:
      match ts:
        case T.Token{T.Colon{}, _} <> rest:
          Answer13{next(S.t, List<&2, S.fam_ctor>, read_0(dispatch(ParseTerm{rest})), ty => tail => read_13(dispatch(ParseFamCtors{tail, S.FamCtor{name, arrows(bs, ty)} <> acc})))}
        case _:
          Answer13{want(List<&2, S.fam_ctor>, "':' after constructor binders", ts)}

def finish(value: parsed(List<&2, S.decl>)) -> L.result(List<&2, S.decl>):
  match value:
    case Fail{e}: Fail{e}
    case Done{F.Pair2{ds, _}}: Done{ds}
def parse_tokens(ts: tokens) -> L.result(List<&2, S.decl>):
  L.bind(tokens, List<&2, S.decl>, Source.tokens(ts), admitted => finish(read_7(dispatch(ParseDecls{admitted, []}))))
def parse_bytes(source: List<&2, U32>) -> L.result(List<&2, S.decl>):
  L.bind(tokens, List<&2, S.decl>, L.lex_bytes(source), ts => parse_tokens(ts))
def parse(source: String) -> L.result(List<&2, S.decl>):
  L.bind(tokens, List<&2, S.decl>, L.lex(source), ts => parse_tokens(ts))
