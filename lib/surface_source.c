/* lib/surface_source.c: twin of surface/source.bend (S5 session 1).
   Object-language admission before parsing. Comments and byte literals are
   already tokens, so reserved words inside them are not mistaken for syntax.
   tokens (line 7) is non-tail recursion over the list (PORT-PLAN hazard 3).
   The C twin is a loop with a reversed accumulator and one reverse at the
   end. The first refused token in list order gives the error, as in the
   Bend: each recursive call comes after the check of its head. */
#include "sole.h"

static int refuse(SurfaceLoc loc, const char *message, const KError **error) {
  *error = surface_lexer_error(loc, text_of_cstring(message));
  return 0;
}

/* tokens (line 7). */
int surface_source_tokens(const SurfaceTokens *ts, const SurfaceTokens **out,
                          const KError **error) {
  const SurfaceTokens *acc = NULL;
  for (; ts != NULL; ts = ts->tail) {
    SurfaceToken token = ts->head;
    switch (token.kind.tag) {
      case TOKEN_KCASE:
        return refuse(token.loc, "E-R4-MATCH: 'case' is reserved; use elim",
                      error);
      case TOKEN_KMATCH:
        return refuse(token.loc, "E-R4-MATCH: 'match' is reserved; use elim",
                      error);
      case TOKEN_KREC:
        return refuse(token.loc, "E-R4-MATCH: 'rec' is reserved; use elim",
                      error);
      case TOKEN_PIPE:
        return refuse(token.loc,
                      "E-R4-MATCH: branch-binder syntax is reserved; use elim "
                      "arms",
                      error);
      case TOKEN_DOT1:
        return refuse(token.loc, "E-R4-MATCH: '.1' is reserved", error);
      case TOKEN_DOT2:
        return refuse(token.loc, "E-R4-MATCH: '.2' is reserved", error);
      case TOKEN_KNU:
        return refuse(token.loc,
                      "E-SOURCE-NOT-YET: nu declarations are not supported",
                      error);
      case TOKEN_KMUTUAL:
        return refuse(token.loc,
                      "E-SOURCE-NOT-YET: use 'mu ... { ... } and ... { ... }' "
                      "for mutual families",
                      error);
      case TOKEN_KWITH:
        token.kind = surface_token_ident(text_of_cstring("with"));
        break;
      case TOKEN_KABSURD:
        token.kind = surface_token_ident(text_of_cstring("absurd"));
        break;
      case TOKEN_KEND:
        token.kind = surface_token_ident(text_of_cstring("end"));
        break;
      case TOKEN_LPAREN: case TOKEN_RPAREN: case TOKEN_LBRACE:
      case TOKEN_RBRACE: case TOKEN_SEMICOLON: case TOKEN_KELIM:
      case TOKEN_KELSE: case TOKEN_KRECORD: case TOKEN_COLON:
      case TOKEN_COLON_EQ: case TOKEN_ARROW: case TOKEN_DARROW:
      case TOKEN_STAR: case TOKEN_COMMA: case TOKEN_DOT: case TOKEN_UNIT:
      case TOKEN_KDEF: case TOKEN_KAXIOM: case TOKEN_KFUN: case TOKEN_KINJ:
      case TOKEN_KOF: case TOKEN_KAS: case TOKEN_KRETURN: case TOKEN_KTUPLE:
      case TOKEN_KSUM: case TOKEN_KPROD: case TOKEN_KPROP: case TOKEN_KTYPE:
      case TOKEN_KLET: case TOKEN_KIN: case TOKEN_KAUTO: case TOKEN_KMU:
      case TOKEN_KAND: case TOKEN_KNAT_ADD: case TOKEN_KNAT_SUB:
      case TOKEN_KNAT_MUL: case TOKEN_KNAT_EQ: case TOKEN_KNAT_LT:
      case TOKEN_IDENT: case TOKEN_NUMBER: case TOKEN_BYTES: case TOKEN_EOF:
        break;
    }
    acc = SurfaceTokens_cons(token, acc);
  }
  *out = SurfaceTokens_reverse(acc);
  return 1;
}
