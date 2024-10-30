#pragma once

#include "gc.h"
#include "runtime.h"
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
  case BOX_T:
    // pointer, do not free original object
    break;
  case STR_T:
    if (dh_param(var.str.data_header)) { // not const string
      // free(var.str.value); // FIXME
    }
    break;
  case CLOJURE_T:
    // TODO
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
    if (var.sexp.values != NULL) {
      for (size_t i = 0; i < dh_param(var.sexp.data_header); ++i) {
        free_var_ptr(to_var(var.sexp.values[i]));
      }
      // free(var.sexp.values); // FIXME
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

inline struct NilT clear_var() {
  struct NilT var = {.data_header = NIL_T};
  return var;
}

// ------ put on stack ---

inline void s_put_ptr(struct State *s, char *val) { // any var
  *s->vp = (struct NilT *)val;
  ++s->vp;
}

inline void s_put_var_ptr(struct State *s, struct NilT **val) { // any var
  *s->vp = (struct NilT *)val;
  ++s->vp;
}

inline void s_put_var(struct State *s, struct NilT *val) { // any var
  *s->vp = val;
  ++s->vp;
}

inline void s_put_nil(struct State *s) {
  struct NilT *var = (struct NilT *)alloc(sizeof(struct NilT));
  var->data_header = NIL_T; // no param
  s_put_var(s, var);
}

inline void s_putn_nil(struct State *s, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    s_put_nil(s);
  }
}

inline void s_put_i(struct State *s, int val) {
  struct IntT *var = (struct IntT *)alloc(sizeof(struct IntT));
  var->data_header = INT_T; // no param
  var->value = val;
  s_put_var(s, (struct NilT *)var);
}

inline void s_put_box(struct State *s, struct NilT **val) {
  struct BoxT *var = (struct BoxT *)alloc(sizeof(struct BoxT));
  var->data_header = BOX_T; // no param
  var->value = val;
  s_put_var(s, (struct NilT *)var);
}

inline void s_put_const_str(struct State *s, const char *val) {
  struct StrT *var = (struct StrT *)alloc(sizeof(struct StrT));
  var->data_header = 0 & STR_T; // param - is const
  var->value = val;
  s_put_var(s, (struct NilT *)var);
}

inline void s_put_str(struct State *s, char *val) {
  struct StrT *var = (struct StrT *)alloc(sizeof(struct StrT));
  var->data_header = 1 & STR_T; // param - is not const
  var->value = val;
  s_put_var(s, (struct NilT *)var);
}

inline void s_put_array(struct State *s, int sz) {
  struct ArrayT *var = (struct ArrayT *)alloc(sizeof(struct ArrayT));

  if (sz < 0) {
    failure("array size < 0");
  }

  if (sz > MAX_ARRAY_SIZE) {
    failure("too big array size");
  }

  var->data_header = sz & ARRAY_T;
  var->values = (struct NilT **)alloc(sizeof(struct NilT *) * sz);

  for (size_t i = 0; i < sz; ++i) {
    var->values[i] = NULL;
  }
  s_put_var(s, (struct NilT *)var);
}

inline union VarT *s_take_var(struct State *s);
inline void s_put_sexp(struct State *s, const char *tag, int sz) {
  struct SExpT *var = (struct SExpT *)alloc(sizeof(struct SExpT));

  if (sz < 0) {
    failure("array size < 0");
  }

  if (sz > MAX_ARRAY_SIZE) {
    failure("too big array size");
  }

  var->data_header = sz & SEXP_T;
  var->values = (struct NilT **)alloc(sizeof(struct NilT *) * sz);

  var->tag = tag;

  for (size_t i = 0; i < sz; ++i) {
    var->values[i] = (struct NilT *)s_take_var(s);
  }
  s_put_var(s, (struct NilT *)var);
}

// inline void s_put_empty_list(struct State *s, struct NilT *first_elem) {
//   struct ListT *var = (ListT *)alloc(sizeof(ListT));
//   var->data_header = LIST_T; // no param
//   var->value = first_elem;
//   var->next = NULL;

//   s_put_var(s, (struct NilT *)var);

//   *first_elem = clear_var();
// }

// ------ take from stack ------

inline union VarT *s_take_var(struct State *s) {
  if (s->vp == s->stack || (s->fp != NULL && s->vp == s->fp->end)) {
    failure("take: no var");
  }
  --s->vp;

  union VarT *ret = (union VarT *)*s->vp;
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
  free_var_ptr((union VarT *)*s->vp);
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

inline union VarT **var_by_category(struct State *s, enum VarCategory category,
                                    int id) {
  union VarT **var = NULL;
  switch (category) {
  case VAR_GLOBAL:
    // TODO: FIXME
    break;
  case VAR_LOCAL:
    if (s->fp == NULL) {
      failure("can't read local outside of function");
    }
    if (id < 0) {
      failure("can't read local: negative id %i", id);
    }
    if (frame_locals_sz(s->fp) <= id) {
      failure("can't read local: too big id, %i >= %ul", frame_locals_sz(s->fp),
              id);
    }
    var = (union VarT **)&s->fp->locals[id];
    break;
  case VAR_A:
    // TODO
    break;
  case VAR_C:
    // TODO
    break;
  }

  return var;
}
