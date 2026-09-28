# Observers for A.2 tests. A mismatch produces a diagnostic string.
import Base
import ../lib/foundation.bend as F
import ../lib/kernel_error.bend as E
import ../lib/kernel_level.bend as L
import ../lib/kernel_literal.bend as Lit
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_shape.bend as S
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V
import ../lib/kernel_positivity.bend as Pos
import ../lib/kernel_prim.bend as P
import ../lib/kernel_global.bend as G
import ./foundation.bend as FT

def join(parts: List<&2, String>) -> String:
  match parts:
    case []: ""
    case part <> []: part
    case part <> next <> rest: String.append(part, String.append("|", join(next <> rest)))

def present(-A: Data, value: Maybe<&2, A>) -> String:
  match value:
    case None{}: "none"
    case Some{_}: "some"

def integer(value: F.Int63.t) -> String: F.Bignum.to_string(F.Bignum.of_int(value))
def boolean(value: Bool) -> String: FT.boolean(value)
def count(-A: Data, items: List<&2, A>) -> String: F.Bignum.to_string(F.Bignum.from_nat(List.length(&2, A, items)))

def opt_int(value: Maybe<&2, F.Int63.t>) -> String:
  match value:
    case None{}: "none"
    case Some{i}: integer(i)

def opt_text(value: Maybe<&2, String>) -> String:
  match value:
    case None{}: "none"
    case Some{text}: text

def opt_level(value: Maybe<&2, L.t>) -> String:
  match value:
    case None{}: "none"
    case Some{level}: F.Bignum.to_string(level)

def literal(value: Lit.t) -> String:
  match value:
    case Lit.LInt{n}: F.Bignum.to_string(n)
    case Lit.LString{s}: String.append("string:", s)

def maybe_literal(value: Maybe<&2, Lit.t>) -> String:
  match value:
    case None{}: "stuck"
    case Some{lit}: literal(lit)

def reduced(value: Result<&2, &2, E.t, Maybe<&2, Lit.t>>) -> String:
  match value:
    case Fail{_}: "error"
    case Done{answer}: maybe_literal(answer)

def value_tag(value: V.t) -> String:
  match value:
    case V.VUniv{l}: String.append("univ:", F.Bignum.to_string(l))
    case V.VLan{_, _, _}: "lan"
    case V.VRan{_, _, _}: "ran"
    case V.VIn{_, _, _}: "in"
    case V.VSec{_, _}: "sec"
    case V.VLit{lit}: literal(lit)
    case V.VNeutral{_, _}: "neutral"

def value_tags(values: List<&2, V.t>) -> List<&2, String>:
  match values:
    case []: []
    case value <> rest: value_tag(value) <> value_tags(rest)

def answer(value: V.t) -> String:
  match value:
    case V.VLit{lit}: literal(lit)
    case V.VIn{S.SColl{width}, V.VALeg{index}, [V.VSec{S.SColl{zero}, []}]}:
      join(["bool", integer(width), integer(index), integer(zero)])
    case _: "bad-value"

def maybe_answer(value: Maybe<&2, V.t>) -> String:
  match value:
    case None{}: "stuck"
    case Some{v}: answer(v)

def applied(value: Result<&2, &2, E.t, Maybe<&2, V.t>>) -> String:
  match value:
    case Fail{_}: "error"
    case Done{v}: maybe_answer(v)

def term_tag(term: T.t) -> String:
  match term:
    case T.Var{i}: String.append("var:", integer(i))
    case T.Univ{l}: String.append("univ:", F.Bignum.to_string(l))
    case T.Lan{_, _}: "lan"
    case T.Ran{_, _}: "ran"
    case T.In{_, _, _}: "in"
    case T.Elim{_}: "elim"
    case T.Sec{_, _}: "sec"
    case T.Out{_, _, _}: "out"
    case T.Let{_, _, _, _}: "let"
    case T.Ann{_, _}: "ann"
    case T.Global{name}: name
    case T.Lit{lit}: literal(lit)
    case T.Auto{}: "auto"

def term_tags(terms: List<&2, T.t>) -> List<&2, String>:
  match terms:
    case []: []
    case term <> rest: term_tag(term) <> term_tags(rest)

def maybe_term(term: Maybe<&2, T.t>) -> String:
  match term:
    case None{}: "none"
    case Some{t}: term_tag(t)

def quantity(q: Q.t) -> String:
  match q:
    case Q.Zero{}: "0"
    case Q.One{}: "1"
    case Q.Many{}: "w"

def point(value: Maybe<&2, F.Pair2<Q.t, T.t>>) -> String:
  match value:
    case None{}: "none"
    case Some{F.Pair2{q, t}}: join([quantity(q), term_tag(t)])

def vpoint(value: Maybe<&2, F.Pair2<Q.t, V.t>>) -> String:
  match value:
    case None{}: "none"
    case Some{F.Pair2{q, v}}: join([quantity(q), value_tag(v)])

