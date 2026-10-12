/* io.c -- console input, file handles, Transform, and the helper
   procedures of the library (port of 60_helpers.crexx, 61_io.crexx).

   The REXX string functions that the cREXX sources use are
   reproduced with their REXX behaviour (SUBSTR and LEFT pad with
   blanks, ...): see rx_substr, rx_left below.
*/
#include "xb.h"

int ineof = 0;

/* ------------------------------------------------------ helpers */
char *sarg_z(V v)
{
    if (isstr(v)) return SS(v);
    return "";
}

long iarg(V v, long dflt)
{
    if (isnum(v)) return toint(numval(v));
    return dflt;
}

/* REXX substr(s, st, ln): st >= 1; ln < 0 means "to the end"; a
   given length pads with blanks */
V rx_substr(const char *s, long sl, long st, long ln)
{
    Sb b;
    long av;

    if (st < 1) st = 1;
    av = sl - st + 1;
    if (av < 0) av = 0;
    if (ln < 0) ln = av;
    sb_init(&b);
    if (ln == 0) return sb_val(&b);
    if (av > ln) av = ln;
    if (av > 0) sb_addn(&b, s + st - 1, av);
    while (av < ln) {
        sb_addc(&b, ' ');
        av++;
    }
    return sb_val(&b);
}

/* REXX left(s, k): pads with blanks */
V rx_left(const char *s, long sl, long k)
{
    return rx_substr(s, sl, 1, k < 0 ? 0 : k);
}

/* REXX strip(): mode 0 both sides, 1 leading, 2 trailing */
V rx_strip(V s, int mode)
{
    long a = 0, e = SL(s);
    char *p = SS(s);

    if (mode != 2) while (a < e && p[a] == ' ') a++;
    if (mode != 1) while (e > a && p[e - 1] == ' ') e--;
    return mkstrn(p + a, e - a);
}

/* REXX pos(): 1-based position of f in h from 1-based start,
   0 if none or f empty */
long rx_pos(const char *f, long fl, const char *h, long hl,
            long from)
{
    long i;

    if (fl == 0) return 0;
    if (from < 1) from = 1;
    for (i = from - 1; i + fl <= hl; i++)
        if (h[i] == f[0] && memcmp(h + i, f, fl) == 0)
            return i + 1;
    return 0;
}

/* REXX lastpos() */
long rx_lastpos(const char *f, long fl, const char *h, long hl)
{
    long i;

    if (fl == 0) return 0;
    for (i = hl - fl; i >= 0; i--)
        if (h[i] == f[0] && memcmp(h + i, f, fl) == 0)
            return i + 1;
    return 0;
}

/* substring with xBase rules: start may be negative */
V xsubstr(V s, long st, long ln)
{
    long n = SL(s);

    if (st < 0) st = n + st + 1;
    if (st < 1) st = 1;
    if (st > n) return mkstrn("", 0);
    if (ln < 0) ln = n - st + 1;
    if (ln == 0) return mkstrn("", 0);
    return rx_substr(SS(s), n, st, ln);
}

V xtrimall(V s)
{
    return rx_strip(s, 0);
}

V xaspace(long n)
{
    Sb b;

    sb_init(&b);
    while (n > 0) {
        sb_addc(&b, ' ');
        n--;
    }
    return sb_val(&b);
}

/* numeric part at the beginning of a string */
V valnum(const char *s)
{
    const char *t = s;
    long n, i, dig = 0, dot = 0, decs = 0, ln;
    int c;
    char *txt, *q;
    double x;

    while (*t == ' ') t++;
    n = (long)strlen(t);
    i = 0;
    if (i < n && (t[i] == '-' || t[i] == '+')) i++;
    while (i < n) {
        c = (unsigned char)t[i];
        if (isdigitc(c)) {
            dig++;
            if (dot == 1) decs++;
        } else if (c == '.' && dot == 0) dot = 1;
        else break;
        i++;
    }
    if (dig == 0) return mkint(0);
    txt = xstrndup(t, i);
    q = txt;
    if (*q == '+') q++;
    ln = (long)strlen(q);
    if (dot == 0) {
        if (ln < 17) return mkint(strtol(q, NULL, 10));
        x = strtod(q, NULL);
        return mknum(x, ln > 255 ? 255 : (int)ln, 0);
    }
    if (q[ln - 1] == '.') {
        q = xfmt("%s0", q);
        ln++;
    }
    x = strtod(q, NULL);
    return mknum(x, ln > 255 ? 255 : (int)ln,
                 decs > 255 ? 255 : (int)decs);
}

