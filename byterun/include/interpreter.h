#pragma once

#include "utils.h"
#include <stdbool.h>

// void run(Bytefile *bf, int argc, char **argv);

void run_init(size_t *stack);

void run_mod_rec(uint mod_id, int argc, char **argv, bool do_verification);

void run_prepare_exec(int argc, char **argv);

void run_mod(uint mod_id, int argc, char **argv);

void run_cleanup();
