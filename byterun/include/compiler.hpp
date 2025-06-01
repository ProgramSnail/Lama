#pragma once

#include "sm_parser.hpp"
#include <vector>

std::vector<std::string> compile_to_code(const std::vector<SMInstr> &code);