int isalphac(int c)
{
    if (c >= 65 && c <= 90) return 1;
    if (c >= 97 && c <= 122) return 1;
    return 0;
}

long nextrand(void)
{
    unsigned long s = (unsigned long)seed;

    s = (s * 1103515245UL + 12345UL) & 0x7fffffffUL;
    seed = (long)s;
    return seed;
}

/* position of a substring, with optional start/end */
long xat(V needle, V hay, long st, long en)
{
    long p;

    if (SL(needle) == 0) return 0;
    if (st < 1) st = 1;
    if (en <= 0 || en > SL(hay)) en = SL(hay);
    if (st > en) return 0;
    p = rx_pos(SS(needle), SL(needle), SS(hay) + st - 1,
               en - st + 1, 1);
    if (p == 0) return 0;
    return p + st - 1;
}

long xrat(V needle, V hay)
{
    if (SL(needle) == 0) return 0;
    return rx_lastpos(SS(needle), SL(needle), SS(hay), SL(hay));
}

/* sort comparison: element order (block or default) */
int sortless(V a, V b, V blk)
{
    V r;
    int c;

    if (isblk(blk)) {
        r = evalblock2(blk, a, b);
        return truthy(r) ? 1 : 0;
    }
    c = cmpv(a, b);
    if (c == 2) return 0;
    if (c < 0) return 1;
    return 0;
}

/* stable merge sort of arr[first .. first+n-1] */
void msort_arr(V arr, long first, long n, V blk)
{
    V *tmp;
    long w, i, lo, mid, hi, a, b, k, j;
    V ea, eb;

    if (n < 2) return;
    tmp = (V *)xmalloc(n * (long)sizeof(V));
    w = 1;
    while (w < n) {
        i = 0;
        while (i < n) {
            lo = i;
            mid = i + w;
            if (mid > n) mid = n;
            hi = i + 2 * w;
            if (hi > n) hi = n;
            a = lo;
            b = mid;
            k = lo;
            while (a < mid && b < hi) {
                ea = arrget(arr, first + a);
                eb = arrget(arr, first + b);
                if (sortless(eb, ea, blk)) {
                    tmp[k] = eb;
                    b++;
                } else {
                    tmp[k] = ea;
                    a++;
                }
                k++;
                if (ctl != 0) {
                    free(tmp);
                    return;
                }
            }
            while (a < mid) {
                tmp[k] = arrget(arr, first + a);
                a++;
                k++;
            }
            while (b < hi) {
                tmp[k] = arrget(arr, first + b);
                b++;
                k++;
            }
            i += 2 * w;
        }
        for (j = 0; j < n; j++) arrset(arr, first + j, tmp[j]);
        w *= 2;
    }
    free(tmp);
}

long bitop(int op, long a, long b)
{
    long r = 0, w = 1, x, y, z;
    int i;

    if (a < 0) a = 0 - a;
    if (b < 0) b = 0 - b;
    for (i = 0; i < 62; i++) {
        x = a % 2;
        y = b % 2;
        z = 0;
        if (op == 1) {
            if (x == 1 && y == 1) z = 1;
        } else if (op == 2) {
            if (x == 1 || y == 1) z = 1;
        } else {
            if (x != y) z = 1;
        }
        r = r + z * w;
        w = w * 2;
        a = a / 2;
        b = b / 2;
    }
    return r;
}

/* Array(n1, n2, ...) : nested arrays */
V mkmulti(int base, int lvl, int tot)
{
    V k = fv[base + lvl];
    V a;
    long cnt = 0, i;

    if (isnum(k)) cnt = toint(numval(k));
    if (cnt < 0) cnt = 0;
    a = mkarray(cnt);
    if (lvl < tot) {
        for (i = 1; i <= cnt; i++)
            arrset(a, i, mkmulti(base, lvl + 1, tot));
    }
    return a;
}

