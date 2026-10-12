/* xb.h -- shared declarations of the xBase interpreter (C89)

   This is a C version of the interpreter in ../xbase.crexx (sources
   in ../src).  The behaviour must be the same; the cREXX sources are
   the specification.  The procedures keep their cREXX names.

   RULES FOR ALL SOURCE FILES
   - strict C89 (no //, no declarations after statements, no long
     long, no snprintf, no inline); compile with -std=c89 -pedantic
   - NO LINE LONGER THAN 71 COLUMNS (make lines checks it)
   - data model: see "VALUES" below.  There is no garbage collector;
     strings, arrays, ... are allocated and never freed (as in the
     cREXX version).  Use xmalloc/xrealloc, never plain malloc.
   - indexes of the tables are 1-based like in the cREXX version
     (element 0 is unused), except where said otherwise.
*/
#ifndef XB_H
#define XB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <limits.h>

#include "xb_consts.h"
/* T_NUM.. T_REF, E_IDENT.., E_NONE.., K_.. (nodes), C_.. (commands) */
#define T_NIL 0

/* --- VALUES */
typedef struct XStr { long n; char s[4]; } XStr; /* s[n] == 0 */
typedef struct XArr { long n, cap; struct XV *v; } XArr; /* v[0..] */

typedef struct XV {
    unsigned char t;            /* T_NIL, T_NUM, T_STR, ... T_REF */
    unsigned char w, d;         /* T_NUM: display width, decimals */
    unsigned char f;            /* T_NUM: 1 = compact whole number
                                   (made by mkint), like the cREXX
                                   'negative reference' numbers */
    union {
        double n;               /* T_NUM */
        long j;                 /* T_LOG 0/1, T_DATE julian day (0 =
                                   empty date) */
        XStr *s;                /* T_STR */
        XArr *a;                /* T_ARR */
        struct XHashT *h;       /* T_HASH */
        struct XBlk *b;         /* T_BLK */
        struct XObj *o;         /* T_OBJ */
        struct XBoxT *r;        /* T_REF: variable shared by
                                   reference (closures, @var) */
    } u;
} V;

struct XHashT { V keys, vals; };    /* two T_ARR values, same length,
                                       insertion ordered */
struct XBlk {
    int node;                   /* K_BLOCK or K_CODEBLOCK node */
    V self;                     /* Self when the block was created */
    int ncap;
    struct XBoxT **cap;         /* captured variables, 0-based */
};
struct XObj { int cls; V slots; };  /* slots: T_ARR, 1-based slots */
struct XBoxT { V v; };

extern V NILV, TRUEV, FALSEV;

/* type tests (macros on a V) */
#define isnil(v)  ((v).t == T_NIL)
#define isnum(v)  ((v).t == T_NUM)
#define isstr(v)  ((v).t == T_STR)
#define islog(v)  ((v).t == T_LOG)
#define isdate(v) ((v).t == T_DATE)
#define isarr(v)  ((v).t == T_ARR)
#define ishash(v) ((v).t == T_HASH)
#define isblk(v)  ((v).t == T_BLK)
#define isobj(v)  ((v).t == T_OBJ)
#define isref(v)  ((v).t == T_REF)
#define truthy(v) ((v).t == T_LOG && (v).u.j != 0)
/* accessors; only valid for the right type */
#define SS(v)  ((v).u.s->s)     /* char * of a string, NUL ended */
#define SL(v)  ((v).u.s->n)     /* length of a string */
#define DJ(v)  ((v).u.j)        /* julian day number of a date */

/* constructors */
V mkint(long n);
V mkwhole(double x);            /* whole number kept in the double */
V mknum(double x, int w, int d);/* width w, d decimals; a whole x
                                   with d = 0 and |x| < 9e15 becomes
                                   width 10 like mkint */
V mkfloat(double x);            /* mknum(x, 10, 0) */
V mknumd(double x, int d);      /* mknum(x, 10, d) */
V mkstr(const char *z);         /* copy of a NUL ended string */
V mkstrn(const char *p, long n);/* copy of n bytes (may hold NULs) */
V mklog(int b);
V mkdate(long jdn);
V mkarray(long n);              /* n elements, all NIL */
V mkhash(void);
/* numbers */
double numval(V v);             /* 0.0 if not a number */
int numdec(V v);
int numwid(V v);
long toint(double x);           /* truncate towards zero */
long intval(V v);               /* toint(numval(v)) */
int isint(V v);                 /* compact whole number (f == 1) */
int vsame(V a, V b);            /* same value and identity */
/* variables shared by reference */
V deref(V v);                   /* T_REF -> its value, else v */
V mkboxed(V v);                 /* new T_REF holding v */
void boxset(V ref, V val);      /* store through a T_REF */

