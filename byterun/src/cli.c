#include "interpreter.h"
#include "module_manager.h"
#include "parser.h"
#include "types.h"
#include "utils.h"
#include "../../runtime/runtime.h"

int main(int argc, char** argv) {
  if (argc < 3) {
    failure("two arguments should be provided: execution option (-i/-p) and file name\n");
  }


  bytefile *f = read_file(argv[2]);
  if (strcmp(argv[1], "-i") == 0) {
    uint main_mod_id = mod_add(f);

    size_t stack[STACK_SIZE];
    run_init(stack);

    run_init_mod_rec(main_mod_id);
    run_prepare_exec(argc - 1, argv + 1);
    // TODO: run all included modules before
    run_mod(main_mod_id, argc - 1, argv + 1);
  } else if (strcmp(argv[1], "-p") == 0) {
    dump_file (stdout, f);
  } else {
    failure("undefined execution mode. Execution mode should be -i (interpret) or -p (print)\n");
  }

  free(f);

  return 0;
}
