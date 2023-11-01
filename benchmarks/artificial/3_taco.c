// Generated by the Tensor Algebra Compiler (tensor-compiler.org)
#include <stdint.h>
#ifndef TACO_TENSOR_T_DEFINED
#define TACO_TENSOR_T_DEFINED
typedef enum { taco_mode_dense, taco_mode_sparse } taco_mode_t;
typedef struct {
  int32_t      order;         // tensor order (number of modes)
  int32_t*     dimensions;    // tensor dimensions
  int32_t      csize;         // component size
  int32_t*     mode_ordering; // mode storage ordering
  taco_mode_t* mode_types;    // mode storage types
  uint8_t***   indices;       // tensor index data (per mode)
  uint8_t*     vals;          // tensor values
  uint8_t*     fill_value;    // tens/hp  or fill value
  int32_t      vals_size;     // values array size
} taco_tensor_t;
#endif

int compute(taco_tensor_t *a, taco_tensor_t *b, taco_tensor_t *c) {
  int a1_dimension = (int)(a->dimensions[0]);
  int a2_dimension = (int)(a->dimensions[1]);
  int32_t* restrict a_vals = (int32_t*)(a->vals);
  int b1_dimension = (int)(b->dimensions[0]);
  int b2_dimension = (int)(b->dimensions[1]);
  int32_t* restrict b_vals = (int32_t*)(b->vals);
  int c1_dimension = (int)(c->dimensions[0]);
  int32_t* restrict c_vals = (int32_t*)(c->vals);

  #pragma omp parallel for schedule(runtime)
  for (int32_t i = 0; i < b1_dimension; i++) {
    for (int32_t j = 0; j < c1_dimension; j++) {
      int32_t ja = i * a2_dimension + j;
      int32_t jb = i * b2_dimension + j;
      a_vals[ja] = b_vals[jb] * c_vals[j];
    }
  }
  return 0;
}

