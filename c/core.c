/* core.c -- values, memory, symbols, nodes, tables, output.
   C port of src/10_base.crexx and the globals of 05_globals. */
#include <stdarg.h>
#include "xb.h"

V NILV = { T_NIL, 0, 0, 0, { 0.0 } };
V TRUEV = { T_LOG, 0, 0, 0, { 0.0 } };
V FALSEV = { T_LOG, 0, 0, 0, { 0.0 } };

/* ---------------------------------------------------------- memory */
void *xmalloc(long n)
{
    void *p;
    if (n < 1) n = 1;
    p = malloc((size_t)n);
    if (p == NULL) {
        fputs("xbase: out of memory\n", stderr);
        exit(3);
    }
    return p;
}

void *xrealloc(void *p, long n)
{
    void *q;
    if (n < 1) n = 1;
    q = p ? realloc(p, (size_t)n) : malloc((size_t)n);
    if (q == NULL) {
        fputs("xbase: out of memory\n", stderr);
        exit(3);
    }
    return q;
}

char *xstrdup(const char *z)
{
    return xstrndup(z, (long)strlen(z));
}

char *xstrndup(const char *p, long n)
{
    char *r = (char *)xmalloc(n + 1);
    if (n > 0) memcpy(r, p, (size_t)n);
    r[n] = 0;
    return r;
}

char *xfmt(const char *fmt, ...)
{
    static char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    return xstrdup(buf);
}

char *xupper(const char *z)
{
    char *r = xstrdup(z);
    char *p;
    for (p = r; *p; p++) *p = (char)toupper((unsigned char)*p);
    return r;
}

