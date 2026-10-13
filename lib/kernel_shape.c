/* kernel_shape.c: C twin of lib/kernel_shape.bend (stage S1b).

   The Bend type t<-A: Data> is generic. SOLE_SHAPE_DEFINE defines one
   instance of the sole.h SOLE_SHAPE declarations. This file defines the two
   instances: TermShape (shape_*_term) and ValueShape (shape_*_value). */
#include "sole.h"

#define SOLE_SHAPE_DEFINE(Name, Elem, List, suffix)                          \
  static Name *shape_cell_##suffix(ShapeTag tag) {                           \
    Name *shape = arena_alloc(sizeof *shape);                                \
    shape->tag = tag;                                                        \
    return shape;                                                            \
  }                                                                          \
                                                                             \
  const Name *shape_spi_##suffix(Quantity quantity, Text binder,            \
                                 Elem domain) {                              \
    Name *shape = shape_cell_##suffix(SHAPE_SPI);                            \
    shape->as.spi.quantity = quantity;                                       \
    shape->as.spi.binder = binder;                                           \
    shape->as.spi.domain = domain;                                           \
    return shape;                                                            \
  }                                                                          \
                                                                             \
  const Name *shape_scoll_##suffix(Int63 size) {                             \
    Name *shape = shape_cell_##suffix(SHAPE_SCOLL);                          \
    shape->as.scoll.size = size;                                             \
    return shape;                                                            \
  }                                                                          \
                                                                             \
  const Name *shape_spar_##suffix(Elem left, Elem right) {                   \
    Name *shape = shape_cell_##suffix(SHAPE_SPAR);                           \
    shape->as.spar.left = left;                                              \
    shape->as.spar.right = right;                                            \
    return shape;                                                            \
  }                                                                          \
                                                                             \
  const Name *shape_smu_##suffix(Text family, const List *indices) {         \
    Name *shape = shape_cell_##suffix(SHAPE_SMU);                            \
    shape->as.smu.family = family;                                           \
    shape->as.smu.indices = indices;                                         \
    return shape;                                                            \
  }                                                                          \
                                                                             \
  const Name *shape_snu_##suffix(Text family, const List *indices) {         \
    Name *shape = shape_cell_##suffix(SHAPE_SNU);                            \
    shape->as.snu.family = family;                                           \
    shape->as.snu.indices = indices;                                         \
    return shape;                                                            \
  }                                                                          \
                                                                             \
  /* payload */                                                              \
  const List *shape_payload_##suffix(const Name *shape) {                    \
    switch (shape->tag) {                                                    \
    case SHAPE_SPI: return List##_cons(shape->as.spi.domain, NULL);          \
    case SHAPE_SCOLL: return NULL;                                           \
    case SHAPE_SPAR:                                                         \
      return List##_cons(shape->as.spar.left,                                \
                         List##_cons(shape->as.spar.right, NULL));           \
    case SHAPE_SMU: return shape->as.smu.indices;                            \
    case SHAPE_SNU: return shape->as.snu.indices;                            \
    }                                                                        \
    return NULL;                                                             \
  }                                                                          \
                                                                             \
  /* point_dom */                                                            \
  Elem shape_point_dom_##suffix(const Name *shape) {                         \
    switch (shape->tag) {                                                    \
    case SHAPE_SPI: return shape->as.spi.domain;                             \
    case SHAPE_SCOLL: return NULL;                                           \
    case SHAPE_SPAR: return NULL;                                            \
    case SHAPE_SMU: return NULL;                                             \
    case SHAPE_SNU: return NULL;                                             \
    }                                                                        \
    return NULL;                                                             \
  }                                                                          \
                                                                             \
  /* family */                                                               \
  MaybeText shape_family_##suffix(const Name *shape) {                       \
    switch (shape->tag) {                                                    \
    case SHAPE_SPI: return maybe_text_none();                                \
    case SHAPE_SCOLL: return maybe_text_none();                              \
    case SHAPE_SPAR: return maybe_text_none();                               \
    case SHAPE_SMU: return maybe_text_some(shape->as.smu.family);            \
    case SHAPE_SNU: return maybe_text_some(shape->as.snu.family);            \
    }                                                                        \
    return maybe_text_none();                                                \
  }                                                                          \
                                                                             \
  /* name */                                                                 \
  Text shape_name_##suffix(const Name *shape) {                              \
    switch (shape->tag) {                                                    \
    case SHAPE_SPI: return text_of_cstring("SPi");                           \
    case SHAPE_SCOLL: return text_of_cstring("SColl");                       \
    case SHAPE_SPAR: return text_of_cstring("SPar");                         \
    case SHAPE_SMU: return text_of_cstring("SMu");                           \
    case SHAPE_SNU: return text_of_cstring("SNu");                           \
    }                                                                        \
    return text_of_cstring("SPi");                                           \
  }

SOLE_SHAPE_DEFINE(TermShape, const Term *, Terms, term)
SOLE_SHAPE_DEFINE(ValueShape, const Value *, Values, value)

/* declared */
const Texts *shape_declared(void) {
  return Texts_cons(text_of_cstring("SPi"),
         Texts_cons(text_of_cstring("SColl"),
         Texts_cons(text_of_cstring("SPar"),
         Texts_cons(text_of_cstring("SMu"),
         Texts_cons(text_of_cstring("SNu"), NULL)))));
}
