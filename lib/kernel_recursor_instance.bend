# Validate the parameter diagram before extracting the values of a recursor instance.
import Base
import ./foundation.bend as F
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

def checked(~C: Data, ~op: R.check_ops<C>, +c: C, +width: F.Int63.t, pair: F.Pair2<S.t<V.t>, List<&2, V.vleg>>) -> R.program<C, List<&2, V.t>>:
  match pair:
    case F.Pair2{shape, +legs}:
      R.pbind(C, F.Int63.t, List<&2, V.t>, R.crequire(C, F.Int63.t, R.coll_width(V.t, shape), E.Mismatch{R.diagram_msg}), actual =>
        R.pchoose(C, List<&2, V.t>, Bool.and(F.Int63.equal(actual, width), Bool.and(F.Int63.equal(R.length(V.vleg, legs), width), nullary(legs))),
          _ => R.mu_param_legs(C, op, c, legs, []),
          _ => R.Abort{E.Mismatch{R.diagram_msg}}))

def section(~C: Data, ~op: R.check_ops<C>, +c: C, width: F.Int63.t, value: V.t) -> R.program<C, List<&2, V.t>>:
  R.pbind(C, F.Pair2<S.t<V.t>, List<&2, V.vleg>>, List<&2, V.t>, R.crequire(C, F.Pair2<S.t<V.t>, List<&2, V.vleg>>, V.as_sec(value), E.Mismatch{R.diagram_msg}), pair => checked(C, op, c, width, pair))

def environment(~C: Data, ~op: R.check_ops<C>, +c: C, params: P.telescope, d: V.closure) -> R.program<C, List<&2, V.t>>:
  R.pbind(C, V.t, List<&2, V.t>, R.c_open(C, op, c, d, []), value => section(C, op, c, R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, params), value))
