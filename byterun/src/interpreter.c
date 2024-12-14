#include "interpreter.h"

#include "../../runtime/gc.h"
#include "../../runtime/runtime.h"

#include "runtime_externs.h"
#include "stack.h"
#include "types.h"
#include "utils.h"

#define ASSERT_UNBOXED(memo, x)                                                \
  do                                                                           \
    if (!UNBOXED(x))                                                           \
      failure("unboxed value expected in %s\n", memo);                         \
  while (0)

struct State s;

static inline uint16_t ip_read_half_int(char **ip) {
  if (*ip + sizeof(uint16_t) > s.bf->code_ptr + s.bf->code_size) {
    failure("last command is invalid, int parameter can not be read\n");
  }
  *ip += sizeof(uint16_t);
  return *(uint16_t *)((*ip) - sizeof(uint16_t));
}

static inline int ip_read_int(char **ip) {
  if (*ip + sizeof(int) > s.bf->code_ptr + s.bf->code_size) {
    failure("last command is invalid, int parameter can not be read\n");
  }
  *ip += sizeof(int);
  return *(int *)((*ip) - sizeof(int));
}

static inline uint8_t ip_read_byte(char **ip) {
  if (*ip + sizeof(char) > s.bf->code_ptr + s.bf->code_size) {
    failure("last command is invalid, byte parameter can not be read\n");
  }
  return *(*ip)++;
}

static inline const char *ip_read_string(char **ip) {
  return get_string(s.bf, ip_read_int(ip));
}

const size_t BUFFER_SIZE = 1000;

