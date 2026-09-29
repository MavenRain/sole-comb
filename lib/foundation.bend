import Base

# Stage A host representations. Integer arithmetic follows Kanon 69f3be5.
# Limbs are little endian, each below 32768; zero has no limbs or sign.
# The arithmetic core is adapted from attest/lib/foundation.bend 30dfb2d9.
type Pair2<-A: Data, -B: Data> is Data:
  Pair2{fst: A, snd: B}

type Bignum.t is Data:
  Bignum.Int{negative: Bool, limbs: List<&2, U32>}

# An OCaml 64-bit host int has 63 signed bits. Bend Nat cannot hold it.
# Construct through Bignum.to_int; keep the checked range across conversion.
type Int63.t is Data:
  Int63.Int{negative: Bool, limbs: List<&2, U32>}


def Bignum.trim_zero(h: U32, zero: Bool) -> List<&2, U32>:
  match zero:
    case True{}:
      []
    case False{}:
      [h]

def Bignum.trim_cons(+h: U32, tail: List<&2, U32>) -> List<&2, U32>:
  match tail:
    case Nil{}:
      Bignum.trim_zero(h, U32.is_zero(h))
    case Con{x, xs}:
      Con{h, Con{x, xs}}

def Bignum.trim(xs: List<&2, U32>) -> List<&2, U32>:
  match xs:
    case Nil{}:
      []
    case Con{h, t}:
      Bignum.trim_cons(h, Bignum.trim(t))

def Bignum.make_norm(negative: Bool, limbs: List<&2, U32>) -> Bignum.t:
  match limbs:
    case Nil{}:
      Bignum.Int{False{}, []}
    case Con{h, t}:
      Bignum.Int{negative, Con{h, t}}

def Bignum.make(negative: Bool, limbs: List<&2, U32>) -> Bignum.t:
  Bignum.make_norm(negative, Bignum.trim(limbs))

def Bignum.zero() -> Bignum.t:
  Bignum.Int{False{}, []}

def Bignum.one() -> Bignum.t:
  Bignum.Int{False{}, [1]}

def Bignum.nat_limbs_step(+n: Nat, empty: Bool) -> List<&2, U32>:
  match empty:
    case True{}:
      []
    case False{}:
      [U32.from_nat(n)]

def Bignum.from_u32(+n: U32) -> Bignum.t:
  Bignum.make(False{}, [(n % 32768 : U32), ((n / 32768 : U32) % 32768 : U32), (n / 1073741824 : U32)])

@unsafe
def Bignum.nat_limbs(+n: Nat) -> List<&2, U32>:
  match n:
    case 0n:
      []
    case 1n+p:
      Con{U32.from_nat(((1n+p) % 32768n : Nat)), Bignum.nat_limbs(((1n+p) / 32768n : Nat))}

def Bignum.from_nat(n: Nat) -> Bignum.t:
  Bignum.Int{False{}, Bignum.nat_limbs(n)}

def Bignum.of_int(n: Int63.t) -> Bignum.t:
  match n:
    case Int63.Int{negative, limbs}:
      Bignum.make(negative, limbs)

def Bignum.of_nat(n: Nat) -> Bignum.t:
  Bignum.from_nat(n)

def Bignum.cmp_head(a: U32, b: U32, tails: Cmp) -> Cmp:
  match tails:
    case LT{}:
      LT{}
    case EQ{}:
      U32.cmp(a, b)
    case GT{}:
      GT{}

def Bignum.cmp_abs(xs: List<&2, U32>, ys: List<&2, U32>) -> Cmp:
  match xs:
    case Nil{}:
      match ys:
        case Nil{}:
          EQ{}
        case Con{h, t}:
          LT{}
    case Con{h, t}:
      match ys:
        case Nil{}:
          GT{}
        case Con{k, u}:
          Bignum.cmp_head(h, k, Bignum.cmp_abs(t, u))

