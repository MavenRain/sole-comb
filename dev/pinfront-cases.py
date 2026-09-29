"""Deterministic pinned-parser probes, including expectations independent of either port."""
def cases():
    rows = []
    def add(name, source, **fixed):
        rows.append({"name": name, "source": source.encode() if isinstance(source, str) else source, **fixed})
    add("empty", "", printed="")
    add("comment-eof", "-- comment", printed="")
    add("comment-newline", "-- comment\r\n", printed="")
    add("long-comment", "--" + "x" * 40000, printed="")
    name = "x" * 40000
    add("long-identifier", "axiom " + name + " : Type", printed="axiom " + name + " : Type 0\n")
    add("identity", "def id : Type := fun (x : Type) => x",
        printed="def id : Type 0 := fun (x : Type 0) => x\n")
    add("declaration-order", "axiom a : Prop axiom b : Type def c : Type := a",
        printed="axiom a : Prop\naxiom b : Type 0\ndef c : Type 0 := a\n")
    add("binder-backtrack", "axiom a : (x : Type)", printed="axiom a : (x : Type 0)\n")
    add("binder-commit", "axiom a : (x : Type) -> x", printed="axiom a : (x : Type 0) -> x\n")
    add("application-left", "axiom a : f x y", printed="axiom a : f x y\n")
    add("arrow-right", "axiom a : A -> B -> C", printed="axiom a : (_ : A) -> (_ : B) -> C\n")
    add("star-right", "axiom a : A * B * C", printed="axiom a : (_ : A) * (_ : B) * C\n")
    add("arrow-star", "axiom a : A -> B * C", printed="axiom a : (_ : A) -> (_ : B) * C\n")
    add("projection-digits", "axiom a : x.1.2.10.01.0",
        printed="axiom a : x.1.2.10.1.0\n")
    add("leading-zero-bounds", "axiom a : Type 000000000000000000000000001",
        printed="axiom a : Type 1\n")
    add("host-bound-max", "axiom a : Type 999999999999999999", printed="axiom a : Type 999999999999999999\n")
    add("host-bound-overflow", "axiom a : Type 1000000000000000000", error="numeric literal too long")
    add("projection-overflow", "axiom a : x.1000000000000000000", error="numeric literal too long")
    add("inj-overflow", "axiom a : inj 1000000000000000000 of 2 x", error="numeric literal too long")
    add("inj-size-overflow", "axiom a : inj 1 of 1000000000000000000 x", error="numeric literal too long")
    add("branch-overflow", "axiom a : case x with | 1000000000000000000 => y", error="numeric literal too long")
    add("unbounded-natural", "axiom a : 100000000000000000000000000000000000000000",
        printed="axiom a : 100000000000000000000000000000000000000000\n")
    add("quantity-marks", "axiom a : fun (0 x : Type) (1 y : x) (z : y) => z",
        printed="axiom a : fun (0 x : Type 0) (1 y : x) (z : y) => z\n")
    add("quantity-leading-zeros", "axiom a : fun (000 x : Type) (01 y : x) => y",
        printed="axiom a : fun (0 x : Type 0) (1 y : x) => y\n")
    add("invalid-binder-mark", "axiom a : fun (2 x : Type) => x", error="a binder name and ':'")
    add("no-fun-binder", "axiom a : fun => x", error="expected '('")
    add("let", "axiom a : let x : Type := A in x", printed="axiom a : let x : Type 0 := A in x\n")
    add("empty-collections", "axiom a : tuple (sum ( ), prod (), tuple ())",
        printed="axiom a : tuple (sum (), prod (), tuple ())\n")
    add("multiple-items", "axiom a : prod (A, B, C)", printed="axiom a : prod (A, B, C)\n")
    add("case-legs", "axiom a : case x with | 1 (a : A) => a | 2 (b : B) => b",
        printed="axiom a : case x with | 1 (a : A) => a | 2 (b : B) => b\n")
    add("match-leg-refused", "axiom a : match x with | 1 => x",
        error="a match branch keys a constructor, not a leg number")
    add("match-empty", "axiom a : match x with", printed="axiom a : match x with\n")
    add("case-empty", "axiom a : case x with", printed="axiom a : case x with\n")
    add("typed-fields", "axiom a : match x with | c 0 a (1 b : A) c => c",
        printed="axiom a : match x with | c 0 a (1 b : A) c => c\n")
    add("motive-indices", "axiom a : match x as y in F i j return P y with | c => z",
        printed="axiom a : match x as y in F i j return P y with | c => z\n")
    add("motive-without-indices", "axiom a : case x as y return P with", printed="axiom a : case x as y return P with\n")
    add("legacy-family", "mu F (A : Type) : Type with | c : F A",
        printed="mu F (A : Type 0) : Type 0 :=\n| c : F A\n")
    add("constructor-sugar", "mu F : Type := | c (0 A : Type) (a : A) : F",
        printed="mu F : Type 0 :=\n| c : (0 A : Type 0) -> (a : A) -> F\n")
    add("second-constructor", "mu F : Type := | a : F | b : F",
        printed="mu F : Type 0 :=\n| a : F\n| b : F\n")
    add("legacy-mutual", "mu A : Type with | a : A and B : Type with | b : B",
        printed="mutual\nmu A : Type 0 :=\n| a : A\nmu B : Type 0 :=\n| b : B\nend\n")
    add("explicit-mutual", "mutual mu A : Type := mu B : Type := end",
        printed="mutual\nmu A : Type 0 :=\nmu B : Type 0 :=\nend\n")
    add("mutual-empty", "mutual end", error="a mutual group needs at least two mu declarations")
    add("mutual-singleton", "mutual mu A : Type := end", error="a mutual group needs at least two mu declarations")
    add("mutual-unclosed", "mutual mu A : Type :=", error="'mu' or 'end' in a mutual group")
    add("recursive-group", "def rec f : A := x and g : B := y",
        printed="def rec f : A := x\nand g : B := y\n")
    add("nu-declaration", "nu", error="nu arrives at M2")
    add("nu-atom", "axiom a : nu", error="nu arrives at M2")
    add("mu-atom", "axiom a : mu", error="a mu group is a declaration and not a term")
    add("and-atom", "axiom a : and", error="'and' joins two members")
    add("orphan-dot", "axiom a : x.", error="expected a leg number after '.'")
    add("orphan-bar", "axiom a : case x with |", error="expected a leg number or a constructor name after '|'")
    add("orphan-constructor", "mu A : Type := |", error="expected a constructor name and ':' after '|'")
    add("bad-as", "axiom a : case x as with", error="expected 'NAME [in FAMILY IDX..] return TYPE' after 'as'")
    add("bad-index-clause", "axiom a : case x as y in return A with", error="expected 'return'")
    add("field-mark-unconsumed", "axiom a : case x with | c 1 => x", error="expected '=>'")
    add("trailing-comma", "axiom a : tuple (x,)", error="expected a term")
    add("empty-parens", "axiom a : ( )", error="expected a term")
    add("bytes-empty", 'axiom a : b""', printed="axiom a : bytesNil\n")
    add("bytes-utf8", 'axiom a : b"é猫🧪"')
    add("comment-utf8", '-- é猫🧪\naxiom a : Type', printed="axiom a : Type 0\n")
    add("utf8-outside", "é", error="unexpected character '\\195'")
    add("byte-escapes", r'axiom a : b"\n\r\t\0\\\"\x00\x7f\xFF"')
    add("bad-hex", r'axiom a : b"\xg0"', error="byte escape requires two hexadecimal digits")
    add("short-hex", r'axiom a : b"\x0', error="invalid byte escape \\x")
    add("bad-escape", r'axiom a : b"\z"', error="invalid byte escape \\z")
    add("terminal-backslash", 'axiom a : b"\\', error="unterminated byte escape")
    add("unclosed-bytes", 'axiom a : b"abc', error="unterminated byte literal")
    add("newline-bytes", 'axiom a : b"a\n"', error="unescaped newline in byte literal")
    add("cr-bytes", 'axiom a : b"a\r"', error="unescaped newline in byte literal")
    add("line-column", "\r\t-- xyz\n@", error="unexpected character '@'", contains="line 2, column 1")
    for byte in range(256):
        add(f"single-byte-{byte:03}", bytes([byte]))
    for byte in [0, 1, 8, 9, 10, 13, 31, 32, 34, 39, 65, 92, 127, 128, 195, 233, 255]:
        add(f"literal-byte-{byte:03}", b'axiom a : b"' + bytes([byte]) + b'"')
    atoms = ["x", "123", "Prop", "Type 2", "auto", "()", "(x, y)", "tuple (a, b)", "sum (A, B)",
             "prod (A, B)", "x.10", "(f x)", "(x : A)", "natAdd", "natSub", "natMul", "natEq", "natLt"]
    for i, atom in enumerate(atoms):
        for j, form in enumerate([atom, f"f {atom}", f"{atom} -> Type", f"fun (x : A) => {atom}"]):
            add(f"grammar-{i:02}-{j}", "axiom generated : " + form)
    return rows
