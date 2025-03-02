#pragma once

extern "C" {
#include "utils.h"
}

#include <vector>

void analyze(Bytefile *bf, std::vector<size_t> &&add_publics = {});
