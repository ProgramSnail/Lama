#include "types.h"

#include "../../runtime/gc.h"

#include <stdlib.h>

extern size_t __gc_stack_top, __gc_stack_bottom;

const size_t STACK_SIZE = 100000;

// --- Frame ---

// NOTE: stack is [top -> bottom]
size_t frame_sz() {
  return sizeof(struct Frame) / sizeof(void *);
}
void **f_prev_fp(struct Frame *fp) {
  return (void **)fp + UNBOX(fp->to_prev_fp_box);
}
uint64_t f_locals_sz(struct Frame *fp) { return UNBOX(fp->locals_sz_box); }
uint64_t f_args_sz(struct Frame *fp) { return UNBOX(fp->args_sz_box); }
void **f_locals(struct Frame *fp) { return (void **)fp - f_locals_sz(fp) - frame_sz(); }
void **f_args(struct Frame *fp) { return (void **)fp + 1; }


// --- State ---

static struct State alloc_state(bytefile *bf) {
  struct State state = {
    .stack = calloc(STACK_SIZE + 1, sizeof(void*)),
    .ip = bf->code_ptr,
    .prev_ip = NULL,
  };

  state.sp = *state.stack + STACK_SIZE; // [top -> bottom] stack
  state.fp = NULL;
  return state;
}

struct State init_state(bytefile *bf) {
  __init();
  struct State state = alloc_state(bf);
  __gc_stack_bottom = (size_t)state.sp;
  return state;
}

static void destruct_state(struct State* state) {
  free(state->stack);

  state->sp = NULL;
  state->fp = NULL;
  state->ip = NULL;
  state->prev_ip = NULL;
}

void cleanup_state(struct State* state) {
  destruct_state(state);
  __shutdown();
}

// --- VarCategory ---

enum VarCategory to_var_category(uint8_t category) {
  if (category > 3) {
    failure("unexpected variable category");
  }
  return (enum VarCategory)category;
}
