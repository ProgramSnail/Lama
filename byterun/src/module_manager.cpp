#include <iostream>
extern "C" {
#include "interpreter.h"
#include "module_manager.h"
#include "runtime_externs.h"
#include "stack.h"
#include "utils.h"
}

#include "analyzer.hpp"
#include "parser.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

template <size_t N, typename... Args>
  requires(N == 0)
void call_func(void (*f)(), Args... args) {
  s_push(((void *(*)(Args...))f)(args...));
}

template <size_t N, typename... Args>
  requires(N != 0)
void call_func(void (*f)(), Args... args) {
  void *arg = s_pop();
  call_func<N - 1, Args..., void *>(f, args..., arg);
  // TODO: check that arg is added on the right position
}

template <size_t N, bool do_check = true>
void call_anyarg_func(void (*f)(), size_t n) {
  if constexpr (do_check) {
    if (n > N) {
      failure("too many function arguments (not supported): %zu > %zu\n", n, N);
    }
  }
  if (n == N) {
    call_func<N>(f);
  } else if constexpr (N > 0) {
    call_anyarg_func<N - 1, false>(f, n);
  }
}

// ---

struct Offsets {
  size_t strings;
  size_t globals;
  size_t code;
  size_t publics_num;
};

void rewrite_code_with_offsets(Bytefile *bytefile, const Offsets &offsets) {
  // TODO: globals offsets

  char *ip = bytefile->code_ptr;
  while (ip - bytefile->code_ptr < bytefile->code_size) {
    char *instr_ip = ip;

#ifdef DEBUG_VERSION
    std::cout << ip - bytefile->code_ptr << ": ";
    const auto [cmd, l] = parse_command(&ip, bytefile, std::cout);
    std::cout << '\n';
#else
    const auto [cmd, l] = parse_command(&ip, bytefile);
#endif

    char *read_ip = instr_ip + 1;
    char *write_ip = instr_ip + 1;
    switch (cmd) {
    case Cmd::STRING:
      ip_write_int_unsafe(write_ip,
                          ip_read_int_unsafe(&read_ip) + offsets.strings);
      break;
    case Cmd::JMP:
    case Cmd::CJMPnz:
    case Cmd::CJMPz:
    case Cmd::CALL:
      ip_write_int_unsafe(write_ip,
                          ip_read_int_unsafe(&read_ip) + offsets.code);
      break;
    case Cmd::CLOSURE: {
      ip_write_int_unsafe(write_ip,
                          ip_read_int_unsafe(&read_ip) + offsets.code);
      size_t args_count = ip_read_int_unsafe(&read_ip);
      for (size_t i = 0; i < args_count; ++i) {
        uint8_t arg_type = ip_read_byte_unsafe(&read_ip);
        if (to_var_category(arg_type) == VAR_GLOBAL) {
          write_ip = read_ip;
          ip_write_int_unsafe(write_ip,
                              ip_read_int_unsafe(&read_ip) + offsets.globals);
        }
      }
      break;
    }
    case Cmd::LD:
    case Cmd::LDA:
    case Cmd::ST:
      if (to_var_category(l) == VAR_GLOBAL) {
        ip_write_int_unsafe(write_ip,
                            ip_read_int_unsafe(&read_ip) + offsets.globals);
      }
      break;
    default:
      break;
    }
  }
}

void subst_in_code(Bytefile *bytefile,
                   const std::unordered_map<std::string, size_t> &publics) {
  for (size_t i = 0; i < bytefile->substs_area_size; ++i) {
    if (i + sizeof(uint32_t) >= bytefile->substs_area_size) {
      failure("substitution %zu offset is out of area\n", i);
    }

    uint32_t offset = *(uint32_t *)(bytefile->substs_ptr + i);
    i += sizeof(uint32_t);
    const char *name = bytefile->substs_ptr + i;
    i += strlen(name);

#ifdef DEBUG_VERSION
    printf("subst: offset %u, name %s\n", offset, name);
#endif

    if (i > bytefile->substs_area_size) {
      failure("substitution %zu name is out of area\n", i);
    }

    BUILTIN builtin = id_by_builtin(name);

    // NOTE: address is first argument of the call
    if (builtin != BUILTIN_NONE) {
      uint8_t cmd = ((CMD_CTRL << 4) | CMD_CTRL_BUILTIN);
#ifdef DEBUG_VERSION
      printf("set builtin %i, offset %i, cmd %u = (%u << 4) | %u, h = %u, l = "
             "%u\n",
             builtin, offset, cmd, CMD_CTRL, CMD_CTRL_BUILTIN,
             (cmd & 0xF0) >> 4, cmd & 0x0F);
#endif
      *(uint8_t *)(bytefile->code_ptr + offset - 1) =
          cmd; // set BUILTIN command
      *(uint32_t *)(bytefile->code_ptr + offset) = builtin;
      continue;
    }

    const auto it = publics.find(name);
    if (it == publics.end()) {
      failure("public name for substitution is not found: <%s>\n", name);
    }

    *(uint32_t *)(bytefile->code_ptr + offset) = it->second;
    // TODO: check: +4 to match ?
  }
}

