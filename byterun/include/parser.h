#pragma once

#include <stdio.h>

#include "utils.h"

#define FORALL_BINOP(DEF)                                                      \
  DEF(CMD_BINOP_ADD, +)                                                        \
  DEF(CMD_BINOP_SUB, -)                                                        \
  DEF(CMD_BINOP_MULT, *)                                                       \
  DEF(CMD_BINOP_DIV, /)                                                        \
  DEF(CMD_BINOP_MOD, %)                                                        \
  DEF(CMD_BINOP_LEQ, <)                                                        \
  DEF(CMD_BINOP_LT, <=)                                                        \
  DEF(CMD_BINOP_GT, >)                                                         \
  DEF(CMD_BINOP_GEQ, >=)                                                       \
  DEF(CMD_BINOP_EQ, ==)                                                        \
  DEF(CMD_BINOP_NEQ, !=)                                                       \
  DEF(CMD_BINOP_AND, &&)                                                       \
  DEF(CMD_BINOP_OR, ||)

#define FORALL_BINOP_FUNC(DEF)                                                 \
  DEF(Ls__Infix_3333)                                                          \
  DEF(Ls__Infix_3838)                                                          \
  DEF(Ls__Infix_6161)                                                          \
  DEF(Ls__Infix_3361)                                                          \
  DEF(Ls__Infix_6061)                                                          \
  DEF(Ls__Infix_60)                                                            \
  DEF(Ls__Infix_6261)                                                          \
  DEF(Ls__Infix_62)                                                            \
  DEF(Ls__Infix_43)                                                            \
  DEF(Ls__Infix_45)                                                            \
  DEF(Ls__Infix_42)                                                            \
  DEF(Ls__Infix_47)                                                            \
  DEF(Ls__Infix_37)

const char *read_cmd(char *ip, const Bytefile *bf);

// Bytefile *read_file(const char *fname);
