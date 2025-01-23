// based on src/X86_64.ml

#include "../../runtime/runtime.h"

#include <format>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace utils {

// https://en.cppreference.com/w/cpp/utility/variant/visit2
template <class... Ts> struct multifunc : Ts... {
  using Ts::operator()...;
};
template <class... Ts> multifunc(Ts...) -> multifunc<Ts...>;

// https://en.cppreference.com/w/cpp/utility/unreachable
[[noreturn]] inline void unreachable() {
  // Uses compiler specific extensions if possible.
  // Even if no extension is used, undefined behavior is still raised by
  // an empty function body and the noreturn attribute.
#if defined(_MSC_VER) && !defined(__clang__) // MSVC
  __assume(false);
#else // GCC, Clang
  __builtin_unreachable();
#endif
}

// BETTER: use ranges transform
template <typename T, typename U>
std::vector<U> transform(std::vector<T> v, const std::function<U(T &&)> &f) {
  std::vector<U> result;
  for (auto &&x : v) {
    result.push_back(f(std::move(x)));
  }
  return result;
}

template <typename T>
std::vector<T> filter(std::vector<T> &&x,
                      const std::function<bool(const T &)> &f) {
  std::erase_if(x, f);
  return std::move(x);
}

template <typename T> void insert(std::vector<T> &x, std::vector<T> &&y) {
  x.insert(x.end(), std::move_iterator(y.begin()), std::move_iterator(y.end()));
}

template <typename T> std::vector<T> reverse(std::vector<T> &&x) {
  std::reverse(x.begin(), x.end());
  return std::move(x);
}

template <typename T> std::vector<T> concat(std::vector<T> &&x) {
  return std::move(x);
}

// --> declarations
template <typename T, typename U, typename... Args>
  requires std::is_same_v<U, T>
std::vector<T> concat(std::vector<T> &&x, U &&y, Args &&...args);
template <typename T, typename U, typename... Args>
  requires std::is_same_v<U, std::optional<T>>
std::vector<T> concat(std::vector<T> &&x, U &&y, Args &&...args);
template <typename T, typename U, typename... Args>
  requires std::is_same_v<U, std::vector<T>>
std::vector<T> concat(std::vector<T> &&x, U &&y, Args &&...args);
// <--

template <typename T, typename U, typename... Args>
  requires std::is_same_v<U, std::vector<T>>
std::vector<T> concat(std::vector<T> &&x, U &&y, Args &&...args) {
  insert(x, std::move(y));
  return concat<T, Args...>(std::move(x), std::forward<Args>(args)...);
}

template <typename T, typename U, typename... Args>
  requires std::is_same_v<U, T>
std::vector<T> concat(std::vector<T> &&x, U &&y, Args &&...args) {
  x.push_back(std::move(y));
  return concat<T, Args...>(std::move(x), std::forward<Args>(args)...);
}

template <typename T, typename U, typename... Args>
  requires std::is_same_v<U, std::optional<T>>
std::vector<T> concat(std::vector<T> &&x, U &&y, Args &&...args) {
  if (y) {
    x.push_back(std::move(*y));
  }
  return concat<T, Args...>(std::move(x), std::forward<Args>(args)...);
}

// template <typename T, typename... Args>
// std::vector<T> concat(std::vector<T> x, const std::vector<T> &y,
//                       Args &&...args) {
//   x.insert(x.end(), y.begin(), y.end());
//   return concat(std::move(x), std::forward<Args>(args)...);
// }

constexpr const std::string_view normal_label = "L";
constexpr const std::string_view builtin_label = "B";
constexpr const std::string_view global_label = "global_";
std::string labeled(const std::string_view s) {
  return std::format("{}{}", normal_label, s);
}
std::string labeled_builtin(const std::string_view s) {
  return std::format("{}{}", builtin_label, s);
}
std::string labeled_global(const std::string_view s) {
  return std::format("{}{}", global_label, s);
}
std::string labeled_scoped(int64_t i, const std::string_view s) {
  return std::format("{}{}_{}", normal_label, s, i);
}

} // namespace utils

enum class OS {
  LINUX,
  DARWIN,
};

struct Options {
  std::string topname;
  std::string filename;
};

struct CompilationMode {
  bool is_debug;
  OS os;
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

namespace Register {
struct Desc {
  std::string name8;
  std::string name64;

  bool operator==(const Desc &other) const = default;
};

struct T {
  std::string name;
  Desc reg;

  bool operator==(const T &other) const = default;
};

T from_names(const std::string &l8, const std::string &l64) {
  return {.name = l64, .reg = {.name8 = l8, .name64 = l64}};
}

T from_number(int n) {
  std::string str_of_int = std::to_string(n);
  std::string name64 = std::format("%r{}", str_of_int);
  std::string name8 = std::format("%r{}b", std::move(str_of_int));
  return {.name = name8, .reg = {.name8 = name8, .name64 = name64}};
}
T of_8bit(const T &r) { return {.name = r.reg.name8, .reg = r.reg}; }
T of_64bit(const T &r) { return {.name = r.reg.name64, .reg = r.reg}; }

const std::string &to_string(const T &r) { return r.name; }

// const auto none = Register::T{}; // NOTE: not used
} // namespace Register

namespace Registers {
const auto rax = Register::from_names("%al", "%rax");

const auto rdx = Register::from_names("%dl", "%rdx");

/* Caller-saved argument registers */
const auto rdi = Register::from_names("%dil", "%rdi");
const auto rsi = Register::from_names("%sil", "%rsi");
const auto rcx = Register::from_names("%cl", "%rcx");
const auto r8 = Register::from_number(8);
const auto r9 = Register::from_number(9);

/* Extra caller-saved registers */
const auto r10 = Register::from_number(10);
const auto r11 = Register::from_number(11);

/* Callee-saved special registers */
const auto rbp = Register::from_names("%bpl", "%rbp");
const auto rsp = Register::from_names("%spl", "%rsp");

/* r12-15 registes are calee-saved in X86_64
   But we are using them as caller-save for simplicity
   This disallows calling Lama code from C
   While does not affects C calls from Lama */
const auto r12 = Register::from_number(12);
const auto r13 = Register::from_number(13);
const auto r14 = Register::from_number(14);
const auto r15 = Register::from_number(15);

const std::vector<Register::T> argument_registers = {rdi, rsi, rdx,
                                                     rcx, r8,  r9};
const std::vector<Register::T> extra_caller_saved_registers = {r10, r11, r12,
                                                               r13, r14};

} // namespace Registers

/* Attributes of the named memory location addressing */

/* External symbols have to be acessed through plt or GOTPCREL.
   While internal just using rip-based addressing. */
enum class Externality { I /** Internal */, E /** External */ };

/* External functions have to pe acessed through plt.
   While data through GOTPCREL. */
enum class DataKind { F /** Function */, D /** Data */ };

/* For functions and string their value is their address.
   While for numbers is the value on this address. */
enum class Addressed { A /** Address */, V /** Value */ };

/* We need to distinguish the following operand types: */
struct Opnd {
  struct R {
    Register::T reg; /* Hard register */

    bool operator==(const R &other) const = default;
  };

  struct S {
    size_t pos; /* Position on the hardware stack */

    bool operator==(const S &other) const = default;
  };
  struct M {
    DataKind kind;
    Externality ext;
    Addressed addr;
    std::string name;

    bool operator==(const M &other) const = default;
    /* Named memory location */
  };
  struct C {
    std::string name; /* Named constant */

    bool operator==(const C &other) const = default;
  };
  struct L {
    int num; /* Immediate operand */

    bool operator==(const L &other) const = default;
  };
  struct I {
    I(int num, const Opnd &opnd)
        : num(num), opnd(std::make_unique<Opnd>(opnd)) {}

    I(const I &other)
        : num(other.num), opnd(std::make_unique<Opnd>(*other.opnd)) {}

    I(I &&other) : num(other.num), opnd(std::move(other.opnd)) {}

    I &operator=(const I &other) {
      if (&other != this) {
        num = other.num;
        opnd = std::make_unique<Opnd>(*other.opnd);
      }
      return *this;
    }
    I &operator=(I &&other) {
      if (&other != this) {
        num = other.num;
        opnd = std::move(other.opnd);
      }
      return *this;
    }

    int num;
    std::unique_ptr<Opnd> opnd; /* Indirect operand with offset */

    bool operator==(const I &other) const = default;
  };

  using T = std::variant<R, S, M, C, L, I>;

  T val;

  template <typename U>
    requires(not std::is_same_v<Opnd, std::remove_reference_t<U>>)
  Opnd(U &&x) : val(std::forward<U>(x)) {}

  Opnd(const Register::T x) : val(R{x}) {}

  template <typename U> bool is() const {
    return std::holds_alternative<U>(val);
  }

