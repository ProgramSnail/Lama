#include "interpreter.h"

#include "../../runtime/gc.h"
#include "../../runtime/runtime.h"

#include "runtime_externs.h"
#include "stack.h"
#include "types.h"
#include "utils.h"

struct State s;

static inline int ip_read_int(char **ip) {
  *ip += sizeof(int);
  return *(int *)((*ip) - sizeof(int));
}

static inline char ip_read_byte(char **ip) { return *(*ip)++; }

static inline char *ip_read_string(char **ip, bytefile *bf) {
  return get_string(bf, ip_read_int(ip));
}

const size_t BUFFER_SIZE = 1000;

void run(bytefile *bf, int argc, char **argv) {
  void *stack[STACK_SIZE];
  void *buffer[BUFFER_SIZE];
  construct_state(bf, &s, stack);

#ifdef DEBUG_VERSION
  printf("--- interpreter run ---\n");
#endif

  const size_t OPS_SIZE = 13;
  const char *ops[] = {
      "+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  aint (*ops_func[])(void *, void *) = {
      &Ls__Infix_43,   // +
      &Ls__Infix_45,   // -
      &Ls__Infix_42,   // *
      &Ls__Infix_47,   // /
      &Ls__Infix_37,   // %
      &Ls__Infix_60,   // <
      &Ls__Infix_6061, // <=
      &Ls__Infix_62,   // >
      &Ls__Infix_6261, // >=
      &Ls__Infix_6161, // ==
      &Ls__Infix_3361, // !=
      &Ls__Infix_3838, // &&
      &Ls__Infix_3333, // !!
  };

  const size_t PATS_SIZE = 7;
  const char *pats[] = {"=str", "#string", "#array", "#sexp",
                        "#ref", "#val",    "#fun"};

  // argc, argv
  {
    s_push_i(BOX(argc));
    void **argv_strs = calloc(argc, sizeof(void *));
    for (size_t i = 0; i < argc; ++i) {
      argv_strs[i] = Bstring((aint *)&argv[i]);
    }
    s_push(Barray((aint *)&argv_strs, argc));
    free(argv_strs);
  }

#ifdef DEBUG_VERSION
  printf("- loop start\n");
#endif

  do {
    // char *before_op_ip = s.ip; // save to set s.prev_ip
    bool call_happened = false;

    char x = ip_read_byte(&s.ip), h = (x & 0xF0) >> 4, l = x & 0x0F;

#ifdef DEBUG_VERSION
    printf("0x%.8x\n", s.ip - bf->code_ptr - 1);
#endif

    switch (h) {
    case 15:
      goto stop;

    /* BINOP */
    case 0: // BINOP ops[l-1]
      if (l > OPS_SIZE) {
        s_failure(&s, "undefined binop type index");
      }
      if (l < 1) {
        s_failure(&s, "negative binop type index");
      }
      void *left = s_pop();
      void *right = s_pop();
      s_push((void *)ops_func[l - 1](right, left));
      break;

    case 1:
      switch (l) {
      case 0: // CONST %d
        s_push_i(BOX(ip_read_int(&s.ip)));
        break;

      case 1: { // STRING %s
        void *str = ip_read_string(&s.ip, bf);
        s_push(Bstring((aint *)&str));
        break;
      }

      case 2: { // SEXP %s %d // create sexpr with tag=%s and %d elements from
                // stack
        // params read from stack
        const char *name = ip_read_string(&s.ip, bf);
        aint args_count = ip_read_int(&s.ip);
#ifdef DEBUG_VERSION
        printf("tag hash is %i, n is %i\n", UNBOX(LtagHash((char *)name)),
               args_count);
#endif

        if (args_count < 0) {
          s_failure(&s, "args count should be >= 0");
        }
        void **opr_buffer = args_count >= BUFFER_SIZE ? calloc(args_count + 1, sizeof(void *)) : buffer;

        for (size_t i = 1; i <= args_count; ++i) {
          opr_buffer[args_count - i] = s_pop();
        }
        opr_buffer[args_count] = (void *)LtagHash((char *)name);

        void *sexp = Bsexp((aint *)opr_buffer, BOX(args_count + 1));

        push_extra_root(sexp);
        s_push(sexp);
        pop_extra_root(sexp);

        if (args_count >= BUFFER_SIZE) {
          free(opr_buffer);
        }
        break;
      }

      case 3: { // STI - write by ref (?)
        // NOTE: example not found, no checks done
        void *elem = s_pop();
        void **addr = (void **)s_pop();
        *addr = elem;
        s_push(elem);
        break;
      }

      case 4: { // STA - write to array elem
        void *elem = s_pop();
        aint index = s_pop_i();
        void *data = s_pop();
        s_push(Bsta(data, index, elem));
        break;
      }

      case 5: { // JMP 0x%.8x
        int jmp_p = ip_read_int(&s.ip);

        if (jmp_p < 0) {
          s_failure(&s, "negative file offset jumps are not allowed");
        }
        s.ip = bf->code_ptr + jmp_p;
        break;
      }

      case 6: // END
        if (!s_is_empty() && s.fp->prev_fp != 0) {
          s.fp->ret = *s_peek();
          s_pop();
        }
        s_exit_f();
        break;

      case 7: // RET
        if (!s_is_empty() && s.fp->prev_fp != 0) {
          s.fp->ret = *s_peek();
          s_pop();
        }
        s_exit_f();
        break;

      case 8: // DROP
        s_pop();
        break;

      case 9: // DUP
      {
        s_push(*s_peek());
        break;
      }

      case 10: // SWAP
      {
        void* x = s_pop();
        void* y = s_pop();
        s_push(y);
        s_push(x);

        // if (s.sp + 1 >= s.stack + STACK_SIZE ||
        //     (s.fp != NULL && s.sp + 1 >= f_locals(s.fp))) {
        //   s_failure(&s, "can't SWAP: < 2 values on stack");
        // }
        // void *v = *s.sp;
        // push_extra_root(v);
        // *s.sp = *(s.sp + 1);
        // *(s.sp + 1) = v;
        // pop_extra_root(v);
      } break;

      case 11: // ELEM
      {
        aint index = s_pop_i();
        void *data = s_pop();
        s_push(Belem(data, index));
      } break;

      default:
        s_failure(&s, "invalid opcode"); // %d-%d\n", h, l);
      }
      break;

    case 2: { // LD %d
      void **var_ptr =
          var_by_category(to_var_category(l), ip_read_int(&s.ip));
      s_push(*var_ptr);
      break;
    }

    case 3: { // LDA %d
      void **var_ptr =
          var_by_category(to_var_category(l), ip_read_int(&s.ip));
      s_push(*var_ptr);
      break;
    }
    case 4: { // ST %d
      void **var_ptr =
          var_by_category(to_var_category(l), ip_read_int(&s.ip));
      *var_ptr = *s_peek();
      break;
    }

    case 5:
      switch (l) {
      case 0: { // CJMPz 0x%.8x
        int jmp_p = ip_read_int(&s.ip);

        if (jmp_p < 0) {
          s_failure(&s, "negative file offset jumps are not allowed");
        }
        if (UNBOX(s_pop_i()) == 0) {
          s.ip = bf->code_ptr + jmp_p;
        }
        break;
      }

      case 1: { // CJMPnz  0x%.8x
        int jmp_p = ip_read_int(&s.ip);

        if (jmp_p < 0) {
          s_failure(&s, "negative file offset jumps are not allowed");
        }
        if (UNBOX(s_pop_i()) != 0) {
          s.ip = bf->code_ptr + jmp_p;
        }
        break;
      }

      case 2: { // BEGIN %d %d // function begin
        int args_sz = ip_read_int(&s.ip);
        int locals_sz = ip_read_int(&s.ip);
        if (s.fp != NULL && s.call_ip == NULL) {
          s_failure(&s, "begin should only be called after call");
        }
        s_enter_f(s.call_ip /*ip from call*/, s.is_closure_call, args_sz,
                  locals_sz);
        break;
      }

      case 3: { // CBEGIN %d %d
        // NOTE: example not found, no checks done
        int args_sz = ip_read_int(&s.ip);
        int locals_sz = ip_read_int(&s.ip);
        if (s.fp != NULL && s.call_ip == NULL) {
          s_failure(&s, "begin should only be called after call");
        }
        s_enter_f(s.call_ip /*ip from call*/, s.is_closure_call, args_sz,
                  locals_sz);
        break;
      }

      case 4: // CLOSURE 0x%.8x
      {
        aint call_offset = ip_read_int(&s.ip);
        aint args_count = ip_read_int(&s.ip);
        for (aint i = 0; i < args_count; i++) {
          aint arg_type = ip_read_byte(&s.ip);
          aint arg_id = ip_read_int(&s.ip);

          void **var_ptr =
              var_by_category(to_var_category(l), ip_read_int(&s.ip));
          s_push(*var_ptr);
        }
        s_push(bf->code_ptr + call_offset);

        void *closure = Bclosure((aint *)s.sp, args_count);

        push_extra_root(closure);
        s_popn(args_count + 1);
        s_push(closure);
        pop_extra_root(closure);
        break;
      }

      case 5: {                               // CALLC %d // call clojure
        aint args_count = ip_read_int(&s.ip); // args count

        call_happened = true;
        s.is_closure_call = true;
        s.call_ip = s.ip;

        s.ip = Belem(*s_nth(args_count), BOX(0)); // use offset instead ??
        break;
      }

      case 6: { // CALL 0x%.8x %d // call function
        int call_p = ip_read_int(&s.ip);
        ip_read_int(&s.ip); // args count

        call_happened = true;
        s.is_closure_call = false;
        s.call_ip = s.ip;

        if (call_p < 0) {
          s_failure(&s, "negative file offset jumps are not allowed");
        }
        s.ip = bf->code_ptr + call_p;
        break;
      }

      case 7: { // TAG %s %d
        const char *name = ip_read_string(&s.ip, bf);
        aint args_count = ip_read_int(&s.ip);

#ifdef DEBUG_VERSION
        printf("tag hash is %i, n is %i, peek is %i\n",
               UNBOX(LtagHash((char *)name)), args_count, s_peek(&s));
#endif

        s_push_i(Btag(s_pop(), LtagHash((char *)name), BOX(args_count)));
        break;
      }

      case 8: // ARRAY %d
        s_push_i(Barray_patt(s_pop(), BOX(ip_read_int(&s.ip))));
        break;

      case 9: {                        // FAIL %d %d
        int line = ip_read_int(&s.ip); // ??
        int col = ip_read_int(&s.ip);  // ??
        Bmatch_failure(s_pop(), argv[0], BOX(line), BOX(col));
        break;
      }

      case 10: // LINE %d
        s.current_line = ip_read_int(&s.ip);
        // maybe some metainfo should be collected
        break;

      default:
        s_failure(&s, "invalid opcode"); // %d-%d\n", h, l);
      }
      break;

    case 6: // PATT pats[l]
      // {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"}
      switch (l) {
      case 0: // =str
        s_push_i(Bstring_patt(s_pop(), s_pop()));
        break;
      case 1: // #string
        s_push_i(Bstring_tag_patt(s_pop()));
        break;
      case 2: // #array
        s_push_i(Barray_tag_patt(s_pop()));
        break;
      case 3: // #sexp
        s_push_i(Bsexp_tag_patt(s_pop()));
        break;
      case 4: // #ref
        s_push_i(Bunboxed_patt(s_pop()));
        break;
      case 5: // #val
        s_push_i(Bboxed_patt(s_pop()));
        break;
      case 6: // #fun
        s_push_i(Bclosure_tag_patt(s_pop()));
        break;
      default:
        s_failure(&s, "invalid opcode"); // %d-%d\n", h, l);
      }
      break;

    case 7: {
      switch (l) {
      case 0: // CALL Lread
        s_push_i(Lread());
        break;

      case 1: // CALL Lwrite
        Lwrite(*s_peek_i());
        break;

      case 2: // CALL Llength
        s_push_i(Llength(s_pop()));
        break;

      case 3: { // CALL Lstring
        void *val = s_pop();
        void *str = Lstring((aint *)&val);
        s_push(str);
        break;
      }

      case 4: { // CALL Barray %d
        size_t elem_count = ip_read_int(&s.ip);
        if (elem_count < 0) {
          s_failure(&s, "elements count should be >= 0");
        }

        void **opr_buffer = elem_count > BUFFER_SIZE ? calloc(elem_count, sizeof(void *)) : buffer;
        for (size_t i = 0; i < elem_count; ++i) {
          opr_buffer[elem_count - i - 1] = s_pop();
        }

        void *array =
            Barray((aint *)opr_buffer, BOX(elem_count)); // NOTE: not shure if elems should be added
        s_push(array);

        if (elem_count > BUFFER_SIZE) {
          free(opr_buffer);
        }
        break;
      }

      default:
        s_failure(&s, "invalid opcode"); // %d-%d\n", h, l);
      }
    } break;

    default:
      s_failure(&s, "invalid opcode"); // %d-%d\n", h, l);
    }

    if (!call_happened) {
      s.is_closure_call = false;
      s.call_ip = NULL;
    }

    if (s.fp == NULL) {
      break;
    }
#ifdef DEBUG_VERSION
    print_stack(&s);
#endif
  } while (1);
stop:;
#ifdef DEBUG_VERSION
  printf("--- run end ---\n");
#endif
  cleanup_state(&s);
}
