"""Raw Bend kernel inputs checked against pinned reference fixtures."""
from dataclasses import dataclass
import json
import random


@dataclass(frozen=True)
class Expr:
    bend: str


def text(s):
    return Expr(json.dumps(s))


def integer(n):
    remaining, digits = abs(n), []
    while remaining:
        digits.append(str(remaining % 32768))
        remaining //= 32768
    return Expr(f'F.Int63.Int{{{"True" if n < 0 else "False"}{{}}, [{", ".join(digits)}]}}')


def number(n):
    return Expr(f'FT.number({json.dumps(str(abs(n)))})' if n >= 0 else f'F.Bignum.negate(FT.number({json.dumps(str(-n))}))')


def level(n):
    if n < 0:
        raise ValueError(f"the pinned Level interface has no negative level: {n}")
    return Expr(number(n).bend)


def nat(n):
    return Expr(f"{n}n")


def boolean(b):
    return Expr("True{}" if b else "False{}")


def seq(xs):
    return Expr("[" + ", ".join(x.bend for x in xs) + "]")


def ctor(name, *args):
    return Expr(name + "{" + ", ".join(x.bend for x in args) + "}")


def record(name, fields, *args):
    return Expr(ctor(name, *args).bend)


def pair(a, b):
    return Expr(f"F.Pair2{{{a.bend}, {b.bend}}}")


def call(name, *args):
    return Expr(name + "(" + ", ".join(x.bend for x in args) + ")")


NONE = Expr("None{}")
ZERO, ONE, MANY = (ctor("Q." + q) for q in ("Zero", "One", "Many"))
GLOBALS = Expr("G.initial")
EMPTY = Expr("G.empty")


def some(x):
    return Expr(f"Some{{{x.bend}}}")


def var(i): return ctor("T.Var", integer(i))
def univ(i=1): return ctor("T.Univ", level(i))
def lit(i): return ctor("T.Lit", ctor("Lit.LInt", number(i)))
def vlit(i): return ctor("V.VLit", ctor("Lit.LInt", number(i)))
def vu(i=1): return ctor("V.VUniv", level(i))
def coll(i): return ctor("S.SColl", integer(i))
def pi(dom=None, q=MANY, name="x"): return ctor("S.SPi", q, text(name), univ() if dom is None else dom)
def mu(indices=(), name="F"): return ctor("S.SMu", text(name), seq(indices))
def pt(t, q=ONE): return ctor("T.APt", q, t)
def vpt(v, q=ONE): return ctor("V.VAPt", q, v)
def legaddr(i): return ctor("T.ALeg", integer(i))
def ctoraddr(n="c"): return ctor("T.ACtor", text(n))
def global_(n): return ctor("T.Global", text(n))
def binders(names): return seq([pair(q, text(n)) for q, n in names])
def leg(body, bs=()): return record("T.Leg", ("T.l_binders", "T.l_body"), binders(bs), body)
def clo(body, env=()): return record("V.Closure", ("V.env", "V.body"), seq(env), body)
def vleg(body, bs=(), env=()): return record("V.VLeg", ("V.vl_binders", "V.vl_clo"), binders(bs), clo(body, env))
def former(tag, s, d, u=NONE, env=()): return ctor("V.V" + tag, s, clo(d, env), u)
def neutral(name="a", spine=()): return ctor("V.VNeutral", ctor("V.HGlobal", text(name)), seq(spine))
def local(n): return ctor("V.VNeutral", ctor("V.HLocal", integer(n)), seq([]))
def section(s, legs): return ctor("T.Sec", s, seq(legs))
def out(s, a, t): return ctor("T.Out", s, a, t)
def app(f, x): return out(pi(), pt(x), f)
def inj(s, a, args): return ctor("T.In", s, a, seq(args))
def motive(body, idx=(), self="self", ind=NONE): return record("T.Motive", ("T.m_ind", "T.m_idx", "T.m_self", "T.m_body"), ind, seq([text(n) for n in idx]), text(self), body)
def elim(s, scrut, branches, mo=NONE, q=ONE):
    return ctor("T.Elim", record("T.Elimination", ("T.e_shape", "T.e_scrut", "T.e_scrut_q", "T.e_motive", "T.e_branches"), s, scrut, q, mo, seq([pair(a, l) for a, l in branches])))
