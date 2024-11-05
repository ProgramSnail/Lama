#include "stack.h"

#include "../../runtime/runtime.h"

extern size_t __gc_stack_top, __gc_stack_bottom;

#define PRE_GC()                                                                                   \
  bool flag = false;                                                                               \
  flag      = __gc_stack_top == 0;                                                                 \
  if (flag) { __gc_stack_top = (size_t)__builtin_frame_address(0); }                               \
  assert(__gc_stack_top != 0);                                                                     \
  assert((__gc_stack_top & 0xF) == 0);                                                             \
  assert(__builtin_frame_address(0) <= (void *)__gc_stack_top);

#define POST_GC()                                                                                  \
  assert(__builtin_frame_address(0) <= (void *)__gc_stack_top);                                    \
  if (flag) { __gc_stack_top = 0; }

// ------

#define ASSERT_BOXED(memo, x)                                                                      \
  do                                                                                               \
    if (UNBOXED(x)) failure("boxed value expected in %s\n", memo);                                 \
  while (0)
#define ASSERT_UNBOXED(memo, x)                                                                    \
  do                                                                                               \
    if (!UNBOXED(x)) failure("unboxed value expected in %s\n", memo);                              \
  while (0)
#define ASSERT_STRING(memo, x)                                                                     \
  do                                                                                               \
    if (!UNBOXED(x) && TAG(TO_DATA(x)->data_header) != STRING_TAG)                                 \
      failure("string value expected in %s\n", memo);                                              \
  while (0)

// ------ basic stack oprs ------

void** s_top(struct State* s) {
  return s->stack + STACK_SIZE - s->bf->global_area_size;
}

bool s_is_empty(struct State* s) {
  if (s->sp == s_top(s) || (s->fp != NULL && s->sp == f_locals(s->fp))) {
    return true;
  }
  return false;
}

void** s_peek(struct State* s) {
  if (s->sp == s_top(s)) {
    failure("empty stack");
  }
  if (s->fp != NULL && s->sp == f_locals(s->fp)) {
    failure("empty function stack");
  }

  return s->sp;
}

aint* s_peek_i(struct State* s) {
  return (aint*)s_peek(s);
}

void s_push(struct State *s, void *val) {
  if (s->sp == s->stack) {
    failure("stack overflow");
  }
  printf("--> push\n");
  --s->sp;
  *s->sp = val;
}

void s_push_i(struct State *s, aint val) {
  s_push(s, (void*)val);
}

void s_push_nil(struct State *s) {
  s_push(s, NULL);
}

void s_pushn_nil(struct State *s, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    s_push(s, NULL);
  }
}

void* s_pop(struct State *s) {
  if (s->sp == s_top(s)) {
    failure("empty stack");
  }
  if (s->fp != NULL && s->sp == f_locals(s->fp)) {
    failure("empty function stack");
  }
  printf("--> pop\n");
  void* value = *s->sp;
  *s->sp = NULL;
  ++s->sp;

  return value;
}

aint s_pop_i(struct State *s) {
  return (aint)s_pop(s);
}

void s_popn(struct State *s, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    s_pop(s);
  }
}

// ------ functions ------

void s_enter_f(struct State *s, char *rp, bool is_closure_call, auint args_sz, auint locals_sz) {
  printf("-> %i args sz\n", args_sz);
  printf("-> %i locals sz\n", locals_sz);

  // check that params count is valid
  if (s->sp + (aint)args_sz - 1 >= s_top(s)) {
    failure("not enough parameters in stack");
  }
  if (s->fp != NULL && s->sp + (aint)args_sz - 1 >= f_locals(s->fp)) {
    failure("not enough parameters in function stack");
  }

  if (!is_closure_call) {
    s_push_nil(s);
  }

  void* closure = s_peek(s);

  // s_push_nil(s); // sp contains value, frame starts with next value
  s_pushn_nil(s, frame_sz() - 1);

  // create frame
  struct Frame frame = {
       .closure = closure,
      .ret = NULL, // field in frame itself
      .rp = rp,
      .prev_fp = (void**)s->fp,
      .args_sz_box = BOX(args_sz),
      .locals_sz_box = BOX(locals_sz),
  };

  // put frame on stack
  s->fp = (struct Frame *)s->sp;
  (*s->fp) = frame;

  s_pushn_nil(s, locals_sz);
}

void s_exit_f(struct State *s) {
  if (s->fp == NULL) {
    failure("exit: no func");
  }

  struct Frame frame = *s->fp;
  push_extra_root((void **)&frame.ret);

  // drop stack entities, locals, frame
  size_t to_pop = f_args(s->fp) - s->sp;
  s->fp = (struct Frame*)f_prev_fp(&frame);
  printf("-> %zu to pop\n", to_pop);
  s_popn(s, to_pop);

  // drop args
  printf("-> + %zu to pop\n", f_args_sz(&frame));
  s_popn(s, f_args_sz(&frame));

  // save returned value, not in main
  if (frame.prev_fp != 0) {
    s_push(s, frame.ret);
  }

  s->ip = frame.rp;

  pop_extra_root((void **)&frame.ret);
}

void print_stack(struct State* s) {
  printf("stack (%i) is\n[", s->stack + STACK_SIZE - s->sp);
  for (void** x = s->stack + STACK_SIZE - 1; x >= s->sp; --x) {
    printf("%li ", (long)UNBOX(*x));
  }
  printf("]\n");
}

// --- category ---

void **var_by_category(struct State *s, enum VarCategory category,
                             int id) {
  if (id < 0) {
    failure("can't read variable: negative id %i", id);
  }
  void **var = NULL;
  switch (category) {
  case VAR_GLOBAL:
    if (s->bf->global_area_size <= id) {
      failure("can't read global: too big id, %i >= %ul", id, s->bf->global_area_size);
    }
    var = s->stack + STACK_SIZE - 1 - id;
    break;
  case VAR_LOCAL:
    if (s->fp == NULL) {
      failure("can't read local outside of function");
    }
    if (f_locals_sz(s->fp) <= id) {
      failure("can't read local: too big id, %i >= %ul", id, f_locals_sz(s->fp));
    }
    // printf("id is %i, local is %i, %i\n", id, UNBOX((auint)*((void**)f_locals(s->fp) + id)), f_locals(s->fp) - s->sp);
    var = f_locals(s->fp) + (f_locals_sz(s->fp) - id - 1);
    break;
  case VAR_ARGUMENT:
    if (s->fp == NULL) {
      failure("can't read argument outside of function");
    }
    if (f_args_sz(s->fp) <= id) {
      failure("can't read arguments: too big id, %i >= %ul", id, f_args_sz(s->fp));
    }
    var = f_args(s->fp) + (f_args_sz(s->fp) - id - 1);
    break;
  case VAR_CLOSURE:
    if (s->fp == NULL) {
      failure("can't read closure parameter outside of function");
    }
    if (s->fp->closure == NULL) {
      failure("can't read closure parameter not in closure");
    }
    if (UNBOXED(s->fp->closure)) { ASSERT_BOXED(".elem:1", s->fp->closure); }
    data* d =  TO_DATA(s->fp->closure);
    size_t count = get_len(d) - 1;
    printf("id is %i, count is %i\n", id, count);
    if (count <= id) {
      failure("can't read arguments: too big id, %i >= %ul", id, count);
    }
    // TODO: check if not reversed order
    return (void **)d->contents + id;
    // &Belem(s->fp->closure, BOX(id + 1));
    break;
    break;
  }

  return var;
}
