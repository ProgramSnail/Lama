#include "stack.h"

#include "../../runtime/runtime.h"

extern size_t STACK_SIZE;

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

// ------ basic stack oprs ------

void s_push(struct State *s, void *val) {
  if (s->sp == s->stack) {
    failure("stack overflow");
  }
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
  if (s->sp == s->stack + STACK_SIZE || (s->fp != NULL && s->sp == f_locals(s->fp))) {
    failure("take: no var");
  }
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

void s_enter_f(struct State *s, char *func_ip, auint args_sz,
                             auint locals_sz) {
  // check that params count is valid
  if (s->sp + (aint)args_sz - 1 >= s->stack + STACK_SIZE ||
      (s->fp != NULL && args_sz > s->sp + STACK_SIZE - f_locals(s->fp))) {
    failure("not enough parameters in stack");
  }

  // create frame
  struct Frame frame = {
      .ret = NULL, // field in frame itself
      .rp = s->ip,
      .to_prev_fp_box = BOX((void**)s->fp - s->sp),
      .args_sz_box = BOX(args_sz),
      .locals_sz_box = BOX(locals_sz),
  };

  // put frame on stack
  s_push_nil(s); // sp contains value
  s->fp = (struct Frame *)s->sp;
  s_pushn_nil(s, frame_sz() - 1);
  (*s->fp) = frame;

  s_pushn_nil(s, locals_sz);

  // go to function body
  s->ip = func_ip;
}

void s_exit_f(struct State *s) {
  if (s->fp == NULL) {
    failure("exit: no func");
  }

  struct Frame frame = *s->fp;
  push_extra_root((void **)&frame.ret);

  // drop stack entities, locals, frame
  s_popn(s, (void**)s->fp - s->sp + 1); // TODO:check +1

  // drop args
  s_popn(s, f_args_sz(&frame));

  // save returned value
  s_push(s, frame.ret);

  s->ip = frame.rp;
  s->fp = (struct Frame*)f_prev_fp(&frame);

  pop_extra_root((void **)&frame.ret);
}

void **var_by_category(struct State *s, enum VarCategory category,
                             int id) {
  if (id < 0) {
    failure("can't read variable: negative id %i", id);
  }
  void **var = NULL;
  switch (category) {
  case VAR_GLOBAL:
    // TODO: FIXME
    break;
  case VAR_LOCAL:
    if (s->fp == NULL) {
      failure("can't read local outside of function");
    }
    if (f_args_sz(s->fp) <= id) {
      failure("can't read local: too big id, %i >= %ul", f_locals_sz(s->fp),
              id);
    }
    var = &f_locals(s->fp)[id];
    break;
  case VAR_ARGUMENT:
    if (s->fp == NULL) {
      failure("can't read argument outside of function");
    }
    if (f_args_sz(s->fp) <= id) {
      failure("can't read arguments: too big id, %i >= %ul", f_args_sz(s->fp),
              id);
    }
    var = &f_args(s->fp)[id]; // TODO: check if not reversed order
    break;
  case VAR_C:
    // TODO: ??
    break;
  }

  // TODO: push extra root ??

  return var;
}
