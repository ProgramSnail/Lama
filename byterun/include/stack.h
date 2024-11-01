#pragma once

#include "../../runtime/gc.h"
#include "runtime_externs.h"
#include "types.h"
#include "utils.h"

#include "stdlib.h"

void s_push(struct State *s, void *val);

void s_push_nil(struct State *s);

void s_pushn_nil(struct State *s, size_t n);

void *s_pop(struct State *s);

void s_popn(struct State *s, size_t n);

// ------ functions ------

// |> param_0 ... param_n | frame[ ret rp prev_fp &params &locals &end
// ]
// |> local_0 ... local_m |> | ...
//
// where |> defines corresponding frame pointer, | is stack pointer
// location before / after new frame added
void s_enter_f(struct State *s, char *func_ip, size_t params_sz,
               size_t locals_sz);

void s_exit_f(struct State *s);

void **var_by_category(struct State *s, enum VarCategory category, int id);

// --- changed runtime operations ---

void *s_Bsexp(struct State *state, int bn, int tag);
