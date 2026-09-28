# Bidirectional checking and declaration admission at Kanon 69f3be51.
import Base
import ./foundation.bend as F
import ./kernel_budget.bend as B
import ./kernel_error.bend as E
import ./kernel_level.bend as L
import ./kernel_literal.bend as Lit
import ./kernel_quantity.bend as Q
import ./kernel_term.bend as T
import ./kernel_value.bend as V
import ./kernel_shape.bend as S
import ./kernel_positivity.bend as Pos
import ./kernel_global.bend as G
import ./kernel_prim.bend as Prim
import ./kernel_rules.bend as R
import ./kernel_eval.bend as EV
import ./kernel_conv.bend as CV
import ./kernel_pp.bend as PP

type local is Data:
  Local{name: String, quantity: Q.t, ty: V.t}

type ctx is Data:
  Context{globals: G.t, env: List<&2, V.t>, locals: List<&2, local>, size: F.Int63.t, budget: B.t}

def make(globals: G.t, budget: B.t) -> ctx: Context{globals, [], [], F.Int63.zero, budget}

def globals(c: ctx) -> G.t:
  match c:
    case Context{g, _, _, _, _}: g

def env(c: ctx) -> List<&2, V.t>:
  match c:
    case Context{_, e, _, _, _}: e

def locals(c: ctx) -> List<&2, local>:
  match c:
    case Context{_, _, ls, _, _}: ls

def size(c: ctx) -> F.Int63.t:
  match c:
    case Context{_, _, _, s, _}: s

def budget(c: ctx) -> B.t:
  match c:
    case Context{_, _, _, _, b}: b

def bind(x: String, q: Q.t, ty: V.t, c: ctx) -> ctx:
  match c:
    case Context{g, e, ls, +s, b}: Context{g, V.var(s) <> e, Local{x, q, ty} <> ls, F.Int63.succ(s), b}

def define(x: String, q: Q.t, ty: V.t, value: V.t, c: ctx) -> ctx:
  match c:
    case Context{g, e, ls, s, b}: Context{g, value <> e, Local{x, q, ty} <> ls, F.Int63.succ(s), b}

def local_names(ls: List<&2, local>) -> List<&2, String>:
  match ls:
    case []: []
    case Local{x, _, _} <> rest: x <> local_names(rest)

def pp_result(names: List<&2, String>, value: R.result(T.t)) -> String:
  match value:
    case Fail{_}: "a value that does not read back"
    case Done{tm}: PP.term(names, tm)

def pp_value(+c: ctx, value: V.t) -> String: pp_result(local_names(locals(c)), EV.quote(globals(c), size(c), value))

def local_ty(value: local) -> V.t:
  match value:
    case Local{_, _, ty}: ty

def head_ty(+c: ctx, head: V.head) -> R.result(V.t):
  match head:
    case V.HLocal{lvl}: R.map_result(local, V.t, local_ty, R.require(local, R.at(local, F.Int63.pred(F.Int63.sub(size(c), lvl)), locals(c)), E.Unbound{"a local level is outside the context"}))
    case V.HGlobal{+n}: R.bind(G.entry, V.t, R.require(G.entry, G.find(n, globals(c)), E.Unbound{n}), e => EV.eval(globals(c), [], G.entry_ty(e)))

def readable(+mode: Q.t, q: Q.t) -> Bool: Bool.or(Q.equal(mode, Q.Zero{}), Bool.not(Q.equal(q, Q.Zero{})))

def close_loop(ls: List<&2, local>, +level: F.Int63.t, +lower: F.Int63.t, +mode: Q.t, +affine: Bool, +uses: Q.usage) -> R.result(Q.usage):
  match ls:
    case []: Done{uses}
    case Local{+name, q, _} <> rest:
      R.choose_lazy(R.result(Q.usage), R.less_than(F.Int63.compare(level, lower)), _ => Done{uses}, _ =>
        R.choose_lazy(R.result(Q.usage), Bool.and(Bool.not(Q.equal(mode, Q.Zero{})), Bool.and(Q.equal(q, Q.One{}), Bool.not(F.choose(Bool, affine, Q.at_most_once(level, uses), Q.exactly_once(level, uses))))),
          _ => Fail{E.Quantity{F.choose(String, affine, String.concat(["the linear alias ", name, " may be used at most once"]), String.concat(["the linear binder ", name, " must be used exactly once on every runtime path"]))}},
          _ => close_loop(rest, F.Int63.pred(level), lower, mode, affine, Q.remove(level, uses))))

