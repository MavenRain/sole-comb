"""Pinned erasure support cases with independent printer and binding goldens."""
from dataclasses import dataclass
import json


@dataclass(frozen=True)
class Expr:
    bend: str
    ocaml: str


def text(value):
    # OCaml strings hold UTF-8 bytes; Bend strings hold Unicode scalars.
    encoded = "".join(f"\\{byte:03d}" for byte in value.encode())
    bend = json.dumps(value)
    if any(ord(c) < 32 or ord(c) >= 127 for c in value):
        bend = "SNil{}"
        for c in reversed(value):
            bend = f"SCon{{Chr{{{ord(c)}}}, {bend}}}"
    return Expr(bend, '"' + encoded + '"')


def integer(value):
    magnitude = abs(value)
    limbs = []
    while magnitude:
        limbs.append(magnitude % 32768)
        magnitude //= 32768
    return Expr(f'F.Int63.Int{{{"True" if value < 0 else "False"}{{}}, {limbs}}}', f"({value})")


def items(values):
    return Expr("[" + ", ".join(v.bend for v in values) + "]", "[" + "; ".join(v.ocaml for v in values) + "]")


def ctor(name, *args):
    return Expr(f'K.{name}{{{", ".join(a.bend for a in args)}}}',
                f'(K.{name}' + (" (" + ", ".join(a.ocaml for a in args) + ")" if args else "") + ")")


def call(bend, ocaml, *args):
    return Expr(f'{bend}({", ".join(a.bend for a in args)})', f'({ocaml} ' + " ".join(a.ocaml for a in args) + ")")


def var(n):
    return ctor("KVar", integer(n))


def tid(name):
    return ctor("Tid", text(name))


def fid(name):
    return ctor("Fid", text(name))


def branch(tag, arity, body):
    return Expr(f'K.Branch{{{integer(tag).bend}, {integer(arity).bend}, {body.bend}}}',
                f'{{ K.tag = {tag}; arity = {arity}; body = {body.ocaml} }}')


def lit_int(n):
    value = call("C.number", "number", text(str(abs(n))))
    if n < 0:
        value = Expr(f"F.Bignum.negate({value.bend})", f"(Bignum.mul (Bignum.of_int (-1)) {value.ocaml})")
    return Expr(f'K.KLit{{Lit.LInt{{{value.bend}}}}}', f'(K.KLit (Literal.LInt {value.ocaml}))')


def lit_string(s):
    value = text(s)
    return Expr(f'K.KLit{{Lit.LString{{{value.bend}}}}}', f'(K.KLit (Literal.LString {value.ocaml}))')


def printed(term):
    return call("K.print_ktm", "K.print_ktm", term)


def mapped(term):
    return call("C.mapping", "K.print_ktm", term)


