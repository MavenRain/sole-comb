# The constructors needed by A.2, from Kanon 69f3be51 rules.ml:257-284.
# Checking and shape rules remain A.4 work.
import Base
import ./foundation.bend as F
import ./kernel_level.bend as L
import ./kernel_quantity.bend as Q
import ./kernel_shape.bend as S
import ./kernel_term.bend as T
import ./kernel_value.bend as V

def arrow(q: Q.t, name: String, domain: T.t, codomain: T.t) -> T.t:
  T.Ran{S.SPi{q, name, domain}, codomain}

def unit_ty(level: L.t) -> T.t:
  T.Ann{T.Ran{S.SColl{F.Int63.Int{False{}, []}}, T.Sec{S.SColl{F.Int63.Int{False{}, []}}, []}}, T.Univ{level}}

def unit_val() -> T.t:
  T.Sec{S.SColl{F.Int63.Int{False{}, []}}, []}

def bool_ty() -> T.t:
  T.Lan{S.SColl{F.Int63.Int{False{}, [2]}}, T.Sec{S.SColl{F.Int63.Int{False{}, [2]}}, [T.Leg{[], unit_ty(L.one)}, T.Leg{[], unit_ty(L.one)}]}}

def bool_value(b: Bool) -> V.t:
  V.VIn{S.SColl{F.Int63.Int{False{}, [2]}}, V.VALeg{F.choose(F.Int63.t, b, F.Int63.Int{False{}, [1]}, F.Int63.Int{False{}, []})}, [V.VSec{S.SColl{F.Int63.Int{False{}, []}}, []}]}