/* deep copy of arrays and hashes */
V cloneval(V v)
{
    V a, h;
    long i, n;

    if (isarr(v)) {
        n = arrlen(v);
        a = mkarray(n);
        for (i = 1; i <= n; i++)
            arrset(a, i, cloneval(arrget(v, i)));
        return a;
    }
    if (ishash(v)) {
        h = mkhash();
        n = hashlen(v);
        for (i = 1; i <= n; i++)
            hset(h, hkey(v, i), cloneval(hval(v, i)));
        return h;
    }
    return v;
}

/* value as source text, for hb_ValToExp */
static void vte(Sb *b, V v)
{
    long i, n;

    if (isstr(v)) {
        if (memchr(SS(v), '"', SL(v)) == NULL) {
            sb_addc(b, '"');
            sb_addn(b, SS(v), SL(v));
            sb_addc(b, '"');
        } else {
            sb_addc(b, '\'');
            sb_addn(b, SS(v), SL(v));
            sb_addc(b, '\'');
        }
        return;
    }
    if (isdate(v)) {
        sb_adds(b, "0d");
        sb_adds(b, dtos(DJ(v)));
        return;
    }
    if (isarr(v)) {
        n = arrlen(v);
        sb_addc(b, '{');
        for (i = 1; i <= n; i++) {
            if (i > 1) sb_adds(b, ", ");
            vte(b, arrget(v, i));
        }
        sb_addc(b, '}');
        return;
    }
    if (ishash(v)) {
        n = hashlen(v);
        sb_addc(b, '{');
        for (i = 1; i <= n; i++) {
            if (i > 1) sb_adds(b, ", ");
            vte(b, hkey(v, i));
            sb_adds(b, " => ");
            vte(b, hval(v, i));
        }
        if (n == 0) sb_adds(b, "=>");
        sb_addc(b, '}');
        return;
    }
    sb_adds(b, vtostr(v));
}

char *vtoexp(V v)
{
    Sb b;

    sb_init(&b);
    vte(&b, v);
    return sb_str(&b);
}

/* hb_StrFormat: %s %d %i %f %% */
V strformat(int base, int n)
{
    V fs = fv[base + 1];
    const char *f = SS(fs);
    long fl = SL(fs), i = 0;
    int k = 1;
    Sb b;

    sb_init(&b);
    while (i < fl) {
        if (f[i] == '%') {
            i++;
            if (i < fl && f[i] == '%') sb_addc(&b, '%');
            else {
                k++;
                if (k <= n) sb_adds(&b, vtostr(fv[base + k]));
            }
        } else sb_addc(&b, f[i]);
        i++;
    }
    return sb_val(&b);
}

/* ------------------------------------------------ console input */
static int inhave = 0;
static char *inbuf = NULL;
static long inlen = 0, inpos = 0;

/* one line of standard input; CR and LF each end a line (like the
   cREXX linein); 0 at the end of input */
static int rdline(char **p, long *n)
{
    Sb b;
    int c;

    c = getc(stdin);
    if (c == EOF) return 0;
    sb_init(&b);
    while (c != EOF && c != '\r' && c != '\n') {
        sb_addc(&b, c);
        c = getc(stdin);
    }
    *n = b.n;
    *p = sb_str(&b);
    return 1;
}

char *readcon(void)
{
    char *p;
    long n;

    flushout();
    if (inhave == 1) {
        inhave = 0;
        p = xstrndup(inbuf + inpos, inlen - inpos);
        inbuf = NULL;
        return p;
    }
    if (!rdline(&p, &n)) {
        ineof = 1;
        return xstrdup("");
    }
    return p;
}

/* one character from standard input: 13 at the end of a line, 0 at
   the end of input */
int readkey(void)
{
    int c;

    flushout();
    if (inhave == 0) {
        if (!rdline(&inbuf, &inlen)) {
            ineof = 1;
            return 0;
        }
        inpos = 0;
        inhave = 1;
    }
    if (inpos >= inlen) {
        inhave = 0;
        return 13;
    }
    c = (unsigned char)inbuf[inpos];
    inpos++;
    return c;
}

/* KEYBOARD: put text in front of the pending console input */
void keyboard_push(const char *t)
{
    Sb b;

    if (inhave == 1) {
        sb_init(&b);
        sb_adds(&b, t);
        sb_adds(&b, inbuf + inpos);
        inbuf = xstrdup(sb_str(&b));
    } else {
        inbuf = xstrdup(t);
        inhave = 1;
    }
    inlen = (long)strlen(inbuf);
    inpos = 0;
}

