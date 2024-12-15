#include <iostream>

#include "analyzer.hpp"
#include "parser.hpp"

extern "C" {
#include "../../runtime/runtime.h"
#include "interpreter.h"
#include "parser.h"
#include "utils.h"
}

int main(int argc, char **argv) {
  if (argc < 2) {
    failure("no execution option");
  }

  bool do_verification = false;
  bool do_interpretation = false;
  bool do_print = false;
  if (strcmp(argv[1], "-vi") == 0) {
    do_verification = true;
    do_interpretation = true;
  } else if (strcmp(argv[1], "-i") == 0) {
    do_interpretation = true;
  } else if (strcmp(argv[1], "-v") == 0) {
    do_verification = true;
  } else if (strcmp(argv[1], "-p") == 0) {
    do_print = true;
  } else {
    failure("wrong execution option (acceptable options - '-i', '-v', '-vi')");
  }

  if (argc < 3) {
    failure("no file name provided");
  }

  Bytefile *f = read_file(argv[2]);
  // #ifdef DEBUG_VERSION
  //   dump_file (stdout, f);
  // #endif
  if (do_print) {
    print_file(*f, std::cout);
  }
  if (do_verification) {
    analyze(f);
  }
  if (do_interpretation) { // TODO: switch between enabled/disabled verification
    run(f, argc - 2, argv + 2);
  }

  free(f->global_ptr);
  free(f);

  return 0;
}
