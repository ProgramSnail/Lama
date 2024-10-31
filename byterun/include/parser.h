#pragma once

#include <stdio.h>

#include "utils.h"

bytefile *read_file(char *fname);

void dump_file(FILE *f, bytefile *bf);
