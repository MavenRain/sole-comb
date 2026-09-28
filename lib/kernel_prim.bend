# Five Nat primitives from Kanon 69f3be51 prim.ml, before the Stage B delta.
import Base
import ./foundation.bend as F
import ./kernel_error.bend as E
import ./kernel_literal.bend as Lit
import ./kernel_quantity.bend as Q
import ./kernel_rules.bend as R
import ./kernel_term.bend as T
import ./kernel_value.bend as V

type t is Data:
  Nat_add{}
  Nat_sub{}
  Nat_mul{}
  Nat_eq{}
  Nat_lt{}

def name(prim: t) -> String:
  match prim:
    case Nat_add{}: "natAdd"
    case Nat_sub{}: "natSub"
    case Nat_mul{}: "natMul"
    case Nat_eq{}: "natEq"
    case Nat_lt{}: "natLt"

def arity(prim: t) -> Nat:
  match prim:
    case Nat_add{}: 2n
    case Nat_sub{}: 2n
    case Nat_mul{}: 2n
    case Nat_eq{}: 2n
    case Nat_lt{}: 2n

def catalog() -> List<&2, t>: [Nat_add{}, Nat_sub{}, Nat_mul{}, Nat_eq{}, Nat_lt{}]

def lookup(+source: String, prims: List<&2, t>) -> Maybe<&2, t>:
  match prims:
    case []: None{}
    case +prim <> rest: F.choose(Maybe<&2, t>, String.eq(source, name(prim)), Some{prim}, lookup(source, rest))

def of_name(source: String) -> Maybe<&2, t>: lookup(source, catalog)
def nat_name() -> String: "Nat"
def nat_ty() -> T.t: T.Global{nat_name}

def binary(result: T.t) -> T.t:
  R.arrow(Q.Many{}, "a", nat_ty, R.arrow(Q.Many{}, "b", nat_ty, result))

def ty(prim: t) -> T.t:
  match prim:
    case Nat_add{}: binary(nat_ty)
    case Nat_sub{}: binary(nat_ty)
    case Nat_mul{}: binary(nat_ty)
    case Nat_eq{}: binary(R.bool_ty)
    case Nat_lt{}: binary(R.bool_ty)

def nonnegative(sign: Cmp, value: F.Bignum.t) -> Maybe<&2, F.Bignum.t>:
  match sign:
    case LT{}: None{}
    case EQ{}: Some{value}
    case GT{}: Some{value}

def as_nat(literal: Lit.t) -> Maybe<&2, F.Bignum.t>:
  match literal:
    case Lit.LInt{+n}: nonnegative(F.Bignum.sign(n), n)
    case Lit.LString{_}: None{}

def add(a: F.Bignum.t, b: F.Bignum.t) -> Result<&2, &2, E.t, F.Bignum.t>: Done{F.Bignum.add(a, b)}
def sub(a: F.Bignum.t, b: F.Bignum.t) -> Result<&2, &2, E.t, F.Bignum.t>: Done{F.Bignum.sub(a, b)}
def mul(a: F.Bignum.t, b: F.Bignum.t) -> Result<&2, &2, E.t, F.Bignum.t>: Done{F.Bignum.mul(a, b)}

def natural_pair(a: Maybe<&2, F.Bignum.t>, b: Maybe<&2, F.Bignum.t>) -> Maybe<&2, F.Pair2<F.Bignum.t, F.Bignum.t>>:
  match a, b:
    case None{}, None{}: None{}
    case None{}, Some{_}: None{}
    case Some{_}, None{}: None{}
    case Some{x}, Some{y}: Some{F.Pair2{x, y}}

def two_nats(args: List<&2, Lit.t>) -> Maybe<&2, F.Pair2<F.Bignum.t, F.Bignum.t>>:
  match args:
    case []: None{}
    case _ <> []: None{}
    case a <> b <> []: natural_pair(as_nat(a), as_nat(b))
    case _ <> _ <> _ <> _: None{}

# Defunctionalized arithmetic and comparison rows retain the pinned split.
def reduce_nats(prim: t, a: F.Bignum.t, b: F.Bignum.t) -> Maybe<&2, Lit.t>:
  match prim:
    case Nat_add{}: Some{Lit.LInt{F.Bignum.add(a, b)}}
    case Nat_sub{}: Some{Lit.LInt{F.Bignum.sub(a, b)}}
    case Nat_mul{}: Some{Lit.LInt{F.Bignum.mul(a, b)}}
    case Nat_eq{}: None{}
    case Nat_lt{}: None{}

def reduce_pair(prim: t, pair: Maybe<&2, F.Pair2<F.Bignum.t, F.Bignum.t>>) -> Maybe<&2, Lit.t>:
  match pair:
    case None{}: None{}
    case Some{F.Pair2{a, b}}: reduce_nats(prim, a, b)

def reduce(prim: t, args: List<&2, Lit.t>) -> Result<&2, &2, E.t, Maybe<&2, Lit.t>>:
  Done{reduce_pair(prim, two_nats(args))}

def less(order: Cmp) -> Bool:
  match order:
    case LT{}: True{}
    case EQ{}: False{}
    case GT{}: False{}

def apply_nats(prim: t, a: F.Bignum.t, b: F.Bignum.t) -> Maybe<&2, V.t>:
  match prim:
    case Nat_add{}: Some{V.VLit{Lit.LInt{F.Bignum.add(a, b)}}}
    case Nat_sub{}: Some{V.VLit{Lit.LInt{F.Bignum.sub(a, b)}}}
    case Nat_mul{}: Some{V.VLit{Lit.LInt{F.Bignum.mul(a, b)}}}
    case Nat_eq{}: Some{R.bool_value(F.Bignum.equal(a, b))}
    case Nat_lt{}: Some{R.bool_value(less(F.Bignum.compare(a, b)))}

def apply_pair(prim: t, pair: Maybe<&2, F.Pair2<F.Bignum.t, F.Bignum.t>>) -> Maybe<&2, V.t>:
  match pair:
    case None{}: None{}
    case Some{F.Pair2{a, b}}: apply_nats(prim, a, b)

def apply(prim: t, args: List<&2, Lit.t>) -> Result<&2, &2, E.t, Maybe<&2, V.t>>:
  Done{apply_pair(prim, two_nats(args))}