/* ------------------------------------------------- file table */
long nfh = 0;
char **fhname = NULL, **fhdata = NULL;
long *fhlen = NULL, *fhcap = NULL, *fhpos = NULL;
int *fhopen = NULL, *fhdirty = NULL, *fhmode = NULL;
static long fhalloc = 0;

char *bytesof(const char *fname, long *len)
{
    FILE *f;
    Sb b;
    char buf[4096];
    size_t k;

    sb_init(&b);
    f = fopen(fname, "rb");
    if (f != NULL) {
        while ((k = fread(buf, 1, sizeof buf, f)) > 0)
            sb_addn(&b, buf, (long)k);
        fclose(f);
    }
    *len = b.n;
    return sb_str(&b);
}

int putbytes(const char *fname, const char *p, long n)
{
    FILE *f;
    int rc = 0;

    f = fopen(fname, "wb");
    if (f == NULL) return 1;
    if (n > 0 && fwrite(p, 1, (size_t)n, f) != (size_t)n) rc = 1;
    if (fclose(f) != 0) rc = 1;
    return rc;
}

static void fhgrow(long need)
{
    long nc, i;

    if (need < fhalloc) return;
    nc = fhalloc * 2 + 8;
    if (nc <= need) nc = need + 8;
    fhname = (char **)xrealloc(fhname, nc * (long)sizeof(char *));
    fhdata = (char **)xrealloc(fhdata, nc * (long)sizeof(char *));
    fhlen = (long *)xrealloc(fhlen, nc * (long)sizeof(long));
    fhcap = (long *)xrealloc(fhcap, nc * (long)sizeof(long));
    fhpos = (long *)xrealloc(fhpos, nc * (long)sizeof(long));
    fhopen = (int *)xrealloc(fhopen, nc * (long)sizeof(int));
    fhdirty = (int *)xrealloc(fhdirty, nc * (long)sizeof(int));
    fhmode = (int *)xrealloc(fhmode, nc * (long)sizeof(int));
    for (i = fhalloc; i < nc; i++) {
        fhname[i] = NULL;
        fhdata[i] = NULL;
        fhlen[i] = fhcap[i] = fhpos[i] = 0;
        fhopen[i] = fhdirty[i] = fhmode[i] = 0;
    }
    fhalloc = nc;
}

/* data: malloc'd buffer of len bytes (taken over); returns the
   DOS style handle (slot + 2) */
int newhandle(const char *fname, char *data, long len, int mode)
{
    long i, h = 0;

    for (i = 1; i <= nfh; i++) {
        if (fhopen[i] == 0) {
            h = i;
            break;
        }
    }
    if (h == 0) {
        nfh++;
        fhgrow(nfh);
        h = nfh;
    }
    fhname[h] = xstrdup(fname);
    fhdata[h] = data;
    fhlen[h] = len;
    fhcap[h] = len;
    fhpos[h] = 0;
    fhopen[h] = 1;
    fhdirty[h] = 0;
    fhmode[h] = mode;
    return (int)(h + 2);
}

/* write n bytes at the position of slot i (zero fill past the
   end), as the cREXX FWrite does */
void fhput(long i, const char *s, long n)
{
    long p = fhpos[i];

    if (p + n > fhcap[i]) {
        fhcap[i] = p + n + 64;
        if (fhdata[i] == NULL)
            fhdata[i] = (char *)xmalloc(fhcap[i] + 1);
        else
            fhdata[i] = (char *)xrealloc(fhdata[i], fhcap[i] + 1);
    }
    if (p > fhlen[i]) memset(fhdata[i] + fhlen[i], 0, p - fhlen[i]);
    if (n > 0) memcpy(fhdata[i] + p, s, n);
    if (p + n > fhlen[i]) fhlen[i] = p + n;
    else if (p > fhlen[i]) fhlen[i] = p;
    fhpos[i] = p + n;
    fhdirty[i] = 1;
}

