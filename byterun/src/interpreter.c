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
#define FAIL   failure ("ERROR: invalid opcode %d-%d\n", h, l)

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
};

struct State init_state(bytefile *bf) {
  struct State state = {
    .vars = calloc(1000/* TODO */, sizeof(struct Var)),
    .funcs = calloc(1000/* TODO */, sizeof(struct Frame)),
    .ip = bf->code_ptr,
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
  struct Var var = {.type = CONST_STR_T, .value.const_str_v = val, .size = 0}; // TODO: size ?
  s_put_var(s, var);
}
void s_put_str(struct State* s, char* val) { // memory controlled by var
  struct Var var = {.type = STR_T, .value.str_v = val, .size = 0}; // TODO: size ?
  s_put_var(s, var);
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

  s->fp = clear_frame(); // clear top frame
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

// TODO: builtin calls
//

void run(bytefile *bf) {
  struct State s = init_state(bf);

  const char *ops [] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  const char *pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
  const char *lds [] = {"LD", "LDA", "ST"};
  do {
    char x = read_byte(&s.ip),
         h = (x & 0xF0) >> 4,
         l = x & 0x0F;

    // fprintf (f, "0x%.8x:\t", ip-bf->code_ptr-1);

    switch (h) {
    case 15:
      goto stop;
      
    /* BINOP */
    case 0:
      // fprintf (f, "BINOP\t%s", ops[l-1]);
      break;
      
    case 1:
      switch (l) {
      case  0:
        stack_put_i(read_int(&ip));
        // fprintf (f, "CONST\t%d", INT);
        break;
        
      case  1:
        stack_put_str(read_string(&ip, bf));
        // fprintf (f, "STRING\t%s", STRING);
        break;
          
      case  2:
        // TODO: call sexp ??
        // fprintf (f, "SEXP\t%s ", STRING);
        // fprintf (f, "%d", INT);
        break;
        
      case  3:
        // fprintf (f, "STI");
        break;
        
      case  4:
        // fprintf (f, "STA");
        break;
        
      case  5:
        // fprintf (f, "JMP\t0x%.8x", INT);
        break;
        
      case  6:
        // fprintf (f, "END");
        break;
        
      case  7:
        // fprintf (f, "RET");
        break;
        
      case  8:
        // fprintf (f, "DROP");
        break;
        
      case  9:
        // fprintf (f, "DUP");
        break;
        
      case 10:
        // fprintf (f, "SWAP");
        break;

      case 11:
        // fprintf (f, "ELEM");
        break;
        
      default:
        FAIL;
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
      default: FAIL;
      }
      break;
      
    case 5:
      switch (l) {
      case  0:
        // fprintf (f, "CJMPz\t0x%.8x", INT);
        break;
        
      case  1:
        // fprintf (f, "CJMPnz\t0x%.8x", INT);
        break;
        
      case  2:
        // fprintf (f, "BEGIN\t%d ", INT);
        // fprintf (f, "%d", INT);
        break;
        
      case  3:
        // fprintf (f, "CBEGIN\t%d ", INT);
        // fprintf (f, "%d", INT);
        break;
        
      case  4:
        // fprintf (f, "CLOSURE\t0x%.8x", INT);
        {int n = INT;
         for (int i = 0; i<n; i++) {
         switch (BYTE) {
           // case 0: fprintf (f, "G(%d)", INT); break;
           // case 1: fprintf (f, "L(%d)", INT); break;
           // case 2: fprintf (f, "A(%d)", INT); break;
           // case 3: fprintf (f, "C(%d)", INT); break;
           default: FAIL;
         }
         }
        };
        break;
          
      case  5:
        // fprintf (f, "CALLC\t%d", INT);
        break;
        
      case  6:
        // fprintf (f, "CALL\t0x%.8x ", INT);
        // fprintf (f, "%d", INT);
        break;
        
      case  7:
        // fprintf (f, "TAG\t%s ", STRING);
        // fprintf (f, "%d", INT);
        break;
        
      case  8:
        // fprintf (f, "ARRAY\t%d", INT);
        break;
        
      case  9:
        // fprintf (f, "FAIL\t%d", INT);
        // fprintf (f, "%d", INT);
        break;
        
      case 10:
        // fprintf (f, "LINE\t%d", INT);
        break;

      default:
        FAIL;
      }
      break;
      
    case 6:
      // fprintf (f, "PATT\t%s", pats[l]);
      break;

    case 7: {
      switch (l) {
      case 0:
        // fprintf (f, "CALL\tLread");
        read_int();
        break;
        
      case 1:
        // fprintf (f, "CALL\tLwrite");
        write_int();
        break;

      case 2:
        // fprintf (f, "CALL\tLlength");
        break;

      case 3:
        // fprintf (f, "CALL\tLstring");
        break;

      case 4:
        // fprintf (f, "CALL\tBarray\t%d", INT);
        break;

      default:
        FAIL;
      }
    }
    break;
      
    default:
      FAIL;
    }

    // fprintf (f, "\n");
  }
  while (1);
stop:; // fprintf (f, "<end>\n");i
  free_state(&s);
}
