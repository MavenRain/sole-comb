"""Focused source cases for the pinned expression/declaration entry points."""


def cases():
    rows = []

    def add(name, source, expected=None, prefix=None):
        row = {"name": "probe/" + name, "source": source.encode("utf8")}
        if expected is not None:
            row["expected"] = expected
        if prefix is not None:
            row["prefix"] = prefix
        rows.append(row)

    add("empty", "", "ok:")
    add("axiom", "axiom A : Type", prefix="ok:")
    add("literal", "def three : Nat := 3", prefix="ok:")
    add("large-literal", "def large : Nat := 123456789012345678901234567890", prefix="ok:")
    add("identity", "def id : (x : Nat) -> Nat := fun (x : Nat) => x", prefix="ok:")
    add("application", "def id : (x : Nat) -> Nat := fun (x : Nat) => x def n : Nat := id 4", prefix="ok:")
    add("local-shadow", "axiom x : Type def f : (x : Nat) -> Nat := fun (x : Nat) => x", prefix="ok:")
    add("nested-shadow", "def f : (x : Nat) -> (x : Nat) -> Nat := fun (x : Nat) (x : Nat) => x", prefix="ok:")
    add("dependent-identity", "def f : (A : Type) -> (x : A) -> A := fun (A : Type) (x : A) => x", prefix="ok:")
    add("let", "def n : Nat := let x : Nat := 3 in x", prefix="ok:")
    add("let-shadow", "def n : Nat := let x : Nat := 2 in let x : Nat := 3 in x", prefix="ok:")
    add("annotation", "def n : Nat := (3 : Nat)", prefix="ok:")
    pair = "def p : (x : Nat) * Nat := (2, 3) "
    add("pair", pair, prefix="ok:")
    for key in (0, 1, 2, 3):
        add(f"pair-projection-{key}", pair + f"def n : Nat := p.{key}", prefix="ok:" if key in (1, 2) else "error:wrong leg:")
    add("dependent-pair-intro", "axiom A : Type axiom a : A def p : (X : Type) * X := (A, a)", prefix="ok:")
    add("dependent-pair", "axiom A : Type axiom a : A def p : (X : Type) * X := (A, a) def x : p.1 := p.2",
        "error:cannot infer: an elimination without a motive needs an expected type")
    add("pair-without-expectation", "def n : Nat := (2, 3).1")
    add("pair-wrong-expectation", "def n : Nat := (2, 3)", "error:mismatch: a pair needs a left former as its expected type")
    add("application-not-function", "def n : Nat := 0 1", "error:mismatch: the head of an application is not a function")
    add("projection-not-former", "def n : Nat := 0.1", "error:mismatch: a projection reads a pair or a tuple, and this scrutinee is neither")
    for width in range(5):
        types = ", ".join(["Nat"] * width)
        values = ", ".join(map(str, range(width)))
        source = f"def t : prod ({types}) := tuple ({values}) "
        add(f"tuple-{width}", source, prefix="ok:" if width else None)
        for key in range(width):
            add(f"tuple-{width}-projection-{key}", source + f"def n : Nat := t.{key}", prefix="ok:")
    for key in (0, 1, 2):
        add(f"sum-injection-{key}", f"def s : sum (Nat, Nat) := inj {key} of 2 3", prefix="ok:" if key < 2 else None)
    sum_source = "def s : sum (Nat, Nat) := inj 0 of 2 3 "
    branches = "with | 0 (x : Nat) => x | 1 (y : Nat) => y"
    add("collection-case", sum_source + "def n : Nat := case s " + branches, prefix="ok:")
    add("collection-motive", sum_source + "def n : Nat := case s as self return Nat " + branches, prefix="ok:")
    add("collection-payload-type", sum_source + "def n : Nat := case s with | 0 (x : Type) => x | 1 (y : Type) => y", prefix="ok:")
    add("collection-missing-branch", sum_source + "def n : Nat := case s with | 0 (x : Nat) => x")
    add("collection-constructor-branch", sum_source + "def n : Nat := case s with | c x => x")
    add("collection-index-clause", sum_source + "def n : Nat := case s as self in F return Nat " + branches,
        "error:mismatch: a case at a collection takes no index clause and its motive names F")
    add("match-collection", sum_source + "def n : Nat := match s with",
        "error:mismatch: a match needs a mu family as the type of its scrutinee")
    add("absurd", "axiom e : (sum () : Type) def n : Nat := absurd e")
    family = "mu F : Type := | z : F | s (x : F) : F "
    add("family", family, prefix="ok:")
    add("constructor", family + "def x : F := s z", prefix="ok:")
    add("family-shadow", family + "def id : (F : Type) -> (x : F) -> F := fun (F : Type) (x : F) => x", prefix="ok:")
    add("constructor-arity", family + "def x : F := z z", "error:mismatch: z takes 0 arguments and the term gives 1")
    add("family-arity", family + "axiom x : F Nat", "error:mismatch: F takes 0 arguments and the term gives 1")
    match = family + "def n : Nat := match (z : F) as self in F return Nat with | z => 0 | s "
    add("family-match", match + "x => 1", prefix="ok:")
    add("field-annotation", match + "(x : F) => 1", prefix="ok:")
    add("field-annotation-refused", match + "(x : Nat) => 1",
        "error:mismatch: the annotation of constructor field x differs from its declared type")
    add("field-count-refused", match + "=> 1", "error:missing branch: the branch at s binds 0 fields and s takes 1")
    add("unknown-constructor", family + "def n : Nat := match (z : F) as self in F return Nat with | nope => 0")
    add("missing-motive", family + "def n : Nat := match (z : F) with | z => 0 | s x => 1")
    add("wrong-motive-index-count", family + "def n : Nat := match (z : F) as self in F i return Nat with",
        "error:mismatch: the motive of F binds 1 index names and the family has 0")
    add("mutual", "mutual mu A : Type := | a (x : B) : A mu B : Type := | b : B end", prefix="ok:")
    add("negative-field", "mu F : Type := | c (f : (x : F) -> Nat) : F")
    add("wrong-constructor-result", "mu F : Type := | c : Nat", "error:mismatch: a constructor of F ends at another type")
    add("wrong-family-header", "mu F : Nat :=", "error:universe: the header of a family ends in a universe")
    add("parameterized-family", "mu Box (A : Type) : Type := | box (x : A) : Box A", prefix="ok:")
    add("parameterized-constructor", "mu Box (A : Type) : Type := | box (x : A) : Box A def x : Box Nat := box 0")
    add("recursive-declaration", "def rec f : (x : Nat) -> Nat := fun (x : Nat) => x",
        "error:mismatch: a recursive group is elaborated as a group")
    for mark in ("0 ", "1 ", ""):
        add("quantity-" + (mark.strip() or "many"), f"def f : ({mark}x : Nat) -> Nat := fun ({mark}x : Nat) => " + ("0" if mark == "0 " else "x"), prefix="ok:")
    for depth in (2, 4, 8, 16):
        binders = " ".join(f"(x{i} : Nat)" for i in range(depth))
        ty = " -> ".join([f"(x{i} : Nat)" for i in range(depth)] + ["Nat"])
        add(f"nested-binders-{depth}", f"def f : {ty} := fun {binders} => x0", prefix="ok:")
    return rows
