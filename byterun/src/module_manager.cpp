#include <optional>
extern "C" {
#include "module_manager.h"
#include "parser.h"
#include "utils.h"
}

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct ModSymbolPos {
  uint32_t mod_id;
  size_t offset;
};

struct ModuleManager {
  std::unordered_map<std::string, uint32_t> loaded_modules;
  std::unordered_map<std::string, ModSymbolPos> public_symbols_mods;
  std::vector<bytefile *> modules;
  std::vector<std::filesystem::path> search_paths;
};

static ModuleManager manager;

uint32_t mod_add_impl(bytefile *module,
                      std::optional<const char *> name = std::nullopt) {
  uint32_t id = manager.modules.size();
  manager.modules.push_back(module);
  for (size_t i = 0; i < module->public_symbols_number; ++i) {
    if (!manager.public_symbols_mods
             .insert({
                 get_public_name(module, i),
                 {.mod_id = id, .offset = get_public_offset(module, i)},
             })
             .second) {
      failure("public symbol loaded more then once\n");
    }
  }
  if (name) {
    manager.loaded_modules.insert({*name, id});
  }
  return id;
}

uint32_t path_mod_load(const char *name, std::filesystem::path &&path) {
  bytefile *module = read_file(path.c_str());
  return mod_add_impl(module, name);
}
extern "C" {

void mod_add_search_path(const char *path) {
  manager.search_paths.emplace_back(path);
}

bytefile *mod_get(uint32_t id) {
  if (id > manager.modules.size()) {
    failure("module id is out of range\n");
  }
  return manager.modules[id];
}

int32_t find_mod_loaded(const char *name) {
  auto it = manager.loaded_modules.find(name);

  // module already loaded
  if (it != manager.loaded_modules.end()) {
    return it->second;
  }

  return -1;
}

int32_t mod_load(const char *name) {
  std::string full_name = std::string{name} + ".bc";

  auto it = manager.loaded_modules.find(name);

  // module already loaded
  if (it != manager.loaded_modules.end()) {
    return it->second;
  }

  if (std::filesystem::exists(full_name)) {
    return path_mod_load(name, full_name);
  }
  for (const auto &dir_path : manager.search_paths) {
    auto path = dir_path / full_name;
    if (std::filesystem::exists(path)) {
      return path_mod_load(name, std::move(path));
    }
  }

  return -1;
}

uint32_t mod_add(bytefile *module) { return mod_add_impl(module); }

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
