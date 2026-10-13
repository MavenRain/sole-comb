/* lib/surface_lexer.c: twin of surface/lexer.bend (S5 session 1).
   Production lexer, based on Kanon 69f3be51 with native elim punctuation.
   L.result(A) is an int return (1 = Done with *out, 0 = Fail with *error).
   The source bytes (Bend List<U32>) are a Text slice: text_tail and
   text_drop are O(1). The @unsafe CPS defs span, byte_literal and go are
   loops (PORT-PLAN hazards 1 and 2). L.choose, L.maybe, L.bind, with_span
   and with_bytes have no C twin: they are C if statements and returns. */
#include "sole.h"

/* error (line 13). */
const KError *surface_lexer_error(SurfaceLoc position, Text message) {
  return error_parse(message, position.line, position.col);
}

static int fail(SurfaceLoc position, Text message, const KError **error) {
  *error = surface_lexer_error(position, message);
  return 0;
}

static int fail_c(SurfaceLoc position, const char *message,
                  const KError **error) {
  return fail(position, text_of_cstring(message), error);
}

/* digit, ident_start, ident_char (lines 20 to 23). */
static int digit(U32 c) { return c >= 48 && c <= 57; }

static int ident_start(U32 c) {
  return (c >= 97 && c <= 122) || (c >= 65 && c <= 90) || c == 95;
}

static int ident_char(U32 c) { return ident_start(c) || digit(c) || c == 39; }

/* The three ~p arguments of span in go (lines 131, 147, 152 and 156) are a
   closed sum. No function pointer. */
typedef enum { SPAN_COMMENT, SPAN_DIGITS, SPAN_IDENT } SpanKind;

static int span_takes(SpanKind kind, U32 c) {
  switch (kind) {
    case SPAN_COMMENT: return c != 10;
    case SPAN_DIGITS: return digit(c);
    case SPAN_IDENT: return ident_char(c);
  }
  return 0;
}

/* span_result (line 7). */
typedef struct {
  Text taken;
  SurfaceLoc loc;
  Text rest;
} Span;

/* span (line 49, @unsafe): a loop. reverse_text(acc, "") is the taken
   prefix of cs, thus the C twin returns that prefix as a slice in place of
   the reversed accumulator. loc advances by 1 for each taken byte. */
static Span span(SpanKind kind, SurfaceLoc loc, Text cs) {
  Span out;
  Text rest = cs;
  U32 count = 0;
  while (!text_is_end(rest) && span_takes(kind, text_head(rest))) {
    loc = surface_token_advance(loc, 1);
    rest = text_tail(rest);
    count++;
  }
  out.taken = text_take(cs, count);
  out.loc = loc;
  out.rest = rest;
  return out;
}

/* number (line 57). */
static int number(SurfaceLoc loc, Text digits, Bignum *out,
                  const KError **error) {
  if (bignum_of_decimal(digits, out)) return 1;
  return fail_c(loc, "invalid natural literal", error);
}

static const SurfaceTokens *one(SurfaceTokenTag tag, SurfaceLoc loc,
                                const SurfaceTokens *tail) {
  return SurfaceTokens_cons(surface_token(surface_token_kind(tag), loc), tail);
}

/* digits = [c] (one item, equal to c). */
static int single(Text digits, U32 c) {
  return digits.size == 1 && text_head(digits) == c;
}

/* dot_tokens (line 61). */
static int dot_tokens(SurfaceLoc loc, Text digits, const SurfaceTokens **out,
                      const KError **error) {
  Bignum n;
  if (text_is_end(digits)) {
    *out = one(TOKEN_DOT, loc, NULL);
    return 1;
  }
  if (single(digits, 49)) {
    *out = one(TOKEN_DOT1, loc, NULL);
    return 1;
  }
  if (single(digits, 50)) {
    *out = one(TOKEN_DOT2, loc, NULL);
    return 1;
  }
  if (!number(loc, digits, &n, error)) return 0;
  *out = one(TOKEN_DOT, loc,
             SurfaceTokens_cons(surface_token(surface_token_number(n),
                                              surface_token_advance(loc, 1)),
                                NULL));
  return 1;
}

/* hex_digit (line 70): 1 = Some with *out. */
int surface_lexer_hex_digit(U32 c, U32 *out) {
  if (digit(c)) {
    *out = c - 48;
    return 1;
  }
  if (c >= 97 && c <= 102) {
    *out = c - 87;
    return 1;
  }
  if (c >= 65 && c <= 70) {
    *out = c - 55;
    return 1;
  }
  return 0;
}

/* simple_escape (line 75): 1 = Some with *out. */
static int simple_escape(U32 c, U32 *out) {
  switch (c) {
    case 110: *out = 10; return 1;
    case 114: *out = 13; return 1;
    case 116: *out = 9; return 1;
    case 48: *out = 0; return 1;
    case 92: *out = 92; return 1;
    case 34: *out = 34; return 1;
    default: return 0;
  }
}

