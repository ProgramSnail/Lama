#include "interpreter.h"

#include "../../runtime/gc.h"
#include "../../runtime/runtime.h"

#include "module_manager.h"
#include "runtime_externs.h"
#include "stack.h"
#include "types.h"
#include "utils.h"

void *__start_custom_data;
void *__stop_custom_data;

#define ASSERT_UNBOXED(memo, x)                                                \
  do                                                                           \
    if (!UNBOXED(x))                                                           \
      failure("%s: unboxed value expected in %s\n", __LINE__, memo);                         \
  while (0)

struct State s;

static inline uint16_t ip_read_half_int(char **ip) {
#ifdef WITH_CHECK
  return ip_read_half_int_unsafe(ip);
#else
  return ip_read_half_int_safe(ip, s.bf);
#endif
}

static inline int ip_read_int(char **ip) {
#ifdef WITH_CHECK
  return ip_read_int_unsafe(ip);
#else
  return ip_read_int_safe(ip, s.bf);
#endif
}

static inline uint8_t ip_read_byte(char **ip) {
#ifdef WITH_CHECK
  return ip_read_byte_unsafe(ip);
#else
  return ip_read_byte_safe(ip, s.bf);
#endif
}

static inline const char *ip_read_string(char **ip) {
#ifdef WITH_CHECK
  return get_string_unsafe(s.bf, ip_read_int(ip));
#else
  return get_string_safe(s.bf, ip_read_int(ip));
#endif
}

const size_t BUFFER_SIZE = 1000;

void run_init(size_t *stack) {
  init_state(&s, (void**)stack);
}

void set_argc_argv(int argc, char **argv) {
    s_push_i(BOX(argc));
#ifdef DEBUG_VERSION
    printf("- argc: %i\n", argc);
#endif
    for (size_t i = 0; i < argc; ++i) {
#ifdef DEBUG_VERSION
      printf("- arg: %s\n", argv[argc - i - 1]);
#endif
      s_push(Bstring((aint *)&argv[argc - i - 1]));
    }
    s_push(Barray((aint *)s_peek(), argc));
    void *argv_elem = s_pop();
    s_popn(argc);
    s_push(argv_elem);

#ifdef DEBUG_VERSION
  print_stack(s);
  printf("- state init done\n");
#endif
}

void call_Barray(size_t elem_count, char** ip, void** buffer) {
    // size_t elem_count = ip_read_int(ip);

    bool use_new_buffer = (elem_count > BUFFER_SIZE);

    void **opr_buffer = (void**)(use_new_buffer
                            ? alloc(elem_count * sizeof(void *))
                            : buffer);

    if (use_new_buffer) {
      for (size_t i = 0; i < elem_count; ++i) {
        opr_buffer[i] = 0;
        push_extra_root(&opr_buffer[i]);
      }
    }

    for (size_t i = 0; i < elem_count; ++i) {
      opr_buffer[elem_count - i - 1] = s_pop();
    }

    // s_rotate_n(elem_count);

    // NOTE: not sure if elems should be added
    void *array =
        Barray((aint *)opr_buffer,
               BOX(elem_count)); 

    // void *array = Barray((aint *)s_peek(), BOX(elem_count));
    s_push(array);

    if (use_new_buffer) {
      for (size_t i = 0; i < elem_count; ++i) {
        pop_extra_root(&opr_buffer[i]);
      }
      free(opr_buffer);
    }
}

