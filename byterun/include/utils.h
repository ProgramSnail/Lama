#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "../../runtime/runtime.h"
#include "../../runtime/runtime_common.h"

/* The unpacked representation of bytecode file */
typedef struct {
  uint main_offset;  /* offset of the function 'main'                        */
  char *string_ptr;  /* A pointer to the beginning of the string table */
  int *imports_ptr;  /* A pointer to the beginning of imports table    */
  int *public_ptr;   /* A pointer to the beginning of publics table    */
  char *code_ptr;    /* A pointer to the bytecode itself               */
  void **global_ptr; /* A pointer to the global area                   */
  char *substs_ptr;  /* A pointer to the substs area                   */
  int code_size;     /* The size (in bytes) of code                    */
  uint stringtab_size;   /* The size (in bytes) of the string table        */
  uint global_area_size; /* The size (in words) of global area             */
  uint substs_area_size; /* number of required address substitutions          */
  uint imports_number;   /* The number of imports                          */
  uint public_symbols_number; /* The number of public symbols              */
  char buffer[0];
} Bytefile;

static inline void exec_failure(const char *cmd, int line, aint offset,
                                const char *msg) {
  failure("*** RUNTIME ERROR: %i(0x%.8x):%s error: %s\n", line, offset, cmd,
          msg);
}

// --- unsafe versions

// access data

/* Gets a string from a string table by an index */
static inline const char *get_string_unsafe(const Bytefile *bf, size_t pos) {
  return &bf->string_ptr[pos];
}

/* Gets import */
static inline const char *get_import_unsafe(const Bytefile *f, size_t i) {
  if (i >= f->imports_number) {
    failure("import pos is out of range: %zu >= %i\n", i, f->imports_number);
  }
  return get_string_unsafe(f, f->imports_ptr[i]);
}

/* Gets a name offset for a public symbol */
static inline size_t get_public_name_offset_unsafe(const Bytefile *bf,
                                                   size_t i) {
  return bf->public_ptr[i * 2];
}

/* Gets a name for a public symbol */
static inline const char *get_public_name_unsafe(const Bytefile *bf, size_t i) {
  return get_string_unsafe(bf, get_public_name_offset_unsafe(bf, i));
}

/* Gets an offset for a publie symbol */
static inline size_t get_public_offset_unsafe(const Bytefile *bf, size_t i) {
  return bf->public_ptr[i * 2 + 1];
}

// read from ip

static inline void ip_write_int_unsafe(char *ip, int32_t x) {
  *((int32_t *)ip) = x;
}

static inline uint16_t ip_read_half_int_unsafe(char **ip) {
  *ip += sizeof(uint16_t);
  return *(uint16_t *)((*ip) - sizeof(uint16_t));
}

static inline int32_t ip_read_int_unsafe(char **ip) {
  *ip += sizeof(int32_t);
  return *(int32_t *)((*ip) - sizeof(int32_t));
}

static inline uint8_t ip_read_byte_unsafe(char **ip) { return *(*ip)++; }

static inline const char *ip_read_string_unsafe(char **ip, const Bytefile *bf) {
  return get_string_unsafe(bf, ip_read_int_unsafe(ip));
}
// --- safe versions

// access data

/* Gets a string from a string table by an index */
static inline const char *get_string_safe(const Bytefile *f, size_t pos) {
  if (pos >= f->stringtab_size) {
    failure("string pos is out of range: %zu >= %i\n", pos, f->stringtab_size);
  }
  return get_string_unsafe(f, pos);
}

/* Gets import */
static inline const char *get_import_safe(const Bytefile *f, size_t i) {
  if (i >= f->imports_number) {
    failure("import pos is out of range: %zu >= %i\n", i, f->imports_number);
  }
  return get_string_safe(f, f->imports_ptr[i]);
}

/* Gets a name offset for a public symbol */
static inline size_t get_public_name_offset_safe(const Bytefile *f, size_t i) {
  if (i >= f->public_symbols_number) {
    failure("public number is out of range: %zu >= %i\n", i,
            f->public_symbols_number);
  }
  return get_public_name_offset_unsafe(f, i);
}

/* Gets a name for a public symbol */
static inline const char *get_public_name_safe(const Bytefile *f, size_t i) {
  return get_string_safe(f, get_public_name_offset_safe(f, i));
}

/* Gets an offset for a publie symbol */
static inline size_t get_public_offset_safe(const Bytefile *f, size_t i) {
  if (i >= f->public_symbols_number) {
    failure("public number is out of range: %zu >= %i\n", i,
            f->public_symbols_number);
  }
  return get_public_offset_unsafe(f, i);
}

// read from ip

static inline void ip_write_int_safe(char *ip, int32_t x, const Bytefile *bf) {
  if (ip + sizeof(int32_t) > bf->code_ptr + bf->code_size) {
    failure("last command is invalid, int parameter can not be read\n");
  }
  ip_write_int_unsafe(ip, x);
}

static inline uint16_t ip_read_half_int_safe(char **ip, const Bytefile *bf) {
  if (*ip + sizeof(uint16_t) > bf->code_ptr + bf->code_size) {
    failure("last command is invalid, int parameter can not be read\n");
  }
  return ip_read_half_int_unsafe(ip);
}

static inline int32_t ip_read_int_safe(char **ip, const Bytefile *bf) {
  if (*ip + sizeof(int32_t) > bf->code_ptr + bf->code_size) {
    failure("last command is invalid, int parameter can not be read\n");
  }
  return ip_read_int_unsafe(ip);
}

static inline uint8_t ip_read_byte_safe(char **ip, const Bytefile *bf) {
  if (*ip + sizeof(char) > bf->code_ptr + bf->code_size) {
    failure("last command is invalid, byte parameter can not be read\n");
  }
  return ip_read_byte_unsafe(ip);
}

static inline const char *ip_read_string_safe(char **ip, const Bytefile *bf) {
  return get_string_safe(bf, ip_read_int_safe(ip, bf));
}

// ---

static inline size_t calc_publics_size(size_t publics_number) {
  return publics_number * 2 * sizeof(int);
}
