/* lib4.c -- builtin functions, part 4: the file functions of
   lib3.tpl (FOPEN ... HB_FREADLINE).  lib4.tpl itself holds only
   database functions, which are in dbf.c. */
#include "xb.h"

/* helpers and the file table, defined in io.c */
V rx_substr(const char *s, long sl, long st, long ln);
extern long nfh;
extern char **fhname, **fhdata;
extern long *fhlen, *fhcap, *fhpos;
extern int *fhopen, *fhdirty, *fhmode;
int newhandle(const char *fname, char *data, long len, int mode);
void fhput(long i, const char *s, long n);
int fclosehandle(int h);

#define P(k) fv[base + (k)]

/* FOPEN */
static V p_fopen(int base, int n)
{
    const char *fname;
    char *data;
    long len;
    int md = 0;

    if (!isstr(P(1))) return mkint(-1);
    fname = SS(P(1));
    if (fileexists(fname) == 0) return mkint(-1);
    if (n >= 2) md = (int)(iarg(P(2), 0) % 4);
    data = bytesof(fname, &len);
    return mkint(newhandle(fname, data, len, md));
}

/* FCREATE */
static V p_fcreate(int base, int n)
{
    int h;

    (void)n;
    if (!isstr(P(1))) return mkint(-1);
    h = newhandle(SS(P(1)), NULL, 0, 1);
    fhdirty[h - 2] = 1;
    putbytes(SS(P(1)), "", 0);
    return mkint(h);
}

/* FCLOSE */
static V p_fclose(int base, int n)
{
    (void)n;
    return mklog(fclosehandle((int)iarg(P(1), 0)));
}

/* FWRITE */
static V p_fwrite(int base, int n)
{
    long i = iarg(P(1), 0) - 2, len, k;

    if (i < 1 || i > nfh) return mkint(-1);
    if (!isstr(P(2))) return mkint(0);
    len = SL(P(2));
    if (n >= 3) {
        k = iarg(P(3), len);
        if (k < len) len = k < 0 ? 0 : k;
    }
    fhput(i, SS(P(2)), len);
    return mkint(len);
}

/* the next k bytes at the file position, padded with blanks like
   the REXX substr of the cREXX version */
static V fhtake(long i, long k)
{
    V s;
    long p = fhpos[i];

    if (k < 0) k = 0;
    s = rx_substr(fhdata[i], fhlen[i], p + 1, k);
    fhpos[i] = p + SL(s);
    return s;
}

/* FREAD */
static V p_fread(int base, int n)
{
    long i = iarg(P(1), 0) - 2;
    V s;

    (void)n;
    if (i < 1 || i > nfh) return mkint(0);
    s = fhtake(i, iarg(P(3), 0));
    if (isref(P(2))) boxset(P(2), s);
    return mkint(SL(s));
}

/* FREADSTR */
static V p_freadstr(int base, int n)
{
    long i = iarg(P(1), 0) - 2;

    (void)n;
    if (i < 1 || i > nfh) return mkstr("");
    return fhtake(i, iarg(P(2), 0));
}

/* FSEEK */
static V p_fseek(int base, int n)
{
    long i = iarg(P(1), 0) - 2, off, org = 0, p;

    if (i < 1 || i > nfh) return mkint(-1);
    off = iarg(P(2), 0);
    if (n >= 3) org = iarg(P(3), 0);
    if (org == 0) p = off;
    else if (org == 1) p = fhpos[i] + off;
    else p = fhlen[i] + off;
    if (p < 0) p = 0;
    fhpos[i] = p;
    return mkint(p);
}

/* HB_FEOF */
static V p_feof(int base, int n)
{
    long i = iarg(P(1), 0) - 2;

    (void)n;
    if (i < 1 || i > nfh) return TRUEV;
    if (fhpos[i] >= fhlen[i]) return TRUEV;
    return FALSEV;
}

/* FERROR */
static V p_ferror(int base, int n)
{
    (void)base;
    (void)n;
    return mkint(0);
}

