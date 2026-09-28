# Quantities and pure path usage intervals from Kanon 69f3be51 quantity.ml.
import Base
import ./foundation.bend as F

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

def mul(a: t, b: t) -> t:
  match a:
    case Zero{}: Zero{}
    case One{}: b
    case Many{}:
      match b:
        case Zero{}: Zero{}
        case One{}: Many{}
        case Many{}: Many{}

def to_string(q: t) -> String:
  match q:
    case Zero{}: "0"
    case One{}: "1"
    case Many{}: "w"

def runtime(q: t) -> t: F.choose(t, equal(q, Zero{}), Zero{}, One{})
def add(+a: t, +b: t) -> t:
  F.choose(t, equal(a, Zero{}), b, F.choose(t, equal(b, Zero{}), a, Many{}))

def minimum(a: t, b: t) -> t:
  match a:
    case Zero{}: Zero{}
    case One{}: F.choose(t, equal(b, Zero{}), Zero{}, One{})
    case Many{}: b

def maximum(a: t, b: t) -> t:
  match a:
    case Zero{}: b
    case One{}: F.choose(t, equal(b, Many{}), Many{}, One{})
    case Many{}: Many{}

def interval_t() -> Data: F.Pair2<t, t>
def uses() -> Data: List<&2, F.Pair2<F.Int63.t, interval_t>>
type usage is Data:
  Usage{paths: Maybe<&2, uses>, reads: uses}

def empty() -> usage: Usage{Some{[]}, []}
def unreachable() -> usage: Usage{None{}, []}

def if_else(-A: Data, flag: Bool, yes: Unit -> A, no: Unit -> A) -> A:
  match flag:
    case True{}: yes(Unit{})
    case False{}: no(Unit{})

def interval_order(order: Cmp, value: interval_t, next: Unit -> interval_t) -> interval_t:
  match order:
    case LT{}: F.Pair2{Zero{}, Zero{}}
    case EQ{}: value
    case GT{}: next(Unit{})

def interval(+level: F.Int63.t, rows: uses) -> interval_t:
  match rows:
    case []: F.Pair2{Zero{}, Zero{}}
    case F.Pair2{key, value} <> rest:
      interval_order(F.Int63.compare(level, key), value, _ => interval(level, rest))

def returning(+rows: uses) -> usage: Usage{Some{rows}, rows}
def occurrence(level: F.Int63.t, +q: t) -> usage: returning([F.Pair2{level, F.Pair2{q, q}}])

def join_interval(f: t -> t -> t, g: t -> t -> t, a: interval_t, b: interval_t) -> interval_t:
  match a, b:
    case F.Pair2{lo, hi}, F.Pair2{lo2, hi2}: F.Pair2{f(lo, lo2), g(hi, hi2)}

def combine_order(order: Cmp, less: Unit -> uses, same: Unit -> uses, more: Unit -> uses) -> uses:
  match order:
    case LT{}: less(Unit{})
    case EQ{}: same(Unit{})
    case GT{}: more(Unit{})

def combine(~f: t -> t -> t, ~g: t -> t -> t, a: uses, b: uses) -> uses:
  match a, b:
    case [], []: []
    case [], F.Pair2{k, v} <> rest:
      F.Pair2{k, join_interval(f, g, F.Pair2{Zero{}, Zero{}}, v)} <> combine(f, g, [], rest)
    case F.Pair2{k, v} <> rest, []:
      F.Pair2{k, join_interval(f, g, v, F.Pair2{Zero{}, Zero{}})} <> combine(f, g, rest, [])
    case F.Pair2{+k, +v} <> +rest, F.Pair2{+j, +w} <> +tail:
      combine_order(F.Int63.compare(k, j),
        _ => F.Pair2{k, join_interval(f, g, v, F.Pair2{Zero{}, Zero{}})} <> combine(f, g, rest, F.Pair2{j, w} <> tail),
        _ => F.Pair2{k, join_interval(f, g, v, w)} <> combine(f, g, rest, tail),
        _ => F.Pair2{j, join_interval(f, g, F.Pair2{Zero{}, Zero{}}, w)} <> combine(f, g, F.Pair2{k, v} <> rest, tail))