def close(affine: Bool, +c: ctx, lower: F.Int63.t, mode: Q.t, uses: Q.usage) -> R.result(Q.usage): close_loop(locals(c), F.Int63.pred(size(c)), lower, mode, affine, uses)
# Universe requests are handled by dispatch before this runtime-only adapter.

def quote_value(+c: ctx, value: V.t) -> R.result(T.t): EV.quote(globals(c), size(c), value)

def runtime_ops() -> R.ops<ctx>:
  R.Ops{_ => _ => Fail{E.Mismatch{"internal universe request outside the checking interpreter"}}, c => v => EV.whnf(globals(c), v), bind, size, env, c => e => t => EV.eval(globals(c), e, t), c => v => quote_value(c, v), c => h => head_ty(c, h), c => n => G.find_family(n, globals(c))}

def ops() -> R.check_ops<ctx>: R.CheckOps{runtime_ops, c => s => q => uses => close(False{}, c, s, q, uses), c => v => pp_value(c, v)}

def infer_request(c: ctx, q: Q.t, tm: T.t) -> R.program<ctx, R.inferred>: R.c_infer(ctx, ops, c, q, tm)

def check_request(c: ctx, q: Q.t, tm: T.t, ty: V.t) -> R.program<ctx, Q.usage>: R.c_check(ctx, ops, c, q, tm, ty)

def univ_request(c: ctx, tm: T.t) -> R.program<ctx, L.t>: R.ask_univ(ctx, c, tm)

def ensure(+c: ctx, +got: V.t, +expected: V.t) -> R.program<ctx, Unit>:
  R.pbind(ctx, Bool, Unit, R.ask_type(ctx, c, got, expected), same =>
    R.pchoose(ctx, Unit, same, _ => R.Pure{Unit{}}, _ => R.Abort{E.Mismatch{String.concat(["the term has type ", pp_value(c, got), " and the expected type is ", pp_value(c, expected)])}}))

def infer_local(+c: ctx, +mode: Q.t, +ix: F.Int63.t, entry: local) -> R.program<ctx, R.inferred>:
  match entry:
    case Local{x, q, ty}: R.pchoose(ctx, R.inferred, readable(mode, q), _ => R.Pure{F.Pair2{ty, Q.occurrence(F.Int63.pred(F.Int63.sub(size(c), ix)), mode)}}, _ => R.Abort{E.Quantity{String.concat(["the erased binder ", x, " is read in a runtime position"])}})

def no_infer(what: String) -> E.t: E.Cannot_infer{String.concat([what, " has no type of its own;  it needs an expected type"])}

def infer_literal(c: ctx, lit: Lit.t) -> R.program<ctx, R.inferred>:
  match lit:
    case Lit.LString{_}: R.Abort{E.Not_yet{"string types arrive at M1"}}
    case Lit.LInt{n}: R.pbind(ctx, Unit, R.inferred, R.guard(ctx, R.nonnegative(F.Bignum.sign(n)), E.Mismatch{"a Nat literal must be nonnegative"}), _ =>
      R.lift(ctx, R.inferred, R.map_result(V.t, R.inferred, ty => F.Pair2{ty, Q.empty}, EV.eval(globals(c), [], Prim.nat_ty))))

def linear_definition(ls: List<&2, local>, +level: F.Int63.t, +uses: Q.usage) -> Bool:
  match ls:
    case []: False{}
    case Local{_, q, _} <> rest: Bool.or(Bool.and(Q.equal(q, Q.One{}), Q.used(level, uses)), linear_definition(rest, F.Int63.pred(level), uses))

