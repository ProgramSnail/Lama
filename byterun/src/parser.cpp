#include <cassert>
#include <errno.h>
#include <iostream>
#include <malloc.h>
#include <string.h>

#include "parser.hpp"

extern "C" {
#include "utils.h"
}

enum CMD_TOPLVL {
  CMD_BINOP = 0,
  CMD_BASIC,
  CMD_LD,
  CMD_LDA,
  CMD_ST,
  CMD_CTRL,
  CMD_PATT,
  CMD_BUILTIN,
  CMD_EXIT = 15,
};

enum CMD_BASICS {
  CMD_BASIC_CONST = 0,
  CMD_BASIC_STRING,
  CMD_BASIC_SEXP,
  CMD_BASIC_STI,
  CMD_BASIC_STA,
  CMD_BASIC_JMP,
  CMD_BASIC_END,
  CMD_BASIC_RET,
  CMD_BASIC_DROP,
  CMD_BASIC_DUP,
  CMD_BASIC_SWAP,
  CMD_BASIC_ELEM,
};

enum CMD_CTRLS {
  CMD_CTRL_CJMPz = 0,
  CMD_CTRL_CJMPnz,
  CMD_CTRL_BEGIN,
  CMD_CTRL_CBEGIN,
  CMD_CTRL_CLOSURE,
  CMD_CTRL_CALLC,
  CMD_CTRL_CALL,
  CMD_CTRL_TAG,
  CMD_CTRL_ARRAY,
  CMD_CTRL_FAIL,
  CMD_CTRL_LINE,
};

enum CMD_PATTS {
  CMD_PATT_STR = 0,
  CMD_PATT_STR_TAG,
  CMD_PATT_ARRAY_TAG,
  CMD_PATT_SEXP_TAG,
  CMD_PATT_REF_TAG,
  CMD_PATT_VAL_TAG,
  CMD_PATT_FUN_TAG,
};

enum CMD_BUILTINS {
  CMD_BUILTIN_Lread = 0,
  CMD_BUILTIN_Lwrite,
  CMD_BUILTIN_Llength,
  CMD_BUILTIN_Lstring,
  CMD_BUILTIN_Barray,
};

enum class ArgT {
  INT,
  OFFSET,
  STR,
};

#define FORALL_BINOP(DEF)                                                      \
  DEF(0, +)                                                                    \
  DEF(1, -)                                                                    \
  DEF(2, *)                                                                    \
  DEF(3, /)                                                                    \
  DEF(4, %)                                                                    \
  DEF(5, <)                                                                    \
  DEF(6, <=)                                                                   \
  DEF(7, >)                                                                    \
  DEF(8, >=)                                                                   \
  DEF(9, ==)                                                                   \
  DEF(10, !=)                                                                  \
  DEF(11, &&)                                                                  \
  DEF(12, ||)

// Reads a binary bytecode file by name and unpacks it
Bytefile *read_file(const char *fname) {
  FILE *f = fopen(fname, "rb");
  Bytefile *file;

  if (f == 0) {
    failure(strerror(errno));
  }

  if (fseek(f, 0, SEEK_END) == -1) {
    failure(strerror(errno));
  }

  long size = ftell(f);
  long additional_size = sizeof(void *) * 4 + sizeof(int);
  file = (Bytefile *)malloc(size +
                            additional_size); // file itself + additional data

  char *file_begin = (char *)file + additional_size;
  char *file_end = file_begin + size;

  if (file == 0) {
    failure("unable to allocate memory to store file data");
  }

  rewind(f);

  if (size != fread(&file->stringtab_size, 1, size, f)) {
    failure(strerror(errno));
  }

  fclose(f);

  long public_symbols_size = file->public_symbols_number * 2 * sizeof(int);
  if (file->buffer + public_symbols_size >= file_end) {
    failure("public symbols are out of the file size");
  }
  if (file->string_ptr + file->stringtab_size > file_end) {
    failure("strings table is out of the file size");
  }
  if (file->code_size < 0 || public_symbols_size < 0 ||
      file->stringtab_size < 0) {
    failure("file zones sizes should be >= 0");
  }

  file->string_ptr = &file->buffer[public_symbols_size];
  file->public_ptr = (int *)file->buffer;
  file->code_ptr = &file->string_ptr[file->stringtab_size];
  file->global_ptr = (int *)calloc(file->global_area_size, sizeof(int));

  file->code_size = size - public_symbols_size - file->stringtab_size;

  return file;
}

