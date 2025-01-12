#include <iostream>
extern "C" {
#include "module_manager.h"
#include "utils.h"
}

#include "analyzer.hpp"
#include "parser.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct ModSymbolPos {
  uint32_t mod_id;
  size_t offset;
};

struct Module {
  std::string name;
  Bytefile *bf;
};

struct ModuleManager {
  std::unordered_map<std::string, uint32_t> loaded_modules;
  std::unordered_map<std::string, ModSymbolPos> public_symbols_mods;
  std::vector<Module> modules;
  std::vector<std::filesystem::path> search_paths;
};

static ModuleManager manager;

uint32_t mod_add_impl(Bytefile *bf, bool do_verification,
                      std::optional<const char *> name = std::nullopt) {
#ifdef DEBUG_VERSION
  std::cerr << "- add module (impl) '" << std::string{name ? *name : ""}
            << "'\n";
#endif
  uint32_t id = manager.modules.size();
  manager.modules.push_back({.name = name ? *name : "", .bf = bf});
  for (size_t i = 0; i < bf->public_symbols_number; ++i) {
    const char *public_name = get_public_name_safe(bf, i);
    size_t public_offset = get_public_offset_safe(bf, i);
    if (strcmp(public_name, "main") == 0) {
      bf->main_offset = public_offset;
    } else if (!manager.public_symbols_mods
                    .insert(
                        {public_name, {.mod_id = id, .offset = public_offset}})
                    .second) {
      failure("public symbol '%s' loaded more then once\n",
              get_public_name_safe(bf, i));
    }
  }
  if (name) {
    manager.loaded_modules.insert({*name, id});
  }
  if (do_verification) {
    analyze(id);
  }
  return id;
}

uint32_t path_mod_load(const char *name, std::filesystem::path &&path,
                       bool do_verification) {
#ifdef DEBUG_VERSION
  std::cerr << "- module path load '" << name << "'\n";
#endif
  Bytefile *module = read_file(path.c_str());
  return mod_add_impl(module, do_verification, name);
}
extern "C" {

void mod_add_search_path(const char *path) {
  manager.search_paths.emplace_back(path);
}

const char *mod_get_name(uint32_t id) {
  if (id > manager.modules.size()) {
    failure("module id is out of range\n");
  }
  return manager.modules[id].name.c_str();
}

Bytefile *mod_get(uint32_t id) {
  if (id > manager.modules.size()) {
    failure("module id is out of range\n");
  }
  return manager.modules[id].bf;
}

int32_t find_mod_loaded(const char *name) {
  auto it = manager.loaded_modules.find(name);

  // module already loaded
  if (it != manager.loaded_modules.end()) {
    return it->second;
  }

  return -1;
}

int32_t mod_load(const char *name, bool do_verification) {
  std::string full_name = std::string{name} + ".bc";

  auto it = manager.loaded_modules.find(name);

  // module already loaded
  if (it != manager.loaded_modules.end()) {
    return it->second;
  }

  if (std::filesystem::exists(full_name)) {
    return path_mod_load(name, full_name, do_verification);
  }
  for (const auto &dir_path : manager.search_paths) {
    auto path = dir_path / full_name;
    if (std::filesystem::exists(path)) {
      return path_mod_load(name, std::move(path), do_verification);
    }
  }

  return -1;
}

uint32_t mod_add(Bytefile *module, bool do_verification) {
#ifdef DEBUG_VERSION
  std::cerr << "- add module, no name\n";
#endif
  return mod_add_impl(module, do_verification);
}

ModSearchResult mod_search_pub_symbol(const char *name) {
  auto it = manager.public_symbols_mods.find(name);
  if (it == manager.public_symbols_mods.end()) {
    return {.symbol_offset = 0, .mod_id = 0, .mod_file = NULL};
  }

  return {
      .symbol_offset = it->second.offset,
      .mod_id = it->second.mod_id,
      .mod_file = mod_get(it->second.mod_id),
  };
}
}
