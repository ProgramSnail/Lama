#include "types.h"

#include "stack.h"
#include "utils.h"
#include "parser.h"
#include "../../runtime/gc.h"

#include <stdlib.h>

extern size_t __gc_stack_top, __gc_stack_bottom;

// --- State ---

void init_state(struct State* s, void** stack) {
  __init(); // FIXME, disable gc

  s->stack = stack;
  s->fp = NULL;
  s->bf = NULL;
  s->current_line = 0;
  s->is_closure_call = false;
  s->ip = NULL; //s->bf->code_ptr;
  s->instr_ip = NULL; //s->bf->code_ptr;
  s->call_ip = NULL;

  for (size_t i = 0; i < STACK_SIZE; ++i) {
    s->stack[i] = NULL;
  }

  __gc_stack_bottom = (size_t)(s->stack + STACK_SIZE);
  __gc_stack_top = __gc_stack_bottom;

#ifdef DEBUG_VERSION
  printf("- state init done\n");
#endif
}

void prepare_state(Bytefile* bf, struct State* s) {
  // init data
  s->bf = bf;

  // clearup from previous executions

  s->is_closure_call = false;
  s->call_ip = NULL;
  s->current_line = 0;

  s->fp = NULL;

  s->ip = s->bf->code_ptr + s->bf->main_offset;
  s->instr_ip = s->ip;

#ifdef DEBUG_VERSION
  printf("- mod state init done\n");
#endif
}

void push_globals(struct State *s) {
  s->bf->global_ptr = s_peek(); // (void*)__gc_stack_top;
  s_pushn_nil(s->bf->global_area_size);

#ifdef DEBUG_VERSION
  printf("- state globals init done\n");
#endif
}

static void destruct_state(struct State* state) {
  // free(state->stack);

  // state->sp = NULL;
  state->fp = NULL;
  state->ip = NULL;
  state->call_ip = NULL;
}

void cleanup_state(struct State* state) {
  destruct_state(state);
  __shutdown();
}
