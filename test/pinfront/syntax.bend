# Syntax and canonical printer from Kanon 69f3be51 surface/syntax.ml.
import Base
import ../../lib/foundation.bend as F
import ../../lib/kernel_quantity.bend as Q

type prim is Data:
  PAdd{}
  PSub{}
  PMul{}
  PEq{}
  PLt{}
type binder_of<-A: Data> is Data:
  Binder{q: Q.t, name: String, ty: A}
type motive_of<-A: Data> is Data:
  Motive{self: String, ind: Maybe<&2, String>, idx: List<&2, String>, body: A}
type field_of<-A: Data> is Data:
  Field{q: Q.t, name: String, ty: Maybe<&2, A>}
type branch_of<-A: Data> is Data:
  BrLeg{key: F.Int63.t, binders: List<&2, binder_of<A>>, body: A}
  BrCtor{name: String, fields: List<&2, field_of<A>>, body: A}
type fam_ctor_of<-A: Data> is Data:
  FamCtor{name: String, ty: A}
type fam_of<-A: Data> is Data:
  Fam{name: String, params: List<&2, binder_of<A>>, ty: A, ctors: List<&2, fam_ctor_of<A>>}
type rec_def_of<-A: Data> is Data:
  RecDef{name: String, ty: A, body: A}
type t is Data:
  SVar{name: String}
  SNat{value: F.Bignum.t}
  SProp{}
  SType{level: F.Int63.t}
  SPrim{prim: prim}
  SUnit{}
  SAuto{}
  SPair{fst: t, snd: t}
  STuple{items: List<&2, t>}
  SSum{items: List<&2, t>}
  SProd{items: List<&2, t>}
  SProj{term: t, key: F.Int63.t}
  SInj{key: F.Int63.t, size: F.Int63.t, term: t}
  SAbsurd{term: t}
  SApp{function: t, argument: t}
  SFun{binders: List<&2, binder_of<t>>, body: t}
  SArrow{binder: binder_of<t>, body: t}
  SStar{binder: binder_of<t>, body: t}
  SLet{name: String, ty: t, value: t, body: t}
  SAnn{term: t, ty: t}
  SCase{scrutinee: t, motive: Maybe<&2, motive_of<t>>, branches: List<&2, branch_of<t>>}
  SMatch{scrutinee: t, motive: Maybe<&2, motive_of<t>>, branches: List<&2, branch_of<t>>}
type decl is Data:
  DDef{name: String, ty: t, body: t}
  DAxiom{name: String, ty: t}
  DMu{families: List<&2, fam_of<t>>}
  DRec{members: List<&2, rec_def_of<t>>}

def binder() -> Data: binder_of<t>
def motive() -> Data: motive_of<t>
def field() -> Data: field_of<t>
def branch() -> Data: branch_of<t>
def fam_ctor() -> Data: fam_ctor_of<t>
def fam() -> Data: fam_of<t>
def rec_def() -> Data: rec_def_of<t>

def cat(xs: List<&2, String>) -> String:
  match xs:
    case []: ""
    case x <> rest: String.append(x, cat(rest))
def join_rev(xs: List<&2, String>, acc: String) -> String:
  match xs:
    case []: acc
    case x <> rest: join_rev(rest, String.append(x, acc))
def texts_rev(~A: Data, ~show: A -> String, +sep: String, xs: List<&2, A>, acc: List<&2, String>) -> List<&2, String>:
  match xs:
    case []: acc
    case x <> rest: texts_rev(~A, ~show, sep, rest, show(x) <> sep <> acc)
def texts(~A: Data, ~show: A -> String, +sep: String, xs: List<&2, A>) -> String:
  match xs:
    case []: ""
    case x <> rest: join_rev(texts_rev(~A, ~show, sep, rest, [show(x)]), "")

def prim_name(p: prim) -> String:
  match p:
    case PAdd{}: "natAdd"
    case PSub{}: "natSub"
    case PMul{}: "natMul"
    case PEq{}: "natEq"
    case PLt{}: "natLt"
