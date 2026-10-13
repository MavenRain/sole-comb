/* lib/surface_token.c: twin of surface/token.bend (S5 session 1).
   Production tokens, based on Kanon 69f3be51 with native elim delimiters.
   The keyword list (record first, then the core keywords) is one static
   table; lookup is a loop over it in the Bend order. */
#include "sole.h"

SurfaceKind surface_token_kind(SurfaceTokenTag tag) {
  SurfaceKind kind;
  kind.tag = tag;
  kind.as.number.negative = NO;
  kind.as.number.limbs = NULL;
  kind.as.number.size = 0;
  return kind;
}

SurfaceKind surface_token_ident(Text value) {
  SurfaceKind kind = surface_token_kind(TOKEN_IDENT);
  kind.as.ident = value;
  return kind;
}

SurfaceKind surface_token_number(Bignum value) {
  SurfaceKind kind = surface_token_kind(TOKEN_NUMBER);
  kind.as.number = value;
  return kind;
}

SurfaceKind surface_token_bytes(Text value) {
  SurfaceKind kind = surface_token_kind(TOKEN_BYTES);
  kind.as.bytes = value;
  return kind;
}

SurfaceToken surface_token(SurfaceKind kind, SurfaceLoc loc) {
  SurfaceToken token;
  token.kind = kind;
  token.loc = loc;
  return token;
}

/* start (line 66). */
SurfaceLoc surface_token_start(void) {
  SurfaceLoc loc;
  loc.line = 1;
  loc.col = 1;
  return loc;
}

/* advance (line 67): col + n (Nat). The source is at most
   SOLE_SOURCE_LIMIT bytes, thus the sum stays far below 2^64. */
SurfaceLoc surface_token_advance(SurfaceLoc position, Nat n) {
  SurfaceLoc loc;
  loc.line = position.line;
  loc.col = position.col + n;
  return loc;
}

/* next_line (line 70): line + 1 (Nat), col 1. */
SurfaceLoc surface_token_next_line(SurfaceLoc position) {
  SurfaceLoc loc;
  loc.line = position.line + 1;
  loc.col = 1;
  return loc;
}

static Text two(const char *prefix, Text value) {
  return text_append(text_of_cstring(prefix), value);
}