def let_finish(+outer: ctx, +inner: ctx, +mode: Q.t, linear: Bool, def_uses: Q.usage, answer: R.inferred) -> R.program<ctx, R.inferred>:
  match answer:
    case F.Pair2{ty, uses}: R.pmap(ctx, Q.usage, R.inferred, free => F.Pair2{ty, Q.sequence(Q.scale(mode, def_uses), free)}, R.lift(ctx, Q.usage, close(linear, inner, size(outer), mode, uses)))

def let_bound(+outer: ctx, +mode: Q.t, +inner: ctx, uses: Q.usage, linear: Bool, body: ctx -> R.program<ctx, R.inferred>) -> R.program<ctx, R.inferred>:
  R.pbind(ctx, R.inferred, R.inferred, body(inner), answer => let_finish(outer, inner, mode, linear, uses, answer))

def inferred_universe(+c: ctx, value: R.inferred) -> R.program<ctx, L.t>:
  match value:
    case F.Pair2{ty, _}: R.pbind(ctx, V.t, L.t, R.lift(ctx, V.t, EV.whnf(globals(c), ty)), +w => R.crequire(ctx, L.t, V.as_univ(w), E.Universe{String.concat(["a term used as a type is not a universe:  ", pp_value(c, w)])}))

def check_former(+c: ctx, s: S.t<T.t>, d: T.t, expected: V.t, right: Bool) -> R.program<ctx, Q.usage>:
  R.pbind(ctx, V.t, Q.usage, R.lift(ctx, V.t, EV.whnf(globals(c), expected)), +w =>
    R.pbind(ctx, L.t, Q.usage, R.crequire(ctx, L.t, V.as_univ(w), E.Universe{String.concat(["a type former is checked against a type that is not a universe:  ", pp_value(c, w)])}), +l0 =>
      R.pbind(ctx, L.t, Q.usage, R.form(ctx, ops, c, s, d, Some{l0}, right), +l =>
        R.pmap(ctx, Unit, Q.usage, _ => Q.empty, R.guard(ctx, L.equal(l, l0), E.Universe{String.concat(["the former lives at ", L.to_string(l), " and the expected universe is ", L.to_string(l0)])})))))

def inferred_check(+c: ctx, expected: V.t, value: R.inferred) -> R.program<ctx, Q.usage>:
  match value:
    case F.Pair2{got, uses}: R.pmap(ctx, Unit, Q.usage, _ => uses, ensure(c, got, expected))

def inferred_uses(value: R.inferred) -> Q.usage:
  match value:
    case F.Pair2{_, uses}: uses

def infer_then_check(+c: ctx, mode: Q.t, tm: T.t, expected: V.t) -> R.program<ctx, Q.usage>:
  R.pbind(ctx, R.inferred, Q.usage, infer_request(c, mode, tm), value => inferred_check(c, expected, value))

def inferred_answer(value: R.inferred) -> R.answer:
  match value:
    case F.Pair2{ty, uses}: R.InferredAnswer{ty, uses}

type fuelled<-A: Data> is Data:
  Fuelled{answer: R.result(A), remaining: B.t}

def with_fuel(~A: Data, -D: Data, value: fuelled<A>, next: R.result(A) -> B.t -> fuelled<D>) -> fuelled<D>:
  match value:
    case Fuelled{answer, remaining}: next(answer, remaining)

def poll(job: R.task<ctx>, budget: B.t) -> R.result(B.t):
  match job:
    case R.InferUses{_, _, _}: B.spend("the check budget is exhausted", budget)
    case R.CheckUses{_, _, _, _}: Done{budget}
    case R.InferUniverse{_, _}: Done{budget}
    case R.Compare{_, _, _, _}: Done{budget}
    case R.Evaluate{_, _, _}: Done{budget}
    case R.Weak{_, _}: Done{budget}
    case R.Readback{_, _, _}: Done{budget}

def polled(budget: B.t, value: R.result(B.t), next: B.t -> fuelled<R.answer>) -> fuelled<R.answer>:
  match value:
    case Fail{e}: Fuelled{Fail{e}, budget}
    case Done{remaining}: next(remaining)
# Each inference spends shared fuel; failure preserves the remaining budget.
# Requests and resumptions traverse the finite checking derivation.

def result_of(-A: Data, value: fuelled<A>) -> R.result(A):
  match value:
    case Fuelled{answer, _}: answer