/* FERASE */
static V p_ferase(int base, int n)
{
    (void)n;
    if (!isstr(P(1))) return mkint(-1);
    if (fileexists(SS(P(1))) == 0) return mkint(-1);
    remove(SS(P(1)));
    return mkint(0);
}

/* FRENAME */
static V p_frename(int base, int n)
{
    char *data;
    long len;

    (void)n;
    if (!isstr(P(1)) || !isstr(P(2))) return mkint(-1);
    if (fileexists(SS(P(1))) == 0) return mkint(-1);
    data = bytesof(SS(P(1)), &len);
    putbytes(SS(P(2)), data, len);
    remove(SS(P(1)));
    return mkint(0);
}

/* FILE HB_FILEEXISTS */
static V p_file(int base, int n)
{
    (void)n;
    if (!isstr(P(1))) return FALSEV;
    return mklog(fileexists(xstrip(SS(P(1)))));
}

/* MEMOREAD HB_MEMOREAD: line ends (CR, LF, CRLF, LFCR) become LF,
   one final LF is dropped */
static V p_memoread(int base, int n)
{
    char *d;
    long len, i;
    Sb b;

    (void)n;
    if (!isstr(P(1))) return mkstr("");
    if (fileexists(SS(P(1))) == 0) return mkstr("");
    d = bytesof(SS(P(1)), &len);
    sb_init(&b);
    for (i = 0; i < len; i++) {
        if (d[i] == '\r') {
            sb_addc(&b, '\n');
            if (i + 1 < len && d[i + 1] == '\n') i++;
        } else if (d[i] == '\n') {
            sb_addc(&b, '\n');
            if (i + 1 < len && d[i + 1] == '\r') i++;
        } else sb_addc(&b, (unsigned char)d[i]);
    }
    if (b.n > 0 && sb_str(&b)[b.n - 1] == '\n') b.n--;
    return sb_val(&b);
}

/* MEMOWRIT HB_MEMOWRIT */
static V p_memowrit(int base, int n)
{
    (void)n;
    if (!isstr(P(1)) || !isstr(P(2))) return FALSEV;
    remove(SS(P(1)));
    putbytes(SS(P(1)), SS(P(2)), SL(P(2)));
    return TRUEV;
}

/* HB_FREADLINE */
static V p_freadline(int base, int n)
{
    long i = iarg(P(1), 0) - 2, p, q, len, e;
    char *d;

    (void)n;
    if (i < 1 || i > nfh) return mkstr("");
    d = fhdata[i];
    len = fhlen[i];
    p = fhpos[i];
    if (p >= len) return mkstr("");
    q = p;
    while (q < len && d[q] != '\n') q++;
    if (q >= len) {
        e = len;
        fhpos[i] = len;
    } else {
        e = q;
        fhpos[i] = q + 1;
    }
    if (e > p && d[e - 1] == '\r') e--;
    return mkstrn(d + p, e - p);
}

void lib4_init(void)
{
    addprim("FOPEN", p_fopen, 1, 2);
    addprim("FCREATE", p_fcreate, 1, 2);
    addprim("FCLOSE", p_fclose, 1, 1);
    addprim("FWRITE", p_fwrite, 2, 3);
    addprim("FREAD", p_fread, 3, 3);
    addprim("FREADSTR", p_freadstr, 2, 2);
    addprim("FSEEK", p_fseek, 2, 3);
    addprim("HB_FEOF", p_feof, 1, 1);
    addprim("FERROR", p_ferror, 0, 0);
    addprim("FERASE", p_ferase, 1, 1);
    addprim("FRENAME", p_frename, 2, 2);
    addprim("FILE|HB_FILEEXISTS", p_file, 1, 1);
    addprim("MEMOREAD|HB_MEMOREAD", p_memoread, 1, 1);
    addprim("MEMOWRIT|HB_MEMOWRIT", p_memowrit, 2, 2);
    addprim("HB_FREADLINE", p_freadline, 1, 2);
}
