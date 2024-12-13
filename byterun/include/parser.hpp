#pragma once

#include <ostream>

extern "C" {
#include "utils.h"
}

enum class Cmd : int8_t {
  BINOP,
  CONST,
  STRING,
  SEXP,
  STI,
  STA,
  JMP,
  END,
  RET,
  DROP,
  DUP,
  SWAP,
  ELEM,
  LD,
  LDA,
  ST,
  CJMPz,
  CJMPnz,
  BEGIN,
  CBEGIN,
  CLOSURE,
  CALLC,
  CALL,
  TAG,
  ARRAY,
  FAIL,
  LINE,
  PATT,
  Lread,
  Lwrite,
  Llength,
  Lstring,
  Barray,
  EXIT,
  _UNDEF_,
};

Bytefile *read_file(const char *fname);

static inline int ip_read_int(char **ip, const Bytefile &bf) {
  if (*ip + sizeof(int) > bf.code_ptr + bf.code_size) {
    failure("last command is invalid, int parameter can not be read\n");
  }
  *ip += sizeof(int);
  return *(int *)((*ip) - sizeof(int));
}

static inline uint8_t ip_read_byte(char **ip, const Bytefile &bf) {
  if (*ip + sizeof(char) > bf.code_ptr + bf.code_size) {
    failure("last command is invalid, byte parameter can not be read\n");
  }
  return *(*ip)++;
}

static inline uint8_t ip_read_byte_unsafe(char **ip) { return *(*ip)++; }

static inline const char *ip_read_string(char **ip, const Bytefile &bf) {
  return get_string(&bf, ip_read_int(ip, bf));
}

Cmd parse_command(char **ip, const Bytefile &bf);
Cmd parse_command(char **ip, const Bytefile &bf, std::ostream &out);
