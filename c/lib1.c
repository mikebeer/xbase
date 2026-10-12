/* lib1.c -- builtin functions, part 1 (port of lib1.tpl):
   strings, numbers, type tests, bit operations, random numbers */
#include "xb.h"

/* helpers defined in io.c */
V rx_substr(const char *s, long sl, long st, long ln);
V rx_left(const char *s, long sl, long k);
V rx_strip(V s, int mode);
V xsubstr(V s, long st, long ln);
V xaspace(long n);
int isalphac(int c);
long nextrand(void);
long xat(V needle, V hay, long st, long en);
long xrat(V needle, V hay);
long bitop(int op, long a, long b);
long rx_pos(const char *f, long fl, const char *h, long hl,
            long from);

#define P(k) fv[base + (k)]

/* a number held as a whole number (the cREXX "int cell") */
static int icell(V v)
{
    double x = numval(v);

    if (numdec(v) != 0 || numwid(v) != 10) return 0;
    if (x > 9.0e18 || x < -9.0e18) return 1;
    return x == floor(x);
}

static V caseconv(V s, int up)
{
    V r = mkstrn(SS(s), SL(s));
    long i;
    char *p = SS(r);

    for (i = 0; i < SL(r); i++) {
        if (up) p[i] = (char)toupper((unsigned char)p[i]);
        else p[i] = (char)tolower((unsigned char)p[i]);
    }
    return r;
}

/* LEN */
static V p_len(int base, int n)
{
    V v = P(1);

    (void)n;
    if (isstr(v)) return mkint(SL(v));
    if (isarr(v)) return mkint(arrlen(v));
    if (ishash(v)) return mkint(hashlen(v));
    return argerr("LEN");
}

/* EMPTY */
static V p_empty(int base, int n)
{
    V v = P(1);
    long i;

    (void)n;
    if (isnil(v)) return TRUEV;
    if (islog(v)) return mklog(!truthy(v));
    if (isstr(v)) {
        for (i = 0; i < SL(v); i++)
            if (SS(v)[i] != ' ') return FALSEV;
        return TRUEV;
    }
    if (isnum(v)) return mklog(numval(v) == 0.0);
    if (isdate(v)) return mklog(DJ(v) == 0);
    if (isarr(v)) return mklog(arrlen(v) == 0);
    if (ishash(v)) return mklog(hashlen(v) == 0);
    return FALSEV;
}

/* VALTYPE */
static V p_valtype(int base, int n)
{
    char t[2];

    (void)n;
    t[0] = typech(P(1));
    t[1] = 0;
    return mkstr(t);
}

/* STR */
static V p_str(int base, int n)
{
    V v = P(1);
    long w = -1, d = -1;

    if (!isnum(v)) return argerr("STR");
    if (n >= 2 && isnum(P(2))) w = toint(numval(P(2)));
    if (n >= 3 && isnum(P(3))) d = toint(numval(P(3)));
    if (w >= 0 && d < 0) d = 0;
    return mkstr(numstrw(v, (int)w, (int)d));
}

/* STRZERO */
static V p_strzero(int base, int n)
{
    V v = P(1);
    long w = 10, d = 0, ls;
    int neg = 0;
    char *s;
    Sb b;

    if (!isnum(v)) return argerr("STRZERO");
    if (n >= 2) w = iarg(P(2), 10);
    if (n >= 3) d = iarg(P(3), 0);
    s = xstrip(numstrw(v, (int)w, (int)d));
    if (s[0] == '-') {
        neg = 1;
        s = xstrip(s + 1);
    }
    ls = (long)strlen(s);
    sb_init(&b);
    if (neg) sb_addc(&b, '-');
    while (ls < w - neg) {
        sb_addc(&b, '0');
        ls++;
    }
    sb_adds(&b, s);
    return sb_val(&b);
}

/* VAL */
static V p_val(int base, int n)
{
    (void)n;
    if (!isstr(P(1))) return argerr("VAL");
    return valnum(SS(P(1)));
}

