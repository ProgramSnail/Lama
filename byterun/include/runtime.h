#pragma once

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#define WORD_SIZE (CHAR_BIT * sizeof(int))

inline void vfailure(char *s, va_list args) {
  fprintf(stderr, "*** FAILURE: ");
  vfprintf(stderr, s,
           args); // vprintf (char *, va_list) <-> printf (char *, ...)
  exit(255);
}

inline void failure(char *s, ...) {
  va_list args;

  va_start(args, s);
  vfailure(s, args);
}
