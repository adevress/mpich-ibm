
%{
#include "detect-upc.h"
%}

 /* 
  * UPC keywords and standard functions: the presence of any of these 
  * in a file indicate that it contains UPC code for our purposes.
  * Note that 'shared', 'strict' and 'relaxed' are handled separately
  * to allow for their use as valid identifiers in C-mode headers.
  */
upc1  THREADS|MYTHREAD|upc_forall|upc_global_exit
upc2  upc_global_alloc|upc_all_alloc|upc_alloc|upc_local_alloc|upc_free
upc3  upc_localsizeof|upc_blocksizeof|UPC_MAX_BLOCK_SIZE
upc4  upc_elemsizeof|upc_affinitysize
upc5  upc_phaseof|upc_threadof|upc_resetphase|upc_addrfield
upc6  upc_notify|upc_wait|upc_barrier|upc_fence
upc7  upc_lock_t|upc_global_lock_alloc|upc_all_lock_alloc|upc_lock_free
upc8  upc_lock|upc_lock_attempt|upc_unlock
upc9  upc_memcpy|upc_memget|upc_memput|upc_memset

 /* Since upc pragmas use a '^' start of line match, we can't
  * group them together with the other rules */
ws [\ \t\n]
blanks {ws}+
upc_code     ^{ws}*#{ws}*pragma{ws}+upc{ws}+upc_code
c_code       ^{ws}*#{ws}*pragma{ws}+upc{ws}+c_code
startupconly ^{ws}*#{ws}*pragma{ws}+upc{ws}+start_upc_only{ws}*\n
endupconly   ^{ws}*#{ws}*pragma{ws}+upc{ws}+end_upc_only{ws}*\n
upc_pragma   ^{ws}*#{ws}*pragma{ws}+upc{ws}+(strict|relaxed)
upc_token {upc1}|{upc2}|{upc3}|{upc4}|{upc5}|{upc6}|{upc7}|{upc8}|{upc9}
upc_shared (shared|relaxed|strict)

literalslash    \\\\
charquote       '\"'
doublequote     \"
escapedquote    \\\"
line_number     [1-9][0-9]*
entering_file   ^#{ws}*(line)?{ws}*{line_number}{ws}+\"[^<>"]+\".*\n
wholeline       ^.*\n
token           [_A-Za-z0-9]+
brackets        \[[^]]*\]

%s INSTRING INSHARED OUTSTRING OUTPUT INUPCONLY OUTUPCONLY

%%
    if (do_output) 
        BEGIN OUTPUT;

<INITIAL>{entering_file} { handle_include(yytext); }
<INITIAL>{startupconly}  { BEGIN INUPCONLY; }
<INITIAL>{charquote}     ;
<INITIAL>{upc_code}      { mark_as_upc(yytext, PRAGMA);} 
<INITIAL>{c_code}        { mark_as_c(yytext); }
<INITIAL>{upc_pragma}    { mark_as_upc(yytext, LEXXER);} 
<INITIAL>{upc_token}     { mark_as_upc(yytext, LEXXER);} 
<INITIAL>{upc_shared}    { BEGIN INSHARED; }
<INITIAL>{doublequote}   { BEGIN INSTRING; }
<INITIAL>{token}         ;
<INITIAL>.|\n            ;

%{
 /*
  * following 'shared', 'relaxed' or 'strict':
  * Determine if the keyword is followed by an identifier-like word
  * (making it UPC) or not (making it C),
  * while ignoring []s which can appear legally in both C and UPC:
  * UPC:	shared [8] int *x;
  * C:		int relaxed[8][4];
  */
%}
<INSHARED>{token}     { mark_as_upc("shared/relaxed/strict", LEXXER);}
<INSHARED>{brackets}  ;
<INSHARED>{blanks}    ;
<INSHARED>.|\n        { BEGIN INITIAL; }

%{
 /* 
  * input string literal: go until another double quote seen, but not
  * if it's preceded by a \ (unless it's a \\, i.e. a literal \)
  */
%}
<INSTRING>{literalslash} ;
<INSTRING>{escapedquote} ;
<INSTRING>{doublequote}  { BEGIN INITIAL; }
<INSTRING>.|\n           ;

%{
 /* On input, between the delimiters
  *   # line 1 "<START-UPC-ONLY>"
  * and
  *   # line 1 "<END-UPC-ONLY>"
  * we don't apply our UPC detection heuristics.
  * NOTE: These don't (yet) nest, but a "depth" counter could fix that.
  */
%}
<INUPCONLY>{endupconly}  { BEGIN INITIAL; }
<INUPCONLY>{wholeline}   ;

%{
 /* 
  * output string literal: go until another double quote seen, but not
  * if it's preceded by a \ (unless it's a \\, i.e. a literal \)
  */
%}
<OUTSTRING>{literalslash} { print_out(yytext, yyleng); }
<OUTSTRING>{escapedquote} { print_out(yytext, yyleng); }
<OUTSTRING>{doublequote}  { print_out(yytext, yyleng);
                            BEGIN OUTPUT; }
<OUTSTRING>.|\n           { print_out(yytext, yyleng); }

%{
 /* On output, between the delimiters
  *   # line 1 "<START-UPC-ONLY>"
  * and
  *   # line 1 "<END-UPC-ONLY>"
  * we swallow the text if not in UPC mode, and always swallow the delimiters.
  * NOTE: These don't (yet) nest, but a "depth" counter could fix that.
  */
%}
<OUTUPCONLY>{endupconly}  { BEGIN OUTPUT; }
<OUTUPCONLY>{wholeline}   { if (!in_c) print_out(yytext, yyleng); }

%{
 /* output scan: only care about when we enter file and
  * transforming of 'shared', 'strict' and 'relaxed'.
  */
%}
<OUTPUT>{entering_file}  { insert_pragma(yytext); }
<OUTPUT>{startupconly}   { BEGIN OUTUPCONLY; }
<OUTPUT>{doublequote}    { print_out(yytext, yyleng);
                           BEGIN OUTSTRING; }
<OUTPUT>{upc_shared}     { if (in_c) print_out("_bupc_",6);
                           print_out(yytext, yyleng); }
<OUTPUT>{c_code}         { ; /* Swallow it, leaving a blank line for counting */ }
<OUTPUT>{token}|.|\n     { print_out(yytext, yyleng); }

%%

/* 
 * we use only 1 file per run, so stop yylex() at EOF 
 */
int yywrap()
{
    return 1;
}

