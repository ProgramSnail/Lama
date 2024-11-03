#pragma once

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

// ---

void Lassert(void *f, char *s, ...);

// Gets a raw data_header
aint LkindOf(void *p);

// Compare s-exprs tags
aint LcompareTags(void *p, void *q);

// Functional synonym for built-in operator ":";
void *Ls__Infix_58(void *p, void *q);

// Functional synonym for built-in operator "!!";
aint Ls__Infix_3333(void *p, void *q);

// Functional synonym for built-in operator "&&";
aint Ls__Infix_3838(void *p, void *q);

// Functional synonym for built-in operator "==";
aint Ls__Infix_6161(void *p, void *q);

// Functional synonym for built-in operator "!=";
aint Ls__Infix_3361(void *p, void *q);

// Functional synonym for built-in operator "<=";
aint Ls__Infix_6061(void *p, void *q);

// Functional synonym for built-in operator "<";
aint Ls__Infix_60(void *p, void *q);

// Functional synonym for built-in operator ">=";
aint Ls__Infix_6261(void *p, void *q);

// Functional synonym for built-in operator ">";
aint Ls__Infix_62(void *p, void *q);

// Functional synonym for built-in operator "+";
aint Ls__Infix_43(void *p, void *q);

// Functional synonym for built-in operator "-";
aint Ls__Infix_45(void *p, void *q);

// Functional synonym for built-in operator "*";
aint Ls__Infix_42(void *p, void *q);

// Functional synonym for built-in operator "/";
aint Ls__Infix_47(void *p, void *q);

// Functional synonym for built-in operator "%";
aint Ls__Infix_37(void *p, void *q);

aint Llength(void *p);

aint LtagHash(char *s);
char *de_hash(aint n);

aint Luppercase(void *v);

aint Llowercase(void *v);

aint LmatchSubString(char *subj, char *patt, aint pos);

void *Lsubstring(aint *args /*void *subj, aint p, aint l*/);

regex_t *Lregexp(char *regexp);

aint LregexpMatch(struct re_pattern_buffer *b, char *s, aint pos);

void *Lclone(void *p);

aint inner_hash(aint depth, auint acc, void *p);

void *LstringInt(char *b);

aint Lhash(void *p);

aint LflatCompare(void *p, void *q);
aint Lcompare(void *p, void *q);

void *Belem(void *p, aint i);

void *LmakeArray(aint length);
void *LmakeString(aint length);

void *Bstring(aint *p);
void *Lstringcat(aint *p);
void *Lstring(aint *p);

void *Bclosure(aint *args, aint bn);
void *Barray(aint *args, aint bn);
void *Bsexp(aint *args, aint bn);
aint Btag(void *d, aint t, aint n);

aint get_tag(data *d);
aint get_len(data *d);

aint Barray_patt(void *d, aint n);
aint Bstring_patt(void *x, void *y);
aint Bclosure_tag_patt(void *x);
aint Bboxed_patt(void *x);
aint Bunboxed_patt(void *x);
aint Barray_tag_patt(void *x);
aint Bstring_tag_patt(void *x);
aint Bsexp_tag_patt(void *x);

void *Bsta(void *v, aint i, void *x);

void Lfailure(char *s, ...);
void LprintfPerror(char *s, ...);

void Bmatch_failure(void *v, char *fname, aint line, aint col);

void * /*Lstrcat*/ Li__Infix_4343(void *a, void *b);

void *Lsprintf(char *fmt, ...);
void *LgetEnv(char *var);

aint Lsystem(char *cmd);

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
aint Lread();

aint Lbinoperror(void);

aint Lbinoperror2(void);

/* Lwrite is an implementation of the "write" construct */
aint Lwrite(aint n);

aint Lrandom(aint n);

aint Ltime();

void set_args(aint argc, char *argv[]);
