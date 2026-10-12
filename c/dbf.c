/* dbf.c -- work areas, DBF tables, memo files, indexes, seek,
   relations and the database builtin functions (port of
   src/70_dbf.crexx and of the database prims at the end of
   src/lib4.tpl, from '#PRIM RECNO' on).

   TABLE LAYOUT (all indexes are 1-based like the cREXX version)

   Per work area a (1..MAXWA, MAXWA = 40) the tables wa_* are
   plain arrays indexed by a.  They are allocated for 1024 areas so
   that a stray SELECT 500 can still read wa_used[500] == 0.
   Field tables (fsym fnam ftyp flen fdec foff) are indexed
   fidx(a, f) = (a - 1) * 256 + f, f = 1..wa_nfld[a].  ftyp and
   fnam are NUL terminated C strings (ftyp: one letter).  foff[i]
   is the 0-based byte offset of the field inside a record image
   (the first field has foff 1, byte 0 is the deletion flag).

   recs: one record image per record, a char * of exactly
   wa_reclen[a] bytes (plus a NUL byte; the bytes may be anything,
   so always use wa_reclen, never strlen).  The image of record r
   of area a is recs[ridx(a, r)] with ridx(a, r) = (a - 1) * 100000
   + r, so an area holds at most 99999 records.  Unused entries are
   NULL.  Record images are never modified in place (fieldput,
   dbdelete make a new image), so a caller may share the pointer of
   one record between two tables.

   Indexes: id = 1..nix.  The scalar tables ix_* are indexed by id
   (they grow on demand; they are NULL until initareas or the first
   createindex).  The entries of index id live in the pools ixk
   (V, the key) and ixr (int, the record number): entry p
   (1..ix_len[id]) is ixk[ix_start[id] + p - 1] and
   ixr[ix_start[id] + p - 1]; ix_cap[id] entries are reserved.
   nixpool is the number of pool cells used; the pools are
   reallocated when they grow, so never keep a pointer into them.
   An index of an area is attached through waix[(a - 1) * 8 + j],
   j = 1..wa_nix[a].  Relations of area a: wa_relchild[(a - 1) * 8
   + j], wa_relkey[(a - 1) * 8 + j] (key expression node), j =
   1..wa_nrel[a].

   wa_memo[a] holds the whole .dbt file (binary, NUL terminated);
   its length is kept privately in memolen[].
*/
#include "xb.h"

#define WAMAX 1024
#define RECSTRIDE 100000L
#define NFTAB ((MAXWA + 1) * MAXFLD + 2)
#define NRTAB ((MAXWA + 1) * 8 + 64)

/* ---------------------------------------------- the tables */
static int s_used[WAMAX + 2];
static char *s_alias[WAMAX + 2];
static char *s_file[WAMAX + 2];
static int s_nrec[WAMAX + 2];
static int s_recno[WAMAX + 2];
static int s_eof[WAMAX + 2];
static int s_bof[WAMAX + 2];
static int s_found[WAMAX + 2];
static int s_nfld[WAMAX + 2];
static int s_hdr[WAMAX + 2];
static int s_reclen[WAMAX + 2];
static int s_dirty[WAMAX + 2];
static int s_order[WAMAX + 2];
static int s_ipos[WAMAX + 2];
static int s_filter[WAMAX + 2];
static char *s_filtertxt[WAMAX + 2];
static int s_locfor[WAMAX + 2];
static int s_locwhile[WAMAX + 2];
static int s_ro[WAMAX + 2];
static int s_nix[WAMAX + 2];
static int s_nrel[WAMAX + 2];
static char *s_memo[WAMAX + 2];
static int s_hasmemo[WAMAX + 2];
static char *s_blank[WAMAX + 2];
static long memolen[WAMAX + 2];
static int s_relchild[NRTAB];
static int s_relkey[NRTAB];
static int s_waix[NRTAB];
static int s_fsym[NFTAB];
static char *s_fnam[NFTAB];
static char *s_ftyp[NFTAB];
static int s_flen[NFTAB];
static int s_fdec[NFTAB];
static int s_foff[NFTAB];
static char *s_recs[MAXWA * RECSTRIDE + 2];

int *wa_used = s_used;
char **wa_alias = s_alias;
char **wa_file = s_file;
int *wa_nrec = s_nrec;
int *wa_recno = s_recno;
int *wa_eof = s_eof;
int *wa_bof = s_bof;
int *wa_found = s_found;
int *wa_nfld = s_nfld;
int *wa_hdr = s_hdr;
int *wa_reclen = s_reclen;
int *wa_dirty = s_dirty;
int *wa_order = s_order;
int *wa_ipos = s_ipos;
int *wa_filter = s_filter;
char **wa_filtertxt = s_filtertxt;
int *wa_locfor = s_locfor;
int *wa_locwhile = s_locwhile;
int *wa_ro = s_ro;
int *wa_nix = s_nix;
int *wa_relchild = s_relchild;
int *wa_relkey = s_relkey;
int *wa_nrel = s_nrel;
char **wa_memo = s_memo;
int *wa_hasmemo = s_hasmemo;
char **wa_blank = s_blank;
int *fsym = s_fsym;
char **fnam = s_fnam;
char **ftyp = s_ftyp;
int *flen = s_flen;
int *fdec = s_fdec;
int *foff = s_foff;
char **recs = s_recs;
int *waix = s_waix;

int *ix_area = NULL, *ix_node = NULL, *ix_for = NULL;
int *ix_desc = NULL, *ix_uniq = NULL, *ix_start = NULL;
int *ix_len = NULL, *ix_cap = NULL, *ix_dirty = NULL;
char **ix_text = NULL, **ix_fortext = NULL;
char **ix_file = NULL, **ix_name = NULL;
int nix = 0;
V *ixk = NULL;
int *ixr = NULL;
int nixpool = 0;
int curarea = 0;

static long ixtabcap = 0;       /* capacity of the ix_ tables */
static long poolcap = 0;        /* capacity of ixk / ixr */
static V okeys[16];

/* ------------------------------------------ forward declarations */
static V noarea(void);
static V memoread(int a, const char *blk);
static char *memowrite(int a, const char *txt, long n);
static V blankfield(int a, int f);
static void keysbefore(int a, int r);
static void keysafter(int a, int r);
static void keysappend(int a, int r);
static int passes(int a);
static void setrec(int a, int r);
static void setipos(int a, int p);
static void movenext(int a);
static void moveprev(int a);
static V evalkey(int a, int id, int r);
static int ixcmp(int id, V k1, int r1, V k2, int r2);
static int ixlower(int id, V k, int r);
static int ixfind(int id, int a, int r);
static void ixinsert(int id, V k, int r);
static void ixremove(int id, int p);
static void ixbuild(int id);
static void ixsort(int id);
static void ixunique(int id);
static char *ixfilename(const char *f);
static void ixwrite(int id);
static int isused(int a);

/* ------------------------------------------------------ helpers */
static int isused(int a)
{
    return a >= 1 && a < WAMAX && wa_used[a] == 1;
}

static long le(const char *d, long len, long p, int nbl)
{
    long r = 0, m = 1;
    int i;
    for (i = 0; i < nbl; i++) {
        long q = p + i;     /* 1-based */
        if (q >= 1 && q <= len)
            r += (long)(unsigned char)d[q - 1] * m;
        m *= 256;
    }
    return r;
}

static void addle(Sb *b, long v, int nbl)
{
    int i;
    for (i = 0; i < nbl; i++) {
        sb_addc(b, (int)(v % 256));
        v /= 256;
    }
}

static void addnuls(Sb *b, int n)
{
    int i;
    for (i = 0; i < n; i++) sb_addc(b, 0);
}

static int byteat(const char *d, long len, long p)
{
    if (p < 1 || p > len) return 0;
    return (unsigned char)d[p - 1];
}

/* numeric text -> integer / float (the cREXX 'as' conversions) */
static long as_int(const char *s)
{
    return strtol(s, NULL, 10);
}

static double as_float(const char *s)
{
    return strtod(s, NULL);
}

static void stripbuf(char *s)
{
    long n = (long)strlen(s), b = 0;
    while (n > 0 && s[n - 1] == ' ') n--;
    while (b < n && s[b] == ' ') b++;
    if (b > 0) memmove(s, s + b, (size_t)(n - b));
    s[n - b] = 0;
}

static V spaces(long n)
{
    static char sp[MAXFLD + 8];
    static int init = 0;
    if (!init) {
        memset(sp, ' ', sizeof(sp));
        init = 1;
    }
    return mkstrn(sp, n);
}

/* .dbt name of a .dbf name: changestr('.dbf', fn, '.dbt') */
static char *memofile(const char *fn)
{
    Sb b;
    const char *p = fn, *q;
    int hit = 0;
    sb_init(&b);
    while ((q = strstr(p, ".dbf")) != NULL) {
        sb_addn(&b, p, (long)(q - p));
        sb_adds(&b, ".dbt");
        p = q + 4;
        hit = 1;
    }
    sb_adds(&b, p);
    if (!hit) sb_adds(&b, ".dbt");
    return xstrndup(b.p, b.n);
}

