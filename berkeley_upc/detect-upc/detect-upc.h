
#ifndef __DETECT_UPC_H
#define __DETECT_UPC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void handle_include(char *line_directive);
void mark_as_upc(char *keyword, int isPragma);
void mark_as_c(char *keyword);
void insert_pragma(char *yytxt);
void print_out(char *str, int len);

/* input and output files */
extern FILE *ifile, *ofile;
extern int do_output;

/* mode flag */
extern int in_c;

enum mark_reason {
    PRAGMA = 1,
    SUFFIX,
    LEXXER
};

/* This gets rid of a flex warning */
#define YY_NO_UNPUT

#endif /* __DETECT_UPC_H */