/* describe (line 73). */
Text surface_token_describe(SurfaceKind kind) {
  switch (kind.tag) {
    case TOKEN_LPAREN: return text_of_cstring("'('");
    case TOKEN_RPAREN: return text_of_cstring("')'");
    case TOKEN_LBRACE: return text_of_cstring("'{'");
    case TOKEN_RBRACE: return text_of_cstring("'}'");
    case TOKEN_SEMICOLON: return text_of_cstring("';'");
    case TOKEN_KELIM: return text_of_cstring("'elim'");
    case TOKEN_KELSE: return text_of_cstring("'else'");
    case TOKEN_KRECORD: return text_of_cstring("'record'");
    case TOKEN_COLON: return text_of_cstring("':'");
    case TOKEN_COLON_EQ: return text_of_cstring("':='");
    case TOKEN_ARROW: return text_of_cstring("'->'");
    case TOKEN_DARROW: return text_of_cstring("'=>'");
    case TOKEN_STAR: return text_of_cstring("'*'");
    case TOKEN_COMMA: return text_of_cstring("','");
    case TOKEN_DOT: return text_of_cstring("'.'");
    case TOKEN_DOT1: return text_of_cstring("'.1'");
    case TOKEN_DOT2: return text_of_cstring("'.2'");
    case TOKEN_PIPE: return text_of_cstring("'|'");
    case TOKEN_UNIT: return text_of_cstring("'()'");
    case TOKEN_KDEF: return text_of_cstring("'def'");
    case TOKEN_KAXIOM: return text_of_cstring("'axiom'");
    case TOKEN_KFUN: return text_of_cstring("'fun'");
    case TOKEN_KINJ: return text_of_cstring("'inj'");
    case TOKEN_KOF: return text_of_cstring("'of'");
    case TOKEN_KCASE: return text_of_cstring("'case'");
    case TOKEN_KMATCH: return text_of_cstring("'match'");
    case TOKEN_KAS: return text_of_cstring("'as'");
    case TOKEN_KRETURN: return text_of_cstring("'return'");
    case TOKEN_KWITH: return text_of_cstring("'with'");
    case TOKEN_KTUPLE: return text_of_cstring("'tuple'");
    case TOKEN_KSUM: return text_of_cstring("'sum'");
    case TOKEN_KPROD: return text_of_cstring("'prod'");
    case TOKEN_KABSURD: return text_of_cstring("'absurd'");
    case TOKEN_KPROP: return text_of_cstring("'Prop'");
    case TOKEN_KTYPE: return text_of_cstring("'Type'");
    case TOKEN_KLET: return text_of_cstring("'let'");
    case TOKEN_KIN: return text_of_cstring("'in'");
    case TOKEN_KAUTO: return text_of_cstring("'auto'");
    case TOKEN_KMU: return text_of_cstring("'mu'");
    case TOKEN_KMUTUAL: return text_of_cstring("'mutual'");
    case TOKEN_KEND: return text_of_cstring("'end'");
    case TOKEN_KNU: return text_of_cstring("'nu'");
    case TOKEN_KAND: return text_of_cstring("'and'");
    case TOKEN_KREC: return text_of_cstring("'rec'");
    case TOKEN_KNAT_ADD: return text_of_cstring("'natAdd'");
    case TOKEN_KNAT_SUB: return text_of_cstring("'natSub'");
    case TOKEN_KNAT_MUL: return text_of_cstring("'natMul'");
    case TOKEN_KNAT_EQ: return text_of_cstring("'natEq'");
    case TOKEN_KNAT_LT: return text_of_cstring("'natLt'");
    case TOKEN_IDENT: return two("identifier ", kind.as.ident);
    case TOKEN_NUMBER: return two("number ", bignum_to_string(kind.as.number));
    case TOKEN_BYTES: return text_of_cstring("byte literal");
    case TOKEN_EOF: return text_of_cstring("end of input");
  }
  return text_end();
}

/* keywords (line 132) = record, then core_keywords (line 129). */
typedef struct {
  const char *word;
  SurfaceTokenTag tag;
} Keyword;

static const Keyword keywords[] = {
  {"record", TOKEN_KRECORD}, {"def", TOKEN_KDEF}, {"axiom", TOKEN_KAXIOM},
  {"fun", TOKEN_KFUN}, {"inj", TOKEN_KINJ}, {"of", TOKEN_KOF},
  {"case", TOKEN_KCASE}, {"match", TOKEN_KMATCH}, {"as", TOKEN_KAS},
  {"return", TOKEN_KRETURN}, {"with", TOKEN_KWITH}, {"tuple", TOKEN_KTUPLE},
  {"sum", TOKEN_KSUM}, {"prod", TOKEN_KPROD}, {"absurd", TOKEN_KABSURD},
  {"Prop", TOKEN_KPROP}, {"Type", TOKEN_KTYPE}, {"let", TOKEN_KLET},
  {"in", TOKEN_KIN}, {"auto", TOKEN_KAUTO}, {"mu", TOKEN_KMU},
  {"mutual", TOKEN_KMUTUAL}, {"end", TOKEN_KEND}, {"nu", TOKEN_KNU},
  {"and", TOKEN_KAND}, {"rec", TOKEN_KREC}, {"natAdd", TOKEN_KNAT_ADD},
  {"natSub", TOKEN_KNAT_SUB}, {"natMul", TOKEN_KNAT_MUL},
  {"natEq", TOKEN_KNAT_EQ}, {"natLt", TOKEN_KNAT_LT}};

#define KEYWORD_COUNT (sizeof keywords / sizeof keywords[0])

/* ident_kind (line 140) = lookup (line 135, a loop): the first keyword
   equal to s, else Ident{s}. */
SurfaceKind surface_token_ident_kind(Text s) {
  const Keyword *word = keywords;
  const Keyword *end = keywords + KEYWORD_COUNT;
  for (; word != end; word++)
    if (text_same(s, text_of_cstring(word->word)) == YES)
      return surface_token_kind(word->tag);
  return surface_token_ident(s);
}
