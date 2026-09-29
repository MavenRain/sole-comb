# Test frontend port of Kanon 69f3be51 surface/token.ml.
import Base
import ../../lib/foundation.bend as F

type loc is Data:
  Loc{line: Nat, col: Nat}

type kind is Data:
  LParen{}
  RParen{}
  Colon{}
  ColonEq{}
  Arrow{}
  DArrow{}
  Star{}
  Comma{}
  Dot{}
  Dot1{}
  Dot2{}
  Pipe{}
  Unit{}
  KDef{}
  KAxiom{}
  KFun{}
  KInj{}
  KOf{}
  KCase{}
  KMatch{}
  KAs{}
  KReturn{}
  KWith{}
  KTuple{}
  KSum{}
  KProd{}
  KAbsurd{}
  KProp{}
  KType{}
  KLet{}
  KIn{}
  KAuto{}
  KMu{}
  KMutual{}
  KEnd{}
  KNu{}
  KAnd{}
  KRec{}
  KNatAdd{}
  KNatSub{}
  KNatMul{}
  KNatEq{}
  KNatLt{}
  Ident{value: String}
  Number{value: F.Bignum.t}
  Bytes{value: List<&2, U32>}
  Eof{}

type t is Data:
  Token{kind: kind, loc: loc}

def start() -> loc: Loc{1n, 1n}
def advance(position: loc, n: Nat) -> loc:
  match position:
    case Loc{line, col}: Loc{line, (col + n : Nat)}
def next_line(position: loc) -> loc:
  match position:
    case Loc{line, _}: Loc{(line + 1n : Nat), 1n}
def describe(k: kind) -> String:
  match k:
    case LParen{}: "'('"
    case RParen{}: "')'"
    case Colon{}: "':'"
    case ColonEq{}: "':='"
    case Arrow{}: "'->'"
    case DArrow{}: "'=>'"
    case Star{}: "'*'"
    case Comma{}: "','"
    case Dot{}: "'.'"
    case Dot1{}: "'.1'"
    case Dot2{}: "'.2'"
    case Pipe{}: "'|'"
    case Unit{}: "'()'"
    case KDef{}: "'def'"
    case KAxiom{}: "'axiom'"
    case KFun{}: "'fun'"
    case KInj{}: "'inj'"
    case KOf{}: "'of'"
    case KCase{}: "'case'"
    case KMatch{}: "'match'"
    case KAs{}: "'as'"
    case KReturn{}: "'return'"
    case KWith{}: "'with'"
    case KTuple{}: "'tuple'"
    case KSum{}: "'sum'"
    case KProd{}: "'prod'"
    case KAbsurd{}: "'absurd'"
    case KProp{}: "'Prop'"
    case KType{}: "'Type'"
    case KLet{}: "'let'"
    case KIn{}: "'in'"
    case KAuto{}: "'auto'"
    case KMu{}: "'mu'"
    case KMutual{}: "'mutual'"
    case KEnd{}: "'end'"
    case KNu{}: "'nu'"
    case KAnd{}: "'and'"
    case KRec{}: "'rec'"
    case KNatAdd{}: "'natAdd'"
    case KNatSub{}: "'natSub'"
    case KNatMul{}: "'natMul'"
    case KNatEq{}: "'natEq'"
    case KNatLt{}: "'natLt'"
    case Ident{s}: String.append("identifier ", s)
    case Number{n}: String.append("number ", F.Bignum.to_string(n))
    case Bytes{_}: "byte literal"
    case Eof{}: "end of input"

def keywords() -> List<&2, F.Pair2<String, kind>>:
  [F.Pair2{"def", KDef{}}, F.Pair2{"axiom", KAxiom{}}, F.Pair2{"fun", KFun{}}, F.Pair2{"inj", KInj{}}, F.Pair2{"of", KOf{}}, F.Pair2{"case", KCase{}}, F.Pair2{"match", KMatch{}}, F.Pair2{"as", KAs{}}, F.Pair2{"return", KReturn{}}, F.Pair2{"with", KWith{}}, F.Pair2{"tuple", KTuple{}}, F.Pair2{"sum", KSum{}}, F.Pair2{"prod", KProd{}}, F.Pair2{"absurd", KAbsurd{}}, F.Pair2{"Prop", KProp{}}, F.Pair2{"Type", KType{}}, F.Pair2{"let", KLet{}}, F.Pair2{"in", KIn{}}, F.Pair2{"auto", KAuto{}}, F.Pair2{"mu", KMu{}}, F.Pair2{"mutual", KMutual{}}, F.Pair2{"end", KEnd{}}, F.Pair2{"nu", KNu{}}, F.Pair2{"and", KAnd{}}, F.Pair2{"rec", KRec{}}, F.Pair2{"natAdd", KNatAdd{}}, F.Pair2{"natSub", KNatSub{}}, F.Pair2{"natMul", KNatMul{}}, F.Pair2{"natEq", KNatEq{}}, F.Pair2{"natLt", KNatLt{}}]

def lookup(+s: String, words: List<&2, F.Pair2<String, kind>>) -> kind:
  match words:
    case []: Ident{s}
    case F.Pair2{word, token} <> rest:
      F.choose(kind, String.eq(s, word), token, lookup(s, rest))
def ident_kind(+s: String) -> kind: lookup(s, keywords)
