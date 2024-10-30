#include "interpreter.h"
#include "parser.h"
#include "runtime.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    failure("no file name provided");
  }
  if (argc > 2) {
    failure("too many arguments");
  }

  
  bytefile *f = read_file (argv[1]);
  run(f);
//   dump_file (stdout, f);

  free(f);

  return 0;
}
