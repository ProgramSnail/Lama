// based on src/X86_64.ml

#include "../../runtime/runtime.h"

#include <format>
#include <memory>
#include <string>
#include <unordered_map>
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

} // namespace utils

enum class OS { // TODO: other oses
  LINUX,
};

struct CompilationMode {
  bool is_debug;
  OS os;
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

const auto none = Register::T{};
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
const std::array<Register::T, 6> argument_registers = {rdi, rsi, rdx,
                                                       rcx, r8,  r9};
const std::array<Register::T, 5> extra_caller_saved_registers = {r10, r11, r12,
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
    int pos; /* Position on the hardware stack */

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
    I(const I &other)
        : num(other.num), opnd(std::make_unique<Opnd>(*other.opnd)) {}

    I(I &&other) : num(other.num), opnd(std::move(other.opnd)) {}

    int num;
    std::unique_ptr<Opnd> opnd; /* Indirect operand with offset */

    bool operator==(const I &other) const = default;
  };

  using T = std::variant<R, S, M, C, L, I>;

  T val;

  Opnd(const Opnd &x) : val(x.val) {}
  Opnd(Opnd &&x) : val(std::move(x.val)) {}
  template <typename U>
    requires std::is_same_v<U, R> || std::is_same_v<U, S> ||
             std::is_same_v<U, M> || std::is_same_v<U, C> ||
             std::is_same_v<U, L> || std::is_same_v<U, I> ||
             std::is_same_v<U, T>
  Opnd(U &&x) : val(std::forward<U>(x)) {}

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
  template <typename T> Instr(T &&x) : val(std::forward<T>(x)) {}

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
    std::string op;
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
    std::string left;
    std::string right;
  }; // TODO: right names (?)
  /* a non-conditional jump by a name                      */
  struct Jmp {
    std::string name;
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

int stack_offset(int i) { return (i >= 0 ? (i + 1) : (-i + 1)) * word_size; }

std::string to_code(const std::string &env, const Opnd &opnd) {
  // TODO: check that 'env#prefixed·l' <-> l + env
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
            return std::format("{}{}@GOTPCREL(%rip)", x.name,
                               env); // TODO: does @ mean something (?)
          },
          [&env](const Opnd::C &x) {
            return std::format("${}{}", x.name, env);
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

// TODO: Instr to_string
std::string to_code(const std::string &env, const Instr &instr) {
  const auto opnd_to_code = [&env](const Opnd &opnd) -> std::string {
    return to_code(env, opnd);
  };

  const auto binop_to_code = [](const std::string &binop) -> std::string {
    static std::unordered_map<std::string, std::string> ops = {
        {"+", "addq"}, {"-", "subq"}, {"*", "imulq"},  {"&&", "andq"},
        {"!!", "orq"}, {"^", "xorq"}, {"cmp", "cmpq"}, {"test", "test"},
    };

    auto it = ops.find(binop);

    if (it != ops.end()) {
      return it->second;
    }
    failure("unknown binary operator");
    utils::unreachable();
  };

  // TODO: check that 'env#prefixed·l' <-> l + env
  return std::visit(
      utils::multifunc{
          [](const Cltd &x) -> std::string { return "\tcqo"; },
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
          [](const Ret &x) -> std::string { return "\tret"; },
          [&env](const Call &x) -> std::string {
            return std::format("\tcall\t{}{}", x.name, env);
          },
          [&opnd_to_code](const CallI &x) -> std::string {
            return std::format("\tcall\t*({})", opnd_to_code(x.val));
          },
          [&env](const Label &x) -> std::string {
            return std::format("{}{}:\n", x.name, env);
          },
          [&env](const Jmp &x) -> std::string {
            return std::format("\tjmp\t{}{}", x.name, env);
          },
          [&opnd_to_code](const JmpI &x) -> std::string {
            return std::format("\tjmp\t*({})", opnd_to_code(x.opnd));
          },
          [&env](const CJmp &x) -> std::string {
            return std::format("\tj{}\t{}{}", x.left, x.right, env);
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
          [](const Repmovsl &x) -> std::string { return "\trep movsq\t"; },
      },
      *instr);
}

bool in_memory(const Opnd &opnd) {
  return std::visit(utils::multifunc{
                        [](const Opnd::M &) { return true; },
                        [](const Opnd::S &) { return true; },
                        [](const Opnd::I &) { return true; },
                        [](const Opnd::C &) { return false; },
                        [](const Opnd::R &) { return false; },
                        [](const Opnd::L &) { return false; },
                    },
                    *opnd);
}

std::vector<Instr> mov(const Opnd &x, const Opnd &s) {
  /* Numeric literals with more than 32 bits cannot ne directly moved to memory
   * location */
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
