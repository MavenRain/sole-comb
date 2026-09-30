# Production lexer, based on Kanon 69f3be51 with native elim punctuation.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_error.bend as E
import token.bend as T

type span_result is Data:
  Span{taken: String, loc: T.loc, rest: List<&2, U32>}
type bytes_result is Data:
  ByteLiteral{bytes: List<&2, U32>, loc: T.loc, rest: List<&2, U32>}

def result(-A: Data) -> Data: Result<&2, &2, E.t, A>
def error(-A: Data, position: T.loc, message: String) -> result(A):
  match position:
    case T.Loc{line, col}: Fail{E.Parse{message, line, col}}
def reverse(-A: Data, xs: List<&2, A>, acc: List<&2, A>) -> List<&2, A>:
  match xs:
    case []: acc
    case x <> rest: reverse(A, rest, x <> acc)
def digit(+c: U32) -> Bool: Bool.and((c >= 48 : U32), (c <= 57 : U32))
def ident_start(+c: U32) -> Bool:
  Bool.or(Bool.or(Bool.and((c >= 97 : U32), (c <= 122 : U32)), Bool.and((c >= 65 : U32), (c <= 90 : U32))), U32.is_eq(c, 95))
def ident_char(+c: U32) -> Bool: Bool.or(ident_start(c), Bool.or(digit(c), U32.is_eq(c, 39)))

def choose(-A: Data, flag: Bool, yes: Unit -> A, no: Unit -> A) -> A:
  match flag:
    case True{}: yes(Unit{})
    case False{}: no(Unit{})
def maybe(-A: Data, -B: Data, value: Maybe<&2, A>, no: Unit -> B, yes: A -> B) -> B:
  match value:
    case None{}: no(Unit{})
    case Some{x}: yes(x)
def bind(-A: Data, -B: Data, value: result(A), next: A -> result(B)) -> result(B):
  match value:
    case Fail{e}: Fail{e}
    case Done{x}: next(x)
def with_span(-A: Data, value: span_result, next: String -> T.loc -> List<&2, U32> -> A) -> A:
  match value:
    case Span{digits, loc, rest}: next(digits, loc, rest)
def with_bytes(-A: Data, value: bytes_result, next: List<&2, U32> -> T.loc -> List<&2, U32> -> A) -> A:
  match value:
    case ByteLiteral{bytes, loc, rest}: next(bytes, loc, rest)
def reverse_text(src: String, acc: String) -> String:
  match src:
    case SNil{}: acc
    case SCon{c, rest}: reverse_text(rest, SCon{c, acc})

@unsafe
def span(~p: U32 -> Bool, +loc: T.loc, cs: List<&2, U32>, +acc: String) -> span_result:
  match cs:
    case []: Span{reverse_text(acc, ""), loc, []}
    case +c <> +rest:
      choose(span_result, p(c),
        _ => span(~(p), T.advance(loc, 1n), rest, SCon{Chr{c}, acc}),
        _ => Span{reverse_text(acc, ""), loc, c <> rest})

def number(+loc: T.loc, digits: String) -> result(F.Bignum.t):
  maybe(F.Bignum.t, result(F.Bignum.t), F.Bignum.of_decimal(digits),
    _ => error(F.Bignum.t, loc, "invalid natural literal"), n => Done{n})

def dot_tokens(+loc: T.loc, +digits: String) -> result(List<&2, T.t>):
  match digits:
    case SNil{}: Done{[T.Token{T.Dot{}, loc}]}
    case SCon{Chr{49}, SNil{}}: Done{[T.Token{T.Dot1{}, loc}]}
    case SCon{Chr{50}, SNil{}}: Done{[T.Token{T.Dot2{}, loc}]}
    case SCon{_, _}:
      bind(F.Bignum.t, List<&2, T.t>, number(loc, digits),
        n => Done{[T.Token{T.Dot{}, loc}, T.Token{T.Number{n}, T.advance(loc, 1n)}]})

