#include <cassert>
#include <errno.h>
#include <iomanip>
#include <iostream>
#include <malloc.h>
#include <string.h>
#include <unordered_map>
#include <vector>

#include "parser.hpp"

extern "C" {
#include "types.h"
#include "utils.h"
}

enum class ArgT {
  INT,
  OFFSET,
  STR,
};

// #define FORALL_BINOP(DEF)                                                      \
//   DEF(0, +)                                                                    \
//   DEF(1, -)                                                                    \
//   DEF(2, *)                                                                    \
//   DEF(3, /)                                                                    \
//   DEF(4, %)                                                                    \
//   DEF(5, <)                                                                    \
//   DEF(6, <=)                                                                   \
//   DEF(7, >)                                                                    \
//   DEF(8, >=)                                                                   \
//   DEF(9, ==)                                                                   \
//   DEF(10, !=)                                                                  \
//   DEF(11, &&)                                                                  \
//   DEF(12, ||)

// extern "C" {

// void *__start_custom_data;
// void *__stop_custom_data;

//

// Reads a binary bytecode file by name and unpacks it
Bytefile *read_file(const char *fname) {
  FILE *f = fopen(fname, "rb");
  Bytefile *file;

  if (f == 0) {
    failure("read file %s: %s\n", fname, strerror(errno));
  }

  if (fseek(f, 0, SEEK_END) == -1) {
    failure("read file %s: %s\n", fname, strerror(errno));
  }

  size_t size = ftell(f);

  // [uint] stringtab_size
  // [uint] global_area_size
  // [uint] substs_area_size
  // [uint] imports_number
  // [uint] public_symbols_number
  // char[0] buffer
  size_t file_header_size = 5 * sizeof(uint) + sizeof(char[0]);

  long additional_size = sizeof(Bytefile) - file_header_size;
  file = (Bytefile *)malloc(size +
                            additional_size); // file itself + additional data

  char *file_begin = (char *)file + additional_size;
  char *file_end = file_begin + size;

  if (file == 0) {
    failure("unable to allocate memory to store file data\n");
  }

  rewind(f);

  if (size != fread(&file->stringtab_size, 1, size, f)) {
    failure("%s\n", strerror(errno));
  }

  fclose(f);

  size_t imports_size = file->imports_number * sizeof(int);
  size_t public_symbols_size = file->public_symbols_number * 2 * sizeof(int);

  size_t strings_buffer_offset = public_symbols_size + imports_size;
  if (file->buffer + strings_buffer_offset >= file_end) {
    failure("public symbols are out of the file size\n");
  }
  file->string_ptr = file->buffer + strings_buffer_offset;
  if (file->string_ptr + file->stringtab_size > file_end) {
    failure("strings table is out of the file size\n");
  }

  size_t substs_buffer_offset = strings_buffer_offset + file->stringtab_size;
  file->substs_ptr = file->buffer + substs_buffer_offset;
  if ((char *)file->substs_ptr + file->substs_area_size > file_end) {
    failure("substitutions table is out of the file size\n");
  }

  // if (file->stringtab_size > 0 &&
  //     file->string_ptr[file->stringtab_size - 1] != 0) {
  //   failure("strings table is not zero-ended\n");
  // }
  file->code_size = size - substs_buffer_offset - file->substs_area_size;

  if (file->code_size < 0 || public_symbols_size < 0 ||
      file->stringtab_size < 0) {
    failure("file zones sizes should be >= 0\n");
  }

  file->imports_ptr = (int *)file->buffer;
  file->public_ptr = (int *)(file->buffer + imports_size);
  file->global_ptr = NULL; // is allocated on module run on stack
  file->code_ptr = file->string_ptr + file->stringtab_size;
  // file->global_ptr  = (int*) calloc (file->global_area_size, sizeof (int));

  return file;
}

struct Offsets {
  size_t strings;
  size_t globals;
  size_t code;
};

void rewrite_code_with_offsets(Bytefile *bytefile, const Offsets &offsets) {
  char *ip = bytefile->code_ptr;
  while (ip - bytefile->code_ptr < bytefile->code_size) {
    const auto [cmd, l] = parse_command(&ip, bytefile);

    char *cmd_ip = ip;

    switch (cmd) {
    case Cmd::STRING:
      ip_write_int_unsafe(cmd_ip, ip_read_int_unsafe(&ip) +
                                      offsets.strings); // TODO: check
      break;
    case Cmd::JMP:
    case Cmd::CJMPnz:
    case Cmd::CJMPz:
    case Cmd::CLOSURE:
    case Cmd::CALL:
      ip_write_int_unsafe(cmd_ip, ip_read_int_unsafe(&ip) +
                                      offsets.code); // TODO: check
      break;
    default:
      break;
    }
  }
}

