#include "../include/interpreter.h"
#include "../../runtime/runtime.h"

int read_int(char** ip) {
  *ip += sizeof(int);
  return *(int*)((*ip) - sizeof(int));
}

char read_byte(char** ip) {
  return *(*ip)++;
}

char* read_string(char** ip, bytefile* bf) {
  return get_string(bf, read_int(ip));
}

//

struct Var;

struct ListElem {
  struct Var* elem;
  struct Var* next;
};

struct Var {
  enum Type { INT_T, CONST_STR_T, STR_T, LIST_T, ARRAY_T, FUN_T, NIL_T/*?*/ } type;
  size_t size;
  union {
    int int_v; // int value => size = 1;
    const char* const_str_v; // point to str in str table => size = string size (?)
    char* str_v; // poitert ot string => size = string size (?)
    struct ListElem list_v; // elem => size = list size
    struct Var* array_v; // pointer to array of elements => size = array size
    char* fun_v; // pointer to function definition => size = 1
  } value;
};

struct Frame {
  struct Var* params; // store arguments
  struct Var* ret; // store returned value
  struct Var* locals; // store locals
  size_t locals_sz;
  size_t params_sz;

  char* rp; // ret instruction pointer
};

// TODO: store globals in some way ?? // maybe some first vars ??
struct State {
  struct Var* vars;
  struct Frame* funcs;
  struct Var* vp; // var pointer
  struct Frame* fp; // function pointer

  char* ip; // instruction pointer
  char* prev_ip; // prev instruction pointer
};

struct State init_state(bytefile *bf) {
  struct State state = {
    .vars = calloc(1000/* TODO */, sizeof(struct Var)),
    .funcs = calloc(1000/* TODO */, sizeof(struct Frame)),
    .ip = bf->code_ptr,
    .prev_ip = NULL,
  };

  // TODO: init vars vith NIL_T ??

  state.vp = state.vars;
  state.fp = state.funcs;
  return state;
}

void free_state(struct State* state) {
  free(state->vars);
  free(state->funcs);

  state->vars = NULL;
  state->funcs = NULL;
  state->vp = NULL;
  state->fp = NULL;
  state->ip = NULL;
  state->prev_ip = NULL;
}

//

void free_var_ptr(struct Var* var);

void free_var(struct Var var) {
  switch (var.type) {
    case INT_T:
      break;
    case CONST_STR_T:
      break;
    case STR_T:
      free(var.value.str_v);
      break;
    case LIST_T:
      if (var.value.list_v.elem != NULL) {
        free_var_ptr(var.value.list_v.elem);
      }
      if (var.value.list_v.next != NULL) {
        free_var_ptr(var.value.list_v.next);
      }
      break;
    case ARRAY_T:
      for (size_t i = 0; i < var.size; ++i) {
        free_var(*(var.value.array_v + i));
      }
      free(var.value.array_v);
      break;
    case FUN_T:
      break;
    case NIL_T:
      break;
  }
}

void free_var_ptr(struct Var* var) {
  free_var(*var);
  free(var);
}

//

struct Var clear_var() {
  struct Var var = {
    .type = NIL_T,
    .size = 0,
  };
  return var;
}

struct Frame clear_frame() {
  struct Frame frame = {
    .params = NULL,
    .ret = NULL,
    .locals = NULL,
    .params_sz = 0,
    .locals_sz = 0,
  };
  return frame;
}

struct Var deep_copy_var(struct Var var) {
  switch (var.type) {
    case INT_T:
      break;
    case CONST_STR_T:
      break;
    case STR_T: {
        char* old_str = var.value.str_v;
        var.value.str_v = calloc(var.size + 1, sizeof(char));
        strcpy(var.value.str_v, old_str);
        break;
    }
    case LIST_T:
      if (var.value.list_v.elem != NULL) {
        struct Var* old_elem = var.value.list_v.elem;
        var.value.list_v.elem = calloc(1, sizeof(struct Var));
        *var.value.list_v.elem = deep_copy_var(*old_elem);
      }
      if (var.value.list_v.next != NULL) {
        struct Var* old_next = var.value.list_v.next;
        var.value.list_v.next = calloc(1, sizeof(struct Var));
        *var.value.list_v.next = deep_copy_var(*old_next);
      }
      break;
    case ARRAY_T: {
      struct Var *old_array = var.value.array_v;
      var.value.array_v = calloc(var.size, sizeof(char));
      for (size_t i = 0; i < var.size; ++i) {
        var.value.array_v[i] = deep_copy_var(*(old_array + i));
      }
      break;
    }
    case FUN_T:
      break;
    case NIL_T:
      break;
  }

  return var;
}

//

void s_put_var(struct State* s, struct Var val) {
  *s->vp = val;
  ++s->vp;
}
void s_put_nil(struct State* s) {
  struct Var var = {.type = NIL_T, .size = 0};
  s_put_var(s, var);
}
void s_putn_nil(struct State* s, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    s_put_nil(s);
  }
}
void s_put_i(struct State* s, int val) {
  struct Var var = {.type = INT_T, .value.int_v = val, .size = 1};
  s_put_var(s, var);
}
void s_put_const_str(struct State* s, const char* val) { // memory controlled externally
  struct Var var = {.type = CONST_STR_T, .value.const_str_v = val, .size = strlen(val)};
  s_put_var(s, var);
}
void s_put_str(struct State* s, char* val) { // memory controlled by var
  struct Var var = {.type = STR_T, .value.str_v = val, .size = strlen(val)};
  s_put_var(s, var);
}
void s_put_array(struct State* s, int sz) { // memory controlled by var
  struct Var var = {
      .type = ARRAY_T,
      .value.array_v = calloc(sz, sizeof(struct Var)),
      .size = sz,
  };
  s_put_var(s, var);

  // fill array with nils ?
}

