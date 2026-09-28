# Quantity tags from Kanon 69f3be51 quantity.ml. The usage algebra lands at A.4.
import Base

type t is Data:
  Zero{}
  One{}
  Many{}

def equal(a: t, b: t) -> Bool:
  match a, b:
    case Zero{}, Zero{}: True{}
    case Zero{}, One{}: False{}
    case Zero{}, Many{}: False{}
    case One{}, Zero{}: False{}
    case One{}, One{}: True{}
    case One{}, Many{}: False{}
    case Many{}, Zero{}: False{}
    case Many{}, One{}: False{}
    case Many{}, Many{}: True{}
