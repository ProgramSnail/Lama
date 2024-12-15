#pragma once

#include <ostream>

extern "C" {
#include "parser.h"
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

std::pair<Cmd, uint8_t> parse_command(char **ip, const Bytefile *bf);
std::pair<Cmd, uint8_t> parse_command(char **ip, const Bytefile *bf,
                                      std::ostream &out);

std::pair<Cmd, uint8_t> parse_command_name(char **ip, const Bytefile *bf);

bool is_command_name(char *ip, const Bytefile *bf, Cmd cmd);

void print_file(const Bytefile &bf, std::ostream &out);
