#include <fstream>
#include <iostream>

#include "analyzer.hpp"
#include "compiler.hpp"
#include "parser.hpp"
#include "sm_parser.hpp"

extern "C" {
#include "../../runtime/runtime.h"
#include "interpreter.h"
#include "module_manager.h"
#include "parser.h"
#include "types.h"
#include "utils.h"
}

int main(int argc, char **argv) {
  if (argc < 2) {
    failure("no execution option");
  }

  bool do_verification = false;
  bool do_interpretation = false;
  bool do_print = false;
  if (strcmp(argv[1], "-i") == 0) {
    do_interpretation = true;
  } else if (strcmp(argv[1], "-ds") == 0) { // TODO: TMP, FOR CHECKS
    std::ifstream file(argv[2]);
    std::cout << "-- parse\n";
    auto instrs = parse_sm(file);
    std::cout << "-- print\n";
    for (auto &instr : instrs) {
      std::cout << print_sm(instr) << "\n";
    }
    return 0;
  } else if (strcmp(argv[1], "-s") == 0) {
    std::ifstream file(argv[2]);
    // std::cout << "-- parse\n";
    auto instrs = parse_sm(file);
    // std::cout << "-- compile\n";
    auto asm_instrs = compile_to_code(instrs);
    for (auto &instr : asm_instrs) {
      std::cout << instr << "\n";
    }
    return 0;
  }
#ifdef WITH_CHECK
  else if (strcmp(argv[1], "-vi") == 0) {
    do_verification = true;
    do_interpretation = true;
  } else if (strcmp(argv[1], "-v") == 0) {
    do_verification = true;
  }
#endif
  else if (strcmp(argv[1], "-p") == 0) {
    do_print = true;
  } else {
#ifdef WITH_CHECK
    failure("wrong execution option (acceptable options - '-i')");
#else
    failure("wrong execution option (acceptable options - '-i', '-v', '-vi')");
#endif
  }

  if (argc < 3) {
    failure("no file name provided");
  }

  Bytefile *f = read_file(argv[2]);
  if (do_print) {
    print_file(*f, std::cout);
  }
  if (do_verification || do_interpretation) {
    size_t stack[STACK_SIZE];
    run_init(stack);
    f = run_with_imports(f, argc - 2, argv + 2, do_verification);
  }

  free(f);
  return 0;
}