def inferred_type(value: R.inferred) -> V.t:
  match value:
    case F.Pair2{ty, _}: ty

type kind is Data:
  Definition{}
  Postulate{}

type decl is Data:
  Declaration{name: String, kind: kind, ty: T.t, body: Maybe<&2, T.t>}

def declaration_name(d: decl) -> String:
  match d:
    case Declaration{n, _, _, _}: n

def declaration_body(c: ctx, n: String, kind: kind, ty: T.t, body: Maybe<&2, T.t>, tyv: V.t) -> R.program<ctx, G.entry>:
  match kind:
    case Postulate{}: R.Pure{G.Axiom{G.Postulate{ty}}}
    case Definition{}:
      R.pbind(ctx, T.t, G.entry, R.crequire(ctx, T.t, body, E.Cannot_infer{String.concat(["the definition ", n, " has no body"])}), +tm =>
        R.pmap(ctx, Q.usage, G.entry, _ => G.Def{G.Definition{ty, tm, True{}, None{}, False{}}}, check_request(c, Q.One{}, tm, tyv)))

def check_decl_program(+c: ctx, d: decl) -> R.program<ctx, G.entry>:
  match d:
    case Declaration{n, k, +ty, body}:
      R.pbind(ctx, L.t, G.entry, univ_request(c, ty), _ =>
        R.pbind(ctx, V.t, G.entry, R.c_eval(ctx, ops, c, ty), tyv => declaration_body(c, n, k, ty, body, tyv)))

def declarations_program(ds: List<&2, decl>, +g: G.t, +b: B.t) -> R.program<ctx, List<&2, F.Pair2<String, G.entry>>>:
  match ds:
    case []: R.Pure{[]}
    case +d <> rest:
      R.pbind(ctx, G.entry, List<&2, F.Pair2<String, G.entry>>, check_decl_program(make(g, b), d), +entry =>
        R.pmap(ctx, List<&2, F.Pair2<String, G.entry>>, List<&2, F.Pair2<String, G.entry>>, tail => F.Pair2{declaration_name(d), entry} <> tail, declarations_program(rest, G.add(declaration_name(d), entry, g), b)))

type family_decl is Data:
  FamilyDecl{name: String, params: Pos.telescope, indices: Pos.telescope, level: L.t}

type ctor_decl is Data:
  CtorDecl{name: String, args: Pos.telescope, params: List<&2, T.t>, indices: List<&2, T.t>}

def check_tele_loop(tele: Pos.telescope, +c: ctx) -> R.program<ctx, ctx>:
  match tele:
    case []: R.Pure{c}
    case F.Pair2{q, F.Pair2{x, +ty}} <> rest:
      R.pbind(ctx, L.t, ctx, univ_request(c, ty), _ => R.pbind(ctx, V.t, ctx, R.c_eval(ctx, ops, c, ty), tyv => check_tele_loop(rest, bind(x, q, tyv, c))))

def check_telescope(c: ctx, tele: Pos.telescope) -> R.program<ctx, ctx>: check_tele_loop(tele, c)

def index_rules(+c: ctx, +name: String, +level: L.t, entry: F.Pair2<Q.t, F.Pair2<String, T.t>>) -> R.program<ctx, Unit>:
  match entry:
    case F.Pair2{+q, F.Pair2{+x, ty}}:
      R.pbind(ctx, Unit, Unit, R.guard(ctx, Q.equal(q, Q.Zero{}), E.Index_not_zero{String.concat(["the index ", x, " of ", name, " is at quantity ", Q.to_string(q), " and every index binder is at 0"])}), _ =>
        R.pbind(ctx, L.t, Unit, univ_request(c, ty), +l => R.guard(ctx, L.le(l, level), E.Index_above_universe{String.concat(["the index ", x, " of ", name, " lives at ", L.to_string(l), " and ", name, " is declared at ", L.to_string(level)])})))

