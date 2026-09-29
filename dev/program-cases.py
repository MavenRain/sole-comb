"""Independent whole-program and recursive-group expectations."""


def cases():
    rows = []

    def add(name, source, code=0, **expected):
        rows.append(dict(name="program/" + name, source=source.encode(), code=code, **expected))

    family = "mu N : Type 0 with | zero : N | succ : N -> N\n"
    body = "fun (n : N) => case n as x in N return N with | zero => zero | succ m => f m"
    recursive = family + "def rec f : N -> N := " + body
    termination = "termination: recursive definition f failed the structural termination guard\n"
    add("empty", "", print_stdout="")
    add("family-only", family, print_stdout="")
    add("postulate", "axiom x : Nat", print_stdout="axiom x : Nat\n")
    add("plain-recursive-value", "def rec x : Nat := 0", print_stdout="def x : Nat := 0\n")
    add("plain-group-order", "def rec x : Nat := 0 and y : Nat := 1", print_stdout="def x : Nat := 0\ndef y : Nat := 1\n")
    add("ordinary-after-group", "def rec x : Nat := 0 and y : Nat := 1 def z : Nat := y", print_stdout="def x : Nat := 0\ndef y : Nat := 1\ndef z : Nat := y\n")
    add("guarded", recursive)
    add("guarded-unfold", recursive + " def z : N := f (succ zero)")
    add("unguarded", recursive.replace("f m", "f n"), 1, stderr=termination)
    add("nested-argument", recursive.replace("f m", "f (succ m)"), 1, stderr=termination)
    add("bare-self", family + "def rec f : N -> N := f", 1, stderr=termination)
    add("guard-before-typecheck", family + "def rec f : N -> N := fun (n : N) => f n n", 1)
    add("plain-forward-reference", "def rec x : Nat := y and y : Nat := 0", 1)
    add("missing-group-type", "def rec f : Missing := f", 1)
    add("missing-group-body", "def rec f : Nat := missing", 1)
    add("self-in-type", "def rec f : f := f", 1)
    add("earlier-declaration", "def A : Type := Nat def rec x : A := 0")
    add("later-family", "def rec f : N -> N := fun (n : N) => n " + family, 1)
    add("helper-without-calls", recursive + " and constant : N := zero")
    add("mutual", family + "def rec f : N -> N := " + body.replace("f m", "g m") + " and g : N -> N := " + body)
    add("mutual-unguarded", family + "def rec f : N -> N := " + body.replace("f m", "g n") + " and g : N -> N := " + body, 1, stderr=termination)
    second = "fun (z : Nat) (n : N) => case n as x in N return N with | zero => zero | succ m => f z m"
    add("second-argument", family + "def rec f : Nat -> N -> N := " + second)
    add("missing-motive", recursive.replace(" as x in N return N", ""), 1)
    add("guarded-nonhead", family + "def rec f : N -> N := fun (n : N) => let r : N := (case n as x in N return N with | zero => zero | succ m => f m) in r", 1)
    return rows
