"""Raw Bend kernel inputs for the A.4 reference fixture checks."""
import importlib.util
from pathlib import Path
import sys

spec = importlib.util.spec_from_file_location("sole_a4_expr", Path(__file__).with_name("evaluation-cases.py"))
H = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = H
spec.loader.exec_module(H)
Expr, call, ctor, record, seq, pair = H.Expr, H.call, H.ctor, H.record, H.seq, H.pair
text, integer, nat, level = H.text, H.integer, H.nat, H.level
univ, var, lit, global_, pi, coll, mu = H.univ, H.var, H.lit, H.global_, H.pi, H.coll, H.mu
leg, section, inj, out, elim, pt = H.leg, H.section, H.inj, H.out, H.elim, H.pt
ZERO, ONE, MANY, NONE, GLOBALS = H.ZERO, H.ONE, H.MANY, H.NONE, H.GLOBALS


def tele(xs):
    return Expr(seq([pair(q, pair(text(n), t)) for q, n, t in xs]).bend)


def family(name="F", params=(), indices=(), lev=1):
    return record("K.FamilyDecl", ("K.fam_name", "K.fam_params", "K.fam_indices", "K.fam_level"), text(name), tele(params), tele(indices), level(lev))


def constructor(name="c", args=(), params=(), indices=()):
    return record("K.CtorDecl", ("K.ct_name", "K.ct_args", "K.ct_res_params", "K.ct_res_idx"), text(name), tele(args), seq(params), seq(indices))


def declaration(name, ty, body=NONE, postulate=False):
    return record("K.Declaration", ("K.d_name", "K.d_kind", "K.d_ty", "K.d_body"), text(name), ctor("K.Postulate" if postulate else "K.Definition"), ty, body)


