#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#include "../../runtime/runtime_common.h"

#define WORD_SIZE (CHAR_BIT * sizeof(int))

// ---

void *Bsexp(int n, ...);
int LtagHash(char *);

// Gets a raw data_header
int LkindOf(void *p);

// Compare s-exprs tags
int LcompareTags(void *p, void *q);

// Functional synonym for built-in operator ":";
void *Ls__Infix_58(void *p, void *q);

// Functional synonym for built-in operator "!!";
int Ls__Infix_3333(void *p, void *q);

// Functional synonym for built-in operator "&&";
int Ls__Infix_3838(void *p, void *q);

// Functional synonym for built-in operator "==";
int Ls__Infix_6161(void *p, void *q);

// Functional synonym for built-in operator "!=";
int Ls__Infix_3361(void *p, void *q);

// Functional synonym for built-in operator "<=";
int Ls__Infix_6061(void *p, void *q);

// Functional synonym for built-in operator "<";
int Ls__Infix_60(void *p, void *q);

// Functional synonym for built-in operator ">=";
int Ls__Infix_6261(void *p, void *q);

// Functional synonym for built-in operator ">";
int Ls__Infix_62(void *p, void *q);

// Functional synonym for built-in operator "+";
int Ls__Infix_43(void *p, void *q);

// Functional synonym for built-in operator "-";
int Ls__Infix_45(void *p, void *q);

// Functional synonym for built-in operator "*";
int Ls__Infix_42(void *p, void *q);

// Functional synonym for built-in operator "/";
int Ls__Infix_47(void *p, void *q);

// Functional synonym for built-in operator "%";
int Ls__Infix_37(void *p, void *q);

int Llength(void *p);

int LtagHash(char *s);
char *de_hash(int n);

int Luppercase(void *v);

int Llowercase(void *v);

int LmatchSubString(char *subj, char *patt, int pos);

void *Lsubstring(void *subj, int p, int l);

struct re_pattern_buffer *Lregexp(char *regexp);

int LregexpMatch(struct re_pattern_buffer *b, char *s, int pos);

void *Bstring(void *p);

void *Lclone(void *p);

int inner_hash(int depth, unsigned acc, void *p);

void *LstringInt(char *b);

int Lhash(void *p);

int LflatCompare(void *p, void *q);
int Lcompare(void *p, void *q);

void *Belem(void *p, int i);

void *LmakeArray(int length);
void *LmakeString(int length);

void *Bstring(void *p);
void *Lstringcat(void *p);
void *Lstring(void *p);

void *Bclosure(int bn, void *entry, ...);
void *Barray(int bn, ...);
void *Bsexp(int bn, ...);
int Btag(void *d, int t, int n);

int get_tag(data *d);
int get_len(data *d);

int Barray_patt(void *d, int n);
int Bstring_patt(void *x, void *y);
int Bclosure_tag_patt(void *x);
int Bboxed_patt(void *x);
int Bunboxed_patt(void *x);
int Barray_tag_patt(void *x);
int Bstring_tag_patt(void *x);
int Bsexp_tag_patt(void *x);

void *Bsta(void *v, int i, void *x);

void Lfailure(char *s, ...);
void LprintfPerror(char *s, ...);

void Bmatch_failure(void *v, char *fname, int line, int col);

void * /*Lstrcat*/ Li__Infix_4343(void *a, void *b);

void *Lsprintf(char *fmt, ...);
void *LgetEnv(char *var);

int Lsystem(char *cmd);

void Lfprintf(FILE *f, char *s, ...);
void Lprintf(char *s, ...);
FILE *Lfopen(char *f, char *m);
void Lfclose(FILE *f);
void *LreadLine();
void *Lfread(char *fname);
void Lfwrite(char *fname, char *contents);
void *Lfexists(char *fname);

void *Lfst(void *v);
void *Lsnd(void *v);
void *Lhd(void *v);
void *Ltl(void *v);

/* Lread is an implementation of the "read" construct */
int Lread();

int Lbinoperror(void);

int Lbinoperror2(void);

/* Lwrite is an implementation of the "write" construct */
int Lwrite(int n);

int Lrandom(int n);

int Ltime();

void set_args(int argc, char *argv[]);