/* julian day number of a date given as YYYYMMDD text */
static V datelit(const char *t, long n)
{
    char b[8];
    long y, m, d;
    if (n < 8) return mkdate(0);
    memcpy(b, t, 4); b[4] = 0; y = as_int(b);
    memcpy(b, t + 4, 2); b[2] = 0; m = as_int(b);
    memcpy(b, t + 6, 2); b[2] = 0; d = as_int(b);
    if (y == 0 && m == 0 && d == 0) return mkdate(0);
    return mkdate(ymd2jdn((int)y, (int)m, (int)d));
}

/* ---------------------------------------------- work areas */
static void ixtabinit(void)
{
    long i;
    if (ixtabcap == 0) {
        ixtabcap = 64;
        ix_area = (int *)xmalloc(ixtabcap * sizeof(int));
        ix_node = (int *)xmalloc(ixtabcap * sizeof(int));
        ix_for = (int *)xmalloc(ixtabcap * sizeof(int));
        ix_desc = (int *)xmalloc(ixtabcap * sizeof(int));
        ix_uniq = (int *)xmalloc(ixtabcap * sizeof(int));
        ix_start = (int *)xmalloc(ixtabcap * sizeof(int));
        ix_len = (int *)xmalloc(ixtabcap * sizeof(int));
        ix_cap = (int *)xmalloc(ixtabcap * sizeof(int));
        ix_dirty = (int *)xmalloc(ixtabcap * sizeof(int));
        ix_text = (char **)xmalloc(ixtabcap * sizeof(char *));
        ix_fortext = (char **)xmalloc(ixtabcap * sizeof(char *));
        ix_file = (char **)xmalloc(ixtabcap * sizeof(char *));
        ix_name = (char **)xmalloc(ixtabcap * sizeof(char *));
        for (i = 0; i < ixtabcap; i++) {
            ix_area[i] = ix_node[i] = ix_for[i] = 0;
            ix_desc[i] = ix_uniq[i] = ix_start[i] = 0;
            ix_len[i] = ix_cap[i] = ix_dirty[i] = 0;
            ix_text[i] = ix_fortext[i] = NULL;
            ix_file[i] = ix_name[i] = NULL;
        }
    }
}

void initareas(void)
{
    curarea = 1;
    ixtabinit();
}

static void ixtabgrow(long need)
{
    long ncap, i;
    ixtabinit();
    if (need < ixtabcap) return;
    ncap = ixtabcap * 2;
    if (ncap <= need) ncap = need + 16;
#define GROWI(p) p = (int *)xrealloc(p, ncap * (long)sizeof(int))
#define GROWS(p) p = (char **)xrealloc(p, ncap * (long)sizeof(char *))
    GROWI(ix_area); GROWI(ix_node); GROWI(ix_for);
    GROWI(ix_desc); GROWI(ix_uniq); GROWI(ix_start);
    GROWI(ix_len); GROWI(ix_cap); GROWI(ix_dirty);
    GROWS(ix_text); GROWS(ix_fortext);
    GROWS(ix_file); GROWS(ix_name);
#undef GROWI
#undef GROWS
    for (i = ixtabcap; i < ncap; i++) {
        ix_area[i] = ix_node[i] = ix_for[i] = 0;
        ix_desc[i] = ix_uniq[i] = ix_start[i] = 0;
        ix_len[i] = ix_cap[i] = ix_dirty[i] = 0;
        ix_text[i] = ix_fortext[i] = NULL;
        ix_file[i] = ix_name[i] = NULL;
    }
    ixtabcap = ncap;
}

/* make the pools hold cells 1..n */
static void poolneed(long n)
{
    long ncap, i;
    if (n + 2 <= poolcap) return;
    ncap = poolcap * 2;
    if (ncap < n + 2) ncap = n + 2;
    if (ncap < 1024) ncap = 1024;
    ixk = (V *)xrealloc(ixk, ncap * (long)sizeof(V));
    ixr = (int *)xrealloc(ixr, ncap * (long)sizeof(int));
    for (i = poolcap; i < ncap; i++) {
        memset(&ixk[i], 0, sizeof(V));
        ixr[i] = 0;
    }
    poolcap = ncap;
}

int ridx(int a, int r)
{
    return (int)((a - 1) * RECSTRIDE + r);
}

int fidx(int a, int f)
{
    return (a - 1) * 256 + f;
}

int areaok(int a)
{
    if (a < 1) return 0;
    if (a >= WAMAX) return 0;
    if (wa_used[a] == 1) return 1;
    return 0;
}

int findfreearea(void)
{
    int a;
    for (a = 1; a <= MAXWA; a++)
        if (wa_used[a] == 0) return a;
    return 0;
}

int aliasarea(const char *nm)
{
    int a;
    for (a = 1; a <= MAXWA; a++)
        if (wa_used[a] == 1 && wa_alias[a] != NULL
            && strcmp(wa_alias[a], nm) == 0) return a;
    return 0;
}

int fieldfind(int a, int sym)
{
    int b, f;
    if (a < 1) return 0;
    if (!isused(a)) return 0;
    b = (a - 1) * 256;
    for (f = 1; f <= wa_nfld[a]; f++)
        if (fsym[b + f] == sym) return f;
    return 0;
}

static V noarea(void)
{
    return rterr("DBFNTX", 2001, "Workarea not in use", "");
}

/* ---------------------------------------------------------- fields */
V fieldval(int a, int f)
{
    long r = wa_recno[a];
    const char *s, *raw;
    char buf[MAXFLD + 8];
    int i, w;
    char t;
    if (wa_eof[a] == 1 || r < 1 || r > wa_nrec[a])
        return blankfield(a, f);
    s = recs[ridx(a, (int)r)];
    i = fidx(a, f);
    raw = s + foff[i];
    w = flen[i];
    t = ftyp[i][0];
    if (t == 'C') return mkstrn(raw, w);
    if (t == 'N' || t == 'F') {
        memcpy(buf, raw, (size_t)w);
        buf[w] = 0;
        stripbuf(buf);
        if (buf[0] == 0 || strcmp(buf, ".") == 0
            || strcmp(buf, "-") == 0)
            return mknum(0.0, w, fdec[i]);
        if (fdec[i] == 0 && strchr(buf, '.') == NULL
            && strlen(buf) < 17)
            return mkint(as_int(buf));
        return mknum(as_float(buf), w, fdec[i]);
    }
    if (t == 'L') {
        int c = w > 0 ? toupper((unsigned char)raw[0]) : 0;
        if (c == 'T' || c == 'Y') return mklog(1);
        return mklog(0);
    }
    if (t == 'D') {
        memcpy(buf, raw, (size_t)w);
        buf[w] = 0;
        stripbuf(buf);
        if (buf[0] == 0) return mkdate(0);
        return datelit(raw, w);
    }
    if (t == 'M') {
        memcpy(buf, raw, (size_t)w);
        buf[w] = 0;
        stripbuf(buf);
        return memoread(a, buf);
    }
    return mkstrn(raw, w);
}

static V blankfield(int a, int f)
{
    int i = fidx(a, f);
    char t = ftyp[i][0];
    if (t == 'C') return spaces(flen[i]);
    if (t == 'N' || t == 'F') return mknum(0.0, flen[i], fdec[i]);
    if (t == 'L') return mklog(0);
    if (t == 'D') return mkdate(0);
    return mkstr("");
}

/* the stored text of a value for field i: exactly flen[i] bytes in
   a static buffer, or NULL after an error */
static char *fieldtext(int a, int i, V v)
{
    static char out[MAXFLD * 2 + 64];
    char t = ftyp[i][0];
    int w = flen[i];
    char *s;
    long n;
    const char *src = NULL;
    long srcn = 0;
    if (t == 'C') {
        if (!isstr(v)) {
            rterr("DBFNTX", 1020, "Data type error", wa_alias[a]);
            return NULL;
        }
        src = SS(v);
        srcn = SL(v);
    } else if (t == 'N' || t == 'F') {
        if (!isnum(v)) {
            rterr("DBFNTX", 1020, "Data type error", wa_alias[a]);
            return NULL;
        }
        s = fmtfloat(numval(v), fdec[i]);
        if ((int)strlen(s) > w) {
            memset(out, '*', (size_t)w);
            out[w] = 0;
            return out;
        }
        src = padleft(s, w);
        srcn = (long)strlen(src);
    } else if (t == 'L') {
        if (!islog(v)) {
            rterr("DBFNTX", 1020, "Data type error", wa_alias[a]);
            return NULL;
        }
        src = truthy(v) ? "T" : "F";
        srcn = 1;
    } else if (t == 'D') {
        if (!isdate(v)) {
            rterr("DBFNTX", 1020, "Data type error", wa_alias[a]);
            return NULL;
        }
        src = dtos(DJ(v));
        srcn = (long)strlen(src);
    } else if (t == 'M') {
        if (!isstr(v)) {
            rterr("DBFNTX", 1020, "Data type error", wa_alias[a]);
            return NULL;
        }
        src = memowrite(a, SS(v), SL(v));
        srcn = (long)strlen(src);
    }
    /* fit the text to the field width (the record length must stay
       fixed) */
    n = srcn < w ? srcn : w;
    if (n > 0) memcpy(out, src, (size_t)n);
    if (n < w) memset(out + n, ' ', (size_t)(w - n));
    out[w] = 0;
    return out;
}

