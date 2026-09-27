# S0-5 scratch lexer

The scratch probe now has a checked Bend lexer. This completes the byte-token
boundary of the frontend. It does not parse or typecheck a sole-comb program,
qualify an R2 endpoint, or supply a performance verdict.

The sources remain outside the repository, as required by M0-PLAN S0-5:

```text
/private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/lexer.bend
/private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/assertions.bend
/private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/validate.py
/private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/bend-policy.json
```

[The validation record](s0-5-r2-lexer.json) binds these sources, the validator,
the Bend policy file, toolchain file, launcher implementation, pin checker,
commands, and full logs to their hashes. The scratch directory is required to
rerun the checks. These files are not part of the Stage A kernel or a portable
source distribution.

## Lexer contract

`lex(fuel, text)` returns either a complete token list ending in `End` or a
typed error. The input is a Latin-1 carrier of source bytes, as in the reference
Node frontend. Every carrier character must represent one byte. Offsets and
exclusive token ends count those bytes; lines and columns start at one.
LF advances the line, and CR is whitespace, so CRLF input retains byte offsets.

Identifiers use ASCII letters, underscore, digits after the first character,
and apostrophes after the first character. Decimal tokens preserve their text;
there is no host integer conversion or implicit Nat primitive. Punctuation
includes parentheses, braces, colon, `:=`, `->`, `=>`, comma, semicolon, bar,
star, and dot. Comments start with `--` and end at LF or EOF. Non-ASCII bytes
are allowed inside comments. NUL is rejected even in a comment.

Every consumed byte spends one unit of fuel. EOF itself costs no fuel.
`Exhausted` is distinct from `BadByte` and `BadSymbol`; exhaustion never
returns partial success. The implementation has no `@unsafe` declarations.
All datatype matches are exhaustive. Five numeric dispatch fallbacks are
listed in the scratch `bend-policy.json`, and the validation record binds that
file.

Words, including `elim`, `match`, `case`, `record`, and `else`, remain word
tokens. The [scratch parser](s0-5-r2-parser.md) owns keyword interpretation
and E-R4-MATCH refusal. Lexing one of those words is not evidence that the
object language admits it.

## Validation

The pinned Bend compiler checks the generated cases before building them.
All 35 cases pass under Bun and the Node worker with the pinned 65500 KiB
stack. The same 35 cases also pass in the informational native executable.
Both pin checks pass all 35 checks.

The fixtures check complete token contents and order, exclusive spans,
line/column positions, operator lookahead, comments, CRLF, Latin-1 byte
carriers, malformed bytes and symbols, exact, insufficient, and surplus fuel,
EOF, and a 2048-byte identifier. Six compiling mutations are detected: the
wrong assignment token, reversed identifier text, the wrong column increment,
fuel exhaustion turned into success, a NUL accepted in a comment, and surplus
fuel at EOF turned into exhaustion.

The repository's `make gates` also passes: 143 regression tests (26 benchmark,
4 build, 3 pin-check, 40 evidence, 32 runner, and 38 preparation), all pin
checks, and the host entry check. Full [stdout](s0-5-r2-lexer-gates.stdout)
and [stderr](s0-5-r2-lexer-gates.stderr) are retained alongside this record.

To rerun without overwriting evidence, choose a new output directory:

```sh
python3 -P /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/validate.py \
  --output /private/tmp/claude/kan-elim-lang-m0/r2-risk/frontend/validation-next \
  --mutations
```

## Next step

The [scratch parser record](s0-5-r2-parser.md) owns the next step. Implement
the checker, the portable file-input boundary, and the small and
conversion-heavy source twins. The current lexer tests are not suitable inputs
to `r2-prepare.py` and must not be treated as qualified probe workloads. After
semantic and rejection checks pass, use the existing preparer and serial
measurement runner. The pinned endpoint remains null.
