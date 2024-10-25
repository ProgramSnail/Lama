#pragma once

#include "../../runtime/gc.h"
#include "../../runtime/runtime.h"
#include "types.h"

#include "stdlib.h"

// ------ general ------

inline void free_var_ptr(union VarT *var);

inline void free_var(union VarT var) {
  switch (dh_type(var.nil.data_header)) {
  case NIL_T:
    break;
  case INT_T:
    break;
  case CONST_STR_T:
    break;
  case STR_T:
    free(var.str.value);
    break;
  case LIST_T:
    if (var.list.value != NULL) {
      free_var_ptr(to_var(var.list.value));
    }
    if (var.list.next != NULL) {
      free_var_ptr(to_var(var.list.next));
    }
    break;
  case ARRAY_T:
    // dh param is size
    for (size_t i = 0; i < dh_param(var.array.data_header); ++i) {
      free_var_ptr(to_var(var.array.values[i]));
    }
    // free(var.array.values); // FIXME
    break;
  case SEXP_T:
    // tag is const string, no need to free
    if (var.sexp.next != NULL) {
      // free(var.sexp.next); // FIXME
    }
    break;
  case FUN_T:
    break;
  }
}

// TODO: use gc
inline void free_var_ptr(union VarT *var) {
  free_var(*var);
  // free((void *)var); // FIXME
}

//

inline struct NilT clear_var() { return NilT{.data_header = NIL_T}; }

// ------ put on stack ---

inline void s_put_ptr(struct State *s, char *val) { // any var
  *s->vp = (NilT *)val;
  ++s->vp;
}

inline void s_put_var_ptr(struct State *s, struct NilT **val) { // any var
  *s->vp = (NilT *)val;
  ++s->vp;
}

inline void s_put_var(struct State *s, struct NilT *val) { // any var
  *s->vp = val;
  ++s->vp;
}

inline void s_put_nil(struct State *s) {
  struct NilT *var = (NilT *)alloc(sizeof(NilT));
  var->data_header = NIL_T; // no param
  s_put_var(s, var);
}

inline void s_putn_nil(struct State *s, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    s_put_nil(s);
  }
}

inline void s_put_i(struct State *s, int val) {
  struct IntT *var = (IntT *)alloc(sizeof(IntT));
  var->data_header = INT_T; // no param
  var->value = val;
  s_put_var(s, (NilT *)var);
}

inline void s_put_const_str(struct State *s, const char *val) {
  struct ConstStrT *var = (ConstStrT *)alloc(sizeof(ConstStrT));
  var->data_header = CONST_STR_T; // no param
  var->value = val;
  s_put_var(s, (NilT *)var);
}

inline void s_put_str(struct State *s, char *val) {
  struct StrT *var = (StrT *)alloc(sizeof(StrT));
  var->data_header = STR_T; // no param
  var->value = val;
  s_put_var(s, (NilT *)var);
}

inline void s_put_enum(struct State *s, const char *tag, int args_sz) {
  // TODO FIXME
}

inline void s_put_array(struct State *s, int sz) {
  struct ArrayT *var = (ArrayT *)alloc(sizeof(ArrayT));

  if (sz < 0) {
    failure("array size < 0");
  }

  if (sz > MAX_ARRAY_SIZE) {
    failure("too big array size");
  }

  var->data_header = sz & ARRAY_T;
  var->values = (NilT **)alloc(sizeof(NilT *) * sz);

  for (size_t i = 0; i < sz; ++i) {
    var->values[i] = NULL;
  }
  s_put_var(s, (NilT *)var);
}

inline void s_put_list(struct State *s, struct NilT *first_elem) {
  struct ListT *var = (ListT *)alloc(sizeof(ListT));
  var->data_header = LIST_T; // no param
  var->value = first_elem;
  var->next = NULL;

  s_put_var(s, (NilT *)var);

  *first_elem = clear_var();
}

// ------ take from stack ------

inline union VarT *s_take_var(struct State *s) {
  if (s->vp == s->stack || (s->fp != NULL && s->vp == s->fp->end)) {
    failure("take: no var");
  }
  --s->vp;

  union VarT *ret = (VarT *)*s->vp;
  *s->vp = NULL; // clear top var
  return ret;
}

inline int s_take_i(struct State *s) {
  union VarT *v = s_take_var(s);
  if (dh_type(v->nil.data_header) != INT_T) {
    failure("take int: not int");
  }
  return v->int_t.value;
}

inline void s_drop_var(struct State *s) {
  if (s->vp == s->stack || (s->fp != NULL && s->vp == s->fp->end)) {
    failure("drop: no var");
  }
  --s->vp;
  free_var_ptr((VarT *)*s->vp);
  *s->vp = NULL;
}

inline void s_dropn_var(struct State *s, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    s_drop_var(s);
  }
}

// ------ functions ------

// |> param_0 ... param_n | frame[ ret rp prev_fp &params &locals &end ]
// |> local_0 ... local_m |> | ...
//
// where |> defines corresponding frame pointer, | is stack pointer location
// before / after new frame added
inline void s_enter_f(struct State *s, char *func_ip, size_t params_sz,
                      size_t locals_sz) {
  if (params_sz > s->vp - s->stack or
      (s->fp != NULL and params_sz > s->vp - s->fp->end)) {
    failure("not enough parameters in stack");
  }
  size_t frame_sz_in_ptr = sizeof(Frame) / sizeof(void *);
  struct Frame frame = {
      .ret = NULL, // field in frame itself
      .rp = s->ip,
      .prev_fp = s->fp,
      .params = s->vp - params_sz,
      .locals = s->vp + frame_sz_in_ptr,
      .end = s->vp + frame_sz_in_ptr + locals_sz,
  };

  // put frame on stack
  s->fp = (Frame *)s->vp;
  (*s->fp) = frame;

  // update stack pointer
  s->vp = frame.end;

  // go to function body
  s->ip = func_ip;
}

inline void s_exit_f(struct State *s) {
  if (s->fp == NULL) {
    failure("exit: no func");
  }

  // drop stack entities and locals
  s_dropn_var(s, s->vp - s->fp->locals);

  // drop params
  s->vp = (void **)s->fp;
  s_dropn_var(s, s->vp - s->fp->params);

  // s->vp = s->fp->params; // done automatically

  // save ret_val;
  if (s->fp->ret != NULL) {
    (*s->vp) = s->fp->ret;
    ++s->vp;
  }

  s->ip = s->fp->rp;
  s->fp = s->fp->prev_fp;
}

inline union VarT *var_by_category(struct State *s, enum VarCategory category,
                                   int id) {
  // TODO: FIXME
}
