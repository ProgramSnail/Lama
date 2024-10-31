#include "stack.h"

#include "../../runtime/runtime.h"

extern size_t STACK_SIZE;

void s_push(struct State *s, void *val) {
  if (s->vp == s->stack) {
    failure("stack overflow");
  }
  --s->vp;
  *s->vp = val;
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
  if (s->vp == s->stack + STACK_SIZE || (s->fp != NULL && s->vp == s->fp->end)) {
    failure("take: no var");
  }
  void* value = *s->vp;
  *s->vp = NULL;
  ++s->vp;

  return value;
}

void s_popn(struct State *s, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    s_pop(s);
  }
}

// ------ functions ------

// TODO
// |> param_0 ... param_n | frame[ ret rp prev_fp &params &locals &end ]
// |> local_0 ... local_m |> | ...
//
// where |> defines corresponding frame pointer, | is stack pointer location
// before / after new frame added
void s_enter_f(struct State *s, char *func_ip, size_t params_sz,
                             size_t locals_sz) {
  if (params_sz > s->vp - s->stack ||
      (s->fp != NULL && params_sz > s->vp - s->fp->end)) {
    failure("not enough parameters in stack");
  }
  size_t frame_sz_in_ptr = sizeof(struct Frame) / sizeof(void *);
  struct Frame frame = {
      .ret = NULL, // field in frame itself
      .rp = s->ip,
      .prev_fp = s->fp,
      .params = s->vp - params_sz,
      .locals = s->vp + frame_sz_in_ptr,
      .end = s->vp + frame_sz_in_ptr + locals_sz,
  };

  // put frame on stack
  s->fp = (struct Frame *)s->vp;
  (*s->fp) = frame;

  // update stack pointer
  s->vp = frame.end;

  // go to function body
  s->ip = func_ip;
}

// TODO
void s_exit_f(struct State *s) {
  if (s->fp == NULL) {
    failure("exit: no func");
  }

  // drop stack entities and locals
  s_popn(s, f_locals_sz(s->fp));

  // TODO: skip

  // drop params
  s->vp = (void **)s->fp;
  s_popn(s, f_args_sz(s->fp));

  // s->vp = s->fp->params; // done automatically

  // save ret_val
  s_push(s, s->fp->ret);

  s->ip = s->fp->rp;
  s->fp = s->fp->prev_fp;
}

// TODO
union VarT **var_by_category(struct State *s, enum VarCategory category,
                             int id) {
  if (id < 0) {
    failure("can't read variable: negative id %i", id);
  }
  union VarT **var = NULL;
  switch (category) {
  case VAR_GLOBAL:
    // TODO: FIXME
    break;
  case VAR_LOCAL:
    if (s->fp == NULL) {
      failure("can't read local outside of function");
    }
    if (frame_args_sz(s->fp) <= id) {
      failure("can't read local: too big id, %i >= %ul", f_locals_sz(s->fp),
              id);
    }
    var = (union VarT **)&f_locals_at(s->fp, id);
    break;
  case VAR_ARGUMENT:
    if (s->fp == NULL) {
      failure("can't read argument outside of function");
    }
    if (f_args_sz(s->fp) <= id) {
      failure("can't read arguments: too big id, %i >= %ul", f_args_sz(s->fp),
              id);
    }
    var = (union VarT **)&f_args_at(s->fp, id);
    break;
  case VAR_C:
    // TODO: ??
    break;
  }

  return var;
}