def sequence_paths(a: Maybe<&2, uses>, b: Maybe<&2, uses>) -> Maybe<&2, uses>:
  match a:
    case None{}: None{}
    case Some{x}:
      match b:
        case None{}: None{}
        case Some{y}: Some{combine(x => y => add(x, y), x => y => add(x, y), x, y)}

def sequence(a: usage, b: usage) -> usage:
  match a, b:
    case Usage{ap, ar}, Usage{bp, br}:
      Usage{sequence_paths(ap, bp), combine(x => y => add(x, y), x => y => add(x, y), ar, br)}

def alternative_paths(a: Maybe<&2, uses>, b: Maybe<&2, uses>) -> Maybe<&2, uses>:
  match a:
    case None{}: b
    case Some{x}:
      match b:
        case None{}: Some{x}
        case Some{y}: Some{combine(x => y => minimum(x, y), x => y => maximum(x, y), x, y)}

def alternative(a: usage, b: usage) -> usage:
  match a, b:
    case Usage{ap, ar}, Usage{bp, br}:
      Usage{alternative_paths(ap, bp), combine(x => y => maximum(x, y), x => y => maximum(x, y), ar, br)}

def scale_map(+lo: t, +hi: t, rows: uses) -> uses:
  match rows:
    case []: []
    case F.Pair2{k, F.Pair2{a, b}} <> rest:
      F.Pair2{k, F.Pair2{mul(lo, a), mul(hi, b)}} <> scale_map(lo, hi, rest)

def map_paths(f: uses -> uses, paths: Maybe<&2, uses>) -> Maybe<&2, uses>:
  match paths:
    case None{}: None{}
    case Some{xs}: Some{f(xs)}

def scale_range(range: interval_t, value: usage) -> usage:
  match range, value:
    case F.Pair2{+lo, +hi}, Usage{paths, reads}:
      Usage{map_paths(xs => scale_map(lo, hi, xs), paths), scale_map(lo, hi, reads)}

def scale(+q: t, value: usage) -> usage:
  if_else(usage, equal(q, Zero{}), _ => empty, _ => scale_range(F.Pair2{q, q}, value))

def unreachable_interval(value: interval_t) -> interval_t:
  match value:
    case F.Pair2{_, hi}: F.Pair2{One{}, maximum(One{}, hi)}

def get(+level: F.Int63.t, value: usage) -> interval_t:
  match value:
    case Usage{paths, reads}:
      match paths:
        case Some{xs}: interval(level, xs)
        case None{}: unreachable_interval(interval(level, reads))

def remove_map(+level: F.Int63.t, rows: uses) -> uses:
  match rows:
    case []: []
    case F.Pair2{+key, v} <> +rest:
      if_else(uses, F.Int63.equal(level, key), _ => rest, _ => F.Pair2{key, v} <> remove_map(level, rest))

def remove(+level: F.Int63.t, value: usage) -> usage:
  match value:
    case Usage{paths, reads}:
      Usage{map_paths(xs => remove_map(level, xs), paths), remove_map(level, reads)}

def interval_once(value: interval_t) -> Bool:
  match value:
    case F.Pair2{lo, hi}: Bool.and(equal(lo, One{}), equal(hi, One{}))

def exactly_once(level: F.Int63.t, value: usage) -> Bool: interval_once(get(level, value))
def interval_used(value: interval_t) -> Bool:
  match value:
    case F.Pair2{_, hi}: Bool.not(equal(hi, Zero{}))

def used(level: F.Int63.t, value: usage) -> Bool:
  match value:
    case Usage{_, reads}: interval_used(interval(level, reads))

def interval_at_most_once(value: interval_t) -> Bool:
  match value:
    case F.Pair2{_, hi}: Bool.not(equal(hi, Many{}))

def at_most_once(level: F.Int63.t, value: usage) -> Bool: interval_at_most_once(get(level, value))

def capture_paths(paths: Maybe<&2, uses>, reads: uses) -> uses:
  match paths:
    case None{}: reads
    case Some{xs}: combine(lo => _ => lo, x => y => maximum(x, y), xs, reads)

def captures(value: usage) -> usage:
  match value:
    case Usage{paths, +reads}: Usage{Some{capture_paths(paths, reads)}, reads}