/* arrays (1-based) */
long arrlen(V a);
V arrget(V a, long i);
void arrset(V a, long i, V v);
void arrpush(V a, V v);
void arrresize(V a, long n);    /* new elements are NIL */
void arrgrow(V a, long cap);
/* hashes: keys compare by value, strings exactly; whole numbers
   compare equal to numbers of the same value */
long hashlen(V h);
V hkey(V h, long i);
V hval(V h, long i);
long hfind(V h, V key);         /* position, 0 if missing */
void hset(V h, V key, V v);
void hdel(V h, long pos);
int keyeq(V a, V b);

/* string builder for the library */
typedef struct { char *p; long n, cap; } Sb;
void sb_init(Sb *b);
void sb_addn(Sb *b, const char *p, long n);
void sb_adds(Sb *b, const char *z);
void sb_addc(Sb *b, int c);
V sb_val(Sb *b);                /* mkstrn of the content */
char *sb_str(Sb *b);            /* NUL ended content (do not free) */

/* memory: abort with a message when out of memory */
void *xmalloc(long n);
void *xrealloc(void *p, long n);
char *xstrdup(const char *z);
char *xstrndup(const char *p, long n);
char *xfmt(const char *fmt, ...);   /* sprintf into new memory; the
                                       result must be < 2000 bytes */
char *xupper(const char *z);        /* new upper case copy */
char *xlower(const char *z);
char *xstrip(const char *z);        /* blanks removed both sides */
void growi(int **p, long *cap, long need); /* grow an int table */
void grows(char ***p, long *cap, long need);
void growv(V **p, long *cap, long need);

/* ------------------------------------------------- symbol table */
extern long nsym;
extern char **snm;      /* name (upper case) */
extern int *shn;        /* hash chain */
extern int sbk[2049];   /* hash buckets */
extern int *sfn;        /* user function id */
extern int *sbi;        /* builtin id */
extern V *smv;          /* memvar value (may be T_REF) */
extern int *smvd;       /* memvar visible: 0 no, 1 yes */
extern int *sdef;       /* #define index */
extern int *scls;       /* symbol -> class id */
extern int *smacro;     /* compiled macro expression node */
int intern(const char *name);   /* name must be upper case */

/* builtin functions ("prims") */
typedef V (*PrimFn)(int base, int n);
extern int *pmin, *pmax;        /* argument counts per prim id */
int addprim(const char *names, PrimFn f, int mn, int mx);
        /* names: "NAME" or "NAME|ALIAS|ALIAS" (upper case) */
V callprim(int id, int base, int n);
void initprims(void);           /* calls lib1_init() ... */
void lib1_init(void);
void lib2_init(void);
void lib3_init(void);
void lib4_init(void);
void dbfprims_init(void);

/* -------------------------------------------------------- tokens */
extern long ntok, tp;
extern int *tk;         /* E_IDENT, E_NUM, E_STR, E_OP, E_EOL, ... */
extern char **ts;       /* text (identifiers upper case) */
extern int *tl;         /* line */
extern int *tsp;        /* 1 = preceded by blank */
extern int *tv;         /* identifier symbol id */
extern char **tor;      /* original text */
extern int inblk;
extern int curline;
extern char *srcname;
extern char *srcdir;
extern char *incpath;

/* ---------------------------------------------------------- nodes */
extern long nn;
extern int *nt, *na, *nb, *nc, *nd, *ne, *nf, *nx, *nl;
extern char **ns;
extern V *nk;           /* value of a K_CONST node (cREXX: na[n]) */
int mknode(int kind, int a, int b, int c);  /* line = curline */
int constnode(V v);             /* K_CONST node with value v */

/* ------------------------------------------- functions, classes */
extern long nfn;
extern char **fnname;
extern int *fnpar, *fnslots, *fnbody, *fnclass;
int newfn(const char *name);
extern int mainfn, firstfn;
extern int *initfns, *exitfns;
extern long ninit, nexitp;
void add_initfn(int f);
void add_exitfn(int f);

extern V *sv;           /* static variables 1..nstatic */
extern long nstatic;
int newstatic(void);            /* new static slot, NIL */
extern int *svinit, *svslot;    /* static initialisers */
extern long nsvinit;
void add_svinit(int node, int slot);

extern char **clname;
extern int *clpar, *clnslot;
extern long ncls;
extern int *cinitcls, *cinitslot, *cinitnode;
extern long ncinit;
void add_cinit(int cls, int slot, int node);
int newclass(const char *name, int par);
int adddata(int cls, int sym);  /* instance variable slot */
/* pair map: kind 1 method (fn id), 2 data slot, 3 class data
   (static index), 4 declared method; keys (kind, class, symbol) */
