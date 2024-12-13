#include "analyzer.hpp"
#include "parser.hpp"

#include <vector>

// TODO
void analyze(const Bytefile &bf) {
  std::vector<size_t> to_visit;
  std::vector<bool> visited(bf.code_size, false);
  std::vector<bool> control_flow_in(bf.code_size, false);

  // TODO: 2 control flow sets, for functions and for control flow inside
  // functions
  // + current stack depth
  // + ast begin pos

  auto const to_visit_push = [&visited, &to_visit](size_t offset) {
    if (!visited[offset]) {
      visited[offset] = true;
      to_visit.push_back(offset);
    }
  };

  auto const control_push = [&to_visit_push, &control_flow_in](size_t offset) {
    control_flow_in[offset] = true;
    to_visit_push(offset);
  };

  // add publics
  to_visit.reserve(bf.public_symbols_number);
  for (size_t i = 0; i < bf.public_symbols_number; ++i) {
    control_push(get_public_offset(&bf, i));
  }

  if (to_visit.size() == 0) {
    failure("no public symbols detected");
  }

  while (!to_visit.empty()) {
    char *ip = bf.code_ptr + to_visit.back();
    to_visit.pop_back();

    if (ip >= bf.code_ptr + bf.code_size) {
      failure("instruction pointer is out of range (>= size)");
    }

    if (ip < bf.code_ptr) {
      failure("instruction pointer is out of range (< 0)");
    }

    char *current_ip = ip;

#ifdef DEBUG_VERSION
    printf("0x%.8lx \n", current_ip - bf.code_ptr - 1);
#endif

    Cmd cmd = parse_command(&ip, bf);

    ++current_ip; // skip command byte
    switch (cmd) {
    case Cmd::EXIT:
    case Cmd::END:
      break;

    case Cmd::CJMPz:
    case Cmd::CJMPnz:
    case Cmd::CLOSURE:
    case Cmd::CALL:
      to_visit_push(ip - bf.code_ptr);
    case Cmd::JMP: {
      uint jmp_p = ip_read_int(&current_ip, bf);
      if (jmp_p >= bf.code_size) {
        failure("jump/call out of file");
      }
      control_push(jmp_p);
      break;
    }

    case Cmd::_UNDEF_:
      failure("undefined command");
      break;

    default:
      to_visit_push(ip - bf.code_ptr);
      break;
    }
  }
}
