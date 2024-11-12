#include "types.h"

#include "stack.h"
#include "utils.h"
#include "parser.h"
#include "../../runtime/gc.h"

#include <stdlib.h>

extern size_t __gc_stack_top, __gc_stack_bottom;

// --- State ---

static void init_state(bytefile *bf, struct State* s, void** stack) {
  s->stack = stack;
  s->bf = bf;
  s->is_closure_call = false;
  s->ip = bf->code_ptr;
  s->instr_ip = bf->code_ptr;
  s->call_ip = NULL;
  s->current_line = 0;

  for (size_t i = 0; i < STACK_SIZE; ++i) {
    s->stack[i] = NULL;
  }

  // printf("%p:%zu - %zu", s->stack, (size_t)s->stack, (size_t)s->stack & 0xF);

  s->sp = s->stack + STACK_SIZE; // [top -> bottom] stack
  s->fp = NULL;
}

void construct_state(bytefile *bf, struct State* s, void** stack) {
  __init();
  init_state(bf, s, stack);
  __gc_stack_bottom = (size_t)s->sp;
  __gc_stack_top = __gc_stack_bottom;

  s_pushn_nil(bf->global_area_size);

#ifdef DEBUG_VERSION
  print_stack(s);
  printf("- state init done\n");
#endif
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