def hex_digit(+c: U32) -> Maybe<&2, U32>:
  F.choose(Maybe<&2, U32>, digit(c), Some{(c - 48 : U32)},
    F.choose(Maybe<&2, U32>, Bool.and((c >= 97 : U32), (c <= 102 : U32)), Some{(c - 87 : U32)},
      F.choose(Maybe<&2, U32>, Bool.and((c >= 65 : U32), (c <= 70 : U32)), Some{(c - 55 : U32)}, None{})))

def simple_escape(c: U32) -> Maybe<&2, U32>:
  match c:
    case 110: Some{10}
    case 114: Some{13}
    case 116: Some{9}
    case 48: Some{0}
    case 92: Some{92}
    case 34: Some{34}
    case _: None{}
@unsafe
def byte_literal(+loc: T.loc, cs: List<&2, U32>, +acc: List<&2, U32>) -> result(bytes_result):
  match cs:
    case []: error(bytes_result, loc, "unterminated byte literal")
    case 34 <> rest: Done{ByteLiteral{reverse(U32, acc, []), T.advance(loc, 1n), rest}}
    case 10 <> _: error(bytes_result, loc, "unescaped newline in byte literal")
    case 13 <> _: error(bytes_result, loc, "unescaped newline in byte literal")
    case 92 <> 120 <> a <> +b <> +rest:
      maybe(U32, result(bytes_result), hex_digit(a),
        _ => error(bytes_result, loc, "byte escape requires two hexadecimal digits"),
        hi => maybe(U32, result(bytes_result), hex_digit(b),
          _ => error(bytes_result, loc, "byte escape requires two hexadecimal digits"),
          lo => byte_literal(T.advance(loc, 4n), rest, (hi * 16 + lo : U32) <> acc)))
    case 92 <> +c <> +rest:
      maybe(U32, result(bytes_result), simple_escape(c),
        _ => error(bytes_result, loc, String.append("invalid byte escape \\", SCon{Chr{c}, SNil{}})),
        b => byte_literal(T.advance(loc, 2n), rest, b <> acc))
    case 92 <> []: error(bytes_result, loc, "unterminated byte escape")
    case c <> rest: byte_literal(T.advance(loc, 1n), rest, c <> acc)

# OCaml %C escapes a byte, including decimal escapes for non-ASCII bytes.

def escaped_char(+c: U32) -> String:
  match c:
    case 39: "\\'"
    case 92: "\\\\"
    case 10: "\\n"
    case 13: "\\r"
    case 9: "\\t"
    case 8: "\\b"
    case _:
      F.choose(String, Bool.and((c >= 32 : U32), (c <= 126 : U32)), SCon{Chr{c}, SNil{}},
        SCon{Chr{92}, SCon{Chr{(48 + c / 100 : U32)}, SCon{Chr{(48 + (c / 10 : U32) % 10 : U32)}, SCon{Chr{(48 + c % 10 : U32)}, SNil{}}}}})

