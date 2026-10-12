/* xb_parse.h -- parser internals shared by the parser files and
   the command parser (pcmd.c).  Included by lex.c pexpr.c pstmt.c
   pclass.c pcmd.c.  All of these are defined in the parser files
   (pexpr.c unless noted). */
#ifndef XB_PARSE_H
#define XB_PARSE_H
#include "xb.h"
extern int perrflag, perrline;
extern char *perrmsg;
void perr(const char *msg);     /* error; tp = ntok */
int curk(void);                 /* tk[tp] */
void adv(void);
int isop(const char *s);
int isword(const char *s);
int nextisop(const char *s);
int nextisword(const char *s);
void expectop(const char *s);
void skipeol(void);
int atend(void);
int parseexpr(void);
int parseassign(void);
int parsepostfix(void);
int parseunary(void);
extern int argcount;
int parseargs(void);
int resolve(int sym);           /* K_LOCAL/K_STATIC/K_CAPV node or 0 */
/* pstmt.c */
extern int listcount;
int parselist(void);
int bareword(void);
int filearg(void);
int aliasarg(void);
void skipline(void);
int parsestmts(void);
int parsestmt(void);
void closewith(const char *specific);
int iscloser(void);
/* pcmd.c, called by the statement parser */
int parsecmd(void);
int parsescopedcmd(void);
#endif