def Bignum.cmp_flip(c: Cmp) -> Cmp:
  match c:
    case LT{}:
      GT{}
    case EQ{}:
      EQ{}
    case GT{}:
      LT{}

def Bignum.compare(a: Bignum.t, b: Bignum.t) -> Cmp:
  match a b:
    case Bignum.Int{False{}, al} Bignum.Int{False{}, bl}:
      Bignum.cmp_abs(al, bl)
    case Bignum.Int{True{}, al} Bignum.Int{True{}, bl}:
      Bignum.cmp_flip(Bignum.cmp_abs(al, bl))
    case Bignum.Int{True{}, al} Bignum.Int{False{}, bl}:
      LT{}
    case Bignum.Int{False{}, al} Bignum.Int{True{}, bl}:
      GT{}

def Bignum.is_equal(c: Cmp) -> Bool:
  match c:
    case EQ{}:
      True{}
    case LT{}:
      False{}
    case GT{}:
      False{}

def Bignum.equal(a: Bignum.t, b: Bignum.t) -> Bool:
  Bignum.is_equal(Bignum.compare(a, b))

def Bignum.sign(a: Bignum.t) -> Cmp:
  Bignum.compare(a, Bignum.zero)

def Bignum.negate(a: Bignum.t) -> Bignum.t:
  match a:
    case Bignum.Int{negative, limbs}:
      Bignum.make(Bool.not(negative), limbs)

def Bignum.carry_end(carry: U32, empty: Bool) -> List<&2, U32>:
  match empty:
    case True{}:
      []
    case False{}:
      [carry]

def Bignum.add_carry(xs: List<&2, U32>, +carry: U32) -> List<&2, U32>:
  match xs:
    case Nil{}:
      Bignum.carry_end(carry, U32.is_zero(carry))
    case Con{+h, t}:
      Con{((h + carry) % 32768 : U32), Bignum.add_carry(t, ((h + carry) / 32768 : U32))}

def Bignum.add_abs(xs: List<&2, U32>, ys: List<&2, U32>, +carry: U32) -> List<&2, U32>:
  match xs:
    case Nil{}:
      Bignum.add_carry(ys, carry)
    case Con{+h, t}:
      match ys:
        case Nil{}:
          Con{((h + carry) % 32768 : U32), Bignum.add_carry(t, ((h + carry) / 32768 : U32))}
        case Con{+k, u}:
          Con{((h + k + carry) % 32768 : U32), Bignum.add_abs(t, u, ((h + k + carry) / 32768 : U32))}

def Bignum.borrow(a: U32, b: U32, carry: U32) -> U32:
  Bool.to_u32((a < (b + carry) : U32))

def Bignum.sub_abs(xs: List<&2, U32>, ys: List<&2, U32>, +borrow: U32) -> List<&2, U32>:
  match xs:
    case Nil{}:
      []
    case Con{+h, t}:
      match ys:
        case Nil{}:
          Con{((32768 + h - borrow) % 32768 : U32), Bignum.sub_abs(t, [], Bignum.borrow(h, 0, borrow))}
        case Con{+k, u}:
          Con{((32768 + h - k - borrow) % 32768 : U32), Bignum.sub_abs(t, u, Bignum.borrow(h, k, borrow))}

def Bignum.add_different(negative: Bool, al: List<&2, U32>, bl: List<&2, U32>, order: Cmp) -> Bignum.t:
  match order:
    case EQ{}:
      Bignum.zero
    case GT{}:
      Bignum.make(negative, Bignum.sub_abs(al, bl, 0))
    case LT{}:
      Bignum.make(Bool.not(negative), Bignum.sub_abs(bl, al, 0))