char *xlower(const char *z)
{
    char *r = xstrdup(z);
    char *p;
    for (p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}

char *xstrip(const char *z)
{
    long n = (long)strlen(z);
    while (n > 0 && z[n - 1] == ' ') n--;
    while (n > 0 && *z == ' ') {
        z++;
        n--;
    }
    return xstrndup(z, n);
}

void growi(int **p, long *cap, long need)
{
    long nc;
    if (need <= *cap) return;
    nc = *cap < 64 ? 64 : *cap * 2;
    while (nc < need) nc *= 2;
    *p = (int *)xrealloc(*p, nc * (long)sizeof(int));
    memset(*p + *cap, 0, (size_t)((nc - *cap) * (long)sizeof(int)));
    *cap = nc;
}

void grows(char ***p, long *cap, long need)
{
    long nc;
    if (need <= *cap) return;
    nc = *cap < 64 ? 64 : *cap * 2;
    while (nc < need) nc *= 2;
    *p = (char **)xrealloc(*p, nc * (long)sizeof(char *));
    memset(*p + *cap, 0,
           (size_t)((nc - *cap) * (long)sizeof(char *)));
    *cap = nc;
}

void growv(V **p, long *cap, long need)
{
    long nc, i;
    if (need <= *cap) return;
    nc = *cap < 64 ? 64 : *cap * 2;
    while (nc < need) nc *= 2;
    *p = (V *)xrealloc(*p, nc * (long)sizeof(V));
    for (i = *cap; i < nc; i++) (*p)[i] = NILV;
    *cap = nc;
}

/* ---------------------------------------------------- constructors */
V mkint(long n)
{
    V v;
    v.t = T_NUM;
    v.w = 10;
    v.d = 0;
    v.f = 1;
    v.u.n = (double)n;
    return v;
}

/* a whole number from a sum, difference or product: kept exact in the
   double even where long has 32 bits (VM/370 CMS) */
V mkwhole(double x)
{
    V v = mkint(0);
    if (x >= 9.2e18 || x <= -9.2e18) return mknum(x, 20, 0);
    v.u.n = x;
    return v;
}

V mknum(double x, int w, int d)
{
    V v;
    /* a whole number fits an int value only within long (32 bits on
       VM/370 CMS, where 1e16 would come out as 2147483647) */
    if (d == 0 && x < 9.0e15 && x > -9.0e15
        && x < (double)LONG_MAX && x > -(double)LONG_MAX) {
        double q = (double)(long)x;
        if (q == x) return mkint((long)x);
    }
    v.t = T_NUM;
    v.w = (unsigned char)(w > 255 ? 255 : (w < 0 ? 0 : w));
    v.d = (unsigned char)(d > 255 ? 255 : (d < 0 ? 0 : d));
    v.f = 0;
    v.u.n = x;
    return v;
}

V mkfloat(double x)
{
    return mknum(x, 10, 0);
}

V mknumd(double x, int d)
{
    return mknum(x, 10, d);
}

V mkstrn(const char *p, long n)
{
    V v;
    XStr *s;
    if (n < 0) n = 0;
    s = (XStr *)xmalloc((long)sizeof(XStr) + n);
    s->n = n;
    if (n > 0) memcpy(s->s, p, (size_t)n);
    s->s[n] = 0;
    v.t = T_STR;
    v.w = v.d = v.f = 0;
    v.u.s = s;
    return v;
}

V mkstr(const char *z)
{
    return mkstrn(z, (long)strlen(z));
}

V mklog(int b)
{
    V v;
    v.t = T_LOG;
    v.w = v.d = v.f = 0;
    v.u.j = b ? 1 : 0;
    return v;
}

V mkdate(long jdn)
{
    V v;
    v.t = T_DATE;
    v.w = v.d = v.f = 0;
    v.u.j = jdn;
    return v;
}

V mkarray(long n)
{
    V v;
    XArr *a;
    long i, cap;
    if (n < 0) n = 0;
    cap = n < 4 ? 4 : n;
    a = (XArr *)xmalloc((long)sizeof(XArr));
    a->n = n;
    a->cap = cap;
    a->v = (V *)xmalloc(cap * (long)sizeof(V));
    for (i = 0; i < cap; i++) a->v[i] = NILV;
    v.t = T_ARR;
    v.w = v.d = v.f = 0;
    v.u.a = a;
    return v;
}

V mkhash(void)
{
    V v;
    struct XHashT *h;
    h = (struct XHashT *)xmalloc((long)sizeof(struct XHashT));
    h->keys = mkarray(0);
    h->vals = mkarray(0);
    v.t = T_HASH;
    v.w = v.d = v.f = 0;
    v.u.h = h;
    return v;
}

double numval(V v)
{
    if (v.t == T_NUM) return v.u.n;
    return 0.0;
}

int numdec(V v)
{
    if (v.t != T_NUM) return 0;
    return v.d;
}

int numwid(V v)
{
    if (v.t != T_NUM) return 10;
    return v.w;
}

long toint(double x)
{
    if (x >= 2147483647.0 && sizeof(long) <= 4) return 2147483647L;
    if (x <= -2147483647.0 && sizeof(long) <= 4) return -2147483647L;
    if (x >= 9.2e18) return LONG_MAX;
    if (x <= -9.2e18) return -LONG_MAX;
    return (long)x;
}

long intval(V v)
{
    return toint(numval(v));
}

int isint(V v)
{
    return v.t == T_NUM && v.f != 0;
}

int vsame(V a, V b)
{
    if (a.t != b.t) return 0;
    switch (a.t) {
    case T_NIL: return 1;
    case T_NUM:
        return a.u.n == b.u.n && a.w == b.w && a.d == b.d;
    case T_LOG: case T_DATE: return a.u.j == b.u.j;
    case T_STR: return a.u.s == b.u.s;
    case T_ARR: return a.u.a == b.u.a;
    case T_HASH: return a.u.h == b.u.h;
    case T_BLK: return a.u.b == b.u.b;
    case T_OBJ: return a.u.o == b.u.o;
    case T_REF: return a.u.r == b.u.r;
    }
    return 0;
}

V deref(V v)
{
    if (v.t == T_REF) return v.u.r->v;
    return v;
}

V mkboxed(V v)
{
    V r;
    struct XBoxT *b = (struct XBoxT *)xmalloc(
        (long)sizeof(struct XBoxT));
    b->v = v;
    r.t = T_REF;
    r.w = r.d = r.f = 0;
    r.u.r = b;
    return r;
}

void boxset(V ref, V val)
{
    ref.u.r->v = val;
}

/* ---------------------------------------------------------- arrays */
long arrlen(V a)
{
    return a.u.a->n;
}

V arrget(V a, long i)
{
    if (i < 1 || i > a.u.a->n) return NILV;
    return a.u.a->v[i - 1];
}

void arrset(V a, long i, V v)
{
    if (i < 1 || i > a.u.a->n) return;
    a.u.a->v[i - 1] = v;
}

void arrgrow(V a, long cap)
{
    XArr *r = a.u.a;
    long i;
    if (cap <= r->cap) return;
    r->v = (V *)xrealloc(r->v, cap * (long)sizeof(V));
    for (i = r->cap; i < cap; i++) r->v[i] = NILV;
    r->cap = cap;
}

void arrpush(V a, V v)
{
    XArr *r = a.u.a;
    if (r->n >= r->cap) arrgrow(a, r->n * 2 + 4);
    r->v[r->n] = v;
    r->n++;
}

void arrresize(V a, long n)
{
    XArr *r = a.u.a;
    long i;
    if (n < 0) n = 0;
    if (n > r->cap) arrgrow(a, n + 4);
    for (i = r->n; i < n; i++) r->v[i] = NILV;
    r->n = n;
}

/* ---------------------------------------------------------- hashes */
long hashlen(V h)
{
    return arrlen(h.u.h->keys);
}

V hkey(V h, long i)
{
    return arrget(h.u.h->keys, i);
}

V hval(V h, long i)
{
    return arrget(h.u.h->vals, i);
}

int keyeq(V a, V b)
{
    if (a.t != b.t) return 0;
    switch (a.t) {
    case T_NIL: return 1;
    case T_NUM: return a.u.n == b.u.n;
    case T_LOG: case T_DATE: return a.u.j == b.u.j;
    case T_STR:
        return a.u.s->n == b.u.s->n &&
               memcmp(a.u.s->s, b.u.s->s, (size_t)a.u.s->n) == 0;
    case T_ARR: return a.u.a == b.u.a;
    case T_HASH: return a.u.h == b.u.h;
    case T_BLK: return a.u.b == b.u.b;
    case T_OBJ: return a.u.o == b.u.o;
    }
    return 0;
}

long hfind(V h, V key)
{
    XArr *ks = h.u.h->keys.u.a;
    long i;
    for (i = 0; i < ks->n; i++)
        if (keyeq(ks->v[i], key)) return i + 1;
    return 0;
}

void hset(V h, V key, V v)
{
    long p = hfind(h, key);
    if (p > 0) {
        arrset(h.u.h->vals, p, v);
        return;
    }
    arrpush(h.u.h->keys, key);
    arrpush(h.u.h->vals, v);
}

void hdel(V h, long p)
{
    V ks = h.u.h->keys, vs = h.u.h->vals;
    long n = arrlen(ks), i;
    if (p < 1 || p > n) return;
    for (i = p; i < n; i++) {
        arrset(ks, i, arrget(ks, i + 1));
        arrset(vs, i, arrget(vs, i + 1));
    }
    ks.u.a->n = n - 1;
    vs.u.a->n = n - 1;
}

/* -------------------------------------------------- string builder */
void sb_init(Sb *b)
{
    b->p = NULL;
    b->n = 0;
    b->cap = 0;
}

void sb_addn(Sb *b, const char *p, long n)
{
    if (n <= 0) return;
    if (b->n + n + 1 > b->cap) {
        long nc = b->cap < 64 ? 64 : b->cap * 2;
        while (nc < b->n + n + 1) nc *= 2;
        b->p = (char *)xrealloc(b->p, nc);
        b->cap = nc;
    }
    memcpy(b->p + b->n, p, (size_t)n);
    b->n += n;
    b->p[b->n] = 0;
}

void sb_adds(Sb *b, const char *z)
{
    sb_addn(b, z, (long)strlen(z));
}

void sb_addc(Sb *b, int c)
{
    char ch = (char)c;
    sb_addn(b, &ch, 1);
}

V sb_val(Sb *b)
{
    if (b->n == 0) return mkstrn("", 0);
    return mkstrn(b->p, b->n);
}

char *sb_str(Sb *b)
{
    if (b->p == NULL) return "";
    return b->p;
}

/* --------------------------------------------------------- symbols */
long nsym = 0;
char **snm = NULL;
int *shn = NULL;
int sbk[2049];
int *sfn = NULL;
int *sbi = NULL;
V *smv = NULL;
int *smvd = NULL;
int *sdef = NULL;
int *scls = NULL;
int *smacro = NULL;
static long symcap = 0, smvcap = 0;

static int symhash(const char *s)
{
    long h = 7;
    for (; *s; s++) h = (h * 31 + (unsigned char)*s) % 100003L;
    return (int)(h % 2048) + 1;
}

int intern(const char *name)
{
    int b = symhash(name);
    int s = sbk[b];
    long need;
    while (s > 0) {
        if (strcmp(snm[s], name) == 0) return s;
        s = shn[s];
    }
    nsym++;
    need = nsym + 1;
    {
        long c0 = symcap;
        grows(&snm, &c0, need);
        c0 = symcap;
        growi(&shn, &c0, need);
        c0 = symcap;
        growi(&sfn, &c0, need);
        c0 = symcap;
        growi(&sbi, &c0, need);
        c0 = symcap;
        growi(&sdef, &c0, need);
        c0 = symcap;
        growi(&scls, &c0, need);
        c0 = symcap;
        growi(&smacro, &c0, need);
        c0 = symcap;
        growi(&smvd, &c0, need);
        growv(&smv, &smvcap, need);
        symcap = c0;
    }
    snm[nsym] = xstrdup(name);
    shn[nsym] = sbk[b];
    sbk[b] = (int)nsym;
    sfn[nsym] = 0;
    sbi[nsym] = 0;
    smv[nsym] = NILV;
    smvd[nsym] = 0;
    sdef[nsym] = 0;
    scls[nsym] = 0;
    smacro[nsym] = 0;
    return (int)nsym;
}

/* --------------------------------------------------- builtin table */
typedef struct { PrimFn fn; } PrimEnt;
static PrimEnt *primtab = NULL;
static long nprim = 0, primcap = 0;
int *pmin = NULL, *pmax = NULL;
static long pmincap = 0, pmaxcap = 0;

int addprim(const char *names, PrimFn f, int mn, int mx)
{
    char *copy = xstrdup(names);
    char *p = copy, *q;
    int id;
    if (nprim + 2 > primcap) {
        long nc = primcap < 64 ? 64 : primcap * 2;
        primtab = (PrimEnt *)xrealloc(primtab,
                                      nc * (long)sizeof(PrimEnt));
        primcap = nc;
    }
    nprim++;
    id = (int)nprim;
    primtab[id].fn = f;
    growi(&pmin, &pmincap, nprim + 1);
    growi(&pmax, &pmaxcap, nprim + 1);
    pmin[id] = mn;
    pmax[id] = mx;
    while (p && *p) {
        q = strchr(p, '|');
        if (q) *q = 0;
        { int sy_ = intern(p); sbi[sy_] = id; }
        p = q ? q + 1 : NULL;
    }
    return id;
}

V callprim(int id, int base, int n)
{
    return primtab[id].fn(base, n);
}

void initprims(void)
{
    lib1_init();
    lib2_init();
    lib3_init();
    lib4_init();
    dbfprims_init();
}

/* ----------------------------------------------------------- nodes */
long nn = 0;
int *nt = NULL, *na = NULL, *nb = NULL, *nc = NULL, *nd = NULL;
int *ne = NULL, *nf = NULL, *nx = NULL, *nl = NULL;
char **ns = NULL;
V *nk = NULL;
static long ncap = 0, nscap = 0, nkcap = 0;

int mknode(int kind, int a, int b, int c)
{
    long need, c0;
    nn++;
    need = nn + 1;
    c0 = ncap;
    growi(&nt, &c0, need);
    c0 = ncap;
    growi(&na, &c0, need);
    c0 = ncap;
    growi(&nb, &c0, need);
    c0 = ncap;
    growi(&nc, &c0, need);
    c0 = ncap;
    growi(&nd, &c0, need);
    c0 = ncap;
    growi(&ne, &c0, need);
    c0 = ncap;
    growi(&nf, &c0, need);
    c0 = ncap;
    growi(&nx, &c0, need);
    c0 = ncap;
    growi(&nl, &c0, need);
    ncap = c0;
    grows(&ns, &nscap, need);
    growv(&nk, &nkcap, need);
    nt[nn] = kind;
    na[nn] = a;
    nb[nn] = b;
    nc[nn] = c;
    nd[nn] = 0;
    ne[nn] = 0;
    nf[nn] = 0;
    nx[nn] = 0;
    nl[nn] = curline;
    ns[nn] = "";
    nk[nn] = NILV;
    return (int)nn;
}

int constnode(V v)
{
    int n = mknode(K_CONST, 0, 0, 0);
    nk[n] = v;
    return n;
}

/* ------------------------------------- functions, statics, classes */
long nfn = 0;
char **fnname = NULL;
int *fnpar = NULL, *fnslots = NULL, *fnbody = NULL, *fnclass = NULL;
static long fncap = 0, fnnamecap = 0;
int mainfn = 0, firstfn = 0;
int *initfns = NULL, *exitfns = NULL;
long ninit = 0, nexitp = 0;
static long initcap = 0, exitcap = 0;

int newfn(const char *name)
{
    long need, c0;
    nfn++;
    need = nfn + 1;
    c0 = fncap;
    growi(&fnpar, &c0, need);
    c0 = fncap;
    growi(&fnslots, &c0, need);
    c0 = fncap;
    growi(&fnbody, &c0, need);
    c0 = fncap;
    growi(&fnclass, &c0, need);
    fncap = c0;
    grows(&fnname, &fnnamecap, need);
    fnname[nfn] = xstrdup(name);
    fnpar[nfn] = 0;
    fnslots[nfn] = 0;
    fnbody[nfn] = 0;
    fnclass[nfn] = 0;
    return (int)nfn;
}

void add_initfn(int f)
{
    ninit++;
    growi(&initfns, &initcap, ninit + 1);
    initfns[ninit] = f;
}

void add_exitfn(int f)
{
    nexitp++;
    growi(&exitfns, &exitcap, nexitp + 1);
    exitfns[nexitp] = f;
}

V *sv = NULL;
long nstatic = 0;
static long svcap = 0;
int *svinit = NULL, *svslot = NULL;
long nsvinit = 0;
static long svinitcap = 0, svslotcap = 0;

int newstatic(void)
{
    nstatic++;
    growv(&sv, &svcap, nstatic + 1);
    sv[nstatic] = NILV;
    return (int)nstatic;
}

void add_svinit(int node, int slot)
{
    nsvinit++;
    growi(&svinit, &svinitcap, nsvinit + 1);
    growi(&svslot, &svslotcap, nsvinit + 1);
    svinit[nsvinit] = node;
    svslot[nsvinit] = slot;
}

char **clname = NULL;
int *clpar = NULL, *clnslot = NULL;
long ncls = 0;
static long clcap = 0, clnamecap = 0;
int *cinitcls = NULL, *cinitslot = NULL, *cinitnode = NULL;
long ncinit = 0;
static long cinitcap = 0;
int errcls = 0;

/* pair map: (kind, a, b) -> value */
typedef struct PmEnt { int k, a, b, v; struct PmEnt *next; } PmEnt;
static PmEnt *pmbuck[4100];

static int pmhash(int kind, int a, int b)
{
    long h = (long)kind * 7919L + (long)a * 131L + (long)b * 31L;
    return (int)(h % 4093L);
}

int pm_get(int kind, int a, int b)
{
    PmEnt *e = pmbuck[pmhash(kind, a, b)];
    for (; e; e = e->next)
        if (e->k == kind && e->a == a && e->b == b) return e->v;
    return 0;
}

void pm_put(int kind, int a, int b, int v)
{
    int h = pmhash(kind, a, b);
    PmEnt *e = pmbuck[h];
    for (; e; e = e->next)
        if (e->k == kind && e->a == a && e->b == b) {
            e->v = v;
            return;
        }
    e = (PmEnt *)xmalloc((long)sizeof(PmEnt));
    e->k = kind;
    e->a = a;
    e->b = b;
    e->v = v;
    e->next = pmbuck[h];
    pmbuck[h] = e;
}

int newclass(const char *name, int par)
{
    long need, c0;
    ncls++;
    need = ncls + 1;
    c0 = clcap;
    growi(&clpar, &c0, need);
    c0 = clcap;
    growi(&clnslot, &c0, need);
    clcap = c0;
    grows(&clname, &clnamecap, need);
    clname[ncls] = xstrdup(name);
    clpar[ncls] = par;
    clnslot[ncls] = 0;
    if (par > 0) clnslot[ncls] = clnslot[par];
    { int sy_ = intern(name); scls[sy_] = (int)ncls; }
    return (int)ncls;
}

int adddata(int cls, int sym)
{
    int s = pm_get(2, cls, sym);
    if (s > 0) return s;
    clnslot[cls]++;
    pm_put(2, cls, sym, clnslot[cls]);
    return clnslot[cls];
}

void add_cinit(int cls, int slot, int node)
{
    static long cap1 = 0, cap2 = 0;
    ncinit++;
    growi(&cinitcls, &cinitcap, ncinit + 1);
    growi(&cinitslot, &cap1, ncinit + 1);
    growi(&cinitnode, &cap2, ncinit + 1);
    cinitcls[ncinit] = cls;
    cinitslot[ncinit] = slot;
    cinitnode[ncinit] = node;
}

/* ---------------------------------------------------------- frames */
V *fv = NULL;
long fvcap = 0;
long ftop = 0, fbase = 0, fpc = 0;
V curself = { T_NIL, 0, 0, 0, { 0.0 } };
struct XBlk *curblk = NULL;
int calldepth = 0;
int *pvs = NULL;
V *pvv = NULL;
int *pvd = NULL;
long pvtop = 0;
static long pvcap = 0, pvvcap = 0, pvdcap = 0;
int *pcln = NULL;
char **pcname = NULL;
long pctop = 0;
static long pclcap = 0, pcncap = 0;
int curmcls = 0;

void fvgrow(void)
{
    long nc = fvcap < 4096 ? 4096 : fvcap * 2;
    long i;
    while (nc < ftop + 64) nc *= 2;
    fv = (V *)xrealloc(fv, nc * (long)sizeof(V));
    for (i = fvcap; i < nc; i++) fv[i] = NILV;
    fvcap = nc;
}

void pushprivate(int sym)
{
    pvtop++;
    growi(&pvs, &pvcap, pvtop + 1);
    growv(&pvv, &pvvcap, pvtop + 1);
    growi(&pvd, &pvdcap, pvtop + 1);
    pvs[pvtop] = sym;
    pvv[pvtop] = smv[sym];
    pvd[pvtop] = smvd[sym];
}

void pushcall(const char *name, int line)
{
    pctop++;
    growi(&pcln, &pclcap, pctop + 1);
    grows(&pcname, &pcncap, pctop + 1);
    pcname[pctop] = (char *)name;
    pcln[pctop] = line;
}

/* --------------------------------------------------------- control */
int ctl = 0;
V retv = { T_NIL, 0, 0, 0, { 0.0 } };
V brkval = { T_NIL, 0, 0, 0, { 0.0 } };
V errblk = { T_NIL, 0, 0, 0, { 0.0 } };
int errdepth = 0, seqdepth = 0, exitcode = 0;

/* -------------------------------------------------------- settings */
int setdec = 2, setexact = 0, setfixed = 0, setcentury = 0;
char setdatefmt[40] = "mm/dd/yy";
int setdeleted = 0, setsoft = 0, setepoch = 1900, setconsole = 1;
int setprint = 0, setbell = 0, setescape = 1, setunique = 0;
char *setdefault = "";
int outcol = 0, outrow = 0, scrrow = 0, scrcol = 0, outstarted = 0;
long seed = 12345;
char **argvs = NULL;
int nargvs = 0;
char *progname = "";
char *progdir = ".";
int curl = 0;
char *altfile = "";

/* ---------------------------------------------------------- output */
void emitn(const char *s, long n)
{
    long i, k = -1;
    if (n <= 0) return;
    for (i = n - 1; i >= 0; i--)
        if (s[i] == '\n') {
            k = i;
            break;
        }
    if (k >= 0) outcol = (int)(n - 1 - k);
    else outcol += (int)n;
    outstarted = 1;
    fwrite(s, 1, (size_t)n, stdout);
}

void emit(const char *s)
{
    emitn(s, (long)strlen(s));
}

void flushout(void)
{
    fflush(stdout);
}

void emitnl(void)
{
    emitn("\n", 1);
    outcol = 0;
    scrrow++;
    scrcol = 0;
}

void errout(const char *s)
{
    flushout();
    fputs(s, stderr);
    fputc('\n', stderr);
    fflush(stderr);
}