int fclosehandle(int h)
{
    long i = h - 2;

    if (i < 1 || i > nfh) return 0;
    if (fhopen[i] == 0) return 0;
    if (fhdirty[i] == 1) putbytes(fhname[i], fhdata[i], fhlen[i]);
    fhopen[i] = 0;
    if (fhdata[i] != NULL) free(fhdata[i]);
    fhdata[i] = NULL;
    fhlen[i] = fhcap[i] = 0;
    return 1;
}

void closeallfiles(void)
{
    long i;

    for (i = 1; i <= nfh; i++)
        if (fhopen[i] == 1) fclosehandle((int)(i + 2));
}

/* ------------------------------------------------- Transform */
static int hasc(const char *fn, int c)
{
    return strchr(fn, c) != NULL;
}

static char *spaces(long n)
{
    Sb b;

    sb_init(&b);
    while (n > 0) {
        sb_addc(&b, ' ');
        n--;
    }
    return sb_str(&b);
}

static char *transstr(const char *s, const char *fn,
                      const char *tmpl)
{
    Sb r;
    long k = 0, i, ls = (long)strlen(s), lt = (long)strlen(tmpl);
    int rmode = hasc(fn, 'R'), c, uc, slot, ch;

    sb_init(&r);
    for (i = 0; i < lt; i++) {
        c = (unsigned char)tmpl[i];
        uc = toupper(c);
        slot = 0;
        if (uc == 'A' || uc == 'N' || uc == 'X' || c == '9' ||
            c == '#' || c == '!' || uc == 'L' || uc == 'Y')
            slot = 1;
        if (slot == 1) {
            ch = ' ';
            if (k < ls) ch = (unsigned char)s[k];
            k++;
            if (c == '!') ch = toupper(ch);
            sb_addc(&r, ch);
        } else {
            if (!rmode) k++;
            sb_addc(&r, c);
        }
    }
    return sb_str(&r);
}

static char *transnum(V v, const char *fn, const char *tmpl)
{
    double x = numval(v);
    long lt = (long)strlen(tmpl), dp = 0, nint = 0, ndec = 0, i;
    long q, lip, ii, fi, p, lo, lt2;
    int neg = 0, seen = 0, c, ch, fillc;
    char *s, *ip, *fp, *t, *o;
    Sb out;

    if (hasc(fn, 'Z') && x == 0.0) return spaces(lt < 1 ? 1 : lt);
    if (lt == 0) {
        s = numstr(v);
        if (hasc(fn, 'B')) {
            while (*s == ' ') s++;
        }
        return s;
    }
    for (i = 0; i < lt; i++)
        if (tmpl[i] == '.') {
            dp = i + 1;
            break;
        }
    for (i = 1; i <= lt; i++) {
        c = tmpl[i - 1];
        if (c == '9' || c == '#' || c == '*' || c == '$') {
            if (dp == 0 || i < dp) nint++;
            else ndec++;
        }
    }
    if (x < 0.0) {
        neg = 1;
        x = 0.0 - x;
    }
    s = fmtfloat(x, (int)ndec);
    q = 0;
    while (s[q] != 0 && s[q] != '.') q++;
    if (s[q] == '.') {
        ip = xstrndup(s, q);
        fp = xstrdup(s + q + 1);
    } else {
        ip = xstrdup(s);
        fp = xstrdup("");
    }
    lip = (long)strlen(ip);
    if (lip > nint) {
        if (nint > 0 || lip > 1 || strcmp(ip, "0") != 0) {
            Sb st;

            sb_init(&st);
            for (i = 0; i < lt; i++) sb_addc(&st, '*');
            return sb_str(&st);
        }
    }
    fillc = ' ';
    if (hasc(tmpl, '*')) fillc = '*';
    {
        Sb t2;

        sb_init(&t2);
        for (i = lip; i < nint; i++) sb_addc(&t2, fillc);
        sb_adds(&t2, ip);
        ip = sb_str(&t2);
        lip = (long)strlen(ip);
    }
    sb_init(&out);
    ii = 0;
    fi = 0;
    for (i = 1; i <= lt; i++) {
        c = (unsigned char)tmpl[i - 1];
        if (c == '9' || c == '#' || c == '*' || c == '$') {
            if (dp == 0 || i < dp) {
                ch = -1;
                if (ii < lip) ch = (unsigned char)ip[ii];
                ii++;
                if (ch != ' ' && ch != '*' && ch != -1) seen = 1;
                if (c == '$' && ch == fillc) ch = '$';
                if (ch != -1) sb_addc(&out, ch);
            } else {
                if (fi < (long)strlen(fp))
                    sb_addc(&out, fp[fi]);
                fi++;
            }
        } else if (c == ',') {
            if (seen == 1) sb_addc(&out, ',');
            else sb_addc(&out, fillc);
        } else sb_addc(&out, c);
    }
    o = sb_str(&out);
    if (hasc(fn, 'E')) {
        o = xstrdup(o);
        for (i = 0; o[i] != 0; i++) {
            if (o[i] == '.') o[i] = ',';
            else if (o[i] == ',') o[i] = '.';
        }
    }
    if (neg == 1) {
        lo = (long)strlen(o);
        if (hasc(fn, '(')) {
            t = o;
            while (*t == ' ') t++;
            o = xfmt("(%s)", t);
        } else {
            p = 1;
            while (p <= lo && o[p - 1] == ' ') p++;
            t = xstrdup(o);
            if (p > 1) t[p - 2] = '-';
            else t = xfmt("-%s", o);
            o = t;
        }
    }
    if (hasc(fn, 'B')) {
        lo = (long)strlen(o);
        t = o;
        while (*t == ' ') t++;
        lt2 = (long)strlen(t);
        o = xfmt("%s%s", t, spaces(lo - lt2));
    }
    if (hasc(fn, 'C') && neg == 0) o = xfmt("%s CR", o);
    if (hasc(fn, 'X') && neg == 1) o = xfmt("%s DB", o);
    return o;
}