void run(Bytefile *bf, int argc, char **argv) {
  size_t stack[STACK_SIZE];
  void *buffer[BUFFER_SIZE];
  construct_state(bf, &s, (void **)stack);

#ifdef DEBUG_VERSION
  printf("--- interpreter run ---\n");
#endif

  // argc, argv
  {
    s_push_i(BOX(argc));
    for (size_t i = 0; i < argc; ++i) {
      s_push(Bstring((aint *)&argv[argc - i - 1]));
    }
    s_push(Barray((aint *)s_peek(), argc));
    void *argv_elem = s_pop();
    s_popn(argc);
    s_push(argv_elem);
  }

#ifdef DEBUG_VERSION
  printf("- loop start\n");
#endif

  do {
    bool call_happened = false;

    if (s.ip >= bf->code_ptr + bf->code_size) {
      s_failure(&s, "instruction pointer is out of range (>= size)");
    }

    if (s.ip < bf->code_ptr) {
      s_failure(&s, "instruction pointer is out of range (< 0)");
    }

    s.instr_ip = s.ip;
    uint8_t x = ip_read_byte(&s.ip), h = (x & 0xF0) >> 4, l = x & 0x0F;

#ifdef DEBUG_VERSION
    printf("0x%.8x\n", s.ip - bf->code_ptr - 1);
#endif

    switch (h) {
    case CMD_EXIT:
      goto stop;

    /* BINOP */
    case CMD_BINOP: { // BINOP ops[l-1]
      void *snd = s_pop();
      void *fst = s_pop();
      if (l == CMD_BINOP_SUB) {
        s_push_i(Ls__Infix_45(fst, snd));
      } else {
        switch (l) {
#define BINOP_OPR(val, op)                                                     \
  case val:                                                                    \
    ASSERT_UNBOXED("captured op:1", fst);                                      \
    ASSERT_UNBOXED("captured op:2", snd);                                      \
    s_push_i(BOX(UNBOX(fst) op UNBOX(snd)));                                   \
    break;
          FORALL_BINOP(BINOP_OPR)
#undef BINOP_OPR

        default:
          s_failure(&s, "invalid opcode"); // %d-%d\n", h, l);
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

        void **opr_buffer = (void**)(args_count >= BUFFER_SIZE
                                ? alloc((args_count + 1) * sizeof(void *))
                                : buffer);

        // s_put_nth(args_count, (void *)LtagHash((char *)name));

        for (size_t i = 1; i <= args_count; ++i) {
          opr_buffer[args_count - i] = s_pop();
        }
        opr_buffer[args_count] = (void *)LtagHash((char *)name);

        void *sexp = Bsexp((aint *)opr_buffer, BOX(args_count + 1));
        // void *sexp = Bsexp((aint *)s_peek(), BOX(args_count + 1));
        // s_popn(args_count + 1);

        s_push(sexp);
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

        if (jmp_p >= bf->code_size) {
          s_failure(&s, "jump out of file");
        }
        s.ip = bf->code_ptr + jmp_p;
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
        s_failure(&s, "invalid opcode"); // %d-%d\n", h, l);
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

        if (jmp_p >= bf->code_size) {
          s_failure(&s, "jump out of file");
        }
        if (UNBOX(s_pop_i()) == 0) {
          s.ip = bf->code_ptr + jmp_p;
        }
        break;
      }

      case CMD_CTRL_CJMPnz: { // CJMPnz  0x%.8x
        uint jmp_p = ip_read_int(&s.ip);

        if (jmp_p >= bf->code_size) {
          s_failure(&s, "jump out of file");
        }
        if (UNBOX(s_pop_i()) != 0) {
          s.ip = bf->code_ptr + jmp_p;
        }
        break;
      }

      case CMD_CTRL_BEGIN: { // BEGIN %d %d // function begin
        uint args_sz = ip_read_int(&s.ip);
        uint locals_sz = ip_read_half_int(&s.ip);
        uint max_additional_stack_sz = ip_read_half_int(&s.ip);
        if (s.fp != NULL && s.call_ip == NULL) {
          s_failure(&s, "begin should only be called after call");
        }
        s_enter_f(s.call_ip /*ip from call*/, s.is_closure_call, args_sz,
                  locals_sz);
        // TODO: add ifdef
        if ((void **)__gc_stack_top + (aint)max_additional_stack_sz - 1 <= s.stack) {
          s_failure(&s, "stack owerflow");
        }
        break;
      }

      case CMD_CTRL_CBEGIN: { // CBEGIN %d %d
        // NOTE: example not found, no checks done
        uint args_sz = ip_read_int(&s.ip);
        uint max_additional_stack_sz = ip_read_half_int(&s.ip);
        uint locals_sz = ip_read_half_int(&s.ip);
        if (s.fp != NULL && s.call_ip == NULL) {
          s_failure(&s, "begin should only be called after call");
        }
        s_enter_f(s.call_ip /*ip from call*/, s.is_closure_call, args_sz,
                  locals_sz);
        // TODO: add ifdef
        if ((void **)__gc_stack_top + (aint)max_additional_stack_sz - 1 <= s.stack) {
          s_failure(&s, "stack owerflow");
        }
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
        if (call_offset >= bf->code_size) {
          s_failure(&s, "jump out of file");
        }
        s_push(bf->code_ptr + call_offset);

        void *closure = Bclosure((aint *)__gc_stack_top, args_count);

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

        if (call_p >= bf->code_size) {
          s_failure(&s, "jump out of file");
        }
        s.ip = bf->code_ptr + call_p;
        break;
      }

      case CMD_CTRL_TAG: { // TAG %s %d
        const char *name = ip_read_string(&s.ip);
        aint args_count = ip_read_int(&s.ip);

#ifdef DEBUG_VERSION
        printf("tag hash is %i, n is %i, peek is %i\n",
               UNBOX(LtagHash((char *)name)), args_count, s_peek(&s));
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

      default:
        s_failure(&s, "invalid opcode"); // %d-%d\n", h, l);
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

    case CMD_BUILTIN: {
      switch (l) {
      case CMD_BUILTIN_Lread: // CALL Lread
        s_push_i(Lread());
        break;

      case CMD_BUILTIN_Lwrite: // CALL Lwrite
        Lwrite(*s_peek_i());
        break;

      case CMD_BUILTIN_Llength: // CALL Llength
        s_push_i(Llength(s_pop()));
        break;

      case CMD_BUILTIN_Lstring: { // CALL Lstring
        void *val = s_pop();
        void *str = Lstring((aint *)&val);
        s_push(str);
        break;
      }

      case CMD_BUILTIN_Barray: { // CALL Barray %d
        size_t elem_count = ip_read_int(&s.ip);

        void **opr_buffer = (void**)(elem_count > BUFFER_SIZE
                                ? alloc(elem_count * sizeof(void *))
                                : buffer);
        for (size_t i = 0; i < elem_count; ++i) {
          opr_buffer[elem_count - i - 1] = s_pop();
        }

        // s_rotate_n(elem_count);
        void *array =
            Barray((aint *)opr_buffer,
                   BOX(elem_count)); // NOTE: not shure if elems should be
                                     // added

        // void *array = Barray((aint *)s_peek(), BOX(elem_count));
        s_push(array);
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
