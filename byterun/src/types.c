#include "../include/types.h"

#include <stdlib.h>

// TODO: gc use

struct State init_state(bytefile *bf) {
  struct State state = {
    .stack = calloc(1000/* TODO */, sizeof(void*)),
    .ip = bf->code_ptr,
    .prev_ip = NULL,
  };

  state.vp = *state.stack;
  state.fp = NULL;
  return state;
}

void destruct_state(struct State* state) {
  free(state->stack);

  state->vp = NULL;
  state->fp = NULL;
  state->ip = NULL;
  state->prev_ip = NULL;
}

