import UnifiedAggregation.Characterization
import UnifiedAggregation.FunctorExt
import UnifiedAggregation.Z2Group

set_option pp.universes true
set_option pp.fullNames true

-- These commands inspect the Lean baseline. They add no proof or axiom.
#check @CompCatTheory.Category
#print axioms CompCatTheory.Category
#check @CompCatTheory.Functor
#print axioms CompCatTheory.Functor
#check @CompCatTheory.LeftKanExtension
#print axioms CompCatTheory.LeftKanExtension
#check @UnifiedAggregation.SymmetryGroup
#print axioms UnifiedAggregation.SymmetryGroup
#check @UnifiedAggregation.GAction
#print axioms UnifiedAggregation.GAction
#check @UnifiedAggregation.eqRec_heq_dep
#print axioms UnifiedAggregation.eqRec_heq_dep
#check @UnifiedAggregation.Functor.ext
#print axioms UnifiedAggregation.Functor.ext
#check @UnifiedAggregation.Functor.ext_pointwise
#print axioms UnifiedAggregation.Functor.ext_pointwise
#check @UnifiedAggregation.OrbitHom.ext
#print axioms UnifiedAggregation.OrbitHom.ext
#check @UnifiedAggregation.orbitProjection
#print axioms UnifiedAggregation.orbitProjection
#check @UnifiedAggregation.Aggregation
#print axioms UnifiedAggregation.Aggregation
#check @UnifiedAggregation.discreteFunctor
#print axioms UnifiedAggregation.discreteFunctor
#check @UnifiedAggregation.Z2.one_mul
#print axioms UnifiedAggregation.Z2.one_mul
#check @UnifiedAggregation.Z2.mul_one
#print axioms UnifiedAggregation.Z2.mul_one
#check @UnifiedAggregation.Z2.mul_assoc
#print axioms UnifiedAggregation.Z2.mul_assoc
#check @UnifiedAggregation.Z2.mul_inv
#print axioms UnifiedAggregation.Z2.mul_inv
#check @UnifiedAggregation.Z2.inv_mul
#print axioms UnifiedAggregation.Z2.inv_mul
#check @UnifiedAggregation.Z2Group
#print axioms UnifiedAggregation.Z2Group
#check @UnifiedAggregation.discreteHom_eq
#print axioms UnifiedAggregation.discreteHom_eq
#check @UnifiedAggregation.lan_obj_unique_discrete
#print axioms UnifiedAggregation.lan_obj_unique_discrete
#check @UnifiedAggregation.lan_implies_orbit_constant
#print axioms UnifiedAggregation.lan_implies_orbit_constant
