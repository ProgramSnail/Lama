#include "interpreter.h"

#include "../../runtime/gc.h"
#include "../../runtime/runtime.h"

#include "runtime_externs.h"
#include "stack.h"
#include "types.h"
#include "utils.h"

int ip_read_int(char **ip) {
  *ip += sizeof(int);
  return *(int *)((*ip) - sizeof(int));
}

char ip_read_byte(char **ip) { return *(*ip)++; }

char *ip_read_string(char **ip, bytefile *bf) {
  return get_string(bf, ip_read_int(ip));
}

void run(bytefile *bf) {
  struct State s;
  init_state(bf, &s);
  print_stack(&s);

  printf("--- interpreter run ---\n");

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

  // TODO: actual argc, argv
  s_push_i(&s, BOX(1)); // argc
  const char *argv_0 = "interpreter";
  s_push(&s, Bstring((aint *)&argv_0)); // argv

  printf("- loop start\n");

  do {
    // char *before_op_ip = s.ip; // save to set s.prev_ip
    bool call_happened = false;

    char x = ip_read_byte(&s.ip), h = (x & 0xF0) >> 4, l = x & 0x0F;

    printf("0x%.8x\n", s.ip - bf->code_ptr - 1);

    switch (h) {
    case 15:
      goto stop;

    /* BINOP */
    case 0: // BINOP ops[l-1]
      if (l > OPS_SIZE) {
        failure("BINOP: l > OPS_SIZE");
      }
      if (l < 1) {
        failure("BINOP: l < 1");
      }
      void *left = s_pop(&s);
      void *right = s_pop(&s);
      s_push(&s, (void *)ops_func[l - 1](right, left));
      break;

    case 1:
      switch (l) {
      case 0: // CONST %d
        s_push_i(&s, BOX(ip_read_int(&s.ip)));
        break;

      case 1: { // STRING %s
        void *str = ip_read_string(&s.ip, bf);
        s_push(&s, Bstring((aint *)&str));
        break;
      }

      case 2: { // SEXP %s %d // create sexpr with tag=%s and %d elements from
                // stack
        // params read from stack
        const char* name = ip_read_string(&s.ip, bf);
        aint args_count = ip_read_int(&s.ip);
        printf("tag hash is %i, n os %i\n", UNBOX(LtagHash((char*)name)), args_count);

        if (args_count < 0) {
          failure("SEXP: args count should be >= 0");
        }
        void ** buffer = calloc(args_count + 1, sizeof(void*));

        for (size_t i = 0; i < args_count; ++i) {
          buffer[i] = s_pop(&s);
        }
        buffer[args_count] = (void*)LtagHash((char*)name);

        // TODO: FIXME: not working with elems
        void* sexp = Bsexp((aint *)buffer, BOX(args_count + 1));

        push_extra_root(sexp);
        s_push(&s, sexp);
        pop_extra_root(sexp);

        free(buffer);
        break;
      }

      case 3: // STI - write by ref (?)
        // TODO
        break;

      case 4: { // STA - write to array elem
        void* elem = s_pop(&s);
        aint index = s_pop_i(&s);
        void* data = s_pop(&s);
        s_push(&s, Bsta(data, index, elem));
        break;
      }

      case 5: {                         // JMP 0x%.8x
        int jmp_p = ip_read_int(&s.ip); // TODO: check

        if (jmp_p < 0) {
          failure("negative file offset jumps are not allowed");
        }
        s.ip = bf->code_ptr + jmp_p;
        break;
      }

      case 6: // END
        if (!s_is_empty(&s) && s.fp->prev_fp != 0) {
          s.fp->ret = *s_peek(&s);
          s_pop(&s);
        }
        s_exit_f(&s);
        break;

      case 7: // RET
        if (!s_is_empty(&s) && s.fp->prev_fp != 0) {
          s.fp->ret = *s_peek(&s);
          s_pop(&s);
        }
        s_exit_f(&s);
        break;

      case 8: // DROP
        s_pop(&s);
        break;

      case 9: // DUP
      {
        s_push(&s, *s.sp);
        break;
      }

      case 10: // SWAP
      {
        if (s.sp + 1 >= s.stack + STACK_SIZE ||
            (s.fp != NULL && s.sp + 1 >= f_locals(s.fp))) {
          failure("can't SWAP: < 2 values on stack");
        }
        void *v = *s.sp;
        push_extra_root(v);
        *s.sp = *(s.sp + 1);
        *(s.sp + 1) = v;
        pop_extra_root(v);
      } break;

      case 11: // ELEM
      {
        aint index = s_pop_i(&s);
        void *data = s_pop(&s);
        s_push(&s, Belem(data, index));
      } break;

      default:
        failure("invalid opcode %d-%d\n", h, l);
      }
      break;

    case 2: { // LD %d
      void **var_ptr =
          var_by_category(&s, to_var_category(l), ip_read_int(&s.ip));
      s_push(&s, *var_ptr);
      break;
    }

    case 3: { // LDA %d
      void **var_ptr =
          var_by_category(&s, to_var_category(l), ip_read_int(&s.ip));
      s_push(&s, var_ptr);
      break;
    }
    case 4: { // ST %d
      void **var_ptr =
          var_by_category(&s, to_var_category(l), ip_read_int(&s.ip));
      *var_ptr = *s_peek(&s);
      break;
    }

    case 5:
      switch (l) {
      case 0: { // CJMPz 0x%.8x
        int jmp_p = ip_read_int(&s.ip);

        if (jmp_p < 0) {
          failure("negative file offset jumps are not allowed");
        }
        if (UNBOX(s_pop_i(&s)) == 0) {
          s.ip = bf->code_ptr + jmp_p;
        }
        break;
      }

      case 1: { // CJMPnz  0x%.8x
        int jmp_p = ip_read_int(&s.ip);

        if (jmp_p < 0) {
          failure("negative file offset jumps are not allowed");
        }
        if (UNBOX(s_pop_i(&s)) != 0) {
          s.ip = bf->code_ptr + jmp_p;
        }
        break;
      }

      case 2: { // BEGIN %d %d // function begin
        int args_sz = ip_read_int(&s.ip);
        int locals_sz = ip_read_int(&s.ip);
        if (s.fp != NULL && s.call_ip == NULL) {
          failure("BEGIN: not after call");
        }
        s_enter_f(&s, s.call_ip /*ip from call*/, s.is_closure_call, args_sz, locals_sz);
        break;
      }

      case 3: { // CBEGIN %d %d // TODO: used ??
        int args_sz = ip_read_int(&s.ip);
        int locals_sz = ip_read_int(&s.ip);
        if (s.fp != NULL && s.call_ip == NULL) {
          failure("BEGIN: not after call");
        }
        s_enter_f(&s, s.call_ip /*ip from call*/, s.is_closure_call, args_sz, locals_sz);
        break;
      }

      case 4: // CLOSURE 0x%.8x // TODO: check
        {
          aint call_offset = ip_read_int(&s.ip);
          aint args_count = ip_read_int(&s.ip);
          for (aint i = 0; i < args_count; i++) {
            aint arg_type = ip_read_byte(&s.ip);
            aint arg_id = ip_read_int(&s.ip);

            void **var_ptr =
                var_by_category(&s, to_var_category(l), ip_read_int(&s.ip));
            s_push(&s, *var_ptr);
          }
          s_push(&s, bf->code_ptr + call_offset); // TODO: check way of storage ??

          void* closure = Bclosure((aint*)s.sp, args_count);

          push_extra_root(closure);
          s_popn(&s, args_count + 1);
          s_push(&s, closure);
          pop_extra_root(closure);
        break;
      }

      case 5: // CALLC %d // call clojure
        ip_read_int(&s.ip); // args count

        call_happened = true;
        s.is_closure_call = true;
        s.call_ip = s.ip;

        s.ip = Belem(s.sp, BOX(0)); // use offset instead ??
        break;

      case 6: {                          // CALL 0x%.8x %d // call function
        int call_p = ip_read_int(&s.ip);
        ip_read_int(&s.ip); // args count

        call_happened = true;
        s.is_closure_call = false;
        s.call_ip = s.ip;

        if (call_p < 0) {
          failure("negative file offset jumps are not allowed");
        }
        s.ip = bf->code_ptr + call_p;
        break;
      }

      case 7: { // TAG %s %d
        const char* name = ip_read_string(&s.ip, bf);
        aint args_count = ip_read_int(&s.ip);

        printf("tag hash is %i, n is %i, peek is %i\n", UNBOX(LtagHash((char*)name)), args_count, s_peek(&s));
        if (UNBOXED(s_peek(&s))) printf("aaaaaaaaaaaaaaaaaaa");
        else {
          data* r = TO_DATA(s_peek(&s));
          if ((aint)BOX(TAG(r->data_header) != SEXP_TAG)) printf("bbbbbbbb %i %i", TAG(r->data_header), SEXP_TAG);
          if (TO_SEXP(s_peek(&s))->tag != UNBOX(LtagHash((char*)name))) printf("cccccccccc");
          if (LEN(r->data_header) != UNBOX(args_count)) printf("dddddd %i %i", LEN(r->data_header), UNBOX(args_count));
        }

        s_push_i(&s, Btag(s_pop(&s), LtagHash((char*)name), args_count));
        break;
      }

      case 8: // ARRAY %d
        Barray((aint *)s.sp, ip_read_int(&s.ip));
        break;

      case 9: // FAIL %d %d // TODO
        failure("[FAIL]: %d-%d\n", ip_read_int(&s.ip), ip_read_int(&s.ip));
        break;

      case 10: // LINE %d
        ip_read_int(&s.ip);
        // maybe some metainfo should be collected
        break;

      default:
        failure("invalid opcode %d-%d\n", h, l);
      }
      break;

    case 6: // PATT pats[l]
      // {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"}
      switch (l) {
      case 0: // =str
        s_push_i(&s, Bstring_patt(s_pop(&s), s_pop(&s)));
        break;
      case 1: // #string
        s_push_i(&s, Bstring_tag_patt(s_pop(&s)));
        break;
      case 2: // #array
        s_push_i(&s, Barray_tag_patt(s_pop(&s)));
        break;
      case 3: // #sexp
        s_push_i(&s, Bsexp_tag_patt(s_pop(&s)));
        break;
      case 4: // #ref
        s_push_i(&s, Bunboxed_patt(s_pop(&s))); // TODO: check
        // TODO
        break;
      case 5: // #val
        s_push_i(&s, Bboxed_patt(s_pop(&s))); // TODO: check
        break;
      case 6: // #fun
        s_push_i(&s, Bclosure_tag_patt(s_pop(&s)));
        break;
      default:
        failure("invalid opcode %d-%d\n", h, l);
      }
      break;

    case 7: {
      switch (l) {
      case 0: // CALL Lread
        s_push_i(&s, Lread());
        break;

      case 1: // CALL Lwrite
        Lwrite(*s_peek_i(&s));
        break;

      case 2: // CALL Llength
        s_push_i(&s, Llength(s_pop(&s)));
        break;

      case 3: { // CALL Lstring
        void *val = s_pop(&s);
        void *str =
            Lstring((aint *)&val); // FIXME: not working, GC extra root error
        s_push(&s, str);
        break;
      }

      case 4: { // CALL Barray %d
        size_t n = ip_read_int(&s.ip);
        void *array = Barray((aint *)s.sp, n); // TODO: are elems added (?)
        s_popn(&s, n);
        s_push(&s, array);
        break;
      }

      default:
        failure("invalid opcode %d-%d\n", h, l);
      }
    } break;

    default:
      failure("invalid opcode %d-%d\n", h, l);
    }

    if (!call_happened) {
      s.is_closure_call = false;
      s.call_ip = NULL;
    }

    if (s.fp == NULL) {
      break;
    }
    print_stack(&s);
  } while (1);
stop:;
  printf("--- run end ---\n");
  cleanup_state(&s);
}
