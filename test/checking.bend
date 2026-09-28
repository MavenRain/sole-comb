import Base
import ../lib/foundation.bend as F
import ../lib/kernel_budget.bend as B
import ../lib/kernel_check.bend as K
import ../lib/kernel_error.bend as E
import ../lib/kernel_eval.bend as EV
import ../lib/kernel_global.bend as G
import ../lib/kernel_level.bend as L
import ../lib/kernel_pp.bend as PP
import ../lib/kernel_quantity.bend as Q
import ../lib/kernel_rules.bend as R
import ../lib/kernel_term.bend as T
import ../lib/kernel_value.bend as V

def ck_ctx(g: G.t) -> K.ctx: K.make(g, B.create(512n))
def ck_budget(g: G.t, n: Nat) -> K.ctx: K.make(g, B.create(n))
def ck_error(e: E.t) -> String: String.append("error:", E.to_string(e))
def ck_unit(answer: R.result(Unit)) -> String:
  match answer:
    case Done{_}: "ok"
    case Fail{e}: ck_error(e)
def ck_value(c: K.ctx, answer: R.result(V.t)) -> String:
  match answer:
    case Done{v}: String.append("ok:", K.pp_value(c, v))
    case Fail{e}: ck_error(e)
def ck_infer(+c: K.ctx, q: Q.t, t: T.t) -> String: ck_value(c, K.infer(c, q, t))
def ck_check(+c: K.ctx, q: Q.t, t: T.t, ty: T.t) -> String:
  ck_unit(R.bind(V.t, Unit, EV.eval(K.globals(c), K.env(c), ty), v => K.check(c, q, t, v)))
def ck_level(answer: R.result(L.t)) -> String:
  match answer:
    case Done{n}: String.append("ok:", L.to_string(n))
    case Fail{e}: ck_error(e)
def ck_univ(c: K.ctx, t: T.t) -> String: ck_level(K.infer_univ(c, t))
def ck_bool(b: Bool) -> String:
  match b:
    case True{}: "true"
    case False{}: "false"
def ck_range(r: Q.interval_t) -> String:
  match r:
    case F.Pair2{lo, hi}: String.concat([Q.to_string(lo), ":", Q.to_string(hi)])
def ck_usage(+u: Q.usage, +level: F.Int63.t) -> String:
  String.join([ck_range(Q.get(level, u)), ck_bool(Q.exactly_once(level, u)), ck_bool(Q.at_most_once(level, u)), ck_bool(Q.used(level, u))], ",")
def ck_entries(xs: List<&2, F.Pair2<String, G.entry>>) -> String:
  match xs:
    case []: ""
    case F.Pair2{n, _} <> rest: String.concat([n, ";", ck_entries(rest)])
def ck_decls_result(answer: R.result(List<&2, F.Pair2<String, G.entry>>)) -> String:
  match answer:
    case Done{xs}: String.append("ok:", ck_entries(xs))
    case Fail{e}: ck_error(e)
def ck_decls(g: G.t, ds: List<&2, K.decl>) -> String: ck_decls_result(K.check_decls(g, B.create(512n), ds))
def ck_global(answer: R.result(G.t)) -> String:
  match answer:
    case Done{_}: "ok"
    case Fail{e}: ck_error(e)
def ck_family(g: G.t, d: K.family_decl) -> String: ck_global(K.declare_family(g, B.create(512n), d))
def ck_family_twice(g: G.t, +d: K.family_decl) -> String:
  ck_global(R.bind(G.t, G.t, K.declare_family(g, B.create(512n), d), next => K.declare_family(next, B.create(512n), d)))
def ck_install(g: G.t, d: K.family_decl, group: List<&2, String>, name: String, cs: List<&2, K.ctor_decl>) -> R.result(G.t):
  R.bind(G.t, G.t, K.declare_family(g, B.create(512n), d), next => K.define_ctors(next, B.create(512n), group, name, cs))
def ck_ctors(g: G.t, d: K.family_decl, group: List<&2, String>, name: String, cs: List<&2, K.ctor_decl>) -> String:
  ck_global(ck_install(g, d, group, name, cs))
def ck_next_infer(answer: R.result(G.t), t: T.t) -> String:
  match answer:
    case Fail{e}: ck_error(e)
    case Done{next}: ck_infer(ck_ctx(next), Q.Many{}, t)
def ck_family_infer(g: G.t, d: K.family_decl, group: List<&2, String>, name: String, cs: List<&2, K.ctor_decl>, t: T.t) -> String:
  ck_next_infer(ck_install(g, d, group, name, cs), t)
def ck_next_check(answer: R.result(G.t), t: T.t, ty: T.t) -> String:
  match answer:
    case Fail{e}: ck_error(e)
    case Done{next}: ck_check(ck_ctx(next), Q.Many{}, t, ty)
def ck_family_check(g: G.t, d: K.family_decl, group: List<&2, String>, name: String, cs: List<&2, K.ctor_decl>, t: T.t, ty: T.t) -> String:
  ck_next_check(ck_install(g, d, group, name, cs), t, ty)
def ck_install_twice(g: G.t, d: K.family_decl, +group: List<&2, String>, +name: String, +cs: List<&2, K.ctor_decl>) -> String:
  ck_global(R.bind(G.t, G.t, ck_install(g, d, group, name, cs), next => K.define_ctors(next, B.create(512n), group, name, cs)))
def ck_provisional(g: G.t, d: K.family_decl, t: T.t) -> String:
  ck_next_infer(K.declare_family(g, B.create(512n), d), t)
