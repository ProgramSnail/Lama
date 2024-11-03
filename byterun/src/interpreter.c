#include "interpreter.h"

#include "../../runtime/runtime.h"
#include "../../runtime/gc.h"

#include "utils.h"
#include "types.h"
#include "stack.h"
#include "runtime_externs.h"

extern size_t STACK_SIZE;

int ip_read_int(char** ip) {
  *ip += sizeof(int);
  return *(int*)((*ip) - sizeof(int));
}

char ip_read_byte(char** ip) {
  return *(*ip)++;
}

char* ip_read_string(char** ip, bytefile* bf) {
  return get_string(bf, ip_read_int(ip));
}

// TODO: store globals in some way ?? // maybe some first vars ??

void run(bytefile *bf) {
  struct State s = init_state(bf);

  const size_t OPS_SIZE = 13;
  const char *ops [] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  aint(*ops_func[])(void*, void*) = {
    &Ls__Infix_43, // +
    &Ls__Infix_45, // -
    &Ls__Infix_42, // *
    &Ls__Infix_47, // /
    &Ls__Infix_37, // %
    &Ls__Infix_60, // <
    &Ls__Infix_6061, // <=
    &Ls__Infix_62, // >
    &Ls__Infix_6261, // >=
    &Ls__Infix_6161, // ==
    &Ls__Infix_3361, // !=
    &Ls__Infix_3838, // &&
    &Ls__Infix_3333, // !!
  };

  const size_t PATS_SIZE = 7;
  const char *pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
  do {
    char* before_op_ip = s.ip; // save to set s.prev_ip
 
    char x = ip_read_byte(&s.ip),
         h = (x & 0xF0) >> 4,
         l = x & 0x0F;

    // fprintf (f, "0x%.8x:\t", ip-bf->code_ptr-1);

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
      void* left = s_pop(&s);
      void* right = s_pop(&s);
      s_push(&s, (void*)ops_func[l-1](left, right));
      break;
      
    case 1:
      switch (l) {
      case  0: // CONST %d
        s_push_i(&s, BOX(ip_read_int(&s.ip)));
        break;
        
      case  1: { // STRING %s
        void* str = ip_read_string(&s.ip, bf);
        s_push(&s, Bstring((aint*)&str));
        break;
      }
          
      case  2: // SEXP %s %d // create sexpr with tag=%s and %d elements from stack
        // params read from stack
        s_push_i(&s, LtagHash(ip_read_string(&s.ip, bf)));
        Bsexp((aint*)s.sp, ip_read_int(&s.ip)); // TODO: check order
        break;
        
      case  3: // STI - write by ref (?)
        // TODO
        break;
        
      case  4: // STA - write to array elem
        // Bsta // TODO
        break;
        
      case  5: // JMP 0x%.8x
        s.ip = (char*)(long)ip_read_int(&s.ip); // TODO: check
        break;
        
      case  6: // END
        s_exit_f(&s); // TODO: always ??, check that it is enough
        break;
        
      case  7: // RET
        // TODO
        break;
        
      case  8: // DROP
        s_pop(&s);
        break;
        
      case  9: // DUP
        {
          if (s.sp == s.stack + STACK_SIZE || (s.fp != NULL && s.sp == f_locals(s.fp))) {
            failure("can't DUP: no value on stack");
          }
          *s.sp = *(s.sp - 1);
          ++s.sp;
          break;
        }

      case 10: // SWAP
        { // guess
          if (s.sp + 1 >= s.stack + STACK_SIZE || (s.fp != NULL && s.sp + 1 >= f_locals(s.fp))) {
            failure("can't SWAP: < 2 values on stack");
          }
          void* v = *s.sp;
          push_extra_root(v);
          *s.sp = *(s.sp + 1);
          *(s.sp + 1) = v;
          pop_extra_root(v);
        }
        break;

      case 11: // ELEM
        {
          void* array = s_pop(&s);
          aint index = s_pop_i(&s);
          s_push(&s, Belem(array, index));
        }
        break;
        
      default:
        failure("invalid opcode %d-%d\n", h, l);
      }
      break;
      
    case 2: { // LD %d
      int8_t category = ip_read_byte(&s.ip);
      void** var_ptr = var_by_category(&s, to_var_category(category), ip_read_int(&s.ip));
      s_push(&s, *var_ptr);
      break;
    }
    case 3: { // LDA %d
      int8_t category = ip_read_byte(&s.ip);
      void** var_ptr = var_by_category(&s, to_var_category(category), ip_read_int(&s.ip));
      // TODO
      break;
    }
    case 4: { // ST %d
      int8_t category = ip_read_byte(&s.ip);
      void** var_ptr = var_by_category(&s, to_var_category(category), ip_read_int(&s.ip));
      *var_ptr = s_pop(&s);
      break;
    }
    case 5:
      switch (l) {
      case  0: { // CJMPz 0x%.8x
        char* new_ip = (char*)(long)ip_read_int(&s.ip); // TODO: check
        if (s_pop_i(&s) != UNBOX(0)) {
          s.ip = new_ip;
        }
        break;
      }
      case 1: { // CJMPnz  0x%.8x
        char* new_ip = (char*)(long)ip_read_int(&s.ip); // TODO: check
        if (s_pop_i(&s) == UNBOX(0)) {
          s.ip = new_ip;
        }
        break;
      }
      case  2: // BEGIN %d %d // function begin
        s_enter_f(&s, s.prev_ip/*ip from call*/, ip_read_int(&s.ip), ip_read_int(&s.ip));
        break;
        
      case  3: // CBEGIN %d %d // TODO: clojure begin ??
        s_enter_f(&s, s.prev_ip/*ip from call*/, ip_read_int(&s.ip), ip_read_int(&s.ip));
        break;
        
      case  4: // CLOSURE 0x%.8x
        // TODO
        {
          int n = ip_read_int(&s.ip);
          for (int i = 0; i < n; i++) {
            switch (ip_read_byte(&s.ip)) {
            // case 0: // G(%d)
            // case 1: // L(%d)
            // case 2: // A(%d)
            // case 3: // C(%d)
            default:
              failure("invalid opcode %d-%d\n", h, l);
            }
          }
        };
        break;
          
      case  5: // CALLC %d // call clojure
        // TODO FIXME: call clojure
        // s.ip = (char*)(long)ip_read_int(&s.ip); // TODO: check
        break;
        
      case  6: // CALL 0x%.8x %d // call function
        // FIXME: second arg ??
        s.ip = (char*)(long)ip_read_int(&s.ip); // TODO: check
        break;
        
      case  7: // TAG %s
        s_push_i(&s, LtagHash(ip_read_string(&s.ip, bf))); // TODO: check
        break;
        
      case  8: // ARRAY %d
        Barray((aint*)s.sp, ip_read_int(&s.ip));
        break;
        
      case  9: // FAIL %d %d // TODO
        failure("[FAIL]: %d-%d\n", ip_read_int(&s.ip), ip_read_int(&s.ip));
        break;
        
      case 10: // LINE %d
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
        s_push_i(&s, Bstring_patt(s_pop(&s), s_pop(&s))); // TODO: order
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
        // TODO
        break;
      case 5: // #val
        // TODO
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
        Lwrite(s_pop_i(&s));
        break;

      case 2: // CALL Llength
        s_push_i(&s, Llength(s_pop(&s)));
        break;

      case 3: { // CALL Lstring
        void* str = Lstring((aint*)s.sp);
        s_pop(&s);
        s_push(&s, str);
        break;
      }

      case 4: { // CALL Barray %d
        size_t n = ip_read_int(&s.ip);
        void* array = Barray((aint*)s.sp, n); // TODO: are elems added (?)
        s_popn(&s, n);
        s_push(&s, array);
        break;
      }

      default:
        failure("invalid opcode %d-%d\n", h, l);
      }
    }
    break;
      
    default:
      failure("invalid opcode %d-%d\n", h, l);
    }

    s.prev_ip = before_op_ip;
  }
  while (1);
stop:;
  cleanup_state(&s);
}