/* ALLTRIM */
static V p_alltrim(int base, int n)
{
    (void)n;
    if (!isstr(P(1))) return argerr("ALLTRIM");
    return rx_strip(P(1), 0);
}

/* RTRIM TRIM */
static V p_rtrim(int base, int n)
{
    (void)n;
    if (!isstr(P(1))) return argerr("RTRIM");
    return rx_strip(P(1), 2);
}

/* LTRIM */
static V p_ltrim(int base, int n)
{
    (void)n;
    if (!isstr(P(1))) return argerr("LTRIM");
    return rx_strip(P(1), 1);
}

/* LEFT (pads with blanks like the REXX LEFT) */
static V p_left(int base, int n)
{
    long k;

    (void)n;
    if (!isstr(P(1)) || !isnum(P(2))) return argerr("LEFT");
    k = toint(numval(P(2)));
    if (k <= 0) return mkstrn("", 0);
    return rx_left(SS(P(1)), SL(P(1)), k);
}

/* RIGHT */
static V p_right(int base, int n)
{
    long k;
    V s;

    (void)n;
    if (!isstr(P(1)) || !isnum(P(2))) return argerr("RIGHT");
    k = toint(numval(P(2)));
    if (k <= 0) return mkstrn("", 0);
    s = P(1);
    if (k >= SL(s)) return s;
    return mkstrn(SS(s) + SL(s) - k, k);
}

/* SUBSTR */
static V p_substr(int base, int n)
{
    long ln = -1;

    if (!isstr(P(1)) || !isnum(P(2))) return argerr("SUBSTR");
    if (n >= 3) {
        if (!isnum(P(3))) return argerr("SUBSTR");
        ln = toint(numval(P(3)));
    }
    return xsubstr(P(1), toint(numval(P(2))), ln);
}

/* UPPER */
static V p_upper(int base, int n)
{
    (void)n;
    if (!isstr(P(1))) return argerr("UPPER");
    return caseconv(P(1), 1);
}

/* LOWER */
static V p_lower(int base, int n)
{
    (void)n;
    if (!isstr(P(1))) return argerr("LOWER");
    return caseconv(P(1), 0);
}

/* SPACE */
static V p_space(int base, int n)
{
    (void)n;
    if (!isnum(P(1))) return argerr("SPACE");
    return xaspace(toint(numval(P(1))));
}

/* REPLICATE REPL */
static V p_replicate(int base, int n)
{
    long k, i;
    Sb b;

    (void)n;
    if (!isstr(P(1)) || !isnum(P(2))) return argerr("REPLICATE");
    k = toint(numval(P(2)));
    if (k <= 0) return mkstrn("", 0);
    sb_init(&b);
    for (i = 0; i < k; i++) sb_addn(&b, SS(P(1)), SL(P(1)));
    return sb_val(&b);
}

/* source text of PADR/PADL/PADC: 0 if the type is wrong */
static int padsrc(V v, const char **p, long *l)
{
    char *z;

    if (isstr(v)) {
        *p = SS(v);
        *l = SL(v);
        return 1;
    }
    if (isnum(v)) z = xstrip(numstr(v));
    else if (isdate(v)) z = datestr(DJ(v));
    else return 0;
    *p = z;
    *l = (long)strlen(z);
    return 1;
}

/* which: 0 PADR, 1 PADL, 2 PADC */
static V padgen(int base, int n, int which, const char *nm)
{
    const char *s;
    long ls, k, tot, lft;
    int f = ' ';
    Sb b;

    if (!isnum(P(2))) return argerr(nm);
    if (!padsrc(P(1), &s, &ls)) return argerr(nm);
    k = toint(numval(P(2)));
    if (n >= 3 && isstr(P(3)) && SL(P(3)) > 0)
        f = (unsigned char)SS(P(3))[0];
    if (k <= 0) return mkstrn("", 0);
    if (ls >= k) return mkstrn(s, k);
    sb_init(&b);
    tot = k - ls;
    if (which == 0) {
        sb_addn(&b, s, ls);
        while (tot-- > 0) sb_addc(&b, f);
    } else if (which == 1) {
        while (tot-- > 0) sb_addc(&b, f);
        sb_addn(&b, s, ls);
    } else {
        lft = tot / 2;
        for (k = 0; k < lft; k++) sb_addc(&b, f);
        sb_addn(&b, s, ls);
        for (k = lft; k < tot; k++) sb_addc(&b, f);
    }
    return sb_val(&b);
}