  const T &operator*() const { return val; }
  const T &operator->() const { return val; }

  bool operator==(const Opnd &other) const = default;
};
using C = Opnd::C;
using I = Opnd::I;
using L = Opnd::L;
using M = Opnd::M;
using R = Opnd::R;
using S = Opnd::S;

/* Value that could be used to fill unused stack locations.
   Garbage is not allowed as it will affect GC. */
struct ArgumentLocation {
  struct Register {
    Opnd opnd;
  };

  struct Stack {};

  using T = std::variant<Register, Stack>;

  T val;

  const T &operator*() const { return val; }
  const T &operator->() const { return val; }
};

/* We need to know the word size to calculate offsets correctly */
constexpr auto word_size = 8;

const Register::T &as_register(const Opnd &opnd) {
  return std::visit(
      utils::multifunc{
          [](const Opnd::R &r) -> const Register::T & { return r.reg; },
          [](const auto &) -> const Register::T & {
            failure("as_register: not a register");
            utils::unreachable();
          },
      },
      *opnd);
}

std::string to_string(const Opnd &opnd) {
  return std::visit(
      utils::multifunc{
          [](const Opnd::R &x) {
            return std::format("R {}", to_string(x.reg));
          },
          [](const Opnd::S &x) { return std::format("S {}", x.pos); },
          [](const Opnd::L &x) { return std::format("L {}", x.num); },
          [](const Opnd::I &x) {
            return std::format("I {} {}", x.num, to_string(*x.opnd));
          },
          [](const Opnd::C &x) { return std::format("C {}", x.name); },
          [](const Opnd::M &x) {
            return std::format(
                "M {} {} {} {}", x.kind == DataKind::F ? "Function" : "Data",
                x.ext == Externality::I ? "Internal" : "External",
                x.addr == Addressed::A ? "Address" : "Value", x.name);
          },
      },
      *opnd);
}

/* for convenience */
using namespace Registers;

const auto filler =
    Opnd{Opnd::M{DataKind::D, Externality::I, Addressed::V, "filler"}};

struct Instr {
  /* copies a value from the first to the second operand   */
  struct Mov {
    Opnd left;
    Opnd right;
  };
  /* loads an address of the first operand into the second */
  struct Lea {
    Opnd left;
    Opnd right;
  };
  /* makes a binary operation; note, the first operand
     designates x86 operator, not the source language one */
  struct Binop {
    Opr op;
    Opnd left;
    Opnd right;
  };
  /* x86 integer division, see instruction set reference   */
  struct IDiv {
    Opnd opnd;
  };
  /* see instruction set reference                         */
  struct Cltd {};
  /* sets a value from flags; the first operand is the
     suffix, which determines the value being set, the
     the second --- (sub)register name */
  struct Set {
    std::string suffix;
    Register::T reg;
  };
  /* pushes the operand on the hardware stack              */
  struct Push {
    Opnd opnd;
  };
  /* pops from the hardware stack to the operand           */
  struct Pop {
    Opnd opnd;
  };
  /* call a function by a name                             */
  struct Call {
    std::string name;
  };
  /* call a function by indirect address                   */
  struct CallI {
    Opnd val;
  };
  /* returns from a function                               */
  struct Ret {};
  /* a label in the code                                   */
  struct Label {
    std::string name;
  };
  /* a conditional jump                                    */
  struct CJmp {
    std::string cmp;
    std::string label;
  };
  /* a non-conditional jump by a name                      */
  struct Jmp {
    std::string label;
  };
  /* a non-conditional jump by indirect address            */
  struct JmpI {
    Opnd opnd;
  };
  /* directive                                             */
  struct Meta {
    std::string name;
  };
  /* arithmetic correction: decrement                      */
  struct Dec {
    Opnd opnd;
  };
  /* arithmetic correction: or 0x0001                      */
  struct Or1 {
    Opnd opnd;
  };
  /* arithmetic correction: shl 1                          */
  struct Sal1 {
    Opnd opnd;
  };
  /* arithmetic correction: shr 1                          */
  struct Sar1 {
    Opnd opnd;
  };
  struct Repmovsl {};

  using T = std::variant<Mov, Lea, Binop, IDiv, Cltd, Set, Push, Pop, Call,
                         CallI, Ret, Label, CJmp, Jmp, JmpI, Meta, Dec, Or1,
                         Sal1, Sar1, Repmovsl>;

  T val;

  const T &operator*() const { return val; }
  const T &operator->() const { return val; }

  template <typename U>
    requires(not std::is_same_v<Instr, std::remove_reference_t<U>>)
  Instr(U &&x) : val(std::forward<U>(x)) {}

  template <typename U> bool is() const {
    return std::holds_alternative<U>(val);
  }
};
using Mov = Instr::Mov;
using Lea = Instr::Lea;
using Binop = Instr::Binop;
using IDiv = Instr::IDiv;
using Cltd = Instr::Cltd;
using Set = Instr::Set;
using Push = Instr::Push;
using Pop = Instr::Pop;
using Call = Instr::Call;
using CallI = Instr::CallI;
using Ret = Instr::Ret;
using Label = Instr::Label;
using CJmp = Instr::CJmp;
using Jmp = Instr::Jmp;
using JmpI = Instr::JmpI;
using Meta = Instr::Meta;
using Dec = Instr::Dec;
using Or1 = Instr::Or1;
using Sal1 = Instr::Sal1;
using Sar1 = Instr::Sar1;
using Repmovsl = Instr::Repmovsl;

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

template <typename U> struct AbstractSymbolicStack {
  // type 'a t
  // type 'a symbolic_location = Stack of int | Register of 'a

  // val empty : 'a array -> 'a t
  // val is_empty : _ t -> bool
  // val live_registers : 'a t -> 'a list
  // val stack_size : _ t -> int
  // val allocate : 'a t -> 'a t * 'a symbolic_location
  // val pop : 'a t -> 'a t * 'a symbolic_location
  // val peek : 'a t -> 'a symbolic_location
  // val peek2 : 'a t -> 'a symbolic_location * 'a symbolic_location

  //

  /* Last allocated position on symbolic stack */
  struct State {
    struct S {
      size_t n;
    };
    struct R {
      size_t n;
    };
    struct E {};

    using W = std::variant<S, R, E>;

    W val;

    const W &operator*() const { return val; }
    const W &operator->() const { return val; }
  };
  using S = State::S;
  using R = State::R;
  using E = State::E;

  //

  struct Location {
    struct Stack {
      int n;
    };
    struct Register {
      U r;
    };
    struct E {};

    using W = std::variant<Stack, Register>;

    W val;

    const W &operator*() const { return val; }
    const W &operator->() const { return val; }

    template <typename S> bool is() const {
      return std::holds_alternative<S>(val);
    }
  };
  using Stack = Location::Stack;
  using Register = Location::Register;

public:
  State state;
  std::vector<U> registers;

public:
  AbstractSymbolicStack(std::vector<U> registers)
      : state(typename State::E{}), registers(std::move(registers)) {}

  State next_state(const State &v) const {
    return std::visit(utils::multifunc{
                          [](const S &x) -> State { return {S(x.n)}; },
                          [this](const R &x) -> State {
                            if (x.n + 1 >= registers.size()) {
                              return {S(0)};
                            } else {
                              return {R(x.n + 1)};
                            }
                          },
                          [](const E &) -> State { return {R(0)}; },
                      },
                      *v);
  }

  State previous_state(const State &v) const {
    return std::visit(utils::multifunc{
                          [this](const S &x) -> State {
                            return x.n == 0 ? State{R{registers.size() - 1}}
                                            : State{S{x.n - 1}};
                          },
                          [](const R &x) -> State {
                            return x.n == 0 ? State{E{}} : State{R{x.n - 1}};
                          },
                          [](const E &) -> State {
                            failure("Empty stack %s: %d", __FILE__, __LINE__);
                            utils::unreachable();
                          },
                      },
                      *v);
  }

  Location location(const std::optional<State> &another_state = {}) const {
    return std::visit(utils::multifunc{
                          [](const S &x) -> Location { return {Stack(x.n)}; },
                          [this](const R &x) -> Location {
                            return {Register{registers[x.n]}};
                          },
                          [](const E &) -> Location {
                            failure("Empty stack %s: %d", __FILE__, __LINE__);
                            utils::unreachable();
                          },
                      },
                      another_state ? **another_state : *state);
  }

  bool is_empty() const {
    return std::holds_alternative<typename State::E>(*state);
  }

  // BETTER: replace with range
  std::vector<U> live_registers() const {
    return std::visit( //
        utils::multifunc{
            [this](const S &) { return registers; },
            [this](const R &x) {
              std::vector<U> registers_prefix;
              registers_prefix.insert(registers_prefix.end(), registers.begin(),
                                      registers.begin() + x.n + 1);
              // NOTE: same to (Array.sub registers 0 (n + 1))
              return registers_prefix;
            },
            [](const E &) { return std::vector<U>{}; },
        },
        *state);
  }

  size_t stack_size() const {
    return std::visit(utils::multifunc{
                          [](const S &x) { return x.n + 1; },
                          [](const auto &) -> size_t { return 0; },
                      },
                      *state);
  }

  Location allocate() {
    state = next_state(state);
    return location();
  }

  Location pop() {
    state = previous_state(state);
    return location();
  }

  Location peek() const { return location(); }

  std::pair<Location, Location> peek2() const {
    return {location(), location(previous_state(state))};
  }
};

struct SymbolicStack {
  using AbSS = AbstractSymbolicStack<Register::T>;

