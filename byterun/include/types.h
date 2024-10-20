#pragma once

#include "parser.h"
#include <stdint.h>

// ------ Var ------

// TODO: clojures
enum Type {
  NIL_T = 0x00000000,
  INT_T = 0x00000001,
  CONST_STR_T = 0x00000002,
  STR_T = 0x00000003,
  LIST_T = 0x00000004,
  ARRAY_T = 0x00000005,
  SEXP_T = 0x00000006,
  FUN_T = 0x00000007
};

struct NilT { // AnyVarT too
  int32_t data_header;
};

struct IntT {
  int32_t data_header;
  int32_t value; // int value => size = 1;
};

struct ConstStrT {
  int32_t data_header;
  const char *value;
};

struct StrT {
  int32_t data_header;
  char *value;
};

struct ListT {
  int32_t data_header;
  struct NilT *value;
  struct NilT *next;
};

struct ArrayT {
  int32_t data_header;
  struct NilT **values;
};

struct SExpT {
  int32_t data_header;
  const char *tag;
  struct NilT *next;
};

struct FunT {
  int32_t data_header;
  char *fun_ip;
};

union VarT {
  struct NilT nil;
  struct IntT int_t;
  struct ConstStrT const_str;
  struct StrT str;
  struct ListT list;
  struct ArrayT array;
  struct SExpT sexp;
  struct FunT fun;
};

// same to TAG in runtime
inline enum Type dh_type(int data_header) {
  return (Type)(data_header & 0x00000007);
}

// same to LEN in runtime
inline int dh_param(int data_header) { return (data_header & 0xFFFFFFF8) >> 3; }

inline union VarT *to_var(struct NilT *var) { return (union VarT *)var; }

// ------ Frame ------

struct Frame {
  char *rp;             // ret instruction pointer
  struct NilT **ret;    // store returned value
  struct NilT **params; // store arguments
  struct NilT **locals; // store locals
  struct NilT **end;    // store locals
};

inline uint64_t frame_locals_sz(struct Frame *frame) {
  return frame->locals - frame->params;
}
inline uint64_t frame_params_sz(struct Frame *frame) {
  return frame->end - frame->locals;
}

// ------ State ------

union StackValue {
  union VarT *var;
  union VarT **var_ptr;
  // struct Frame frame; // TODO
  char *addr;
};

// inline StackValue *to_sv(void *var) { return (StackValue *)var; }

struct State {
  union StackValue *stack; // vaid**
  struct NilT **vp;        // var pointer
  struct Frame *fp;        // function frame pointer

  char *ip;      // instruction pointer
  char *prev_ip; // prev instruction pointer
};

struct State init_state(bytefile *bf);
void destruct_state(struct State *state);