char *xtransform(V v, const char *pic)
{
    char *fn = xstrdup("");
    const char *tmpl = pic;
    const char *sp;
    char *s, *u;
    long i;

    if (pic[0] == '@') {
        sp = strchr(pic, ' ');
        if (sp == NULL) {
            fn = xupper(pic + 1);
            tmpl = "";
        } else {
            fn = xstrndup(pic + 1, (long)(sp - pic) - 1);
            for (i = 0; fn[i] != 0; i++)
                fn[i] = (char)toupper((unsigned char)fn[i]);
            tmpl = sp + 1;
        }
    }
    if (isnum(v)) return transnum(v, fn, tmpl);
    if (isstr(v)) {
        s = SS(v);
        if (hasc(fn, '!')) s = xupper(s);
        if (tmpl[0] == 0) return s;
        return transstr(s, fn, tmpl);
    }
    if (islog(v)) {
        if (hasc(fn, 'Y')) return xstrdup(truthy(v) ? "Y" : "N");
        return xstrdup(truthy(v) ? "T" : "F");
    }
    if (isdate(v)) {
        if (hasc(fn, 'E')) return datefmt(DJ(v), "dd/mm/yy");
        return datestr(DJ(v));
    }
    u = vtostr(v);
    return u;
}

void setdate(const char *name)
{
    char *u, *l;
    const char *f;

    u = xupper(xstrip(name));
    if (strcmp(u, "AMERICAN") == 0) f = "mm/dd/yy";
    else if (strcmp(u, "ANSI") == 0) f = "yy.mm.dd";
    else if (strcmp(u, "BRITISH") == 0 ||
             strcmp(u, "FRENCH") == 0) f = "dd/mm/yy";
    else if (strcmp(u, "GERMAN") == 0) f = "dd.mm.yy";
    else if (strcmp(u, "ITALIAN") == 0) f = "dd-mm-yy";
    else if (strcmp(u, "JAPAN") == 0) f = "yy/mm/dd";
    else if (strcmp(u, "USA") == 0) f = "mm-dd-yy";
    else if (strcmp(u, "DUTCH") == 0) f = "dd-mm-yy";
    else if (strcmp(u, "SPANISH") == 0) f = "dd-mm-yy";
    else {
        l = xlower(xstrip(name));
        f = l;
    }
    strncpy(setdatefmt, f, sizeof setdatefmt - 1);
    setdatefmt[sizeof setdatefmt - 1] = 0;
}

/* move the cursor: new lines or blanks, as far as a line oriented
   console can */
void gotopos(int r, int c)
{
    if (r > scrrow) {
        while (scrrow < r) emitnl();
    } else if (r < scrrow && outcol > 0) emitnl();
    (void)c; /* padding dropped, as in the cREXX version */
}