def check_index_loop(tele: Pos.telescope, +c: ctx, +name: String, +level: L.t) -> R.program<ctx, ctx>:
  match tele:
    case []: R.Pure{c}
    case +e <> rest:
      R.pbind(ctx, Unit, ctx, index_rules(c, name, level, e), _ => R.pbind(ctx, ctx, ctx, check_telescope(c, [e]), inner => check_index_loop(rest, inner, name, level)))

def check_index_telescope(c: ctx, name: String, level: L.t, tele: Pos.telescope) -> R.program<ctx, ctx>: check_index_loop(tele, c, name, level)

def absent_family(family: Maybe<&2, Pos.family>) -> Bool:
  match family:
    case None{}: True{}
    case Some{_}: False{}

def declare_family_program(+g: G.t, b: B.t, d: family_decl) -> R.program<ctx, G.t>:
  match d:
    case FamilyDecl{+n, +ps, +ix, +level}:
      R.pbind(ctx, Unit, G.t, R.guard(ctx, absent_family(G.find_family(n, g)), E.Mismatch{String.concat(["the family ", n, " is already declared"])}), _ =>
        R.pbind(ctx, ctx, G.t, check_telescope(make(g, b), ps), pctx =>
          R.pmap(ctx, ctx, G.t, _ => G.add_family(n, Pos.Family{n, ps, ix, level, Pos.Provisional{}, [], False{}}, g), check_index_telescope(pctx, n, level, ix))))

def parameter_at(+index: F.Int63.t, tm: T.t) -> Bool:
  match tm:
    case T.Var{i}: F.Int63.equal(i, index)
    case T.Ann{body, _}: parameter_at(index, body)
    case T.Univ{_}: False{}
    case T.Lan{_, _}: False{}
    case T.Ran{_, _}: False{}
    case T.In{_, _, _}: False{}
    case T.Elim{_}: False{}
    case T.Sec{_, _}: False{}
    case T.Out{_, _, _}: False{}
    case T.Let{_, _, _, _}: False{}
    case T.Global{_}: False{}
    case T.Lit{_}: False{}
    case T.Auto{}: False{}

def parameters_at(params: List<&2, T.t>, +index: F.Int63.t) -> Bool:
  match params:
    case []: True{}
    case tm <> rest: Bool.and(parameter_at(index, tm), parameters_at(rest, F.Int63.pred(index)))

def check_field_loop(tele: Pos.telescope, +c: ctx, +name: String, +level: L.t) -> R.program<ctx, ctx>:
  match tele:
    case []: R.Pure{c}
    case F.Pair2{+q, F.Pair2{+x, +ty}} <> rest:
      R.pbind(ctx, L.t, ctx, univ_request(c, ty), l =>
        R.pbind(ctx, Unit, ctx, R.guard(ctx, L.le(l, level), E.Universe{String.concat(["a field of ", name, " exceeds its family universe"])}), _ =>
          R.pbind(ctx, ctx, ctx, check_telescope(c, [F.Pair2{q, F.Pair2{x, ty}}]), inner => check_field_loop(rest, inner, name, level))))

def check_ctor(c: ctx, fam: Pos.family, +group: List<&2, String>, cd: ctor_decl) -> R.program<ctx, Pos.ctor>:
  match fam, cd:
    case Pos.Family{+name, +ps, +ix, level, _, _, _}, CtorDecl{+ctor, +args, +params, +indices}:
      R.pbind(ctx, ctx, Pos.ctor, check_field_loop(args, c, ctor, level), +actx =>
        R.pbind(ctx, Unit, Pos.ctor, R.lift(ctx, Unit, Pos.ctor_fields(group, args)), _ =>
          R.pbind(ctx, Unit, Pos.ctor, R.guard(ctx, Bool.and(F.Int63.equal(R.length(T.t, params), R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, ps)), parameters_at(params, F.Int63.pred(F.Int63.add(R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, args), R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, ps))))), E.Mismatch{String.concat(["the constructor ", ctor, " must preserve its family parameters"])}), _ =>
            R.pbind(ctx, List<&2, V.t>, Pos.ctor, R.mu_telescope(ctx, ops, actx, Q.Zero{}, String.append("the parameters of ", ctor), ps, params, []), penv =>
              R.pbind(ctx, Unit, Pos.ctor, R.guard(ctx, F.Int63.equal(R.length(T.t, indices), R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, ix)), E.Mismatch{String.concat([ctor, " gives ", F.Int63.show(R.length(T.t, indices)), " result indices and ", name, " takes ", F.Int63.show(R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, ix))])}), _ =>
                R.pmap(ctx, List<&2, V.t>, Pos.ctor, _ => Pos.Ctor{ctor, args, indices, F.Int63.add(R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, ps), R.length(F.Pair2<Q.t, F.Pair2<String, T.t>>, args)), Pos.self_rec(group, args)}, R.mu_telescope(ctx, ops, actx, Q.Zero{}, String.append("the result indices of ", ctor), ix, indices, penv)))))))