void subst_in_code(Bytefile *bytefile,
                   const std::unordered_map<std::string, size_t> &publics) {
  for (size_t i = 0; i < bytefile->substs_area_size;) {
    if (i + sizeof(uint32_t) >= bytefile->substs_area_size) {
      failure("substitution %zu offset is out of area", i);
    }

    uint32_t offset = *(uint32_t *)(bytefile->substs_ptr + i);
    i += sizeof(uint32_t);
    const char *name = bytefile->substs_ptr + i;

    i += strlen(name);

    if (i > bytefile->substs_area_size) {
      failure("substitution %zu name is out of area", i);
    }

    const auto it = publics.find(name);
    if (it == publics.end()) {
      failure("public name for substitution is not found: %s", name);
    }

    *(uint32_t *)(bytefile->code_ptr + offset) = it->second;
    // TODO: check: +4 to match ?
  }
}

Offsets calc_merge_sizes(const std::vector<Bytefile *> &bytefiles) {
  Offsets sizes{.strings = 0, .globals = 0, .code = 0};
  for (size_t i = 0; i < bytefiles.size(); ++i) {
    sizes.strings += bytefiles[i]->stringtab_size;
    sizes.strings += bytefiles[i]->global_area_size;
    sizes.strings += bytefiles[i]->code_size;
  }
  return sizes;
}

