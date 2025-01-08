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
  void *closure;       // where closure value stored if needed
  void *ret;           // store returned value [gc pointer]
  char *rp;            // ret instruction pointer [not gc pointer]
  void **prev_fp;      // ret function frame pointer [boxed value, not gc
                       // pointer]
  aint ret_module_box; // module to return [boxed value, not gc pointer]
  aint args_sz_box;    // store arguments [boxed value, not gc pointer]
  aint locals_sz_box;  // store locals [boxed value, not gc pointer]
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
  // void **sp;        // stack pointer
  struct Frame *fp; // function frame pointer
  Bytefile *bf;
  int current_line;

  bool is_closure_call;

  uint current_module_id;
  uint call_module_id;

  char *ip;       // instruction pointer
  char *instr_ip; // poiter to current instruction
  char *call_ip;  // prev instruction pointer (to remember jmp locations)
};

void init_state(struct State *s, void **stack);
void init_mod_state(uint mod_id, struct State *s);
void init_mod_state_globals(struct State *s);
void cleanup_state(struct State *state);

// TODO: print current mod id
static inline void s_failure(struct State *s, const char *msg) {
  exec_failure(read_cmd(s->instr_ip, s->bf), s->current_line,
               s->instr_ip - s->bf->code_ptr, msg);
}

static inline void ip_failure(char *ip, Bytefile *bf, const char *msg) {
  exec_failure(read_cmd(ip, bf), 0, ip - bf->code_ptr, msg);
}

static inline void ip_safe_failure(char *ip, Bytefile *bf, const char *msg) {
  exec_failure("_UNDEF_", 0, ip - bf->code_ptr, msg);
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

enum CMD_TOPLVL {
  CMD_BINOP = 0,
  CMD_BASIC,
  CMD_LD,
  CMD_LDA,
  CMD_ST,
  CMD_CTRL,
  CMD_PATT,
  CMD_BUILTIN,
  CMD_EXIT = 15,
};

enum CMD_BINOPS {
  CMD_BINOP_ADD = 0, // +
  CMD_BINOP_SUB,     // -
  CMD_BINOP_MULT,    // *
  CMD_BINOP_DIV,     // /
  CMD_BINOP_MOD,     // %
  CMD_BINOP_LEQ,     // <
  CMD_BINOP_LT,      // <=
  CMD_BINOP_GT,      // >
  CMD_BINOP_GEQ,     // >=
  CMD_BINOP_EQ,      // ==
  CMD_BINOP_NEQ,     // !=
  CMD_BINOP_AND,     // &&
  CMD_BINOP_OR,      // !!
};

enum CMD_BASICS {
  CMD_BASIC_CONST = 0,
  CMD_BASIC_STRING,
  CMD_BASIC_SEXP,
  CMD_BASIC_STI,
  CMD_BASIC_STA,
  CMD_BASIC_JMP,
  CMD_BASIC_END,
  CMD_BASIC_RET,
  CMD_BASIC_DROP,
  CMD_BASIC_DUP,
  CMD_BASIC_SWAP,
  CMD_BASIC_ELEM,
};

enum CMD_CTRLS {
  CMD_CTRL_CJMPz = 0,
  CMD_CTRL_CJMPnz,
  CMD_CTRL_BEGIN,
  CMD_CTRL_CBEGIN,
  CMD_CTRL_CLOSURE,
  CMD_CTRL_CALLC,
  CMD_CTRL_CALL,
  CMD_CTRL_TAG,
  CMD_CTRL_ARRAY,
  CMD_CTRL_FAIL,
  CMD_CTRL_LINE,
  CMD_CTRL_CALLF,
};

enum CMD_PATTS {
  CMD_PATT_STR = 0,
  CMD_PATT_STR_TAG,
  CMD_PATT_ARRAY_TAG,
  CMD_PATT_SEXP_TAG,
  CMD_PATT_REF_TAG,
  CMD_PATT_VAL_TAG,
  CMD_PATT_FUN_TAG,
};

enum CMD_BUILTINS {
  CMD_BUILTIN_Lread = 0,
  CMD_BUILTIN_Lwrite,
  CMD_BUILTIN_Llength,
  CMD_BUILTIN_Lstring,
  CMD_BUILTIN_Barray,
};
