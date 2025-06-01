#include "sm_parser.hpp"

#include <algorithm>
#include <any>
#include <charconv>
#include <functional>
#include <iostream>
#include <map>
#include <unordered_map>

using Result = utils::Result<SMInstr, std::string>;

std::vector<SMInstr> parse_sm(std::istream &in) {
  std::vector<SMInstr> result;

  for (size_t i = 1; !in.eof(); ++i) {
    if (in.fail()) {
      std::cerr << "line " << i << ": input failure\n";
      break;
    }
    std::string instr_str;
    std::getline(in, instr_str);

    std::cout << "line: <" << instr_str << ">\n";
    if (instr_str.empty()) {
      continue;
    }
    auto instr = parse_sm(instr_str);

    if (instr.failed()) {
      std::cerr << "line " << i << ": instr parsing failure: <"
                << instr.get_error() << ">\n";
      break;
    }

    result.push_back(std::move(instr.get_value()));
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

std::string_view trim(std::string_view str) {
  std::cout << __func__ << '\n';
  auto begin = str.begin();
  for (; begin != str.end() && *begin == ' '; ++begin) {
  }

  auto end = str.end();
  for (; end > begin && *std::prev(end) == ' '; --end) {
  }

  return {begin, end};
}

std::string_view substr_to(std::string_view line, size_t &pos, char to) {
  std::cout << __func__ << " with " << line.substr(pos) << '\n';

  auto offset = line.find(to, pos);

  if (offset == std::string::npos) {
    std::cout << "value \"\"\n";
    return "";
  };

  std::string_view result = line.substr(pos, offset);
  pos += offset + 1;

  std::cout << "value " << result << "\n";
  return result;
}

//

template <typename T> using Matches = std::vector<std::pair<std::string, T>>;

// NOTE: prefix matching can be done better (but probably such performance is
// not required here)
template <typename T>
ParsingResult prefix_matcher(std::string_view s, const Matches<T> &values) {
  std::cout << __func__ << '\n';
  for (auto &value : values) {
    if (s.substr(0, value.first.size()) == value.first) {
      return {value.second, s.substr(value.first.size())};
    }
  }

  std::cout << "can't parse prefix from " << s << '\n';
  return {{}, s};
}

ParsingResult parse_any_val(std::string_view s);

ParsingResult parse_str(std::string_view s) {
  std::cout << __func__ << '\n';
  if (s.size() < 2 || s.front() != '"') {
    std::cout << "can't parse string from " << s << '\n';
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
  std::cout << __func__ << '\n';
  int value = 0;

  auto res = std::from_chars(s.data(), s.data() + s.size(), value);

  if (res.ec != std::errc{}) {
    std::cout << "can't parse int from " << s << '\n';
    return {{}, s};
  }

  return {value, s.substr(res.ptr - s.data())};
}

ParsingResult parse_bool(std::string_view s) {
  std::cout << __func__ << '\n';
  static const Matches<bool> bools = {{"true", true}, {"false", false}};
  return prefix_matcher(s, bools);
}

ParsingResult parse_opr(std::string_view s) {
  std::cout << __func__ << '\n';
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

Opr any_opr_cast(std::any value) {
  auto res = parse_opr(std::any_cast<std::string>(std::move(value)));
  if (!res.value.has_value()) {
    throw std::bad_any_cast{};
  }
  return std::any_cast<Opr>(std::move(res.value));
}

ParsingResult parse_patt(std::string_view s) {
  std::cout << __func__ << '\n';
  static const Matches<Patt> patts = {
      {"Boxed", Patt::BOXED},   {"UnBoxed", Patt::UNBOXED},
      {"Array", Patt::ARRAY},   {"String", Patt::STRING},
      {"SExp", Patt::SEXP},     {"Closure", Patt::CLOSURE},
      {"StrCmp", Patt::STRCMP},
  }; // TODO: check
  return prefix_matcher(s, patts);
}

Patt any_patt_cast(std::any value) {
  auto res = parse_patt(std::any_cast<std::string>(std::move(value)));
  if (!res.value.has_value()) {
    throw std::bad_any_cast{};
  }
  return std::any_cast<Patt>(std::move(res.value));
}

// ---

ParsingResult parse_var(std::string_view s) {
  std::cout << __func__ << '\n';
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
    std::cout << "can't parse var from " << s << '\n';
    return {{}, s};
  }
  ++pos; // '('

  // NOTE: s_rest starts with ')'
  auto [id, s_rest] = parse_any_val(s.substr(pos));
  if (not id.has_value()) {
    std::cout << "any val: can't parse int from " << s << '\n';
    return {{}, s};
  }

  try {
    return {arg_it->second(std::move(id)), s_rest.substr(1)}; // skip ')'
  } catch (const std::bad_any_cast &) {
    std::cout << "bad any cast: can't parse var from " << s << '\n';
    return {{}, s};
  }
}

// (_, _)
ParsingResult parse_pair(std::string_view s) {
  std::cout << __func__ << '\n';
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
ParsingResult parse_array(std::string_view s, char first_symbol = '[',
                          char last_symbol = ']') {
  std::cout << __func__ << '\n';
  if (s.size() < 2 || s.front() != first_symbol) {
    std::cout << "can't parse array from " << s << '\n';
    return {};
  }

  std::vector<std::any> values;
  ParsingResult res{{}, s.substr(1)}; // skip first_symbol
  while (true) {
    if (res.rest.front() == ',' /*in regular arrays*/ ||
        res.rest.front() == ';' /*in scopes*/) {
      res.rest = res.rest.substr(2); // skip ', '
    } else if (res.rest.front() == last_symbol) {
      break;
    }

    res = parse_any_val(res.rest);

    if (not res.value.has_value()) {
      std::cout << "can't parse array elem from " << s << '\n';
      return {{}, s};
    }

    values.push_back(std::move(res.value));
    res.value = {}; // do not use moved value
  }

  return {values, res.rest.substr(1)};
}

// { blab="_"; elab="_" names=[...]; subs=[...]}
ParsingResult parse_scope(std::string_view s) {
  std::cout << __func__ << '\n';
  if (s.size() < 2 || s.front() != '{') {
    std::cout << "can't parse scope from " << s << '\n';
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

    return {scope, res.rest.substr(3)}; // skip '; }'
  } catch (const std::bad_any_cast &) {
    std::cout << "bad any cast: can't parse int from " << s << '\n';
    return {{}, s};
  }
}

ParsingResult parse_any_val(std::string_view s) {
  std::cout << __func__ << " with " << s << '\n';
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
  // NOTE: parsed from string later
  // if (res = parse_opr(s); res.value.has_value()) {
  //   return res;
  // }
  // if (res = parse_patt(s); res.value.has_value()) {
  //   return res;
  // }
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

  Result build() {
    // TODO: check too many args ??
    try {
      // TODO: check for all args present
      return Result::success(std::visit<SMInstr>( //
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
                x.name = std::any_cast<std::string>(args.at(0));
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
                x.opr = any_opr_cast(args.at(0));
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
                x.n = std::any_cast<int>(args.at(0));
                x.tail = std::any_cast<bool>(args.at(1));
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
                x.patt = any_patt_cast(args.at(0));
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
          *instr));
    } catch (const std::bad_any_cast &) {
      return Result::failure("build: bad any cast");
    } catch (const std::out_of_range &) {
      return Result::failure("build: out of range");
    }
  }

  template <typename T> void push_arg(T &&value) { args.emplace_back(value); }

private:
  SMInstr instr;
  std::vector<std::any> args;
};

utils::Result<SMInstr> parse_sm(const std::string &line) {
  std::cout << __func__ << '\n';
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

  if (cmd.empty()) {
    cmd = line;
    pos = line.size();
  }

  auto instr_it = to_instr.find(cmd);
  if (instr_it == to_instr.end()) {
    return Result::failure("instr name not found: `" + cmd + "`");
  }
  SMInstrBuilder instr{instr_it->second};

  // no args case
  if (pos == line.size()) {
    return instr.build();
  }

  // (_, ..., _) - args
  ParsingResult args_res =
      parse_array({line.data() + pos, line.data() + line.size()}, '(', ')');

  if (!args_res.value.has_value()) {
    return Result::failure("arguments list parsing error");
  }

  try {
    auto args = std::any_cast<std::vector<std::any>>(std::move(args_res.value));
    args_res.value = {};

    if (not args_res.rest.empty()) {
      return Result::failure("extra symbols after instr parsed args");
    }

    // TODO: put all array at once
    for (auto &&arg : args) {
      instr.push_arg(arg);
    }
    args = {};
  } catch (const std::bad_any_cast &) {
    return Result::failure("bad any cast: parsed argument list is not array");
  }

  return instr.build();
}

// TODO: TMP: not efficient, for test purposes only
// TODO: number of printed information reduced for now
std::string print_sm(const SMInstr &instr) {
  return {std::visit<std::string>( //
      utils::multifunc{
          //
          [](const SMInstr::PUBLIC &x) -> std::string {
            return "PUBLIC [" + x.name + "]";
          },
          [](const SMInstr::EXTERN &x) -> std::string {
            return "EXTERN [" + x.name + "]";
          },
          [](const SMInstr::IMPORT &x) -> std::string {
            return "IMPORT [" + x.name + "]";
          },
          [](const SMInstr::CLOSURE &x) -> std::string {
            return "CLOSURE [" + x.name +
                   ". args_count=" + std::to_string(x.closure.size()) + "]";
          },
          [](const SMInstr::CONST &x) -> std::string {
            return "CONST [" + std::to_string(x.n) + "]";
          },
          [](const SMInstr::STRING &x) -> std::string {
            return "STRING [" + x.str + "]";
          },
          [](const SMInstr::LDA &) -> std::string {
            // x.v
            return "LDA";
          },
          [](const SMInstr::LD &) -> std::string {
            // x.v
            return "LD";
          },
          [](const SMInstr::ST &) -> std::string {
            // x.v
            return "ST";
          },
          [](const SMInstr::STA &) -> std::string { return "STA"; },
          [](const SMInstr::STI &) -> std::string { return "STI"; },
          [](const SMInstr::BINOP &) -> std::string {
            // x.opr
            return "BINOP";
          },
          [](const SMInstr::LABEL &x) -> std::string {
            return "LABEL [" + x.s + "]";
          },
          [](const SMInstr::FLABEL &x) -> std::string {
            return "FLABEL [" + x.s + "]";
          },
          [](const SMInstr::SLABEL &x) -> std::string {
            return "SLABEL [" + x.s + "]";
          },
          [](const SMInstr::JMP &x) -> std::string {
            return "JMP [" + x.l + "]";
          },
          [](const SMInstr::CJMP &x) -> std::string {
            return "CJMP [" + x.s + ". " + x.l + "]";
          },
          [](const SMInstr::BEGIN &) -> std::string {
            // x.f
            // x.nargs
            // x.nlocals
            // x.closure
            // x.args
            // x.scopes
            return "BEGIN";
          },
          [](const SMInstr::END &) -> std::string { return "END"; },
          [](const SMInstr::RET &) -> std::string { return "RET"; },
          [](const SMInstr::ELEM &) -> std::string { return "ELEM"; },
          [](const SMInstr::CALL &x) -> std::string {
            // x.tail
            return "CALL [" + x.fname + ". " + std::to_string(x.n) + "]";
          },
          [](const SMInstr::CALLC &x) -> std::string {
            // x.tail
            return "CALLC [" + std::to_string(x.n) + "]";
          },
          [](const SMInstr::SEXP &x) -> std::string {
            return "SEXP [" + x.tag + ". " + std::to_string(x.n) + "]";
          },
          [](const SMInstr::DROP &) -> std::string { return "DROP"; },
          [](const SMInstr::DUP &) -> std::string { return "DUP"; },
          [](const SMInstr::SWAP &) -> std::string { return "SWAP"; },
          [](const SMInstr::TAG &x) -> std::string {
            return "TAG [" + x.tag + ". " + std::to_string(x.n) + "]";
          },
          [](const SMInstr::ARRAY &x) -> std::string {
            return "ARRAY [" + std::to_string(x.n) + "]";
          },
          [](const SMInstr::PATT &) -> std::string {
            // x.patt
            return "PATT";
          },
          [](const SMInstr::LINE &x) -> std::string {
            return "LINE [" + std::to_string(x.n) + "]";
          },
          [](const SMInstr::FAIL &x) -> std::string {
            return "FAIL [" + std::to_string(x.line) + ". " +
                   std::to_string(x.col) + ". " + std::to_string(x.val) + ". " +
                   "]";
          },
          // [](auto) -> std::string {
          //   throw std::bad_any_cast{}; // create another error ?
          // },
      },
      *instr)};
}