@unsafe
def go(+loc: T.loc, +cs: List<&2, U32>, +acc: List<&2, T.t>) -> result(List<&2, T.t>):
  match cs:
    case []: Done{reverse(T.t, T.Token{T.Eof{}, loc} <> acc, [])}
    case 98 <> 34 <> rest:
      bind(bytes_result, List<&2, T.t>, byte_literal(T.advance(loc, 2n), rest, []),
        value => with_bytes(result(List<&2, T.t>), value,
          bytes => next => tail => go(next, tail, T.Token{T.Bytes{bytes}, loc} <> acc)))
    case 32 <> rest: go(T.advance(loc, 1n), rest, acc)
    case 9 <> rest: go(T.advance(loc, 1n), rest, acc)
    case 13 <> rest: go(T.advance(loc, 1n), rest, acc)
    case 10 <> rest: go(T.next_line(loc), rest, acc)
    case 45 <> 45 <> rest:
      with_span(result(List<&2, T.t>), span(~(c => Bool.not(U32.is_eq(c, 10))), T.advance(loc, 2n), rest, ""),
        _ => next => tail => go(next, tail, acc))
    case 45 <> 62 <> rest: go(T.advance(loc, 2n), rest, T.Token{T.Arrow{}, loc} <> acc)
    case 61 <> 62 <> rest: go(T.advance(loc, 2n), rest, T.Token{T.DArrow{}, loc} <> acc)
    case 58 <> 61 <> rest: go(T.advance(loc, 2n), rest, T.Token{T.ColonEq{}, loc} <> acc)
    case 58 <> rest: go(T.advance(loc, 1n), rest, T.Token{T.Colon{}, loc} <> acc)
    case 40 <> 41 <> rest: go(T.advance(loc, 2n), rest, T.Token{T.Unit{}, loc} <> acc)
    case 40 <> rest: go(T.advance(loc, 1n), rest, T.Token{T.LParen{}, loc} <> acc)
    case 41 <> rest: go(T.advance(loc, 1n), rest, T.Token{T.RParen{}, loc} <> acc)
    case 123 <> rest: go(T.advance(loc, 1n), rest, T.Token{T.LBrace{}, loc} <> acc)
    case 125 <> rest: go(T.advance(loc, 1n), rest, T.Token{T.RBrace{}, loc} <> acc)
    case 59 <> rest: go(T.advance(loc, 1n), rest, T.Token{T.Semicolon{}, loc} <> acc)
    case 42 <> rest: go(T.advance(loc, 1n), rest, T.Token{T.Star{}, loc} <> acc)
    case 44 <> rest: go(T.advance(loc, 1n), rest, T.Token{T.Comma{}, loc} <> acc)
    case 124 <> rest: go(T.advance(loc, 1n), rest, T.Token{T.Pipe{}, loc} <> acc)
    case 46 <> rest:
      with_span(result(List<&2, T.t>), span(~(c => digit(c)), T.advance(loc, 1n), rest, ""),
        digits => next => tail => bind(List<&2, T.t>, List<&2, T.t>, dot_tokens(loc, digits),
          tokens => go(next, tail, reverse(T.t, tokens, acc))))
    case +c <> _:
      choose(result(List<&2, T.t>), digit(c),
        _ => with_span(result(List<&2, T.t>), span(~(c => digit(c)), loc, cs, ""),
          digits => next => tail => bind(F.Bignum.t, List<&2, T.t>, number(loc, digits),
            n => go(next, tail, T.Token{T.Number{n}, loc} <> acc))),
        _ => choose(result(List<&2, T.t>), ident_start(c),
          _ => with_span(result(List<&2, T.t>), span(~(c => ident_char(c)), loc, cs, ""),
            +word => next => tail => go(next, tail, T.Token{F.choose(T.kind, String.eq(word, "elim"), T.KElim{}, F.choose(T.kind, String.eq(word, "else"), T.KElse{}, T.ident_kind(word))), loc} <> acc)),
          _ => error(List<&2, T.t>, loc, String.append("unexpected character '", String.append(escaped_char(c), "'")))))

# Bend strings contain Unicode scalars; positions count UTF-8 bytes.

def utf8_char(+c: U32, +tail: List<&2, U32>) -> List<&2, U32>:
  F.choose(List<&2, U32>, (c <= 127 : U32), c <> tail,
    F.choose(List<&2, U32>, (c <= 2047 : U32), (192 + c / 64 : U32) <> (128 + c % 64 : U32) <> tail,
      F.choose(List<&2, U32>, (c <= 65535 : U32),
        (224 + c / 4096 : U32) <> (128 + (c / 64 : U32) % 64 : U32) <> (128 + c % 64 : U32) <> tail,
        (240 + c / 262144 : U32) <> (128 + (c / 4096 : U32) % 64 : U32) <> (128 + (c / 64 : U32) % 64 : U32) <> (128 + c % 64 : U32) <> tail)))

def utf8_rev(src: String, acc: List<&2, U32>) -> List<&2, U32>:
  match src:
    case SNil{}: acc
    case SCon{Chr{c}, rest}: utf8_rev(rest, reverse(U32, utf8_char(c, []), acc))
def utf8(src: String) -> List<&2, U32>: reverse(U32, utf8_rev(src, []), [])
def lex_bytes(src: List<&2, U32>) -> result(List<&2, T.t>): go(T.start, src, [])
def lex(src: String) -> result(List<&2, T.t>): lex_bytes(utf8(src))