  using S = AbSS::State::S;
  using R = AbSS::State::R;
  using E = AbSS::State::E;

  using Location = AbSS::Location;
  using Stack = Location::Stack;
  using Register = Location::Register;

  // type t

  // val empty : int -> t
  // val is_empty : t -> bool
  // val live_registers : t -> opnd list
  // val stack_size : t -> int
  // val allocate : t -> t * opnd
  // val pop : t -> t * opnd
  // val peek : t -> opnd
  // val peek2 : t -> opnd * opnd

public:
  AbSS state;
  size_t nlocals;

public:
  /* To use free argument registers we have to rewrite function call
     compilation. Otherwise we will result with the following code in
     arguments setup: movq    %rcx,   %rdx movq    %rdx,   %rsi */
  SymbolicStack(size_t nlocals)
      : state(AbSS(Registers::extra_caller_saved_registers)), nlocals(nlocals) {
  }

  Opnd opnd_from_loc(const Location &loc) const {
    return std::visit(
        utils::multifunc{
            [](const Register &x) -> Opnd { return {Opnd::R{x.r}}; },
            [this](const Stack &x) -> Opnd { return Opnd::S{x.n + nlocals}; },
        },
        *loc);
  }

  bool is_empty() const { return state.is_empty(); };

  std::vector<Opnd> live_registers() const {
    return utils::transform<::Register::T, Opnd>(
        state.live_registers(), [](auto &&r) -> Opnd { return Opnd::R{r}; });
  }

  size_t stack_size() const { return state.stack_size(); }

  Opnd allocate() { return opnd_from_loc(state.allocate()); }

  Opnd pop() { return opnd_from_loc(state.pop()); }

  Opnd peek() const { return opnd_from_loc(state.peek()); }

  std::pair<Opnd, Opnd> peek2() const {
    const auto [loc1, loc2] = state.peek2();
    return {opnd_from_loc(loc1), opnd_from_loc(loc2)};
  }
};

/* A set of strings */
using SetS = std::unordered_set<std::string>;

/* A map indexed by strings */
template <typename T> using MapS = std::unordered_map<std::string, T>;

struct Mode {
  bool is_debug;
  OS target_os;
};

// TODO: remove unrequired parts
struct Env {
private:
  const static std::string chars;

  // NOTE: is not required
  // const std::vector<Opnd> argument_registers = Registers.argument_registers

private:
  SetS globals;              /* a set of global variables */
  MapS<std::string> stringm; /* a string map */
  size_t scount = 0;         /* string count */
  size_t stack_slots = 0;    /* maximal number of stack positions */
  size_t static_size = 0;    /* static data size */
  SymbolicStack stack = SymbolicStack(0); /* symbolic stack */
  std::vector<std::vector<std::string>> locals;
  MapS<SymbolicStack> stackmap; /* labels to stack map */
  bool barrier = false;         /* barrier condition */

  SetS externs;
  size_t nlabels = 0;
  bool first_line = true;

public:
  SetS publics;

  size_t max_locals_size =
      0; /* maximal number of stack position in all functions */
  bool has_closure = false;
  std::string fname; /* function name */

  size_t nargs = 0; /* number of function arguments */

  const Mode mode;

public:
  Env(Mode mode) : mode(std::move(mode)) {}

  void register_public(std::string name) { publics.insert(std::move(name)); }
  void register_extern(std::string name) { externs.insert(std::move(name)); }

  void leave() {
    if (stack_slots > max_locals_size) {
      max_locals_size = stack_slots;
    }
  }

  // TODO: add SymbolicStack functions to opimize
  std::string print_stack() const {
    std::stringstream result;
    SymbolicStack stack_copy = stack;

    std::vector<std::string> elements;
    while (!stack_copy.is_empty()) {
      elements.push_back(to_string(stack_copy.pop()));
    }
    std::reverse(elements.begin(), elements.end());
    for (const auto &element : elements) {
      result << element << " ";
    }
    return result.str();
  }

  /* Assert empty stack */
  void assert_empty_stack() const { assert(stack.is_empty()); }

  /* check barrier condition */
  bool is_barrier() { return barrier; }

  /* set barrier */
  void set_barrier() { barrier = true; }

  /* drop barrier */
  void drop_barrier() { barrier = false; }

  /* drop stack */
  void drop_stack() { stack = SymbolicStack(static_size); }

  /* associates a stack to a label */
  void set_stack(std::string const &l) { stackmap.insert({l, stack}); }

  /* retrieves a stack for a label */
  std::optional<SymbolicStack *> retrieve_stack(std::string const &l) {
    auto it = stackmap.find(l);
    if (it != stackmap.end()) {
      return &it->second;
    }

    return std::nullopt;
  }

  /* checks if there is a stack for a label */
  bool has_stack(const std::string &l) const { return stackmap.count(l) != 0; }
  bool is_external(const std::string &name) const {
    return externs.count(name) != 0;
  }

  /* gets a location for a variable */
  Opnd loc(const ValT &x) {
    return std::visit(
        utils::multifunc{
            [this](const ValT::Global &x) -> Opnd {
              auto loc_name = utils::labeled_global(x.s);
              const auto ext =
                  is_external(x.s) ? Externality::E : Externality::I;
              return M{DataKind::D, ext, Addressed::V, std::move(loc_name)};
            },
            [this](const ValT::Fun &x) -> Opnd {
              const auto ext =
                  is_external(x.s) ? Externality::E : Externality::I;
              return M{DataKind::F, ext, Addressed::A, x.s};
            },
            [](const ValT::Local &x) -> Opnd { return S{x.n}; },
            [](const ValT::Arg &x) -> Opnd {
              return x.n < Registers::argument_registers.size()
                         ? Opnd{argument_registers[x.n]}
                         : S{-(x.n - Registers::argument_registers.size()) - 1};
            },
            [](const ValT::Access &x) -> Opnd {
              return I{static_cast<int>(word_size * (x.n + 1)), r15};
            },
        },
        *x);
  }

  /* allocates a fresh position on a symbolic stack */
  Opnd allocate() {
    auto opnd = stack.allocate();
    stack_slots = std::max(stack_slots, (static_size + stack.stack_size()));
    return opnd;
  }

  /* pops one operand from the symbolic stack */
  Opnd pop() { return stack.pop(); }

  /* is rdx register in use */
  bool rdx_in_use() const { return nargs > 2; }

  std::pair<std::vector<SymbolicStack::Location>, size_t>
  arguments_locations(size_t n) {
    using Location = SymbolicStack::Location;
    using Register = SymbolicStack::Register;
    using Stack = SymbolicStack::Stack;

    if (n < Registers::argument_registers.size()) {
      std::vector<::Register::T> result;
      result.insert(result.end(), argument_registers.begin(),
                    argument_registers.begin() + n);
      return {utils::transform<::Register::T, Location>(
                  std::move(result),
                  [](const auto &r) -> Location { return {Register{r}}; }),
              0};
    } else {
      return {utils::concat( //
                  utils::transform<::Register::T, Location>(
                      std::move(argument_registers),
                      [](const auto &r) -> Location { return {Register{r}}; }),
                  std::vector<Location>(
                      n - Registers::argument_registers.size(), {Stack{}})),
              n - Registers::argument_registers.size()};
    }
  }

  /* peeks the top of the stack (the stack does not change) */
  Opnd peek() const { return stack.peek(); }

  /* peeks two topmost values from the stack (the stack itself does not
   * change)
   */
  std::pair<Opnd, Opnd> peek2() const { return stack.peek2(); }

  /* tag hash: gets a hash for a string tag */
  static uint64_t hash(const std::string &tag) {
    assert(!tag.empty());
    uint64_t h = 0;
    for (size_t i = 0; i < std::min((tag.size() - 1), 9lu); ++i) {
      h = (h << 6) | chars[tag[i]];
    }
    return h;
  }

