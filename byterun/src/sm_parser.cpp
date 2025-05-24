#include "sm_parser.hpp"

#include <algorithm>
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

// throws std::bad_any_cast
template <typename T> std::vector<T> any_array_cast(std::any v) {
  auto values = std::any_cast<std::vector<std::any>>(std::move(v));

  std::vector<T> res;

  std::ranges::transform(
      values, std::back_inserter(res),
      [](const auto &value) { return std::any_cast<T>(value); });

  return res;
}

struct ParsingResult {
  std::any value;
  std::string_view rest;
};

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

//

template <typename T> using Matches = std::vector<std::pair<std::string, T>>;

// NOTE: prefix matching can be done better (but probably such performance is
// not required here)
template <typename T>
ParsingResult prefix_matcher(std::string_view s, const Matches<T> &values) {
  for (auto &value : values) {
    if (s.substr(0, value.first.size()) == value.first) {
      return {value.second, s.substr(value.first.size())};
    }
  }

  return {{}, s};
}

ParsingResult parse_any_val(std::string_view s);

ParsingResult parse_str(std::string_view s) {
  if (s.size() < 2 || s.front() != '"') {
    return {{}, s};
  }

  size_t end = 1; // skip front
  for (; end < s.size(); ++end) {
    if (s[end] == '\\') {
      ++end;
      continue;
    }

    if (s[end] == '\"') {
      break;
    }
  }

  return {std::string{s.substr(1, end - 1)}, s.substr(end + 1)};
}

ParsingResult parse_int(std::string_view s) {
  int value = 0;

  auto res = std::from_chars(s.data(), s.data() + s.size(), value);

  if (res.ec == std::errc{}) {
    return {{}, s};
  }

  return {value, s.substr(res.ptr - s.data())};
}

ParsingResult parse_bool(std::string_view s) {
  static const Matches<bool> bools = {{"true", true}, {"false", false}};
  return prefix_matcher(s, bools);
}

ParsingResult parse_opr(std::string_view s) {
  static const Matches<Opr> oprs = {
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
  return prefix_matcher(s, oprs);
}

ParsingResult parse_patt(std::string_view s) {
  static const Matches<Patt> patts = {
      {"Boxed", Patt::BOXED},   {"UnBoxed", Patt::UNBOXED},
      {"Array", Patt::ARRAY},   {"String", Patt::STRING},
      {"SExp", Patt::SEXP},     {"Closure", Patt::CLOSURE},
      {"StrCmp", Patt::STRCMP},
  }; // TODO: check
  return prefix_matcher(s, patts);
}

// ---

ParsingResult parse_var(std::string_view s) {
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
  auto arg_it = vars.find(arg_str);
  if (arg_it == vars.end()) {
    return {{}, s};
  }
  ++pos; // '('

  // NOTE: s_rest starts with ')'
  auto [id, s_rest] = parse_any_val(s.substr(pos));
  if (not id.has_value()) {
    return {{}, s};
  }

  try {
    return {arg_it->second(std::move(id)), s_rest.substr(1)}; // skip ')'
  } catch (const std::bad_any_cast &) {
    return {{}, s};
  }
}

// (_, _)
ParsingResult parse_pair(std::string_view s) {
  if (s.size() < 2 || s.front() != '(') {
    return {};
  }

  ParsingResult first_elem = parse_any_val(s.substr(1)); // skip '('
  ParsingResult second_elem =
      parse_any_val(first_elem.rest.substr(2)); // skip ', '

  return {std::pair<std::any, std::any>{first_elem, second_elem},
          second_elem.rest.substr(1)}; // skip ')'
}

// [_, ..., _]
ParsingResult parse_array(std::string_view s, char first_symbol = '[') {
  if (s.size() < 2 || s.front() != first_symbol) {
    return {};
  }

  std::vector<std::any> values;
  ParsingResult res{{}, s.substr(1)}; // skip '[' (first_symbol)

  while (not s.empty()) {
    res = parse_any_val(res.rest);

    if (not res.value.has_value()) {
      return {{}, s};
    }

    values.push_back(std::move(res.value));
    res.value = {};                // do not use moved value
    res.rest = res.rest.substr(1); // skip ',' (or ']' at the end)
  }

  return {values, res.rest};
}

// { blab="_"; elab="_" names=[...]; subs=[...]}
ParsingResult parse_scope(std::string_view s) {
  if (s.size() < 2 || s.front() != '{') {
    return {};
  }

  Scope scope;
  ParsingResult res{{}, s.substr(1)}; // skip '{'

  try {
    { // blab
      size_t pos = 0;
      substr_to(res.rest, pos, '=');
      res = parse_str(res.rest.substr(pos));
      scope.blab = std::any_cast<std::string>(res.value);
    }

    { // elab
      size_t pos = 0;
      substr_to(res.rest, pos, '=');
      res = parse_str(res.rest.substr(pos));
      scope.elab = std::any_cast<std::string>(res.value);
    }

    { // names
      size_t pos = 0;
      substr_to(res.rest, pos, '=');
      res = parse_array(res.rest.substr(pos));

      auto names =
          any_array_cast<std::pair<std::any, std::any>>(std::move(res.value));
      res.value = {}; // do not use moved value
      std::ranges::transform(names, std::back_inserter(scope.names),
                             [](const auto &name) {
                               return std::pair<std::string, int>{
                                   std::any_cast<std::string>(name.first),
                                   std::any_cast<int>(name.second)};
                             });
    }

    { // subs
      size_t pos = 0;
      substr_to(res.rest, pos, '=');
      res = parse_array(res.rest.substr(pos));
      scope.subs = any_array_cast<Scope>(std::move(res.value));
      res.value = {}; // do not use moved vlue
    }

    return {scope, res.rest.substr(1)}; // skip '}'
  } catch (const std::bad_any_cast &) {
    return {{}, s};
  }
}

ParsingResult parse_any_val(std::string_view s) {
  ParsingResult res;

  if (res = parse_str(s); res.value.has_value()) {
    return res;
  }
  if (res = parse_int(s); res.value.has_value()) {
    return res;
  }
  if (res = parse_bool(s); res.value.has_value()) {
    return res;
  }
  if (res = parse_opr(s); res.value.has_value()) {
    return res;
  }
  if (res = parse_patt(s); res.value.has_value()) {
    return res;
  }
  if (res = parse_var(s); res.value.has_value()) {
    return res;
  }
  if (res = parse_array(s); res.value.has_value()) {
    return res;
  }
  if (res = parse_scope(s); res.value.has_value()) {
    return res;
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

  // (_, ..., _) - args
  ParsingResult args_res = parse_array({line.data(), line.data() + pos});

  try {
    auto args = std::any_cast<std::vector<std::any>>(std::move(args_res.value));
    args_res.value = {};

    if (not args_res.rest.empty()) {
      return std::nullopt;
    }

    // TODO: put all array at once
    for (auto &&arg : args) {
      instr.push_arg(arg);
    }
    args = {};
  } catch (const std::bad_any_cast &) {
    return std::nullopt;
  }

  return instr.build();
}