void run_main(Bytefile* bf, int argc, char **argv) {
#ifdef DEBUG_VERSION
  printf("--- init state ---\n");
#endif

  prepare_state(bf, &s);

  void *buffer[BUFFER_SIZE];

#ifdef DEBUG_VERSION
  printf("--- run begin ---\n");
#endif

  do {
    bool call_happened = false;

#ifndef WITH_CHECK
    if (s.ip >= s.bf->code_ptr + s.bf->code_size) {
      s_failure(&s, "instruction pointer is out of range (>= size)");
    }

    if (s.ip < s.bf->code_ptr) {
      s_failure(&s, "instruction pointer is out of range (< 0)");
    }
#endif

    s.instr_ip = s.ip;
    uint8_t x = ip_read_byte(&s.ip), h = (x & 0xF0) >> 4, l = x & 0x0F;

// #ifdef DEBUG_VERSION
    printf("0x%.8x: %s\n", s.ip - s.bf->code_ptr - 1, read_cmd(s.ip - 1, s.bf));
// #endif

    switch (h) {
    case CMD_EXIT:
      goto stop;

    /* BINOP */
    case CMD_BINOP: { // BINOP ops[l-1]
      void *snd = s_pop();
      void *fst = s_pop();
      int op = l - 1;
      if (op == CMD_BINOP_SUB) {
        s_push_i(Ls__Infix_45(fst, snd));
      } else if (op == CMD_BINOP_EQ) {
        s_push_i(Ls__Infix_6161(fst, snd));
      } else {
        switch (op) {
#define BINOP_OPR(val, op)                                                     \
  case val:                                                                    \
    ASSERT_UNBOXED("captured op:1", fst);                                      \
    ASSERT_UNBOXED("captured op:2", snd);                                      \
    s_push_i(BOX(UNBOX(fst) op UNBOX(snd)));                                   \
    break;
          FORALL_BINOP(BINOP_OPR)
#undef BINOP_OPR

        default:
          s_failure(&s, "interpreter: invalid opcode"); // %d-%d\n", h, l);
          break;
        }
      }
      break;
    }

    case CMD_BASIC:
      switch (l) {
      case CMD_BASIC_CONST: // CONST %d
        s_push_i(BOX(ip_read_int(&s.ip)));
        break;

      case CMD_BASIC_STRING: { // STRING %s
        void *str = (void *)ip_read_string(&s.ip);
        s_push(Bstring((aint *)&str));
        break;
      }

      case CMD_BASIC_SEXP: { // SEXP %s %d // create sexpr with tag=%s and %d
                             // elements from
                             // stack
        // params read from stack
        const char *name = ip_read_string(&s.ip);
        size_t args_count = ip_read_int(&s.ip);
#ifdef DEBUG_VERSION
        printf("tag hash is %i, n is %i\n", UNBOX(LtagHash((char *)name)),
               args_count);
#endif

        bool use_new_buffer = (args_count >= BUFFER_SIZE);

        void **opr_buffer = (void**)(use_new_buffer
                                ? alloc((args_count + 1) * sizeof(void *))
                                : buffer);

        if (use_new_buffer) {
          for (size_t i = 0; i <= args_count; ++i) {
            opr_buffer[i] = 0;
            push_extra_root(&opr_buffer[i]);
          }
        }

        // s_put_nth(args_count, (void *)LtagHash((char *)name));

        for (size_t i = 1; i <= args_count; ++i) {
          opr_buffer[args_count - i] = s_pop();
        }
        opr_buffer[args_count] = (void *)LtagHash((char *)name);

        void *sexp = Bsexp((aint *)opr_buffer, BOX(args_count + 1));
        // void *sexp = Bsexp((aint *)s_peek(), BOX(args_count + 1));
        // s_popn(args_count + 1);

        s_push(sexp);

        if (use_new_buffer) {
          for (size_t i = 0; i <= args_count; ++i) {
            pop_extra_root(&opr_buffer[i]);
          }
          free(opr_buffer);
        }
        break;
      }

      case CMD_BASIC_STI: { // STI - write by ref (?)
        // NOTE: example not found, no checks done
        void *elem = s_pop();
        void **addr = (void **)s_pop();
        *addr = elem;
        s_push(elem);
        break;
      }

      case CMD_BASIC_STA: { // STA - write to array elem
        void *elem = s_pop();
        aint index = s_pop_i();
        void *data = s_pop();
        s_push(Bsta(data, index, elem));
        break;
      }

      case CMD_BASIC_JMP: { // JMP 0x%.8x
        uint jmp_p = ip_read_int(&s.ip);

#ifndef WITH_CHECK
        if (jmp_p >= s.bf->code_size) {
          s_failure(&s, "jump out of file");
        }
#endif
        s.ip = s.bf->code_ptr + jmp_p;
        break;
      }

      case CMD_BASIC_END: // END
        if (!s_is_empty() && s.fp->prev_fp != 0) {
          s.fp->ret = *s_peek();
          s_pop();
        }
        s_exit_f();
        break;

      case CMD_BASIC_RET: // RET
        if (!s_is_empty() && s.fp->prev_fp != 0) {
          s.fp->ret = *s_peek();
          s_pop();
        }
        s_exit_f();
        break;

      case CMD_BASIC_DROP: // DROP
        s_pop();
        break;

      case CMD_BASIC_DUP: // DUP
      {
        s_push(*s_peek());
        break;
      }

      case CMD_BASIC_SWAP: // SWAP
      {
        void *x = s_pop();
        void *y = s_pop();
        s_push(y);
        s_push(x);
      } break;

      case CMD_BASIC_ELEM: // ELEM
      {
        aint index = s_pop_i();
        void *data = s_pop();
        s_push(Belem(data, index));
      } break;

      default:
        s_failure(&s, "interpreter: basic, invalid opcode"); // %d-%d\n", h, l);
      }
      break;

    case CMD_LD: { // LD %d
      void **var_ptr = var_by_category(to_var_category(l), ip_read_int(&s.ip));
      s_push(*var_ptr);
      break;
    }

    case CMD_LDA: { // LDA %d
      void **var_ptr = var_by_category(to_var_category(l), ip_read_int(&s.ip));
      s_push(*var_ptr);
      break;
    }
    case CMD_ST: { // ST %d
      void **var_ptr = var_by_category(to_var_category(l), ip_read_int(&s.ip));
      *var_ptr = *s_peek();
      break;
    }

    case CMD_CTRL:
      switch (l) {
      case CMD_CTRL_CJMPz: { // CJMPz 0x%.8x
        uint jmp_p = ip_read_int(&s.ip);

#ifndef WITH_CHECK
        if (jmp_p >= s.bf->code_size) {
          s_failure(&s, "jump out of file");
        }
#endif
        if (UNBOX(s_pop_i()) == 0) {
          s.ip = s.bf->code_ptr + jmp_p;
        }
        break;
      }

      case CMD_CTRL_CJMPnz: { // CJMPnz  0x%.8x
        uint jmp_p = ip_read_int(&s.ip);

#ifndef WITH_CHECK
        if (jmp_p >= s.bf->code_size) {
          s_failure(&s, "jump out of file");
        }
#endif
        if (UNBOX(s_pop_i()) != 0) {
          s.ip = s.bf->code_ptr + jmp_p;
        }
        break;
      }

      case CMD_CTRL_BEGIN: { // BEGIN %d %d // function begin
        uint args_sz = ip_read_int(&s.ip);
// #ifdef WITH_CHECK
        uint locals_sz = ip_read_half_int(&s.ip);
        uint max_additional_stack_sz = ip_read_half_int(&s.ip);
// #else
//         uint locals_sz = ip_read_int(&s.ip);
// #endif
#ifdef WITH_CHECK
        if (s.fp != NULL && s.call_ip == NULL) {
          s_failure(&s, "begin should only be called after call");
        }
#endif
        s_enter_f(s.call_ip /*ip from call*/,
                  s.is_closure_call, args_sz, locals_sz);
#ifndef WITH_CHECK
        if ((void **)__gc_stack_top + (aint)max_additional_stack_sz - 1 <= s.stack) {
          s_failure(&s, "stack owerflow");
        }
#endif
        break;
      }

      case CMD_CTRL_CBEGIN: { // CBEGIN %d %d
        // NOTE: example not found, no checks done
        uint args_sz = ip_read_int(&s.ip);
// #ifdef WITH_CHECK
        uint locals_sz = ip_read_half_int(&s.ip);
        uint max_additional_stack_sz = ip_read_half_int(&s.ip);
// #else
        // uint locals_sz = ip_read_int(&s.ip);
// #endif
#ifndef WITH_CHECK
        if (s.fp != NULL && s.call_ip == NULL) {
          s_failure(&s, "begin should only be called after call");
        }
#endif
        s_enter_f(s.call_ip /*ip from call*/,
                  s.is_closure_call, args_sz, locals_sz);
#ifdef WITH_CHECK
        if ((void **)__gc_stack_top + (aint)max_additional_stack_sz - 1 <= s.stack) {
          s_failure(&s, "stack overflow");
        }
#endif
        break;
      }

      case CMD_CTRL_CLOSURE: // CLOSURE 0x%.8x
      {
        aint call_offset = ip_read_int(&s.ip);
        aint args_count = ip_read_int(&s.ip);
        for (aint i = 0; i < args_count; i++) {
          uint8_t arg_type = ip_read_byte(&s.ip);
          auint arg_id = ip_read_int(&s.ip);

          void **var_ptr =
              var_by_category(to_var_category(arg_type), arg_id);
          s_push(*var_ptr);
        }
#ifndef WITH_CHECK
        if (call_offset >= s.bf->code_size) {
          s_failure(&s, "jump out of file");
        }
#endif
        s_push(s.bf->code_ptr + call_offset);

        void *closure = Bclosure((aint *)__gc_stack_top, BOX(args_count));
        // printf("args is %li, count is %li\n", args_count, get_len(TO_DATA(closure)));

        s_popn(args_count + 1);
        s_push(closure);
        break;
      }

      case CMD_CTRL_CALLC: {                  // CALLC %d // call clojure
        aint args_count = ip_read_int(&s.ip); // args count

        call_happened = true;
        s.is_closure_call = true;
        s.call_ip = s.ip;

        s.ip = (char*)Belem(*s_nth(args_count), BOX(0)); // use offset instead ??
        break;
      }

      case CMD_CTRL_CALL: { // CALL 0x%.8x %d // call function
        uint call_p = ip_read_int(&s.ip);
        ip_read_int(&s.ip); // args count

        call_happened = true;
        s.is_closure_call = false;
        s.call_ip = s.ip;

#ifndef WITH_CHECK
        if (call_p >= s.bf->code_size) {
          s_failure(&s, "jump out of file");
        }
#endif
        s.ip = s.bf->code_ptr + call_p;
        break;
      }

      case CMD_CTRL_TAG: { // TAG %s %d
        const char *name = ip_read_string(&s.ip);
        aint args_count = ip_read_int(&s.ip);

#ifdef DEBUG_VERSION
        printf("tag hash is %i, n is %i, peek is %i, unboxed: %li\n",
               UNBOX(LtagHash((char *)name)), args_count, s_peek(&s), UNBOXED(s_peek(&s)));
#endif

        s_push_i(Btag(s_pop(), LtagHash((char *)name), BOX(args_count)));
        break;
      }

      case CMD_CTRL_ARRAY: // ARRAY %d
        s_push_i(Barray_patt(s_pop(), BOX(ip_read_int(&s.ip))));
        break;

      case CMD_CTRL_FAIL: { // FAIL %d %d
        int line = ip_read_int(&s.ip);
        int col = ip_read_int(&s.ip);
        Bmatch_failure(s_pop(), argv[0], BOX(line), BOX(col));
        break;
      }

      case CMD_CTRL_LINE: // LINE %d
        s.current_line = ip_read_int(&s.ip);
        // maybe some metainfo should be collected
        break;

      case CMD_CTRL_BUILTIN: { // BUILTIN %d %d // call builtin
        size_t builtin_id = ip_read_int(&s.ip);
        size_t args_count = ip_read_int(&s.ip); // args count

        printf("builtin id: %zu\n", builtin_id);
// #ifndef WITH_CHECK
        if (builtin_id >= BUILTIN_NONE) {
          s_failure(&s, "invalid builtin");
        }
// #endif

        if (builtin_id == BUILTIN_Barray) {
          call_Barray(args_count, &s.ip, buffer);
        } else {
          run_stdlib_func(builtin_id, args_count);
        }
        printf("builtin end\n");
        fflush(stdout);
        break;
      }

      default:
        s_failure(&s, "interpreter: ctrl, invalid opcode"); // %d-%d\n", h, l);
      }
      break;

    case CMD_PATT: // PATT pats[l]
      // {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"}
      switch (l) {
      case CMD_PATT_STR: // =str
        s_push_i(Bstring_patt(s_pop(), s_pop()));
        break;
      case CMD_PATT_STR_TAG: // #string
        s_push_i(Bstring_tag_patt(s_pop()));
        break;
      case CMD_PATT_ARRAY_TAG: // #array
        s_push_i(Barray_tag_patt(s_pop()));
        break;
      case CMD_PATT_SEXP_TAG: // #sexp
        s_push_i(Bsexp_tag_patt(s_pop()));
        break;
      case CMD_PATT_REF_TAG: // #ref
        s_push_i(Bunboxed_patt(s_pop()));
        break;
      case CMD_PATT_VAL_TAG: // #val
        s_push_i(Bboxed_patt(s_pop()));
        break;
      case CMD_PATT_FUN_TAG: // #fun
        s_push_i(Bclosure_tag_patt(s_pop()));
        break;
      default:
        s_failure(&s, "invalid opcode"); // %d-%d\n", h, l);
      }
      break;

    // NOTE: no longer used
    // case CMD_BUILTIN: {
    //   switch (l) {
    //   case CMD_BUILTIN_Lread: // CALL Lread
    //     s_push_i(Lread());
    //     break;

    //   case CMD_BUILTIN_Lwrite: // CALL Lwrite
    //     Lwrite(*s_peek_i());
    //     break;

    //   case CMD_BUILTIN_Llength: // CALL Llength
    //     s_push_i(Llength(s_pop()));
    //     break;

    //   case CMD_BUILTIN_Lstring: { // CALL Lstring
    //     void *val = s_pop();
    //     void *str = Lstring((aint *)&val);
    //     s_push(str);
    //     break;
    //   }

    //   case CMD_BUILTIN_Barray: { // CALL Barray %d
    //     size_t elem_count = ip_read_int(&s.ip);

    //     void **opr_buffer = (void**)(elem_count > BUFFER_SIZE
    //                             ? alloc(elem_count * sizeof(void *))
    //                             : buffer);
    //     for (size_t i = 0; i < elem_count; ++i) {
    //       opr_buffer[elem_count - i - 1] = s_pop();
    //     }

    //     // s_rotate_n(elem_count);
    //     void *array =
    //         Barray((aint *)opr_buffer,
    //                BOX(elem_count)); // NOTE: not sure if elems should be
    //                                  // added

    //     // void *array = Barray((aint *)s_peek(), BOX(elem_count));
    //     s_push(array);
    //     break;
    //   }

    //   default:
    //     s_failure(&s, "invalid opcode"); // %d-%d\n", h, l);
    //   }
    // } break;

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
  printf("--- module run end ---\n");
#endif
}

void run_cleanup() { cleanup_state(&s); }
