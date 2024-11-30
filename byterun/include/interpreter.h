#pragma once

#include "parser.h"

void run_init(size_t *stack);

void run_mod_rec(uint mod_id, int argc, char **argv);

void run_prepare_exec(int argc, char **argv);

void run_mod(uint mod_id, int argc, char **argv);

void run_cleanup();
