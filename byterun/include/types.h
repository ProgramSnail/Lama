#pragma once

#include "../../runtime/runtime.h"
#include "parser.h"
#include <stdint.h>

// ------ Var ------

enum Type {
  NIL_T = 0x00000000,
  INT_T = 0x00000001,
  BOX_T = 0x00000002,
  STR_T = 0x00000003,
  CLOJURE_T = 0x00000004,
  ARRAY_T = 0x00000005,
  SEXP_T = 0x00000006,
  FUN_T = 0x00000007
};

struct NilT { // AnyVarT too
  uint32_t data_header;
};

struct IntT {
  uint32_t data_header;
  int32_t value;
};

struct BoxT {
  uint32_t data_header;
  struct NilT **value;
};

struct StrT {
  uint32_t data_header; // param - is not const (0 for const, 1 for not const)
  const char *value;
};

struct ClojureT { // TODO
  uint32_t data_header;
  char *fun_ip;
  struct ArrayT *vars;
};

// struct ListT {
//   uint32_t data_header;
//   struct NilT *value;
//   struct NilT *next;
// };

struct ArrayT {
  uint32_t data_header;
  struct NilT **values;
};
static const size_t MAX_ARRAY_SIZE = 0x11111110;

struct SExpT {
  uint32_t data_header;
  const char *tag;
  struct NilT **values;
};

struct FunT {
  uint32_t data_header;
  char *fun_ip;
};

union VarT {
  struct NilT nil;
  struct IntT int_t;
  struct BoxT box;
  struct StrT str;
  struct ClojureT clojure;
  // struct ListT list;
  struct ArrayT array;
  struct SExpT sexp;
  struct FunT fun;
};

// same to TAG in runtime
static inline enum Type dh_type(int data_header) {
  return (enum Type)(data_header & 0x00000007);
}

// same to LEN in runtime
static inline int dh_param(int data_header) {
  return (data_header & 0xFFFFFFF8) >> 3;
}

static inline union VarT *to_var(struct NilT *var) { return (union VarT *)var; }

// ------ Frame ------

// TODO: store boxed offsets instead
struct Frame {
  struct NilT *ret;      // store returned value
  char *rp;              // ret instruction pointer
  struct Frame *prev_fp; // ret function frame pointer
  void **params;         // store arguments
  void **locals;         // store locals
  void **end;            // store locals
};

static inline uint64_t frame_locals_sz(struct Frame *frame) {
  return frame->locals - frame->params;
}
static inline uint64_t frame_params_sz(struct Frame *frame) {
  return frame->end - frame->locals;
}

// ------ State ------

union StackValue {
  union VarT *var;
  union VarT **var_ptr;
  struct Frame frame; // ??
  char *addr;
};

// static inline StackValue *to_sv(void *var) { return (StackValue *)var; }

struct State {
  void **stack;     // vaid**
  void **vp;        // stack pointer
  struct Frame *fp; // function frame pointer

  char *ip;      // instruction pointer
  char *prev_ip; // prev instruction pointer
};

struct State init_state(bytefile *bf);
void cleanup_state(struct State *state);

// ------ VarCategory ------

enum VarCategory {
  VAR_GLOBAL = 0,
  VAR_LOCAL = 1,
  VAR_ARGUMENT = 2,
  VAR_C = 3 // TODO: constant ??
};

static inline enum VarCategory to_var_category(uint8_t category) {
  if (category > 3) {
    failure("unexpected variable category");
  }
  return (enum VarCategory)category;
}