def stuck(s, branches=(), mo=NONE, q=ONE, env=()):
    return record("V.Stuck", ("V.s_shape", "V.s_scrut_q", "V.s_motive", "V.s_branches", "V.s_env"), s, q, mo, seq([pair(a, l) for a, l in branches]), seq(env))
def definition(name, body, g=GLOBALS, reducible=True, rec=None):
    d = record("G.Definition", ("G.ty", "G.def", "G.reducible", "G.rec_arg", "G.partial"), univ(), body, boolean(reducible), NONE if rec is None else some(integer(rec)), boolean(False))
    return call("G.add", text(name), ctor("G.Def", d), g)
def axiom(name, g=GLOBALS):
    return call("G.add", text(name), ctor("G.Axiom", record("G.Postulate", ("G.ax_ty",), univ())), g)
def ctx(g=GLOBALS, head=None, probe=0, exhausted=False):
    return call("C.context", g, vu() if head is None else head, nat(probe), boolean(exhausted))


def cases():
    result = []
    def add(name, expr, expected=None): result.append((name, expr, expected))
    def nf(name, term, expected=None, g=GLOBALS, env=(), size=0): add(name, call("C.nf", g, integer(size), seq(env), term), expected)
    def conv(name, a, b, expected=None, ty=None, context=None):
        add(name, call("C.conv", ctx() if context is None else context, vu() if ty is None else ty, a, b), expected)
    def quote(name, value, size=0, g=GLOBALS):
        add(name, call("C.term_result", call("EV.quote", g, integer(size), value)))
        # Module names differ only at the public observer boundary.
        name_, e, ex = result[-1]
        result[-1] = (name_, Expr(e.bend), ex)
    nf("universe", univ(4), "u4")
    nf("integer", lit(-17), "lit:-17")
    nf("string", ctor("T.Lit", ctor("Lit.LString", text("hello"))), "lit:string:hello")
    for i in (-1, 0, 1, 2, 3): nf(f"variable-{i}", var(i), env=(vlit(10), vlit(20), vlit(30)))
    nf("auto-refused", ctor("T.Auto"))
    nf("global-unbound", global_("missing"))
    nf("let-ignores-type", ctor("T.Let", text("x"), ctor("T.Auto"), lit(12), var(0)), "lit:12")
    nf("let-shadows", ctor("T.Let", text("x"), univ(), lit(12), var(1)), "lit:37", env=(vlit(37),))
    nf("global-reducible", global_("d"), "lit:41", g=definition("d", lit(41)))
    nf("global-opaque", global_("d"), "g:d", g=definition("d", lit(41), reducible=False))
    nf("global-axiom", global_("a"), "g:a", g=axiom("a"))
    shapes = [("pi", pi()), ("coll", coll(2)), ("mu", mu([lit(8)])), ("par", ctor("S.SPar", univ(), univ(2))), ("nu", ctor("S.SNu", text("F"), seq([lit(9)])))]
    for name, s in shapes:
        for tag in ("Lan", "Ran"): nf(f"former-{tag}-{name}", ctor("T." + tag, s, univ(2)))
        nf(f"in-{name}", inj(s, ctoraddr(), [lit(1), lit(2)]))
        nf(f"section-{name}", section(s, [leg(var(0), [(ONE, "x")])]))
    for annotation in (univ(3), lit(1), ctor("T.Auto"), global_("missing")):
        nf(f"annotation-{annotation.bend}", ctor("T.Ann", ctor("T.Ran", coll(0), univ()), annotation))
    nf("annotation-existing", ctor("T.Ann", ctor("T.Ann", ctor("T.Ran", coll(0), univ()), univ(3)), univ(7)))
    nf("annotation-nonformer", ctor("T.Ann", lit(2), ctor("T.Auto")), "lit:2")
    for body in (0, 1):
        nf(f"pi-beta-{body}", app(section(pi(), [leg(var(body), [(ONE, "x")])]), lit(7)), env=(vlit(13),))
    nf("pi-beta-wrong-address", out(pi(), legaddr(0), section(pi(), [leg(lit(1))])))
    nf("pi-beta-multiple-legs", app(section(pi(), [leg(lit(1)), leg(lit(2))]), lit(7)))
    for i in (-1, 0, 1, 2): nf(f"coll-beta-{i}", out(coll(2), legaddr(i), section(coll(2), [leg(lit(11)), leg(lit(22))])))
    nf("coll-beta-wrong-address", out(coll(1), pt(lit(0)), section(coll(1), [leg(lit(1))])))
    nf("out-canonical-refused", out(pi(), pt(lit(1)), lit(3)))
    nf("out-mu-refused", out(mu(), ctoraddr(), inj(mu(), ctoraddr(), [])))
    for i in range(3):
        nf(f"pi-elim-order-{i}", elim(pi(), inj(pi(), pt(lit(11)), [lit(22)]), [(ctoraddr("ignored"), leg(var(i), [(ONE,"p"),(ONE,"v")]))]), env=(vlit(33),))
    nf("pi-elim-arity", elim(pi(), inj(pi(), pt(lit(11)), [lit(22),lit(33)]), [(pt(lit(0)), leg(lit(1)))]))
    for name,s,address,wrong in (("coll",coll(2),legaddr(1),legaddr(0)),("mu",mu(),ctoraddr("c"),ctoraddr("d"))):
        for i in range(4): nf(f"{name}-elim-order-{i}", elim(s,inj(s,address,[lit(10),lit(20),lit(30)]),[(wrong,leg(lit(99))),(address,leg(var(i),[(ONE,"a"),(ONE,"b"),(ONE,"c")]))]),env=(vlit(40),))
        nf(f"{name}-elim-missing",elim(s,inj(s,address,[]),[(wrong,leg(lit(99)))]))
        nf(f"{name}-elim-first",elim(s,inj(s,address,[]),[(address,leg(lit(11))),(address,leg(lit(22)))]),"lit:11")
    nf("elim-canonical-refused",elim(coll(0),lit(1),[]))
    neutral_g = axiom("a")
    nf("frozen-out-order", out(coll(2),legaddr(1),app(global_("a"),lit(7))),g=neutral_g)
    nf("frozen-elim-capture",elim(coll(2),global_("a"),[(legaddr(1),leg(var(0)))],some(motive(var(1),["i"])),MANY),g=neutral_g,env=(vlit(8),),size=1)
    for name in ("natAdd","natSub","natMul","natEq","natLt"):
        for a,b in ((9,4),(4,9),(4,4),(-1,3),(3,-1),(10**25,7)):
            nf(f"primitive-{name}-{a}-{b}",app(app(global_(name),lit(a)),lit(b)))
        nf(f"primitive-{name}-partial",app(global_(name),lit(3)))
    nf("primitive-overapplication",app(app(app(global_("natSub"),lit(9)),lit(4)),lit(2)))
    nf("primitive-wrong-literal",app(app(global_("natAdd"),ctor("T.Lit",ctor("Lit.LString",text("s")))),lit(4)))
    for index in (0,1,-1):
        body=section(pi(),[leg(section(pi(),[leg(lit(73),[(ONE,"b")])]),[(ONE,"a")])])
        g=definition("rec",body,rec=index)
        constructor=inj(mu(),ctoraddr(),[])
        for name,a,b in (("first",constructor,lit(2)),("second",lit(2),constructor),("neither",lit(1),lit(2))):
            nf(f"guard-{index}-{name}",app(app(global_("rec"),a),b),g=g)
        nf(f"guard-{index}-partial",app(global_("rec"),constructor),g=g)
    for size_,level_ in ((0,0),(3,0),(3,2),(0,1),(-(1<<62),(1<<62)-1),((1<<62)-1,-(1<<62))): quote(f"quote-local-{size_}-{level_}",local(level_),size=size_)
    quote("quote-closure-two-binders",ctor("V.VSec",coll(1),seq([vleg(var(1),[(ZERO,"a"),(MANY,"b")])])),size=2)
    quote("quote-captured-closure",ctor("V.VSec",coll(1),seq([vleg(var(2),[(ONE,"a"),(ONE,"b")],[vlit(27)])])))
    quote("quote-does-not-whnf",neutral("natSub",[ctor("V.SOut",pi(vu()),vpt(vlit(4))),ctor("V.SOut",pi(vu()),vpt(vlit(9)))]))
    add("weak-primitive",call("C.weak",GLOBALS,integer(0),neutral("natSub",[ctor("V.SOut",pi(vu()),vpt(vlit(4))),ctor("V.SOut",pi(vu()),vpt(vlit(9)))])),"lit:5")
    for a,b,expected in ((vu(1),vu(1),"true"),(vu(1),vu(2),"false"),(vlit(3),vlit(4),"false"),(vlit(3),vlit(3),"true"),(vu(),vlit(1),"false")):
        conv(f"structural-{len(result)}",a,b,expected)
    conv("proof-irrelevance",vlit(3),vlit(4),"true",context=ctx(probe=1))
    conv("probe-failure-falls-back",vlit(3),vlit(4),"false",context=ctx(probe=2))
    conv("budget-exhaustion",vu(),vu(),context=ctx(exhausted=True))
    for tag in ("Lan","Ran"):
        for width in (0,1):
            a=former(tag,coll(width),univ(),NONE)
            for u in (NONE,some(level(0)),some(level(2))): conv(f"former-level-{tag}-{width}-{u.bend}",a,former(tag,coll(width),univ(),u))
        conv(f"former-binder-name-{tag}",former(tag,pi(vu(),name="x"),var(0)),former(tag,pi(vu(),name="y"),var(0)),"true")
        conv(f"former-binder-quantity-{tag}",former(tag,pi(vu(),q=ONE),var(0)),former(tag,pi(vu(),q=MANY),var(0)),"false")
        conv(f"former-domain-{tag}",former(tag,pi(vu(1)),var(0)),former(tag,pi(vu(2)),var(0)),"false")
        conv(f"former-captured-diagram-{tag}",former(tag,pi(vu()),var(1),env=[vlit(3)]),former(tag,pi(vu()),lit(3)),"true")
    for shape in (coll(0),coll(2),pi(vu()),mu([vu()]),ctor("S.SPar",vu(),vu()),ctor("S.SNu",text("F"),seq([]))):
        conv(f"in-structural-{shape.bend}",ctor("V.VIn",shape,ctor("V.VACtor",text("c")),seq([vlit(1)])),ctor("V.VIn",shape,ctor("V.VACtor",text("c")),seq([vlit(1)])))
    for second in (vlit(2),vlit(3)):
        conv(f"in-second-payload-{second.bend}",ctor("V.VIn",mu(),ctor("V.VACtor",text("c")),seq([vlit(1),vlit(2)])),ctor("V.VIn",mu(),ctor("V.VACtor",text("c")),seq([vlit(1),second])))
    conv("section-binder-names-quantities-ignored",ctor("V.VSec",coll(1),seq([vleg(var(0),[(ZERO,"a")])])),ctor("V.VSec",coll(1),seq([vleg(var(0),[(MANY,"b")])])),"true")
    conv("section-binder-count",ctor("V.VSec",coll(1),seq([vleg(lit(1),[(ONE,"a")])])),ctor("V.VSec",coll(1),seq([vleg(lit(1))])),"false")
    conv("eta-empty-collection",neutral("a"),neutral("b"),"true",ty=former("Ran",coll(0),section(coll(0),[])))
    for a,b in ((neutral("a"),neutral("a")),(neutral("a"),neutral("b")),(local(0),local(1)),(local(0),neutral("a"))): conv(f"head-{len(result)}",a,b)
    pi_ty=former("Ran",pi(vu()),univ())
    for q in (ZERO,ONE,MANY):
        a=neutral("a",[ctor("V.SOut",pi(vu()),vpt(vlit(3),ONE))])
        b=neutral("a",[ctor("V.SOut",pi(vu()),vpt(vlit(3),q))])
        conv(f"spine-point-quantity-{q.bend}",a,b,"true",context=ctx(head=pi_ty))
    conv("spine-point-value",neutral("a",[ctor("V.SOut",pi(vu()),vpt(vlit(3)))]),neutral("a",[ctor("V.SOut",pi(vu()),vpt(vlit(4)))]),"false",context=ctx(head=pi_ty))
    for field in ("same","env","body","motive-count","scrut-quantity","branch-count","branch-address"):
        bs1=[(legaddr(0),leg(var(1),[(ONE,"x")]))]
        bs2=[] if field=="branch-count" else [(legaddr(1 if field=="branch-address" else 0),leg(lit(5) if field=="body" else var(1),[(MANY,"y")]))]
        s1=stuck(coll(1),bs1,some(motive(var(2),["i"])),env=[vlit(7)])
        s2=stuck(coll(1),bs2,some(motive(var(2),[] if field=="motive-count" else ["j"])),q=MANY if field=="scrut-quantity" else ONE,env=[vlit(8 if field=="env" else 7)])
        conv(f"captured-elim-{field}",neutral("a",[ctor("V.SElim",s1)]),neutral("a",[ctor("V.SElim",s2)]))
    # Eta must identify a neutral with its explicit expansion.
    g = axiom("a")
    eta_fun = ctor("V.VSec",pi(vu()),seq([vleg(app(global_("a"),var(0)),[(ONE,"z")])]))
    conv("eta-pi-ran",neutral("a"),eta_fun,"true",ty=pi_ty,context=ctx(g=g,head=pi_ty))
    conv("eta-pi-ran-different",neutral("a"),ctor("V.VSec",pi(vu()),seq([vleg(lit(8),[(ONE,"z")])])),"false",ty=pi_ty,context=ctx(g=g,head=pi_ty))
    pair_ty = former("Lan",pi(vu()),univ())
    pair_value = ctor("V.VIn",pi(vu()),vpt(vu(1)),seq([vu(2)]))
    for name,p,f in (("same",1,2),("point",2,2),("fibre",1,3)):
        conv("eta-pi-lan-"+name,pair_value,ctor("V.VIn",pi(vu()),vpt(vu(p)),seq([vu(f)])),"true" if name=="same" else "false",ty=pair_ty)
    coll_ty = former("Ran",coll(2),section(coll(2),[leg(univ()),leg(univ())]))
    eta_coll = ctor("V.VSec",coll(2),seq([vleg(out(coll(2),legaddr(i),global_("a"))) for i in range(2)]))
    conv("eta-collection",neutral("a"),eta_coll,"true",ty=coll_ty,context=ctx(g=g,head=coll_ty))
    for name,b in (("same",2),("second",3)):
        conv("eta-collection-"+name,ctor("V.VSec",coll(2),seq([vleg(lit(1)),vleg(lit(2))])),ctor("V.VSec",coll(2),seq([vleg(lit(1)),vleg(lit(b))])),"true" if name=="same" else "false",ty=coll_ty)
    # Complete positive Prop families are subsingletons only under the pin's criterion.
    for name,status,count,q,recursive,positive,lvl in (
        ("empty","Complete",0,ZERO,False,True,0),("erased","Complete",1,ZERO,False,True,0),
        ("runtime","Complete",1,ONE,False,True,0),("recursive","Complete",1,ZERO,True,True,0),
        ("two","Complete",2,ZERO,False,True,0),("negative","Complete",1,ZERO,False,False,0),
        ("type","Complete",0,ZERO,False,True,1),("provisional","Provisional",0,ZERO,False,True,0),
        ("builtin","Builtin",0,ZERO,False,True,0)):
        names=["c"+str(i) for i in range(count)]
        constructors=[record("Pos.Ctor",("Pos.c_name","Pos.c_args","Pos.c_res_idx","Pos.c_full_arity","Pos.c_self_rec"),text(n),
            Expr(seq([pair(q,pair(text("x"),univ()))]).bend),seq([]),integer(1),boolean(recursive)) for n in names]
        status_ = ctor("Pos.Complete",seq([text(n) for n in names])) if status=="Complete" else ctor("Pos."+status)
        family = record("Pos.Family",("Pos.f_name","Pos.f_params","Pos.f_indices","Pos.f_level","Pos.f_status","Pos.f_ctors","Pos.f_positive"),text("F"),seq([]),seq([]),level(lvl),status_,seq(constructors),boolean(positive))
        globals_ = call("G.add_family",text("F"),family,GLOBALS)
        conv("subsingleton-"+name,vlit(1),vlit(2),"true" if name in ("empty","erased") else "false",ty=former("Lan",mu(),univ()),context=ctx(g=globals_))
    conv("subsingleton-absent",vlit(1),vlit(2),"false",ty=former("Lan",mu(),univ()))
    # The second captured branch and point-address environment are independently observable.
    for changed in (False,True):
        left=stuck(coll(2),[(legaddr(0),leg(lit(1))),(pt(var(0)),leg(var(0)))],env=[vlit(7)])
        right=stuck(coll(2),[(legaddr(0),leg(lit(1))),(pt(lit(8 if changed else 7)),leg(lit(7)))])
        conv("captured-second-point-"+str(changed),neutral("a",[ctor("V.SElim",left)]),neutral("a",[ctor("V.SElim",right)]),"false" if changed else "true")
    conv("short-circuit-shape-before-bad-diagram",former("Ran",coll(1),ctor("T.Auto")),former("Ran",coll(2),ctor("T.Auto")),"false")
    conv("matching-shape-reports-bad-diagram",former("Ran",coll(1),ctor("T.Auto")),former("Ran",coll(1),ctor("T.Auto")))
    for tag,s in (("par",ctor("S.SPar",vu(),vu())),("nu",ctor("S.SNu",text("F"),seq([])))):
        conv("former-refusal-"+tag,former("Ran",s,univ()),former("Ran",s,univ()))
    # Mu right formers compare structurally; only mu formation and mu Out refuse at the pin.
    conv("former-mu-converts",former("Ran",mu(),univ()),former("Ran",mu(),univ()),"true")
    arithmetic=[((1<<62)-1,1),(-(1<<62),-1),(-(1<<62),(1<<62)-1),(32767,1),(-32768,1),
                ((1<<62)-1,(1<<62)-1),(-(1<<62),-(1<<62)),(0,-1),(-1,0)]
    rng=random.Random(20260928)
    arithmetic += [(rng.randrange(-(1<<62),1<<62),rng.randrange(-(1<<62),1<<62)) for _ in range(12)]
    for a,b in arithmetic:
        for op,symbol in (("add","+"),("sub","-")):
            e=call("F.Int63.show",call("F.Int63."+op,integer(a),integer(b)))
            raw=a+b if op=="add" else a-b
            expected=str(((raw+(1<<62))%(1<<63))-(1<<62))
            add(f"int63-{op}-{a}-{b}",Expr(e.bend),expected)
    return result
