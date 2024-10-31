#include "interpreter.h"

#include "../../runtime/runtime.h"
#include "../../runtime/gc.h"

#include "utils.h"
#include "types.h"
#include "stack.h"
#include "runtime_externs.h"

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
  int(*ops_func[])(void*, void*) = {
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
      f_binop(&s, ops[l-1]);
      break;
      
    case 1:
      switch (l) {
      case  0: // CONST %d
        s_put_i(&s, ip_read_int(&s.ip));
        break;
        
      case  1: // STRING %s
        s_put_const_str(&s, ip_read_string(&s.ip, bf));
        break;
          
      case  2: // SEXP %s %d // create sexpr with tag=%s and %d elements from stack
        // params read from stack
        s_put_sexp(&s, ip_read_string(&s.ip, bf), ip_read_int(&s.ip));
        break;
        
      case  3: // STI
        // TODO
        break;
        
      case  4: // STA
        // TODO
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
        s_drop_var(&s);
        break;
        
      case  9: // DUP
        { // guess
          if (s.vp == s.stack || (s.fp != NULL && s.vp == s.fp->end)) {
            failure("can't DUP: no value on stack");
          }
          *s.vp = *(s.vp - 1);
          ++s.vp;
          break;
        }

      case 10: // SWAP
        { // guess
          struct NilT* v = *s.vp;
          *s.vp = *(s.vp - 1);
          *(s.vp - 1) = v;
        }
        break;

      case 11: // ELEM
        {
          union VarT* array = s_take_var(&s);
          union VarT* index = s_take_var(&s);
          if (dh_type(array->nil.data_header) != ARRAY_T) {
            failure("ELEM: elem, previous element is not array");
          }
          if (dh_type(index->nil.data_header) != INT_T) {
            failure("ELEM: elem, last element is not int");
          }
          if (index->int_t.value < 0) {
            failure("ELEM: can't access by index < 0");
          }
          if (index->int_t.value >= dh_param(array->array.data_header)) {
            failure("ELEM: array index is out of range");
          }
          s_put_var(&s, array->array.values[index->int_t.value]);

          free_var_ptr(array);
          free_var_ptr(index);
        }
        break;
        
      default:
        failure("invalid opcode %d-%d\n", h, l);
      }
      break;
      
    case 2: { // LD %d
      int8_t category = ip_read_byte(&s.ip);
      union VarT** var_ptr = var_by_category(&s, to_var_category(category), ip_read_int(&s.ip));
      s_put_var(&s, (struct NilT*)*var_ptr); // TODO: check
      break;
    }
    case 3: { // LDA %d
      int8_t category = ip_read_byte(&s.ip);
      union VarT** var_ptr = var_by_category(&s, to_var_category(category), ip_read_int(&s.ip));
      // TODO
      break;
    }
    case 4: { // ST %d
      int8_t category = ip_read_byte(&s.ip);
      union VarT** var_ptr = var_by_category(&s, to_var_category(category), ip_read_int(&s.ip));
      *var_ptr = s_take_var(&s); // TODO: check
      break;
    }
    case 5:
      switch (l) {
      case  0: { // CJMPz 0x%.8x
        char* new_ip = (char*)(long)ip_read_int(&s.ip); // TODO: check
        if (s_take_i(&s) != 0) {
          s.ip = new_ip;
        }
        break;
      }
      case 1: { // CJMPnz  0x%.8x
        char* new_ip = (char*)(long)ip_read_int(&s.ip); // TODO: check
        if (s_take_i(&s) == 0) {
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
        // TODO: ??
        break;
        
      case  8: // ARRAY %d
        s_put_array(&s, ip_read_int(&s.ip));
        break;
        
      case  9: // FAIL %d %d
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
      // TODO: JMP if same to pattern ??
      break;

    case 7: {
      switch (l) {
      case 0: // CALL Lread
        f_read(&s);
        break;
        
      case 1: // CALL Lwrite
        f_write(&s);
        break;

      case 2: // CALL Llength
        f_length(&s);
        break;

      case 3: // CALL Lstring
        f_string(&s);
        break;

      case 4: // CALL Barray %d
        f_array(&s, ip_read_int(&s.ip));
        break;

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
