#include "sm_parser.hpp"

#include <charconv>
#include <iostream>

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

std::optional<SMInstr> parse_sm(const std::string &line) {}
