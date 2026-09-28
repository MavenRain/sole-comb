#!/usr/bin/env python3
"""Check the A.2 representation contracts on the pinned Bend hosts."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import random
import subprocess

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "_build/representation"
EXPECTED_CASES = 465


def quoted(text):
    return json.dumps(text, ensure_ascii=False)


def integer(value):
    n = abs(value)
    limbs = []
    while n:
        limbs.append(n % 32768)
        n //= 32768
    return f'F.Int63.Int{{{"True" if value < 0 else "False"}{{}}, {limbs}}}'


def number(value):
    result = f"FT.number({quoted(str(abs(value)))})"
    return f"F.Bignum.negate({result})" if value < 0 else result


def cases():
    result = []

    def case(name, expression, expected):
        result.append((name, expression, expected))

    coll = f"S.SColl{{{integer(2)}}}"
    shape_rows = [("point", 'S.SPi{Q.One{}, "binder", T.Global{"domain"}}', "SPi", "domain", "domain", "none"),
                  ("collection", coll, "SColl", "", "none", "none"),
                  ("pair", 'S.SPar{T.Global{"left"}, T.Global{"right"}}', "SPar", "left|right", "none", "none"),
                  ("mu", 'S.SMu{"family", [T.Global{"i"}, T.Global{"j"}]}', "SMu", "i|j", "none", "family"),
                  ("nu", 'S.SNu{"cofamily", [T.Global{"k"}]}', "SNu", "k", "none", "cofamily")]
    case("shape-declared", "C.join(S.declared)", "SPi|SColl|SPar|SMu|SNu")
    case("term-formers", "C.join(T.formers)", "Lan|Ran")
    case("term-schema", "C.join(T.schema)", "In|Elim|Sec|Out")
    for label, shape, name, payload, point, family in shape_rows:
        case(f"shape-{label}-name", f"S.name(T.t, {shape})", name)
        case(f"shape-{label}-payload", f"C.join(C.term_tags(S.payload(T.t, {shape})))", payload)
        case(f"shape-{label}-point", f"C.maybe_term(S.point_dom(T.t, {shape}))", point)
        case(f"shape-{label}-family", f"C.opt_text(S.family(T.t, {shape}))", family)

    addresses = [('T.APt{Q.One{}, T.Global{"arg"}}', "1|arg", "none", "none"),
                 (f"T.ALeg{{{integer(-3)}}}", "none", "-3", "none"),
                 ('T.ACtor{"ctor"}', "none", "none", "ctor")]
    for i, (address, pt, leg, ctor) in enumerate(addresses):
        case(f"address-{i}-point", f"C.point(T.as_apt({address}))", pt)
        case(f"address-{i}-leg", f"C.opt_int(T.as_aleg({address}))", leg)
        case(f"address-{i}-ctor", f"C.opt_text(T.as_actor({address}))", ctor)

    # Each tuple places the only matching global in one distinct child slot.
    hit, miss = 'T.Global{"needle"}', 'T.Global{"other"}'
    pt = f"T.APt{{Q.Zero{{}}, {hit}}}"
    leg = f"T.Leg{{[F.Pair2{{Q.Many{{}}, \"binder\"}}], {hit}}}"
    motive = f'Some{{T.Motive{{Some{{"family"}}, ["i"], "self", {hit}}}}}'
    occurrences = [("global", hit), ("lan-diagram", f"T.Lan{{{coll}, {hit}}}"),
                   ("ran-diagram", f"T.Ran{{{coll}, {hit}}}"),
                   ("point-domain", f'T.Lan{{S.SPi{{Q.Many{{}}, "x", {hit}}}, {miss}}}'),
                   ("pair-left", f"T.Lan{{S.SPar{{{hit}, {miss}}}, {miss}}}"),
                   ("pair-right", f"T.Ran{{S.SPar{{{miss}, {hit}}}, {miss}}}"),
                   ("mu-index", f'T.Lan{{S.SMu{{"family", [{miss}, {hit}]}}, {miss}}}'),
                   ("nu-index", f'T.Ran{{S.SNu{{"family", [{hit}]}}, {miss}}}'),
                   ("in-point", f"T.In{{{coll}, {pt}, []}}"),
                   ("in-arg", f'T.In{{{coll}, T.ACtor{{"ctor"}}, [{miss}, {hit}]}}'),
                   ("section", f"T.Sec{{{coll}, [{leg}]}}"),
                   ("out-point", f"T.Out{{{coll}, {pt}, {miss}}}"),
                   ("out-head", f'T.Out{{{coll}, T.ACtor{{"ctor"}}, {hit}}}'),
                   ("elim-scrutinee", f"T.Elim{{T.Elimination{{{coll}, {hit}, Q.Many{{}}, None{{}}, []}}}}"),
                   ("elim-motive", f"T.Elim{{T.Elimination{{{coll}, {miss}, Q.Many{{}}, {motive}, []}}}}"),
                   ("elim-address", f"T.Elim{{T.Elimination{{{coll}, {miss}, Q.Many{{}}, None{{}}, [F.Pair2{{{pt}, T.Leg{{[], {miss}}}}}]}}}}"),
                   ("elim-branch", f'T.Elim{{T.Elimination{{{coll}, {miss}, Q.Many{{}}, None{{}}, [F.Pair2{{T.ACtor{{"ctor"}}, {leg}}}]}}}}'),
                   ("let-type", f'T.Let{{"x", {hit}, {miss}, {miss}}}'),
                   ("let-value", f'T.Let{{"x", {miss}, {hit}, {miss}}}'),
                   ("let-body", f'T.Let{{"x", {miss}, {miss}, {hit}}}'),
                   ("ann-term", f"T.Ann{{{hit}, {miss}}}"), ("ann-type", f"T.Ann{{{miss}, {hit}}}")]
    for label, term in occurrences:
        for include in ("False", "True"):
            case(f"occurs-{label}-{include}", f'C.boolean(T.exists_name({include}{{}}, ["needle"], {term}))', "true")
        case(f"absent-{label}", f'C.boolean(T.exists_name(True{{}}, ["absent"], {term}))', "false")
    for tag in ("SMu", "SNu"):
        for include, expected in (("False", "false"), ("True", "true")):
            case(f"family-{tag}-{include}", f'C.boolean(T.exists_name({include}{{}}, ["needle"], T.Lan{{S.{tag}{{"needle", []}}, {miss}}}))', expected)
    leaves = [f"T.Var{{{integer(-1)}}}", "T.Univ{L.one}", 'T.Lit{Lit.LString{"needle"}}', "T.Auto{}",
              f'T.Sec{{{coll}, [T.Leg{{[F.Pair2{{Q.One{{}}, "needle"}}], {miss}}}]}}',
              f'T.In{{{coll}, T.ACtor{{"needle"}}, []}}',
              f'T.Elim{{T.Elimination{{{coll}, {miss}, Q.Zero{{}}, Some{{T.Motive{{Some{{"needle"}}, ["needle"], "needle", {miss}}}}}, []}}}}']
    for i, term in enumerate(leaves):
        case(f"nonoccurring-metadata-{i}", f'C.boolean(T.exists_name(True{{}}, ["needle"], {term}))', "false")
    case("empty-name-set", f"C.boolean(T.exists_name(True{{}}, [], {hit}))", "false")
    # Each match sits after the first list item: a leg, a branch, or a name.
    case("occurs-section-second-leg", f'C.boolean(T.exists_name(False{{}}, ["needle"], T.Sec{{{coll}, [T.Leg{{[], {miss}}}, {leg}]}}))', "true")
    case("occurs-elim-second-branch", f'C.boolean(T.exists_name(False{{}}, ["needle"], T.Elim{{T.Elimination{{{coll}, {miss}, Q.Many{{}}, None{{}}, [F.Pair2{{T.ACtor{{"c"}}, T.Leg{{[], {miss}}}}}, F.Pair2{{T.ACtor{{"ctor"}}, {leg}}}]}}}}))', "true")
    case("occurs-second-name", f'C.boolean(T.exists_name(False{{}}, ["absent", "needle"], {hit}))', "true")
    case("family-second-name", f'C.boolean(T.exists_name(True{{}}, ["absent", "needle"], T.Lan{{S.SMu{{"needle", []}}, {miss}}}))', "true")
    deep = hit
    for _ in range(96):
        deep = f"T.Ann{{{miss}, {deep}}}"
    case("deep-occurrence", f'C.boolean(T.exists_name(False{{}}, ["needle"], {deep}))', "true")

    vshape = 'S.SMu{"family", [V.VUniv{L.zero}]}'
    clo = f"V.close([V.VUniv{{L.zero}}, V.VUniv{{L.one}}], T.Var{{{integer(2)}}})"
    values = ["V.VUniv{L.one}", f"V.VLan{{{vshape}, {clo}, Some{{L.one}}}}",
              f"V.VRan{{{vshape}, {clo}, None{{}}}}", f'V.VIn{{{vshape}, V.VACtor{{"ctor"}}, [V.VUniv{{L.one}}]}}',
              f"V.VSec{{{vshape}, [V.VLeg{{[], {clo}}}]}}", 'V.VLit{Lit.LString{"literal"}}', f"V.var({integer(-5)})"]
    views = [("univ", "L.t"), ("lan", "V.Former"), ("ran", "V.Former"), ("in", "V.Intro"),
             ("sec", "F.Pair2<S.t<V.t>, List<&2, V.vleg>>"), ("lit", "Lit.t"), ("neutral", "F.Pair2<V.head, List<&2, V.spine>>")]
    for i, value in enumerate(values):
        for j, (view, payload) in enumerate(views):
            case(f"value-view-{i}-{view}", f"C.present({payload}, V.as_{view}({value}))", "some" if i == j else "none")
    case("closed-environment-order", f"C.closure({clo})", "univ:0|univ:1|var:2")
    case("lan-universe", f"C.former(V.as_lan({values[1]}))", "SMu|univ:0|univ:1|var:2|1")
    case("ran-universe-absent", f"C.former(V.as_ran({values[2]}))", "SMu|univ:0|univ:1|var:2|none")
    case("introduction-payload", f"C.introduction(V.as_in({values[3]}))", "SMu|ctor|univ:1")
    case("local-level-preserved", f"C.neutral(V.as_neutral({values[6]}))", "-5")
    case("universe-view-payload", f"C.opt_level(V.as_univ({values[0]}))", "1")
    case("literal-view-payload", f"C.maybe_literal(V.as_lit({values[5]}))", "string:literal")
    case("section-payload", f'C.section(V.as_sec(V.VSec{{{vshape}, [V.VLeg{{[F.Pair2{{Q.One{{}}, "x"}}], {clo}}}]}}))', "SMu|1|x|univ:0|univ:1|var:2")
    stuck = f'V.Stuck{{{vshape}, Q.Zero{{}}, {motive}, [F.Pair2{{T.APt{{Q.One{{}}, T.Global{{"arg"}}}}, T.Leg{{[], T.Global{{"branch"}}}}}}], [V.VUniv{{L.one}}, V.VUniv{{L.zero}}]}}'
    case("stuck-elimination-and-spine-order", f'C.stuck(V.as_neutral(V.VNeutral{{V.HGlobal{{"head"}}, [V.SElim{{{stuck}}}, V.SOut{{{coll}, V.VALeg{{{integer(-4)}}}}}]}}))', "head|SMu|0|family|i|self|needle|1|arg|branch|univ:1|univ:0|SColl|-4")
    vaddresses = [("V.VAPt{Q.Zero{}, V.VUniv{L.one}}", "0|univ:1", "none", "none"),
                  (f"V.VALeg{{{integer(-2)}}}", "none", "-2", "none"), ('V.VACtor{"ctor"}', "none", "none", "ctor")]
    for i, (address, point, leg, ctor) in enumerate(vaddresses):
        for view, observer, expected in (("pt", "vpoint", point), ("leg", "opt_int", leg), ("ctor", "opt_text", ctor)):
            case(f"value-address-{i}-{view}", f"C.{observer}(V.as_{view}({address}))", expected)
    heads = [(f"V.HLocal{{{integer(-2)}}}", "local:-2"), (f"V.HLocal{{{integer(3)}}}", "local:3"),
             ('V.HGlobal{"x"}', "global:x"), ('V.HGlobal{"y"}', "global:y")]
    for i, (a, key_a) in enumerate(heads):
        for j, (b, key_b) in enumerate(heads):
            case(f"head-equality-{i}-{j}", f"C.boolean(V.head_equal({a}, {b}))", str(key_a == key_b).lower())

    prims = [("Nat_add", "natAdd"), ("Nat_sub", "natSub"), ("Nat_mul", "natMul"), ("Nat_eq", "natEq"), ("Nat_lt", "natLt")]
    bool_type = "bool|2|2|0|0|1|0|0|1"
    rng = random.Random(69251)
    numbers = [(0, 0), (0, 1), (1, 0), (7, 7), (32767, 32768), (2**62 - 1, 2), (2**140 + 5, 2**93 + 7)]
    numbers += [(rng.getrandbits(90), rng.getrandbits(80)) for _ in range(6)]
    for i, (constructor, name) in enumerate(prims):
        prim = f"P.{constructor}{{}}"
        codomain = "Nat" if i < 3 else bool_type
        expected_type = "w|a|Nat|w|b|Nat|" + codomain
        case(f"primitive-name-{name}", f"P.name({prim})", name)
        case(f"primitive-arity-{name}", f"F.Bignum.to_string(F.Bignum.from_nat(P.arity({prim})))", "2")
        case(f"primitive-lookup-{name}", f"C.prim_name(P.of_name({quoted(name)}))", name)
        case(f"primitive-type-{name}", f"C.prim_type(P.ty({prim}))", expected_type)
        case(f"initial-{name}", f"C.primitive(G.find_prim({quoted(name)}, G.initial))", name + "|" + expected_type)
        for j, (a, b) in enumerate(numbers):
            args = f"[Lit.LInt{{{number(a)}}}, Lit.LInt{{{number(b)}}}]"
            expected = [str(a + b), str(max(0, a - b)), str(a * b), f"bool|2|{int(a == b)}|0", f"bool|2|{int(a < b)}|0"][i]
            case(f"apply-{name}-{j}", f"C.applied(P.apply({prim}, {args}))", expected)
            case(f"reduce-{name}-{j}", f"C.reduced(P.reduce({prim}, {args}))", expected if i < 3 else "stuck")
        invalid = ["[]", "[Lit.LInt{F.Bignum.one}]", "[Lit.LInt{F.Bignum.one}, Lit.LInt{F.Bignum.one}, Lit.LInt{F.Bignum.one}]",
                   '[Lit.LString{"x"}, Lit.LInt{F.Bignum.one}]', '[Lit.LInt{F.Bignum.one}, Lit.LString{"x"}]',
                   f"[Lit.LInt{{{number(-1)}}}, Lit.LInt{{F.Bignum.one}}]", f"[Lit.LInt{{F.Bignum.one}}, Lit.LInt{{{number(-2)}}}]"]
        for j, args in enumerate(invalid):
            case(f"invalid-apply-{name}-{j}", f"C.applied(P.apply({prim}, {args}))", "stuck")
            case(f"invalid-reduce-{name}-{j}", f"C.reduced(P.reduce({prim}, {args}))", "stuck")
    for name in ("", "NatAdd", "natadd", "natAddx", "Nat"):
        case(f"unknown-primitive-{name}", f"C.prim_name(P.of_name({quoted(name)}))", "none")
    case("initial-Nat-universe", 'C.axiom(G.find_axiom("Nat", G.initial))', "univ:1")
    case("initial-catalog-size", "C.global_sizes(G.initial)", "6|0")

    definition = f'G.Definition{{T.Global{{"ty"}}, T.Global{{"body"}}, True{{}}, Some{{{integer(1)}}}, False{{}}}}'
    entries = [(f"G.Def{{{definition}}}", "ty"), ('G.Axiom{G.Postulate{T.Global{"ax"}}}', "ax"),
               ('G.Prim{G.Primitive{T.Global{"pty"}, P.Nat_add{}}}', "pty")]
    for i, (entry, ty) in enumerate(entries):
        case(f"entry-type-{i}", f"C.term_tag(G.entry_ty({entry}))", ty)
        for j, (view, typ) in enumerate((("def", "G.def_entry"), ("axiom", "G.axiom_entry"), ("prim", "G.prim_entry"))):
            case(f"entry-view-{i}-{view}", f"C.present({typ}, G.{view}_of({entry}))", "some" if i == j else "none")
    env = f'G.add("definition", {entries[0][0]}, G.initial)'
    case("definition-flags", f'C.definition(G.find_def("definition", {env}))', "ty|body|true|1|false")
    partial = 'G.Definition{T.Auto{}, T.Auto{}, False{}, None{}, True{}}'
    case("partial-metadata", f"C.definition(Some{{{partial}}})", "auto|auto|false|none|true")
    replaced = f'G.add("definition", {entries[1][0]}, {env})'
    case("replace-kind", f'C.definition(G.find_def("definition", {replaced}))', "none")
    case("replace-value", f'C.axiom(G.find_axiom("definition", {replaced}))', "ax")
    case("persistent-old-map", f'C.definition(G.find_def("definition", {env}))', "ty|body|true|1|false")
    for name in ("missing", "", "Nat", "natAdd"):
        case(f"empty-lookup-{name}", f"C.present(G.entry, G.find({quoted(name)}, G.empty))", "none")
    for view, name, typ in (("def", "Nat", "G.def_entry"), ("axiom", "natAdd", "G.axiom_entry"), ("prim", "Nat", "G.prim_entry")):
        case(f"wrong-entry-lookup-{view}", f"C.present({typ}, G.find_{view}({quoted(name)}, G.initial))", "none")

    ctor = f'Pos.Ctor{{"C", [F.Pair2{{Q.One{{}}, F.Pair2{{"x", T.Global{{"Ty"}}}}}}], [T.Global{{"ix"}}], {integer(4)}, True{{}}}}'
    fam = f'Pos.Family{{"Fam", [F.Pair2{{Q.Many{{}}, F.Pair2{{"p", T.Auto{{}}}}}}], [], L.one, Pos.Complete{{["C"]}}, [{ctor}], True{{}}}}'
    family_env = f'G.add_family("Nat", {fam}, {env})'
    case("family-write", f'C.family(G.find_family("Nat", {family_env}))', "Fam|1|0|1|C|1|true")
    case("family-write-preserves-entry", f'C.axiom(G.find_axiom("Nat", {family_env}))', "univ:1")
    case("entry-write-preserves-family", f'C.family(G.find_family("Nat", G.add("Nat", {entries[1][0]}, {family_env})))', "Fam|1|0|1|C|1|true")
    case("family-empty", 'C.family(G.find_family("Nat", G.empty))', "none")
    case("family-missing", f'C.family(G.find_family("missing", {family_env}))', "none")
    case("constructor-present", f'C.constructor(Pos.ctor_of("C", {fam}))', "C|1|ix|4|true")
    case("constructor-missing", f'C.constructor(Pos.ctor_of("missing", {fam}))', "none")
    case("provisional-status", "C.status(Pos.Provisional{})", "provisional")
    case("builtin-status", "C.status(Pos.Builtin{})", "builtin")
    replacement = 'Pos.Family{"Replacement", [], [], L.zero, Pos.Provisional{}, [], False{}}'
    case("family-replacement", f'C.family(G.find_family("Nat", G.add_family("Nat", {replacement}, {family_env})))', "Replacement|0|0|0|provisional|0|false")
    case("family-old-map-preserved", f'C.family(G.find_family("Nat", {family_env}))', "Fam|1|0|1|C|1|true")
    return result


def run(name, argv, env):
    completed = subprocess.run(argv, cwd=ROOT, env=env, capture_output=True, text=True, timeout=120)
    (WORK / f"{name}.log").write_text(completed.stdout + completed.stderr)
    if completed.returncode:
        raise RuntimeError(f"{name} failed ({completed.returncode}):\n{(completed.stdout + completed.stderr)[-4000:]}")
    return completed.stdout.strip()


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "result.json").unlink(missing_ok=True)
    pins = json.loads((ROOT / "dev/toolchain.json").read_text())
    spec = importlib.util.spec_from_file_location("sole_build", ROOT / "dev/build.py")
    build = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(build)
    binary, _, _ = build.compiler(pins)
    checks = cases()
    names = {name for name, _, _ in checks}
    if len(checks) != EXPECTED_CASES or len(names) != len(checks):
        raise RuntimeError(f"expected {EXPECTED_CASES} uniquely named cases, found {len(checks)} with {len(names)} names")
    imports =["import Base", "import ../../test/representation.bend as C", "import ../../test/foundation.bend as FT"]
    for filename, alias in [("foundation", "F"), ("kernel_error", "E"), ("kernel_level", "L"), ("kernel_literal", "Lit"),
                            ("kernel_quantity", "Q"), ("kernel_shape", "S"), ("kernel_term", "T"), ("kernel_value", "V"),
                            ("kernel_positivity", "Pos"), ("kernel_prim", "P"), ("kernel_rules", "R"), ("kernel_global", "G")]:
        imports.append(f"import ../../lib/{filename}.bend as {alias}")
    definitions = [f'def case_{i}() -> String:\n  F.choose(String, String.eq({expr}, {quoted(expected)}), "", {quoted(name + chr(10))})'
                   for i, (name, expr, expected) in enumerate(checks)]
    groups = []
    for start in range(0, len(checks), 32):
        body = '""'
        for i in reversed(range(start, min(start + 32, len(checks)))):
            body = f"String.append(case_{i}, {body})"
        groups.append(f"group_{start}")
        definitions.append(f"def group_{start}() -> String:\n  {body}")
    body = '"PASS"'
    for group in reversed(groups):
        body = f"String.append({group}, {body})"
    source = WORK / "checks.bend"
    source.write_text("\n\n".join(imports + definitions + [f"def main() -> String:\n  {body}\n"]))
    env = dict(os.environ, BEND_NO_TELEMETRY="1", BEND_LIB=str(ROOT / "_build/bend-cache"))
    (ROOT / "_build/bend-cache").mkdir(parents=True, exist_ok=True)
    run("check", [str(binary), str(source), "--check-only"], env)
    run("compile-js", [str(binary), str(source), "-o", str(WORK / "checks.js")], env)
    endpoints = {"bun": [pins["tools"]["bun"]["path"], str(WORK / "checks.js")],
                 "node-worker": [pins["tools"]["node"]["path"], "-e", build.node_worker_script(pins), str(WORK / "checks.js")]}
    for endpoint, argv in endpoints.items():
        output = run(endpoint, argv, env)
        if output not in ('"PASS"', "PASS"):
            raise RuntimeError(f"{endpoint} failed representation cases:\n{output}")
        print(f"PASS A.2 {endpoint}: {len(checks)} cases", flush=True)
    run("compile-native", [str(binary), str(source), "-o", str(WORK / "checks.exe")], env)
    output = run("native-info", [str(WORK / "checks.exe")], env)
    if output not in ('"PASS"', "PASS"):
        raise RuntimeError(f"native INFO failed representation cases:\n{output}")
    print(f"PASS A.2 native INFO: {len(checks)} cases", flush=True)
    inputs = sorted((ROOT / "lib").glob("*.bend")) + [ROOT / name for name in (
        "test/foundation.bend", "test/representation.bend", "dev/test-foundation.py", "dev/test-representation.py",
        "dev/build.py", "dev/pin-check.py", "dev/toolchain.json", "dev/house-bend.py", "dev/test-house.py", "dev/bend-policy.json", "Makefile")]
    sources = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    upstream = Path(pins["kanon"]["checkout"]) / "lib"
    reference = {name: hashlib.sha256((upstream / name).read_bytes()).hexdigest() for name in (
        "term.ml", "value.ml", "shape.ml", "global.ml", "prim.ml", "quantity.ml", "positivity.ml", "rules.ml")}
    report = {"milestone": "A.2", "cases_per_endpoint": len(checks), "endpoints": list(endpoints),
              "informational_endpoints": ["native"], "sources": sources,
              "harness_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
              "kanon_revision": pins["kanon"]["revision"], "kanon_sources": reference,
              "r2_qualification": "pending", "stage_a_oracle": "pending"}
    (WORK / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
