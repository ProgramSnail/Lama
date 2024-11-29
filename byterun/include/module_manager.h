#pragma once

#include <stdint.h>

#include "utils.h"

struct ModSearchResult {
  ssize_t symbol_offset; // < 0 => not found
  uint32_t mod_id;
  bytefile *mod_file;
};

void mod_add_search_path(const char *path);

bytefile *mod_get(uint32_t id);

int32_t mod_load(const char *name); // < 0 => not found

struct ModSearchResult mod_search_pub_symbol(const char *name);
