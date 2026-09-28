import Base
import ./kernel_error.bend as E

# The caller owns finite fuel. Pure kernel code receives and returns it;
# polling a host clock and the OCaml unlimited sentinel are not kernel APIs.
type t is Data:
  Budget{remaining: Nat}

def create(remaining: Nat) -> t:
  Budget{remaining}

def exhausted(budget: t) -> Bool:
  match budget:
    case Budget{remaining}:
      Nat.is_eq(remaining, 0n)

def spend(context: String, budget: t) -> Result<&2, &2, E.t, t>:
  match budget:
    case Budget{remaining}:
      match remaining:
        case 0n:
          Fail{E.Budget_exhausted{context}}
        case 1n+rest:
          Done{Budget{rest}}