def level_of(s: t) -> Nat:
  match s:
    case SVar{_}: 2n
    case SNat{_}: 2n
    case SProp{}: 2n
    case SType{_}: 2n
    case SPrim{_}: 2n
    case SUnit{}: 2n
    case SAuto{}: 2n
    case SPair{_, _}: 2n
    case STuple{_}: 2n
    case SSum{_}: 2n
    case SProd{_}: 2n
    case SProj{_, _}: 2n
    case SAnn{_, _}: 2n
    case SInj{_, _, _}: 1n
    case SAbsurd{_}: 1n
    case SApp{_, _}: 1n
    case SFun{_, _}: 0n
    case SArrow{_, _}: 0n
    case SStar{_, _}: 0n
    case SLet{_, _, _, _}: 0n
    case SCase{_, _, _}: 0n
    case SMatch{_, _, _}: 0n
def mark(q: Q.t) -> String:
  match q:
    case Q.Zero{}: "0 "
    case Q.One{}: "1 "
    case Q.Many{}: ""

def at_text(tight: Bool, +text: String) -> String:
  F.choose(String, tight, text, cat(["(", text, ")"]))






def ind_text(ind: Maybe<&2, String>, idx: List<&2, String>) -> String:
  match ind:
    case None{}: ""
    case Some{name}: cat([" in ", name, texts(~String, ~(x => cat([" ", x])), "", idx)])



type render_job is Data:
  RenderJoin{sep: String, jobs: List<&2, render_job>}
  RenderPrefix{prefix: String, job: render_job}
  RenderAt{lvl: Nat, s: t}
  RenderBinderText{b: binder}
  RenderFieldText{f: field}
  RenderBranchText{br: branch}
  RenderMotiveText{mo: motive}
  RenderOptionalMotive{mo: Maybe<&2, motive>}
  RenderRaw{s: t}

def jobs(~A: Data, ~pack: A -> render_job, xs: List<&2, A>) -> List<&2, render_job>:
  match xs:
    case []: []
    case x <> rest: pack(x) <> jobs(~A, ~pack, rest)
@unsafe
def render(job: render_job) -> String:
  match job:
    case RenderJoin{+sep, []}: ""
    case RenderJoin{+sep, x <> []}: render(x)
    case RenderJoin{+sep, x <> y <> rest}: cat([render(x), sep, render(RenderJoin{sep, y <> rest})])
    case RenderPrefix{prefix, x}: cat([prefix, render(x)])
    case RenderAt{lvl, +s}:
      at_text((level_of(s) >= lvl : Nat), render(RenderRaw{s}))
    case RenderBinderText{b}:
      match b:
        case Binder{q, name, ty}: cat(["(", mark(q), name, " : ", render(RenderAt{0n, ty}), ")"])
    case RenderFieldText{f}:
      match f:
        case Field{q, name, None{}}: cat([" ", mark(q), name])
        case Field{q, name, Some{ty}}: cat([" ", render(RenderBinderText{Binder{q, name, ty}})])
    case RenderBranchText{br}:
      match br:
        case BrLeg{k, bs, body}:
          cat([" | ", F.Int63.show(k), render(RenderJoin{"", jobs(~binder, ~(b => RenderPrefix{" ", RenderBinderText{b}}), bs)}), " => ", render(RenderAt{1n, body})])
        case BrCtor{c, fs, body}:
          cat([" | ", c, render(RenderJoin{"", jobs(~field, ~(f => RenderFieldText{f}), fs)}), " => ", render(RenderAt{1n, body})])
    case RenderMotiveText{mo}:
      match mo:
        case Motive{self, ind, idx, body}: cat([" as ", self, ind_text(ind, idx), " return ", render(RenderAt{1n, body})])
    case RenderOptionalMotive{mo}:
      match mo:
        case None{}: ""
        case Some{m}: render(RenderMotiveText{m})
    case RenderRaw{s}:
      match s:
        case SVar{x}: x
        case SNat{n}: F.Bignum.to_string(n)
        case SProp{}: "Prop"
        case SType{n}: cat(["Type ", F.Int63.show(n)])
        case SPrim{p}: prim_name(p)
        case SUnit{}: "()"
        case SAuto{}: "auto"
        case SPair{a, b}: cat(["(", render(RenderAt{0n, a}), ", ", render(RenderAt{0n, b}), ")"])
        case STuple{xs}: cat(["tuple (", render(RenderJoin{", ", jobs(~t, ~(x => RenderAt{0n, x}), xs)}), ")"])
        case SSum{xs}: cat(["sum (", render(RenderJoin{", ", jobs(~t, ~(x => RenderAt{0n, x}), xs)}), ")"])
        case SProd{xs}: cat(["prod (", render(RenderJoin{", ", jobs(~t, ~(x => RenderAt{0n, x}), xs)}), ")"])
        case SProj{a, k}: cat([render(RenderAt{2n, a}), ".", F.Int63.show(k)])
        case SInj{k, n, a}: cat(["inj ", F.Int63.show(k), " of ", F.Int63.show(n), " ", render(RenderAt{1n, a})])
        case SAbsurd{a}: cat(["absurd ", render(RenderAt{1n, a})])
        case SApp{f, a}: cat([render(RenderAt{1n, f}), " ", render(RenderAt{2n, a})])
        case SFun{bs, body}: cat(["fun ", render(RenderJoin{" ", jobs(~binder, ~(b => RenderBinderText{b}), bs)}), " => ", render(RenderAt{0n, body})])
        case SArrow{b, cod}: cat([render(RenderBinderText{b}), " -> ", render(RenderAt{0n, cod})])
        case SStar{b, cod}: cat([render(RenderBinderText{b}), " * ", render(RenderAt{0n, cod})])
        case SLet{x, ty, value, body}: cat(["let ", x, " : ", render(RenderAt{1n, ty}), " := ", render(RenderAt{1n, value}), " in ", render(RenderAt{0n, body})])
        case SAnn{a, ty}: cat(["(", render(RenderAt{0n, a}), " : ", render(RenderAt{0n, ty}), ")"])
        case SCase{scrut, mo, brs}: cat(["case ", render(RenderAt{1n, scrut}), render(RenderOptionalMotive{mo}), " with", render(RenderJoin{"", jobs(~branch, ~(br => RenderBranchText{br}), brs)})])
        case SMatch{scrut, mo, brs}: cat(["match ", render(RenderAt{1n, scrut}), render(RenderOptionalMotive{mo}), " with", render(RenderJoin{"", jobs(~branch, ~(br => RenderBranchText{br}), brs)})])

