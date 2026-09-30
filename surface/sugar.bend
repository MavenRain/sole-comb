# Native default arms expand to ordinary positional arms before branch elaboration.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_error.bend as E
import syntax.bend as S
import elab_program.bend as M

# Preserve the lambda payload binder and any remaining function binders.
def arm(+k: F.Int63.t, body: S.t) -> S.branch:
  match body:
    case S.SFun{[binder], value}: S.BrLeg{k, [binder], value}
    case S.SFun{binder <> rest, value}: S.BrLeg{k, [binder], S.SFun{rest, value}}
    case other: S.BrLeg{k, [], other}

def fill(~A: Data, slots: List<&2, A>, +k: F.Int63.t, +body: S.t) -> List<&2, S.branch>:
  match slots:
    case []: []
    case _ <> rest: arm(k, body) <> fill(A, rest, F.Int63.succ(k), body)

def default(~A: Data, k: F.Int63.t, body: S.t, slots: List<&2, A>) -> M.result(List<&2, S.branch>):
  match slots:
    case []: M.Abort{E.Wrong_leg{"a default elim arm must cover at least one remaining leg"}}
    case item <> rest: M.Pure{fill(A, item <> rest, k, body)}

# An explicit arm past the last leg stops the expansion, so branch elaboration
# reports its leg address before any default. The parser closes the arm list
# after a default, so the final-position refusal only guards other callers.
def expand(~A: Data, bs: List<&2, S.branch>, +k: F.Int63.t, +slots: List<&2, A>) -> M.result(List<&2, S.branch>):
  match bs:
    case []: M.Pure{[]}
    case S.BrLeg{+key, +binders, +body} <> rest:
      match slots:
        case []: M.Pure{S.BrLeg{key, binders, body} <> rest}
        case _ <> more:
          M.map_result(List<&2, S.branch>, List<&2, S.branch>, tail => S.BrLeg{key, binders, body} <> tail,
            expand(A, rest, F.Int63.succ(k), more))
    case S.BrCtor{name, _, _} <> _: M.Abort{E.Wrong_leg{S.cat(["the branch at ", name, " keys a constructor and a case at a collection keys a leg"])}}
    case S.BrDefault{body} <> rest:
      match rest:
        case []: default(A, k, body, slots)
        case _ <> _: M.Abort{E.Wrong_leg{"a default elim arm must be last"}}