  /* registers a variable in the environment */
  void register_variable(const ValT &x) {
    if (x.is<ValT::Global>()) {
      globals.insert(utils::labeled_global(std::get<ValT::Global>(*x).s));
    }
  }

  /* registers a string constant */
  Opnd register_string(const std::string &x) {
    const auto escape = [](const std::string &y) {
      const size_t n = y.size();
      std::string buffer;
      buffer.reserve(n * 2);
      for (size_t i = 0; i < n; ++i) {
        switch (y[i]) {
        case '"':
          buffer.push_back('\\');
          buffer.push_back('"');
          break;
        case '\\':
          buffer.push_back('\\');
          if (i + 1 >= n) {
            buffer.push_back('\\');
          } else {
            switch (y[i + 1]) {
            case 'n':
            case 't':
              buffer.push_back(y[i + 1]);
              ++i;
              break;
            default:
              buffer.push_back('\\');
              break;
            }
          }
          break;
        default:
          buffer.push_back(y[i]);
          break;
        }
      }
      return buffer;
    };
    const auto y = escape(x);

    const auto it = stringm.find(y);

    if (it == stringm.end()) {
      const auto name = std::format("string_{}", scount);
      stringm.insert({y, name});
      ++scount;
    }
    return M{DataKind::D, Externality::I, Addressed::A, it->second};
  }

  /* gets all global variables */
  std::vector<std::string> get_globals() const {
    std::vector<std::string> result;
    result.resize(globals.size());
    for (const auto &x : globals) {
      if (externs.count(x) == 0) {
        result.push_back(x);
      }
    }
    return result;
  }

  /* gets a number of stack positions allocated */
  size_t get_allocated() const { return stack_slots; }
  std::string get_allocated_size() const {
    return utils::labeled(std::format("S{}_SIZE", fname));
  }

  /* enters a function */
  void enter(std::string const &f, size_t new_nargs, size_t new_nlocals,
             bool new_has_closure) {
    nargs = new_nargs;
    static_size = new_nlocals;
    stack_slots = new_nlocals;
    stack = SymbolicStack(new_nlocals);
    fname = f;
    has_closure = new_has_closure;
    first_line = true;
  }

  /* returns a label for the epilogue */
  std::string epilogue() {
    return utils::labeled(std::format("{}_epilogue", fname));
  }

  /* returns a name for local size meta-symbol */
  std::string lsize() const {
    return utils::labeled(std::format("{}_SIZE", fname));
  }

  /* returns a list of live registers */
  std::vector<Opnd> live_registers() {
    std::vector<Register::T> array_registers;
    array_registers.insert(array_registers.end(), argument_registers.begin(),
                           argument_registers.begin() +
                               std::min(nargs, argument_registers.size()));

    std::vector<Opnd> array_result = utils::transform<Register::T, Opnd>(
        std::move(array_registers),
        [](Register::T &&r) -> Opnd { return {r}; });

    return utils::concat(std::move(array_result), stack.live_registers());
  }

  bool do_opt_stabs() const { return mode.target_os == OS::LINUX; }

  /* generate a line number information for current function */
  std::vector<Instr> gen_line(size_t line) {
    const std::string lab = std::format(".L{}", nlabels);
    ++nlabels;
    first_line = false;

    std::vector<Instr> code;
    if (do_opt_stabs()) {
      if (fname == "main") {
        code.push_back(Meta{std::format("\t.stabn 68,0,{},{}", line, lab)});
      } else {

        if (first_line) {
          code.push_back(Meta{std::format("\t.stabn 68,0,{},0", line)});
        }
        code.push_back(
            Meta{std::format("\t.stabn 68,0,{},{}-{}", line, lab, fname)});
      }
    }
    code.push_back(Label{lab});
    return code;
  }