def at(lvl: Nat, +s: t) -> String:
  render(RenderAt{lvl, s})

def binder_text(b: binder) -> String:
  render(RenderBinderText{b})

def field_text(f: field) -> String:
  render(RenderFieldText{f})

def branch_text(br: branch) -> String:
  render(RenderBranchText{br})

def motive_text(mo: motive) -> String:
  render(RenderMotiveText{mo})

def optional_motive(mo: Maybe<&2, motive>) -> String:
  render(RenderOptionalMotive{mo})

def raw(s: t) -> String:
  render(RenderRaw{s})

def fam_ctor_text(fc: fam_ctor) -> String:
  match fc:
    case FamCtor{name, ty}: cat(["| ", name, " : ", at(0n, ty), "\n"])
def fam_text(word: String, fm: fam) -> String:
  match fm:
    case Fam{name, params, ty, ctors}:
      cat([word, " ", name, texts(~binder, ~(b => cat([" ", binder_text(b)])), "", params), " : ", at(0n, ty), " :=\n", texts(~fam_ctor, ~(c => fam_ctor_text(c)), "", ctors)])
def rec_text(ms: List<&2, rec_def>, word: String) -> String:
  match ms:
    case []: ""
    case RecDef{name, ty, body} <> rest: cat([word, " ", name, " : ", at(0n, ty), " := ", at(0n, body), "\n", rec_text(rest, "and")])
def decl_text(d: decl) -> String:
  match d:
    case DDef{name, ty, body}: cat(["def ", name, " : ", at(0n, ty), " := ", at(0n, body), "\n"])
    case DAxiom{name, ty}: cat(["axiom ", name, " : ", at(0n, ty), "\n"])
    case DMu{[]}: ""
    case DMu{fm <> []}: fam_text("mu", fm)
    case DMu{a <> b <> rest}: cat(["mutual\n", texts(~fam, ~(f => fam_text("mu", f)), "", a <> b <> rest), "end\n"])
    case DRec{ms}: rec_text(ms, "def rec")
def print(ds: List<&2, decl>) -> String: texts(~decl, ~(d => decl_text(d)), "", ds)
