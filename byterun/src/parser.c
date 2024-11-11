/* Lama SM Bytecode interpreter */

#include <string.h>
#include <errno.h>
#include <malloc.h>

#include "../../runtime/runtime.h"

#include "utils.h"
#include "parser.h"

void *__start_custom_data;
void *__stop_custom_data;

/* Reads a binary bytecode file by name and unpacks it */
bytefile* read_file (char *fname) {
  FILE *f = fopen (fname, "rb");
  bytefile *file;

  if (f == 0) {
    failure ("%s\n", strerror (errno));
  }
  
  if (fseek (f, 0, SEEK_END) == -1) {
    failure ("%s\n", strerror (errno));
  }

  long size = ftell (f);
  file = (bytefile*) malloc (size + sizeof(void*) * 4);

  if (file == 0) {
    failure ("unable to allocate memory to store file data\n");
  }
  
  rewind (f);

  if (size != fread (&file->stringtab_size, 1, size, f)) {
    failure ("%s\n", strerror (errno));
  }
  
  fclose (f);
  
  file->string_ptr  = &file->buffer [file->public_symbols_number * 2 * sizeof(int)];
  file->public_ptr  = (int*) file->buffer;
  file->code_ptr    = &file->string_ptr [file->stringtab_size];
  file->global_ptr  = (int*) calloc (file->global_area_size, sizeof (int));
  
  return file;
}

const char *read_cmd(char *ip) {
  char x = (*ip++), h = (x & 0xF0) >> 4, l = x & 0x0F;

  switch (h) {
  case 15:
    return "END";
  case 0:
    return "BINOP";
  case 1:
    switch (l) {
    case 0:
      return "CONST";
    case 1:
      return "STRING";
    case 2:
      return "SEXP ";
    case 3:
      return "STI";
    case 4:
      return "STA";
    case 5:
      return "JMP";
    case 6:
      return "END";
    case 7:
      return "RET";
    case 8:
      return "DROP";
    case 9:
      return "DUP";
    case 10:
      return "SWAP";
    case 11:
      return "ELEM";
    default:
      return "_UNDEF_CMD1_";
    }
    break;

  case 2:
    return "LD";
  case 3:
    return "LDA";
  case 4:
    return "ST";
  case 5:
    switch (l) {
    case 0:
      return "CJMPz";
    case 1:
      return "CJMPnz";
    case 2:
      return "BEGIN";
    case 3:
      return "CBEGIN";
    case 4:
      return "CLOSURE";
    case 5:
      return "CALLC";
    case 6:
      return "CALL";
    case 7:
      return "TAG";
    case 8:
      return "ARRAY";
    case 9:
      return "FAIL";
    case 10:
      return "LINE";
    default:
      return "_UNDEF_CMD5_";
    }
  case 6:
    return "PATT";
  case 7: {
    switch (l) {
    case 0:
      return "CALL\tLread";
    case 1:
      return "CALL\tLwrite";
    case 2:
      return "CALL\tLlength";
    case 3:
      return "CALL\tLstring";
    case 4:
      return "CALL\tBarray\t%d";
    default:
      return "_UNDEF_CALL_";
    }
  }
  default:
    return "_UNDEF_CODE_";
  }
}

