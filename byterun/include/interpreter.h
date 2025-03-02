#pragma once

#include "utils.h"
#include <stdbool.h>

// void run(Bytefile *bf, int argc, char **argv);

void run_init(size_t *stack);

void set_argc_argv(int argc, char **argv);

void run_main(Bytefile *bf, int argc, char **argv);

void run_cleanup();