def Bignum.add(a: Bignum.t, b: Bignum.t) -> Bignum.t:
  match a b:
    case Bignum.Int{False{}, al} Bignum.Int{False{}, bl}:
      Bignum.make(False{}, Bignum.add_abs(al, bl, 0))
    case Bignum.Int{True{}, al} Bignum.Int{True{}, bl}:
      Bignum.make(True{}, Bignum.add_abs(al, bl, 0))
    case Bignum.Int{True{}, +al} Bignum.Int{False{}, +bl}:
      Bignum.add_different(True{}, al, bl, Bignum.cmp_abs(al, bl))
    case Bignum.Int{False{}, +al} Bignum.Int{True{}, +bl}:
      Bignum.add_different(False{}, al, bl, Bignum.cmp_abs(al, bl))

def Bignum.sub_order(a: Bignum.t, b: Bignum.t, order: Cmp) -> Bignum.t:
  match order:
    case GT{}:
      Bignum.add(a, Bignum.negate(b))
    case EQ{}:
      Bignum.zero
    case LT{}:
      Bignum.zero

def Bignum.sub(+a: Bignum.t, +b: Bignum.t) -> Bignum.t:
  Bignum.sub_order(a, b, Bignum.compare(a, b))

def Bignum.mul_limb(xs: List<&2, U32>, +factor: U32, +carry: U32) -> List<&2, U32>:
  match xs:
    case Nil{}:
      Bignum.carry_end(carry, U32.is_zero(carry))
    case Con{+h, t}:
      Con{((h * factor + carry) % 32768 : U32), Bignum.mul_limb(t, factor, ((h * factor + carry) / 32768 : U32))}

def Bignum.mul_abs(xs: List<&2, U32>, +ys: List<&2, U32>) -> List<&2, U32>:
  match xs:
    case Nil{}:
      []
    case Con{h, t}:
      Bignum.add_abs(Bignum.mul_limb(ys, h, 0), Con{0, Bignum.mul_abs(t, ys)}, 0)

def Bignum.mul(a: Bignum.t, b: Bignum.t) -> Bignum.t:
  match a b:
    case Bignum.Int{an, al} Bignum.Int{bn, bl}:
      Bignum.make(Bool.xor(an, bn), Bignum.mul_abs(al, bl))

# of_decimal reads five digits per pass: each pass multiplies the limbs by
# 10^5 and adds the chunk in one tail-recursive walk.  A limb is below
# 2^15, so limb * 10^5 + carry stays below 2^32 and every step is U32.
# The limbs never carry a high zero limb, so the result needs no trim.
def Bignum.carry_lo(zero: Bool, +lo: U32, out: List<&2, U32>) -> List<&2, U32>:
  match zero:
    case True{}:
      out
    case False{}:
      Con{lo, out}

def Bignum.carry_out(+carry: U32, out: List<&2, U32>) -> List<&2, U32>:
  Bignum.carry_lo(U32.is_zero((carry / 32768 : U32)), (carry / 32768 : U32), Bignum.carry_lo(U32.is_zero(carry), (carry % 32768 : U32), out))

def Bignum.limbs_onto(xs: List<&2, U32>, acc: List<&2, U32>) -> List<&2, U32>:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      Bignum.limbs_onto(t, Con{h, acc})

def Bignum.muladd_go(xs: List<&2, U32>, +factor: U32, +carry: U32, out: List<&2, U32>) -> List<&2, U32>:
  match xs:
    case Nil{}:
      Bignum.carry_out(carry, out)
    case Con{+h, t}:
      Bignum.muladd_go(t, factor, ((h * factor + carry) / 32768 : U32), Con{((h * factor + carry) % 32768 : U32), out})

def Bignum.muladd(xs: List<&2, U32>, +factor: U32, +chunk: U32) -> List<&2, U32>:
  Bignum.limbs_onto(Bignum.muladd_go(xs, factor, chunk, []), [])

def Bignum.all_digits(xs: String, ok: Bool) -> Bool:
  match xs:
    case SNil{}:
      ok
    case SCon{Chr{+c}, tail}:
      Bignum.all_digits(tail, Bool.and(ok, Bool.and((c >= 48 : U32), (c <= 57 : U32))))

