#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "utils.h"

Bytefile *run_with_imports(Bytefile *root, int argc, char **argv,
                           bool do_verification);

// ---

enum BUILTIN : uint {
  BUILTIN_Luppercase,
  BUILTIN_Llowercase,
  BUILTIN_Lassert,
  BUILTIN_Lstring,
  BUILTIN_Llength,
  BUILTIN_LstringInt,
  BUILTIN_Lread,
  BUILTIN_Lwrite,
  BUILTIN_LmakeArray,
  BUILTIN_LmakeString,
  BUILTIN_Lstringcat,
  BUILTIN_LmatchSubString,
  BUILTIN_Lsprintf,
  BUILTIN_Lsubstring,
  BUILTIN_Li__Infix_4343, // ++
  BUILTIN_Lclone,
  BUILTIN_Lhash,
  BUILTIN_LtagHash,
  BUILTIN_Lcompare,
  BUILTIN_LflatCompare,
  BUILTIN_Lfst,
  BUILTIN_Lsnd,
  BUILTIN_Lhd,
  BUILTIN_Ltl,
  BUILTIN_Lprintf,
  BUILTIN_LreadLine,
  BUILTIN_Lfopen,
  BUILTIN_Lfclose,
  BUILTIN_Lfread,
  BUILTIN_Lfwrite,
  BUILTIN_Lfexists,
  BUILTIN_Lfprintf,
  BUILTIN_Lregexp,
  BUILTIN_LregexpMatch,
  BUILTIN_Lfailure,
  BUILTIN_Lsystem,
  BUILTIN_LgetEnv,
  BUILTIN_Lrandom,
  BUILTIN_Ltime,
  BUILTIN_Barray, // can't be run with run_stdlib_func
  BUILTIN_NONE,
};

enum BUILTIN id_by_builtin(const char *name);

void run_stdlib_func(enum BUILTIN id, size_t args_count);