static V p_padr(int base, int n)
{
    return padgen(base, n, 0, "PADR");
}

static V p_padl(int base, int n)
{
    return padgen(base, n, 1, "PADL");
}

static V p_padc(int base, int n)
{
    return padgen(base, n, 2, "PADC");
}

/* AT HB_AT */
static V p_at(int base, int n)
{
    long st = 1, en = 0;

    if (!isstr(P(1)) || !isstr(P(2))) return argerr("AT");
    if (n >= 3) st = iarg(P(3), 1);
    if (n >= 4) en = iarg(P(4), 0);
    return mkint(xat(P(1), P(2), st, en));
}

/* RAT */
static V p_rat(int base, int n)
{
    (void)n;
    if (!isstr(P(1)) || !isstr(P(2))) return argerr("RAT");
    return mkint(xrat(P(1), P(2)));
}

/* STRTRAN */
static V p_strtran(int base, int n)
{
    V sv_, fv_, rv_;
    const char *s, *f, *r = "";
    long sl, fl, rl = 0, st = 1, mx = 0, off = 0, cnt = 0, occ = 0;
    long p;
    Sb b;

    if (!isstr(P(1)) || !isstr(P(2))) return argerr("STRTRAN");
    sv_ = P(1);
    fv_ = P(2);
    s = SS(sv_);
    sl = SL(sv_);
    f = SS(fv_);
    fl = SL(fv_);
    if (n >= 3 && isstr(P(3))) {
        rv_ = P(3);
        r = SS(rv_);
        rl = SL(rv_);
    }
    if (n >= 4) st = iarg(P(4), 1);
    if (n >= 5) mx = iarg(P(5), 0);
    if (fl == 0) return sv_;
    sb_init(&b);
    for (;;) {
        p = rx_pos(f, fl, s + off, sl - off, 1);
        if (p == 0) break;
        occ++;
        if (occ < st) {
            sb_addn(&b, s + off, p - 1 + fl);
            off += p - 1 + fl;
            continue;
        }
        if (mx > 0 && cnt >= mx) break;
        sb_addn(&b, s + off, p - 1);
        sb_addn(&b, r, rl);
        off += p - 1 + fl;
        cnt++;
    }
    sb_addn(&b, s + off, sl - off);
    return sb_val(&b);
}

/* STUFF */
static V p_stuff(int base, int n)
{
    V s;
    long st, dl, ls;
    Sb b;
    V tail;

    (void)n;
    if (!isstr(P(1)) || !isnum(P(2)) || !isnum(P(3)) ||
        !isstr(P(4)))
        return argerr("STUFF");
    s = P(1);
    ls = SL(s);
    st = toint(numval(P(2)));
    dl = toint(numval(P(3)));
    if (st < 1) st = 1;
    if (st > ls + 1) st = ls + 1;
    sb_init(&b);
    sb_addn(&b, SS(s), st - 1);
    sb_addn(&b, SS(P(4)), SL(P(4)));
    tail = rx_substr(SS(s), ls, st + dl, -1);
    sb_addn(&b, SS(tail), SL(tail));
    return sb_val(&b);
}

/* CHR */
static V p_chr(int base, int n)
{
    long k;
    char c;

    (void)n;
    if (!isnum(P(1))) return argerr("CHR");
    k = toint(numval(P(1)));
    if (k < 0) k = k + 256;
    k = ((k % 256) + 256) % 256;
    c = (char)k;
    return mkstrn(&c, 1);
}