def Bignum.dec_limbs(full: Bool, limbs: List<&2, U32>, +factor: U32, +value: U32) -> List<&2, U32>:
  match full:
    case True{}:
      Bignum.muladd(limbs, factor, value)
    case False{}:
      limbs

def Bignum.dec_value(full: Bool, +value: U32) -> U32:
  match full:
    case True{}:
      0
    case False{}:
      value

def Bignum.dec_factor(full: Bool, +factor: U32) -> U32:
  match full:
    case True{}:
      1
    case False{}:
      factor

def Bignum.dec_count(full: Bool, +count: Nat) -> Nat:
  match full:
    case True{}:
      0n
    case False{}:
      count

# decimal_go runs on checked digits only, so c - 48 never wraps.  The
# fifth digit of a chunk flushes the chunk into the limbs.
def Bignum.decimal_go(xs: String, limbs: List<&2, U32>, +value: U32, +factor: U32, +count: Nat) -> Bignum.t:
  match xs:
    case SNil{}:
      Bignum.Int{False{}, Bignum.muladd(limbs, factor, value)}
    case SCon{Chr{+c}, tail}:
      Bignum.decimal_go(tail, Bignum.dec_limbs(Nat.is_eq(count, 4n), limbs, (factor * 10 : U32), (value * 10 + (c - 48) : U32)), Bignum.dec_value(Nat.is_eq(count, 4n), (value * 10 + (c - 48) : U32)), Bignum.dec_factor(Nat.is_eq(count, 4n), (factor * 10 : U32)), Bignum.dec_count(Nat.is_eq(count, 4n), (count + 1n : Nat)))

def Bignum.of_decimal_valid(valid: Bool, source: String) -> Maybe<&2, Bignum.t>:
  match valid:
    case False{}:
      None{}
    case True{}:
      Some{Bignum.decimal_go(source, [], 0, 1, 0n)}

def Bignum.of_decimal(source: String) -> Maybe<&2, Bignum.t>:
  match source:
    case SNil{}:
      None{}
    case SCon{+h, +t}:
      Bignum.of_decimal_valid(Bignum.all_digits(SCon{h, t}, True{}), SCon{h, t})

def Bignum.div_ten(xs: List<&2, U32>, +carry: U32, acc: List<&2, U32>) -> Pair2<List<&2, U32>, U32>:
  match xs:
    case Nil{}:
      Pair2{Bignum.trim(acc), carry}
    case Con{+h, t}:
      Bignum.div_ten(t, ((carry * 32768 + h) % 10 : U32), Con{((carry * 32768 + h) / 10 : U32), acc})

def Bignum.show_digit(divided: Pair2<List<&2, U32>, U32>, text: String) -> Pair2<List<&2, U32>, String>:
  match divided:
    case Pair2{q, digit}:
      Pair2{q, SCon{Chr{(48 + digit : U32)}, text}}

@unsafe
def Bignum.show_loop(state: Pair2<List<&2, U32>, String>) -> String:
  match state:
    case Pair2{Nil{}, text}:
      text
    case Pair2{Con{h, t}, text}:
      Bignum.show_loop(Bignum.show_digit(Bignum.div_ten(List.reverse(&2, U32, Con{h, t}), 0, []), text))

def Bignum.to_string(value: Bignum.t) -> String:
  match value:
    case Bignum.Int{negative, Nil{}}:
      "0"
    case Bignum.Int{False{}, Con{h, t}}:
      Bignum.show_loop(Pair2{Con{h, t}, ""})
    case Bignum.Int{True{}, Con{h, t}}:
      String.append("-", Bignum.show_loop(Pair2{Con{h, t}, ""}))

def Bignum.narrow_ok(h: U32, n: Nat, valid: Bool) -> Maybe<&2, Nat>:
  match valid:
    case False{}:
      None{}
    case True{}:
      Some{(n * 32768n + U32.to_nat(h) : Nat)}

