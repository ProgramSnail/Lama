#pragma once

#include "../../runtime/runtime.h"
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
  uint32_t data_header;
};

struct IntT {
  uint32_t data_header;
  int32_t value; // int value => size = 1;
};

struct ConstStrT {
  uint32_t data_header;
  const char *value;
};

struct StrT {
  uint32_t data_header;
  char *value;
};

struct ListT {
  uint32_t data_header;
  struct NilT *value;
  struct NilT *next;
};

struct ArrayT {
  uint32_t data_header;
  struct NilT **values;
};
const size_t MAX_ARRAY_SIZE = 0x11111110;

struct SExpT {
  uint32_t data_header;
  const char *tag;
  struct NilT *next;
};

struct FunT {
  uint32_t data_header;
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
  struct NilT *ret;      // store returned value
  char *rp;              // ret instruction pointer
  struct Frame *prev_fp; // ret function frame pointer
  void **params;         // store arguments
  void **locals;         // store locals
  void **end;            // store locals
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
  struct Frame frame; // ??
  char *addr;
};

// inline StackValue *to_sv(void *var) { return (StackValue *)var; }

struct State {
  void **stack;     // vaid**
  void **vp;        // stack pointer
  struct Frame *fp; // function frame pointer

  char *ip;      // instruction pointer
  char *prev_ip; // prev instruction pointer
};

struct State init_state(bytefile *bf);
void destruct_state(struct State *state);

// ------ VarCategory ------

enum VarCategory {
  VAR_GLOBAL = 0,
  VAR_LOCAL = 1,
  VAR_A = 2, // TODO: ??
  VAR_C = 3  // TODO: ??
};

inline enum VarCategory to_var_category(uint8_t category) {
  if (category > 3) {
    failure("unexpected variable category");
  }
  return (VarCategory)category;
}
