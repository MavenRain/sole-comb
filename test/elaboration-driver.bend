import Base
import ../lib/foundation.bend as F
import ./pinfront/lexer.bend as L
import ./elaboration.bend as O

# Shared test transport. Python supplies UTF-8 text or pairs of hex digits.
def nibble(+c: U32) -> U32:
  F.choose(U32, (c <= 57 : U32), (c - 48 : U32), (c - 87 : U32))
def unhex(src: String, acc: List<&2, U32>) -> List<&2, U32>:
  match src:
    case SNil{}: L.reverse(U32, acc, [])
    case SCon{_, SNil{}}: []
    case SCon{Chr{a}, SCon{Chr{b}, rest}}: unhex(rest, (nibble(a) * 16 + nibble(b) : U32) <> acc)
def frame(name: String, +value: String) -> String:
  String.append(name, String.append("\t", String.append(O.nat(String.length(value)), String.append(":", value))))
def run_mode(raw: Bool, name: String, source: String) -> IO(Unit):
  match raw:
    case True{}: IO.print(frame(name, O.observe_bytes(unhex(source, []))))
    case False{}: IO.print(frame(name, O.observe(source)))
def run_case(args: List<String>) -> IO(Unit):
  match args:
    case name <> mode <> source <> []: run_mode(String.eq(mode, "hex"), name, source)
    case _: IO.print("invalid elaboration arguments")
def main() -> IO(Unit): IO.bind(List<String>, Unit, IO.args, run_case)