std::string command_name(Cmd cmd, int8_t l) {
  static const char *const ops[] = {
#define OP_TO_STR(id, op) #op,
      FORALL_BINOP(OP_TO_STR)
#undef OP_TO_STR
  };
  static const char *const pats[] = {"=str", "#string", "#array", "#sexp",
                                     "#ref", "#val",    "#fun"};
  static const char *const ldts[] = {"G", "L", "A", "C"};

  switch (cmd) {
  case Cmd::EXIT:
    return "EXIT";
  case Cmd::BINOP:
    if (l - 1 >= sizeof(ops) / sizeof(char *)) {
      return "_UNDEF_BINOP_";
    }
    return "BINOP:" + std::string{ops[l - 1]};
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
    if (l >= sizeof(ldts) / sizeof(char *)) {
      return "_UNDEF_LD_";
    }
    return "LD:" + std::string{ldts[l]};
  case Cmd::LDA:
    if (l >= sizeof(ldts) / sizeof(char *)) {
      return "_UNDEF_LDA_";
    }
    return "LDA:" + std::string{ldts[l]};
  case Cmd::ST:
    if (l >= sizeof(ldts) / sizeof(char *)) {
      return "_UNDEF_ST_";
    }
    return "ST:" + std::string{ldts[l]};
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
  case Cmd::PATT:
    if (l >= sizeof(pats) / sizeof(char *)) {
      return "_UNDEF_PATT_";
    }
    return "PATT:" + std::string{pats[l]};
  case Cmd::Lread:
    return "CALL\tLread";
  case Cmd::Lwrite:
    return "CALL\tLwrite";
  case Cmd::Llength:
    return "CALL\tLlength";
  case Cmd::Lstring:
    return "CALL\tLstring";
  case Cmd::Barray:
    return "CALL\tBarray\t%d";
  case Cmd::_UNDEF_:
    return "_UNDEF_";
  }

  exit(1);
}

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
  uint val = ip_read_int(ip, bf);
  if constexpr (use_out) {
    out << val;
  }
  return val;
}

template <bool use_out, ArgT arg>
  requires(arg == ArgT::OFFSET)
static inline uint read_print_val(char **ip, const Bytefile &bf,
                                  std::ostream &out) {
  uint val = ip_read_int(ip, bf);
  if constexpr (use_out) {
    out << val; // TODO
  }
  return val;
}

template <bool use_out, ArgT arg>
  requires(arg == ArgT::STR)
static inline const char *read_print_val(char **ip, const Bytefile &bf,
                                         std::ostream &out) {
  const char *val = ip_read_string(ip, bf);
  if constexpr (use_out) {
    out << val;
  }
  return val;
}

template <bool use_out>
static inline void read_print_seq(char **ip, const Bytefile &bf,
                                  std::ostream &out) {}

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

