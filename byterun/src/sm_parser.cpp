#include "sm_parser.hpp"

#include <any>
#include <charconv>
#include <functional>
#include <iostream>
#include <map>
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

//

struct EmptyArrayTok {};

template <typename T> std::vector<T> any_array_cast(const std::any &v) {
  if (v.type().name() == typeid(EmptyArrayTok).name()) {
    return {};
  }

  return std::any_cast<std::vector<T>>(v);
}

//

std::string_view substr_to(const std::string_view line, size_t &pos, char to) {
  auto offset = line.find(pos, to);

  if (offset == std::string::npos) {
    return "";
  };

  std::string_view result = line.substr(pos, offset);
  pos += offset + 1;

  return result;
}

// TODO: parsers + combinators

std::any parse_any_val(std::string_view s);

std::any parse_str(std::string_view s) {
  if (s.size() < 2 || s.front() != '"') {
    return {};
  }
  return s.substr(1, s.size() - 2);
}

std::any parse_int(std::string_view s) {
  int n = 0;
  if (std::from_chars(s.data(), s.data() + s.size(), n).ec != std::errc{}) {
    return {};
  }

  return n;
}
std::any parse_bool(std::string_view s) {
  if (s == "true") {
    return true;
  } else if (s == "false") {
    return false;
  }

  return {};
}

std::any parse_opr(std::string_view s) {
  static const std::map<std::string, Opr, std::less<>> oprs = {
      {"+", Opr::ADD},  // +
      {"-", Opr::SUB},  // -
      {"*", Opr::MULT}, // *
      {"/", Opr::DIV},  // /
      {"%", Opr::MOD},  // %
      {"<=", Opr::LEQ}, // <=
      {"<", Opr::LT},   // <
      {">", Opr::GT},   // >
      {">=", Opr::GEQ}, // >=
      {"==", Opr::EQ},  // ==
      {"!=", Opr::NEQ}, // !=
      {"&&", Opr::AND}, // &&
      {"!!", Opr::OR},  // !!
  }; // TODO: check format: cpp vs lama

  auto it = oprs.find(s);

  if (it != oprs.end()) {
    return it->second;
  }

  return {};
}

std::any parse_patt(std::string_view s) {
  static const std::map<std::string, Patt, std::less<>> patts = {
      {"Boxed", Patt::BOXED},   {"UnBoxed", Patt::UNBOXED},
      {"Array", Patt::ARRAY},   {"String", Patt::STRING},
      {"SExp", Patt::SEXP},     {"Closure", Patt::CLOSURE},
      {"StrCmp", Patt::STRCMP},
  }; // TODO: check

  auto it = patts.find(s);

  if (it != patts.end()) {
    return it->second;
  }

  return {};
}

std::any parse_var(std::string_view s) {
  static const std::map<std::string, std::function<ValT(std::any &&)>,
                        std::less<>>
      vars = {
          {"Arg",
           [](std::any &&n) {
             return ValT::Arg{size_t(std::any_cast<int>(n))};
           }},
          {"Local",
           [](std::any &&n) {
             return ValT::Local{size_t(std::any_cast<int>(n))};
           }},
          {"Global",
           [](std::any &&s) {
             return ValT::Global{std::any_cast<std::string>(std::move(s))};
           }},
          {"Access",
           [](std::any &&n) {
             return ValT::Access{size_t(std::any_cast<int>(n))};
           }},
          {"Fun",
           [](std::any &&s) {
             return ValT::Fun{std::any_cast<std::string>(std::move(s))};
           }},
      }; // TODO: check

  size_t pos = 0;
  auto arg_str = std::string{substr_to(s, pos, ' ')};
  if (arg_str.empty()) {
    return {};
  }
  ++pos; // '('

  if (s.size() <= pos + 1) {
    return {};
  }

  auto id_str = s.substr(pos, s.size() - pos - 1);

  auto arg_it = vars.find(arg_str);

  std::any id = parse_any_val(id_str);
  if (not id.has_value()) {
    return {};
  }

  if (arg_it != vars.end()) {
    try {
      return arg_it->second(std::move(id));
    } catch (const std::bad_any_cast &) {
      return {};
    }
  }

  return {};
}