def Bignum.narrow_step(h: U32, tail: Maybe<&2, Nat>, max: Nat) -> Maybe<&2, Nat>:
  match tail:
    case None{}:
      None{}
    case Some{+n}:
      Bignum.narrow_ok(h, n, (n <= max : Nat))

def Bignum.narrow(xs: List<&2, U32>) -> Maybe<&2, Nat>:
  match xs:
    case Nil{}:
      Some{0n}
    case Con{h, t}:
      Bignum.narrow_step(h, Bignum.narrow(t), (4294967295n * 2n + 1n : Nat))

def Bignum.to_nat(value: Bignum.t) -> Maybe<&2, Nat>:
  match value:
    case Bignum.Int{True{}, limbs}:
      None{}
    case Bignum.Int{False{}, limbs}:
      Bignum.narrow(limbs)

def Bignum.i31_check(value: Bignum.t, order: Cmp) -> Maybe<&2, Nat>:
  match order:
    case GT{}:
      None{}
    case EQ{}:
      Bignum.to_nat(value)
    case LT{}:
      Bignum.to_nat(value)

def Bignum.to_i31(+value: Bignum.t) -> Maybe<&2, Nat>:
  Bignum.i31_check(value, Bignum.compare(value, Bignum.from_nat(1073741823n)))

def Order.le(order: Cmp) -> Bool:
  match order:
    case LT{}:
      True{}
    case EQ{}:
      True{}
    case GT{}:
      False{}

def Order.first(order: Cmp, next: Cmp) -> Cmp:
  match order:
    case LT{}:
      LT{}
    case EQ{}:
      next
    case GT{}:
      GT{}

def choose(-A: Data, yes: Bool, left: A, right: A) -> A:
  match yes:
    case True{}:
      left
    case False{}:
      right

def maybe_when(-A: Data, valid: Bool, value: A) -> Maybe<&2, A>:
  match valid:
    case True{}:
      Some{value}
    case False{}:
      None{}

def Bignum.to_int(value: Bignum.t) -> Maybe<&2, Int63.t>:
  match value:
    case Bignum.Int{+negative, +limbs}:
      maybe_when(Int63.t, Order.le(Bignum.cmp_abs(limbs, choose(List<&2, U32>, negative, [0, 0, 0, 0, 4], [32767, 32767, 32767, 32767, 3]))), Int63.Int{negative, limbs})

def Bignum.limbs15(value: Bignum.t) -> Maybe<&2, List<&2, U32>>:
  match value:
    case Bignum.Int{negative, limbs}:
      maybe_when(List<&2, U32>, Bool.not(negative), limbs)

# Input is in the signed OCaml-int range. Stage A preserves host overflow.
def Bignum.succ_int63(+value: Bignum.t) -> Bignum.t:
  choose(Bignum.t, Bignum.equal(value, Bignum.Int{False{}, [32767, 32767, 32767, 32767, 3]}), Bignum.Int{True{}, [0, 0, 0, 0, 4]}, Bignum.add(value, Bignum.one))

def Text.compare(a: String, b: String) -> Cmp:
  match a:
    case SNil{}:
      match b:
        case SNil{}:
          EQ{}
        case SCon{c, tail}:
          LT{}
    case SCon{Chr{c}, tail}:
      match b:
        case SNil{}:
          GT{}
        case SCon{Chr{d}, rest}:
          Order.first(U32.cmp(c, d), Text.compare(tail, rest))

# Persistent character trie. Terminal bindings precede their extensions;
# character branches traverse in codepoint order, preserving lexical order.
type Map.t<-A: Data> is Data:
  Map.Empty{}
  Map.End{value: A, rest: Map.t<A>}
  Map.Char{code: U32, less: Map.t<A>, same: Map.t<A>, greater: Map.t<A>}

def Map.empty(-A: Data) -> Map.t<A>:
  Map.Empty{}

