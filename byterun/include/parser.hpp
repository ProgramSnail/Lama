#pragma once

#include <ostream>
#include <vector>

extern "C" {
#include "parser.h"
#include "utils.h"
}

static const constexpr char *GLOBAL_VAR_TAG = "global_";
static const size_t GLOBAL_VAR_TAG_LEN = strlen(GLOBAL_VAR_TAG);

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
  BUILTIN,
  PATT,
  // NOTE: no longer used
  // Lread,
  // Lwrite,
  // Llength,
  // Lstring,
  // Barray,
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
