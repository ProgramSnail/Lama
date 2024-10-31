#include "types.h"

#include "../../runtime/gc.h"

#include <stdlib.h>

extern size_t __gc_stack_top, __gc_stack_bottom;

const size_t STACK_SIZE = 100000;

// ---

void st_stack_push(struct State* state, void* value) {
  
}

void st_stack_pop(struct State* state) {
  if (state->vp == st->stack)
}

size_t st_stack_size(struct State* state) {
  return (state->stack + STACK_SIZE) - state->vp;
}

void** st_stack_top(struct State* state) {
  return state->vp;
}

// ---

static struct State alloc_state(bytefile *bf) {
  struct State state = {
    .stack = calloc(STACK_SIZE, sizeof(void*)),
    .ip = bf->code_ptr,
    .prev_ip = NULL,
  };

  state.vp = *state.stack + STACK_SIZE; // [top -> bottom] stack
  state.fp = NULL;
  return state;
}

struct State init_state(bytefile *bf) {
  __init();
  struct State state = alloc_state(bf);
  __gc_stack_bottom = (size_t)state.vp;
  return state;
}

static void destruct_state(struct State* state) {
  free(state->stack);

  state->vp = NULL;
  state->fp = NULL;
  state->ip = NULL;
  state->prev_ip = NULL;
}

void cleanup_state(struct State* state) {
  destruct_state(state);
  __shutdown();
}