  std::string prefixed(const std::string &label) const {
    if (mode.target_os == OS::DARWIN) {
      return std::format("_{}", label);
    }
    return label;
  }
};
const std::string Env::chars =
    "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'";

int stack_offset(int i) { return (i >= 0 ? (i + 1) : (-i + 1)) * word_size; }

std::string to_code(const Env &env, const Opnd &opnd) {
  return std::visit(
      utils::multifunc{
          [](const Opnd::R &x) { return to_string(x.reg); },
          [](const Opnd::S &x) {
            int offset = stack_offset(x.pos);
            return x.pos >= 0 ? std::format("-{}(%rbp)", offset)
                              : std::format("-{}(%rbp)", offset);
          },
          [&env](const Opnd::M &x) {
            return std::format(
                "M {} {} {} {}", x.kind == DataKind::F ? "Function" : "Data",
                x.ext == Externality::I ? "Internal" : "External",
                x.addr == Addressed::A ? "Address" : "Value", x.name);
            if (x.ext == Externality::I || x.kind == DataKind::F) {
              return std::format("{}(%rip)", x.name);
            }
            // else -> x.ext == Externality::E && x.kind == DataKind::D
            return std::format("{}@GOTPCREL(%rip)", env.prefixed(x.name));
          },
          [&env](const Opnd::C &x) {
            return std::format("${}", env.prefixed(x.name));
          },
          [](const Opnd::L &x) { return std::format("${}", x.num); },
          [&env](const Opnd::I &x) {
            return x.num == 0
                       ? std::format("({})", to_code(env, *x.opnd))
                       : std::format("{}({})", x.num, to_code(env, *x.opnd));
          },
      },
      *opnd);
}

// template <typename Prg, Mode mode>
// FIXME: testing
std::string to_code(const Env &env, const Instr &instr) {
  const auto opnd_to_code = [&env](const Opnd &opnd) -> std::string {
    return to_code(env, opnd);
  };

  const auto binop_to_code = [](Opr binop) -> std::string {
    const static std::unordered_map<Opr, std::string> ops = {
        {Opr::ADD, "addq"}, {Opr::SUB, "subq"},  {Opr::MULT, "imulq"},
        {Opr::AND, "andq"}, {Opr::OR, "orq"},    {Opr::XOR, "xorq"},
        {Opr::CMP, "cmpq"}, {Opr::TEST, "test"},
    };

    auto it = ops.find(binop);

    if (it != ops.end()) {
      return it->second;
    }
    failure("unknown binary operator");
    utils::unreachable();
  };

  return std::visit(
      utils::multifunc{
          [](const Cltd &) -> std::string { return "\tcqo"; },
          [](const Set &x) -> std::string {
            auto r = to_string(Register::of_8bit(x.reg));
            return std::format("\tset{}\t{}", x.suffix, std::move(r));
          },

          [&opnd_to_code](const IDiv &x) -> std::string {
            return std::format("\tidivq\t{}", opnd_to_code(x.opnd));
          },
          [&binop_to_code, &opnd_to_code](const Binop &x) -> std::string {
            return std::format("\t{}\t{},\t{}", binop_to_code(x.op),
                               opnd_to_code(x.left), opnd_to_code(x.right));
          },
          [&opnd_to_code](const Lea &x) -> std::string {
            return std::format("\tleaq\t{},\t{}", opnd_to_code(x.left),
                               opnd_to_code(x.right));
          },
          [&opnd_to_code](const Mov &x) -> std::string {
            if (std::holds_alternative<M>(*x.left) &&
                std::get<M>(*x.left).addr == Addressed::A) {
              // Mov ((M (_, _, A, _) as x), y)
              return std::format("\tleaq\t{},\t{}", opnd_to_code(x.left),
                                 opnd_to_code(x.right));

            } else {
              return std::format("\tmovq\t{},\t{}", opnd_to_code(x.left),
                                 opnd_to_code(x.right));
            }
          },
          [&opnd_to_code](const Push &x) -> std::string {
            return std::format("\tpushq\t{}", opnd_to_code(x.opnd));
          },
          [&opnd_to_code](const Pop &x) -> std::string {
            return std::format("\tpopq\t{}", opnd_to_code(x.opnd));
          },
          [](const Ret &) -> std::string { return "\tret"; },
          [&env](const Call &x) -> std::string {
            return std::format("\tcall\t{}", env.prefixed(x.name));
          },
          [&opnd_to_code](const CallI &x) -> std::string {
            return std::format("\tcall\t*({})", opnd_to_code(x.val));
          },
          [&env](const Label &x) -> std::string {
            return std::format("{}:\n", env.prefixed(x.name));
          },
          [&env](const Jmp &x) -> std::string {
            return std::format("\tjmp\t{}", env.prefixed(x.label));
          },
          [&opnd_to_code](const JmpI &x) -> std::string {
            return std::format("\tjmp\t*({})", opnd_to_code(x.opnd));
          },
          [&env](const CJmp &x) -> std::string {
            return std::format("\tj{}\t{}", x.cmp, env.prefixed(x.label));
          },
          [](const Meta &x) -> std::string {
            return std::format("{}\n", x.name);
          },
          [&opnd_to_code](const Dec &x) -> std::string {
            return std::format("\tdecq\t{}", opnd_to_code(x.opnd));
          },
          [&opnd_to_code](const Or1 &x) -> std::string {
            return std::format("\torq\t$0x0001,\t{}", opnd_to_code(x.opnd));
          },
          [&opnd_to_code](const Sal1 &x) -> std::string {
            return std::format("\tsalq\t{}", opnd_to_code(x.opnd));
          },
          [&opnd_to_code](const Sar1 &x) -> std::string {
            return std::format("\tsarq\t{}", opnd_to_code(x.opnd));
          },
          [](const Repmovsl &) -> std::string { return "\trep movsq\t"; },
      },
      *instr);
}

bool in_memory(const Opnd &opnd) {
  return opnd.is<M>() || opnd.is<S>() || opnd.is<I>();
}

std::vector<Instr> mov(const Opnd &x, const Opnd &s) {
  /* Numeric literals with more than 32 bits cannot ne directly moved to
   * memory location */
  auto const big_numeric_literal = [](const Opnd &opnd) {
    return std::visit(utils::multifunc{
                          [](const Opnd::L &l) { return l.num > 0xFFFFFFFF; },
                          [](const auto &) { return false; },
                      },
                      *opnd);
  };
  if (x == s) {
    return {};
  } else if ((in_memory(x) and in_memory(s)) || big_numeric_literal(x)) {
    return {Mov{x, R{rax}}, Mov{R{rax}, s}};
  }
  return {Mov(x, s)};
}

/* Boxing for numeric values */
int box(int n) { return (n << 1) | 1; }

/*
  Compile binary operation

  compile_binop : env -> string -> env * instr list
 */
// template <typename Prg, Mode mode>
std::vector<Instr> compile_binop(Env &env, Opr op) {
  const auto suffix = [](Opr op) {
    const static std::unordered_map<Opr, std::string> ops = {
        {Opr::LT, "l"},   {Opr::LEQ, "le"}, {Opr::EQ, "e"},
        {Opr::NEQ, "ne"}, {Opr::GEQ, "ge"}, {Opr::GT, "g"},
    };

    auto it = ops.find(op);

    if (it != ops.end()) {
      return it->second;
    }
    failure("unknown operator");
    utils::unreachable();
  };

  std::pair<Opnd, Opnd> xy = env.peek2();

  const auto [x, y] = xy;
  /* For binary operations requiring no extra register */
  const auto without_extra =
      [&env](const std::function<std::vector<Instr>()> &op) {
        const auto _x = env.pop();
        return op();
      };
  /* For binary operations requiring rdx */
  const auto with_rdx =
      [&env](const std::function<std::vector<Instr>(Register::T)> &op) {
        if (not env.rdx_in_use()) {
          const auto _x = env.pop();
          return op(rdx);
        }
        const auto extra = env.allocate();
        const auto _extra = env.pop();
        const auto _x = env.pop();

        return utils::concat(std::vector<Instr>{Mov{rdx, extra}}, op(rdx),
                             std::vector<Instr>{Mov{extra, rdx}});
      };
  /* For binary operations requiring any extra register */
  const auto with_extra =
      [&env](const std::function<std::vector<Instr>(Opnd)> &op) {
        const auto extra = env.allocate();
        const auto _extra = env.pop();
        const auto _x = env.pop();

        if (in_memory(extra)) {
          return utils::concat(std::vector<Instr>{Mov{rdx, extra}}, op(rdx),
                               std::vector<Instr>{Mov{extra, rdx}});
        }
        return op(extra);
      };
  switch (op) {
  case Opr::DIV:
    return with_rdx([&x, &y](const auto &rdx) -> std::vector<Instr> {
      return {Mov{y, rax}, Sar1{rax}, Binop{"^", rdx, rdx},
              Cltd{},      Sar1{x},   IDiv{x},
              Sal1{rax},   Or1{rax},  Mov{rax, y}};
    });
  case Opr::MOD:
    return with_rdx([&x, &y](const auto &rdx) -> std::vector<Instr> {
      return {
          Mov{y, rax}, Sar1{rax}, Cltd{},   Sar1{x},
          IDiv{x},     Sal1{rdx}, Or1{rdx}, Mov{rdx, y},
      };
    });
  case Opr::LT:
  case Opr::LEQ:
  case Opr::EQ:
  case Opr::NEQ:
  case Opr::GEQ:
  case Opr::GT:
    return in_memory(x)
               ? with_extra([&x, &y, &op,
                             &suffix](const auto &extra) -> std::vector<Instr> {
                   return {
                       Binop{Opr::XOR, rax, rax},
                       Mov{x, extra},
                       Binop{"cmp", extra, y},
                       Set{suffix(op), Registers::rax},
                       Sal1{rax},
                       Or1{rax},
                       Mov{rax, y},
                   };
                 })
               : without_extra([&x, &y, &op, &suffix]() -> std::vector<Instr> {
                   return {
                       Binop{Opr::XOR, rax, rax},
                       Binop{Opr::CMP, x, y},
                       Set{suffix(op), Registers::rax},
                       Sal1{rax},
                       Or1{rax},
                       Mov{rax, y},
                   };
                 });
  case Opr::MULT:
    return without_extra([&x, &y, &op]() {
      return in_memory(y) ? std::vector<Instr>{
        Dec {y},
        Mov{x, rax},
        Sar1{ rax},
        Binop{op, y, rax},
        Or1 {rax},
        Mov{rax, y},
      } : std::vector<Instr>{
        Dec{y}, Mov{x, rax}, Sar1{rax},
        Binop{op, rax, y}, Or1{y},
      };
    });
  case Opr::AND:
    return with_extra([&x, &y, &op](const auto &extra) -> std::vector<Instr> {
      return {
          Dec{x},
          Mov{x, rax},
          Binop{op, x, rax},
          Mov{L{0}, rax},
          Set{"ne", Registers::rax},
          Dec{y},
          Mov{y, extra},
          Binop{op, y, extra},
          Mov{L{0}, extra},
          Set{"ne", as_register(extra)},
          Binop{op, extra, rax},
          Set{"ne", Registers::rax},
          Sal1{rax},
          Or1{rax},
          Mov{rax, y},
      };
    });
  case Opr::OR:
    return without_extra([&x, &y, &op]() -> std::vector<Instr> {
      return {
          Mov{y, rax},       Sar1{rax},      Sar1{x},
          Binop{op, x, rax}, Mov{L{0}, rax}, Set{"ne", Registers::rax},
          Sal1{rax},         Or1{rax},       Mov{rax, y},
      };
    });
  case Opr::ADD:
    return without_extra([&x, &y, &op]() {
      return in_memory(x) && in_memory(y) ?std::vector<Instr> {
        Mov{x, rax},
        Dec{ rax},
        Binop{op, rax, y},
      }  : std::vector<Instr>{
        Binop{op, x, y},
        Dec{ y},
      };
    });
  case Opr::SUB:
    return without_extra([&x, &y, &op]() {
      return in_memory(x) && in_memory(y) ?std::vector<Instr> {
        Mov{x, rax},
        Binop{op, rax, y},
        Or1{y},
      } :std::vector<Instr> {
        Binop{op, x, y},
        Or1{y},
      };
    });
  default:
    failure("Unexpected pattern: %s: %d", __FILE__, __LINE__);
    break;
  }
  utils::unreachable();
}

/* For pointers to be marked by GC as alive they have to be located on the
   stack. As we do not have control where does the C compiler locate them in
   the moment of GC, we have to explicitly locate them on the stack. And to
   the runtime function we are passing a reference to their location. */
const std::unordered_set<std::string> safepoint_functions = {
    utils::labeled("s__Infix_58"),     utils::labeled("substring"),
    utils::labeled("clone"),           utils::labeled_builtin("string"),
    utils::labeled("stringcat"),       utils::labeled("string"),
    utils::labeled_builtin("closure"), utils::labeled_builtin("array"),
    utils::labeled_builtin("sexp"),    utils::labeled("i__Infix_4343"),
    /* "makeArray"; not required as do not have ptr arguments */
    /* "makeString"; not required as do not have ptr arguments */
    /* "getEnv", not required as do not have ptr arguments */
    /* "set_args", not required as do not have ptr arguments */
    /* Lsprintf, or Bsprintf is an extra dirty hack that probably works */
};

const std::unordered_map<std::string, size_t> vararg_functions = {
    {utils::labeled("printf"), 1},
    {utils::labeled("fprintf"), 2},
    {utils::labeled("sprintf"), 1},
    {utils::labeled("failure"), 1},
};

namespace utils::call_compilation::tail {

// NOTE: all comands in result are in inversed order
void push_args_rec_inv(Env &env, std::vector<Instr> &acc, size_t n) {
  if (n == 0) {
    return;
  }
  const auto x = env.pop();
  utils::insert(acc, utils::reverse(mov(x, env.loc(ValT::Arg{n - 1}))));
  push_args_rec_inv(env, acc, n - 1);
}
std::vector<Instr> push_args(Env &env, size_t n) {
  std::vector<Instr> acc;
  push_args_rec_inv(env, acc, n);
  std::reverse(acc.begin(), acc.end());
  return acc;
}

} // namespace utils::call_compilation::tail
std::vector<Instr> compile_tail_call(Env &env,
                                     const std::optional<std::string> &fname,
                                     size_t nargs) {
  using namespace utils::call_compilation::tail;
  std::vector<Instr> pushs = push_args(env, nargs);

  std::optional<Instr> setup_closure;
  if (!fname) {
    const auto closure = env.pop();
    setup_closure = Mov{closure, r15};
  }

  Instr add_argc_counter = Mov{L{static_cast<int>(nargs)}, r11};

  Instr jump = fname ? Instr{Jmp{*fname}} : Instr{JmpI{r15}};

  env.allocate();
  return utils::concat(std::move(pushs), Instr{Mov{rbp, rsp}}, Instr{Pop{rbp}},
                       std::move(setup_closure), std::move(add_argc_counter),
                       std::move(jump));
}

namespace utils::call_compilation {

std::vector<Opnd> pop_arguments(Env &env, size_t n) {
  std::vector<Opnd> result;
  result.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    const auto x = env.pop();
    result.push_back(x);
  }
  std::reverse(result.begin(), result.end());
  return result;
};

namespace common {

std::pair<size_t, std::vector<Instr>> setup_arguments(Env &env, size_t nargs) {
  const auto move_arguments =
      [](std::vector<Opnd> &&args,
         std::vector<SymbolicStack::Location> &&arg_locs) {
        using Register = SymbolicStack::Register;

        assert(args.size() == arg_locs.size());

        std::vector<Instr> result;
        result.reserve(args.size());
        // NOTE: direction should be (fold left)
        for (size_t i = 0; i < args.size(); ++i) {
          result.push_back(
              arg_locs[i].is<Register>()
                  ? Instr{Mov{args[i], std::get<Register>(*arg_locs[i]).r}}
                  : /*Stack*/ Push{args[i]});
        }
        std::reverse(result.begin(), result.end());
        return result;
      };
  auto args = pop_arguments(env, nargs);
  auto [arg_locs, stack_slots] = env.arguments_locations(args.size());
  auto setup_args_code = move_arguments(std::move(args), std::move(arg_locs));
  return {stack_slots, std::move(setup_args_code)};
}

std::optional<Instr> setup_closure(Env &env,
                                   const std::optional<std::string> &fname) {
  if (!fname) {
    return {};
  }
  const auto closure = env.pop();
  return Mov{closure, r15};
}

Instr call(const std::optional<std::string> &fname) {
  return fname ? Instr{Call{*fname}} : Instr{CallI{r15}};
}

Instr add_argc_counter(const std::optional<std::string> &fname, size_t nargs) {
  const auto it =
      fname ? vararg_functions.find(*fname) : vararg_functions.end();
  size_t argc = it == vararg_functions.end() ? 0 : it->second;
  return Mov{L{static_cast<int>(nargs - argc)}, r11};
}

} // namespace common

std::pair<std::vector<Instr>, std::vector<Instr>> protect_registers(Env &env) {
  std::vector<Instr> pushr;
  std::vector<Instr> popr;
  if (env.has_closure) {
    pushr.push_back(Push{r15});
    popr.push_back(Pop{r15});
  }

  pushr = utils::concat(
      std::move(pushr),
      utils::transform<Opnd, Instr>(env.live_registers(),
                                    [](const auto &r) { return Push{r}; }));
  popr = utils::concat(
      std::move(popr),
      utils::transform<Opnd, Instr>(
          env.live_registers(), [](const auto &r) -> Instr { return Pop{r}; }));

  return {pushr, popr};
}

std::pair<std::optional<Instr>, std::optional<Instr>>
align_stack(size_t saved_registers, size_t stack_arguments) {
  const bool aligned = (saved_registers + stack_arguments) % 2 == 0;
  if (aligned && stack_arguments == 0) {
    return {{}, {}};
  }
  if (aligned) {
    return {{},
            {Binop{Opr::ADD, L{static_cast<int>(word_size * stack_arguments)},
                   rsp}}};
  }

  return {Push{filler},
          {Binop{Opr::ADD,
                 L{static_cast<int>(word_size * (1 + stack_arguments))}, rsp}}};
}

Instr move_result(Env &env) {
  const auto y = env.allocate();
  return Mov{rax, y};
}

} // namespace utils::call_compilation
std::vector<Instr> compile_common_call(Env &env,
                                       const std::optional<std::string> &fname,
                                       size_t nargs) {
  using namespace utils::call_compilation::common;
  using namespace utils::call_compilation;

  auto add_argc_counter_code = add_argc_counter(fname, nargs);

  auto [stack_slots, setup_args_code] = setup_arguments(env, nargs);
  auto [push_registers, pop_registers] = protect_registers(env);
  auto [align_prologue, align_epilogue] =
      align_stack(push_registers.size(), stack_slots);
  auto setup_closure_code = setup_closure(env, fname);
  auto call_code = call(fname);
  auto move_result_code = move_result(env);

  return utils::concat(
      std::move(push_registers), std::move(align_prologue),
      std::move(setup_args_code), std::move(setup_closure_code),
      std::move(add_argc_counter_code), std::move(call_code),
      std::move(align_epilogue), utils::reverse(std::move(pop_registers)),
      std::move(move_result_code));
}

