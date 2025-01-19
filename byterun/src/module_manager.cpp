#include <functional>
#include <iostream>
extern "C" {
#include "module_manager.h"
#include "runtime_externs.h"
#include "stack.h"
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

struct StdFunc {
  void (*ptr)();
  size_t args_count;
  bool is_args = false; // one var for all args
  bool is_vararg = false;
};
bool run_stdlib_func(const char *name) {
  static const std::unordered_map<std::string, StdFunc> std_func = {
      {"Luppercase", {.ptr = (void (*)()) & Luppercase, .args_count = 1}},
      {"Llowercase", {.ptr = (void (*)()) & Llowercase, .args_count = 1}},
      {"Lassert",
       {.ptr = (void (*)()) & Lassert, .args_count = 2, .is_vararg = true}},
      {"Lstring",
       {.ptr = (void (*)()) & Lstring, .args_count = 1, .is_args = true}},
      {"Llength", {.ptr = (void (*)()) & Llength, .args_count = 1}},
      {"LstringInt", {.ptr = (void (*)()) & LstringInt, .args_count = 1}},
      {"Lread", {.ptr = (void (*)()) & Lread, .args_count = 0}},
      {"Lwrite", {.ptr = (void (*)()) & Lwrite, .args_count = 1}},
      {"LmakeArray", {.ptr = (void (*)()) & LmakeArray, .args_count = 1}},
      {"LmakeString", {.ptr = (void (*)()) & LmakeString, .args_count = 1}},
      {"Lstringcat",
       {.ptr = (void (*)()) & Lstringcat, .args_count = 1, .is_args = true}},
      {"LmatchSubString",
       {.ptr = (void (*)()) & LmatchSubString, .args_count = 3}},
      {"Lsprintf",
       {.ptr = (void (*)()) & Lsprintf, .args_count = 1, .is_vararg = true}},
      {"Lsubstring",
       {.ptr = (void (*)()) & Lsubstring, .args_count = 3, .is_args = true}},
      {"Li__Infix_4343",
       {.ptr = (void (*)()) & Li__Infix_4343,
        .args_count = 2,
        .is_args = true}}, // ++
      {"Lclone",
       {.ptr = (void (*)()) & Lclone, .args_count = 1, .is_args = true}},
      {"Lhash", {.ptr = (void (*)()) & Lhash, .args_count = 1}},
      {"LtagHash", {.ptr = (void (*)()) & LtagHash, .args_count = 1}},
      {"Lcompare", {.ptr = (void (*)()) & Lcompare, .args_count = 2}},
      {"LflatCompare", {.ptr = (void (*)()) & LflatCompare, .args_count = 2}},
      {"Lfst", {.ptr = (void (*)()) & Lfst, .args_count = 1}},
      {"Lsnd", {.ptr = (void (*)()) & Lsnd, .args_count = 1}},
      {"Lhd", {.ptr = (void (*)()) & Lhd, .args_count = 1}},
      {"Ltl", {.ptr = (void (*)()) & Ltl, .args_count = 1}},
      {"LreadLine", {.ptr = (void (*)()) & LreadLine, .args_count = 0}},
      {"Lprintf",
       {.ptr = (void (*)()) & Lprintf, .args_count = 1, .is_vararg = true}},
      {"Lfopen", {.ptr = (void (*)()) & Lfopen, .args_count = 2}},
      {"Lfclose", {.ptr = (void (*)()) & Lfclose, .args_count = 1}},
      {"Lfread", {.ptr = (void (*)()) & Lfread, .args_count = 1}},
      {"Lfwrite", {.ptr = (void (*)()) & Lfwrite, .args_count = 2}},
      {"Lfexists", {.ptr = (void (*)()) & Lfexists, .args_count = 1}},
      {"Lfprintf",
       {.ptr = (void (*)()) & Lfprintf, .args_count = 2, .is_vararg = true}},
      {"Lregexp", {.ptr = (void (*)()) & Lregexp, .args_count = 1}},
      {"LregexpMatch", {.ptr = (void (*)()) & LregexpMatch, .args_count = 3}},
      {"Lfailure",
       {.ptr = (void (*)()) & Lfailure, .args_count = 1, .is_vararg = true}},
      {"Lsystem", {.ptr = (void (*)()) & Lsystem, .args_count = 1}},
      {"LgetEnv", {.ptr = (void (*)()) & LgetEnv, .args_count = 1}},
      {"Lrandom", {.ptr = (void (*)()) & Lrandom, .args_count = 1}},
      {"Ltime", {.ptr = (void (*)()) & Ltime, .args_count = 0}},
  };
  // some functions do use on args pointer

  const auto it = std_func.find(name);

  if (it == std_func.end()) {
    return false;
  }

  // TODO: stack safity check
  // TODO: add stdlib func stack check to verification step

  // TODO: work with varargs
  if (it->second.is_vararg) {
    failure("vararg stdlib functions are not supported yet");
  }

  if (it->second.is_args) {
    void *ret = ((void *(*)(aint *))it->second.ptr)((aint *)s_peek());
    s_popn(it->second.args_count);
    s_push(ret);
  } else {
    // TODO: check if arg order is not reversed
    switch (it->second.args_count) {
    case 0: {
      s_push(((void *(*)())it->second.ptr)());
    } break;
    case 1: {
      void *arg1 = s_pop();
      s_push(((void *(*)(void *))it->second.ptr)(arg1));
    } break;
    case 2: {
      void *arg1 = s_pop();
      void *arg2 = s_pop();
      s_push(((void *(*)(void *, void *))it->second.ptr)(arg1, arg2));
    } break;
    case 3: {
      void *arg1 = s_pop();
      void *arg2 = s_pop();
      void *arg3 = s_pop();
      s_push(((void *(*)(void *, void *, void *))it->second.ptr)(arg1, arg2,
                                                                 arg3));
    } break;
    default:
      failure("too many std function args (> 3)");
    }
  }
  return true;
}

} // extern "C"
