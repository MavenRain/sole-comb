# Bridge for the recursor modules not yet ported to the kernel rule style; deleted by 2e.18c.
import Base
import ./kernel_check.bend as C
import ./kernel_rules.bend as R

def rules(-A: Data, +c: C.ctx, value: R.program<C.ctx, A>) -> R.result(A):
  C.result_of(A, C.drive(A, value, C.budget(c)))
