# Native family declarations and positional elim arms use checked constructor metadata.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_error.bend as E
import ../lib/kernel_positivity.bend as P
import token.bend as T
import lexer.bend as L
import syntax.bend as S
import elab_program.bend as M

def tokens() -> Data: List<&2, T.t>
def parsed(-A: Data) -> Data: L.result(F.Pair2<A, tokens>)
def want(-A: Data, what: String, ts: tokens) -> parsed(A):
  match ts:
    case []: L.error(F.Pair2<A, tokens>, T.start, "unexpected end of family declaration")
    case T.Token{kind, loc} <> _: L.error(F.Pair2<A, tokens>, loc, S.cat(["expected ", what, ", found ", T.describe(kind)]))

type ctor_step is Data:
  Read{fuel: Nat, ts: tokens, acc: List<&2, S.fam_ctor>, seen: List<&2, String>}
  AfterType{fuel: Nat, name: String, acc: List<&2, S.fam_ctor>, seen: List<&2, String>, pair: F.Pair2<S.t, tokens>}
  AfterTail{fuel: Nat, ctors: List<&2, S.fam_ctor>, seen: List<&2, String>, ts: tokens}

# A constructor name differs from each family and constructor name seen so far in its group.
def known(+name: String, seen: List<&2, String>) -> Bool:
  match seen:
    case []: False{}
    case label <> rest: Bool.or(String.eq(name, label), known(name, rest))
def ctor_names(cs: List<&2, S.fam_ctor>, tail: List<&2, String>) -> List<&2, String>:
  match cs:
    case []: tail
    case S.FamCtor{name, _} <> rest: name <> ctor_names(rest, tail)
def group_names(fams: List<&2, S.fam>) -> List<&2, String>:
  match fams:
    case []: []
    case S.Fam{name, _, _, cs} <> rest: name <> ctor_names(cs, group_names(rest))
# Both '}' exits return the constructors in declaration order.
def finished(ctors: List<&2, S.fam_ctor>, rest: tokens) -> parsed(List<&2, S.fam_ctor>):
  Done{F.Pair2{L.reverse(S.fam_ctor, ctors, []), rest}}

@unsafe
def constructors(~parse: tokens -> parsed(S.t), step: ctor_step) -> parsed(List<&2, S.fam_ctor>):
  match step:
    case Read{fuel, +ts, +acc, +seen}:
      match fuel:
        case 0n: want(List<&2, S.fam_ctor>, "'}'", ts)
        case 1n+remaining:
          match ts:
            case T.Token{T.RBrace{}, _} <> rest: finished(acc, rest)
            case T.Token{T.Ident{+name}, +loc} <> T.Token{T.Colon{}, _} <> rest:
              L.choose(parsed(List<&2, S.fam_ctor>), known(name, seen),
                _ => L.error(F.Pair2<List<&2, S.fam_ctor>, tokens>, loc, S.cat(["E-FAMILY-CTOR: duplicate constructor ", name])),
                _ => L.bind(F.Pair2<S.t, tokens>, F.Pair2<List<&2, S.fam_ctor>, tokens>, parse(rest), pair => constructors(~parse, AfterType{remaining, name, acc, name <> seen, pair})))
            case _: want(List<&2, S.fam_ctor>, "a constructor 'name : type' or '}'", ts)
    case AfterType{fuel, name, acc, seen, pair}:
      match pair:
        case F.Pair2{ty, ts}: constructors(~parse, AfterTail{fuel, S.FamCtor{name, ty} <> acc, seen, ts})
    case AfterTail{fuel, +ctors, +seen, +ts}:
      match ts:
        case T.Token{T.Semicolon{}, _} <> rest: constructors(~parse, Read{fuel, rest, ctors, seen})
        case T.Token{T.RBrace{}, _} <> rest: finished(ctors, rest)
        case _: want(List<&2, S.fam_ctor>, "';' or '}' after a constructor", ts)

def body(bs: List<&2, S.binder>, value: S.t) -> S.t:
  match bs:
    case []: value
    case first <> rest: S.SFun{first <> rest, value}
def field(b: S.binder) -> S.field:
  match b:
    case S.Binder{q, name, ty}: S.Field{q, name, Some{ty}}
def field_body() -> Data: F.Pair2<List<&2, S.field>, S.t>

# Consume exactly the constructor telescope, preserving a function-valued result.
@unsafe
def fields(+ctor: String, args: P.telescope, bs: List<&2, S.binder>, value: S.t, acc: List<&2, S.field>) -> M.result(field_body):
  match args:
    case []: M.Pure{F.Pair2{L.reverse(S.field, acc, []), body(bs, value)}}
    case arg <> rest:
      match bs:
        case first <> more: fields(ctor, rest, more, value, field(first) <> acc)
        case []:
          match value:
            case S.SFun{more, inner}: fields(ctor, arg <> rest, more, inner, acc)
            case other: M.Abort{E.Missing_branch{S.cat(["the elim arm for ", ctor, " must bind each of its fields"])}}