Bytefile *merge_files(std::vector<Bytefile *> &&bytefiles) {
  Offsets sizes = calc_merge_sizes(bytefiles);
  Bytefile *result = (Bytefile *)malloc(sizeof(Bytefile) + sizes.strings +
                                        sizes.code); // globals - on stack

  // collect publics
  std::unordered_map<std::string, size_t> publics;
  std::vector<size_t> main_offsets;
  {
    size_t code_offset = 0;
    for (size_t i = 0; i < bytefiles.size(); ++i) {
      for (size_t j = 0; j < bytefiles[i]->public_symbols_number; ++j) {
        const char *name = get_public_name_unsafe(bytefiles[i], j);
        size_t offset =
            get_public_name_offset_unsafe(bytefiles[i], j) + code_offset;

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
  result->public_symbols_number = 0;

  result->main_offset = 0; // TODO: save al main offsets in some way (?)
  result->string_ptr = result->buffer;
  result->imports_ptr = NULL;
  result->public_ptr = NULL;
  result->code_ptr = result->string_ptr + result->stringtab_size;
  result->global_ptr = NULL;
  result->substs_ptr = NULL;

  // update & merge code segments
  Offsets offsets{.strings = 0, .globals = 0, .code = 0};
  for (size_t i = 0; i < bytefiles.size(); ++i) {
    rewrite_code_with_offsets(bytefiles[i], offsets);
    subst_in_code(bytefiles[i], publics);

    // copy data to merged file
    memcpy(result->string_ptr + offsets.strings, bytefiles[i]->string_ptr,
           bytefiles[i]->stringtab_size);
    memcpy(result->code_ptr + offsets.code, bytefiles[i]->code_ptr,
           bytefiles[i]->code_size);

    // update offsets
    offsets.strings += bytefiles[i]->stringtab_size;
    offsets.globals += bytefiles[i]->global_area_size;
    offsets.code += bytefiles[i]->code_size;
    free(bytefiles[i]);
  }
  return result;
}

const char *command_name(Cmd cmd, int8_t l) {
  static const char *const ops[] = {
#define OP_TO_STR(id, op) "BINOP:" #op,
      FORALL_BINOP(OP_TO_STR)
#undef OP_TO_STR
  };
  static const char *const pats[] = {
      "PATT:=str", "PATT:#string", "PATT:#array", "PATT:#sexp",
      "PATT:#ref", "PATT:#val",    "PATT:#fun"};

#define FORALL_LDTS(DEF)                                                       \
  DEF(G)                                                                       \
  DEF(L)                                                                       \
  DEF(A)                                                                       \
  DEF(C)

  static const char *const ld_ldts[] = {
#define LDT_TO_STR(type) "LD:" #type,
      FORALL_LDTS(LDT_TO_STR)
#undef LDT_TO_STR
  };
  static const char *const lda_ldts[] = {
#define LDT_TO_STR(type) "LDA:" #type,
      FORALL_LDTS(LDT_TO_STR)
#undef LDT_TO_STR
  };
  static const char *const st_ldts[] = {
#define LDT_TO_STR(type) "ST:" #type,
      FORALL_LDTS(LDT_TO_STR)
#undef LDT_TO_STR
  };

#undef FORALL_LDTS

  switch (cmd) {
  case Cmd::EXIT:
    return "EXIT";
  case Cmd::BINOP:
    if (l - 1 >= sizeof(ops) / sizeof(char *)) {
      return "_UNDEF_BINOP_";
    }
    return ops[l - 1];
  case Cmd::CONST:
    return "CONST";
  case Cmd::STRING:
    return "STRING";
  case Cmd::SEXP:
    return "SEXP ";
  case Cmd::STI:
    return "STI";
  case Cmd::STA:
    return "STA";
  case Cmd::JMP:
    return "JMP";
  case Cmd::END:
    return "END";
  case Cmd::RET:
    return "RET";
  case Cmd::DROP:
    return "DROP";
  case Cmd::DUP:
    return "DUP";
  case Cmd::SWAP:
    return "SWAP";
  case Cmd::ELEM:
    return "ELEM";
  case Cmd::LD:
    if (l >= sizeof(ld_ldts) / sizeof(char *)) {
      return "_UNDEF_LD_";
    }
    return ld_ldts[l];
  case Cmd::LDA:
    if (l >= sizeof(lda_ldts) / sizeof(char *)) {
      return "_UNDEF_LDA_";
    }
    return lda_ldts[l];
  case Cmd::ST:
    if (l >= sizeof(st_ldts) / sizeof(char *)) {
      return "_UNDEF_ST_";
    }
    return st_ldts[l];
  case Cmd::CJMPz:
    return "CJMPz";
  case Cmd::CJMPnz:
    return "CJMPnz";
  case Cmd::BEGIN:
    return "BEGIN";
  case Cmd::CBEGIN:
    return "CBEGIN";
  case Cmd::CLOSURE:
    return "CLOSURE";
  case Cmd::CALLC:
    return "CALLC";
  case Cmd::CALL:
    return "CALL";
  case Cmd::TAG:
    return "TAG";
  case Cmd::ARRAY:
    return "ARRAY";
  case Cmd::FAIL:
    return "FAIL";
  case Cmd::LINE:
    return "LINE";
  // case Cmd::CALLF:
  //   return "CALLF";
  case Cmd::PATT:
    if (l >= sizeof(pats) / sizeof(char *)) {
      return "_UNDEF_PATT_";
    }
    return pats[l];
  // NOTE: no longer used
  // case Cmd::Lread:
  //   return "CALL\tLread";
  // case Cmd::Lwrite:
  //   return "CALL\tLwrite";
  // case Cmd::Llength:
  //   return "CALL\tLlength";
  // case Cmd::Lstring:
  //   return "CALL\tLstring";
  // case Cmd::Barray:
  //   return "CALL\tBarray\t%d";
  case Cmd::_UNDEF_:
    return "_UNDEF_";
  }

  exit(1);
}
// } // extern "C"

template <bool use_out, typename T>
static inline const T &print_val(std::ostream &out, const T &val) {
  if constexpr (use_out) {
    out << val;
  }
  return val;
}

template <bool use_out> static inline void print_space(std::ostream &out) {
  if constexpr (use_out) {
    out << ' ';
  }
}

template <bool use_out, ArgT arg>
  requires(arg == ArgT::INT)
static inline uint read_print_val(char **ip, const Bytefile &bf,
                                  std::ostream &out) {
  uint val = ip_read_int_safe(ip, &bf);
  if constexpr (use_out) {
    out << val;
  }
  return val;
}

template <bool use_out, ArgT arg>
  requires(arg == ArgT::OFFSET)
static inline uint read_print_val(char **ip, const Bytefile &bf,
                                  std::ostream &out) {
  uint val = ip_read_int_safe(ip, &bf);
  if constexpr (use_out) {
    out << val;
  }
  return val;
}

template <bool use_out, ArgT arg>
  requires(arg == ArgT::STR)
static inline const char *read_print_val(char **ip, const Bytefile &bf,
                                         std::ostream &out) {
  const char *val = ip_read_string_safe(ip, &bf);
  if constexpr (use_out) {
    out << val;
  }
  return val;
}

template <bool use_out>
static inline void read_print_seq(char **, const Bytefile &, std::ostream &) {}

template <bool use_out, ArgT arg, ArgT... args>
static inline void read_print_seq(char **ip, const Bytefile &bf,
                                  std::ostream &out) {
  read_print_val<use_out, arg>(ip, bf, out);
  if constexpr (use_out && sizeof...(args) != 0) {
    out << ' ';
  }
  read_print_seq<use_out, args...>(ip, bf, out);
}

template <bool use_out, ArgT... args>
static inline void read_print_cmd_seq(Cmd cmd, uint8_t l, char **ip,
                                      const Bytefile &bf, std::ostream &out) {
  if constexpr (use_out) {
    out << command_name(cmd, l);
    if constexpr (sizeof...(args) != 0) {
      out << ' ';
    }
  }
  read_print_seq<use_out, args...>(ip, bf, out);
}

template <bool do_read, bool use_out, ArgT... args>
static inline void read_print_cmd_seq_opt(Cmd cmd, uint8_t l, char **ip,
                                          const Bytefile &bf,
                                          std::ostream &out) {
  if constexpr (do_read) {
    read_print_cmd_seq<use_out, args...>(cmd, l, ip, bf, out);
  }
}

template <bool use_out, bool do_read_args = true>
std::pair<Cmd, uint8_t> parse_command_impl(char **ip, const Bytefile &bf,
                                           std::ostream &out) {
  static const char *const ops[] = {
#define OP_TO_STR(id, op) #op,
      FORALL_BINOP(OP_TO_STR)
#undef OP_TO_STR
  };

  static const char *const pats[] = {"=str", "#string", "#array", "#sexp",
                                     "#ref", "#val",    "#fun"};
  static const char *const ldts[] = {"G", "L", "A", "C"};

  //

  if (*ip >= bf.code_ptr + bf.code_size) {
    failure("instruction pointer is out of range (>= size)");
  }

  if (*ip < bf.code_ptr) {
    failure("instruction pointer is out of range (< 0)");
  }

  Cmd cmd = Cmd::_UNDEF_;

  char *instr_ip = *ip;
  uint8_t x = ip_read_byte_safe(ip, &bf), h = (x & 0xF0) >> 4, l = x & 0x0F;

#ifdef DEBUG_VERSION
  printf("0x%.8lx ", *ip - bf.code_ptr - 1);
  std::cout << ' ' << (int)x << ' ' << (int)h << ' ' << (int)l << ' ';
#endif

  switch (h) {
  case CMD_EXIT:
    cmd = Cmd::EXIT;
    read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
    break;

  /* BINOP */
  case CMD_BINOP: // BINOP ops[l-1]
    cmd = Cmd::BINOP;
    read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
    break;

  case CMD_BASIC:
    switch (l) {
    case CMD_BASIC_CONST: // CONST %d
      cmd = Cmd::CONST;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::INT>(cmd, l, ip, bf,
                                                               out);
      break;
    case CMD_BASIC_STRING: // STRING %s
      cmd = Cmd::STRING;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::INT>(cmd, l, ip, bf,
                                                               out);
      break;

    case CMD_BASIC_SEXP: // SEXP %s %d
      cmd = Cmd::SEXP;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::STR, ArgT::INT>(
          cmd, l, ip, bf, out);
      break;

    case CMD_BASIC_STI: // STI - write by ref (?)
      cmd = Cmd::STI;
      read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BASIC_STA: // STA - write to array elem
      cmd = Cmd::STA;
      read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
      break;

    case CMD_BASIC_JMP: // JMP 0x%.8x
      cmd = Cmd::JMP;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::OFFSET>(cmd, l, ip,
                                                                  bf, out);
      break;

    case CMD_BASIC_END: // END
      cmd = Cmd::END;
      read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BASIC_RET: // RET
      cmd = Cmd::RET;
      read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BASIC_DROP: // DROP
      cmd = Cmd::DROP;
      read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BASIC_DUP: // DUP
      cmd = Cmd::DUP;
      read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BASIC_SWAP: // SWAP
      cmd = Cmd::SWAP;
      read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BASIC_ELEM: // ELEM
      cmd = Cmd::ELEM;
      read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
      break;

    default:
      failure("invalid opcode");
    }
    break;

  case CMD_LD: // LD %d
    cmd = Cmd::LD;
    if (l > sizeof(ldts) / sizeof(char *)) {
      failure("wrong ld argument type");
    }
    read_print_cmd_seq_opt<do_read_args, use_out, ArgT::INT>(cmd, l, ip, bf,
                                                             out);
    break;
  case CMD_LDA: // LDA %d
    cmd = Cmd::LDA;
    if (l > sizeof(ldts) / sizeof(char *)) {
      failure("wrong lda argument type");
    }
    read_print_cmd_seq_opt<do_read_args, use_out, ArgT::INT>(cmd, l, ip, bf,
                                                             out);
    break;
  case CMD_ST: // ST %d
    cmd = Cmd::ST;
    if (l > sizeof(ldts) / sizeof(char *)) {
      failure("wrong st argument type");
    }
    read_print_cmd_seq_opt<do_read_args, use_out, ArgT::INT>(cmd, l, ip, bf,
                                                             out);
    break;

  case CMD_CTRL:
    switch (l) {
    case CMD_CTRL_CJMPz: // CJMPz  0x%.8x
      cmd = Cmd::CJMPz;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::OFFSET>(cmd, l, ip,
                                                                  bf, out);
      break;
    case CMD_CTRL_CJMPnz: // CJMPnz  0x%.8x
      cmd = Cmd::CJMPnz;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::OFFSET>(cmd, l, ip,
                                                                  bf, out);
      break;

    case CMD_CTRL_BEGIN: // BEGIN %d %d // function begin
      cmd = Cmd::BEGIN;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::INT, ArgT::INT>(
          cmd, l, ip, bf, out);
      break;
    case CMD_CTRL_CBEGIN: // CBEGIN %d %d
      cmd = Cmd::CBEGIN;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::INT, ArgT::INT>(
          cmd, l, ip, bf, out);
      break;

    case CMD_CTRL_CLOSURE: { // CLOSURE 0x%.8x
      cmd = Cmd::CLOSURE;
      read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
      print_space<use_out>(out);
      if constexpr (do_read_args) {
        size_t call_p = read_print_val<use_out, ArgT::OFFSET>(ip, bf, out);
        print_space<use_out>(out);
        size_t args_count = read_print_val<use_out, ArgT::INT>(ip, bf, out);
        for (size_t i = 0; i < args_count; i++) {
          uint8_t arg_type = ip_read_byte_safe(ip, &bf);
          if (arg_type > sizeof(ldts) / sizeof(char *)) {
            failure("wrong closure argument type");
          }
          print_space<use_out>(out);
          print_val<use_out>(out, ldts[arg_type]);
          print_space<use_out>(out);
          read_print_val<use_out, ArgT::INT>(ip, bf, out);
        }
      }
      break;
    }

    case CMD_CTRL_CALLC: // CALLC %d // call clojure
      cmd = Cmd::CALLC;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::INT>(cmd, l, ip, bf,
                                                               out);
      break;

    case CMD_CTRL_CALL: // CALL 0x%.8x %d // call function
      cmd = Cmd::CALL;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::OFFSET, ArgT::INT>(
          cmd, l, ip, bf, out);
      break;

    case CMD_CTRL_TAG: // TAG %s %d
      cmd = Cmd::TAG;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::STR, ArgT::INT>(
          cmd, l, ip, bf, out);
      break;
    case CMD_CTRL_FAIL: // FAIL %d %d
      cmd = Cmd::FAIL;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::INT, ArgT::INT>(
          cmd, l, ip, bf, out);
      break;

    case CMD_CTRL_ARRAY: // ARRAY %d
      cmd = Cmd::ARRAY;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::INT>(cmd, l, ip, bf,
                                                               out);
      break;
    case CMD_CTRL_LINE: // LINE %d
      cmd = Cmd::LINE;
      read_print_cmd_seq_opt<do_read_args, use_out, ArgT::INT>(cmd, l, ip, bf,
                                                               out);
      break;
      // NOTE: is replaced
      // case CMD_CTRL_CALLF: // CALLF %s %d
      //   cmd = Cmd::CALLF;
      //   read_print_cmd_seq_opt<do_read_args, use_out, ArgT::STR, ArgT::INT>(
      //       cmd, l, ip, bf, out);
      //   break;

    default:
      failure("invalid opcode");
    }
    break;

  case CMD_PATT: // PATT pats[l]
    // {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"}
    if (l >= sizeof(pats) / sizeof(char *)) {
      failure("invalid opcode");
    }
    cmd = Cmd::PATT;
    read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
    break;

    // NOTE: no longer used
    // case CMD_BUILTIN: {
    //   switch (l) {
    //   case CMD_BUILTIN_Lread: // CALL Lread
    //     cmd = Cmd::Lread;
    //     read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
    //     break;
    //   case CMD_BUILTIN_Lwrite: // CALL Lwrite
    //     cmd = Cmd::Lwrite;
    //     read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
    //     break;
    //   case CMD_BUILTIN_Llength: // CALL Llength
    //     cmd = Cmd::Llength;
    //     read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
    //     break;
    //   case CMD_BUILTIN_Lstring: // CALL Lstring
    //     cmd = Cmd::Lstring;
    //     read_print_cmd_seq_opt<do_read_args, use_out>(cmd, l, ip, bf, out);
    //     break;

    //   case CMD_BUILTIN_Barray: // CALL Barray %d
    //     cmd = Cmd::Barray;
    //     read_print_cmd_seq_opt<do_read_args, use_out, ArgT::INT>(cmd, l,
    //     ip, bf,
    //                                                              out);
    //     break;

    //   default:
    //     failure("invalid opcode");
    //   }
    // } break;

  default:
    failure("invalid opcode");
  }
#ifdef DEBUG_VERSION
  std::cout << command_name(cmd, l) << '\n';
#endif
  return {cmd, l};
}

std::pair<Cmd, uint8_t> parse_command(char **ip, const Bytefile *bf) {
  return parse_command_impl<false>(ip, *bf, std::clog);
}

std::pair<Cmd, uint8_t> parse_command(char **ip, const Bytefile *bf,
                                      std::ostream &out) {
  return parse_command_impl<true>(ip, *bf, out);
}

std::pair<Cmd, uint8_t> parse_command_name(char **ip, const Bytefile *bf) {
  return parse_command_impl<false, false>(ip, *bf, std::clog);
}

bool is_command_name(char *ip, const Bytefile *bf, Cmd cmd) {
  return parse_command_name(&ip, bf).first == cmd;
}

void print_file_info(const Bytefile &bf, std::ostream &out) {
  out << "String table size       : " << bf.stringtab_size << '\n';
  out << "Global area size        : " << bf.global_area_size << '\n';
  out << "Substitutions area size : " << bf.substs_area_size << '\n';
  out << "Number of imports       : " << bf.imports_number << '\n';
  out << "Number of public symbols: " << bf.public_symbols_number << '\n';

  out << "Imports                 :\n";
  for (size_t i = 0; i < bf.imports_number; i++) {
    out << "   " << get_import_safe(&bf, i) << '\n';
  }

  out << "Public symbols          :\n";
  for (size_t i = 0; i < bf.public_symbols_number; i++) {
    out << "   " << std::setfill('0') << std::setw(8) << std::hex
        << get_public_offset_safe(&bf, i) << ": " << std::dec
        << get_public_name_safe(&bf, i) << '\n';
  }
}

void print_file_code(const Bytefile &bf, std::ostream &out) {
  char *ip = bf.code_ptr;

  while (true) {
    out << "   " << std::setfill('0') << std::setw(8) << std::hex
        << ip - bf.code_ptr << ": " << std::dec;
    const auto [cmd, l] = parse_command(&ip, &bf, out);
    out << std::endl;

    if (cmd == Cmd::EXIT) {
      break;
    }
  }
}

void print_file(const Bytefile &bf, std::ostream &out) {
  print_file_info(bf, out);

  out << "Code:\n";
  print_file_code(bf, out);
  out << "code end\n";
}

extern "C" {
const char *read_cmd(char *ip, const Bytefile *bf) {
  const auto [cmd, l] = parse_command_impl<false, false>(&ip, *bf, std::clog);
  return command_name(cmd, l);
  return "";
}
} // extern "C"
