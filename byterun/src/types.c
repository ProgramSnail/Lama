#include "types.h"

#include "module_manager.h"
#include "stack.h"
#include "utils.h"
#include "parser.h"
#include "../../runtime/gc.h"

#include <stdlib.h>

extern size_t __gc_stack_top, __gc_stack_bottom;

// --- State ---

static void init_state(uint mod_id, struct State* s, void** stack) {
  s->stack = stack;
  s->bf = mod_get(mod_id);
  s->is_closure_call = false;
  s->current_module_id = mod_id;
  s->call_module_id = 0; // TODO: ??
  s->ip = s->bf->code_ptr;
  s->instr_ip = s->bf->code_ptr;
  s->call_ip = NULL;
  s->current_line = 0;

  for (size_t i = 0; i < STACK_SIZE; ++i) {
    s->stack[i] = NULL;
  }

  // printf("%p:%zu - %zu", s->stack, (size_t)s->stack, (size_t)s->stack & 0xF);

  // s->sp = s->stack + STACK_SIZE; // [top -> bottom] stack
  s->fp = NULL;
}

void construct_state(uint mod_id, struct State* s, void** stack) {
  __init();
  init_state(mod_id, s, stack);
  __gc_stack_bottom = (size_t)(s->stack + STACK_SIZE);
  __gc_stack_top = __gc_stack_bottom;

  s_pushn_nil(s->bf->global_area_size); // TODO: move to run, do for each module
  s->bf->global_ptr = (void*)__gc_stack_top;

#ifdef DEBUG_VERSION
  print_stack(s);
  printf("- state init done\n");
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
