#pragma once

#include "../../runtime/gc.h"
#include "module_manager.h"
#include "runtime_externs.h"
#include "types.h"
#include "utils.h"

#include "stdlib.h"

extern struct State s;

extern size_t __gc_stack_top, __gc_stack_bottom;

static inline void **s_top() {
  return (void **)__gc_stack_bottom - s.bf->global_area_size;
}

static inline bool s_is_empty() {
  if ((void **)__gc_stack_top == s_top() ||
      (s.fp != NULL && (void **)__gc_stack_top == f_locals(s.fp))) {
    return true;
  }
  return false;
}

static inline void **s_nth(size_t n) {
#ifndef WITH_CHECK
  if ((void **)__gc_stack_top + n >= s_top()) {
    s_failure(&s, "not enough elements in stack");
  }
  if (s.fp != NULL && (void **)__gc_stack_top + n >= f_locals(s.fp)) {
    s_failure(&s, "not enough elements in function stack");
  }
#endif

  return (void **)__gc_stack_top + n;
}

static inline void **s_peek() {
#ifndef WITH_CHECK
  if ((void **)__gc_stack_top == s_top()) {
    s_failure(&s, "empty stack");
  }
  if (s.fp != NULL && (void **)__gc_stack_top == f_locals(s.fp)) {
    s_failure(&s, "empty function stack");
  }
#endif

  return (void **)__gc_stack_top;
}

static inline aint *s_peek_i() { return (aint *)s_peek(); }

static inline void s_push(void *val) {
#ifndef WITH_CHECK
  if ((void **)__gc_stack_top == s.stack) {
    s_failure(&s, "stack overflow");
  }
#endif
#ifdef DEBUG_VERSION
  printf("--> push\n");
#endif
  __gc_stack_top -= sizeof(void *);
  *(void **)__gc_stack_top = val;
}

static inline void s_push_i(aint val) { s_push((void *)val); }

static inline void s_push_nil() { s_push(NULL); }

static inline void s_pushn_nil(size_t n) {
#ifndef WITH_CHECK
  if ((void **)__gc_stack_top + (aint)n - 1 <= s.stack) {
    s_failure(&s, "stack overflow");
  }
#endif
  for (size_t i = 0; i < n; ++i) {
    __gc_stack_top -= sizeof(void *);
    *(void **)__gc_stack_top = NULL;
  }
}

static inline void *s_pop() {
#ifndef WITH_CHECK
  if ((void **)__gc_stack_top == s_top()) {
    s_failure(&s, "empty stack");
  }
  if (s.fp != NULL && (void **)__gc_stack_top == f_locals(s.fp)) {
    s_failure(&s, "empty function stack");
  }
#endif
#ifdef DEBUG_VERSION
  printf("--> pop\n");
#endif
  void *value = *(void **)__gc_stack_top;
  __gc_stack_top += sizeof(void *);
  return value;
}

static inline aint s_pop_i() { return (aint)s_pop(); }

static inline void s_popn(size_t n) {
  if ((void **)__gc_stack_top + (aint)n - 1 >= s_top()) {
    s_failure(&s, "empty stack");
  }
  __gc_stack_top += n * sizeof(void *);
}

// ------ complex operations ------

// for some reason does not work in sexp constructor, probably connected with gc
// behaviour
static inline void s_put_nth(size_t n, void *val) {
  s_push_nil();
#ifndef WITH_CHECK
  if ((void **)__gc_stack_top + n >= s_top()) {
    s_failure(&s, "not enough elements in stack");
  }
  if (s.fp != NULL && (void **)__gc_stack_top + n >= f_locals(s.fp)) {
    s_failure(&s, "not enough elements in function stack");
  }
#endif

  for (size_t i = 0; i < n; ++i) {
    ((void **)__gc_stack_top)[i] = ((void **)__gc_stack_top)[i + 1];
  }
  ((void **)__gc_stack_top)[n] = val;
}

static inline void s_rotate_n(size_t n) {
  s_push_nil();
#ifndef WITH_CHECK
  if ((void **)__gc_stack_top + (aint)n - 1 >= s_top()) {
    s_failure(&s, "not enough elements in stack");
  }
  if (s.fp != NULL && (void **)__gc_stack_top + (aint)n - 1 >= f_locals(s.fp)) {
    s_failure(&s, "not enough elements in function stack");
  }
#endif

  void *buf = NULL;
  for (size_t i = 0; 2 * i < n; ++i) {
    buf = ((void **)__gc_stack_top)[n - i];
    ((void **)__gc_stack_top)[n - i] = ((void **)__gc_stack_top)[i];
    ((void **)__gc_stack_top)[i] = buf;
  }
}

// ------ functions ------