/* bytes_result (line 9). */
typedef struct {
  Text bytes;
  SurfaceLoc loc;
  Text rest;
} ByteLiteral;

static const char hex_message[] = "byte escape requires two hexadecimal digits";

/* byte_literal (line 85, @unsafe): a loop. acc is a TextBuilder in the
   output order in place of the reversed list and reverse(U32, acc, []). The
   cases are in the Bend order. */
static int byte_literal(SurfaceLoc loc, Text cs, ByteLiteral *out,
                        const KError **error) {
  TextBuilder acc = builder_new();
  while (1) {
    U32 c;
    U32 hi;
    U32 lo;
    U32 b;
    if (text_is_end(cs)) return fail_c(loc, "unterminated byte literal", error);
    c = text_head(cs);
    if (c == 34) {
      out->bytes = builder_text(&acc);
      out->loc = surface_token_advance(loc, 1);
      out->rest = text_tail(cs);
      return 1;
    }
    if (c == 10 || c == 13)
      return fail_c(loc, "unescaped newline in byte literal", error);
    if (c == 92 && cs.size >= 4 && text_head(text_tail(cs)) == 120) {
      if (!surface_lexer_hex_digit(text_head(text_drop(cs, 2)), &hi))
        return fail_c(loc, hex_message, error);
      if (!surface_lexer_hex_digit(text_head(text_drop(cs, 3)), &lo))
        return fail_c(loc, hex_message, error);
      builder_push(&acc, hi * 16 + lo);
      loc = surface_token_advance(loc, 4);
      cs = text_drop(cs, 4);
      continue;
    }
    if (c == 92 && cs.size >= 2) {
      if (!simple_escape(text_head(text_tail(cs)), &b))
        return fail(loc,
                    text_append(text_of_cstring("invalid byte escape \\"),
                                text_take(text_tail(cs), 1)),
                    error);
      builder_push(&acc, b);
      loc = surface_token_advance(loc, 2);
      cs = text_drop(cs, 2);
      continue;
    }
    if (c == 92) return fail_c(loc, "unterminated byte escape", error);
    builder_push(&acc, c);
    loc = surface_token_advance(loc, 1);
    cs = text_tail(cs);
  }
}

/* escaped_char (line 106): OCaml %C escapes a byte, including decimal
   escapes for non-ASCII bytes. U32 divisors 100 and 10 (literals). */
static Text escaped_char(U32 c) {
  TextBuilder text;
  switch (c) {
    case 39: return text_of_cstring("\\'");
    case 92: return text_of_cstring("\\\\");
    case 10: return text_of_cstring("\\n");
    case 13: return text_of_cstring("\\r");
    case 9: return text_of_cstring("\\t");
    case 8: return text_of_cstring("\\b");
    default: break;
  }
  text = builder_new();
  if (c >= 32 && c <= 126) {
    builder_push(&text, c);
    return builder_text(&text);
  }
  builder_push(&text, 92);
  builder_push(&text, 48 + c / 100);
  builder_push(&text, 48 + (c / 10) % 10);
  builder_push(&text, 48 + c % 10);
  return builder_text(&text);
}

/* The fixed punctuation of go (lines 133 to 145). The two-byte cases come
   first; no two-byte case shares its first byte with an earlier one-byte
   case, thus the Bend order holds. */
static int punctuation(Text cs, SurfaceTokenTag *tag, Nat *width) {
  U32 c = text_head(cs);
  U32 d = cs.size >= 2 ? text_head(text_tail(cs)) : 0;
  *width = 2;
  if (c == 45 && d == 62) { *tag = TOKEN_ARROW; return 1; }
  if (c == 61 && d == 62) { *tag = TOKEN_DARROW; return 1; }
  if (c == 58 && d == 61) { *tag = TOKEN_COLON_EQ; return 1; }
  if (c == 40 && d == 41) { *tag = TOKEN_UNIT; return 1; }
  *width = 1;
  switch (c) {
    case 58: *tag = TOKEN_COLON; return 1;
    case 40: *tag = TOKEN_LPAREN; return 1;
    case 41: *tag = TOKEN_RPAREN; return 1;
    case 123: *tag = TOKEN_LBRACE; return 1;
    case 125: *tag = TOKEN_RBRACE; return 1;
    case 59: *tag = TOKEN_SEMICOLON; return 1;
    case 42: *tag = TOKEN_STAR; return 1;
    case 44: *tag = TOKEN_COMMA; return 1;
    case 124: *tag = TOKEN_PIPE; return 1;
    default: return 0;
  }
}

