import Base
import ./foundation.bend as F

# LInt remains during Stage A pin compatibility; NAT-MU is a later change.
type t is Data:
  LString{value: String}
  LInt{value: F.Bignum.t}

def equal(a: t, b: t) -> Bool:
  match a, b:
    case LString{x}, LString{y}:
      String.eq(x, y)
    case LInt{x}, LInt{y}:
      F.Bignum.equal(x, y)
    case LString{x}, LInt{y}:
      False{}
    case LInt{x}, LString{y}:
      False{}