def cases():
    rows = []
    def add(name, expr, expected=None): rows.append((name, expr, expected))
    ctx = call("C.ck_ctx", GLOBALS)
    nt = Expr("P.nat_ty")
    def infer(name, tm, expected=None, c=ctx, q=MANY): add("infer-" + name, call("C.ck_infer", c, q, tm), expected)
    def check(name, tm, ty, expected=None, c=ctx, q=MANY): add("check-" + name, call("C.ck_check", c, q, tm, ty), expected)
    def ann(tm, ty): return ctor("T.Ann", tm, ty)
    def ran(s, d): return ctor("T.Ran", s, d)
    def lan(s, d): return ctor("T.Lan", s, d)
    def usage(name, u, n=0, expected=None): add("usage-" + name, call("C.ck_usage", u, integer(n)), expected)
    quantities = [(ZERO, "0"), (ONE, "1"), (MANY, "w")]
    for i, (a, _) in enumerate(quantities):
        for j, (b, _) in enumerate(quantities):
            for op, table in (("add", ((0,1,2),(1,2,2),(2,2,2))), ("mul", ((0,0,0),(0,1,2),(0,2,2))), ("minimum", ((0,0,0),(0,1,1),(0,1,2))), ("maximum", ((0,1,2),(1,1,2),(2,2,2)))):
                add(f"quantity-{op}-{i}-{j}", call("Q.to_string", call("Q." + op, a, b)), quantities[table[i][j]][1])
    empty, dead = Expr("Q.empty"), Expr("Q.unreachable")
    once = call("Q.occurrence", integer(0), ONE)
    twice = call("Q.sequence", once, once)
    for name, u in [("empty",empty),("unreachable",dead),("once",once),("twice",twice),("optional",call("Q.alternative",once,empty)),("dead-sequence",call("Q.sequence",once,dead)),("dead-alternative",call("Q.alternative",once,dead)),("capture",call("Q.captures",call("Q.alternative",once,dead))),("remove",call("Q.remove",integer(0),twice))]:
        usage(name,u)
        usage(name + "-absent",u,4)
    for q, name in quantities:
        usage("scale-"+name,call("Q.scale",q,twice))
        usage("scale-dead-"+name,call("Q.scale",q,dead))
    usage("range",call("Q.scale_range",pair(ZERO,MANY),once))
    other = call("Q.occurrence", integer(1), ONE)
    both = call("Q.sequence", once, other)
    for name, u in [("two-keys",both),("two-keys-reversed",call("Q.sequence",other,once)),("disjoint-alternative",call("Q.alternative",other,once)),("remove-first",call("Q.remove",integer(0),both)),("remove-second",call("Q.remove",integer(1),both))]:
        for n in range(3): usage(f"{name}-{n}",u,n)
    for name, tm, want in [("universe",univ(),"Type 1"),("free",var(3),"#3"),("negative-var",var(-1),"#-1"),("nat",lit(123),"123"),("auto",ctor("T.Auto"),"auto"),("global",global_("x"),"x")]:
        add("print-"+name,call("PP.term",seq([]),tm),want)
    printed = [ran(pi(nt),nt),lan(pi(nt),var(0)),section(pi(nt),[leg(var(0),[(ONE,"x")])]),inj(pi(nt),pt(lit(3)),[lit(4)]),out(coll(1),H.legaddr(0),var(0)),ctor("T.Let",text("x"),nt,lit(1),var(0)),ann(lit(1),nt),elim(mu(),var(0),[(H.ctoraddr(),leg(var(0),[(ONE,"x")]))],H.some(H.motive(var(1),["i"]))),ran(ctor("S.SPar",var(0),var(1)),univ()),ran(ctor("S.SNu",text("F"),seq([var(0)])),univ())]
    printed += [section(pi(nt),[leg(var(0),[(ZERO,"x"),(ONE,"y")]),leg(var(1),[(ONE,"z")])]),inj(pi(nt),pt(lit(3)),[lit(4),lit(5)]),elim(mu([var(0),var(1)]),var(0),[(H.ctoraddr("a"),leg(var(0),[(ONE,"x")])),(H.ctoraddr("b"),leg(var(1),[(ZERO,"x"),(ONE,"y")]))],H.some(H.motive(var(2),["i","j"]))),elim(mu(),var(0),[(H.ctoraddr("a"),leg(var(0))),(H.ctoraddr("b"),leg(var(1)))]),ran(ctor("S.SNu",text("F"),seq([var(0),var(1)])),univ())]
    for i, tm in enumerate(printed): add(f"print-syntax-{i}",call("PP.term",seq([text("outer")]),tm))
    for i, s in enumerate(["", "a\n\t\r\b\"\\", "éλ😀", "\x00\x7f"]):
        bend = "SNil{}"
        for ch in reversed(s): bend = f"SCon{{Chr{{{ord(ch)}}}, {bend}}}"
        value = Expr(bend)
        add(f"print-string-{i}",call("PP.term",seq([]),ctor("T.Lit",ctor("Lit.LString",value))))
    add("spec-count",Expr("PP.escape(SC.print)"))
    pos = [("absent",nt,"ok"),("self",global_("F"),"ok"),("mu-self",lan(mu(),univ()),"ok"),("positive-arrow",ran(pi(nt),global_("F")),"ok"),("negative-arrow",ran(pi(global_("F")),nt),None),("annotated",ann(global_("F"),univ()),None),("applied",H.app(global_("F"),nt),None),("foreign-mu-argument",lan(mu([global_("F")],"G"),univ()),None)]
    for name, tm, want in pos:
        add("positive-"+name,call("C.ck_unit",call("Pos.positive",seq([text("F")]),tm)),want)
        add("self-rec-"+name,call("C.ck_bool",call("Pos.self_rec",seq([text("F")]),tele([(ONE,"x",tm)]))))
    for i in range(3):
        infer("universe-"+str(i),univ(i),"ok:Type " + str(i+1))
        check("universe-"+str(i),univ(i),univ(i+1),"ok")
    for name,tm in [("unbound",global_("missing")),("var-negative",var(-1)),("var-missing",var(0)),("auto",ctor("T.Auto")),("negative-nat",lit(-1)),("string",ctor("T.Lit",ctor("Lit.LString",text("s"))))]: infer(name,tm)
    infer("nat",lit(42),"ok:Nat")
    check("nat",lit(0),nt,"ok")
    check("not-cumulative",univ(0),univ(2))
    check("literal-is-not-type",lit(1),univ())
    add("univ-natural-refused",call("C.ck_univ",ctx,lit(1)))
    add("univ-nat",call("C.ck_univ",ctx,nt),"ok:1")
    for q, name in quantities:
        s = pi(nt,q)
        fn = ran(s,nt)
        sig = lan(s,nt)
        identity = section(s,[leg(var(0),[(q,"x")])])
        infer("pi-form-"+name,fn,"ok:Type 1")
        infer("sigma-form-"+name,sig,"ok:Type 1")
        check("identity-"+name,identity,fn,"ok" if name != "0" else None)
        check("constant-"+name,section(s,[leg(lit(9),[(q,"x")])]),fn,"ok" if name != "1" else None)
        check("sigma-"+name,inj(s,pt(lit(2),q),[lit(3)]),sig,"ok")
        check("sigma-arity-"+name,inj(s,pt(lit(2),q),[]),sig)
        check("lambda-binders-"+name,section(s,[leg(lit(0))]),fn)
        check("lambda-legs-"+name,section(s,[]),fn)
        infer("application-"+name,out(s,pt(lit(4),q),ann(section(s,[leg(lit(7),[(q,"x")])]),fn)),"ok:Nat" if name != "1" else None)
    for width in (0,1,2):
        s=coll(width); diagram=section(s,[leg(nt) for _ in range(width)])
        prod=ran(s,diagram); summ=lan(s,diagram)
        infer(f"product-{width}",prod,"ok:Type 1" if width else None)
        infer(f"sum-{width}",summ,"ok:Type 1" if width else None)
        check(f"tuple-{width}",section(s,[leg(lit(i)) for i in range(width)]),prod,"ok")
        check(f"tuple-short-{width}",section(s,[] if width else [leg(lit(0))]),prod)
        for i in range(width):
            check(f"sum-in-{width}-{i}",inj(s,H.legaddr(i),[lit(i)]),summ,"ok")
            infer(f"projection-{width}-{i}",out(s,H.legaddr(i),ann(section(s,[leg(lit(j)) for j in range(width)]),prod)),"ok:Nat")
        check(f"sum-in-bounds-{width}",inj(s,H.legaddr(width),[lit(0)]),summ)
    infer("collection-negative",ran(coll(-1),section(coll(-1),[])))
    infer("collection-diagram-count",ran(coll(2),section(coll(1),[leg(nt)])))
    s=coll(2); diag=section(s,[leg(nt),leg(nt)]); sumty=lan(s,diag)
    scrut=ann(inj(s,H.legaddr(0),[lit(7)]),sumty)
    branches=[(H.legaddr(i),leg(var(0),[(ONE,"x")])) for i in range(2)]
    for name, bs in [("all",branches),("missing",branches[:1]),("duplicate",[branches[0],branches[0]]),("wrong-address",[(H.ctoraddr(),branches[0][1])])]:
        check("sum-elim-"+name,elim(s,scrut,bs),nt,"ok" if name=="all" else None)
    infer("sum-elim-no-motive",elim(s,scrut,branches))
    infer("sum-elim-motive",elim(s,scrut,branches,H.some(H.motive(nt))),"ok:Nat")
    for q,name in quantities:
        local_ctx=call("K.bind",text("x"),q,H.neutral("Nat"),ctx)
        infer("local-"+name,var(0),"ok:Nat" if name!="0" else None,c=local_ctx)
        infer("local-erased-"+name,var(0),"ok:Nat",c=local_ctx,q=ZERO)
    let=ctor("T.Let",text("x"),nt,lit(5),var(0))
    infer("let",let,"ok:Nat")
    infer("let-wrong-definition",ctor("T.Let",text("x"),nt,univ(),var(0)))
    infer("annotation",ann(lit(1),nt),"ok:Nat")
    infer("annotation-wrong",ann(lit(1),univ()))
    for budget in range(9):
        infer(f"budget-univ-{budget}",univ(),"ok:Type 2" if budget else None,c=call("C.ck_budget",GLOBALS,nat(budget)))
        infer(f"budget-let-{budget}",let,c=call("C.ck_budget",GLOBALS,nat(budget)))
    defs=[declaration("a",nt,H.some(lit(1))),declaration("b",nt,H.some(global_("a")))]
    for name,ds,want in [("ordered",defs,"ok:a;b;"),("forward",list(reversed(defs)),None),("postulate",[declaration("a",nt,postulate=True)],"ok:a;"),("missing-body",[declaration("a",nt)],None),("self",[declaration("a",nt,H.some(global_("a")))],None),("bad-type",[declaration("a",lit(0),postulate=True)],None)]:
        add("decl-"+name,call("C.ck_decls",GLOBALS,seq(ds)),want)
    for name,fd,want in [("plain",family(),"ok"),("parameter",family(params=[(MANY,"A",univ())]),"ok"),("index",family(indices=[(ZERO,"i",nt)]),"ok"),("index-runtime",family(indices=[(ONE,"i",nt)]),None),("index-level",family(indices=[(ZERO,"i",univ(2))]),None),("bad-parameter",family(params=[(ONE,"x",lit(0))]),None),("primitive-type-name",family(name="Nat"),None)]:
        add("family-"+name,call("C.ck_family",GLOBALS,fd),want)
    def ctors_case(name,fd,cs,want=None): add("ctors-"+name,call("C.ck_ctors",GLOBALS,fd,seq([text("F")]),text("F"),seq(cs)),want)
    ctors_case("empty",family(),[],"ok")
    ctors_case("nullary",family(),[constructor()],"ok")
    ctors_case("natural-field",family(),[constructor(args=[(ONE,"x",nt)])],"ok")
    ft=lan(mu(),section(coll(0),[]))
    ctors_case("recursive",family(),[constructor(args=[(ONE,"tail",ft)])],"ok")
    ctors_case("negative",family(),[constructor(args=[(ONE,"f",ran(pi(ft),nt))])])
    ctors_case("field-universe",family(),[constructor(args=[(ONE,"x",univ(2))])])
    ctors_case("missing-parameter",family(params=[(MANY,"A",univ())]),[constructor()])
    ctors_case("preserved-parameter",family(params=[(MANY,"A",univ())]),[constructor(params=[var(0)])],"ok")
    ctors_case("changed-parameter",family(params=[(MANY,"A",univ())]),[constructor(params=[nt])])
    ctors_case("index",family(indices=[(ZERO,"i",nt)]),[constructor(indices=[lit(0)])],"ok")
    ctors_case("missing-index",family(indices=[(ZERO,"i",nt)]),[constructor()])
    ctors_case("bad-index",family(indices=[(ZERO,"i",nt)]),[constructor(indices=[univ()])])
    fd=family(); cs=seq([constructor()]); group=seq([text("F")]); name=text("F")
    for label,tm in [("formation",ft),("unknown-constructor",inj(mu(),H.ctoraddr("bad"),[])),("introduction",ann(inj(mu(),H.ctoraddr(),[]),ft))]:
        add("mu-"+label,call("C.ck_family_infer",GLOBALS,fd,group,name,cs,tm),"ok:Type 1" if label=="formation" else None)
    scrut=ann(inj(mu(),H.ctoraddr(),[]),ft)
    for label,bs in [("complete",[(H.ctoraddr(),leg(lit(1)))]),("missing",[]),("duplicate",[(H.ctoraddr(),leg(lit(1))),(H.ctoraddr(),leg(lit(2)))]),("binder-count",[(H.ctoraddr(),leg(var(0),[(ONE,"x")]))])]:
        add("mu-elim-"+label,call("C.ck_family_check",GLOBALS,fd,group,name,cs,elim(mu(),scrut,bs,H.some(H.motive(nt,ind=H.some(text("F"))))),nt),"ok" if label=="complete" else None)
    add("family-redeclare",call("C.ck_family_twice",GLOBALS,fd),"error:mismatch: the family F is already declared")
    add("family-reinstall",call("C.ck_install_twice",GLOBALS,fd,group,name,cs),"error:mismatch: the constructors of F are already installed")
    add("family-provisional-formation",call("C.ck_provisional",GLOBALS,fd,ft),"ok:Type 1")
    add("family-provisional-elim",call("C.ck_provisional",GLOBALS,fd,elim(mu(),scrut,[],H.some(H.motive(nt,ind=H.some(text("F")))))))
    for label, tm, ty in [("wrong-ctor",inj(mu(),H.ctoraddr("bad"),[]),ft),("arity",inj(mu(),H.ctoraddr(),[lit(0)]),ft),("wrong-address",inj(mu(),H.legaddr(0),[]),ft),("wrong-shape",inj(coll(0),H.ctoraddr(),[]),ft)]:
        add("mu-check-"+label,call("C.ck_family_check",GLOBALS,fd,group,name,cs,tm,ty))
    for label,mo in [("absent",NONE),("unnamed",H.some(H.motive(nt))),("wrong-family",H.some(H.motive(nt,ind=H.some(text("G"))))),("wrong-indices",H.some(H.motive(nt,idx=["i"],ind=H.some(text("F")))))]:
        add("mu-motive-"+label,call("C.ck_family_infer",GLOBALS,fd,group,name,cs,elim(mu(),scrut,[(H.ctoraddr(),leg(lit(0)))],mo)))
    for prop_cs,label in [(seq([]),"empty"),(seq([constructor()]),"singleton"),(seq([constructor(),constructor("d")]),"two"),(seq([constructor(args=[(ONE,"n",nt)])]),"runtime-field")]:
        propfd=family(lev=0); propft=lan(mu(),section(coll(0),[]))
        # Empty families are checked using an assumed scrutinee in a local context below.
        if label!="empty":
            fields=[lit(0)] if label=="runtime-field" else []
            pscrut=ann(inj(mu(),H.ctoraddr(),fields),propft)
            bs=[(H.ctoraddr(),leg(lit(0),[(ONE,"n")] if fields else []))]
            if label=="two": bs.append((H.ctoraddr("d"),leg(lit(0))))
            add("mu-large-elim-"+label,call("C.ck_family_infer",GLOBALS,propfd,group,name,prop_cs,elim(mu(),pscrut,bs,H.some(H.motive(nt,ind=H.some(text("F")))))))
    indexed=family(indices=[(ZERO,"i",nt)])
    ics=seq([constructor(indices=[lit(0)])])
    for i in (0,1):
        ity=lan(mu([lit(i)]),section(coll(0),[]))
        add(f"mu-index-in-{i}",call("C.ck_family_check",GLOBALS,indexed,group,name,ics,inj(mu([lit(i)]),H.ctoraddr(),[]),ity),"ok" if i==0 else None)
    for label,ix in [("absent",[]),("extra",[lit(0),lit(1)]),("illtyped",[univ()])]:
        add("mu-index-"+label,call("C.ck_family_infer",GLOBALS,indexed,group,name,ics,lan(mu(ix),section(coll(0),[]))))
    paramfd=family(params=[(MANY,"A",univ())]); paramcs=seq([constructor(args=[(ONE,"x",var(0))],params=[var(1)])])
    pty=lan(mu(),section(coll(1),[leg(nt)]))
    for label,args in [("ok",[lit(3)]),("bad-field",[univ()]),("missing-field",[])]:
        add("mu-parameter-"+label,call("C.ck_family_check",GLOBALS,paramfd,group,name,paramcs,inj(mu(),H.ctoraddr(),args),pty),"ok" if label=="ok" else None)
    # Pi elimination binds the point first and the fibre element second.
    s=pi(nt,ZERO); sig=lan(s,nt); pscrut=ann(inj(s,pt(lit(2),ZERO),[lit(3)]),sig)
    for label,bs in [("ok",[(H.legaddr(0),leg(var(0),[(ZERO,"x"),(ONE,"y")]))]),("wrong-leg",[(H.legaddr(1),leg(var(0),[(ZERO,"x"),(ONE,"y")]))]),("binder-count",[(H.legaddr(0),leg(var(0),[(ONE,"y")]))]),("quantity",[(H.legaddr(0),leg(var(0),[(ONE,"x"),(ONE,"y")]))])]:
        check("pi-elim-"+label,elim(s,pscrut,bs),nt,"ok" if label=="ok" else None)
    infer("pi-elim-motive",elim(s,pscrut,[(H.legaddr(0),leg(var(0),[(ZERO,"x"),(ONE,"y")]))],H.some(H.motive(nt))),"ok:Nat")
    # A linear argument may occur once on each alternative, but not twice in a product.
    prod=ran(coll(2),section(coll(2),[leg(nt),leg(nt)]))
    linear=pi(nt,ONE)
    for label,body in [("duplicate",section(coll(2),[leg(var(0)),leg(var(0))])),("single",section(coll(2),[leg(var(0)),leg(lit(0))]))]:
        check("linear-product-"+label,section(linear,[leg(body,[(ONE,"x")])]),ran(linear,prod),"ok" if label=="single" else None)
    sum_s=coll(2); sum_ty=lan(sum_s,section(sum_s,[leg(nt),leg(nt)]))
    sum_scrut=ann(inj(sum_s,H.legaddr(0),[lit(0)]),sum_ty)
    for label,second in [("all",var(1)),("missing",lit(0))]:
        bs=[(H.legaddr(0),leg(var(1),[(MANY,"y")])),(H.legaddr(1),leg(second,[(MANY,"y")]))]
        check("linear-alternatives-"+label,section(linear,[leg(elim(sum_s,sum_scrut,bs),[(ONE,"x")])]),ran(linear,nt),"ok" if label=="all" else None)
    for label,body in [("use",var(0)),("drop",lit(0)),("duplicate",out(pi(nt,MANY),pt(var(0),MANY),out(pi(nt,MANY),pt(var(0),MANY),global_("natAdd"))))]:
        alias=ctor("T.Let",text("alias"),nt,var(0),body)
        check("linear-alias-"+label,section(linear,[leg(alias,[(ONE,"x")])]),ran(linear,nt),"ok" if label=="use" else None)
    inner=pi(nt,MANY)
    check("linear-closure",section(linear,[leg(section(inner,[leg(var(1),[(MANY,"y")])]),[(ONE,"x")])]),ran(linear,ran(inner,nt)))
    for label,dom,left,right in [("proof",global_("P"),global_("p"),global_("q")),("natural",nt,lit(0),lit(1))]:
        def apply(f,x): return out(pi(dom,MANY),pt(x,MANY),f)
        ds=[declaration("P",univ(0),postulate=True),declaration("p",global_("P"),postulate=True),declaration("q",global_("P"),postulate=True),declaration("F",ran(pi(dom,MANY),univ()),postulate=True),declaration("x",apply(global_("F"),left),postulate=True),declaration("y",apply(global_("F"),right),H.some(global_("x")))]
        add("conversion-"+label,call("C.ck_decls",GLOBALS,seq(ds)),"ok:P;p;q;F;x;y;" if label=="proof" else None)
    # Parameterized diagnostic paths and the uninhabited collection must remain finite.
    empty_ty=lan(coll(0),section(coll(0),[]))
    empty_ctx=call("K.bind",text("absurd"),ONE,H.former("Lan",coll(0),section(coll(0),[])),ctx)
    check("empty-elimination",elim(coll(0),var(0),[]),nt,"ok",c=empty_ctx)
    return rows