def ctors_program(cds: List<&2, ctor_decl>, +c: ctx, +fam: Pos.family, +group: List<&2, String>) -> R.program<ctx, List<&2, Pos.ctor>>:
  match cds:
    case []: R.Pure{[]}
    case cd <> rest: R.pbind(ctx, Pos.ctor, List<&2, Pos.ctor>, check_ctor(c, fam, group, cd), ctor => R.pmap(ctx, List<&2, Pos.ctor>, List<&2, Pos.ctor>, tail => ctor <> tail, ctors_program(rest, c, fam, group)))

def provisional(f: Pos.family) -> Bool:
  match f:
    case Pos.Family{_, _, _, _, status, _, _}:
      match status:
        case Pos.Provisional{}: True{}
        case Pos.Builtin{}: False{}
        case Pos.Complete{_}: False{}

def ctor_names(ctors: List<&2, Pos.ctor>) -> List<&2, String>:
  match ctors:
    case []: []
    case c <> rest: Pos.ctor_name(c) <> ctor_names(rest)

def complete_family(f: Pos.family, +ctors: List<&2, Pos.ctor>) -> Pos.family:
  match f:
    case Pos.Family{n, ps, ix, level, _, _, _}: Pos.Family{n, ps, ix, level, Pos.Complete{ctor_names(ctors)}, ctors, True{}}

def define_ctors_program(+g: G.t, b: B.t, group: List<&2, String>, +name: String, cds: List<&2, ctor_decl>) -> R.program<ctx, G.t>:
  R.pbind(ctx, Pos.family, G.t, R.crequire(ctx, Pos.family, G.find_family(name, g), E.Unbound{String.concat(["the family ", name, " is not declared"])}), +fam =>
    R.pbind(ctx, Unit, G.t, R.guard(ctx, provisional(fam), E.Mismatch{String.concat(["the constructors of ", name, " are already installed"])}), _ =>
      R.pbind(ctx, ctx, G.t, check_telescope(make(g, b), R.family_params(fam)), pctx =>
        R.pmap(ctx, List<&2, Pos.ctor>, G.t, ctors => G.add_family(name, complete_family(fam, ctors), g), ctors_program(cds, pctx, fam, group)))))

def let_continue(+c: ctx, +mode: Q.t, x: String, ty: V.t, value: V.t, +uses: Q.usage, +linear: Bool, body: ctx -> R.program<ctx, R.inferred>) -> R.program<ctx, R.inferred>:
  let_bound(c, mode, define(x, F.choose(Q.t, Q.equal(mode, Q.Zero{}), Q.Zero{}, F.choose(Q.t, linear, Q.One{}, Q.Many{})), ty, value, c), uses, linear, body)

def let_body(+c: ctx, +mode: Q.t, x: String, +ty: T.t, +definition: T.t, body: ctx -> R.program<ctx, R.inferred>) -> R.program<ctx, R.inferred>:
  R.pbind(ctx, L.t, R.inferred, univ_request(c, ty), _ =>
    R.pbind(ctx, V.t, R.inferred, R.c_eval(ctx, ops, c, ty), +tyv =>
      R.pbind(ctx, Q.usage, R.inferred, check_request(c, Q.runtime(mode), definition, tyv), +uses =>
        R.pbind(ctx, V.t, R.inferred, R.c_eval(ctx, ops, c, definition), v => let_continue(c, mode, x, tyv, v, uses, linear_definition(locals(c), F.Int63.pred(size(c)), uses), body)))))

