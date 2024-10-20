#pragma once

#include "../../runtime/runtime.h"
#include "types.h"

#include <cstdlib>

// TODO: use gc

// ------ general ------

// TODO: use gc
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
    free(var.array.values);
    break;
  case SEXP_T:
    // tag is const string, no need to free
    if (var.sexp.next != NULL) {
      free(var.sexp.next);
    }
    break;
  case FUN_T:
    break;
  }
}

// TODO: use gc
inline void free_var_ptr(union VarT *var) {
  free_var(*var);
  free((void *)var);
}

//

inline NilT clear_var() { return NilT{.data_header = NIL_T}; }

// usually not required, because frame is located on shared stack
inline struct Frame clear_frame() {
  struct Frame frame = {
      .ret_ip = NULL,
      .rp = NULL,
      .ret = NULL,
      .params = NULL,
      .locals = NULL,
      .end = NULL,
  };
  return frame;
}

// TODO:  not required ??
// inline struct Var deep_copy_var(struct Var var) {
//   switch (var.type) {
//   case INT_T:
//     break;
//   case CONST_STR_T:
//     break;
//   case STR_T: {
//     char *old_str = var.value.str_v;
//     var.value.str_v = calloc(var.size + 1, sizeof(char));
//     strcpy(var.value.str_v, old_str);
//     break;
//   }
//   case LIST_T:
//     if (var.value.list_v.elem != NULL) {
//       struct Var *old_elem = var.value.list_v.elem;
//       var.value.list_v.elem = calloc(1, sizeof(struct Var));
//       *var.value.list_v.elem = deep_copy_var(*old_elem);
//     }
//     if (var.value.list_v.next != NULL) {
//       struct Var *old_next = var.value.list_v.next;
//       var.value.list_v.next = calloc(1, sizeof(struct Var));
//       *var.value.list_v.next = deep_copy_var(*old_next);
//     }
//     break;
//   case ARRAY_T: {
//     struct Var *old_array = var.value.array_v;
//     var.value.array_v = calloc(var.size, sizeof(char));
//     for (size_t i = 0; i < var.size; ++i) {
//       var.value.array_v[i] = deep_copy_var(*(old_array + i));
//     }
//     break;
//   }
//   case FUN_T:
//     break;
//   case NIL_T:
//     break;
//   }

//   return var;
// }

// ------ put on stack ---

inline void s_put_var(struct State *s, struct NilT *val) { // any var
  *s->vp = val;
  ++s->vp;
}

inline void s_put_nil(struct State *s) {
  struct NilT *var = alloc();
  var->data_header = NIL_T; // no param
  s_put_var(s, var);
}

inline void s_putn_nil(struct State *s, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    s_put_nil(s);
  }
}

inline void s_put_i(struct State *s, int val) {
  struct IntT *var = alloc();
  var->data_header = INT_T; // no param
  var->value = val;
  s_put_var(s, (NilT *)var);
}

inline void s_put_const_str(struct State *s,
                            const char *val) { // memory controlled externally
  struct ConstStrT *var = alloc();
  var->data_header = CONST_STR_T; // no param
  var->value = val;
  s_put_var(s, (NilT *)var);
}

inline void s_put_str(struct State *s, char *val) { // memory controlled by var
  struct StrT *var = alloc();
  var->data_header = STR_T; // no param
  var->value = val;
  s_put_var(s, (NilT *)var);
}

// TODO
inline void s_put_array(struct State *s, int sz) { // memory controlled by var
  struct Var var = {
      .type = ARRAY_T,
      .value.array_v = calloc(sz, sizeof(struct Var)),
      .size = sz,
  };
  s_put_var(s, var);

  // fill array with nils ?
}

inline void s_put_list(struct State *s,
                       struct NilT *first_elem) { // memory controlled by var
  struct ListT *var;
  var->data_header = LIST_T; // no param
  var->value = first_elem;
  var->next = NULL;

  s_put_var(s, (NilT *)var);

  *first_elem = clear_var();
}

// ------ take from stack ------

inline union VarT *s_take_var(struct State *s) {
  if (s->vp == s->fp->end) {
    failure("take: no var");
  }
  --s->vp;

  union VarT *ret = *s->vp;
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
  if (s->vp == s->fp->end) {
    failure("drop: no var");
  }
  --s->vp;
  free_var_ptr(*s->vp);
  *s->vp = NULL;
}

inline void s_dropn_var(struct State *s, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    s_drop_var(s);
  }
}

// ------ functions ------

// TODO
inline void s_exit_f(struct State *s) {
  if (s->fp == (void *)s->stack) {
    failure("exit: no func");
  }
  --s->fp;
  s_dropn_var(s, s->vp - s->fp->locals);
  // drop local var stack and locals // TODO: check +-1
  union VarT *ret = *s->vp;
  --s->vp;
  s_dropn_var(s, s->vp - s->fp->params);
  // drop params // TODO: check +-1
  s_put_var(s, ret);
  s->ip = s->fp->rp;

  // *s->fp = clear_frame(); // clear top frame
}

// TODO
inline void s_enter_f(struct State *s, char *func_ip, size_t params_sz,
                      size_t locals_sz) {
  struct Frame frame = {
      .rp = s->ip, // ??
      .ret = s->vp,
      .params = s->vp - params_sz, // TODO: check +-1
      .locals = s->vp + 1,
      .end = s->vp + locals_sz + 1, // ??
  };

  // TODO:
  s_put_nil(s);             // ret
  s_putn_nil(s, locals_sz); // locals
  s->ip = func_ip;
  (*s->fp) = frame;
  ++s->fp;
}