// (_, _)
std::any parse_pair(std::string_view s) { // TODO
  if (s.size() < 2 || s.front() != '(') {
    return {};
  }

  // TODO: duduce tokens ends in parsers to find next entity
}

// [_, ..., _]
std::any parse_array(std::string_view s) { // TODO
  if (s.size() < 2 || s.front() != '[') {
    return {};
  }

  // TODO: deal with empty array

  // TODO: duduce tokens ends in parsers to find next entity
}

// { blab="_"; elab="_" names=[...]; subs=[...]}
std::any parse_scope(std::string_view s) {
  if (s.size() < 2 || s.front() != '{') {
    return {};
  }

  Scope scope;

  size_t pos = 0;
  // NOTE: expect no ';' in labels and names

  // blab
  substr_to(s, pos, '=');
  auto blab_str = std::string{substr_to(s, pos, ';')};
  if (blab_str.empty()) {
    return {};
  }

  // elab
  substr_to(s, pos, '=');
  auto elab_str = std::string{substr_to(s, pos, ';')};
  if (elab_str.empty()) {
    return {};
  }

  // names
  substr_to(s, pos, '=');
  auto names_str = std::string{substr_to(s, pos, ';')};
  if (names_str.empty()) {
    return {};
  }

  // subs
  substr_to(s, pos, '=');
  auto subs_str = std::string{s.substr(pos, s.size() - pos - 1)};
  if (subs_str.empty()) {
    return {};
  }

  try {
    scope.blab = std::any_cast<std::string>(parse_str(blab_str));
    scope.elab = std::any_cast<std::string>(parse_str(elab_str));
    scope.names =
        any_array_cast<std::pair<std::string, int>>(parse_array(names_str));
    scope.subs = any_array_cast<Scope>(parse_array(subs_str));
  } catch (const std::bad_any_cast &) {
    return {};
  }

  return scope;
} // TODO

std::any parse_any_val(std::string_view s) {
  std::any val;

  if (val = parse_str(s); val.has_value()) {
    return val;
  }
  if (val = parse_int(s); val.has_value()) {
    return val;
  }
  if (val = parse_bool(s); val.has_value()) {
    return val;
  }
  if (val = parse_opr(s); val.has_value()) {
    return val;
  }
  if (val = parse_patt(s); val.has_value()) {
    return val;
  }
  if (val = parse_var(s); val.has_value()) {
    return val;
  }
  if (val = parse_array(s); val.has_value()) {
    return val;
  }
  if (val = parse_scope(s); val.has_value()) {
    return val;
  }

  return {};
}

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
                x.closure = any_array_cast<ValT>(args.at(1));
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
                x.closure = any_array_cast<ValT>(args.at(3));
                x.args = any_array_cast<std::string>(args.at(4));
                x.scopes = any_array_cast<Scope>(args.at(5));
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
  auto cmd = std::string{substr_to(line, pos, ' ')};

  auto instr_it = to_instr.find(cmd);
  if (instr_it == to_instr.end()) {
    return std::nullopt;
  }
  SMInstrBuilder instr{instr_it->second};

  // no args case
  if (pos == line.size()) {
    return instr.build();
  }

  // NOTE: do not check for valid input
  // if (auto space = std::string{substr_to(line, pos, '(')}; space != " ") {
  //   return std::nullopt;
  // }

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
  //                                           line.data() + pos +
  //                                           line.size(), n)
  //                               .ec == std::errc{}) {
  //       instr.push_arg(n);
  //     } else {
  //       return std::nullopt;
  //     }
  //   }
  // }

  return instr.build();
}
