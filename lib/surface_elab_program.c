/* lib/surface_elab_program.c: twin of surface/elab_program.bend (S5
   session 3). program<A> is the int return of each surface twin (1 = Pure
   with *out, 0 = Abort with *error). A Request plus its drive is a direct
   call of the walker, so bind (an @unsafe def) and map_result are C if
   statements at each use and lift is the identity. Only request has a C
   twin. */
#include "sole.h"

/* request (line 19): the walker answers the request at once. */
int surface_elab_program_request(const void *walk_env, SurfaceElabWalk go,
                                 const CheckCtx *c, const Value *expected,
                                 const SurfaceTerm *s, const Term **out,
                                 const KError **error) {
  return go(walk_env, c, expected, s, out, error);
}