/* Disassembles the bytecode pool */
void disassemble (FILE *f, bytefile *bf) {

# define INT    (ip += sizeof (int), *(int*)(ip - sizeof (int)))
# define BYTE   *ip++
# define STRING get_string (bf, INT)
# define FAIL   failure ("ERROR: invalid opcode %d-%d\n", h, l)

  char *ip     = bf->code_ptr;
  char *ops [] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  char *pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
  char *lds [] = {"LD", "LDA", "ST"};
  do {
    char x = BYTE,
         h = (x & 0xF0) >> 4,
         l = x & 0x0F;

    fprintf (f, "0x%.8x:\t", ip-bf->code_ptr-1);

    switch (h) {
    case 15:
      goto stop;
      
    /* BINOP */
    case 0:
      fprintf (f, "BINOP\t%s", ops[l-1]);
      break;
      
    case 1:
      switch (l) {
      case  0:
        fprintf (f, "CONST\t%d", INT);
        break;
        
      case  1:
        fprintf (f, "STRING\t%s", STRING);
        break;
          
      case  2:
        fprintf (f, "SEXP\t%s ", STRING);
        fprintf (f, "%d", INT);
        break;
        
      case  3:
        fprintf (f, "STI");
        break;
        
      case  4:
        fprintf (f, "STA");
        break;
        
      case  5:
        fprintf (f, "JMP\t0x%.8x", INT);
        break;
        
      case  6:
        fprintf (f, "END");
        break;
        
      case  7:
        fprintf (f, "RET");
        break;
        
      case  8:
        fprintf (f, "DROP");
        break;
        
      case  9:
        fprintf (f, "DUP");
        break;
        
      case 10:
        fprintf (f, "SWAP");
        break;

      case 11:
        fprintf (f, "ELEM");
        break;
        
      default:
        FAIL;
      }
      break;
      
    case 2:
    case 3:
    case 4:
      fprintf (f, "%s\t", lds[h-2]);
      switch (l) {
      case 0: fprintf (f, "G(%d)", INT); break;
      case 1: fprintf (f, "L(%d)", INT); break;
      case 2: fprintf (f, "A(%d)", INT); break;
      case 3: fprintf (f, "C(%d)", INT); break;
      default: FAIL;
      }
      break;
      
    case 5:
      switch (l) {
      case  0:
        fprintf (f, "CJMPz\t0x%.8x", INT);
        break;
        
      case  1:
        fprintf (f, "CJMPnz\t0x%.8x", INT);
        break;
        
      case  2:
        fprintf (f, "BEGIN\t%d ", INT);
        fprintf (f, "%d", INT);
        break;
        
      case  3:
        fprintf (f, "CBEGIN\t%d ", INT);
        fprintf (f, "%d", INT);
        break;
        
      case  4:
        fprintf (f, "CLOSURE\t0x%.8x", INT);
        {int n = INT;
         for (int i = 0; i<n; i++) {
         switch (BYTE) {
           case 0: fprintf (f, "G(%d)", INT); break;
           case 1: fprintf (f, "L(%d)", INT); break;
           case 2: fprintf (f, "A(%d)", INT); break;
           case 3: fprintf (f, "C(%d)", INT); break;
           default: FAIL;
         }
         }
        };
        break;
          
      case  5:
        fprintf (f, "CALLC\t%d", INT);
        break;
        
      case  6:
        fprintf (f, "CALL\t0x%.8x ", INT);
        fprintf (f, "%d", INT);
        break;
        
      case  7:
        fprintf (f, "TAG\t%s ", STRING);
        fprintf (f, "%d", INT);
        break;
        
      case  8:
        fprintf (f, "ARRAY\t%d", INT);
        break;
        
      case  9:
        fprintf (f, "FAIL\t%d", INT);
        fprintf (f, "%d", INT);
        break;
        
      case 10:
        fprintf (f, "LINE\t%d", INT);
        break;

      default:
        FAIL;
      }
      break;
      
    case 6:
      fprintf (f, "PATT\t%s", pats[l]);
      break;

    case 7: {
      switch (l) {
      case 0:
        fprintf (f, "CALL\tLread");
        break;
        
      case 1:
        fprintf (f, "CALL\tLwrite");
        break;

      case 2:
        fprintf (f, "CALL\tLlength");
        break;

      case 3:
        fprintf (f, "CALL\tLstring");
        break;

      case 4:
        fprintf (f, "CALL\tBarray\t%d", INT);
        break;

      default:
        FAIL;
      }
    }
    break;
      
    default:
      FAIL;
    }

    fprintf (f, "\n");
  }
  while (1);
 stop: fprintf (f, "<end>\n");
}

/* Dumps the contents of the file */
void dump_file (FILE *f, bytefile *bf) {
  size_t i;
  
  fprintf (f, "String table size       : %d\n", bf->stringtab_size);
  fprintf (f, "Global area size        : %d\n", bf->global_area_size);
  fprintf (f, "Number of public symbols: %d\n", bf->public_symbols_number);
  fprintf (f, "Public symbols          :\n");

  for (i=0; i < bf->public_symbols_number; i++) 
    fprintf (f, "   0x%.8x: %s\n", get_public_offset (bf, i), get_public_name (bf, i));

  fprintf (f, "Code:\n");
  disassemble (f, bf);
}

