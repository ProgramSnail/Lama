#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "../../runtime/runtime.h"
#include "../../runtime/runtime_common.h"

/* The unpacked representation of bytecode file */
typedef struct {
  char *string_ptr;     /* A pointer to the beginning of the string table */
  int *public_ptr;      /* A pointer to the beginning of publics table    */
  char *code_ptr;       /* A pointer to the bytecode itself               */
  int *global_ptr;      /* A pointer to the global area                   */
  int stringtab_size;   /* The size (in bytes) of the string table        */
  int global_area_size; /* The size (in words) of global area             */
  int public_symbols_number; /* The number of public symbols */
  char buffer[0];
} bytefile;

/* Gets a string from a string table by an index */
static inline char *get_string(bytefile *f, size_t pos) {
  return &f->string_ptr[pos];
}

/* Gets a name for a public symbol */
static inline char *get_public_name(bytefile *f, size_t i) {
  return get_string(f, f->public_ptr[i * 2]);
}

/* Gets an offset for a publie symbol */
static inline size_t get_public_offset(bytefile *f, size_t i) {
  return f->public_ptr[i * 2 + 1];
}

static inline void exec_failure(const char *cmd, int line, aint offset,
                                const char *msg) {
  failure("*** RUNTIME ERROR: %i(0x%.8x):%s error: %s\n", line, offset, cmd,
          msg);
}

// ---