void fieldput(int a, int f, V v)
{
    long r;
    int i, rl;
    char *txt, *s, *ns2;
    if (!isused(a)) {
        noarea();
        return;
    }
    r = wa_recno[a];
    if (wa_eof[a] == 1 || r < 1 || r > wa_nrec[a]) return;
    i = fidx(a, f);
    txt = fieldtext(a, i, v);
    if (ctl != 0) return;
    if (txt == NULL) return;
    if (wa_nix[a] > 0) keysbefore(a, (int)r);
    rl = wa_reclen[a];
    s = recs[ridx(a, (int)r)];
    ns2 = (char *)xmalloc((long)rl + 1);
    memcpy(ns2, s, (size_t)rl);
    ns2[rl] = 0;
    memcpy(ns2 + foff[i], txt, (size_t)flen[i]);
    recs[ridx(a, (int)r)] = ns2;
    wa_dirty[a] = 1;
    if (wa_nix[a] > 0) keysafter(a, (int)r);
}

/* memo fields: the .dbt file is kept in memory */
static V memoread(int a, const char *blk)
{
    long b, p, len, q;
    const char *d;
    if (blk[0] == 0) return mkstr("");
    b = as_int(blk);
    if (b < 1) return mkstr("");
    d = wa_memo[a];
    len = memolen[a];
    p = b * 512;                /* 0-based */
    if (p >= len) return mkstr("");
    for (q = p; q + 1 < len; q++)
        if (d[q] == 0x1a && d[q + 1] == 0x1a)
            return mkstrn(d + p, q - p);
    return mkstrn(d + p, len - p);
}

/* appends the text, returns the 10 character block field */
static char *memowrite(int a, const char *txt, long n)
{
    long len, blk, rem;
    char *d;
    static char res[32];
    char num[32];
    int k;
    if (n == 0) return "          ";
    len = memolen[a];
    d = wa_memo[a];
    if (len < 512) {
        d = (char *)xmalloc(512 + 1);
        memset(d, 0, 513);
        len = 512;
    }
    rem = len % 512;
    if (rem != 0) {
        d = (char *)xrealloc(d, len + 512 - rem + 1);
        memset(d + len, 0, (size_t)(512 - rem));
        len += 512 - rem;
    }
    blk = len / 512;
    d = (char *)xrealloc(d, len + n + 3);
    memcpy(d + len, txt, (size_t)n);
    d[len + n] = 0x1a;
    d[len + n + 1] = 0x1a;
    d[len + n + 2] = 0;
    wa_memo[a] = d;
    memolen[a] = len + n + 2;
    wa_hasmemo[a] = 1;
    sprintf(num, "%ld", blk);
    for (k = 0; k < 10; k++) res[k] = ' ';
    res[10] = 0;
    k = (int)strlen(num);
    if (k > 10) k = 10;
    memcpy(res + 10 - k, num, (size_t)k);
    return res;
}

/* --------------------------------------------------------- opening */
char *dbfstem(const char *f)
{
    const char *s = f, *k;
    char *r, *dot;
    k = strrchr(f, '/');
    if (k != NULL) s = k + 1;
    r = xstrdup(s);
    dot = strrchr(r, '.');
    if (dot != NULL && dot > r) *dot = 0;
    return xupper(r);
}

char *dbfname(const char *f)
{
    const char *k = strrchr(f, '/');
    const char *b = k != NULL ? k + 1 : f;
    if (strchr(b, '.') == NULL) return xfmt("%s.dbf", f);
    return xstrdup(f);
}

int openarea(int a, const char *fname, const char *alias, int ro)
{
    char *fn, *d, *mf;
    long len, nrec, hdr, rl, nfl, off, p, r, rp;
    int base, f, i;
    char nmb[16];
    char *nm;
    fn = dbfname(fname);
    if (a < 1 || a > MAXWA) {
        rterr("DBFNTX", 1001, "No free work area", fn);
        return 0;
    }
    if (fileexists(fn) == 0) {
        rterr("DBFNTX", 1001, "Open error", fn);
        return 0;
    }
    len = 0;
    d = bytesof(fn, &len);
    if (d == NULL || len < 33) {
        rterr("DBFNTX", 1003, "Corruption detected", fn);
        return 0;
    }
    nrec = le(d, len, 5, 4);
    hdr = le(d, len, 9, 2);
    rl = le(d, len, 11, 2);
    nfl = (hdr - 33) / 32;
    if (nfl > 255) nfl = 255;
    if (rl < 1 || nrec >= RECSTRIDE) {
        rterr("DBFNTX", 1003, "Corruption detected", fn);
        return 0;
    }
    base = (a - 1) * 256;
    off = 1;
    for (f = 1; f <= nfl; f++) {
        p = 33 + (f - 1) * 32;
        for (i = 0; i < 11; i++) {
            int c = byteat(d, len, p + i);
            nmb[i] = (char)(c == 0 ? ' ' : c);
        }
        nmb[11] = 0;
        nm = xupper(xstrip(nmb));
        fnam[base + f] = nm;
        fsym[base + f] = intern(nm);
        nmb[0] = (char)byteat(d, len, p + 11);
        nmb[1] = 0;
        ftyp[base + f] = xupper(nmb);
        flen[base + f] = byteat(d, len, p + 16);
        fdec[base + f] = byteat(d, len, p + 17);
        foff[base + f] = (int)off;
        off += flen[base + f];
    }
    wa_used[a] = 1;
    wa_alias[a] = xstrdup(alias);
    wa_file[a] = fn;
    wa_nrec[a] = (int)nrec;
    wa_nfld[a] = (int)nfl;
    wa_hdr[a] = (int)hdr;
    wa_reclen[a] = (int)rl;
    wa_dirty[a] = 0;
    wa_order[a] = 0;
    wa_ipos[a] = 0;
    wa_nix[a] = 0;
    wa_filter[a] = 0;
    wa_filtertxt[a] = xstrdup("");
    wa_ro[a] = ro;
    wa_nrel[a] = 0;
    wa_hasmemo[a] = 0;
    wa_memo[a] = xstrdup("");
    memolen[a] = 0;
    wa_blank[a] = (char *)xmalloc(rl + 1);
    memset(wa_blank[a], ' ', (size_t)rl);
    wa_blank[a][rl] = 0;
    for (r = 1; r <= nrec; r++) {
        char *rec = (char *)xmalloc(rl + 1);
        long avail;
        rp = hdr + (r - 1) * rl;        /* 0-based */
        avail = len - rp;
        if (avail < 0) avail = 0;
        if (avail > rl) avail = rl;
        if (avail > 0) memcpy(rec, d + rp, (size_t)avail);
        if (avail < rl) memset(rec + avail, ' ', (size_t)(rl - avail));
        rec[rl] = 0;
        recs[ridx(a, (int)r)] = rec;
    }
    /* memo file */
    for (f = 1; f <= nfl; f++) {
        if (ftyp[base + f][0] == 'M') {
            long ml = 0;
            char *md;
            mf = memofile(fn);
            wa_hasmemo[a] = 1;
            if (fileexists(mf) == 1) {
                md = bytesof(mf, &ml);
                if (md != NULL) {
                    wa_memo[a] = (char *)xmalloc(ml + 1);
                    memcpy(wa_memo[a], md, (size_t)ml);
                    wa_memo[a][ml] = 0;
                    memolen[a] = ml;
                }
            }
            break;
        }
    }
    wa_recno[a] = 1;
    wa_eof[a] = 0;
    wa_bof[a] = 0;
    wa_found[a] = 0;
    if (nrec == 0) {
        wa_eof[a] = 1;
        wa_bof[a] = 1;
    }
    return 1;
}

void flushdbf(int a)
{
    long nrec, rl, r;
    int nfl, base, f, ver;
    time_t tt;
    struct tm *tm;
    Sb h;
    char nm[16];
    int k, ln;
    if (!isused(a)) return;
    if (wa_dirty[a] == 0 || wa_ro[a] == 1) return;
    nrec = wa_nrec[a];
    rl = wa_reclen[a];
    nfl = wa_nfld[a];
    base = (a - 1) * 256;
    ver = 3;
    if (wa_hasmemo[a] == 1) ver = 131;
    tt = time(NULL);
    tm = localtime(&tt);
    sb_init(&h);
    sb_addc(&h, ver);
    sb_addc(&h, (tm->tm_year + 1900) % 100);
    sb_addc(&h, tm->tm_mon + 1);
    sb_addc(&h, tm->tm_mday);
    addle(&h, nrec, 4);
    addle(&h, wa_hdr[a], 2);
    addle(&h, rl, 2);
    addnuls(&h, 20);
    for (f = 1; f <= nfl; f++) {
        ln = (int)strlen(fnam[base + f]);
        for (k = 0; k < 10; k++)
            nm[k] = k < ln ? fnam[base + f][k] : 0;
        sb_addn(&h, nm, 10);
        sb_addc(&h, 0);
        sb_adds(&h, ftyp[base + f]);
        addnuls(&h, 4);
        sb_addc(&h, flen[base + f]);
        sb_addc(&h, fdec[base + f]);
        addnuls(&h, 14);
    }
    sb_addc(&h, 0x0d);
    for (r = 1; r <= nrec; r++)
        sb_addn(&h, recs[ridx(a, (int)r)], rl);
    sb_addc(&h, 0x1a);
    putbytes(wa_file[a], h.p, h.n);
    if (wa_hasmemo[a] == 1) {
        char *mf = memofile(wa_file[a]);
        long nbl, dl = memolen[a];
        char *d = wa_memo[a];
        char *o;
        if (dl < 512) {
            d = (char *)xmalloc(513);
            memset(d, 0, 513);
            dl = 512;
        }
        nbl = dl / 512 + 1;
        o = (char *)xmalloc(dl + 1);
        memcpy(o, d, (size_t)dl);
        for (k = 0; k < 4; k++) {
            o[k] = (char)(nbl % 256);
            nbl /= 256;
        }
        putbytes(mf, o, dl);
    }
    wa_dirty[a] = 0;
}

