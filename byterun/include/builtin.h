#pragma once

#include <stdio.h>

#include "operations.h"

inline void f_read(struct State *s) {
  int x = 0;
  printf("> "); // TODO: ??
  scanf("%i", &x);
  s_put_i(s, x);
}

inline void f_write(struct State *s) {
  int x = s_take_i(s);
  printf("%i", x);
  // put 0 ??
}

inline void f_length(struct State *s) {
  // TODO
}

inline void f_string(struct State *s) {
  // TODO
}

inline void f_array(struct State *s, int sz) {
  // TODO
}

inline void f_cons(struct State *s) {

  // TODO
}

// TODO
inline void f_binop(struct State *s, const char *opr) {
  size_t len = strlen(opr);

  if (len < 1) {
    failure("empty operation");
  }
  switch (opr[0]) {
  case '+':
    break;
  case '-':
    break;
  case '*':
    break;
  case '/':
    break;
  case '%':
    break;
  case '<':
    if (len == 1) { // <

    } else { // <=
    }
    break;
  case '>':
    if (len == 1) { // >

    } else { // >=
    }
    break;
  case '=': // ==
    break;
  case '!':
    if (len == 1) {
      failure("'!...' opr len is 1");
    }
    if (opr[1] == '=') { // !=

    } else { // !!
    }
    break;
  case '&': // &&
    break;
  default:
    failure("unknown operation");
  }
}