namespace utils::call_compilation::safepoint {

std::pair<size_t, std::vector<Instr>>
setup_arguments(Env &env, const std::optional<std::string> &fname,
                size_t nargs) {
  auto args = pop_arguments(env, nargs);
  auto [arg_locs, stack_slots] = env.arguments_locations(args.size());
  auto setup_args_code =
      utils::transform<Opnd, Instr>(utils::reverse(std::move(args)),
                                    [](const auto &arg) { return Push{arg}; });
  setup_args_code.push_back(Mov{rsp, rdi});
  if (*fname == utils::labeled_builtin("closure")) {
    setup_args_code.push_back(Mov{L{box(nargs - 1)}, rsi});
  } else if (*fname == utils::labeled_builtin("sexp") ||
             *fname == utils::labeled_builtin("array")) {
    setup_args_code.push_back(Mov{L{box(nargs)}, rsi});
  }
  return {nargs, std::move(setup_args_code)};
}

Instr call(const std::optional<std::string> &fname) { return Call{*fname}; }

} // namespace utils::call_compilation::safepoint
std::vector<Instr>
compile_safepoint_call(Env &env, const std::optional<std::string> &fname,
                       size_t nargs) {
  using namespace utils::call_compilation::safepoint;
  using namespace utils::call_compilation;

  auto [stack_slots, setup_args_code] = setup_arguments(env, fname, nargs);
  auto [push_registers, pop_registers] = protect_registers(env);
  auto [align_prologue, align_epilogue] =
      align_stack(push_registers.size(), stack_slots);
  auto call_code = call(fname);
  auto move_result_code = move_result(env);

  return utils::concat(std::move(push_registers), std::move(align_prologue),
                       std::move(setup_args_code), std::move(call_code),
                       std::move(align_epilogue),
                       utils::reverse(std::move(pop_registers)),
                       std::move(move_result_code));
}

std::vector<Instr> compile_call(Env &env,
                                std::optional<std::string_view> fname_in,
                                size_t nargs, bool tail) {
  std::optional<std::string> fname;
  if (fname_in) {
    fname = (*fname_in)[0] == '.' ? utils::labeled_builtin(fname->substr(1))
                                  : std::string{*fname_in};
  }

  bool safepoint_call = false;
  bool allowed_function = true;
  if (fname) {
    safepoint_call = (safepoint_functions.count(*fname) != 0);
    const bool is_vararg = (vararg_functions.count(*fname) != 0);
    const bool is_internal = ((*fname)[0] == 'B');
    allowed_function = not is_internal && not is_vararg;
  }

  const bool same_arguments_count = env.nargs == nargs;
  const bool tail_call_optimization_applicable =
      tail && allowed_function && same_arguments_count;

  if (safepoint_call) {
    return compile_safepoint_call(env, fname, nargs);
  }
  if (tail_call_optimization_applicable) {
    return compile_tail_call(env, fname, nargs);
  }
  return compile_common_call(env, fname, nargs);
}

