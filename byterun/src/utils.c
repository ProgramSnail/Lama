#include "utils.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../runtime/runtime.h"

/* Gets a string from a string table by an index */
extern char* get_string(bytefile *f, size_t pos) {
  return &f->string_ptr[pos];
}

/* Gets a name for a public symbol */
extern char* get_public_name (bytefile *f, size_t i) {
  return get_string(f, f->public_ptr[i*2]);
}

/* Gets an offset for a publie symbol */
extern size_t get_public_offset (bytefile *f, size_t i) {
  return f->public_ptr[i*2+1];
}

void exec_failure(const char *cmd, int line, aint offset, const char *msg) {
  failure("*** RUNTIME ERROR: %i(0x%.8x):%s error: %s\n", line, offset, cmd, msg);
}

// ---