int pm_get(int kind, int a, int b);
void pm_put(int kind, int a, int b, int v);
int findmethod(int cls, int sym);
int findslot(int cls, int sym);
int findcdata(int cls, int sym);
int isderived(int cls, int anc);
extern int errcls;
V newobject(int cl);

/* ------------------------------------------------------ frames */
extern V *fv;           /* value stack: locals/arguments of calls */
extern long fvcap;
extern long ftop, fbase, fpc;
void fvgrow(void);
#define SETFTOP(n) do { ftop = (n); \
    if (ftop + 64 > fvcap) fvgrow(); } while (0)
extern V curself;
extern struct XBlk *curblk;     /* running block (captures) */
extern int calldepth;
extern int *pvs; extern V *pvv; extern int *pvd; extern long pvtop;
extern int *pcln; extern char **pcname; extern long pctop;
extern int curmcls;

/* ------------------------------------------------------ control */
extern int ctl;         /* E_NONE, E_RET, E_EXIT, E_LOOP, E_BRK */
extern V retv, brkval, errblk;
extern int errdepth, seqdepth, exitcode;

/* -------------------------------------------------- settings */
extern int setdec, setexact, setfixed, setcentury;
extern char setdatefmt[40];
extern int setdeleted, setsoft, setepoch, setconsole, setprint;
extern int setbell, setescape, setunique;
extern char *setdefault;
extern int outcol, outrow, scrrow, scrcol, outstarted;
extern long seed;
extern char **argvs; extern int nargvs;
extern char *progname, *progdir;
extern int curarea;
extern int curl;
extern char *altfile;

/* --------------------------------------------------- output (core) */
void emit(const char *s);
void emitn(const char *s, long n);
void emitnl(void);
void flushout(void);
void errout(const char *s);     /* stderr + newline, flushes stdout */

/* --- numbers, dates, ops (ops.c) */
extern int gy, gm, gd;
void jdn2ymd(long j);           /* sets gy gm gd */
long ymd2jdn(int y, int m, int d);
char *pad2(int n);
char *pad4(int n);
char *datefmt(long j, const char *fmt);
char *datestr(long j);          /* with SET DATE format */
char *dtos(long j);
long parsedate(const char *s, const char *fmt);
V argerr(const char *op);       /* raises 'Argument error', 1066 */
char *padleft(const char *s, int w);
char *fmtfloat(double x, int d);/* x with d decimals, no -0 */
char *numstr(V v);              /* display string of a number */
char *numstrw(V v, int w, int d);   /* Str(): w/d -1 = not given */
char *ntos(V v);                /* trimmed number */
V numadd(V a, V b);
V numsub(V a, V b);
V nummul(V a, V b);
V numdiv(V a, V b);
V nummod(V a, V b);
V numpow(V a, V b);
V numneg(V a);
double fpow(double x, double y);
int strcmp_x(const char *a, long an, const char *b, long bn);
        /* byte-wise, shorter string is lower: -1, 0, 1 */
int cmpv(V a, V b);             /* -1 0 1, 2 = not comparable */
int equalv(V a, V b);           /* the = operator, 0/1 */
int exactv(V a, V b);           /* the == operator, 0/1 */
V binadd(V a, V b);
V binsub(V a, V b);
V binop(int kind, V a, V b);    /* K_MUL K_DIV K_MOD K_POW */
V relop(int kind, V a, V b);    /* K_EQ .. K_GE -> logical */
char typech(V v);               /* 'U' 'N' 'C' 'D' 'L' 'A' 'H' ... */
char *valstr(V v);              /* for ? and ?? */
char *vtostr(V v);              /* conversions that never raise */
double lsqrt(double x);
double lsin(double x);
double lcos(double x);
double lln(double x);
double lexp(double x);

/* ------------------------------------- evaluator (eval.c, exec.c) */
V evalnode(int n);
V mkblock(int n);
V evcall(int n);
void assignto(int n, V val);
V mkref(int n);
int evalargs(int a, long base, int mx);
V invoke(int f, long base, int cnt, V obj, int site);
V callblock(V blk, long base, int cnt);
V evalblock0(V blk);
V evalblock1(V blk, V a);
V evalblock2(V blk, V a, V b);
V evalblock3(V blk, V a, V b, V c);
V callsym(int sym, V arr);      /* arr: T_ARR of arguments or NIL */
V getident(int sym);
V getmemvar(int sym);
void setmemvar(int sym, V val);
int resolvealias(int an);
void pushprivate(int sym);
void pushcall(const char *name, int line);
void execstmts(int n);
void exec(int s);
int condval(int n);
V rterr(const char *sub, int code, const char *desc, const char *op);
V mkerror(const char *sub, int code, const char *desc,
          const char *op);
