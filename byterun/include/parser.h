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

const char *read_cmd(char *ip);

bytefile *read_file(char *fname);

// void dump_file(FILE *f, bytefile *bf);
