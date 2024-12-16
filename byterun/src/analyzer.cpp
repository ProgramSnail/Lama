#include "analyzer.hpp"
#include "parser.hpp"
#include <iostream>
#include <limits>

extern "C" {
#include "types.h"
}

#include <vector>

void analyze(Bytefile *bf) {
  static constexpr const int NOT_VISITED = -1;
  std::vector<int> visited(bf->code_size, NOT_VISITED); // store stack depth

  std::vector<size_t> to_visit_func;
  std::vector<size_t> to_visit_jmp;

  int current_stack_depth = 0;
  const uint globals_count = bf->global_area_size;
  uint current_locals_count = 0;
  uint current_args_count = 0;
  bool is_in_closure = false;
  uint16_t *current_begin_counter = nullptr;
  int func_end_found = 0;

  char *ip = bf->code_ptr;
  char *current_ip = ip;
  char *saved_current_ip = current_ip;

  auto const jmp_to_visit_push = [&saved_current_ip, &bf, &visited,
                                  &current_stack_depth,
                                  &to_visit_jmp](size_t offset) {
    if (visited[offset] == NOT_VISITED) {
      visited[offset] = current_stack_depth;
      to_visit_jmp.push_back(offset);
    } else if (visited[offset] != current_stack_depth) {
      ip_failure(saved_current_ip, bf,
                 "different stack depth on same point is not allowed");
    }
  };

  auto const func_to_visit_push = [&saved_current_ip, &bf, &visited,
                                   &current_stack_depth,
                                   &to_visit_func](size_t offset) {
    if (visited[offset] == NOT_VISITED) {
      visited[offset] = 0;
      to_visit_func.push_back(offset);
    } else if (visited[offset] != 0) {
      ip_failure(saved_current_ip, bf,
                 "different stack depth on same point is not allowed");
    }
  };

  auto const check_correct_var = [&saved_current_ip, &bf, &globals_count,
                                  &current_locals_count, &current_args_count,
                                  &is_in_closure](uint8_t l, uint id) {
    if (l > 3) {
      ip_failure(saved_current_ip, bf, "unexpected variable category");
    }
    VarCategory category = to_var_category(l);
    switch (category) {
    case VAR_GLOBAL:
      if (id >= globals_count) {
        ip_failure(saved_current_ip, bf, "global var index is out of range");
      }
      break;
    case VAR_LOCAL:
      if (id >= current_locals_count) {
        ip_failure(saved_current_ip, bf, "local var index is out of range");
      }
      break;
    case VAR_ARGUMENT:
      if (id >= current_args_count) {
        ip_failure(saved_current_ip, bf, "argument var index is out of range");
      }
      break;
    case VAR_CLOSURE:
      if (!is_in_closure) {
        ip_failure(saved_current_ip, bf,
                   "can't access closure vars outside of closure");
      }
      // NOTE: impossible to properly check bounds there
      break;
    }
  };

  // add publics
  to_visit_func.reserve(bf->public_symbols_number);
  for (size_t i = 0; i < bf->public_symbols_number; ++i) {
    func_to_visit_push(get_public_offset_safe(bf, i));
  }

  if (to_visit_func.size() == 0) {
    failure("no public symbols detected");
  }

  while (true) {
    ip = bf->code_ptr;
    if (current_begin_counter == nullptr) {
      if (to_visit_func.empty()) {
        break;
      }
      ip += to_visit_func.back();
      to_visit_func.pop_back();
      current_stack_depth = 0;
      func_end_found = 0;
    } else {
      if (to_visit_jmp.empty()) {
        current_begin_counter = nullptr;
        if (func_end_found != 1) {
          failure("each function should have exactly one end");
        }
        continue;
      }
      ip += to_visit_jmp.back();
      to_visit_jmp.pop_back();
    }

    if (ip >= bf->code_ptr + bf->code_size) {
      ip_safe_failure(ip, bf, "instruction pointer is out of range (>= size)");
    }

    if (ip < bf->code_ptr) {
      ip_safe_failure(ip, bf, "instruction pointer is out of range (< 0)");
    }

    current_ip = ip;
    saved_current_ip = current_ip;

    // #ifdef DEBUG_VERSION
    const auto [cmd, l] = parse_command(&ip, bf, std::cout);
    // #else
    //     const auto [cmd, l] = parse_command(&ip, bf);
    // #endif

    if (current_begin_counter == nullptr && cmd != Cmd::BEGIN &&
        cmd != Cmd::CBEGIN) {
      ip_failure(saved_current_ip, bf, "function does not start with begin");
    }

    if (visited[current_ip - bf->code_ptr] == NOT_VISITED) {
      ip_failure(saved_current_ip, bf, "not visited command");
    }
    current_stack_depth = visited[current_ip - bf->code_ptr];

    // #ifdef DEBUG_VERSION
    std::cout << " -- [" << current_stack_depth << ']' << '\n';
    // #endif

    ++current_ip; // skip command byte

    size_t extra_stack_during_opr = 0;
    // change stack depth
    switch (cmd) {
    case Cmd::BINOP:
      current_stack_depth -= 2;
      if (current_stack_depth < 0) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      ++current_stack_depth;
      break;
    case Cmd::CONST:
      ++current_stack_depth;
      break;
    case Cmd::STRING:
      ++current_stack_depth;
      break;
    case Cmd::SEXP:
      ip_read_string_unsafe(&current_ip, bf);
      current_stack_depth -= ip_read_int_unsafe(&current_ip);
      if (current_stack_depth < 0) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      ++current_stack_depth;
      break;
    case Cmd::STI:
      current_stack_depth -= 2;
      if (current_stack_depth < 0) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      ++current_stack_depth;
      break;
    case Cmd::STA:
      current_stack_depth -= 3;
      if (current_stack_depth < 0) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      ++current_stack_depth;
      break;
    case Cmd::JMP:
      break;
    case Cmd::END:
      ++func_end_found;
      --current_stack_depth;
      break;
    case Cmd::RET:
      --current_stack_depth;
      break;
    case Cmd::DROP:
      --current_stack_depth;
      break;
    case Cmd::DUP:
      if (current_stack_depth < 1) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      ++current_stack_depth;
      break;
    case Cmd::SWAP:
      if (current_stack_depth < 2) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      break;
    case Cmd::ELEM:
      current_stack_depth -= 2;
      if (current_stack_depth < 0) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      ++current_stack_depth;
      break;
    case Cmd::LD:
      check_correct_var(l, ip_read_int_unsafe(&current_ip));
      ++current_stack_depth;
      break;
    case Cmd::LDA:
      check_correct_var(l, ip_read_int_unsafe(&current_ip));
      ++current_stack_depth;
      break;
    case Cmd::ST:
      check_correct_var(l, ip_read_int_unsafe(&current_ip));
      if (current_stack_depth < 1) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      break;
    case Cmd::CJMPz:
    case Cmd::CJMPnz:
      --current_stack_depth;
      break;
    case Cmd::BEGIN:
    case Cmd::CBEGIN:
      if (current_begin_counter != nullptr) {
        ip_failure(saved_current_ip, bf, "unexpected function beginning");
      }
      current_args_count = ip_read_int_unsafe(&current_ip);
      current_begin_counter = (uint16_t *)(current_ip + sizeof(uint16_t));
      current_locals_count = ip_read_int_unsafe(&current_ip);
      if (current_locals_count >= std::numeric_limits<uint16_t>::max()) {
        ip_failure(saved_current_ip, bf, "too many locals in functions");
      }
      (*(uint16_t *)(current_ip - sizeof(uint16_t))) = current_locals_count;
      *current_begin_counter = 0;
      is_in_closure = (cmd == Cmd::CBEGIN);
      break;
    case Cmd::CLOSURE: {
      uint closure_offset = ip_read_int_unsafe(&current_ip); // closure offset
      size_t args_count = ip_read_int_unsafe(&current_ip);   // args count
      extra_stack_during_opr = args_count;
      for (aint i = 0; i < args_count; i++) {
        aint arg_type = ip_read_byte_unsafe(&current_ip);
        aint arg_id = ip_read_int_unsafe(&current_ip);
        check_correct_var(arg_type, arg_id);
      }
      ++current_stack_depth;

      // if (closure_offset >= bf->code_size) {
      //   ip_failure(saved_current_ip, bf, "jump/call out of file");
      // }

      // NOTE: is not always true
      // if (!is_command_name(bf->code_ptr + closure_offset, bf, Cmd::CBEGIN)) {
      //   ip_failure(saved_current_ip, bf, "closure should point to cbegin");
      // }
    } break;
    case Cmd::CALLC: {
      uint args_count = ip_read_int_unsafe(&current_ip);
      current_stack_depth -= args_count + 1; // + closure itself
      if (current_stack_depth < 0) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      ++current_stack_depth;
      // NOTE: can't check args == cbegin args
    } break;
    case Cmd::CALL: {
      uint call_offset = ip_read_int_unsafe(&current_ip); // call offset
      uint args_count = ip_read_int_unsafe(&current_ip);
      current_stack_depth -= args_count;
      if (current_stack_depth < 0) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      ++current_stack_depth;

      if (call_offset >= bf->code_size) {
        ip_failure(saved_current_ip, bf, "jump/call out of file");
      }

      if (!is_command_name(bf->code_ptr + call_offset, bf, Cmd::BEGIN)) {
        ip_failure(saved_current_ip, bf, "call should point to begin");
      }
      if (args_count != *(uint *)(bf->code_ptr + call_offset + 1)) {
        ip_failure(saved_current_ip, bf, "wrong call argument count");
      }
    } break;
    case Cmd::TAG:
      if (current_stack_depth < 1) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      break;
    case Cmd::ARRAY:
      if (current_stack_depth < 1) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      break;
    case Cmd::FAIL:
      --current_stack_depth;
      break;
    case Cmd::LINE:
      break;
    case Cmd::PATT:
      --current_stack_depth;
      if (l == CMD_PATT_STR) {
        --current_stack_depth;
      }
      if (current_stack_depth < 0) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      ++current_stack_depth;
      break;
    case Cmd::Lread:
      ++current_stack_depth;
      break;
    case Cmd::Lwrite:
    case Cmd::Llength:
    case Cmd::Lstring:
      if (current_stack_depth < 1) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      break;
    case Cmd::Barray:
      current_stack_depth -= ip_read_int_unsafe(&current_ip); // elem count
      if (current_stack_depth < 0) {
        ip_failure(saved_current_ip, bf, "not enough elements in stack");
      }
      ++current_stack_depth;
      break;
    case Cmd::EXIT:
      ip_failure(saved_current_ip, bf,
                 "exit should be unreachable"); // NOTE: not sure
      break;
    case Cmd::_UNDEF_:
      ip_failure(saved_current_ip, bf, "undefined command");
      break;
    }

    if (current_begin_counter == nullptr) {
      ip_failure(saved_current_ip, bf, "function does not start with begin");
    }

    if (current_stack_depth < 0) {
      ip_failure(saved_current_ip, bf, "not enough elements in stack");
    }

    *current_begin_counter =
        std::max(*current_begin_counter,
                 (uint16_t)(current_stack_depth + extra_stack_during_opr));

    current_ip = saved_current_ip;
    ++current_ip; // skip command byte
    // do jumps
    switch (cmd) {
    case Cmd::EXIT:
    case Cmd::END:
    case Cmd::FAIL:
      break;

    case Cmd::CJMPz:
    case Cmd::CJMPnz:
    case Cmd::CLOSURE:
    case Cmd::CALL:
      jmp_to_visit_push(ip - bf->code_ptr);
    case Cmd::JMP: {
      bool is_call = (cmd == Cmd::CLOSURE || cmd == Cmd::CALL);

      uint jmp_p = ip_read_int_unsafe(&current_ip);
      if (jmp_p >= bf->code_size) {
        // NOTE: maybe also should check that > begin (?)
        ip_failure(saved_current_ip, bf, "jump/call out of file");
      }
      if (is_call) {
        func_to_visit_push(jmp_p);
      } else {
        jmp_to_visit_push(jmp_p);
      }
      break;
    }

    case Cmd::_UNDEF_:
      ip_failure(saved_current_ip, bf, "undefined command");
      break;

    default:
      jmp_to_visit_push(ip - bf->code_ptr);
      break;
    }
  }
}