void s_put_list(struct State* s, struct Var* first_elem) { // memory controlled by var
  struct Var var = {
      .type = LIST_T,
      .value.list_v = {.elem = first_elem, .next = NULL},
      .size = 0,
  }; // TODO: size ?
  s_put_var(s, var);

  *first_elem = clear_var();
}

//

struct Var s_take_var(struct State* s) {
  if (s->vp == s->vars) {
    failure("take: no var");
  }
  --s->vp;

  struct Var ret = *s->vp;
  *s->vp = clear_var(); // clear top var
  return ret;
}

int s_take_i(struct State* s) {
  struct Var v = s_take_var(s);
  if (v.type != INT_T) {
    failure("take int: not int");
  }
  return v.value.int_v;
}

void s_drop_var(struct State* s) {
  if (s->vp == s->vars) {
    failure("drop: no var");
  }
  --s->vp;
  free_var(*s->vp);

  *s->vp = clear_var(); // clear top var
}

void s_dropn_var(struct State* s, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    s_drop_var(s);
  }
}

//

void s_exit_f(struct State* s) {
  if (s->fp == s->funcs) {
    failure("exit: no func");
  }
  --s->fp;
  s_dropn_var(s, s->vp - s->fp->locals); // drop local var stack and locals // TODO: check +-1
  struct Var ret = *s->vp;
  --s->vp;
  s_dropn_var(s, s->vp - s->fp->params); // drop params // TODO: check +-1
  s_put_var(s, ret);
  s->ip = s->fp->rp;

  *s->fp = clear_frame(); // clear top frame
}

void s_enter_f(struct State* s, char* func_ip, size_t params_sz, size_t locals_sz) {
  struct Frame frame = {
      .params = s->vp - params_sz, // TODO: check +-1
      .ret = s->vp,
      .locals = s->vp + 1,
      .params_sz = params_sz,
      .locals_sz = locals_sz,
      .rp = s->ip,
  };
  s_put_nil(s); // ret
  s_putn_nil(s, locals_sz); // locals
  s->ip = func_ip;
  (*s->fp) = frame;
  ++s->fp;
}

//

// --- builtin calls
void f_read(struct State* s) {
  int x = 0;
  printf("> "); // TODO: ??
  scanf("%i", &x);
  s_put_i(s, x);
}

void f_write(struct State* s) {
  int x = s_take_i(s);
  printf("%i", x);
  // put 0 ??
}
void f_length(struct State* s) {
  // TODO
}
void f_string(struct State* s) {
  // TODO
}
void f_array(struct State* s, int sz) {
  // TODO
}
void f_cons(struct State* s) {
  
  // TODO
}
//

void run(bytefile *bf) {
  struct State s = init_state(bf);

  const char *ops [] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  const char *pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
  const char *lds [] = {"LD", "LDA", "ST"};
  do {
    char* before_op_ip = s.ip; // save to set s.prev_ip
 
    char x = read_byte(&s.ip),
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
        s_put_i(&s, read_int(&s.ip));
        break;
        
      case  1: // STRING %s
        s_put_const_str(&s, read_string(&s.ip, bf));
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
        s.ip = (char*)(long)read_int(&s.ip); // TODO: check
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
        char* new_ip = (char*)(long)read_int(&s.ip); // TODO: check
        if (s_take_i(&s) != 0) { // FIXME: bools ??, other vars ??
          s.ip = new_ip;
        }
        break;
      }
      case 1: { // CJMPnz  0x%.8x
        // FIXME: TODO: jump by top stack condition ??i
        char* new_ip = (char*)(long)read_int(&s.ip); // TODO: check
        if (s_take_i(&s) == 0) { // FIXME: bools ??, other vars ??
          s.ip = new_ip;
        }
        break;
      }
      case  2: // BEGIN %d %d
        s_enter_f(&s, s.prev_ip/*ip from call*/, read_int(&s.ip), read_int(&s.ip));
        // TODO: is func enter ?
        break;
        
      case  3: // CBEGIN %d %d
        s_enter_f(&s, s.prev_ip/*ip from call*/, read_int(&s.ip), read_int(&s.ip));
        // TODO: is func enter ?
        break;
        
      case  4: // CLOSURE 0x%.8x
        {
          int n = read_int(&s.ip);
          for (int i = 0; i < n; i++) {
            switch (read_byte(&s.ip)) {
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
        s.ip = (char*)(long)read_int(&s.ip); // TODO: check
        break;
        
      case  6: // CALL 0x%.8x %d
        // TODO: second arg is given params amount ??
        // TODO: jump only ??
        s.ip = (char*)(long)read_int(&s.ip); // TODO: check
        break;
        
      case  7: // TAG %s
        // TODO: ??
        break;
        
      case  8: // ARRAY %d
        s_put_array(&s, read_int(&s.ip));
        break;
        
      case  9: // FAIL %d %d
        // TODO: ??
        failure("FAIL: %d-%d\n", read_int(&s.ip), read_int(&s.ip));
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
        f_array(&s, read_int(&s.ip));
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
