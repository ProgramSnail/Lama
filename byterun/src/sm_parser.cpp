#include "sm_parser.hpp"

#include <any>
#include <charconv>
#include <iostream>
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

std::string substr_to(const std::string &line, size_t &pos, char to) {
  auto offset = line.find(pos, to);

  if (offset == std::string::npos) {
    return "";
  };

  std::string result = line.substr(pos, offset);
  pos += offset + 1;

  return result;
}

// TODO: parsers + combinators

// parse_str
// parse_int
// parse_bool
// parse_opr
// parse_patt
// parse_var

// parse_array
// parse_scope

// parse_or

struct SMInstrBuilder {
public:
  SMInstrBuilder(SMInstr instr) : instr(instr) {}

  std::optional<SMInstr> build() {
    // TODO: check too many args ??
    try {
      // TODO: check for all args present
      return {std::visit<SMInstr>( //
          utils::multifunc{
              //
              [&args = args](SMInstr::PUBLIC x) -> SMInstr {
                x.name = std::any_cast<std::string>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::EXTERN x) -> SMInstr {
                x.name = std::any_cast<std::string>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::IMPORT x) -> SMInstr {
                x.name = std::any_cast<std::string>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::CLOSURE x) -> SMInstr {
                x.name = std::any_cast<int>(args.at(0));
                x.closure = std::any_cast<std::vector<ValT>>(args.at(1));
                return x;
              },
              [&args = args](SMInstr::CONST x) -> SMInstr {
                x.n = std::any_cast<int>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::STRING x) -> SMInstr {
                x.str = std::any_cast<std::string>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::LDA x) -> SMInstr {
                x.v = std::any_cast<ValT>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::LD x) -> SMInstr {
                x.v = std::any_cast<ValT>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::ST x) -> SMInstr {
                x.v = std::any_cast<ValT>(args.at(0));
                return x;
              },
              [](SMInstr::STA x) -> SMInstr { return x; },
              [](SMInstr::STI x) -> SMInstr { return x; },
              [&args = args](SMInstr::BINOP x) -> SMInstr {
                x.opr = std::any_cast<Opr>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::LABEL x) -> SMInstr {
                x.s = std::any_cast<std::string>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::FLABEL x) -> SMInstr {
                x.s = std::any_cast<std::string>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::SLABEL x) -> SMInstr {
                x.s = std::any_cast<std::string>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::JMP x) -> SMInstr {
                x.l = std::any_cast<std::string>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::CJMP x) -> SMInstr {
                x.s = std::any_cast<std::string>(args.at(0));
                x.l = std::any_cast<std::string>(args.at(1));
                return x;
              },
              [&args = args](SMInstr::BEGIN x) -> SMInstr {
                x.f = std::any_cast<std::string>(args.at(0));
                x.nargs = std::any_cast<int>(args.at(1));
                x.nlocals = std::any_cast<int>(args.at(2));
                x.closure = std::any_cast<std::vector<ValT>>(args.at(3));
                x.args = std::any_cast<std::vector<std::string>>(args.at(4));
                x.scopes = std::any_cast<std::vector<Scope>>(args.at(5));
                return x;
              },
              [](SMInstr::END x) -> SMInstr { return x; },
              [](SMInstr::RET x) -> SMInstr { return x; },
              [](SMInstr::ELEM x) -> SMInstr { return x; },
              [&args = args](SMInstr::CALL x) -> SMInstr {
                x.fname = std::any_cast<std::string>(args.at(0));
                x.n = std::any_cast<int>(args.at(1));
                x.tail = std::any_cast<bool>(args.at(2));
                return x;
              },
              [&args = args](SMInstr::CALLC x) -> SMInstr {
                x.n = std::any_cast<int>(args.at(1));
                x.tail = std::any_cast<bool>(args.at(2));
                return x;
              },
              [&args = args](SMInstr::SEXP x) -> SMInstr {
                x.tag = std::any_cast<std::string>(args.at(0));
                x.n = std::any_cast<int>(args.at(1));
                return x;
              },
              [](SMInstr::DROP x) -> SMInstr { return x; },
              [](SMInstr::DUP x) -> SMInstr { return x; },
              [](SMInstr::SWAP x) -> SMInstr { return x; },
              [&args = args](SMInstr::TAG x) -> SMInstr {
                x.tag = std::any_cast<std::string>(args.at(0));
                x.n = std::any_cast<int>(args.at(1));
                return x;
              },
              [&args = args](SMInstr::ARRAY x) -> SMInstr {
                x.n = std::any_cast<int>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::PATT x) -> SMInstr {
                x.patt = std::any_cast<Patt>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::LINE x) -> SMInstr {
                x.n = std::any_cast<int>(args.at(0));
                return x;
              },
              [&args = args](SMInstr::FAIL x) -> SMInstr {
                x.line = std::any_cast<int>(args.at(0));
                x.col = std::any_cast<int>(args.at(1));
                x.val = std::any_cast<bool>(args.at(2));
                return x;
              },
              // [](auto) -> SMInstr {
              //   throw std::bad_any_cast{}; // create another error ?
              // },
          },
          *instr)};
    } catch (const std::bad_any_cast &) {
      return {};
    } catch (const std::out_of_range &) {
      return {};
    }
  }

  template <typename T> void push_arg(T &&value) { args.emplace_back(value); }

private:
  SMInstr instr;
  std::vector<std::any> args;
};

std::optional<SMInstr> parse_sm(const std::string &line) {
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

  // TODO: Automatically parse any structures with parser combinators
  // if (cmd == "BEGIN") {
  //   // TODO: BEGIN
  // } else {

  //   bool was_last_arg = false;
  //   while (!was_last_arg) {
  //     std::string arg = substr_to(line, pos, '(');
  //     ++pos;

  //     if (arg.empty()) {
  //       arg = line.substr(pos);
  //       arg.pop_back(); // ')'
  //       was_last_arg = true;
  //     }

  //     if (arg.front() == '"') {
  //       instr.push_arg(arg.substr(1, arg.size() - 2));
  //     } else if (arg.front() == '[') {
  //       // TODO: parse array
  //       instr.push_arg(arg.substr(1, arg.size() - 2));
  //     } else if (arg == "true") {
  //       instr.push_arg(true);
  //     } else if (arg == "false") {
  //       instr.push_arg(false);
  //     } else if (auto maybe_var = parse_var(arg); maybe_var) {
  //       instr.push_arg(*maybe_var);
  //     } else if () { // TODO: CLUSURE vector
  //     } else if (auto maybe_patt = parse_patt(arg); maybe_patt) {
  //       instr.push_arg(*maybe_patt);
  //     } else if (int n = 0; std::from_chars(line.data() + pos,
  //                                           line.data() + pos + line.size(),
  //                                           n)
  //                               .ec == std::errc{}) {
  //       instr.push_arg(n);
  //     } else {
  //       return std::nullopt;
  //     }
  //   }
  // }

  return instr.build();
}
