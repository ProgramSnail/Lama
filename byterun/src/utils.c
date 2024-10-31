#include "utils.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* Gets a string from a string table by an index */
extern char* get_string(bytefile *f, int pos) {
  return &f->string_ptr[pos];
}

/* Gets a name for a public symbol */
extern char* get_public_name (bytefile *f, int i) {
  return get_string(f, f->public_ptr[i*2]);
}

/* Gets an offset for a publie symbol */
extern int get_public_offset (bytefile *f, int i) {
  return f->public_ptr[i*2+1];
}

// ---
