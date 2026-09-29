import Base
import ./elaboration-driver.bend as Wire
import ./program.bend as Program
import ./program-contract.bend as Contract

def run_mode(raw: Bool, print_form: Bool, name: String, source: String) -> IO(Unit):
  match raw:
    case True{}: IO.print(Wire.frame(name, Program.observe_bytes(print_form, Wire.unhex(source, []))))
    case False{}: IO.print(Wire.frame(name, Program.observe(print_form, source)))
def run_both(raw: Bool, name: String, source: String) -> IO(Unit):
  match raw:
    case True{}: IO.print(Wire.frame(name, Program.observe_both_bytes(Wire.unhex(source, []))))
    case False{}: IO.print(Wire.frame(name, Program.observe_both(source)))
def output(mode: String, transport: String, name: String, source: String) -> IO(Unit):
  match mode:
    case "both": run_both(String.eq(transport, "hex"), name, source)
    case other: run_mode(String.eq(transport, "hex"), String.eq(other, "print"), name, source)
def run_case(args: List<String>) -> IO(Unit):
  match args:
    case ["contracts"]: IO.print(Contract.report)
    case name <> mode <> transport <> source <> []: output(mode, transport, name, source)
    case _: IO.print("invalid program arguments")
def main() -> IO(Unit): IO.bind(List<String>, Unit, IO.args, run_case)