void closearea(int a)
{
    long r;
    if (a < 1 || !isused(a)) return;
    flushindexes(a);
    flushdbf(a);
    for (r = 1; r <= wa_nrec[a]; r++) recs[ridx(a, (int)r)] = NULL;
    wa_used[a] = 0;
    wa_nix[a] = 0;
    wa_order[a] = 0;
    wa_nrel[a] = 0;
    wa_filter[a] = 0;
}

void closeallareas(void)
{
    int a;
    for (a = 1; a <= MAXWA; a++)
        if (wa_used[a] == 1) closearea(a);
    closeallfiles();
}

/* create a new table; stru is an array of {name, type, len, dec} */
int createdbf(const char *fname, V stru)
{
    char *fn = dbfname(fname);
    long nfl = arrlen(stru), f, rl = 1, hdr;
    int hasmemo = 0, ver, k;
    Sb h, o;
    sb_init(&h);
    sb_init(&o);
    for (f = 1; f <= nfl; f++) {
        V e = arrget(stru, f);
        char *nm, *ty;
        long ln, dc;
        int tc, nl2;
        char nb[16];
        if (!isarr(e) || arrlen(e) < 4) {
            argerr("DBCREATE");
            return 0;
        }
        nm = xupper(sarg_z(arrget(e, 1)));
        ty = xupper(sarg_z(arrget(e, 2)));
        tc = ty[0];
        ln = iarg(arrget(e, 3), 10);
        dc = iarg(arrget(e, 4), 0);
        if (tc == 'D') ln = 8;
        if (tc == 'L') ln = 1;
        if (tc == 'M') {
            ln = 10;
            hasmemo = 1;
        }
        nl2 = (int)strlen(nm);
        for (k = 0; k < 10; k++) nb[k] = k < nl2 ? nm[k] : 0;
        sb_addn(&h, nb, 10);
        sb_addc(&h, 0);
        if (tc != 0) sb_addc(&h, tc);
        addnuls(&h, 4);
        sb_addc(&h, (int)(ln & 255));
        sb_addc(&h, (int)(dc & 255));
        addnuls(&h, 14);
        rl += ln;
    }
    hdr = 32 + nfl * 32 + 1;
    ver = 3;
    if (hasmemo == 1) ver = 131;
    sb_addc(&o, ver);
    sb_addc(&o, 26);
    sb_addc(&o, 1);
    sb_addc(&o, 1);
    addle(&o, 0, 4);
    addle(&o, hdr, 2);
    addle(&o, rl, 2);
    addnuls(&o, 20);
    sb_addn(&o, h.p, h.n);
    sb_addc(&o, 0x0d);
    sb_addc(&o, 0x1a);
    putbytes(fn, o.p, o.n);
    if (hasmemo == 1) {
        char *mf = memofile(fn);
        Sb m;
        sb_init(&m);
        addle(&m, 1, 4);
        addnuls(&m, 508);
        putbytes(mf, m.p, m.n);
    }
    return 1;
}

/* ------------------------------------------------------ navigation */
/* does the current record of area a pass SET DELETED and the filter */
static int passes(int a)
{
    int fnode, old;
    V c;
    if (setdeleted == 1) {
        const char *s = recs[ridx(a, wa_recno[a])];
        if (s != NULL && s[0] == '*') return 0;
    }
    fnode = wa_filter[a];
    if (fnode > 0) {
        old = curarea;
        curarea = a;
        c = evalnode(fnode);
        curarea = old;
        if (!truthy(c)) return 0;
    }
    return 1;
}

static void setrec(int a, int r)
{
    if (r < 1 || r > wa_nrec[a]) {
        wa_eof[a] = 1;
        wa_recno[a] = wa_nrec[a] + 1;
        return;
    }
    wa_eof[a] = 0;
    wa_recno[a] = r;
}

/* position on ordered index entry p (or eof if out of range) */
static void setipos(int a, int p)
{
    int id = wa_order[a];
    if (p < 1 || p > ix_len[id]) {
        wa_eof[a] = 1;
        wa_ipos[a] = ix_len[id] + 1;
        wa_recno[a] = wa_nrec[a] + 1;
        return;
    }
    wa_ipos[a] = p;
    wa_eof[a] = 0;
    wa_recno[a] = ixr[ix_start[id] + p - 1];
}

static void movenext(int a)
{
    int id = wa_order[a];
    if (id == 0) setrec(a, wa_recno[a] + 1);
    else setipos(a, wa_ipos[a] + 1);
}

static void moveprev(int a)
{
    int id = wa_order[a], p;
    if (id == 0) {
        if (wa_recno[a] <= 1) {
            wa_bof[a] = 1;
            wa_eof[a] = 0;
            wa_recno[a] = 1;
            if (wa_nrec[a] == 0) wa_eof[a] = 1;
            return;
        }
        setrec(a, wa_recno[a] - 1);
    } else {
        p = wa_ipos[a] - 1;
        if (wa_eof[a] == 1) p = ix_len[id];
        if (p < 1) {
            wa_bof[a] = 1;
            setipos(a, 1);
            if (ix_len[id] == 0) wa_eof[a] = 1;
            return;
        }
        setipos(a, p);
    }
}

void dbgotop(int a)
{
    int id;
    if (!isused(a)) {
        noarea();
        return;
    }
    wa_bof[a] = 0;
    id = wa_order[a];
    if (id == 0) setrec(a, 1);
    else setipos(a, 1);
    while (wa_eof[a] == 0) {
        if (passes(a) == 1) break;
        movenext(a);
    }
    if (wa_nrec[a] == 0 || wa_eof[a] == 1) wa_bof[a] = 1;
    relsync(a);
}

void dbgobottom(int a)
{
    int id;
    if (!isused(a)) {
        noarea();
        return;
    }
    wa_bof[a] = 0;
    id = wa_order[a];
    if (id == 0) setrec(a, wa_nrec[a]);
    else setipos(a, ix_len[id]);
    while (wa_eof[a] == 0) {
        if (passes(a) == 1) break;
        moveprev(a);
        if (wa_bof[a] == 1) {
            wa_eof[a] = 1;
            wa_recno[a] = wa_nrec[a] + 1;
        }
    }
    if (wa_nrec[a] == 0) wa_bof[a] = 1;
    relsync(a);
}

void dbgoto(int a, int r)
{
    int id;
    if (!isused(a)) {
        noarea();
        return;
    }
    wa_bof[a] = 0;
    if (r < 1 || r > wa_nrec[a]) {
        wa_eof[a] = 1;
        wa_recno[a] = wa_nrec[a] + 1;
        return;
    }
    wa_eof[a] = 0;
    wa_recno[a] = r;
    id = wa_order[a];
    if (id > 0) wa_ipos[a] = ixfind(id, a, r);
    relsync(a);
}

/* leave the top: stay on the first visible record */
static void firstvisible(int a)
{
    while (wa_eof[a] == 0) {
        if (passes(a) == 1) break;
        movenext(a);
    }
}

void dbskip(int a, int n)
{
    int k;
    if (!isused(a)) {
        noarea();
        return;
    }
    if (n == 0) return;
    if (n > 0) {
        wa_bof[a] = 0;
        for (k = 1; k <= n; k++) {
            if (wa_eof[a] == 1) break;
            movenext(a);
            while (wa_eof[a] == 0) {
                if (passes(a) == 1) break;
                movenext(a);
            }
        }
    } else {
        for (k = 1; k <= 0 - n; k++) {
            moveprev(a);
            if (wa_bof[a] == 1) {
                firstvisible(a);
                break;
            }
            while (passes(a) == 0) {
                moveprev(a);
                if (wa_bof[a] == 1) {
                    firstvisible(a);
                    break;
                }
            }
        }
    }
    relsync(a);
}

/* append a blank record */
void dbappend(int a)
{
    int n;
    if (!isused(a)) {
        noarea();
        return;
    }
    if (wa_ro[a] == 1) {
        rterr("DBFNTX", 1021, "Read only", wa_alias[a]);
        return;
    }
    if (wa_nrec[a] + 1 >= RECSTRIDE) {
        rterr("DBFNTX", 1022, "Too many records", wa_alias[a]);
        return;
    }
    n = wa_nrec[a] + 1;
    recs[ridx(a, n)] = xstrndup(wa_blank[a], wa_reclen[a]);
    wa_nrec[a] = n;
    wa_recno[a] = n;
    wa_eof[a] = 0;
    wa_bof[a] = 0;
    wa_dirty[a] = 1;
    if (wa_nix[a] > 0) keysappend(a, n);
    relsync(a);
}

