#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace utils {
// https://en.cppreference.com/w/cpp/utility/variant/visit2
template <class... Ts> struct multifunc : Ts... {
  using Ts::operator()...;
};
template <class... Ts> multifunc(Ts...) -> multifunc<Ts...>;
} // namespace utils

enum class Patt {
  BOXED,
  UNBOXED,
  ARRAY,
  STRING,
  SEXP,
  CLOSURE,
  STRCMP,
};

enum class Opr {
  ADD = 0, // +
  SUB,     // -
  MULT,    // *
  DIV,     // /
  MOD,     // %
  LEQ,     // <=
  LT,      // <
  GT,      // >
  GEQ,     // >=
  EQ,      // ==
  NEQ,     // !=
  AND,     // &&
  OR,      // !!
  XOR,     // ^ (compiler only)
  CMP,     // cmp (compiler only)
  TEST,    // test (compiler only)
};

struct ValT {
  struct Global {
    std::string s;
  };
  struct Local {
    size_t n;
  };
  struct Arg {
    size_t n;
  };
  struct Access {
    size_t n;
  };
  struct Fun {
    std::string s;
  };

  using T = std::variant<Global, Local, Arg, Access, Fun>;

  T val;

  template <typename U>
    requires(not std::is_same_v<ValT, std::remove_reference_t<U>>)
  ValT(U &&x) : val(std::forward<U>(x)) {}

  template <typename U> bool is() const {
    return std::holds_alternative<U>(val);
  }

  const T &operator*() const { return val; }
  const T &operator->() const { return val; }

  bool operator==(const ValT &other) const = default;
};

/* The type for local scopes tree */
struct Scope {
  std::string blab;
  std::string elab;
  std::vector<std::pair<std::string, int>> names;
  std::vector<Scope> subs;
};

// TODO: use bytecode in interpreter represientation instead
/* The type for the stack machine instructions */
struct SMInstr {

  /* binary operator                           */
  struct BINOP {
    Opr opr;
  };
  /* put a constant on the stack               */
  struct CONST {
    int n;
  };
  /* put a string on the stack                 */
  struct STRING {
    std::string str;
  };
  /* create an S-expression                    */
  struct SEXP {
    std::string tag;
    int n;
  };
  /* load a variable to the stack              */
  struct LD {
    ValT v;
  };
  /* load a variable address to the stack      */
  struct LDA {
    ValT v;
  };
  /* store a value into a variable             */
  struct ST {
    ValT v;
  };
  /* store a value into a reference            */
  struct STI {};
  /* store a value into array/sexp/string      */
  struct STA {};
  /* takes an element of array/string/sexp     */
  struct ELEM {};
  /* a label                                   */
  struct LABEL {
    std::string s;
  };
  /* a forwarded label                         */
  struct FLABEL {
    std::string s;
  };
  /* a scope label                             */
  struct SLABEL {
    std::string s;
  };
  /* unconditional jump                        */
  struct JMP {
    std::string l;
  };
  /* conditional jump                          */
  struct CJMP {
    std::string s;
    std::string l;
  };
  /* begins procedure definition               */
  struct BEGIN {
    std::string f;
    int nargs;
    int nlocals;
    std::vector<ValT> closure;
    std::vector<std::string> args;
    std::vector<Scope> scopes;
  };
  /* end procedure definition                  */
  struct END {};
  /* create a closure                          */
  struct CLOSURE {
    std::string name;
    std::vector<ValT> closure;
  };
  /* calls a closure                           */
  struct CALLC {
    int n;
    bool tail;
  };
  /* calls a function/procedure                */
  struct CALL {
    std::string fname;
    int n;
    bool tail;
  };
  /* returns from a function                   */
  struct RET {};
  /* drops the top element off                 */
  struct DROP {};
  /* duplicates the top element                */
  struct DUP {};
  /* swaps two top elements                    */
  struct SWAP {};
  /* checks the tag and arity of S-expression  */
  struct TAG {
    std::string tag;
    int n;
  };
  /* checks the tag and size of array          */
  struct ARRAY {
    int n;
  };
  /* checks various patterns                   */
  struct PATT {
    Patt patt;
  };
  /* match failure (location, leave a value    */
  struct FAIL {
    size_t line;
    size_t col;
    bool val;
  };
  /* external definition                       */
  struct EXTERN {
    std::string name;
  };
  /* public   definition                       */
  struct PUBLIC {
    std::string name;
  };
  /* import clause                             */
  struct IMPORT {
    std::string name;
  };
  /* line info                                 */
  struct LINE {
    int n;
  };

  using T = std::variant<BINOP, CONST, STRING, SEXP, LD, LDA, ST, STI, STA,
                         ELEM, LABEL, FLABEL, SLABEL, JMP, CJMP, BEGIN, END,
                         CLOSURE, CALLC, CALL, RET, DROP, DUP, SWAP, TAG, ARRAY,
                         PATT, FAIL, EXTERN, PUBLIC, IMPORT, LINE>;

  T val;

  template <typename U>
    requires(not std::is_same_v<SMInstr, std::remove_reference_t<U>>)
  SMInstr(U &&x) : val(std::forward<U>(x)) {}

  template <typename U> bool is() const {
    return std::holds_alternative<U>(val);
  }

  const T &operator*() const { return val; }
  const T &operator->() const { return val; }

  bool operator==(const SMInstr &other) const = default;
};

std::vector<SMInstr> parse_sm(std::istream &in);

std::optional<SMInstr> parse_sm(const std::string &line);
