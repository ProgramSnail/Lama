#include "interpreter.h"
#include "parser.h"
#include "utils.h"
#include "../../runtime/runtime.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    failure("no file name provided");
  }

  // printf("size of aint is %i\n", sizeof(aint));
  bytefile *f = read_file(argv[1]);
#ifdef DEBUG_VERSION
  dump_file (stdout, f);
#endif
  run(f, argc - 1, argv + 1);

  free(f->global_ptr);
  free(f);

  return 0;
}
