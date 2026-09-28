import Base
import ./foundation.bend as F

# Use these constructors; Stage A succ retains the pin's signed-int wrap.
def t() -> Data:
  F.Bignum.t
def zero() -> t:
  F.Bignum.zero
def one() -> t:
  F.Bignum.one
def succ(level: t) -> t:
  F.Bignum.succ_int63(level)
def max(+a: t, +b: t) -> t:
  F.choose(t, F.Order.le(F.Bignum.compare(a, b)), b, a)
def equal(a: t, b: t) -> Bool:
  F.Bignum.equal(a, b)
def le(a: t, b: t) -> Bool:
  F.Order.le(F.Bignum.compare(a, b))
def of_int(+n: F.Int63.t) -> Maybe<&2, t>:
  F.maybe_when(t, le(zero, F.Bignum.of_int(n)), F.Bignum.of_int(n))
def of_nat(n: Nat) -> t:
  F.Bignum.from_nat(n)
def to_string(level: t) -> String:
  F.Bignum.to_string(level)
