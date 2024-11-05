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

static void alloc_state(bytefile *bf, struct State* s) {
  // s->stack = calloc(STACK_SIZE + 1, sizeof(void*));
  s->bf = bf;
  s->is_closure_call = false;
  s->ip = bf->code_ptr;
  s->call_ip = NULL;

  for (size_t i = 0; i < STACK_SIZE; ++i) {
    s->stack[i] = NULL;
  }

  s->sp = s->stack + STACK_SIZE; // [top -> bottom] stack
  print_stack(s);
  s->fp = NULL;
}

void init_state(bytefile *bf, struct State* s) {
  __init();
  alloc_state(bf, s);
  __gc_stack_bottom = (size_t)s->sp;
  // print_stack(s);

  s_pushn_nil(s, bf->global_area_size);

  // print_stack(&state);
  printf("- state init done\n");
}

static void destruct_state(struct State* state) {
  // free(state->stack);

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