void dbdelete(int a, int flag)
{
    int r, k, rl;
    char *s;
    if (!isused(a)) {
        noarea();
        return;
    }
    r = wa_recno[a];
    if (wa_eof[a] == 1 || r < 1 || r > wa_nrec[a]) return;
    k = ridx(a, r);
    rl = wa_reclen[a];
    s = (char *)xmalloc((long)rl + 1);
    memcpy(s, recs[k], (size_t)rl);
    s[rl] = 0;
    s[0] = flag == 1 ? '*' : ' ';
    recs[k] = s;
    wa_dirty[a] = 1;
}

int isdeleted(int a)
{
    int r = wa_recno[a];
    if (wa_eof[a] == 1 || r < 1 || r > wa_nrec[a]) return 0;
    if (recs[ridx(a, r)][0] == '*') return 1;
    return 0;
}

void dbpack(int a)
{
    int n = 0, r;
    char *s;
    for (r = 1; r <= wa_nrec[a]; r++) {
        s = recs[ridx(a, r)];
        if (s[0] != '*') {
            n++;
            recs[ridx(a, n)] = s;
        }
    }
    for (r = n + 1; r <= wa_nrec[a]; r++) recs[ridx(a, r)] = NULL;
    wa_nrec[a] = n;
    wa_dirty[a] = 1;
    reindexall(a);
    dbgotop(a);
}

void dbzap(int a)
{
    int r;
    for (r = 1; r <= wa_nrec[a]; r++) recs[ridx(a, r)] = NULL;
    wa_nrec[a] = 0;
    wa_dirty[a] = 1;
    reindexall(a);
    dbgotop(a);
}

/* --------------------------------------------------------- indexes */
static V evalkey(int a, int id, int r)
{
    int old = curarea, oldrec = wa_recno[a], oldeof = wa_eof[a];
    V v;
    curarea = a;
    wa_recno[a] = r;
    wa_eof[a] = 0;
    v = evalnode(ix_node[id]);
    wa_recno[a] = oldrec;
    wa_eof[a] = oldeof;
    curarea = old;
    return v;
}

/* ordering of two index entries (key, recno) */
static int ixcmp(int id, V k1, int r1, V k2, int r2)
{
    int c = cmpv(k1, k2);
    if (c == 2) c = 0;
    if (ix_desc[id] == 1) c = 0 - c;
    if (c != 0) return c;
    if (r1 < r2) return -1;
    if (r1 > r2) return 1;
    return 0;
}