/* ASC */
static V p_asc(int base, int n)
{
    (void)n;
    if (!isstr(P(1))) return argerr("ASC");
    if (SL(P(1)) == 0) return mkint(0);
    return mkint((unsigned char)SS(P(1))[0]);
}

/* ABS */
static V p_abs(int base, int n)
{
    V v = P(1);
    double x;

    (void)n;
    if (!isnum(v)) return argerr("ABS");
    x = numval(v);
    if (x < 0.0) x = 0.0 - x;
    return mknum(x, numwid(v), numdec(v));
}

/* INT */
static V p_int(int base, int n)
{
    V v = P(1);
    double x;

    (void)n;
    if (!isnum(v)) return argerr("INT");
    if (icell(v)) return v;
    x = numval(v);
    if (x < 9.0e15 && x > -9.0e15) return mkint(toint(x));
    return mknum(x, 20, 0);
}

/* ROUND */
static V p_round(int base, int n)
{
    V v = P(1);
    long d;
    double f, x;
    char *s;

    (void)n;
    if (!isnum(v) || !isnum(P(2))) return argerr("ROUND");
    d = toint(numval(P(2)));
    if (d < 0) {
        f = fpow(10.0, 0.0 - (double)d);
        x = numval(v) / f;
        s = fmtfloat(x, 0);
        return mknum(strtod(s, NULL) * f, 10, 0);
    }
    if (icell(v)) return v;
    s = fmtfloat(numval(v), (int)d);
    return mknum(strtod(s, NULL), numwid(v), (int)d);
}

/* SQRT */
static V p_sqrt(int base, int n)
{
    (void)n;
    if (!isnum(P(1))) return argerr("SQRT");
    return mknum(lsqrt(numval(P(1))), 10, setdec);
}

/* EXP */
static V p_exp(int base, int n)
{
    (void)n;
    if (!isnum(P(1))) return argerr("EXP");
    return mknum(lexp(numval(P(1))), 10, setdec);
}

/* LOG */
static V p_log(int base, int n)
{
    (void)n;
    if (!isnum(P(1))) return argerr("LOG");
    if (numval(P(1)) <= 0.0) return mknum(0.0, 10, setdec);
    return mknum(lln(numval(P(1))), 10, setdec);
}

/* SIN */
static V p_sin(int base, int n)
{
    (void)n;
    if (!isnum(P(1))) return argerr("SIN");
    return mknum(lsin(numval(P(1))), 10, setdec);
}

/* COS */
static V p_cos(int base, int n)
{
    (void)n;
    if (!isnum(P(1))) return argerr("COS");
    return mknum(lcos(numval(P(1))), 10, setdec);
}

/* MAX */
static V p_max(int base, int n)
{
    int r;

    (void)n;
    r = cmpv(P(1), P(2));
    if (r == 2) return argerr("MAX");
    if (r >= 0) return P(1);
    return P(2);
}

/* MIN */
static V p_min(int base, int n)
{
    int r;

    (void)n;
    r = cmpv(P(1), P(2));
    if (r == 2) return argerr("MIN");
    if (r <= 0) return P(1);
    return P(2);
}

/* MOD */
static V p_mod(int base, int n)
{
    double x, y, q, r;
    long qi;
    int d;

    (void)n;
    if (!isnum(P(1)) || !isnum(P(2))) return argerr("MOD");
    y = numval(P(2));
    if (y == 0.0) return rterr("BASE", 1341, "Zero divisor", "MOD");
    x = numval(P(1));
    qi = toint(x / y);
    q = (double)qi;
    if ((x / y) < 0.0 && q != x / y) q = q - 1.0;
    r = x - q * y;
    d = numdec(P(1));
    if (numdec(P(2)) > d) d = numdec(P(2));
    return mknum(r, 10, d);
}

/* PCOUNT */
static V p_pcount(int base, int n)
{
    (void)base;
    (void)n;
    return mkint(fpc);
}

