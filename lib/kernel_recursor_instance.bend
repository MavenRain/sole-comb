# Validate the parameter diagram before extracting the values of a recursor instance.
import Base
import ./foundation.bend as F
import ./kernel_check.bend as C
import ./kernel_error.bend as E
import ./kernel_positivity.bend as P
import ./kernel_quantity.bend as Q
import ./kernel_recursor.bend as Rec
import ./kernel_rules.bend as R
import ./kernel_shape.bend as S
import ./kernel_term.bend as T
import ./kernel_value.bend as V

def nullary(legs: List<&2, V.vleg>) -> Bool:
  match legs:
    case []: True{}
    case V.VLeg{[], _} <> rest: nullary(rest)
    case V.VLeg{_ <> _, _} <> _: False{}

def checked(+c: C.ctx, +width: F.Int63.t, pair: F.Pair2<S.t<V.t>, List<&2, V.vleg>>) -> R.result(List<&2, V.t>):
  match pair:
    case F.Pair2{shape, +legs}:
      R.bind(F.Int63.t, List<&2, V.t>, R.require(F.Int63.t, R.coll_width(V.t, shape), E.Mismatch{R.diagram_msg}), actual =>
        Q.if_else(R.result(List<&2, V.t>), Bool.and(F.Int63.equal(actual, width), Bool.and(F.Int63.equal(R.length(V.vleg, legs), width), nullary(legs))),
          _ => Rec.rules(List<&2, V.t>, c, R.mu_param_legs(C.ctx, C.ops, c, legs, [])),
          _ => Fail{E.Mismatch{R.diagram_msg}}))

def section(+c: C.ctx, width: F.Int63.t, value: V.t) -> R.result(List<&2, V.t>):
  R.bind(F.Pair2<S.t<V.t>, List<&2, V.vleg>>, List<&2, V.t>, R.require(F.Pair2<S.t<V.t>, List<&2, V.vleg>>, V.as_sec(value), E.Mismatch{R.diagram_msg}), pair => checked(c, width, pair))

def environment(+c: C.ctx, params: P.telescope, d: V.closure) -> R.result(List<&2, V.t>):
  R.bind(V.t, List<&2, V.t>, Rec.rules(V.t, c, R.c_open(C.ctx, C.ops, c, d, [])), value => section(c, R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, params), value))
