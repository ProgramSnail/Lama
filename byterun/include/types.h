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

#define STACK_SIZE 100000

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

auint frame_sz();
void **f_prev_fp(struct Frame *fp);
auint f_locals_sz(struct Frame *fp);
auint f_args_sz(struct Frame *fp);
void **f_locals(struct Frame *fp);
void **f_args(struct Frame *fp);

// ------ State ------

struct State {
  void *stack[STACK_SIZE + 1];
  void **sp;        // stack pointer
  struct Frame *fp; // function frame pointer
  bytefile *bf;
  int current_line;

  bool is_closure_call;

  char *ip;      // instruction pointer
  char *call_ip; // prev instruction pointer (to remember jmp locations)
};

void init_state(bytefile *bf, struct State *s);
void cleanup_state(struct State *state);

// ------ VarCategory ------

enum VarCategory {
  VAR_GLOBAL = 0,
  VAR_LOCAL = 1,
  VAR_ARGUMENT = 2,
  VAR_CLOSURE = 3
};

enum VarCategory to_var_category(uint8_t category);

void print_stack(struct State *s);
