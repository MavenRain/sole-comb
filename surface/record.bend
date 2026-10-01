# Closed record declarations expand to a Ran product and qualified Out getters.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_quantity.bend as Q
import token.bend as T
import lexer.bend as L
import syntax.bend as S

def tokens() -> Data: List<&2, T.t>
def parsed(-A: Data) -> Data: L.result(F.Pair2<A, tokens>)
def next(-A: Data, -B: Data, value: parsed(A), k: A -> tokens -> parsed(B)) -> parsed(B):
  match value:
    case Fail{e}: Fail{e}
    case Done{F.Pair2{x, rest}}: k(x, rest)

type field is Data:
  Field{name: String, ty: S.t}

def want(-A: Data, what: String, ts: tokens) -> parsed(A):
  match ts:
    case []: L.error(F.Pair2<A, tokens>, T.start, "unexpected end of record declaration")
    case T.Token{kind, loc} <> _: L.error(F.Pair2<A, tokens>, loc, S.cat(["expected ", what, ", found ", T.describe(kind)]))

def length(ts: tokens) -> Nat:
  match ts:
    case []: 0n
    case _ <> rest: (1n + length(rest) : Nat)

def known(+name: String, fs: List<&2, field>) -> Bool:
  match fs:
    case []: False{}
    case Field{label, _} <> rest: Bool.or(String.eq(name, label), known(name, rest))

def types(fs: List<&2, field>) -> List<&2, S.t>:
  match fs:
    case []: []
    case Field{_, ty} <> rest: ty <> types(rest)

def getter(name: String, label: String, ty: S.t, key: F.Int63.t, +binder: S.binder) -> S.decl:
  S.DDef{S.cat([name, ".", label]), S.SArrow{binder, ty}, S.SFun{[binder], S.SProj{S.SVar{"#record"}, key}}}

def getters(+name: String, fs: List<&2, field>, +key: F.Int63.t) -> List<&2, S.decl>:
  match fs:
    case []: []
    case Field{label, ty} <> rest:
      # '#' cannot occur in a source identifier, so this binder cannot capture
      # a field type's free name, including an ordinary global called value.
      getter(name, label, ty, key, S.Binder{Q.One{}, "#record", S.SVar{name}}) <> getters(name, rest, F.Int63.succ(key))

def expand(+name: String, sort: S.t, +fs: List<&2, field>) -> List<&2, S.decl>:
  S.DDef{name, sort, S.SProd{types(fs)}} <> getters(name, fs, F.Int63.zero)

type field_step is Data:
  ReadFields{fuel: Nat, ts: tokens, acc: List<&2, field>}
  AfterType{fuel: Nat, name: String, acc: List<&2, field>, pair: F.Pair2<S.t, tokens>}
  AfterTail{fuel: Nat, fs: List<&2, field>, tail: tokens}

@unsafe
def fields(~parse: tokens -> parsed(S.t), step: field_step) -> parsed(List<&2, field>):
  match step:
    case ReadFields{fuel, +ts, +acc}:
      match fuel:
        case 0n: want(List<&2, field>, "'}'", ts)
        case 1n+remaining:
          match ts:
            case T.Token{T.RBrace{}, _} <> rest: Done{F.Pair2{L.reverse(field, acc, []), rest}}
            case T.Token{T.Ident{+name}, +loc} <> T.Token{T.Colon{}, _} <> rest:
              L.choose(parsed(List<&2, field>), known(name, acc),
                _ => L.error(F.Pair2<List<&2, field>, tokens>, loc, S.cat(["E-RECORD-FIELD: duplicate field ", name])),
                _ => L.bind(F.Pair2<S.t, tokens>, F.Pair2<List<&2, field>, tokens>, parse(rest), pair => fields(~parse, AfterType{remaining, name, acc, pair})))
            case _: want(List<&2, field>, "a record field 'name : type' or '}'", ts)
    case AfterType{fuel, name, acc, pair}:
      match pair:
        case F.Pair2{ty, tail}: fields(~parse, AfterTail{fuel, Field{name, ty} <> acc, tail})
    case AfterTail{fuel, +fs, +tail}:
      match tail:
        case T.Token{T.Semicolon{}, _} <> next: fields(~parse, ReadFields{fuel, next, fs})
        case T.Token{T.RBrace{}, _} <> next: Done{F.Pair2{L.reverse(field, fs, []), next}}
        case _: want(List<&2, field>, "';' or '}' after a record field", tail)

def declared(name: String, sort: S.t, value: F.Pair2<List<&2, field>, tokens>) -> parsed(List<&2, S.decl>):
  match value:
    case F.Pair2{fs, suffix}: Done{F.Pair2{expand(name, sort, fs), suffix}}

def declaration_sort(~parse: tokens -> parsed(S.t), name: String, pair: F.Pair2<S.t, tokens>) -> parsed(List<&2, S.decl>):
  match pair:
    case F.Pair2{sort, +tail}:
      match tail:
        case T.Token{T.LBrace{}, _} <> +body:
          L.bind(F.Pair2<List<&2, field>, tokens>, F.Pair2<List<&2, S.decl>, tokens>, fields(~parse, ReadFields{length(body), body, []}), value => declared(name, sort, value))
        case _: want(List<&2, S.decl>, "'{' after a record's type annotation", tail)

def declaration(~parse: tokens -> parsed(S.t), ts: tokens) -> parsed(List<&2, S.decl>):
  match ts:
    case T.Token{T.Ident{+name}, _} <> T.Token{T.Colon{}, _} <> rest:
      L.bind(F.Pair2<S.t, tokens>, F.Pair2<List<&2, S.decl>, tokens>, parse(rest), pair => declaration_sort(~parse, name, pair))
    case _: want(List<&2, S.decl>, "'name : Type' after 'record'", ts)

def prepend(ds: List<&2, S.decl>, acc: List<&2, S.decl>) -> List<&2, S.decl>:
  match ds:
    case []: acc
    case d <> rest: prepend(rest, d <> acc)

@unsafe
def declarations(~parse: tokens -> parsed(S.t), ~parse_decl: tokens -> parsed(S.decl), fuel: Nat, +ts: tokens, +acc: List<&2, S.decl>) -> parsed(List<&2, S.decl>):
  match fuel:
    case 0n: want(List<&2, S.decl>, "end of input", ts)
    case 1n+remaining:
      match ts:
        case T.Token{T.Eof{}, _} <> rest: Done{F.Pair2{L.reverse(S.decl, acc, []), rest}}
        case T.Token{T.KRecord{}, _} <> rest:
          next(List<&2, S.decl>, List<&2, S.decl>, declaration(~parse, rest), ds => tail => declarations(~parse, ~parse_decl, remaining, tail, prepend(ds, acc)))
        case _: next(S.decl, List<&2, S.decl>, parse_decl(ts), d => tail => declarations(~parse, ~parse_decl, remaining, tail, d <> acc))

def qualify(term: S.t, field: String, loc: T.loc) -> L.result(S.t):
  match term:
    case S.SVar{name}: Done{S.SVar{S.cat([name, ".", field])}}
    case _: L.error(S.t, loc, "E-RECORD-PROJECTION: use a qualified getter such as Pair.first value")