def closure(value: V.closure) -> String:
  match value:
    case V.Closure{env, body}: join([join(value_tags(env)), term_tag(body)])

def former(value: Maybe<&2, V.Former>) -> String:
  match value:
    case None{}: "none"
    case Some{V.Former{s, d, u}}: join([S.name(V.t, s), closure(d), opt_level(u)])

def introduction(value: Maybe<&2, V.Intro>) -> String:
  match value:
    case None{}: "none"
    case Some{V.Intro{s, a, args}}: join([S.name(V.t, s), opt_text(V.as_ctor(a)), join(value_tags(args))])

def neutral(value: Maybe<&2, F.Pair2<V.head, List<&2, V.spine>>>) -> String:
  match value:
    case Some{F.Pair2{V.HLocal{i}, []}}: integer(i)
    case _: "bad-neutral"

def prim_name(value: Maybe<&2, P.t>) -> String:
  match value:
    case None{}: "none"
    case Some{p}: P.name(p)

def result_type(term: T.t) -> String:
  match term:
    case T.Global{name}: name
    case T.Lan{S.SColl{size}, T.Sec{S.SColl{width}, [T.Leg{[], T.Ann{T.Ran{S.SColl{a}, T.Sec{S.SColl{b}, []}}, T.Univ{l}}}, T.Leg{[], T.Ann{T.Ran{S.SColl{c}, T.Sec{S.SColl{d}, []}}, T.Univ{m}}}]}}:
      join(["bool", integer(size), integer(width), integer(a), integer(b), F.Bignum.to_string(l), integer(c), integer(d), F.Bignum.to_string(m)])
    case _: "bad-result-type"

def prim_type(term: T.t) -> String:
  match term:
    case T.Ran{S.SPi{q, x, T.Global{a}}, T.Ran{S.SPi{r, y, T.Global{b}}, result}}:
      join([quantity(q), x, a, quantity(r), y, b, result_type(result)])
    case _: "bad-primitive-type"

def definition(value: Maybe<&2, G.def_entry>) -> String:
  match value:
    case None{}: "none"
    case Some{G.Definition{ty, body, reducible, rec_arg, partial}}:
      join([term_tag(ty), term_tag(body), boolean(reducible), opt_int(rec_arg), boolean(partial)])

def axiom(value: Maybe<&2, G.axiom_entry>) -> String:
  match value:
    case None{}: "none"
    case Some{G.Postulate{ty}}: term_tag(ty)

def primitive(value: Maybe<&2, G.prim_entry>) -> String:
  match value:
    case None{}: "none"
    case Some{G.Primitive{ty, p}}: join([P.name(p), prim_type(ty)])

def status(value: Pos.ctor_status) -> String:
  match value:
    case Pos.Provisional{}: "provisional"
    case Pos.Builtin{}: "builtin"
    case Pos.Complete{names}: join(names)

def constructor(value: Maybe<&2, Pos.ctor>) -> String:
  match value:
    case None{}: "none"
    case Some{Pos.Ctor{name, args, indices, arity, rec}}:
      join([name, count(F.Pair2<Q.t, F.Pair2<String, T.t>>, args), join(term_tags(indices)), integer(arity), boolean(rec)])

def family(value: Maybe<&2, Pos.family>) -> String:
  match value:
    case None{}: "none"
    case Some{Pos.Family{name, params, indices, level, state, ctors, positive}}:
      join([name, count(F.Pair2<Q.t, F.Pair2<String, T.t>>, params), count(F.Pair2<Q.t, F.Pair2<String, T.t>>, indices), F.Bignum.to_string(level), status(state), count(Pos.ctor, ctors), boolean(positive)])

def section(value: Maybe<&2, F.Pair2<S.t<V.t>, List<&2, V.vleg>>>) -> String:
  match value:
    case Some{F.Pair2{s, [V.VLeg{[F.Pair2{q, name}], clo}]}}: join([S.name(V.t, s), quantity(q), name, closure(clo)])
    case _: "bad-section"

def stuck(value: Maybe<&2, F.Pair2<V.head, List<&2, V.spine>>>) -> String:
  match value:
    case Some{F.Pair2{V.HGlobal{head}, [V.SElim{V.Stuck{s, q, Some{T.Motive{ind, idx, self, body}}, [F.Pair2{T.APt{bq, arg}, T.Leg{[], branch}}], env}}, V.SOut{out_shape, V.VALeg{index}}]}}:
      join([head, S.name(V.t, s), quantity(q), opt_text(ind), join(idx), self, term_tag(body), quantity(bq), term_tag(arg), term_tag(branch), join(value_tags(env)), S.name(V.t, out_shape), integer(index)])
    case _: "bad-stuck-spine"

def global_sizes(globals: G.t) -> String:
  match globals:
    case G.Globals{entries, families}: join([F.Bignum.to_string(F.Bignum.from_nat(F.Map.size(G.entry, entries))), F.Bignum.to_string(F.Bignum.from_nat(F.Map.size(Pos.family, families)))])
