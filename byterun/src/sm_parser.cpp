#include "sm_parser.hpp"

#include <charconv>
#include <iostream>
#include <sstream>
#include <unordered_map>

std::vector<SMInstr> parse_sm(std::istream &in) {
  std::vector<SMInstr> result;

  for (size_t i = 1; !in.eof(); ++i) {
    if (in.fail()) {
      std::cerr << "line " << i << ": input failure";
      break;
    }
    std::string instr_str;
    std::getline(in, instr_str);
    auto instr = parse_sm(instr_str);

    if (!instr) {
      std::cerr << "line " << i << ": instr parsing failure";
      break;
    }

    result.push_back(std::move(instr.value()));
  }

  return result;
}

std::optional<std::pair<size_t, std::string::iterator>>
parse(std::string::iterator begin, std::string::iterator end) {
  size_t result = 0;
  std::from_chars(&*begin, &*end, result);
  // TODO
}

std::string substr_to(const std::string &line, size_t &pos, char to) {
  auto offset = line.find(pos, to);

  if (offset == std::string::npos) {
    return "";
  };

  std::string result = line.substr(pos, offset);
  pos += offset + 1;

  return result;
}

struct SMInstrBuilder {
public:
  SMInstrBuilder(SMInstr instr) : instr(instr), args_pushed(0) {}

  SMInstr build() {
    // TODO: check for all arps
    return instr;
  }

  // TODO
  void push_arg(std::string &&value) {}
  void push_arg(bool value) {}
  void push_arg(int value) {}
  void push_arg(ValT value) {}
  void push_arg(Patt value) {}

private:
  SMInstr instr;
  size_t args_pushed = 0;
}

std::optional<SMInstr>
parse_sm(const std::string &line) {
  std::unordered_map<std::string, SMInstr> to_instr = {
      {"BINOP", SMInstr{SMInstr::BINOP{}}},
      {"CONST", SMInstr{SMInstr::CONST{}}},
      {"STRING", SMInstr{SMInstr::STRING{}}},
      {"SEXP", SMInstr{SMInstr::SEXP{}}},
      {"LD", SMInstr{SMInstr::LD{ValT::Global{}}}},   // NOTE: as default
      {"LDA", SMInstr{SMInstr::LDA{ValT::Global{}}}}, // NOTE: as default
      {"ST", SMInstr{SMInstr::ST{ValT::Global{}}}},   // NOTE: as default
      {"STI", SMInstr{SMInstr::STI{}}},
      {"STA", SMInstr{SMInstr::STA{}}},
      {"ELEM", SMInstr{SMInstr::ELEM{}}},
      {"LABEL", SMInstr{SMInstr::LABEL{}}},
      {"FLABEL", SMInstr{SMInstr::FLABEL{}}},
      {"SLABEL", SMInstr{SMInstr::SLABEL{}}},
      {"JMP", SMInstr{SMInstr::JMP{}}},
      {"CJMP", SMInstr{SMInstr::CJMP{}}},
      {"BEGIN", SMInstr{SMInstr::BEGIN{}}},
      {"END", SMInstr{SMInstr::END{}}},
      {"CLOSURE", SMInstr{SMInstr::CLOSURE{}}},
      {"CALLC", SMInstr{SMInstr::CALLC{}}},
      {"CALL", SMInstr{SMInstr::CALL{}}},
      {"RET", SMInstr{SMInstr::RET{}}},
      {"DROP", SMInstr{SMInstr::DROP{}}},
      {"DUP", SMInstr{SMInstr::DUP{}}},
      {"SWAP", SMInstr{SMInstr::SWAP{}}},
      {"TAG", SMInstr{SMInstr::TAG{}}},
      {"ARRAY", SMInstr{SMInstr::ARRAY{}}},
      {"PATT", SMInstr{SMInstr::PATT{}}},
      {"FAIL", SMInstr{SMInstr::FAIL{}}},
      {"EXTERN", SMInstr{SMInstr::EXTERN{}}},
      {"PUBLIC", SMInstr{SMInstr::PUBLIC{}}},
      {"IMPORT", SMInstr{SMInstr::IMPORT{}}},
      {"LINE", SMInstr{SMInstr::LINE{}}},
  };

  char ch; // NOTE: for helpers

  size_t pos = 0;
  std::string cmd = substr_to(line, pos, ' ');

  auto instr_it = to_instr.find(cmd);
  if (instr_it == to_instr.end()) {
    return std::nullopt;
  }
  SMInstrBuilder instr{instr_it->second};

  // no args case
  if (pos == line.size()) {
    return instr.build();
  }

  if (std::string space = substr_to(line, pos, '('); space != " ") {
    return std::nullopt;
  }

  bool was_last_arg = false;
  while (!was_last_arg) {
    std::string arg = substr_to(line, pos, '(');
    ++pos;

    if (arg.empty()) {
      arg = line.substr(pos);
      arg.pop_back(); // ')'
      was_last_arg = true;
    }

    if (arg.front() == '"') {
      instr.push_arg(arg.substr(1, arg.size() - 2));
    } else if (arg == "true") {
      instr.push_arg(true);
    } else if (arg == "false") {
      instr.push_arg(false);
    } else if () { // TODO: Local, Global, Arg
    } else if () { // TODO: BEGIN vectors, etc
    } else if () { // TODO: PATT patterns
    } else if () { // TODO: CLUSURE vector
    } else if (int n = 0; std::from_chars(line.data() + pos,
                                          line.data() + pos + line.size(), n)
                              .ec == std::errc{}) {
      instr.push_arg(n);
    } else {
      return std::nullopt;
    }
  }

  return instr.build();
}