def Map.select(-A: Type, order: Cmp, lt: Unit -> A, eq: Unit -> A, gt: Unit -> A) -> A:
  match order:
    case LT{}: lt(Unit{})
    case EQ{}: eq(Unit{})
    case GT{}: gt(Unit{})

def Map.with_key(-A: Type, key: String, empty: Unit -> A, cons: U32 -> String -> A) -> A:
  match key:
    case SNil{}: empty(Unit{})
    case SCon{Chr{code}, rest}: cons(code)(rest)

def Map.singleton(-A: Data, key: String, value: A) -> Map.t<A>:
  match key:
    case SNil{}: Map.End{value, Map.Empty{}}
    case SCon{Chr{code}, rest}: Map.Char{code, Map.Empty{}, Map.singleton(A, rest, value), Map.Empty{}}

def Map.get_at(-A: Data, map: Map.t<A>, +key: String) -> Maybe<&2, A>:
  match map:
    case Map.Empty{}: None{}
    case Map.End{value, rest}:
      Map.with_key(Maybe<&2, A>, key, _ => Some{value}, code => tail => Map.get_at(A, rest, SCon{Chr{code}, tail}))
    case Map.Char{pivot, less, same, greater}:
      Map.with_key(Maybe<&2, A>, key, _ => None{}, +code => +tail =>
        Map.select(Maybe<&2, A>, U32.cmp(code, pivot),
          _ => Map.get_at(A, less, SCon{Chr{code}, tail}),
          _ => Map.get_at(A, same, tail),
          _ => Map.get_at(A, greater, SCon{Chr{code}, tail})))

def Map.get(-A: Data, key: String, map: Map.t<A>) -> Maybe<&2, A>:
  Map.get_at(A, map, key)

def Map.insert_at(-A: Data, map: Map.t<A>, +key: String, +value: A) -> Map.t<A>:
  match map:
    case Map.Empty{}: Map.singleton(A, key, value)
    case Map.End{previous, +rest}:
      Map.with_key(Map.t<A>, key, _ => Map.End{value, rest}, code => tail =>
        Map.End{previous, Map.insert_at(A, rest, SCon{Chr{code}, tail}, value)})
    case Map.Char{+pivot, +less, +same, +greater}:
      Map.with_key(Map.t<A>, key, _ => Map.End{value, Map.Char{pivot, less, same, greater}}, +code => +tail =>
        Map.select(Map.t<A>, U32.cmp(code, pivot),
          _ => Map.Char{pivot, Map.insert_at(A, less, SCon{Chr{code}, tail}, value), same, greater},
          _ => Map.Char{pivot, less, Map.insert_at(A, same, tail, value), greater},
          _ => Map.Char{pivot, less, same, Map.insert_at(A, greater, SCon{Chr{code}, tail}, value)}))

def Map.insert(-A: Data, key: String, value: A, map: Map.t<A>) -> Map.t<A>:
  Map.insert_at(A, map, key, value)

def Map.remove_at(-A: Data, map: Map.t<A>, +key: String) -> Map.t<A>:
  match map:
    case Map.Empty{}: Map.Empty{}
    case Map.End{value, +rest}:
      Map.with_key(Map.t<A>, key, _ => rest, code => tail => Map.End{value, Map.remove_at(A, rest, SCon{Chr{code}, tail})})
    case Map.Char{+pivot, +less, +same, +greater}:
      Map.with_key(Map.t<A>, key, _ => Map.Char{pivot, less, same, greater}, +code => +tail =>
        Map.select(Map.t<A>, U32.cmp(code, pivot),
          _ => Map.Char{pivot, Map.remove_at(A, less, SCon{Chr{code}, tail}), same, greater},
          _ => Map.Char{pivot, less, Map.remove_at(A, same, tail), greater},
          _ => Map.Char{pivot, less, same, Map.remove_at(A, greater, SCon{Chr{code}, tail})}))

def Map.remove(-A: Data, key: String, map: Map.t<A>) -> Map.t<A>:
  Map.remove_at(A, map, key)