enum class Patt {
  BOXED,
  UNBOXED,
  ARRAY,
  STRING,
  SEXP,
  CLOSURE,
  STRCMP,
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

namespace utils::compile {

std::vector<Instr> stabs_scope(Env &env, const SMInstr::BEGIN &x,
                               const Scope &scope) {
  auto names = utils::transform<std::pair<std::string, int>, Instr>(
      scope.names, [](const auto &y) -> Instr {
        return Meta{std::format("\t.stabs \"{}:1\",128,0,0,-{}",
                                y.first /*name*/,
                                stack_offset(y.second /*index*/))};
      });

  std::vector<Instr> sub_stabs;
  for (const auto &sub : scope.subs) {
    insert(sub_stabs, stabs_scope(env, x, sub));
  }

  if (names.empty()) {
    return sub_stabs;
  }
  return utils::concat(
      std::move(names),
      Instr{Meta{std::format("\t.stabn 192,0,0,{}-{}", scope.blab, x.f)}},
      std::move(sub_stabs),
      Instr{Meta{std::format("\t.stabn 224,0,0,{}-{}", scope.elab, x.f)}});
}

} // namespace utils::compile

/* Symbolic stack machine evaluator

     compile : env -> prg -> env * instr list

   Take an environment, a stack machine program, and returns a pair ---
   the updated environment and the list of x86 instructions
*/
std::vector<Instr> compile(const Options &cmd, Env &env,
                           const std::vector<std::string> &imports,
                           const SMInstr &instr) {
  using namespace utils::compile;

  const std::string stack_state = env.mode.is_debug ? env.print_stack() : "";
  if (env.is_barrier()) {
    return std::visit( //
        utils::multifunc{
            //
            [&env](const SMInstr::LABEL &x) -> std::vector<Instr> {
              if (env.has_stack(x.s)) {
                env.drop_barrier();
                env.retrieve_stack(x.s);
                return {Label{x.s}};
              }
              env.drop_stack();
              return {};
            },
            [&env](const SMInstr::FLABEL &x) -> std::vector<Instr> {
              env.drop_barrier();
              return {Label{x.s}};
            },
            [](const SMInstr::SLABEL &x) -> std::vector<Instr> {
              return {Label{x.s}};
            },
            [](const auto &) -> std::vector<Instr> { return {}; },
        },
        *instr);
  } else {
    return std::visit( //
        utils::multifunc{
            //
            [&env](const SMInstr::PUBLIC &x)
                -> std::vector<Instr> { // NOTE: not required in bytecode
              env.register_public(x.name);
              return {};
            },
            [&env](const SMInstr::EXTERN &x)
                -> std::vector<Instr> { // NOTE: not required in bytecode
              env.register_extern(x.name);
              return {};
            },
            [](const SMInstr::IMPORT &)
                -> std::vector<Instr> { // NOTE: not required in bytecode
              return {};
            },
            [&env](const SMInstr::CLOSURE &x) -> std::vector<Instr> {
              // NOTE: probably will change for bytecode cmd
              const Externality ext =
                  env.is_external(x.name) ? Externality::E : Externality::I;
              const auto address = M{DataKind::F, ext, Addressed::A, x.name};
              const auto l = env.allocate();

              std::vector<Instr> result;
              result.reserve(x.closure.size());
              for (const auto &c : x.closure) {
                const auto cr = env.allocate();
                std::vector<Instr> mov_result = mov(env.loc(c), cr);
                result =
                    utils::concat(std::move(result), std::move(mov_result));
              }
              std::reverse(result.begin(), result.end());
              return utils::concat(
                  std::move(result), mov(address, l),
                  compile_call(env, ".closure", 1 + x.closure.size(), false));
            },
            [&env](const SMInstr::CONST &x) -> std::vector<Instr> {
              const auto s = env.allocate();
              return {Mov{L{box(x.n)}, s}};
            },
            [&env](const SMInstr::STRING &x) -> std::vector<Instr> {
              const auto addr = env.register_string(x.str);
              const auto l = env.allocate();
              return utils::concat(mov(addr, l),
                                   compile_call(env, ".string", 1, false));
            },
            [&env](const SMInstr::LDA &x) -> std::vector<Instr> {
              env.register_variable(x.v);
              const auto s = env.allocate();
              const auto s_ = env.allocate();
              return std::vector<Instr>{Lea{env.loc(x.v), rax}, Mov{rax, s},
                                        Mov{rax, s_}};
            },
            [&env](const SMInstr::LD &x) -> std::vector<Instr> {
              const auto s = env.allocate();
              return s.is<S>() || s.is<M>()
                         ? std::vector<Instr>{Mov{s, rax},
                                              Mov{rax, env.loc(x.v)}}
                         : std::vector<Instr>{Mov{s, env.loc(x.v)}};
            },
            [&env](const SMInstr::ST &x) -> std::vector<Instr> {
              env.register_variable(x.v);
              const auto s = env.peek();
              return s.is<S>() || s.is<M>()
                         ? std::vector<Instr>{Mov{s, rax},
                                              Mov{rax, env.loc(x.v)}}
                         : std::vector<Instr>{Mov{s, env.loc(x.v)}};
            },
            [&env](const SMInstr::STA &) -> std::vector<Instr> {
              return compile_call(env, ".sta", 3, false);
            },
            [&env](const SMInstr::STI &) -> std::vector<Instr> {
              const auto v = env.pop();
              const auto x = env.peek();
              return x.is<S>() || x.is<M>()
                         ? std::vector<Instr>{Mov{v, rdx},Mov{x, rax},Mov{rdx, I{0, rax}},Mov{rdx, x},
                                              }
                         : std::vector<Instr>{Mov{v, rax}, Mov{rax, I{0, x}}, Mov{rax, x}};
            },
            [&env](const SMInstr::BINOP &x) -> std::vector<Instr> {
              return compile_binop(env, x.opr);
            },
            [](const SMInstr::LABEL &x) -> std::vector<Instr> {
              return {Label{x.s}};
            },
            [](const SMInstr::FLABEL &x) -> std::vector<Instr> {
              return {Label{x.s}};
            },
            [](const SMInstr::SLABEL &x) -> std::vector<Instr> {
              return {Label{x.s}};
            },
            [&env](const SMInstr::JMP &x) -> std::vector<Instr> {
              env.set_stack(x.l);
              env.set_barrier();
              return {Jmp{x.l}};
            },
            [&env](const SMInstr::CJMP &y) -> std::vector<Instr> {
              const auto x = env.pop();
              env.set_stack(y.l);
              return {Sar1{x}, /*!!!*/ Binop{Opr::CMP, L{0}, x},
                      CJmp{y.s, y.l}};
            },
            [&env, &imports,
             &cmd](const SMInstr::BEGIN &x) -> std::vector<Instr> {
              {
                const bool is_safepoint = safepoint_functions.count(x.f) != 0;
                const bool is_vararg = vararg_functions.count(x.f) != 0;
                if (is_safepoint && is_vararg) {
                  failure("Function name %s is reserved for built-in",
                          x.f.c_str());
                }
              }

              const std::string name = (x.f[0] == 'L' ? x.f.substr(1) : x.f);

              const auto stabs = [&env, &x, &name]() -> std::vector<Instr> {
                if (!env.do_opt_stabs()) {
                  return {};
                }
                if (x.f == "main") {
                  return {Meta{"\t.type main, @function"}};
                }

                std::vector<Instr> func = {
                    Meta{std::format("\t.type {}, @function", name)},
                    Meta{
                        std::format("\t.stabs \"{}:F1\",36,0,0,{}", name, x.f)},
                };

                std::vector<Instr> arguments =
                    {} /* OCAML_VER: TODO: stabs for function arguments */;

                std::vector<Instr> variables;
                for (const auto &scope : x.scopes) {
                  utils::insert(variables, stabs_scope(env, x, scope));
                }

                return utils::concat(std::move(func), std::move(arguments),
                                     std::move(variables));
              };

              auto stabs_code = stabs();

              const auto check_argc = [&env, &cmd, &x,
                                       &name]() -> std::vector<Instr> {
                if (x.f == cmd.topname) {
                  return {};
                }

                auto argc_correct_label = x.f + "_argc_correct";
                auto pat_addr = // TODO: check is that is the same string to
                                // ocaml version one
                    env.register_string(
                        "Function %s called with incorrect arguments count. \
                         Expected: %d. Actual: %d\\n");
                auto name_addr = env.register_string(name);
                const auto pat_loc = env.allocate();
                const auto name_loc = env.allocate();
                const auto expected_loc = env.allocate();
                const auto actual_loc = env.allocate();
                std::vector<Instr> fail_call =
                    compile_call(env, "failure", 4, false);

                env.pop();
                return utils::concat(
                    std::vector<Instr>{
                        Meta{"# Check arguments count"},
                        Binop{Opr::CMP, L{x.nargs}, r11},
                        CJmp{"e", argc_correct_label},
                        Mov{r11, actual_loc},
                        Mov{L{x.nargs}, expected_loc},
                        Mov{name_addr, name_loc},
                        Mov{pat_addr, pat_loc},
                    },
                    std::move(fail_call), Instr{Label{argc_correct_label}});
              };
              auto check_argc_code = check_argc();
              env.assert_empty_stack();
              const bool has_closure = !x.closure.empty();
              env.enter(x.f, x.nargs, x.nlocals, has_closure);
              return utils::concat(
                std::move(stabs_code),
                Instr{Meta{"\t.cfi_startproc"}},
                (x.f == cmd.topname ? std::vector<Instr>{
                   Mov{M{DataKind::D, Externality::I, Addressed::V, "init"}, rax},
                   Binop{Opr::TEST, rax, rax},
                   CJmp{"z", "continue"},
                   Ret{},
                   Label{"_ERROR"},
                   Call{utils::labeled("binoperror")},
                   Ret{},
                   Label{"_ERROR2"},
                   Call {utils::labeled("binoperror2")},
                   Ret{},
                   Label{"continue"},
                   Mov {L {1}, M {DataKind::D, Externality::I, Addressed::V, "init"}},
                } : std::vector<Instr>{}),
                std::vector<Instr>{
                  Push{rbp},
                  Meta{"\t.cfi_def_cfa_offset\t8"},
                  Meta{"\t.cfi_offset 5, -8"},
                  Mov{rsp, rbp},
                  Meta{"\t.cfi_def_cfa_register\t5"},
                  Binop {Opr::SUB, C{env.lsize()}, rsp},
                  Mov {rdi, r12},
                  Mov {rsi, r13},
                  Mov {rcx, r14},
                  Mov {rsp, rdi},
                  Lea {filler, rsi},
                  Mov {C{env.get_allocated_size()}, rcx},
                  Repmovsl{},
                  Mov {r12, rdi},
                  Mov {r13, rsi},
                  Mov {r14, rcx},
                },
                (x.f == "main"? std::vector<Instr>{
                   /* Align stack as `main` function could be called misaligned */
                   Mov {L{0xF}, rax},
                   Binop{Opr::TEST, rsp, rax},
                   CJmp{"z", "ALIGNED"},
                   Push{filler},
                   Label{"ALIGNED"},
                   /* Initialize gc and arguments */
                   Push{rdi},
                   Push{rsi},
                   Call{"__gc_init"},
                   Pop{rsi},
                   Pop{rdi},
                   Call{"set_args"},
                } : std::vector<Instr>{}),
                (x.f == cmd.topname ? // TODO: optimize filter
                   utils::transform<std::string, Instr>(
                       utils::filter<std::string>(
                           std::vector<std::string>{imports},
                           [](const auto &i) { return i != "Std"; }),
                       [](const auto &i) -> Instr { 
                         return Call{std::format("init" + i)};
                }) : std::vector<Instr>{}),
                std::move(check_argc_code)
              );
            },
            [&env](const SMInstr::END &) -> std::vector<Instr> {
              const auto x = env.pop();
              env.assert_empty_stack();
              const auto &name = env.fname;
              std::optional<Instr> stabs =
                  env.do_opt_stabs()
                      ? Meta{std::format("\t.size {}, .-{}", name, name)}
                      : std::optional<Instr>{};

              std::vector<Instr> result = utils::concat(
                  std::vector<Instr>{
                      Mov{x, rax},
                      /*!!*/
                      Label{env.epilogue()},
                      Mov{rbp, rsp},
                      Pop{rbp},
                  },
                  std::optional<Instr>{name == "main"
                                           ? Binop{Opr::XOR, rax, rax}
                                           : std::optional<Instr>{}},
                  std::vector<Instr>{
                      Meta{"\t.cfi_restore\t5"},
                      Meta{"\t.cfi_def_cfa\t4, 4"},
                      Ret{},
                      Meta{"\t.cfi_endproc"},
                      Meta{
                          /* Allocate space for the symbolic stack
                             Add extra word if needed to preserve alignment */
                          std::format(
                              "\t.set\t{},\t{}", env.prefixed(env.lsize()),
                              (env.get_allocated() % 2 == 0
                                   ? (env.get_allocated() * word_size)
                                   : ((env.get_allocated() + 1) * word_size)))},
                      Meta{std::format("\t.set\t{},\t{}",
                                       env.prefixed(env.get_allocated_size()),
                                       env.get_allocated())},
                  },
                  std::move(stabs));

              env.leave();
              return result;
            },
            [&env](const SMInstr::RET &) -> std::vector<Instr> {
              const auto x = env.peek();
              return {Mov{x, rax}, Jmp{env.epilogue()}};
            },
            [&env](const SMInstr::ELEM &) -> std::vector<Instr> {
              return compile_call(env, ".elem", 2, false);
            },
            [&env](const SMInstr::CALL &x) -> std::vector<Instr> {
              return compile_call(env, x.fname, x.n, x.tail); // call
            },
            [&env](const SMInstr::CALLC &x) -> std::vector<Instr> {
              return compile_call(env, {}, x.n, x.tail); // closure call
            },
            [&env](const SMInstr::SEXP &x) -> std::vector<Instr> {
              const auto s = env.allocate();
              auto code = compile_call(env, ".sexp", x.n + 1, false);
              return utils::concat(mov(L{box(env.hash(x.tag))}, s),
                                   std::move(code));
            },
            [&env](const SMInstr::DROP &) -> std::vector<Instr> {
              env.pop();
              return {};
            },
            [&env](const SMInstr::DUP &) -> std::vector<Instr> {
              const auto x = env.peek();
              const auto s = env.allocate();
              return mov(x, s);
            },
            [&env](const SMInstr::SWAP &) -> std::vector<Instr> {
              const auto [x, y] = env.peek2();
              return {Push{x}, Push{y}, Pop{x}, Pop{y}};
            },
            [&env](const SMInstr::TAG &x) -> std::vector<Instr> {
              const auto s1 = env.allocate();
              const auto s2 = env.allocate();
              auto code = compile_call(env, ".tag", 3, false);
              return utils::concat(mov(L{box(env.hash(x.tag))}, s1),
                                   mov(L{box(x.n)}, s2), std::move(code));
            },
            [&env](const SMInstr::ARRAY &x) -> std::vector<Instr> {
              const auto s = env.allocate();
              auto code = compile_call(env, ".array_patt", 2, false);
              return utils::concat(std::vector<Instr>{Mov{L{box(x.n)}, s}},
                                   std::move(code));
            },
            [&env](const SMInstr::PATT &x) -> std::vector<Instr> {
              std::string fname;
              switch (x.patt) {
              case Patt::STRCMP:
                return compile_call(env, ".string_patt", 2, false);
              case Patt::BOXED:
                fname = ".boxed_patt";
                break;
              case Patt::UNBOXED:
                fname = ".unboxed_patt";
                break;
              case Patt::ARRAY:
                fname = ".array_tag_patt";
                break;
              case Patt::STRING:
                fname = ".string_tag_patt";
                break;
              case Patt::SEXP:
                fname = ".sexp_tag_patt";
                break;
              case Patt::CLOSURE:
                fname = ".closure_tag_patt";
                break;
              default:
                failure("Unexpected pattern %s: %d", __FILE__, __LINE__);
                break;
              }
              return compile_call(env, fname, 1, false);
            },
            [&env](const SMInstr::LINE &x) -> std::vector<Instr> {
              return env.gen_line(x.n);
            },
            [&env, &cmd](const SMInstr::FAIL &x) -> std::vector<Instr> {
              const auto v = x.val ? env.peek() : env.pop();
              const auto msg_addr = env.register_string(cmd.filename);
              const auto vr = env.allocate();
              const auto sr = env.allocate();
              const auto liner = env.allocate();
              const auto colr = env.allocate();
              auto code = compile_call(env, ".match_failure", 4, false);
              env.pop();
              return utils::concat(
                  std::vector<Instr>{
                      Mov{L{static_cast<int>(x.col)}, colr},
                      Mov{L{static_cast<int>(x.line)}, liner},
                      Mov{msg_addr, sr},
                      Mov{v, vr},
                  },
                  std::move(code));
            },
            [](const auto &) -> std::vector<Instr> {
              failure("invalid SM insn\n"); // TODO: better error
              utils::unreachable();
            },
        },
        *instr);
  }
}

std::vector<Instr> compile(const Options &cmd, Env &env,
                           const std::vector<std::string> &imports,
                           const std::vector<SMInstr> &code) {
  std::vector<Instr> result;
  for (const auto &instr : code) {
    result =
        utils::concat(std::move(result), compile(cmd, env, imports, instr));
  }
  return result;
}