Offsets calc_merge_sizes(const std::vector<Bytefile *> &bytefiles) {
  Offsets sizes{.strings = 0, .globals = 0, .code = 0, .publics_num = 0};
  for (size_t i = 0; i < bytefiles.size(); ++i) {
    sizes.strings += bytefiles[i]->stringtab_size;
    sizes.globals += bytefiles[i]->global_area_size;
    sizes.code += bytefiles[i]->code_size;
    sizes.publics_num += bytefiles[i]->public_symbols_number;
  }
  return sizes;
}

struct MergeResult {
  Bytefile *bf;
  std::vector<size_t> main_offsets;
};

MergeResult merge_files(std::vector<Bytefile *> &&bytefiles) {
  Offsets sizes = calc_merge_sizes(bytefiles);
  size_t public_symbols_size = calc_publics_size(sizes.publics_num);
  Bytefile *result =
      (Bytefile *)malloc(sizeof(Bytefile) + sizes.strings + sizes.code +
                         public_symbols_size); // globals are on the stack

  // collect publics
  // TODO: add publics + updat name offsets too ?())
  std::unordered_map<std::string, size_t> publics;
  std::vector<size_t> main_offsets;
  {
    size_t code_offset = 0;
    for (size_t i = 0; i < bytefiles.size(); ++i) {
#ifdef DEBUG_VERSION
      printf("bytefile <%zu>\n", i);
#endif
      for (size_t j = 0; j < bytefiles[i]->public_symbols_number; ++j) {
#ifdef DEBUG_VERSION
        printf("symbol <%zu>:<%zu>\n", i, j);
#endif
        const char *name = get_public_name_unsafe(bytefiles[i], j);
        size_t offset = get_public_offset_unsafe(bytefiles[i], j) + code_offset;

#ifdef DEBUG_VERSION
        printf("symbol %s : %zu (code offset %zu)\n", name, offset,
               code_offset);
#endif
        if (strcmp(name, "main") == 0) {
          main_offsets.push_back(offset);
        } else if (!publics.insert({name, offset}).second) {
          failure("public name found more then once: %s", name);
        }
      }
      code_offset += bytefiles[i]->code_size;
    }
  }

  // init result
  result->code_size = sizes.code;
  result->stringtab_size = sizes.strings;
  result->global_area_size = sizes.globals;
  result->substs_area_size = 0;
  result->imports_number = 0;
  result->public_symbols_number = sizes.publics_num;

  result->main_offset = 0; // TODO: save al main offsets in some way (?)
  result->public_ptr = (int *)result->buffer;
  result->string_ptr = (char *)result->public_ptr + public_symbols_size;
  result->code_ptr = result->string_ptr + result->stringtab_size;
  result->imports_ptr = NULL;
  result->global_ptr = NULL;
  result->substs_ptr = NULL;

  // update & merge code segments
  Offsets offsets{.strings = 0, .globals = 0, .code = 0, .publics_num = 0};
  // REMOVE printf("merge bytefiles\n");
  for (size_t i = 0; i < bytefiles.size(); ++i) {
    // REMOVE printf("rewrite offsets %zu\n", i);
    rewrite_code_with_offsets(bytefiles[i], offsets);
    // REMOVE printf("subst in code %zu\n", i);
    subst_in_code(bytefiles[i], publics);

    size_t publics_offset = calc_publics_size(offsets.publics_num);

    // copy data to merged file
    memcpy(result->string_ptr + offsets.strings, bytefiles[i]->string_ptr,
           bytefiles[i]->stringtab_size);
    memcpy(result->code_ptr + offsets.code, bytefiles[i]->code_ptr,
           bytefiles[i]->code_size);
    memcpy((char *)result->public_ptr + publics_offset,
           (char *)bytefiles[i]->public_ptr,
           calc_publics_size(
               bytefiles[i]->public_symbols_number)); // TODO: recalc publics:
                                                      // offsets, strings

    // update offsets
    offsets.strings += bytefiles[i]->stringtab_size;
    offsets.globals += bytefiles[i]->global_area_size;
    offsets.code += bytefiles[i]->code_size;
    offsets.publics_num += bytefiles[i]->public_symbols_number;

    free(bytefiles[i]);
  }

#ifdef DEBUG_VERSION
  std::cout << "- merged file:\n";
  print_file(*result, std::cout);
#endif
  return {result, main_offsets};
}