def cases():
    rows = []

    def add(name, expression, expected=None):
        rows.append((name, expression, expected))

    pair = ctor("KStruct", tid("pair"), items([var(0), var(2), var(2)]))
    nested = ctor("KLet", text("x"), var(2), ctor("KLet", text("y"), var(0),
                   ctor("KApp", var(3), items([var(0), var(1), var(4)]))))
    case = ctor("KCase", tid("sum"), var(3), items([
        branch(0, 0, var(2)), branch(1, 2, pair),
        branch(2, 1, ctor("KLet", text("z"), var(1), var(3)))]))
    samples = [
        ("negative", var(-1), "KVar -1", []),
        ("zero", var(0), "KVar 0", [0]),
        ("outer", var(3), "KVar 3", [3]),
        ("literal", lit_int(123456789012345678901234567890), "KLit 123456789012345678901234567890", []),
        ("string", lit_string('"\\\n\t'), 'KLit "\\\"\\\\\\n\\t"', []),
        ("global", ctor("KGlobal", text("bare name")), "KGlobal bare name", []),
        ("erased", ctor("KErased"), "KErased", []),
        ("let", ctor("KLet", text("x"), var(2), pair),
         "KLet x (KVar 2) (KStruct pair [KVar 0; KVar 2; KVar 2])", [2, 1, 1]),
        ("nested-let", nested, "KLet x (KVar 2) (KLet y (KVar 0) (KApp (KVar 3) [KVar 0; KVar 1; KVar 4]))", [2, 1, 2]),
        ("closure", ctor("KClos", fid("f#0"), integer(2), items([var(1), var(0)])), "KClos f#0 2 [KVar 1; KVar 0]", [1, 0]),
        ("app", ctor("KApp", var(2), items([var(0), var(2)])), "KApp (KVar 2) [KVar 0; KVar 2]", [2, 0, 2]),
        ("tail", ctor("KTail", var(1), items([var(2)])), "KTail (KVar 1) [KVar 2]", [1, 2]),
        ("struct", pair, "KStruct pair [KVar 0; KVar 2; KVar 2]", [0, 2, 2]),
        ("projection", ctor("KProj", tid("pair"), integer(1), var(2)), "KProj pair 1 (KVar 2)", [2]),
        ("tag", ctor("KTag", tid("sum"), integer(2), items([var(1), var(3)])), "KTag sum 2 [KVar 1; KVar 3]", [1, 3]),
        ("case", case, "KCase sum (KVar 3) [{0 0 (KVar 2)}; {1 2 (KStruct pair [KVar 0; KVar 2; KVar 2])}; {2 1 (KLet z (KVar 1) (KVar 3))}]", [3, 2, 0, 0, 0, 1]),
        ("delay", ctor("KDelay", fid("delayed"), items([var(2), var(0)])), "KDelay delayed [KVar 2; KVar 0]", [2, 0]),
        ("force", ctor("KForce", var(4)), "KForce (KVar 4)", [4]),
        ("empty-closure", ctor("KClos", fid("f"), integer(0), items([])), "KClos f 0 []", []),
        ("empty-app", ctor("KApp", var(1), items([])), "KApp (KVar 1) []", [1]),
        ("empty-tail", ctor("KTail", var(1), items([])), "KTail (KVar 1) []", [1]),
        ("empty-struct", ctor("KStruct", tid("unit"), items([])), "KStruct unit []", []),
        ("empty-tag", ctor("KTag", tid("sum"), integer(0), items([])), "KTag sum 0 []", []),
        ("empty-case", ctor("KCase", tid("empty"), var(2), items([])), "KCase empty (KVar 2) []", [2]),
        ("empty-delay", ctor("KDelay", fid("f"), items([])), "KDelay f []", []),
    ]
    remap = Expr("+i => F.Int63.add(F.Int63.add(i, i), F.Int63.one)", "(fun i -> 2 * i + 1)")
    for name, term, golden, variables in samples:
        add("print/" + name, printed(term), golden)
        add("vars/" + name, call("C.vars", "vars", integer(0), term), "[" + "; ".join(map(str, variables)) + "]")
        add("vars-under/" + name, call("C.vars", "vars", integer(2), term))
        add("shift/" + name, mapped(call("R.shift_runtime", "R.shift_runtime", integer(3), integer(0), term)))
        add("shift-under/" + name, mapped(call("R.shift_runtime", "R.shift_runtime", integer(-2), integer(1), term)))
        add("reindex/" + name, mapped(call("C.reindex", "R.reindex_runtime", remap, integer(1), term)))
        identity = Expr("i => i", "(fun i -> i)")
        add("identity/" + name, mapped(call("C.reindex", "R.reindex_runtime", identity, integer(0), term)), golden)

    for name, body, golden, variables in (
        ("force", var(0), "KForce (" * 128 + "KVar 0" + ")" * 128, "[0]"),
        ("let", var(128), "KLet x (KVar 0) (" * 128 + "KVar 128" + ")" * 128, "[0; 0]"),
    ):
        term = call(f"C.{name}_chain", f"{name}_chain", Expr("128n", "128"), body)
        add("deep/print-" + name, printed(term), golden)
        add("deep/vars-" + name, call("C.vars", "vars", integer(0), term), variables)
        add("deep/shift-" + name, mapped(call("R.shift_runtime", "R.shift_runtime", integer(3), integer(0), term)))

    representations = [(ctor("RI31"), "i31")]
    representations += [(ctor(name, tid("tag")), word + " tag") for name, word in (
        ("RStruct", "struct"), ("RUnion", "union"), ("RFunc", "func"), ("RThunk", "thunk"))]
    for i, (representation, golden) in enumerate(representations):
        add(f"repr/{i}", call("K.print_repr", "K.print_repr", representation), golden)
    for i, name in enumerate(("", "raw name;[]", "λ😀")):
        add(f"tid/{i}", call("K.tid_text", "K.tid_text", tid(name)), name)
        add(f"fid/{i}", call("K.fid_text", "K.fid_text", fid(name)), name)
    for n in (0, -1, -(2**100), 2**160, 2**62 - 1):
        add(f"integer/{n}", printed(lit_int(n)), "KLit " + str(n))
    strings = [
        ("", 'KLit ""'), ("a\' z", 'KLit "a\' z"'),
        ("\x00\x01\x08\x09\x0a\x0d\x1f\x7f", 'KLit "\\000\\001\\b\\t\\n\\r\\031\\127"'),
        ("éλ€😀", 'KLit "\\195\\169\\206\\187\\226\\130\\172\\240\\159\\152\\128"'),
    ]
    for i, (value, golden) in enumerate(strings):
        add(f"string/{i}", printed(lit_string(value)), golden)
    for n in (-(2**62), 2**62 - 1):
        add(f"host-index/{n}", printed(var(n)), f"KVar {n}")
    add("shift/wrap", mapped(call("R.shift_runtime", "R.shift_runtime", integer(1), integer(0), var(2**62 - 1))), f"KVar {-2**62}")
    add("branch/negative", call("K.print_branch", "K.print_branch", branch(-1, -2, var(-3))), "{-1 -2 (KVar -3)}")
    add("decl/rec-empty", call("K.print_decl", "K.print_decl", ctor("KRec", items([]))), "rec []")
    add("decl/rec", call("K.print_decl", "K.print_decl", ctor("KRec", items([tid("a"), tid("b")]))), "rec [a; b]")
    add("decl/fun", call("K.print_decl", "K.print_decl", ctor("KFun", fid("f"), items([r for r, _ in representations]), ctor("RI31"), var(0))),
        "fun f (i31, struct tag, union tag, func tag, thunk tag) : i31 := KVar 0")
    add("decl/no-params", call("K.print_decl", "K.print_decl", ctor("KFun", fid("g"), items([]), ctor("RUnion", tid("any")), ctor("KErased"))),
        "fun g () : union any := KErased")

    ps = items([ctor("RStruct", tid("a")), ctor("RStruct", tid("b")), ctor("RStruct", tid("c"))])
    args = items([ctor("KGlobal", text(n)) for n in ("a", "b", "c")])
    pruning = [
        ("none", ctor("KErased"), 2, "[]|[]|KErased"),
        ("parameter", var(1), 2, "[]|[]|KVar 1"),
        ("first", var(4), 2, "[struct a]|[KGlobal a]|KVar 2"),
        ("last", var(2), 2, "[struct c]|[KGlobal c]|KVar 2"),
        ("middle", var(3), 2, "[struct b]|[KGlobal b]|KVar 2"),
        ("gaps", ctor("KApp", var(4), items([var(2), var(4), var(0)])), 2,
         "[struct a; struct c]|[KGlobal a; KGlobal c]|KApp (KVar 3) [KVar 2; KVar 3; KVar 0]"),
        ("nested-closure", ctor("KClos", fid("f"), integer(9), items([var(1)])), 0,
         "[struct b]|[KGlobal b]|KClos f 9 [KVar 0]"),
        ("nested-delay", ctor("KDelay", fid("f"), items([var(2), var(0)])), 0,
         "[struct a; struct c]|[KGlobal a; KGlobal c]|KDelay f [KVar 1; KVar 0]"),
        ("let", ctor("KLet", text("x"), var(2), var(1)), 0,
         "[struct a; struct c]|[KGlobal a; KGlobal c]|KLet x (KVar 1) (KVar 1)"),
        ("branch", ctor("KCase", tid("s"), var(0), items([branch(0, 2, var(3))])), 0,
         "[struct b; struct c]|[KGlobal b; KGlobal c]|KCase s (KVar 0) [{0 2 (KVar 3)}]"),
        ("outer", var(8), 2, "[]|[]|KVar 2"),
    ]
    for name, body, params, golden in pruning:
        result = call("R.prune_captures", "R.prune_captures", ps, args, integer(params), body)
        add("prune/" + name, call("C.pruned", "pruned", result), golden)
    add("prune/empty", call("C.pruned", "pruned", call("R.prune_captures", "R.prune_captures", items([]), items([]), integer(1), var(0))), "[]|[]|KVar 0")
    return rows
