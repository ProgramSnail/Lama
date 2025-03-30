#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "utils.h"

Bytefile *run_with_imports(Bytefile *root, int argc, char **argv,
                           bool do_verification);

// ---

enum BUILTIN : uint {
  BUILTIN_Luppercase,      // 0
  BUILTIN_Llowercase,      // 1
  BUILTIN_Lassert,         // 2
  BUILTIN_Lstring,         // 3
  BUILTIN_Llength,         // 4
  BUILTIN_LstringInt,      // 5
  BUILTIN_Lread,           // 6
  BUILTIN_Lwrite,          // 7
  BUILTIN_LmakeArray,      // 8
  BUILTIN_LmakeString,     // 9
  BUILTIN_Lstringcat,      // 10
  BUILTIN_LmatchSubString, // 11
  BUILTIN_Lsprintf,        // 12
  BUILTIN_Lsubstring,      // 13
  BUILTIN_Li__Infix_4343,  // 14 // ++
  BUILTIN_Lclone,          // 15
  BUILTIN_Lhash,           // 16
  BUILTIN_LtagHash,        // 17
  BUILTIN_Lcompare,        // 18
  BUILTIN_LflatCompare,    // 19
  BUILTIN_Lfst,            // 20
  BUILTIN_Lsnd,            // 21
  BUILTIN_Lhd,             // 22
  BUILTIN_Ltl,             // 23
  BUILTIN_Lprintf,         // 24
  BUILTIN_LreadLine,       // 25
  BUILTIN_Lfopen,          // 26
  BUILTIN_Lfclose,         // 27
  BUILTIN_Lfread,          // 28
  BUILTIN_Lfwrite,         // 29
  BUILTIN_Lfexists,        // 30
  BUILTIN_Lfprintf,        // 31
  BUILTIN_Lregexp,         // 32
  BUILTIN_LregexpMatch,    // 33
  BUILTIN_Lfailure,        // 34
  BUILTIN_Lsystem,         // 35
  BUILTIN_LgetEnv,         // 36
  BUILTIN_Lrandom,         // 37
  BUILTIN_Ltime,           // 38
  BUILTIN_Barray,          // 39 // can't be run with run_stdlib_func
  BUILTIN_NONE,            // 40
};

enum BUILTIN id_by_builtin(const char *name);

void run_stdlib_func(enum BUILTIN id, size_t args_count);
