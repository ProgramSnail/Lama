#include "interpreter.h"
#include "module_manager.h"
#include "parser.h"
#include "utils.h"
#include "../../runtime/runtime.h"

int main(int argc, char** argv) {
  if (argc < 3) {
    failure("two arguments should be provided: execution option (-i/-p) and file name\n");
  }

  bytefile *f = read_file(argv[2]);
  uint main_mod_id = mod_add(f);
  if (strcmp(argv[1], "-i") == 0) {
    run(main_mod_id, argc - 1, argv + 1); // TODO: init all modules, execute all prev. modules
  } else if (strcmp(argv[1], "-p") == 0) {
    dump_file (stdout, f);
  } else {
    failure("undefined execution mode. Execution mode should be -i (interpret) or -p (print)\n");
  }

  free(f);

  return 0;
}