/* The kind of an identifier word (line 157). */
static SurfaceKind word_kind(Text word) {
  if (text_same(word, text_of_cstring("elim")))
    return surface_token_kind(TOKEN_KELIM);
  if (text_same(word, text_of_cstring("else")))
    return surface_token_kind(TOKEN_KELSE);
  return surface_token_ident_kind(word);
}

static Text unexpected(U32 c) {
  return text_append(text_of_cstring("unexpected character '"),
                     text_append(escaped_char(c), text_of_cstring("'")));
}

/* lex_bytes (line 174) = go(T.start, src, []) (line 119, @unsafe): a loop.
   acc is the reversed token list; the end reverses Eof <> acc. A byte 0
   sentinel for a missing second byte matches no two-byte case. */
int surface_lexer_lex_bytes(Text source, const SurfaceTokens **out,
                            const KError **error) {
  SurfaceLoc loc = surface_token_start();
  Text cs = source;
  const SurfaceTokens *acc = NULL;
  while (1) {
    U32 c;
    SurfaceTokenTag tag;
    Nat width;
    Span s;
    ByteLiteral literal;
    const SurfaceTokens *tokens;
    Bignum n;
    if (text_is_end(cs)) {
      *out = SurfaceTokens_reverse(one(TOKEN_EOF, loc, acc));
      return 1;
    }
    c = text_head(cs);
    if (c == 98 && cs.size >= 2 && text_head(text_tail(cs)) == 34) {
      if (!byte_literal(surface_token_advance(loc, 2), text_drop(cs, 2),
                        &literal, error))
        return 0;
      acc = SurfaceTokens_cons(
          surface_token(surface_token_bytes(literal.bytes), loc), acc);
      loc = literal.loc;
      cs = literal.rest;
      continue;
    }
    if (c == 32 || c == 9 || c == 13) {
      loc = surface_token_advance(loc, 1);
      cs = text_tail(cs);
      continue;
    }
    if (c == 10) {
      loc = surface_token_next_line(loc);
      cs = text_tail(cs);
      continue;
    }
    if (c == 45 && cs.size >= 2 && text_head(text_tail(cs)) == 45) {
      s = span(SPAN_COMMENT, surface_token_advance(loc, 2), text_drop(cs, 2));
      loc = s.loc;
      cs = s.rest;
      continue;
    }
    if (punctuation(cs, &tag, &width)) {
      acc = one(tag, loc, acc);
      loc = surface_token_advance(loc, width);
      cs = text_drop(cs, (U32)width);
      continue;
    }
    if (c == 46) {
      s = span(SPAN_DIGITS, surface_token_advance(loc, 1), text_tail(cs));
      if (!dot_tokens(loc, s.taken, &tokens, error)) return 0;
      acc = SurfaceTokens_reverse_onto(tokens, acc);
      loc = s.loc;
      cs = s.rest;
      continue;
    }
    if (digit(c)) {
      s = span(SPAN_DIGITS, loc, cs);
      if (!number(loc, s.taken, &n, error)) return 0;
      acc = SurfaceTokens_cons(surface_token(surface_token_number(n), loc), acc);
      loc = s.loc;
      cs = s.rest;
      continue;
    }
    if (ident_start(c)) {
      s = span(SPAN_IDENT, loc, cs);
      acc = SurfaceTokens_cons(surface_token(word_kind(s.taken), loc), acc);
      loc = s.loc;
      cs = s.rest;
      continue;
    }
    return fail(loc, unexpected(c), error);
  }
}

/* utf8 (line 173): utf8_rev, utf8_char and the two reverses are one loop
   that pushes the UTF-8 bytes of each Chr in order. U32 divisors 64, 4096
   and 262144 (literals). */
static Text utf8(Text src) {
  TextBuilder bytes = builder_new();
  for (; !text_is_end(src); src = text_tail(src)) {
    U32 c = text_head(src);
    if (c <= 127) {
      builder_push(&bytes, c);
      continue;
    }
    if (c <= 2047) {
      builder_push(&bytes, 192 + c / 64);
      builder_push(&bytes, 128 + c % 64);
      continue;
    }
    if (c <= 65535) {
      builder_push(&bytes, 224 + c / 4096);
      builder_push(&bytes, 128 + (c / 64) % 64);
      builder_push(&bytes, 128 + c % 64);
      continue;
    }
    builder_push(&bytes, 240 + c / 262144);
    builder_push(&bytes, 128 + (c / 4096) % 64);
    builder_push(&bytes, 128 + (c / 64) % 64);
    builder_push(&bytes, 128 + c % 64);
  }
  return builder_text(&bytes);
}

/* lex (line 175). */
int surface_lexer_lex(Text source, const SurfaceTokens **out,
                      const KError **error) {
  return surface_lexer_lex_bytes(utf8(source), out, error);
}