/* HB_NTOS NTOS */
static V p_ntos(int base, int n)
{
    (void)n;
    if (!isnum(P(1))) return argerr("HB_NTOS");
    return mkstr(ntos(P(1)));
}

/* HB_VALTOSTR */
static V p_valtostr(int base, int n)
{
    (void)n;
    return mkstr(vtostr(P(1)));
}

/* first character of a string argument, -1 if none */
static int firstch(V v)
{
    if (!isstr(v)) return -1;
    if (SL(v) == 0) return -1;
    return (unsigned char)SS(v)[0];
}

/* ISALPHA */
static V p_isalpha(int base, int n)
{
    int c = firstch(P(1));

    (void)n;
    if (c < 0) return FALSEV;
    return mklog(isalphac(c));
}

/* ISDIGIT */
static V p_isdigit(int base, int n)
{
    int c = firstch(P(1));

    (void)n;
    if (c < 0) return FALSEV;
    return mklog(isdigitc(c));
}

/* ISUPPER */
static V p_isupper(int base, int n)
{
    int c = firstch(P(1));

    (void)n;
    if (c < 0) return FALSEV;
    return mklog(isalphac(c) == 1 && c == toupper(c));
}

/* ISLOWER */
static V p_islower(int base, int n)
{
    int c = firstch(P(1));

    (void)n;
    if (c < 0) return FALSEV;
    return mklog(isalphac(c) == 1 && c == tolower(c));
}

static V p_isstring(int base, int n)
{
    (void)n;
    return mklog(isstr(P(1)));
}

static V p_isnumeric(int base, int n)
{
    (void)n;
    return mklog(isnum(P(1)));
}

static V p_isarray(int base, int n)
{
    (void)n;
    return mklog(isarr(P(1)));
}

static V p_ishash(int base, int n)
{
    (void)n;
    return mklog(ishash(P(1)));
}

static V p_islogical(int base, int n)
{
    (void)n;
    return mklog(islog(P(1)));
}

static V p_isdate(int base, int n)
{
    (void)n;
    return mklog(isdate(P(1)));
}

static V p_isblock(int base, int n)
{
    (void)n;
    return mklog(isblk(P(1)));
}

static V p_isobject(int base, int n)
{
    (void)n;
    return mklog(isobj(P(1)));
}

static V p_isnil(int base, int n)
{
    (void)n;
    return mklog(isnil(P(1)));
}

/* HB_DEFAULT: the first argument is passed by reference */
static V p_default(int base, int n)
{
    V r = P(1);

    (void)n;
    if (isref(r)) {
        if (isnil(r.u.r->v)) r.u.r->v = P(2);
    }
    return NILV;
}

/* RANDOM HB_RANDOM */
static V p_random(int base, int n)
{
    double x = (double)nextrand() / 2147483648.0;

    if (n >= 1 && isnum(P(1))) x = x * numval(P(1));
    return mknum(x, 10, 8);
}

/* HB_RANDOMINT */
static V p_randomint(int base, int n)
{
    long r = nextrand(), m, lo, hi;

    if (n == 0) return mkint(r);
    if (n == 1) {
        m = iarg(P(1), 1);
        if (m < 1) m = 1;
        return mkint(r % m);
    }
    lo = iarg(P(1), 0);
    hi = iarg(P(2), 1);
    if (hi < lo) hi = lo;
    return mkint(lo + r % (hi - lo + 1));
}

static V p_bitand(int base, int n)
{
    (void)n;
    return mkint(bitop(1, iarg(P(1), 0), iarg(P(2), 0)));
}

static V p_bitor(int base, int n)
{
    (void)n;
    return mkint(bitop(2, iarg(P(1), 0), iarg(P(2), 0)));
}

static V p_bitxor(int base, int n)
{
    (void)n;
    return mkint(bitop(3, iarg(P(1), 0), iarg(P(2), 0)));
}

