#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "../../runtime/runtime.h"
#include "../../runtime/runtime_common.h"

/* The unpacked representation of bytecode file */
typedef struct {
  char *string_ptr;      /* A pointer to the beginning of the string table */
  int *public_ptr;       /* A pointer to the beginning of publics table    */
  char *code_ptr;        /* A pointer to the bytecode itself               */
  int *global_ptr;       /* A pointer to the global area                   */
  int code_size;         /* The size (in bytes) of code                    */
  uint stringtab_size;   /* The size (in bytes) of the string table        */
  uint global_area_size; /* The size (in words) of global area             */
  uint public_symbols_number; /* The number of public symbols */
  char buffer[0];
} bytefile;

static inline void exec_failure(const char *cmd, int line, aint offset,
                                const char *msg) {
  failure("*** RUNTIME ERROR: %i(0x%.8x):%s error: %s\n", line, offset, cmd,
          msg);
}

/* Gets a string from a string table by an index */
static inline const char *get_string(const bytefile *f, size_t pos) {
  if (pos >= f->stringtab_size) {
    failure("strinpg pos is out of range: %zu >= %i\n", pos, f->stringtab_size);
  }
  return &f->string_ptr[pos];
}

/* Gets a name for a public symbol */
static inline const char *get_public_name(const bytefile *f, size_t i) {
  if (i >= f->public_symbols_number) {
    failure("public number is out of range: %zu >= %i\n", i * 2,
            f->public_symbols_number);
  }
  return get_string(f, f->public_ptr[i * 2]);
}

/* Gets an offset for a publie symbol */
static inline size_t get_public_offset(const bytefile *f, size_t i) {
  if (i + 1 >= f->public_symbols_number) {
    failure("public number is out of range: %zu >= %i\n", i * 2 + 1,
            f->public_symbols_number);
  }
  return f->public_ptr[i * 2 + 1];
}
