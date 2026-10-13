/* test/pinfront/token.c: twin of test/pinfront/token.bend (S7 session 1).
   Test frontend port of Kanon 69f3be51 surface/token.ml. The pinfront
   kinds are the surface kinds without LBrace, RBrace, Semicolon, KElim,
   KElse and KRecord. Thus this twin shares SurfaceTokenTag, SurfaceKind,
   SurfaceToken, SurfaceLoc and the defs kind, ident, number, bytes,
   token, start, advance, next_line and describe with lib/surface_token.c
   (the pinfront lexer makes none of the six kinds). Only keywords (line
   117, the surface core_keywords: no "record") and ident_kind differ. */
#include "sole.h"

typedef struct {
  const char *word;
  SurfaceTokenTag tag;
} Keyword;

static const Keyword keywords[] = {
  {"def", TOKEN_KDEF}, {"axiom", TOKEN_KAXIOM},
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

/* ident_kind (line 125) = lookup (line 120, a loop): the first keyword
   equal to s, else Ident{s}. */
SurfaceKind pinfront_token_ident_kind(Text s) {
  const Keyword *word = keywords;
  const Keyword *end = keywords + KEYWORD_COUNT;
  for (; word != end; word++)
    if (text_same(s, text_of_cstring(word->word)) == YES)
      return surface_token_kind(word->tag);
  return surface_token_ident(s);
}
