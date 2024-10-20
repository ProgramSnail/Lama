#include "../include/interpreter.h"
#include "../include/types.h"
#include "../../runtime/runtime.h"

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

  const char *ops [] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  const char *pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
  const char *lds [] = {"LD", "LDA", "ST"};
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
      // TODO: all binops
      break;
      
    case 1:
      switch (l) {
      case  0: // CONST %d
        s_put_i(&s, ip_read_int(&s.ip));
        break;
        
      case  1: // STRING %s
        s_put_const_str(&s, ip_read_string(&s.ip, bf));
        break;
          
      case  2: // SEXP %s %d
        // TODO: call sexp ??
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
          struct Var v = deep_copy_var(*s.vp);
          s_put_var(&s, v);
          break;
        }

      case 10: // SWAP
        { // guess
          struct Var v = *s.vp;
          *s.vp = *(s.vp - 1);
          *(s.vp - 1) = v;
        }
        break;

      case 11: // ELEM
        {
          struct Var array = s_take_var(&s);
          struct Var index = s_take_var(&s);
          if (array.type != ARRAY_T) {
            failure("ERROR: elem, previous element is not array", h, l);
          }
          if (index.type != INT_T) {
            failure("ERROR: elem, last element is not int", h, l);
          }
          s_put_var(&s, array.value.array_v[index.value.int_v]);

          free_var(array);
          free_var(index);
          // FIXME: deal vith deletion of shallow copies of locals
        }
        break;
        
      default:
        failure("ERROR: invalid opcode %d-%d\n", h, l);
      }
      break;
      
    case 2:
    case 3:
    case 4:
      // fprintf (f, "%s\t", lds[h-2]);
      switch (l) {
      // case 0: fprintf (f, "G(%d)", INT); break;
      // case 1: fprintf (f, "L(%d)", INT); break;
      // case 2: fprintf (f, "A(%d)", INT); break;
      // case 3: fprintf (f, "C(%d)", INT); break;
      default:
        failure("ERROR: invalid opcode %d-%d\n", h, l);
      }
      break;
      
    case 5:
      switch (l) {
      case  0: { // CJMPz 0x%.8x
        // FIXME: TODO: jump by top stack condition ??
        char* new_ip = (char*)(long)ip_read_int(&s.ip); // TODO: check
        if (s_take_i(&s) != 0) { // FIXME: bools ??, other vars ??
          s.ip = new_ip;
        }
        break;
      }
      case 1: { // CJMPnz  0x%.8x
        // FIXME: TODO: jump by top stack condition ??i
        char* new_ip = (char*)(long)ip_read_int(&s.ip); // TODO: check
        if (s_take_i(&s) == 0) { // FIXME: bools ??, other vars ??
          s.ip = new_ip;
        }
        break;
      }
      case  2: // BEGIN %d %d
        s_enter_f(&s, s.prev_ip/*ip from call*/, ip_read_int(&s.ip), ip_read_int(&s.ip));
        // TODO: is func enter ?
        break;
        
      case  3: // CBEGIN %d %d
        s_enter_f(&s, s.prev_ip/*ip from call*/, ip_read_int(&s.ip), ip_read_int(&s.ip));
        // TODO: is func enter ?
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
              failure("ERROR: invalid opcode %d-%d\n", h, l);
            }
          }
        };
        break;
          
      case  5: // CALLC %d
        // TODO: no arguments given ??
        // TODO: jump only ??
        s.ip = (char*)(long)ip_read_int(&s.ip); // TODO: check
        break;
        
      case  6: // CALL 0x%.8x %d
        // TODO: second arg is given params amount ??
        // TODO: jump only ??
        s.ip = (char*)(long)ip_read_int(&s.ip); // TODO: check
        break;
        
      case  7: // TAG %s
        // TODO: ??
        break;
        
      case  8: // ARRAY %d
        s_put_array(&s, ip_read_int(&s.ip));
        break;
        
      case  9: // FAIL %d %d
        // TODO: ??
        failure("FAIL: %d-%d\n", ip_read_int(&s.ip), ip_read_int(&s.ip));
        break;
        
      case 10: // LINE %d
        // maybe some metainfo should be collected
        break;

      default:
        failure("ERROR: invalid opcode %d-%d\n", h, l);
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
        failure("ERROR: invalid opcode %d-%d\n", h, l);
      }
    }
    break;
      
    default:
      failure("ERROR: invalid opcode %d-%d\n", h, l);
    }

    s.prev_ip = before_op_ip;
  }
  while (1);
stop:;
  free_state(&s);
}