// ---

Bytefile *path_mod_load(const char *name, std::filesystem::path &&path) {
#ifdef DEBUG_VERSION
  std::cout << "- module path load '" << name << "'\n";
#endif
  return read_file(path.c_str());
}

static std::vector<std::filesystem::path> search_paths;

extern "C" {

void mod_add_search_path(const char *path) { search_paths.emplace_back(path); }

Bytefile *mod_load(const char *name) {
  std::string full_name = std::string{name} + ".bc";

  if (std::filesystem::exists(full_name)) {
    return path_mod_load(name, full_name);
  }
  for (const auto &dir_path : search_paths) {
    auto path = dir_path / full_name;
    if (std::filesystem::exists(path)) {
      return path_mod_load(name, std::move(path));
    }
  }

  return NULL;
}

} // extern "C"

// uint32_t mod_add(Bytefile *module, bool do_verification) {
// #ifdef DEBUG_VERSION
//   std::cout << "- add module, no name\n";
// #endif
//   return mod_add_impl(module, do_verification);
// }

// ModSearchResult mod_search_pub_symbol(const char *name) {
//   auto it = manager.public_symbols_mods.find(name);
//   if (it == manager.public_symbols_mods.end()) {
//     return {.symbol_offset = 0, .mod_id = 0, .mod_file = NULL};
//   }

//   return {
//       .symbol_offset = it->second.offset,
//       .mod_id = it->second.mod_id,
//       .mod_file = mod_get(it->second.mod_id),
//   };
// }

void mod_load_rec(Bytefile *mod,
                  std::unordered_map<std::string, Bytefile *> &loaded,
                  std::vector<Bytefile *> &loaded_ord) {
#ifdef DEBUG_VERSION
  printf("- run mod rec, %i imports\n", mod->imports_number);
#endif
  for (size_t i = 0; i < mod->imports_number; ++i) {
    const char *import_str = get_import_safe(mod, i);
    if (loaded.count(import_str) == 0 &&
        strcmp(import_str, "Std") != 0) { // not loaded
#ifdef DEBUG_VERSION
      printf("- mod load <%s>\n", import_str);
#endif
      Bytefile *import_mod = mod_load(import_str); // TODO
      if (import_mod == NULL) {
        failure("module <%s> not found\n", import_str);
      }
      loaded.insert({import_str, import_mod});
      mod_load_rec(import_mod, loaded, loaded_ord);
    }
  }
  loaded_ord.push_back(mod);
}

MergeResult load_with_imports(Bytefile *root, bool do_verification) {
  std::unordered_map<std::string, Bytefile *> loaded;
  std::vector<Bytefile *> loaded_ord;
  mod_load_rec(root, loaded, loaded_ord);

  MergeResult result = merge_files(std::move(loaded_ord));

  if (do_verification) {
#ifdef DEBUG_VERSION
    printf("main offsets count: %zu\n", result.main_offsets.size());
#endif
    analyze(result.bf /*, std::move(result.main_offsets)*/);
  }
  return result;
}

extern "C" {
Bytefile *run_with_imports(Bytefile *root, int argc, char **argv,
                           bool do_verification) {

  MergeResult result = load_with_imports(root, do_verification);

  Bytefile *bf = result.bf;

  bf->main_offset = 0;
  prepare_state(bf, &s); // NOTE: for push_globals
  push_globals(&s);

  for (size_t i = 0; i < result.main_offsets.size(); ++i) {
    bf->main_offset = result.main_offsets[i];
    set_argc_argv(argc, argv); // args for module main
    run_main(bf, argc, argv);
  }

  cleanup_state(&s);

  return bf;
}
} // extern "C"