char *errmessage(V obj);
void fatalerror(V obj);
void initerrclass(void);
V macroeval(const char *s);
void doquit(int code);
void shutdown_all(void);        /* the cREXX "shutdown" */
void initall(void);
void runprogram(void);
void runfn(int f, int withargs);

/* ---------------------------------------- front end (parser) */
int lexfile(const char *fname);     /* 0 ok, 1 cannot open */
void lexchunk(const char *ln, int depth);
void addtok(int kind, const char *text, int sp);
void definemacro(const char *name, const char *par,
                 const char *val);
int fileexists(const char *fname);  /* non-empty file exists */
char *dirof(const char *f);
int parseprogram(void);
extern int perrflag, perrline;
extern char *perrmsg;
int parseexpr(void);
int compileexpr(const char *s);     /* macro: node or 0 */
int isidstart(int c);
int isidchar(int c);
int isdigitc(int c);

/* ------------------------------ commands (pcmd.c, cmd.c) */
int parsecmd(void);                 /* in parser: command statement */
int parsescopedcmd(void);
void execcmd(int s);
void execscoped(int s);
void execsayget(int s);
void exectext(int s);

/* ------------------- work areas, tables, indexes (dbf.c) */
/* All these tables are DEFINED in dbf.c.  Area numbers are 1..MAXWA,
   fields 1..MAXFLD; field tables are indexed (a-1)*MAXFLD+i.  The
   other tables (recs, ix_*) are sized by dbf.c; the dbf.c author
   documents their indexing at the top of dbf.c. */
#define MAXWA 40
#define MAXFLD 256
extern int *wa_used;
extern char **wa_alias, **wa_file;
extern int *wa_nrec, *wa_recno, *wa_eof, *wa_bof, *wa_found;
extern int *wa_nfld, *wa_hdr, *wa_reclen, *wa_dirty, *wa_order;
extern int *wa_ipos, *wa_filter;
extern char **wa_filtertxt;
extern int *wa_locfor, *wa_locwhile, *wa_ro, *wa_nix;
extern int *wa_relchild, *wa_relkey, *wa_nrel;
extern char **wa_memo;
extern int *wa_hasmemo;
extern char **wa_blank;
extern int *fsym;
extern char **fnam, **ftyp;
extern int *flen, *fdec, *foff;
extern char **recs;             /* one record image per record */
extern int *ix_area, *ix_node, *ix_for, *ix_desc, *ix_uniq;
extern int *ix_start, *ix_len, *ix_cap, *ix_dirty;
extern char **ix_text, **ix_fortext, **ix_file, **ix_name;
extern int nix;
extern V *ixk;                  /* index keys */
extern int *ixr;                /* record numbers */
extern int nixpool;
extern int *waix;
void initareas(void);
int ridx(int a, int r);
int fidx(int a, int f);
int areaok(int a);
int findfreearea(void);
int aliasarea(const char *upperalias);  /* 0 if none */
int fieldfind(int a, int sym);          /* field no or 0 */
V fieldval(int a, int f);
void fieldput(int a, int f, V v);
char *dbfstem(const char *f);
char *dbfname(const char *f);
int openarea(int a, const char *fname, const char *alias, int ro);
void flushdbf(int a);
void closearea(int a);
void closeallareas(void);
int createdbf(const char *fname, V stru);
void dbgotop(int a);
void dbgobottom(int a);
void dbgoto(int a, int r);
void dbskip(int a, int n);
void dbappend(int a);
void dbdelete(int a, int flag);
int isdeleted(int a);
void dbpack(int a);
void dbzap(int a);
int createindex(int a, int keynode, const char *fname, int fornode,
                int fl, const char *keytext, const char *fortext);
void flushindexes(int a);
void reindexall(int a);
int openindex(int a, const char *fname);
int dbseek(int a, V v, int soft);
void relsync(int a);

/* ---------------------------- library helpers (lib*.c, io.c) */
char *sarg_z(V v);              /* string or "" */
long iarg(V v, long dflt);
extern int ineof;
char *readcon(void);            /* a line of stdin; ineof set at end */
int readkey(void);
char *xtransform(V v, const char *pic);
void setdate(const char *name);
void gotopos(int r, int c);
void closeallfiles(void);
V valnum(const char *s);
char *bytesof(const char *fname, long *len);  /* whole file */
int putbytes(const char *fname, const char *p, long n);
        /* 0 ok, else error */
V cloneval(V v);
char *vtoexp(V v);
V mkmulti(int base, int lvl, int tot);
void msort_arr(V arr, long first, long n, V blk);

#endif