def infer_node(+c: ctx, +mode: Q.t, tm: T.t) -> R.program<ctx, R.inferred>:
  match tm:
    case T.Var{+ix}: R.pbind(ctx, local, R.inferred, R.crequire(ctx, local, R.at(local, ix, locals(c)), E.Unbound{String.concat(["de Bruijn index ", F.Int63.show(ix), " is outside the context"])}), entry => infer_local(c, mode, ix, entry))
    case T.Univ{l}: R.Pure{F.Pair2{V.VUniv{L.succ(l)}, Q.empty}}
    case T.Lan{s, d}: R.pmap(ctx, L.t, R.inferred, l => F.Pair2{V.VUniv{l}, Q.empty}, R.form(ctx, ops, c, s, d, None{}, False{}))
    case T.Ran{s, d}: R.pmap(ctx, L.t, R.inferred, l => F.Pair2{V.VUniv{l}, Q.empty}, R.form(ctx, ops, c, s, d, None{}, True{}))
    case T.Out{s, a, head}: R.elim_out(ctx, ops, c, mode, s, a, head)
    case T.Elim{e}: R.elim_elim(ctx, ops, c, mode, e, None{})
    case T.Let{x, ty, d, body}: let_body(c, mode, x, ty, d, inner => infer_request(inner, mode, body))
    case T.Ann{tm, +ty}: R.pbind(ctx, L.t, R.inferred, univ_request(c, ty), _ => R.pbind(ctx, V.t, R.inferred, R.c_eval(ctx, ops, c, ty), +tyv => R.pmap(ctx, Q.usage, R.inferred, uses => F.Pair2{tyv, uses}, check_request(c, mode, tm, tyv))))
    case T.Global{n}: R.lift(ctx, R.inferred, R.map_result(V.t, R.inferred, ty => F.Pair2{ty, Q.empty}, head_ty(c, V.HGlobal{n})))
    case T.Lit{lit}: infer_literal(c, lit)
    case T.In{_, _, _}: R.Abort{no_infer("an injection")}
    case T.Sec{_, _}: R.Abort{no_infer("a section")}
    case T.Auto{}: R.Abort{E.Not_yet{R.auto_word}}

def check_node(+c: ctx, +mode: Q.t, tm: T.t, +expected: V.t) -> R.program<ctx, Q.usage>:
  match tm:
    case T.Sec{s, legs}: R.intro_sec(ctx, ops, c, mode, s, legs, expected)
    case T.In{s, a, args}: R.intro_in(ctx, ops, c, mode, s, a, args, expected)
    case T.Elim{e}: R.pbind(ctx, R.inferred, Q.usage, R.elim_elim(ctx, ops, c, mode, e, Some{expected}), value => inferred_check(c, expected, value))
    case T.Lan{s, d}: check_former(c, s, d, expected, False{})
    case T.Ran{s, d}: check_former(c, s, d, expected, True{})
    case T.Let{x, ty, d, body}: R.pmap(ctx, R.inferred, Q.usage, inferred_uses, let_body(c, mode, x, ty, d, inner => R.pmap(ctx, Q.usage, R.inferred, uses => F.Pair2{expected, uses}, check_request(inner, mode, body, expected))))
    case T.Var{ix}: infer_then_check(c, mode, T.Var{ix}, expected)
    case T.Univ{l}: infer_then_check(c, mode, T.Univ{l}, expected)
    case T.Out{s, a, head}: infer_then_check(c, mode, T.Out{s, a, head}, expected)
    case T.Ann{t, ty}: infer_then_check(c, mode, T.Ann{t, ty}, expected)
    case T.Global{n}: infer_then_check(c, mode, T.Global{n}, expected)
    case T.Lit{l}: infer_then_check(c, mode, T.Lit{l}, expected)
    case T.Auto{}: infer_then_check(c, mode, T.Auto{}, expected)