template <bool use_out>
std::pair<Cmd, uint8_t> parse_command_impl(char **ip, const Bytefile &bf,
                                           std::ostream &out) {
  static const char *const ops[] = {
#define OP_TO_STR(id, op) "op",
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
  uint8_t x = ip_read_byte(ip, bf), h = (x & 0xF0) >> 4, l = x & 0x0F;

#ifdef DEBUG_VERSION
  printf("0x%.8lx ", *ip - bf.code_ptr - 1);
  std::cout << ' ' << (int)x << ' ' << (int)h << ' ' << (int)l << ' ';
#endif

  switch (h) {
  case CMD_EXIT:
    cmd = Cmd::EXIT;
    read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
    break;

  /* BINOP */
  case CMD_BINOP: // BINOP ops[l-1]
    cmd = Cmd::BINOP;
    read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
    break;

  case CMD_BASIC:
    switch (l) {
    case CMD_BASIC_CONST: // CONST %d
      cmd = Cmd::CONST;
      read_print_cmd_seq<use_out, ArgT::INT>(cmd, l, ip, bf, out);
      break;
    case CMD_BASIC_STRING: // STRING %s
      cmd = Cmd::STRING;
      read_print_cmd_seq<use_out, ArgT::INT>(cmd, l, ip, bf, out);
      break;

    case CMD_BASIC_SEXP: // SEXP %s %d
      cmd = Cmd::SEXP;
      read_print_cmd_seq<use_out, ArgT::STR, ArgT::INT>(cmd, l, ip, bf, out);
      break;

    case CMD_BASIC_STI: // STI - write by ref (?)
      cmd = Cmd::STI;
      read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BASIC_STA: // STA - write to array elem
      cmd = Cmd::STA;
      read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
      break;

    case CMD_BASIC_JMP: // JMP 0x%.8x
      cmd = Cmd::JMP;
      read_print_cmd_seq<use_out, ArgT::OFFSET>(cmd, l, ip, bf, out);
      break;

    case CMD_BASIC_END: // END
      cmd = Cmd::END;
      read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BASIC_RET: // RET
      cmd = Cmd::RET;
      read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BASIC_DROP: // DROP
      cmd = Cmd::DROP;
      read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BASIC_DUP: // DUP
      cmd = Cmd::DUP;
      read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BASIC_SWAP: // SWAP
      cmd = Cmd::SWAP;
      read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BASIC_ELEM: // ELEM
      cmd = Cmd::ELEM;
      read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
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
    read_print_cmd_seq<use_out, ArgT::INT>(cmd, l, ip, bf, out);
    break;
  case CMD_LDA: // LDA %d
    cmd = Cmd::LDA;
    if (l > sizeof(ldts) / sizeof(char *)) {
      failure("wrong lda argument type");
    }
    read_print_cmd_seq<use_out, ArgT::INT>(cmd, l, ip, bf, out);
    break;
  case CMD_ST: // ST %d
    cmd = Cmd::ST;
    if (l > sizeof(ldts) / sizeof(char *)) {
      failure("wrong st argument type");
    }
    read_print_cmd_seq<use_out, ArgT::INT>(cmd, l, ip, bf, out);
    break;

  case CMD_CTRL:
    switch (l) {
    case CMD_CTRL_CJMPz: // CJMPnz  0x%.8x
      cmd = Cmd::CJMPz;
      read_print_cmd_seq<use_out, ArgT::OFFSET>(cmd, l, ip, bf, out);
      break;
    case CMD_CTRL_CJMPnz: // CJMPnz  0x%.8x
      cmd = Cmd::CJMPnz;
      read_print_cmd_seq<use_out, ArgT::OFFSET>(cmd, l, ip, bf, out);
      break;

    case CMD_CTRL_BEGIN: // BEGIN %d %d // function begin
      cmd = Cmd::BEGIN;
      read_print_cmd_seq<use_out, ArgT::INT, ArgT::INT>(cmd, l, ip, bf, out);
      break;
    case CMD_CTRL_CBEGIN: // CBEGIN %d %d
      cmd = Cmd::CBEGIN;
      read_print_cmd_seq<use_out, ArgT::INT, ArgT::INT>(cmd, l, ip, bf, out);
      break;

    case CMD_CTRL_CLOSURE: { // CLOSURE 0x%.8x
      cmd = Cmd::CLOSURE;
      read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
      print_space<use_out>(out);
      size_t call_p = read_print_val<use_out, ArgT::OFFSET>(ip, bf, out);
      print_space<use_out>(out);
      size_t args_count = read_print_val<use_out, ArgT::INT>(ip, bf, out);
      for (size_t i = 0; i < args_count; i++) {
        uint8_t arg_type = ip_read_byte(ip, bf);
        if (arg_type > sizeof(ldts) / sizeof(char *)) {
          failure("wrong closure argument type");
        }
        print_space<use_out>(out);
        print_val<use_out>(out, ldts[arg_type]);
        print_space<use_out>(out);
        read_print_val<use_out, ArgT::INT>(ip, bf, out);
      }
      break;
    }

    case CMD_CTRL_CALLC: // CALLC %d // call clojure
      cmd = Cmd::CALLC;
      read_print_cmd_seq<use_out, ArgT::INT>(cmd, l, ip, bf, out);
      break;

    case CMD_CTRL_CALL: // CALL 0x%.8x %d // call function
      cmd = Cmd::CALL;
      read_print_cmd_seq<use_out, ArgT::OFFSET, ArgT::INT>(cmd, l, ip, bf, out);
      break;

    case CMD_CTRL_TAG: // TAG %s %d
      cmd = Cmd::TAG;
      read_print_cmd_seq<use_out, ArgT::STR, ArgT::INT>(cmd, l, ip, bf, out);
      break;
    case CMD_CTRL_FAIL: // FAIL %d %d
      cmd = Cmd::FAIL;
      read_print_cmd_seq<use_out, ArgT::INT, ArgT::INT>(cmd, l, ip, bf, out);
      break;

    case CMD_CTRL_ARRAY: // ARRAY %d
      cmd = Cmd::ARRAY;
      read_print_cmd_seq<use_out, ArgT::INT>(cmd, l, ip, bf, out);
      break;
    case CMD_CTRL_LINE: // LINE %d
      cmd = Cmd::LINE;
      read_print_cmd_seq<use_out, ArgT::INT>(cmd, l, ip, bf, out);
      break;

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
    read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
    break;

  case CMD_BUILTIN: {
    switch (l) {
    case CMD_BUILTIN_Lread: // CALL Lread
      cmd = Cmd::Lread;
      read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BUILTIN_Lwrite: // CALL Lwrite
      cmd = Cmd::Lwrite;
      read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BUILTIN_Llength: // CALL Llength
      cmd = Cmd::Llength;
      read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
      break;
    case CMD_BUILTIN_Lstring: // CALL Lstring
      cmd = Cmd::Lstring;
      read_print_cmd_seq<use_out>(cmd, l, ip, bf, out);
      break;

    case CMD_BUILTIN_Barray: // CALL Barray %d
      cmd = Cmd::Barray;
      read_print_cmd_seq<use_out, ArgT::INT>(cmd, l, ip, bf, out);
      break;

    default:
      failure("invalid opcode");
    }
  } break;

  default:
    failure("invalid opcode");
  }
#ifdef DEBUG_VERSION
  std::cout << command_name(cmd, l) << '\n';
#endif
  return {cmd, l};
}

std::pair<Cmd, uint8_t> parse_command(char **ip, const Bytefile &bf) {
  return parse_command_impl<false>(ip, bf, std::clog);
}

std::pair<Cmd, uint8_t> parse_command(char **ip, const Bytefile &bf,
                                      std::ostream &out) {
  return parse_command_impl<true>(ip, bf, out);
}