struct StdFunc {
  void (*ptr)();
  size_t args_count;
  bool is_args = false; // one var for all args
  bool is_vararg = false;
};

BUILTIN id_by_builtin(const char *name) {
  static const std::unordered_map<std::string, BUILTIN> std_func = {
      {"Luppercase", BUILTIN_Luppercase},
      {"Llowercase", BUILTIN_Llowercase},
      {"Lassert", BUILTIN_Lassert},
      {"Lstring", BUILTIN_Lstring},
      {"Llength", BUILTIN_Llength},
      {"LstringInt", BUILTIN_LstringInt},
      {"Lread", BUILTIN_Lread},
      {"Lwrite", BUILTIN_Lwrite},
      {"LmakeArray", BUILTIN_LmakeArray},
      {"LmakeString", BUILTIN_LmakeString},
      {"Lstringcat", BUILTIN_Lstringcat},
      {"LmatchSubString", BUILTIN_LmatchSubString},
      {"Lsprintf", BUILTIN_Lsprintf},
      {"Lsubstring", BUILTIN_Lsubstring},
      {"Li__Infix_4343", BUILTIN_Li__Infix_4343}, // ++
      {"Lclone", BUILTIN_Lclone},
      {"Lhash", BUILTIN_Lhash},
      {"LtagHash", BUILTIN_LtagHash},
      {"Lcompare", BUILTIN_Lcompare},
      {"LflatCompare", BUILTIN_LflatCompare},
      {"Lfst", BUILTIN_Lfst},
      {"Lsnd", BUILTIN_Lsnd},
      {"Lhd", BUILTIN_Lhd},
      {"Ltl", BUILTIN_Ltl},
      {"LreadLine", BUILTIN_LreadLine},
      {"Lprintf", BUILTIN_Lprintf},
      {"Lfopen", BUILTIN_Lfopen},
      {"Lfclose", BUILTIN_Lfclose},
      {"Lfread", BUILTIN_Lfread},
      {"Lfwrite", BUILTIN_Lfwrite},
      {"Lfexists", BUILTIN_Lfexists},
      {"Lfprintf", BUILTIN_Lfprintf},
      {"Lregexp", BUILTIN_Lregexp},
      {"LregexpMatch", BUILTIN_LregexpMatch},
      {"Lfailure", BUILTIN_Lfailure},
      {"Lsystem", BUILTIN_Lsystem},
      {"LgetEnv", BUILTIN_LgetEnv},
      {"Lrandom", BUILTIN_Lrandom},
      {"Ltime", BUILTIN_Ltime},
      {".array", BUILTIN_Barray},
  };

  auto const it = std_func.find(name);

  return it == std_func.end() ? BUILTIN_NONE : it->second;
}