def Map.reverse(key: String, out: String) -> String:
  match key:
    case SNil{}: out
    case SCon{code, rest}: Map.reverse(rest, SCon{code, out})

def Map.bindings_prefix(-A: Data, map: Map.t<A>, +prefix: String) -> List<&2, Pair2<String, A>>:
  match map:
    case Map.Empty{}: []
    case Map.End{value, rest}:
      Pair2{Map.reverse(prefix, SNil{}), value} <> Map.bindings_prefix(A, rest, prefix)
    case Map.Char{code, less, same, greater}:
      List.append(&2, Pair2<String, A>, Map.bindings_prefix(A, less, prefix),
        List.append(&2, Pair2<String, A>, Map.bindings_prefix(A, same, SCon{Chr{code}, prefix}), Map.bindings_prefix(A, greater, prefix)))

def Map.bindings(-A: Data, map: Map.t<A>) -> List<&2, Pair2<String, A>>:
  Map.bindings_prefix(A, map, SNil{})

def Map.size(-A: Data, map: Map.t<A>) -> Nat:
  match map:
    case Map.Empty{}: 0n
    case Map.End{value, rest}: (1n + Map.size(A, rest) : Nat)
    case Map.Char{code, less, same, greater}: (Map.size(A, less) + Map.size(A, same) + Map.size(A, greater) : Nat)

# Signed host arithmetic wraps at 63 bits, as OCaml int at the pin.
# wrap_sum is internal to add/sub: its input lies within one modulus.
def Int63.zero() -> Int63.t: Int63.Int{False{}, []}

def Int63.one() -> Int63.t: Int63.Int{False{}, [1]}

def Int63.equal(a: Int63.t, b: Int63.t) -> Bool:
  Bignum.equal(Bignum.of_int(a), Bignum.of_int(b))

def Int63.compare(a: Int63.t, b: Int63.t) -> Cmp:
  Bignum.compare(Bignum.of_int(a), Bignum.of_int(b))

def Int63.raw(value: Bignum.t) -> Int63.t:
  match value:
    case Bignum.Int{negative, limbs}: Int63.Int{negative, limbs}

def Int63.show(value: Int63.t) -> String: Bignum.to_string(Bignum.of_int(value))

def Int63.wrap_high_order(value: Bignum.t, order: Cmp) -> Int63.t:
  match order:
    case LT{}: Int63.raw(value)
    case EQ{}: Int63.raw(value)
    case GT{}: Int63.raw(Bignum.add(value, Bignum.Int{True{}, [0, 0, 0, 0, 8]}))

def Int63.wrap_high(+value: Bignum.t) -> Int63.t:
  Int63.wrap_high_order(value, Bignum.compare(value, Bignum.Int{False{}, [32767, 32767, 32767, 32767, 3]}))

def Int63.wrap_sum_order(value: Bignum.t, order: Cmp) -> Int63.t:
  match order:
    case LT{}: Int63.raw(Bignum.add(value, Bignum.Int{False{}, [0, 0, 0, 0, 8]}))
    case EQ{}: Int63.raw(value)
    case GT{}: Int63.wrap_high(value)

def Int63.wrap_sum(+value: Bignum.t) -> Int63.t:
  Int63.wrap_sum_order(value, Bignum.compare(value, Bignum.Int{True{}, [0, 0, 0, 0, 4]}))

def Int63.add(a: Int63.t, b: Int63.t) -> Int63.t:
  Int63.wrap_sum(Bignum.add(Bignum.of_int(a), Bignum.of_int(b)))

def Int63.sub(a: Int63.t, b: Int63.t) -> Int63.t:
  Int63.wrap_sum(Bignum.add(Bignum.of_int(a), Bignum.negate(Bignum.of_int(b))))

def Int63.succ(value: Int63.t) -> Int63.t: Int63.add(value, Int63.one)

def Int63.pred(value: Int63.t) -> Int63.t: Int63.sub(value, Int63.one)
