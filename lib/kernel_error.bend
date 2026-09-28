import Base
import ./foundation.bend as F

# Stage A retains every error constructor and message of the Kanon pin.
type t is Data:
  Not_yet{message: String}
  Parse{message: String, line: Nat, column: Nat}
  Carry{message: String}
  Unbound{message: String}
  Mismatch{message: String}
  Universe{message: String}
  Quantity{message: String}
  Wrong_leg{message: String}
  Missing_branch{message: String}
  Overflow{message: String}
  Cannot_infer{message: String}
  Budget_exhausted{message: String}
  Index_not_zero{message: String}
  Index_above_universe{message: String}
  Termination{name: String}

def termination_msg(name: String) -> String:
  String.append("recursive definition ", String.append(name, " failed the structural termination guard"))

def message(error: t) -> String:
  match error:
    case Not_yet{s}:
      s
    case Parse{s, line, column}:
      s
    case Carry{s}:
      s
    case Unbound{s}:
      s
    case Mismatch{s}:
      s
    case Universe{s}:
      s
    case Quantity{s}:
      s
    case Wrong_leg{s}:
      s
    case Missing_branch{s}:
      s
    case Overflow{s}:
      s
    case Cannot_infer{s}:
      s
    case Budget_exhausted{s}:
      s
    case Index_not_zero{s}:
      s
    case Index_above_universe{s}:
      s
    case Termination{name}:
      termination_msg(name)

def to_string(error: t) -> String:
  match error:
    case Not_yet{s}:
      String.append("not yet: ", s)
    case Parse{s, line, column}:
      String.append("line ", String.append(F.Bignum.to_string(F.Bignum.from_nat(line)), String.append(", column ", String.append(F.Bignum.to_string(F.Bignum.from_nat(column)), String.append(": ", s)))))
    case Carry{s}:
      String.append("carry: ", s)
    case Unbound{s}:
      String.append("unbound: ", s)
    case Mismatch{s}:
      String.append("mismatch: ", s)
    case Universe{s}:
      String.append("universe: ", s)
    case Quantity{s}:
      String.append("quantity: ", s)
    case Wrong_leg{s}:
      String.append("wrong leg: ", s)
    case Missing_branch{s}:
      String.append("missing branch: ", s)
    case Overflow{s}:
      String.append("overflow: ", s)
    case Cannot_infer{s}:
      String.append("cannot infer: ", s)
    case Budget_exhausted{s}:
      String.append("budget: ", s)
    case Index_not_zero{s}:
      String.append("index not zero: ", s)
    case Index_above_universe{s}:
      String.append("index above universe: ", s)
    case Termination{name}:
      String.append("termination: ", termination_msg(name))