void run_stdlib_func(BUILTIN id, size_t args_count) {
  void *ret = NULL;
  // TODO: deal with right pointers, etc.
  switch (id) {
  case BUILTIN_Luppercase:
    ret = (void *)Luppercase(*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Llowercase:
    ret = (void *)Llowercase(*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Lassert:
    // .args_count = 2, .is_vararg = true
    call_anyarg_func<20>((void (*)()) & Lassert, args_count);
    break;
  case BUILTIN_Lstring:
    ret = Lstring(s_nth_i(0)); // .is_args = true
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Llength:
    ret = (void *)Llength(*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_LstringInt:
    ret = (void *)LstringInt((char *)*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Lread:
    printf(" ");
    ret = (void *)Lread();
    s_push(ret);
    break;
  case BUILTIN_Lwrite:
    ret = (void *)Lwrite(*s_nth_i(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_LmakeArray:
    ret = (void *)LmakeArray(*s_nth_i(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_LmakeString:
    ret = (void *)LmakeString(*s_nth_i(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Lstringcat:
    ret = (void *)Lstringcat(s_nth_i(0)); // .is_args = true;
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_LmatchSubString:
    ret = (void *)LmatchSubString((char *)*s_nth(0), (char *)*s_nth(1),
                                  *s_nth_i(2));
    s_popn(3);
    s_push(ret);
    break;
  case BUILTIN_Lsprintf:
    // .args_count = 1, .is_vararg = true
    call_anyarg_func<20>((void (*)()) & Lsprintf, args_count);
    break;
  case BUILTIN_Lsubstring:
    ret = (void *)Lsubstring(s_nth_i(0)); // .is_args = true;
    s_popn(3);
    s_push(ret);
    break;
  case BUILTIN_Li__Infix_4343:
    ret = (void *)Li__Infix_4343(s_nth_i(0)); // .is_args = true
    s_popn(2);
    s_push(ret);
    break; // ++
  case BUILTIN_Lclone:
    ret = (void *)Lclone(s_nth_i(0)); // .is_args = true;
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Lhash:
    ret = (void *)Lhash(*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_LtagHash:
    ret = (void *)LtagHash((char *)*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Lcompare:
    ret = (void *)Lcompare(*s_nth(0), *s_nth(1));
    s_popn(2);
    s_push(ret);
    break;
  case BUILTIN_LflatCompare:
    ret = (void *)LflatCompare(*s_nth(0), *s_nth(1));
    s_popn(2);
    s_push(ret);
    break;
  case BUILTIN_Lfst:
    ret = (void *)Lfst(*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Lsnd:
    ret = (void *)Lsnd(*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Lhd:
    ret = (void *)Lhd(*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Ltl:
    ret = (void *)Ltl(*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_LreadLine:
    ret = (void *)LreadLine();
    s_push(ret);
    break;
  case BUILTIN_Lprintf:
    // .args_count = 1, .is_vararg = true
    call_anyarg_func<20>((void (*)()) & Lprintf, args_count);
    break;
  case BUILTIN_Lfopen:
    ret = (void *)Lfopen((char *)*s_nth(0), (char *)*s_nth(1));
    s_popn(2);
    s_push(ret);
    break;
  case BUILTIN_Lfclose:
    /*ret = (void *)*/ Lfclose((FILE *)*s_nth(0));
    s_popn(1);
    // s_push(ret); // NOTE: ??
    break;
  case BUILTIN_Lfread:
    ret = (void *)Lfread((char *)*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Lfwrite:
    /*ret = (void *)*/ Lfwrite((char *)*s_nth(0), (char *)*s_nth(1));
    s_popn(2);
    // s_push(ret); // NOTE: ??
    break;
  case BUILTIN_Lfexists:
    ret = (void *)Lfexists((char *)*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Lfprintf:
    // .args_count = 2, .is_vararg = true
    call_anyarg_func<20>((void (*)()) & Lfprintf, args_count);
    break;
  case BUILTIN_Lregexp:
    ret = (void *)Lregexp((char *)*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_LregexpMatch:
    ret = (void *)LregexpMatch((struct re_pattern_buffer *)*s_nth(0),
                               (char *)*s_nth(1), *s_nth_i(2));
    s_popn(2);
    s_push(ret);
    break;
  case BUILTIN_Lfailure:
    // .args_count = 1, .is_vararg = true
    call_anyarg_func<20>((void (*)()) & Lfailure, args_count);
    break;
  case BUILTIN_Lsystem:
    ret = (void *)Lsystem((char *)*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_LgetEnv:
    ret = (void *)LgetEnv((char *)*s_nth(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Lrandom:
    ret = (void *)Lrandom(*s_nth_i(0));
    s_popn(1);
    s_push(ret);
    break;
  case BUILTIN_Ltime:
    ret = (void *)Ltime();
    s_push(ret);
    break;
  default:
    failure("RUNTIME ERROR: stdlib function <%u> not found\n", id);
    break;
  }
  // some functions do use on args pointer
  // // NOTE: is checked on subst
  // if (id > sizeof(std_func) / sizeof(StdFunc)) {
  //   failure("RUNTIME ERROR: stdlib function <%u> not found\n", id);
  // }

  // // TODO: move to bytecode verifier
  // if ((!func.is_vararg && func.args_count != args_count) ||
  //     func.args_count > args_count) {
  //   failure("RUNTIME ERROR: stdlib function <%u> argument count <%zu> is not
  //   "
  //           "expected (expected is <%s%zu>)\n",
  //           id, func.args_count, func.is_vararg ? ">=" : "=", args_count);
  // }
}