/* position where (key, recno) belongs: first entry that is >= it */
static int ixlower(int id, V k, int r)
{
    int lo = 1, hi = ix_len[id] + 1, mid, s = ix_start[id];
    while (lo < hi) {
        mid = (lo + hi) / 2;
        if (ixcmp(id, ixk[s + mid - 1], ixr[s + mid - 1], k, r) < 0)
            lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static int ixfind(int id, int a, int r)
{
    V k = evalkey(a, id, r);
    int p = ixlower(id, k, r), s = ix_start[id];
    if (p <= ix_len[id]) {
        if (ixr[s + p - 1] == r) return p;
    }
    /* fall back to a scan */
    for (p = 1; p <= ix_len[id]; p++)
        if (ixr[s + p - 1] == r) return p;
    return ix_len[id] + 1;
}

static void ixinsert(int id, V k, int r)
{
    int n = ix_len[id], ncap, nsta, os, i, p, s;
    if (n >= ix_cap[id]) {
        ncap = n * 2 + 64;
        nsta = nixpool + 1;
        os = ix_start[id];
        poolneed((long)nixpool + ncap);
        for (i = 0; i < n; i++) {
            ixk[nsta + i] = ixk[os + i];
            ixr[nsta + i] = ixr[os + i];
        }
        ix_start[id] = nsta;
        ix_cap[id] = ncap;
        nixpool += ncap;
    }
    p = ixlower(id, k, r);
    s = ix_start[id];
    for (i = n; i >= p; i--) {
        ixk[s + i] = ixk[s + i - 1];
        ixr[s + i] = ixr[s + i - 1];
    }
    ixk[s + p - 1] = k;
    ixr[s + p - 1] = r;
    ix_len[id] = n + 1;
    ix_dirty[id] = 1;
}

static void ixremove(int id, int p)
{
    int n = ix_len[id], s = ix_start[id], i;
    for (i = p; i <= n - 1; i++) {
        ixk[s + i - 1] = ixk[s + i];
        ixr[s + i - 1] = ixr[s + i];
    }
    ix_len[id] = n - 1;
    ix_dirty[id] = 1;
}

static void keysbefore(int a, int r)
{
    int j, id;
    for (j = 1; j <= wa_nix[a]; j++) {
        id = waix[(a - 1) * 8 + j];
        okeys[j] = evalkey(a, id, r);
    }
}

static void keysafter(int a, int r)
{
    int j, id, p, s;
    V nkey, okey;
    for (j = 1; j <= wa_nix[a]; j++) {
        id = waix[(a - 1) * 8 + j];
        nkey = evalkey(a, id, r);
        okey = okeys[j];
        if (ixcmp(id, okey, r, nkey, r) != 0) {
            p = ixlower(id, okey, r);
            s = ix_start[id];
            if (p <= ix_len[id]) {
                if (ixr[s + p - 1] == r) ixremove(id, p);
            }
            ixinsert(id, nkey, r);
            if (wa_order[a] == id) wa_ipos[a] = ixfind(id, a, r);
        }
    }
}

static void keysappend(int a, int r)
{
    int j, id;
    V k;
    for (j = 1; j <= wa_nix[a]; j++) {
        id = waix[(a - 1) * 8 + j];
        k = evalkey(a, id, r);
        ixinsert(id, k, r);
        if (wa_order[a] == id) wa_ipos[a] = ixfind(id, a, r);
    }
}

/* build the index id from scratch */
static void ixbuild(int id)
{
    int a = ix_area[id], r, ok, n, s, old, oldrec;
    V c, k;
    ix_len[id] = 0;
    ix_cap[id] = wa_nrec[a] + 64;
    ix_start[id] = nixpool + 1;
    poolneed((long)nixpool + ix_cap[id]);
    nixpool += ix_cap[id];
    /* collect then sort by merge sort on (key, recno) */
    n = 0;
    s = ix_start[id];
    for (r = 1; r <= wa_nrec[a]; r++) {
        ok = 1;
        if (ix_for[id] > 0) {
            old = curarea;
            oldrec = wa_recno[a];
            curarea = a;
            wa_recno[a] = r;
            wa_eof[a] = 0;
            c = evalnode(ix_for[id]);
            wa_recno[a] = oldrec;
            curarea = old;
            if (!truthy(c)) ok = 0;
        }
        if (ok == 1) {
            k = evalkey(a, id, r);
            if (ctl != 0) return;
            n++;
            ixk[s + n - 1] = k;
            ixr[s + n - 1] = r;
        }
    }
    ix_len[id] = n;
    ixsort(id);
    if (ix_uniq[id] == 1) ixunique(id);
    ix_dirty[id] = 1;
}

static void ixsort(int id)
{
    int n = ix_len[id], s = ix_start[id];
    int w, i, lo, mid, hi, p, q, k, j;
    V *tkey;
    int *trec;
    if (n < 2) return;
    tkey = (V *)xmalloc((long)n * (long)sizeof(V));
    trec = (int *)xmalloc((long)n * (long)sizeof(int));
    w = 1;
    while (w < n) {
        i = 0;
        while (i < n) {
            lo = i;
            mid = i + w;
            if (mid > n) mid = n;
            hi = i + 2 * w;
            if (hi > n) hi = n;
            p = lo;
            q = mid;
            k = lo;
            while (p < mid && q < hi) {
                if (ixcmp(id, ixk[s + q], ixr[s + q],
                          ixk[s + p], ixr[s + p]) < 0) {
                    tkey[k] = ixk[s + q];
                    trec[k] = ixr[s + q];
                    q++;
                } else {
                    tkey[k] = ixk[s + p];
                    trec[k] = ixr[s + p];
                    p++;
                }
                k++;
            }
            while (p < mid) {
                tkey[k] = ixk[s + p];
                trec[k] = ixr[s + p];
                p++;
                k++;
            }
            while (q < hi) {
                tkey[k] = ixk[s + q];
                trec[k] = ixr[s + q];
                q++;
                k++;
            }
            i += 2 * w;
        }
        for (j = 0; j < n; j++) {
            ixk[s + j] = tkey[j];
            ixr[s + j] = trec[j];
        }
        w *= 2;
    }
    free(tkey);
    free(trec);
}

static void ixunique(int id)
{
    int n = ix_len[id], s = ix_start[id], m = 0, i, dup;
    for (i = 0; i < n; i++) {
        if (m == 0) dup = 0;
        else if (cmpv(ixk[s + m - 1], ixk[s + i]) == 0) dup = 1;
        else dup = 0;
        if (dup == 0) {
            ixk[s + m] = ixk[s + i];
            ixr[s + m] = ixr[s + i];
            m++;
        }
    }
    ix_len[id] = m;
}

static char *ixfilename(const char *f)
{
    const char *k = strrchr(f, '/');
    const char *b = k != NULL ? k + 1 : f;
    if (strchr(b, '.') == NULL) return xfmt("%s.xdx", f);
    return xstrdup(f);
}

/* INDEX ON expr TO file : creates and activates the index */
int createindex(int a, int keynode, const char *fname, int fornode,
                int fl, const char *keytext, const char *fortext)
{
    int id;
    if (wa_nix[a] >= 8) {
        rterr("DBFNTX", 1011, "Too many indexes", wa_alias[a]);
        return 0;
    }
    ixtabgrow((long)nix + 2);
    nix++;
    id = nix;
    ix_area[id] = a;
    ix_node[id] = keynode;
    ix_text[id] = xstrdup(keytext);
    ix_fortext[id] = xstrdup(fortext);
    ix_for[id] = fornode;
    ix_uniq[id] = fl % 2;
    ix_desc[id] = (fl / 2) % 2;
    ix_file[id] = xstrdup("");
    if (fname[0] != 0) ix_file[id] = ixfilename(fname);
    ix_name[id] = dbfstem(fname);
    ix_dirty[id] = 1;
    poolneed((long)nixpool + 1);
    ixbuild(id);
    if (ctl != 0) return 0;
    wa_nix[a] = wa_nix[a] + 1;
    waix[(a - 1) * 8 + wa_nix[a]] = id;
    wa_order[a] = id;
    ixwrite(id);
    dbgotop(a);
    return id;
}

static void addnum(Sb *b, long v)
{
    char num[32];
    sprintf(num, "%ld", v);
    sb_adds(b, num);
}

/* index files hold the key text, the flags and the record order */
static void ixwrite(int id)
{
    Sb b;
    int a, i, s;
    if (ix_file[id][0] == 0) return;
    a = ix_area[id];
    sb_init(&b);
    sb_adds(&b, "XDX1\n");
    sb_adds(&b, ix_text[id]);
    sb_addc(&b, '\n');
    sb_adds(&b, ix_fortext[id]);
    sb_addc(&b, '\n');
    addnum(&b, ix_uniq[id]);
    sb_addc(&b, ' ');
    addnum(&b, ix_desc[id]);
    sb_addc(&b, ' ');
    addnum(&b, wa_nrec[a]);
    sb_addc(&b, '\n');
    s = ix_start[id];
    for (i = 0; i < ix_len[id]; i++) {
        addnum(&b, ixr[s + i]);
        sb_addc(&b, ' ');
    }
    putbytes(ix_file[id], b.p, b.n);
    ix_dirty[id] = 0;
}

void flushindexes(int a)
{
    int j, id;
    for (j = 1; j <= wa_nix[a]; j++) {
        id = waix[(a - 1) * 8 + j];
        if (ix_dirty[id] == 1) ixwrite(id);
    }
}

void reindexall(int a)
{
    int j, id;
    for (j = 1; j <= wa_nix[a]; j++) {
        id = waix[(a - 1) * 8 + j];
        ixbuild(id);
        if (ix_file[id][0] != 0) ixwrite(id);
    }
    if (wa_order[a] > 0) wa_ipos[a] = 1;
}

/* attach an existing index file */
int openindex(int a, const char *fname)
{
    char *fn = ixfilename(fname), *t, *keytext, *fortext, *flags;
    char *l1, *l2, *l3, *l4;
    long len = 0;
    int node, fnode, fl;
    long u, dsc;
    if (fileexists(fn) == 0) {
        rterr("DBFNTX", 1001, "Open error", fn);
        return 0;
    }
    t = bytesof(fn, &len);
    if (t == NULL) len = 0;
    t = xstrndup(t == NULL ? "" : t, len);
    l1 = strchr(t, '\n');
    l2 = l1 != NULL ? strchr(l1 + 1, '\n') : NULL;
    l3 = l2 != NULL ? strchr(l2 + 1, '\n') : NULL;
    l4 = l3 != NULL ? strchr(l3 + 1, '\n') : NULL;
    if (l4 == NULL) {
        rterr("DBFNTX", 1003, "Corruption detected", fn);
        return 0;
    }
    keytext = xstrndup(l1 + 1, (long)(l2 - l1 - 1));
    fortext = xstrndup(l2 + 1, (long)(l3 - l2 - 1));
    flags = xstrndup(l3 + 1, (long)(l4 - l3 - 1));
    node = compileexpr(keytext);
    if (node == 0) {
        rterr("DBFNTX", 1004, "Index key error", keytext);
        return 0;
    }
    fnode = 0;
    if (fortext[0] != 0) fnode = compileexpr(fortext);
    u = 0;
    dsc = 0;
    {
        char *e;
        u = strtol(flags, &e, 10);
        dsc = strtol(e, NULL, 10);
    }
    fl = (int)(u + 2 * dsc);
    return createindex(a, node, fn, fnode, fl, keytext, fortext);
}

/* ------------------------------------------------------------ seek */
static int cmpseek(V k, V v)
{
    int c;
    if (isstr(k) && isstr(v)) {
        long vn = SL(v), kn = SL(k);
        char *pre = (char *)xmalloc(vn + 1);
        if (kn >= vn) memcpy(pre, SS(k), (size_t)vn);
        else {
            memcpy(pre, SS(k), (size_t)kn);
            memset(pre + kn, ' ', (size_t)(vn - kn));
        }
        c = strcmp_x(pre, vn, SS(v), vn);
        free(pre);
        return c;
    }
    c = cmpv(k, v);
    if (c == 2) return 0;
    return c;
}

int dbseek(int a, V v, int soft)
{
    int id, s, lo, hi, mid, c, found = 0, p;
    if (!isused(a)) {
        noarea();
        return 0;
    }
    id = wa_order[a];
    if (id == 0) {
        rterr("DBFNTX", 2001, "No active order", wa_alias[a]);
        return 0;
    }
    s = ix_start[id];
    lo = 1;
    hi = ix_len[id] + 1;
    while (lo < hi) {
        mid = (lo + hi) / 2;
        c = cmpseek(ixk[s + mid - 1], v);
        if (ix_desc[id] == 1) c = 0 - c;
        if (c < 0) lo = mid + 1;
        else hi = mid;
    }
    wa_bof[a] = 0;
    p = lo;
    /* skip entries that are filtered out */
    while (p <= ix_len[id]) {
        setipos(a, p);
        c = cmpseek(ixk[s + p - 1], v);
        if (c != 0) break;
        if (passes(a) == 1) {
            found = 1;
            break;
        }
        p++;
    }
    if (found == 0) {
        if (soft == 1 || setsoft == 1) {
            setipos(a, p);
            while (wa_eof[a] == 0) {
                if (passes(a) == 1) break;
                movenext(a);
            }
        } else {
            wa_eof[a] = 1;
            wa_recno[a] = wa_nrec[a] + 1;
            wa_ipos[a] = ix_len[id] + 1;
        }
    }
    wa_found[a] = found;
    relsync(a);
    return found;
}

/* ------------------------------------------------------- relations */
void relsync(int a)
{
    int j, ch, kn, old;
    V v;
    for (j = 1; j <= wa_nrel[a]; j++) {
        ch = wa_relchild[(a - 1) * 8 + j];
        kn = wa_relkey[(a - 1) * 8 + j];
        if (!isused(ch)) continue;
        old = curarea;
        curarea = a;
        v = evalnode(kn);
        curarea = old;
        if (wa_order[ch] > 0) dbseek(ch, v, 0);
        else if (isnum(v)) dbgoto(ch, (int)toint(numval(v)));
    }
}

/* ======================================================= PRIMS */
/* argument k (1-based) or NIL when not given */
#define P(k) ((k) <= n ? fv[base + (k)] : NILV)
#define UNUSED (void)base; (void)n

/* a work area given as number or alias value */
static int parea(V v)
{
    if (isnum(v)) return (int)toint(numval(v));
    if (isstr(v)) return aliasarea(xupper(xstrip(SS(v))));
    return 0;
}

static V p_recno(int base, int n)
{
    UNUSED;
    if (!isused(curarea)) return mkint(0);
    return mkint(wa_recno[curarea]);
}

static V p_eof(int base, int n)
{
    UNUSED;
    if (!isused(curarea)) return mklog(1);
    return mklog(wa_eof[curarea]);
}

static V p_bof(int base, int n)
{
    UNUSED;
    if (!isused(curarea)) return mklog(1);
    return mklog(wa_bof[curarea]);
}

static V p_found(int base, int n)
{
    UNUSED;
    if (!isused(curarea)) return mklog(0);
    return mklog(wa_found[curarea]);
}

static V p_deleted(int base, int n)
{
    UNUSED;
    if (!isused(curarea)) return mklog(0);
    return mklog(isdeleted(curarea));
}

static V p_lastrec(int base, int n)
{
    UNUSED;
    if (!isused(curarea)) return mkint(0);
    return mkint(wa_nrec[curarea]);
}

static V p_fcount(int base, int n)
{
    UNUSED;
    if (!isused(curarea)) return mkint(0);
    return mkint(wa_nfld[curarea]);
}

static V p_fieldname(int base, int n)
{
    int a = curarea;
    long i;
    if (n == 2) a = parea(P(2));
    if (!isused(a)) return mkstr("");
    i = iarg(P(1), 0);
    if (i < 1 || i > wa_nfld[a]) return mkstr("");
    return mkstr(fnam[(a - 1) * 256 + i]);
}

static V p_fieldpos(int base, int n)
{
    int a = curarea;
    if (n == 2) a = parea(P(2));
    if (!isused(a)) return mkint(0);
    return mkint(fieldfind(a, intern(xupper(xstrip(sarg_z(P(1)))))));
}

static V p_fieldget(int base, int n)
{
    int a = curarea;
    long i;
    if (n == 2) a = parea(P(2));
    if (!isused(a)) return NILV;
    i = iarg(P(1), 0);
    if (i < 1 || i > wa_nfld[a]) return NILV;
    return fieldval(a, (int)i);
}

static V p_fieldput(int base, int n)
{
    int a = curarea;
    long i;
    if (n == 3) a = parea(P(3));
    if (!isused(a)) return NILV;
    i = iarg(P(1), 0);
    if (i < 1 || i > wa_nfld[a]) return NILV;
    fieldput(a, (int)i, P(2));
    return P(2);
}

static V p_fieldlen(int base, int n)
{
    int a = curarea;
    long i;
    if (a < 1 || a >= WAMAX) return mkint(0);
    i = iarg(P(1), 0);
    if (i < 1 || i > wa_nfld[a]) return mkint(0);
    return mkint(flen[(a - 1) * 256 + i]);
}

static V p_fielddec(int base, int n)
{
    int a = curarea;
    long i;
    if (a < 1 || a >= WAMAX) return mkint(0);
    i = iarg(P(1), 0);
    if (i < 1 || i > wa_nfld[a]) return mkint(0);
    return mkint(fdec[(a - 1) * 256 + i]);
}

static V p_fieldtype(int base, int n)
{
    int a = curarea;
    long i;
    if (a < 1 || a >= WAMAX) return mkstr("");
    i = iarg(P(1), 0);
    if (i < 1 || i > wa_nfld[a]) return mkstr("");
    return mkstr(ftyp[(a - 1) * 256 + i]);
}

static V p_alias(int base, int n)
{
    int a = curarea;
    if (n == 1) a = parea(P(1));
    if (!isused(a)) return mkstr("");
    return mkstr(wa_alias[a]);
}

static V p_select(int base, int n)
{
    char *u;
    if (n == 0) return mkint(curarea);
    if (isstr(P(1))) {
        u = xupper(xstrip(SS(P(1))));
        if (u[0] == 0) return mkint(curarea);
        return mkint(aliasarea(u));
    }
    return mkint(iarg(P(1), 0));
}

static V p_dbselectarea(int base, int n)
{
    int a;
    long k;
    if (isstr(P(1))) {
        a = aliasarea(xupper(xstrip(SS(P(1)))));
        if (a == 0)
            return rterr("BASE", 1002, "Alias does not exist",
                         SS(P(1)));
        curarea = a;
    } else {
        k = iarg(P(1), 0);
        if (k == 0) k = findfreearea();
        curarea = (int)k;
    }
    return NILV;
}

static V p_used(int base, int n)
{
    UNUSED;
    if (curarea < 1) return mklog(0);
    return mklog(isused(curarea));
}

static V p_neterr(int base, int n)
{
    UNUSED;
    return mklog(0);
}

static V p_rlock(int base, int n)
{
    UNUSED;
    return mklog(1);
}

static V p_unlock(int base, int n)
{
    UNUSED;
    return NILV;
}

static V p_dbcommit(int base, int n)
{
    UNUSED;
    if (curarea > 0) {
        flushdbf(curarea);
        flushindexes(curarea);
    }
    return NILV;
}

/* DbUseArea( [lNew], [cDriver], cFile, [cAlias], [lShared],
   [lReadOnly] ) */
static V p_dbusearea(int base, int n)
{
    int newf = 0, a, ro = 0;
    char *f, *al;
    if (n >= 1) {
        if (islog(P(1))) newf = truthy(P(1));
    }
    if (n < 3) {
        if (curarea > 0) closearea(curarea);
        return NILV;
    }
    f = sarg_z(P(3));
    if (newf == 1 || curarea < 1) a = findfreearea();
    else a = curarea;
    if (a == 0) return rterr("DBFNTX", 1001, "No free work area", f);
    if (isused(a)) closearea(a);
    al = "";
    if (n >= 4) {
        if (isstr(P(4))) al = xupper(xstrip(SS(P(4))));
    }
    if (al[0] == 0) al = dbfstem(f);
    if (n >= 6) {
        if (truthy(P(6))) ro = 1;
    }
    if (openarea(a, f, al, ro) == 0) return NILV;
    curarea = a;
    return NILV;
}

static V p_dbclosearea(int base, int n)
{
    UNUSED;
    if (curarea > 0) closearea(curarea);
    return NILV;
}

static V p_dbcloseall(int base, int n)
{
    UNUSED;
    closeallareas();
    curarea = 0;
    return NILV;
}

static V p_dbcreate(int base, int n)
{
    UNUSED;
    if (!isarr(P(2))) return argerr("DBCREATE");
    createdbf(sarg_z(P(1)), P(2));
    return NILV;
}

static V p_dbstruct(int base, int n)
{
    int a = curarea, i;
    V r, e;
    UNUSED;
    if (a < 1 || a >= WAMAX) return mkarray(0);
    r = mkarray(0);
    for (i = 1; i <= wa_nfld[a]; i++) {
        e = mkarray(0);
        arrpush(e, mkstr(fnam[(a - 1) * 256 + i]));
        arrpush(e, mkstr(ftyp[(a - 1) * 256 + i]));
        arrpush(e, mkint(flen[(a - 1) * 256 + i]));
        arrpush(e, mkint(fdec[(a - 1) * 256 + i]));
        arrpush(r, e);
    }
    return r;
}

static V p_dbappend(int base, int n)
{
    UNUSED;
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    dbappend(curarea);
    return NILV;
}

static V p_dbdelete(int base, int n)
{
    UNUSED;
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    dbdelete(curarea, 1);
    return NILV;
}

static V p_dbrecall(int base, int n)
{
    UNUSED;
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    dbdelete(curarea, 0);
    return NILV;
}

static V p_dbskip(int base, int n)
{
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    dbskip(curarea, (int)iarg(P(1), 1));
    return NILV;
}

static V p_dbgoto(int base, int n)
{
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    dbgoto(curarea, (int)iarg(P(1), 0));
    return NILV;
}

static V p_dbgotop(int base, int n)
{
    UNUSED;
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    dbgotop(curarea);
    return NILV;
}

static V p_dbgobottom(int base, int n)
{
    UNUSED;
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    dbgobottom(curarea);
    return NILV;
}

static V p_dbseek(int base, int n)
{
    int soft = 0;
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    if (n >= 2) {
        if (truthy(P(2))) soft = 1;
    }
    return mklog(dbseek(curarea, P(1), soft));
}

static V p_dbpack(int base, int n)
{
    UNUSED;
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    dbpack(curarea);
    return NILV;
}

static V p_dbzap(int base, int n)
{
    UNUSED;
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    dbzap(curarea);
    return NILV;
}

static V p_dbsetfilter(int base, int n)
{
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    wa_filter[curarea] = 0;
    if (n == 2) {
        if (isstr(P(2)))
            wa_filter[curarea] = compileexpr(SS(P(2)));
    }
    return NILV;
}

static V p_dbclearfilter(int base, int n)
{
    UNUSED;
    if (curarea > 0) wa_filter[curarea] = 0;
    return NILV;
}

static V p_dbfilter(int base, int n)
{
    UNUSED;
    return mkstr("");
}

/* DbCreateIndex( cFile, cKey, [bKey], [lUnique] ): cKey is compiled
   from text */
static V p_dbcreateindex(int base, int n)
{
    char *kt;
    int nd2, fl = 0;
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    kt = sarg_z(P(2));
    nd2 = compileexpr(kt);
    if (nd2 == 0) return rterr("DBFNTX", 1004, "Index key error", kt);
    if (n >= 4) {
        if (truthy(P(4))) fl = 1;
    }
    createindex(curarea, nd2, sarg_z(P(1)), 0, fl, kt, "");
    return NILV;
}

static V p_dbsetindex(int base, int n)
{
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    openindex(curarea, sarg_z(P(1)));
    return NILV;
}

static V p_dbreindex(int base, int n)
{
    UNUSED;
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    reindexall(curarea);
    return NILV;
}

static V p_ordlistclear(int base, int n)
{
    UNUSED;
    if (curarea > 0) {
        wa_nix[curarea] = 0;
        wa_order[curarea] = 0;
    }
    return NILV;
}

static V p_dbsetorder(int base, int n)
{
    int a = curarea, old = 0, j;
    long k;
    char *u;
    if (curarea < 1) return mkint(0);
    for (j = 1; j <= wa_nix[a]; j++)
        if (waix[(a - 1) * 8 + j] == wa_order[a]) old = j;
    if (n >= 1) {
        k = -1;
        if (isnum(P(1))) k = iarg(P(1), 0);
        else if (isstr(P(1))) {
            u = xupper(xstrip(SS(P(1))));
            k = 0;
            for (j = 1; j <= wa_nix[a]; j++)
                if (strcmp(ix_name[waix[(a - 1) * 8 + j]], u) == 0)
                    k = j;
        }
        if (k >= 0 && k <= wa_nix[a]) {
            if (k == 0) wa_order[a] = 0;
            else wa_order[a] = waix[(a - 1) * 8 + k];
            dbgotop(a);
        }
    }
    return mkint(old);
}

static V p_indexord(int base, int n)
{
    int a = curarea, j;
    UNUSED;
    if (curarea < 1) return mkint(0);
    for (j = 1; j <= wa_nix[a]; j++)
        if (waix[(a - 1) * 8 + j] == wa_order[a]) return mkint(j);
    return mkint(0);
}

static V p_indexkey(int base, int n)
{
    int a = curarea;
    UNUSED;
    if (curarea < 1) return mkstr("");
    if (wa_order[a] == 0) return mkstr("");
    return mkstr(ix_text[wa_order[a]]);
}

static V p_ordname(int base, int n)
{
    int a = curarea;
    long k;
    if (curarea < 1) return mkstr("");
    k = iarg(P(1), 0);
    if (n == 0) {
        if (wa_order[a] == 0) return mkstr("");
        return mkstr(ix_name[wa_order[a]]);
    }
    if (k < 1 || k > wa_nix[a]) return mkstr("");
    return mkstr(ix_name[waix[(a - 1) * 8 + k]]);
}

static V p_ordcount(int base, int n)
{
    UNUSED;
    if (curarea < 1) return mkint(0);
    return mkint(wa_nix[curarea]);
}

/* DbEval( bBlock, [bFor], [bWhile], [nNext], [nRecord], [lRest] ) */
static V p_dbeval(int base, int n)
{
    int a = curarea, rest = 0, ok;
    long nxt = -1;
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    if (n >= 4) {
        if (isnum(P(4))) nxt = iarg(P(4), -1);
    }
    if (n >= 5) {
        if (isnum(P(5))) {
            dbgoto(a, (int)iarg(P(5), 0));
            nxt = 1;
        }
    }
    if (n >= 6) {
        if (truthy(P(6))) rest = 1;
    }
    if (nxt == -1 && rest == 0 && n < 5) dbgotop(a);
    while (wa_eof[a] == 0 && nxt != 0) {
        if (n >= 3) {
            if (isblk(P(3))) {
                if (!truthy(evalblock0(P(3)))) break;
            }
        }
        ok = 1;
        if (n >= 2) {
            if (isblk(P(2))) {
                if (!truthy(evalblock0(P(2)))) ok = 0;
            }
        }
        if (ok == 1) evalblock0(P(1));
        if (ctl != 0) return NILV;
        if (nxt > 0) nxt--;
        if (n >= 5) break;
        dbskip(a, 1);
    }
    return NILV;
}

static V p_hbdbpack(int base, int n)
{
    UNUSED;
    if (curarea > 0) dbpack(curarea);
    return NILV;
}

static V p_lupdate(int base, int n)
{
    time_t tt;
    struct tm *tm;
    UNUSED;
    tt = time(NULL);
    tm = localtime(&tt);
    return mkdate(ymd2jdn(tm->tm_year + 1900, tm->tm_mon + 1,
                          tm->tm_mday));
}

static V p_header(int base, int n)
{
    UNUSED;
    if (curarea < 1) return mkint(0);
    return mkint(wa_hdr[curarea]);
}

static V p_recsize(int base, int n)
{
    UNUSED;
    if (curarea < 1) return mkint(0);
    return mkint(wa_reclen[curarea]);
}

static V p_dbinfo(int base, int n)
{
    UNUSED;
    return NILV;
}

static V p_dbrelation(int base, int n)
{
    UNUSED;
    return mkstr("");
}

static V p_dbsetrelation(int base, int n)
{
    int a = curarea, k, nd2;
    if (curarea < 1) {
        noarea();
        return NILV;
    }
    k = parea(P(1));
    if (k < 1) return NILV;
    if (!isstr(P(2))) return NILV;
    nd2 = compileexpr(SS(P(2)));
    wa_nrel[a] = wa_nrel[a] + 1;
    wa_relchild[(a - 1) * 8 + wa_nrel[a]] = k;
    wa_relkey[(a - 1) * 8 + wa_nrel[a]] = nd2;
    relsync(a);
    return NILV;
}

static V p_dbclearrelation(int base, int n)
{
    UNUSED;
    if (curarea > 0) wa_nrel[curarea] = 0;
    return NILV;
}

void dbfprims_init(void)
{
    addprim("RECNO", p_recno, 0, 0);
    addprim("EOF", p_eof, 0, 0);
    addprim("BOF", p_bof, 0, 0);
    addprim("FOUND", p_found, 0, 0);
    addprim("DELETED", p_deleted, 0, 0);
    addprim("LASTREC|RECCOUNT", p_lastrec, 0, 0);
    addprim("FCOUNT", p_fcount, 0, 0);
    addprim("FIELDNAME|FIELD", p_fieldname, 1, 2);
    addprim("FIELDPOS", p_fieldpos, 1, 2);
    addprim("FIELDGET", p_fieldget, 1, 2);
    addprim("FIELDPUT", p_fieldput, 2, 3);
    addprim("HB_FIELDLEN|FIELDLEN", p_fieldlen, 1, 2);
    addprim("HB_FIELDDEC|FIELDDEC", p_fielddec, 1, 2);
    addprim("HB_FIELDTYPE|FIELDTYPE", p_fieldtype, 1, 2);
    addprim("ALIAS", p_alias, 0, 1);
    addprim("SELECT", p_select, 0, 1);
    addprim("DBSELECTAREA", p_dbselectarea, 1, 1);
    addprim("USED", p_used, 0, 0);
    addprim("NETERR", p_neterr, 0, 1);
    addprim("RLOCK|FLOCK|DBRLOCK|DBFLOCK", p_rlock, 0, 2);
    addprim("DBUNLOCK|DBUNLOCKALL|DBRUNLOCK|DBCOMMITALL", p_unlock,
            0, 1);
    addprim("DBCOMMIT", p_dbcommit, 0, 0);
    addprim("DBUSEAREA", p_dbusearea, 0, 8);
    addprim("DBCLOSEAREA", p_dbclosearea, 0, 0);
    addprim("DBCLOSEALL", p_dbcloseall, 0, 0);
    addprim("DBCREATE", p_dbcreate, 2, 5);
    addprim("DBSTRUCT", p_dbstruct, 0, 0);
    addprim("DBAPPEND", p_dbappend, 0, 1);
    addprim("DBDELETE", p_dbdelete, 0, 0);
    addprim("DBRECALL", p_dbrecall, 0, 0);
    addprim("DBSKIP", p_dbskip, 0, 1);
    addprim("DBGOTO", p_dbgoto, 1, 1);
    addprim("DBGOTOP", p_dbgotop, 0, 0);
    addprim("DBGOBOTTOM", p_dbgobottom, 0, 0);
    addprim("DBSEEK", p_dbseek, 1, 3);
    addprim("DBPACK", p_dbpack, 0, 0);
    addprim("DBZAP", p_dbzap, 0, 0);
    addprim("DBSETFILTER", p_dbsetfilter, 1, 2);
    addprim("DBCLEARFILTER", p_dbclearfilter, 0, 0);
    addprim("DBFILTER", p_dbfilter, 0, 0);
    addprim("DBCREATEINDEX|ORDCREATE", p_dbcreateindex, 2, 5);
    addprim("DBSETINDEX|ORDLISTADD", p_dbsetindex, 1, 2);
    addprim("DBREINDEX|ORDLISTREBUILD", p_dbreindex, 0, 0);
    addprim("ORDLISTCLEAR|DBCLEARINDEX", p_ordlistclear, 0, 0);
    addprim("DBSETORDER|ORDSETFOCUS", p_dbsetorder, 0, 2);
    addprim("INDEXORD", p_indexord, 0, 0);
    addprim("INDEXKEY|ORDKEY", p_indexkey, 0, 1);
    addprim("ORDNAME", p_ordname, 0, 2);
    addprim("ORDCOUNT", p_ordcount, 0, 2);
    addprim("DBEVAL", p_dbeval, 1, 6);
    addprim("HB_DBPACK", p_hbdbpack, 0, 0);
    addprim("LUPDATE", p_lupdate, 0, 0);
    addprim("HEADER", p_header, 0, 0);
    addprim("RECSIZE", p_recsize, 0, 0);
    addprim("DBINFO|DBORDERINFO|DBRECORDINFO|DBFIELDINFO", p_dbinfo,
            0, -1);
    addprim("DBRELATION|DBRSELECT", p_dbrelation, 0, 1);
    addprim("DBSETRELATION", p_dbsetrelation, 2, 3);
    addprim("DBCLEARRELATION", p_dbclearrelation, 0, 0);
}