// |> param_0 ... param_n | frame[ ret rp prev_fp ... &params &locals &end
// ]
// |> local_0 ... local_m |> | ...
//
// where |> defines corresponding frame pointer, | is stack pointer
// location before / after new frame added
static inline void s_enter_f(char *rp, aint ret_module_id, bool is_closure_call,
                             auint args_sz, auint locals_sz) {
#ifdef DEBUG_VERSION
  printf("-> %i args sz\n", args_sz);
  printf("-> %i locals sz\n", locals_sz);
#endif

  // check that params count is valid
  if ((void **)__gc_stack_top + (aint)args_sz - (is_closure_call ? 0 : 1) >=
      s_top()) {
    s_failure(&s, "not enough parameters in stack");
  }
  if (s.fp != NULL &&
      (void **)__gc_stack_top + (aint)args_sz - (is_closure_call ? 0 : 1) >=
          f_locals(s.fp)) {
    s_failure(&s, "not enough parameters in function stack");
  }

  void *closure = is_closure_call ? *s_nth(args_sz) : NULL;
  // printf("is_closure_call: %i, closure: %li\n", is_closure_call,
  // (aint)closure);

  // s_push_nil(s); // sp contains value, frame starts with next value
  s_pushn_nil(frame_sz());

  // create frame
  struct Frame frame = {
      .closure = closure,
      .ret = NULL, // field in frame itself
      .rp = rp,
      .prev_fp = (void **)s.fp,
      .ret_module_box = BOX(ret_module_id),
      .args_sz_box = BOX(args_sz),
      .locals_sz_box = BOX(locals_sz),
  };

  // put frame on stack
  s.fp = (struct Frame *)__gc_stack_top;
  (*s.fp) = frame;

  s_pushn_nil(locals_sz);
}

static inline void s_exit_f() {
#ifndef WITH_CHECK
  if (s.fp == NULL) {
    s_failure(&s, "exit: no func");
  }
#endif

  struct Frame frame = *s.fp;

  // drop stack entities, locals, frame
  size_t to_pop = f_args(s.fp) - (void **)__gc_stack_top;
  s.fp = (struct Frame *)f_prev_fp(&frame);
#ifdef DEBUG_VERSION
  printf("-> %zu to pop\n", to_pop);
#endif
  s_popn(to_pop);

  // drop args
#ifdef DEBUG_VERSION
  printf("-> + %zu to pop\n", f_args_sz(&frame));
#endif
  s_popn(f_args_sz(&frame));

  // save returned value, not in main
  if (frame.prev_fp != 0) {
    s_push(frame.ret);
  }

  s.ip = frame.rp;

  s.current_module_id = UNBOX(frame.ret_module_box);
  s.bf = mod_get(s.current_module_id);
}

static inline void print_stack(struct State *state) {
  printf("stack (%li) is\n[",
         state->stack + STACK_SIZE - (void **)__gc_stack_top);
  for (void **x = state->stack + STACK_SIZE - 1; x >= (void **)__gc_stack_top;
       --x) {
    printf("%li ", (long)UNBOX(*x));
  }
  printf("]\n");
}

// --- category ---

static inline void **var_by_category(enum VarCategory category, size_t id) {
  void **var = NULL;
  switch (category) {
  case VAR_GLOBAL:
#ifndef WITH_CHECK
    if (s.bf->global_area_size <= id) {
      s_failure(&s,
                "can't read global: too big id"); //, %i >= %ul", id,
                                                  // s.bf->global_area_size);
    }
#endif
    var = s.stack + STACK_SIZE - 1 - id;
    break;
  case VAR_LOCAL:
#ifndef WITH_CHECK
    if (s.fp == NULL) {
      s_failure(&s, "can't read local outside of function");
    }
    if (f_locals_sz(s.fp) <= id) {
      s_failure(&s, "can't read local: too big id"); //, %i >= %ul", id,
                                                     // f_locals_sz(s.fp));
    }
#endif
    // printf("id is %i, local is %i, %i\n", id,
    // UNBOX((auint)*((void**)f_locals(s.fp) + id)), f_locals(s.fp) - (void
    // **)__gc_stack_top);
    var = f_locals(s.fp) + (f_locals_sz(s.fp) - id - 1);
    break;
  case VAR_ARGUMENT:
#ifndef WITH_CHECK
    if (s.fp == NULL) {
      s_failure(&s, "can't read argument outside of function");
    }
    if (f_args_sz(s.fp) <= id) {
      s_failure(&s, "can't read arguments: too big id"); //, %i >= %ul", id,
                                                         // f_args_sz(s.fp));
    }
#endif
    var = f_args(s.fp) + (f_args_sz(s.fp) - id - 1);
    break;
  case VAR_CLOSURE:
#ifndef WITH_CHECK
    if (s.fp == NULL) {
      s_failure(&s, "can't read closure parameter outside of function");
    }
    if (s.fp->closure == NULL) {
      s_failure(&s, "can't read closure parameter not in closure");
    }
#endif
    if (UNBOXED(s.fp->closure)) {
      s_failure(&s, "not boxed value expected in closure index");
    }
    data *d = TO_DATA(s.fp->closure);
    int count = get_len(d) - 1;
#ifdef DEBUG_VERSION
    printf("id is %i, count is %i\n", id, count);
#endif
    if ((int64_t)id >= count) {
      s_failure(&s,
                "can't read arguments: too big id"); //, %i >= %ul", id, count);
    }
    return ((void **)d->contents) + count - id; // order is not important
  }

  return var;
}
