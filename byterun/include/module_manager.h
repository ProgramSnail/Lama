#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "utils.h"

struct ModSearchResult {
  size_t symbol_offset;
  uint32_t mod_id;
  Bytefile *mod_file; // = NULL => not found
};

void mod_add_search_path(const char *path);

const char *mod_get_name(uint32_t id);

Bytefile *mod_get(uint32_t id);

int32_t find_mod_loaded(const char *name); // < 0 => not found

int32_t mod_load(const char *name, bool do_verification); // < 0 => not found

uint32_t mod_add(Bytefile *module, bool do_verification);

struct ModSearchResult mod_search_pub_symbol(const char *name);
