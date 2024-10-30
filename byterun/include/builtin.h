#pragma once

#include <stdio.h>

#include "operations.h"

inline void f_read(struct State *s) {
  int x = 0;
  printf("> ");
  scanf("%i", &x);
  s_put_i(s, x);
}

inline void f_write(struct State *s) {
  int x = s_take_i(s);
  printf("%i", x);
}

inline void f_length(struct State *s) {
  union VarT *x = s_take_var(s);
  uint32_t type = dh_type(x->nil.data_header);

  if (type == ARRAY_T || type == STR_T) {
    s_put_i(s, dh_param(x->array.data_header));
  } else if (type == STR_T) {
    s_put_i(s, strlen(x->str.value));
  } else { // TODO: lists ??
    failure("no length func for type %ui", type);
  }
}

// TODO
inline size_t str_sz(union VarT *var) {
  switch (dh_type(var->nil.data_header)) {
  case NIL_T: // <nil>
    return strlen("<nil>");
  case INT_T: // int
    return snprintf(nullptr, 0, "%d", var->int_t.value);
  case BOX_T: // "<box>:..."
    return strlen("<box>") + (var->box.value != NULL
                                  ? str_sz((union VarT *)&var->box.value) + 1
                                  : 0);
  case STR_T: // "str"
    return strlen(var->str.value);
  case CLOJURE_T: // <clojure> // TODO
    return strlen("<clojure>");
    break;
  case ARRAY_T: { // [a_1 a_2 a_3 ... a_n]
    size_t sz = 0;
    if (var->array.values != NULL) {
      for (size_t i = 0; i < dh_param(var->array.data_header); ++i) {
        sz += str_sz((union VarT *)var->array.values[i]) + 1;
      }
      --sz; // extra space
    }
    return sz + 2; // '[', ']'
  }
  case SEXP_T: { // tag:{a_1 a_2 ...}
    size_t sz = 0;
    if (var->sexp.tag != NULL) {
      sz += strlen(var->sexp.tag) + 1; // tag and ':'
    }
    if (var->sexp.values != NULL) {
      for (size_t i = 0; i < dh_param(var->sexp.data_header); ++i) {
        sz += str_sz((union VarT *)var->sexp.values[i]) + 1;
      }
      --sz; // extra space
    }
    return sz + 2; // '{', '}'
  }
  case FUN_T: // <fun>
    return strlen("<fun>");
  }
}

// TODO
inline char *to_str(union VarT *var, char *str, size_t max_sz) {
  str[0] = 0;
  switch (dh_type(var->nil.data_header)) {
  case NIL_T:
    strcat(str, "<nil>");
    break;
  case INT_T:
    snprintf(str, max_sz, "%d", var->int_t.value);
    break;
  case BOX_T:
    strcat(str, "<box>");
    if (var->box.value != NULL) {
      strcat(str, ":");
      str += strlen(str);
      str = to_str((union VarT *)&var->box.value, str, max_sz);
    }
    break;
  case STR_T:
    strcat(str, "\"");
    strcat(str, var->str.value);
    strcat(str, "\"");
    break;
  case CLOJURE_T: // TODO
    strcat(str, "<clojure>");
    break;
  case ARRAY_T:
    strcat(str, "[");
    ++str;
    for (size_t i = 0; i < dh_param(var->array.data_header); ++i) {
      str = to_str((union VarT *)var->array.values[i], str, max_sz);
      strcat(str, " ");
      ++str;
    }
    strcat(str, "]");
    break;
  case SEXP_T:
    if (var->sexp.tag != NULL) {
      strcat(str, var->sexp.tag);
      strcat(str, ":");
    }
    strcat(str, "{");
    str += strlen(str);
    for (size_t i = 0; i < dh_param(var->sexp.data_header); ++i) {
      str = to_str((union VarT *)var->sexp.values[i], str, max_sz);
      strcat(str, " ");
      ++str;
    }
    strcat(str, "}");
    break;
  case FUN_T:
    strcat(str, "<fun>");
    break;
  }

  return str + strlen(str);
}

inline void f_string(struct State *s) {
  union VarT *var = s_take_var(s);

  size_t var_str_sz = str_sz(var);
  char *var_str = (char *)malloc((var_str_sz + 1) * sizeof(char));
  to_str(var, var_str, var_str_sz);

  s_put_str(s, var_str);

  free_var_ptr(var);
}

inline void f_array(struct State *s, int sz) { s_put_array(s, sz); }

inline void f_binop(struct State *s, const char *opr) {
  size_t len = strlen(opr);

  int y = s_take_i(s);
  int x = s_take_i(s);

  int z = 0;

  if (len < 1) {
    failure("BINOP: empty operation");
  }
  switch (opr[0]) {
  case '+':
    z = x + y;
    break;
  case '-':
    z = x - y;
    break;
  case '*':
    z = x * y;
    break;
  case '/':
    if (y == 0) {
      failure("BINOP: can't divide by zero");
    }
    z = x / y;
    break;
  case '%':
    if (y == 0) {
      failure("BINOP: can't take by mod zero");
    }
    z = x % y;
    break;
  case '<':
    if (len == 1) { // <
      z = x < y;
    } else { // <=
      z = x <= y;
    }
    break;
  case '>':
    if (len == 1) { // >
      z = x > y;
    } else { // >=
      z = x >= y;
    }
    break;
  case '=': // ==
    z = x == y;
    break;
  case '!':
    if (len == 1) {
      failure("BINOP: '!...' opr len is 1");
    }
    if (opr[1] == '=') { // !=
      z = x != y;
    } else { // !!
      z = x || y;
    }
    break;
  case '&': // &&
    z = x && y;
    break;
  default:
    failure("BINOP: unknown operation");
  }

  s_put_i(s, z);
}