def dispatch(job: R.task<ctx>) -> R.program<ctx, R.answer>:
  match job:
    case R.InferUses{c, q, t}: R.pmap(ctx, R.inferred, R.answer, inferred_answer, infer_node(c, q, t))
    case R.CheckUses{c, q, t, ty}: R.pmap(ctx, Q.usage, R.answer, uses => R.UsageAnswer{uses}, check_node(c, q, t, ty))
    case R.InferUniverse{+c, t}: R.pmap(ctx, L.t, R.answer, l => R.LevelAnswer{l}, R.pbind(ctx, R.inferred, L.t, infer_request(c, Q.Zero{}, t), value => inferred_universe(c, value)))
    case R.Compare{c, ty, a, b}: R.pmap(ctx, Bool, R.answer, eq => R.BoolAnswer{eq}, CV.conv_step(ctx, runtime_ops, c, ty, a, b))
    case R.Evaluate{c, e, t}: R.lift(ctx, R.answer, R.map_result(V.t, R.answer, v => R.ValueAnswer{v}, EV.eval(globals(c), e, t)))
    case R.Weak{c, v}: R.lift(ctx, R.answer, R.map_result(V.t, R.answer, w => R.ValueAnswer{w}, EV.whnf(globals(c), v)))
    case R.Readback{c, s, v}: R.lift(ctx, R.answer, R.map_result(T.t, R.answer, t => R.TermAnswer{t}, EV.quote(globals(c), s, v)))

@unsafe
def drive(-A: Data, value: R.program<ctx, A>, +remaining: B.t) -> fuelled<A>:
  match value:
    case R.Pure{x}: Fuelled{Done{x}, remaining}
    case R.Abort{e}: Fuelled{Fail{e}, remaining}
    case R.Request{+job, resume}:
      with_fuel(R.answer, A, polled(remaining, poll(job, remaining), next => drive(R.answer, dispatch(job), next)), answer => next => drive(A, resume(answer), next))

def infer_with_budget(+c: ctx, mode: Q.t, tm: T.t) -> fuelled<V.t>:
  drive(V.t, R.pmap(ctx, R.inferred, V.t, inferred_type, infer_request(c, Q.runtime(mode), tm)), budget(c))

def infer(c: ctx, mode: Q.t, tm: T.t) -> R.result(V.t): result_of(V.t, infer_with_budget(c, mode, tm))

def check_with_budget(+c: ctx, mode: Q.t, tm: T.t, expected: V.t) -> fuelled<Unit>:
  drive(Unit, R.pmap(ctx, Q.usage, Unit, _ => Unit{}, check_request(c, Q.runtime(mode), tm, expected)), budget(c))

def check(c: ctx, mode: Q.t, tm: T.t, expected: V.t) -> R.result(Unit): result_of(Unit, check_with_budget(c, mode, tm, expected))

def infer_univ(+c: ctx, tm: T.t) -> R.result(L.t): result_of(L.t, drive(L.t, univ_request(c, tm), budget(c)))

def conv_type(+c: ctx, a: V.t, b: V.t) -> R.result(Bool): result_of(Bool, drive(Bool, R.ask_type(ctx, c, a, b), budget(c)))

def infer_term(globals: G.t, budget: B.t, tm: T.t) -> R.result(V.t): infer(make(globals, budget), Q.Many{}, tm)

def check_term(globals: G.t, budget: B.t, tm: T.t, ty: V.t) -> R.result(Unit): check(make(globals, budget), Q.Many{}, tm, ty)

def check_decl(g: G.t, +b: B.t, d: decl) -> R.result(G.entry): result_of(G.entry, drive(G.entry, check_decl_program(make(g, b), d), b))

def check_decls(g: G.t, +b: B.t, ds: List<&2, decl>) -> R.result(List<&2, F.Pair2<String, G.entry>>): result_of(List<&2, F.Pair2<String, G.entry>>, drive(List<&2, F.Pair2<String, G.entry>>, declarations_program(ds, g, b), b))

def declare_family(g: G.t, +b: B.t, d: family_decl) -> R.result(G.t): result_of(G.t, drive(G.t, declare_family_program(g, b, d), b))

def define_ctors(g: G.t, +b: B.t, group: List<&2, String>, name: String, cds: List<&2, ctor_decl>) -> R.result(G.t): result_of(G.t, drive(G.t, define_ctors_program(g, b, group, name, cds), b))
