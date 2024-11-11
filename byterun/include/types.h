#pragma once

#include "../../runtime/runtime.h"
#include "../../runtime/runtime_common.h"
#include "parser.h"
#include <stdbool.h>
#include <stdint.h>

// ------ General ------

// enum Type {
//   STR_T = STRING_TAG,
//   ARRAY_T = ARRAY_TAG,
//   SEXP_T = SEXP_TAG,
//   CLOJURE_T = CLOSURE_TAG,
// };

#define STACK_SIZE 128 * 1024

static const size_t MAX_ARRAY_SIZE = 0x11111110;

// ------ Frame ------

struct Frame {
  void *closure;      // where closure value stored if needed
  void *ret;          // store returned value [gc pointer]
  char *rp;           // ret instruction pointer [not gc pointer]
  void **prev_fp;     // ret function frame pointer [boxed value, not gc
                      // pointer]
  aint args_sz_box;   // store arguments [boxed value, not gc pointer]
  aint locals_sz_box; // store locals [boxed value, not gc pointer]
};

// NOTE: stack is [top -> bottom]
static inline size_t frame_sz() {
  return sizeof(struct Frame) / sizeof(void *);
}
static inline void **f_prev_fp(struct Frame *fp) { return fp->prev_fp; }
static inline auint f_locals_sz(struct Frame *fp) {
  return UNBOX(fp->locals_sz_box);
}
static inline auint f_args_sz(struct Frame *fp) {
  return UNBOX(fp->args_sz_box);
}
static inline void **f_locals(struct Frame *fp) {
  return (void **)fp - f_locals_sz(fp);
}
static inline void **f_args(struct Frame *fp) {
  return (void **)fp + frame_sz();
}

// ------ State ------

struct State {
  void **stack;
  void **sp;        // stack pointer
  struct Frame *fp; // function frame pointer
  bytefile *bf;
  int current_line;

  bool is_closure_call;

  char *ip;       // instruction pointer
  char *instr_ip; // poiter to current instruction
  char *call_ip;  // prev instruction pointer (to remember jmp locations)
};

void construct_state(bytefile *bf, struct State *s, void **stack);
void cleanup_state(struct State *state);

static inline void s_failure(struct State *s, const char *msg) {
  exec_failure(read_cmd(s->instr_ip), s->current_line,
               s->instr_ip - s->bf->code_ptr, msg);
}

// ------ VarCategory ------

enum VarCategory {
  VAR_GLOBAL = 0,
  VAR_LOCAL = 1,
  VAR_ARGUMENT = 2,
  VAR_CLOSURE = 3
};

extern struct State s;

static inline enum VarCategory to_var_category(uint8_t category) {
  if (category > 3) {
    s_failure(&s, "unexpected variable category");
  }
  return (enum VarCategory)category;
}
