# Kernel Elim dispatch, refusing nothing: a leg binding more binders than its recursive constructor's fields routes.
import Base
import ./foundation.bend as F
import ./kernel_positivity.bend as P
import ./kernel_quantity.bend as Q
import ./kernel_rules.bend as R
import ./kernel_term.bend as T

def binders(lg: T.leg) -> F.Int63.t:
  match lg:
    case T.Leg{bs, _}: R.length(F.Pair2<Q.t, String>, bs)

def fields(lg: T.leg, ct: Maybe<&2, P.ctor>) -> Bool:
  match ct:
    case None{}: False{}
    case Some{P.Ctor{_, args, _, _, self}}:
      Bool.and(self, R.less_than(F.Int63.compare(R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, args), binders(lg))))

def at(fam: P.family, lg: T.leg, name: Maybe<&2, String>) -> Bool:
  match name:
    case None{}: False{}
    case Some{n}: fields(lg, P.ctor_of(n, fam))

def legs(+fam: P.family, branches: List<&2, F.Pair2<T.addr, T.leg>>) -> Bool:
  match branches:
    case []: False{}
    case F.Pair2{address, lg} <> rest: Bool.or(at(fam, lg, T.as_actor(address)), legs(fam, rest))

def routes(fam: Maybe<&2, P.family>, branches: List<&2, F.Pair2<T.addr, T.leg>>) -> Bool:
  match fam:
    case None{}: False{}
    case Some{f}: legs(f, branches)