def ctor_at(ctors: List<&2, P.ctor>, +key: F.Int63.t) -> Maybe<&2, P.ctor>:
  match ctors:
    case []: None{}
    case first <> rest:
      L.choose(Maybe<&2, P.ctor>, F.Int63.equal(key, F.Int63.zero), _ => Some{first}, _ => ctor_at(rest, F.Int63.pred(key)))
def made(name: String, pair: field_body) -> S.branch:
  match pair:
    case F.Pair2{fs, result}: S.BrCtor{name, fs, result}
def branch(ct: P.ctor, binders: List<&2, S.binder>, value: S.t) -> M.result(S.branch):
  match ct:
    case P.Ctor{+name, args, _, _, _}:
      M.map_result(field_body, S.branch, pair => made(name, pair), fields(name, args, binders, value, []))
def selected(owner: String, key: F.Int63.t, binders: List<&2, S.binder>, value: S.t, found: Maybe<&2, P.ctor>) -> M.result(S.branch):
  match found:
    case None{}: M.Abort{E.Wrong_leg{S.cat(["family elim arm ", F.Int63.show(key), " of ", owner, " has no constructor"])}}
    case Some{ct}: branch(ct, binders, value)
# Only legs reach this helper from the arms helper; the other rows guard other callers.
def arm(owner: String, ctors: List<&2, P.ctor>, br: S.branch) -> M.result(S.branch):
  match br:
    case S.BrCtor{name, fs, value}: M.Pure{S.BrCtor{name, fs, value}}
    case S.BrDefault{_}: M.Abort{E.Wrong_leg{"a family default arm must be expanded before constructor selection"}}
    case S.BrLeg{+key, binders, value}: selected(owner, key, binders, value, ctor_at(ctors, key))
# Remove an explicit constructor so a default covers only the unhandled ones.
def without(+name: String, cs: List<&2, P.ctor>) -> List<&2, P.ctor>:
  match cs:
    case []: []
    case +ct <> +rest:
      L.choose(List<&2, P.ctor>, String.eq(name, P.ctor_name(ct)),
        _ => rest, _ => ct <> without(name, rest))
# A selected arm is always a constructor branch, so the leg and default rows cannot occur.
def after(br: S.branch, cs: List<&2, P.ctor>) -> List<&2, P.ctor>:
  match br:
    case S.BrCtor{name, _, _}: without(name, cs)
    case S.BrLeg{_, _, _}: cs
    case S.BrDefault{_}: cs

# Each generated arm follows the same field and result-lambda path as an explicit arm.
def fill(cs: List<&2, P.ctor>, +value: S.t) -> M.result(List<&2, S.branch>):
  match cs:
    case []: M.Pure{[]}
    case ct <> rest:
      M.bind(S.branch, List<&2, S.branch>, branch(ct, [], value), br =>
        M.map_result(List<&2, S.branch>, List<&2, S.branch>, tail => br <> tail, fill(rest, value)))
def default(cs: List<&2, P.ctor>, value: S.t) -> M.result(List<&2, S.branch>):
  match cs:
    case []: M.Abort{E.Wrong_leg{"a default elim arm must cover at least one remaining constructor"}}
    case ct <> rest: fill(ct <> rest, value)
# The parser yields only legs and closes the arm list after a default, so the
# nonfinal-default and constructor rows only guard other callers. The constructor
# row does not check for duplicate or unknown constructor names.
def arms(+owner: String, +ctors: List<&2, P.ctor>, bs: List<&2, S.branch>, +remaining: List<&2, P.ctor>) -> M.result(List<&2, S.branch>):
  match bs:
    case []: M.Pure{[]}
    case S.BrDefault{value} <> []: default(remaining, value)
    case S.BrDefault{_} <> (_ <> _): M.Abort{E.Wrong_leg{"a default elim arm must be last"}}
    case S.BrLeg{key, bs, value} <> rest:
      M.bind(S.branch, List<&2, S.branch>, arm(owner, ctors, S.BrLeg{key, bs, value}), +br =>
        M.map_result(List<&2, S.branch>, List<&2, S.branch>, tail => br <> tail, arms(owner, ctors, rest, after(br, remaining))))
    case S.BrCtor{+name, fs, value} <> rest:
      M.map_result(List<&2, S.branch>, List<&2, S.branch>, tail => S.BrCtor{name, fs, value} <> tail, arms(owner, ctors, rest, without(name, remaining)))
def expanded(fam: P.family, bs: List<&2, S.branch>) -> M.result(List<&2, S.branch>):
  match fam:
    case P.Family{name, _, _, _, _, +ctors, _}: arms(name, ctors, bs, ctors)
