import Base
import ../surface/lexer.bend as L
import ../surface/check.bend as Check

# The host launcher reads the file and supplies exact bytes as validated hex.
def unhex(src: String, acc: List<&2, U32>) -> Maybe<&2, List<&2, U32>>:
  match src:
    case SNil{}: Some{L.reverse(U32, acc, [])}
    case SCon{_, SNil{}}: None{}
    case SCon{Chr{a}, SCon{Chr{b}, rest}}:
      L.maybe(U32, Maybe<&2, List<&2, U32>>, L.hex_digit(a), _ => None{}, hi =>
        L.maybe(U32, Maybe<&2, List<&2, U32>>, L.hex_digit(b), _ => None{}, lo =>
          unhex(rest, (hi * 16 + lo : U32) <> acc)))
def checked_source(source: String) -> String:
  L.maybe(List<&2, U32>, String, unhex(source, []), _ => "FAIL\nE-CLI: invalid internal source encoding", bytes => Check.source(bytes))
def erased_source(source: String) -> String:
  L.maybe(List<&2, U32>, String, unhex(source, []), _ => "FAIL\nE-CLI: invalid internal source encoding", bytes => Check.erased_source(bytes))
def run(args: List<String>) -> IO(Unit):
  match args:
    case "check" <> source <> []: IO.print(checked_source(source))
    case "erased" <> source <> []: IO.print(erased_source(source))
    case _: IO.print("FAIL\nE-CLI: invalid internal compiler arguments")
def main() -> IO(Unit): IO.bind(List<String>, Unit, IO.args, run)
