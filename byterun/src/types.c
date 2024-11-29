#include "types.h"

#include "module_manager.h"
#include "stack.h"
#include "utils.h"
#include "parser.h"
#include "../../runtime/gc.h"

#include <stdlib.h>

extern size_t __gc_stack_top, __gc_stack_bottom;

// --- State ---

void init_state(struct State* s, void** stack) {
  __init();

  s->stack = stack;
  s->bf = NULL;
  s->is_closure_call = false;
  s->current_module_id = 0;
  s->call_module_id = 0;
  s->ip = s->bf->code_ptr;
  s->instr_ip = s->bf->code_ptr;
  s->call_ip = NULL;
  s->current_line = 0;

  for (size_t i = 0; i < STACK_SIZE; ++i) {
    s->stack[i] = NULL;
  }

  s->fp = NULL;

  __gc_stack_bottom = (size_t)(s->stack + STACK_SIZE);
  __gc_stack_top = __gc_stack_bottom;

#ifdef DEBUG_VERSION
  print_stack(s);
  printf("- state init done\n");
#endif
}

void init_mod_state(uint mod_id, struct State* s) {
  // init module data
  s->bf = mod_get(mod_id);
  s->current_module_id = mod_id;

  // clearup from previous executions

  s->is_closure_call = false;
  s->current_module_id = 0;
  s->call_module_id = 0;
  s->call_ip = NULL;
  s->current_line = 0;

  s->fp = NULL;

#ifdef DEBUG_VERSION
  print_stack(s);
  printf("- mod state init done\n");
#endif
}

void init_mod_state_globals(struct State *s) {
  s_pushn_nil(s->bf->global_area_size);
  s->bf->global_ptr = (void*)__gc_stack_top;

#ifdef DEBUG_VERSION
  print_stack(s);
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
