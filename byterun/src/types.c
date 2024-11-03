#include "types.h"

#include "stack.h"
#include "../../runtime/gc.h"

#include <stdlib.h>

extern size_t __gc_stack_top, __gc_stack_bottom;

// --- Frame ---

// NOTE: stack is [top -> bottom]
size_t frame_sz() {
  return sizeof(struct Frame) / sizeof(void *);
}
void **f_prev_fp(struct Frame *fp) {
  return fp->prev_fp;
}
auint f_locals_sz(struct Frame *fp) { return UNBOX(fp->locals_sz_box); }
auint f_args_sz(struct Frame *fp) { return UNBOX(fp->args_sz_box); }
void **f_locals(struct Frame *fp) { return (void **)fp - f_locals_sz(fp); }
void **f_args(struct Frame *fp) { return (void **)fp + frame_sz(); }


// --- State ---

static struct State alloc_state(bytefile *bf) {
  struct State state = {
    .stack = calloc(STACK_SIZE + 1, sizeof(void*)),
    .ip = bf->code_ptr,
    .call_ip = NULL,
    .bf = bf,
  };

  for (size_t i = 0; i < STACK_SIZE; ++i) {
    state.stack[i] = NULL;
  }

  state.sp = state.stack + STACK_SIZE; // [top -> bottom] stack
  print_stack(&state);
  state.fp = NULL;
  return state;
}

struct State init_state(bytefile *bf) {
  __init();
  struct State state = alloc_state(bf);
  __gc_stack_bottom = (size_t)state.sp;
  // print_stack(&state);

  s_pushn_nil(&state, bf->global_area_size);

  // print_stack(&state);
  printf("- state init done\n");
  return state;
}

static void destruct_state(struct State* state) {
  free(state->stack);

  state->sp = NULL;
  state->fp = NULL;
  state->ip = NULL;
  state->call_ip = NULL;
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