/* HB_BITSHIFT */
static V p_bitshift(int base, int n)
{
    long k = iarg(P(2), 0), v = iarg(P(1), 0);
    unsigned long u;

    (void)n;
    if (k >= 0) {
        u = (unsigned long)v;
        while (k > 0) {
            u = u * 2UL;
            k--;
        }
        v = (long)u;
    } else {
        while (k < 0 && v != 0) {
            v = v / 2;
            k++;
        }
    }
    return mkint(v);
}

void lib1_init(void)
{
    addprim("LEN", p_len, 1, 1);
    addprim("EMPTY", p_empty, 1, 1);
    addprim("VALTYPE", p_valtype, 1, 1);
    addprim("STR", p_str, 1, 3);
    addprim("STRZERO", p_strzero, 1, 3);
    addprim("VAL", p_val, 1, 1);
    addprim("ALLTRIM", p_alltrim, 1, 1);
    addprim("RTRIM|TRIM", p_rtrim, 1, 2);
    addprim("LTRIM", p_ltrim, 1, 1);
    addprim("LEFT", p_left, 2, 2);
    addprim("RIGHT", p_right, 2, 2);
    addprim("SUBSTR", p_substr, 2, 3);
    addprim("UPPER", p_upper, 1, 1);
    addprim("LOWER", p_lower, 1, 1);
    addprim("SPACE", p_space, 1, 1);
    addprim("REPLICATE|REPL", p_replicate, 2, 2);
    addprim("PADR", p_padr, 2, 3);
    addprim("PADL", p_padl, 2, 3);
    addprim("PADC", p_padc, 2, 3);
    addprim("AT|HB_AT", p_at, 2, 4);
    addprim("RAT", p_rat, 2, 2);
    addprim("STRTRAN", p_strtran, 2, 5);
    addprim("STUFF", p_stuff, 4, 4);
    addprim("CHR", p_chr, 1, 1);
    addprim("ASC", p_asc, 1, 1);
    addprim("ABS", p_abs, 1, 1);
    addprim("INT", p_int, 1, 1);
    addprim("ROUND", p_round, 2, 2);
    addprim("SQRT", p_sqrt, 1, 1);
    addprim("EXP", p_exp, 1, 1);
    addprim("LOG", p_log, 1, 1);
    addprim("SIN", p_sin, 1, 1);
    addprim("COS", p_cos, 1, 1);
    addprim("MAX", p_max, 2, 2);
    addprim("MIN", p_min, 2, 2);
    addprim("MOD", p_mod, 2, 2);
    addprim("PCOUNT", p_pcount, 0, 0);
    addprim("HB_NTOS|NTOS", p_ntos, 1, 1);
    addprim("HB_VALTOSTR", p_valtostr, 1, 1);
    addprim("ISALPHA", p_isalpha, 1, 1);
    addprim("ISDIGIT", p_isdigit, 1, 1);
    addprim("ISUPPER", p_isupper, 1, 1);
    addprim("ISLOWER", p_islower, 1, 1);
    addprim("HB_ISSTRING", p_isstring, 1, 1);
    addprim("HB_ISNUMERIC", p_isnumeric, 1, 1);
    addprim("HB_ISARRAY", p_isarray, 1, 1);
    addprim("HB_ISHASH", p_ishash, 1, 1);
    addprim("HB_ISLOGICAL", p_islogical, 1, 1);
    addprim("HB_ISDATE", p_isdate, 1, 1);
    addprim("HB_ISBLOCK", p_isblock, 1, 1);
    addprim("HB_ISOBJECT", p_isobject, 1, 1);
    addprim("HB_ISNIL", p_isnil, 1, 1);
    addprim("HB_DEFAULT", p_default, 2, 2);
    addprim("RANDOM|HB_RANDOM", p_random, 0, 1);
    addprim("HB_RANDOMINT", p_randomint, 0, 2);
    addprim("HB_BITAND", p_bitand, 2, 2);
    addprim("HB_BITOR", p_bitor, 2, 2);
    addprim("HB_BITXOR", p_bitxor, 2, 2);
    addprim("HB_BITSHIFT", p_bitshift, 2, 2);
}
