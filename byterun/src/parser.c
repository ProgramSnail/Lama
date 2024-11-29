/* Lama SM Bytecode interpreter */

#include <string.h>
#include <errno.h>
#include <malloc.h>

#include "../../runtime/runtime.h"

#include "types.h"
#include "utils.h"
#include "parser.h"

void *__start_custom_data;
void *__stop_custom_data;

/* Reads a binary bytecode file by name and unpacks it */
bytefile* read_file (const char *fname) {
  FILE *f = fopen (fname, "rb");
  bytefile *file;

  if (f == 0) {
    failure ("%s\n", strerror (errno));
  }
  
  if (fseek (f, 0, SEEK_END) == -1) {
    failure ("%s\n", strerror (errno));
  }

  long size = ftell (f);
  long additional_size = sizeof(void*) * 5 + sizeof(int);
  file = (bytefile*) malloc (size + additional_size); // file itself + additional data

  char* file_begin = (char*)file + additional_size;
  char* file_end = file_begin + size;

  if (file == 0) {
    failure ("unable to allocate memory to store file data\n");
  }

  rewind (f);

  if (size != fread (&file->stringtab_size, 1, size, f)) {
    failure ("%s\n", strerror (errno));
  }
  
  fclose (f);

  long imports_size = file->imports_number * sizeof(int);
  long public_symbols_size = file->public_symbols_number * 2 * sizeof(int);
  long strings_buffer_offset = public_symbols_size + imports_size;
  if (file->buffer + strings_buffer_offset >= file_end) {
    failure ("public symbols are out of the file size\n");
  }
  file->string_ptr = &file->buffer[strings_buffer_offset];
  if (file->string_ptr + file->stringtab_size > file_end) {
    failure ("strings table is out of the file size\n");
  }
  if (file->stringtab_size > 0 && file->string_ptr[file->stringtab_size - 1] != 0) {
    failure ("strings table is not zero-ended\n");
  }
  if (file->code_size < 0 || public_symbols_size < 0 || file->stringtab_size < 0) {
    failure ("file zones sizes should be >= 0\n");
  }

  file->imports_ptr  = (int*) file->buffer;
  file->public_ptr  = (int*) (file->buffer + imports_size);
  file->code_ptr    = &file->string_ptr [file->stringtab_size];
  file->global_ptr  = (int*) calloc (file->global_area_size, sizeof (int));

  file->code_size = size - strings_buffer_offset - file->stringtab_size;

  return file;
}

const char *read_cmd(const char *ip) {
  uint8_t x = (*ip++), h = (x & 0xF0) >> 4, l = x & 0x0F;

  switch (h) {
  case CMD_EXIT:
    return "END";
  case CMD_BINOP:
    return "BINOP";
  case CMD_BASIC:
    switch (l) {
    case CMD_BASIC_CONST:
      return "CONST";
    case CMD_BASIC_STRING:
      return "STRING";
    case CMD_BASIC_SEXP:
      return "SEXP ";
    case CMD_BASIC_STI:
      return "STI";
    case CMD_BASIC_STA:
      return "STA";
    case CMD_BASIC_JMP:
      return "JMP";
    case CMD_BASIC_END:
      return "END";
    case CMD_BASIC_RET:
      return "RET";
    case CMD_BASIC_DROP:
      return "DROP";
    case CMD_BASIC_DUP:
      return "DUP";
    case CMD_BASIC_SWAP:
      return "SWAP";
    case CMD_BASIC_ELEM:
      return "ELEM";
    default:
      return "_UNDEF_CMD1_";
    }
    break;

  case CMD_LD:
    return "LD";
  case CMD_LDA:
    return "LDA";
  case CMD_ST:
    return "ST";
  case CMD_CTRL:
    switch (l) {
    case CMD_CTRL_CJMPz:
      return "CJMPz";
    case CMD_CTRL_CJMPnz:
      return "CJMPnz";
    case CMD_CTRL_BEGIN:
      return "BEGIN";
    case CMD_CTRL_CBEGIN:
      return "CBEGIN";
    case CMD_CTRL_CLOSURE:
      return "CLOSURE";
    case CMD_CTRL_CALLC:
      return "CALLC";
    case CMD_CTRL_CALL:
      return "CALL";
    case CMD_CTRL_TAG:
      return "TAG";
    case CMD_CTRL_ARRAY:
      return "ARRAY";
    case CMD_CTRL_FAIL:
      return "FAIL";
    case CMD_CTRL_LINE:
      return "LINE";
    case CMD_CTRL_CALLF:
      return "CALL";
    default:
      return "_UNDEF_CMD5_";
    }
  case CMD_PATT:
    return "PATT";
  case CMD_BUILTIN: {
    switch (l) {
    case CMD_BUILTIN_Lread:
      return "CALL\tLread";
    case CMD_BUILTIN_Lwrite:
      return "CALL\tLwrite";
    case CMD_BUILTIN_Llength:
      return "CALL\tLlength";
    case CMD_BUILTIN_Lstring:
      return "CALL\tLstring";
    case CMD_BUILTIN_Barray:
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
    uint8_t x = BYTE,
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

      case 11:
        fprintf (f, "CALLF\t%s ", STRING);
        fprintf (f, "%d", INT);
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
  fprintf (f, "Number of imports       : %d\n", bf->imports_number);
  fprintf (f, "Number of public symbols: %d\n", bf->public_symbols_number);
  fprintf (f, "Imports                 :\n");

  for (i=0; i < bf->imports_number; i++) 
    fprintf (f, "   %s\n", get_import (bf, i));

  fprintf (f, "Public symbols          :\n");

  for (i=0; i < bf->public_symbols_number; i++) 
    fprintf (f, "   0x%.8x: %s\n", get_public_offset (bf, i), get_public_name (bf, i));

  fprintf (f, "Code:\n");
  disassemble (f, bf);
}

