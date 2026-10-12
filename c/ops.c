/* ops.c -- arithmetic, comparison, conversion, dates, math.
   C port of src/40_ops.crexx */
#include "xb.h"

int gy = 0, gm = 0, gd = 0;

void jdn2ymd(long j)
{
    long a, b, c, d, e, m;
    a = j + 32044L;
    b = (4 * a + 3) / 146097L;
    c = a - 146097L * b / 4;
    d = (4 * c + 3) / 1461L;
    e = c - 1461L * d / 4;
    m = (5 * e + 2) / 153;
    gd = (int)(e - (153 * m + 2) / 5 + 1);
    gm = (int)(m + 3 - 12 * (m / 10));
    gy = (int)(100 * b + d - 4800 + m / 10);
}

long ymd2jdn(int y, int m, int d)
{
    long a, yy, mm;
    a = (14 - m) / 12;
    yy = y + 4800 - a;
    mm = m + 12 * a - 3;
    return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100
           + yy / 400 - 32045L;
}

char *pad2(int n)
{
    if (n < 10) return xfmt("0%d", n);
    return xfmt("%d", n);
}

char *pad4(int n)
{
    return xfmt("%04d", n);
}

char *datefmt(long j, const char *fmt)
{
    Sb b;
    long i, n;
    char *u, *t;
    sb_init(&b);
    u = xupper(fmt);
    n = (long)strlen(fmt);
    if (j == 0) {
        for (i = 0; i < n; i++) {
            if (u[i] == 'Y' || u[i] == 'M' || u[i] == 'D')
                sb_addc(&b, ' ');
            else sb_addc(&b, fmt[i]);
        }
        return xstrdup(sb_str(&b));
    }
    jdn2ymd(j);
    i = 0;
    while (i < n) {
        if (strncmp(u + i, "YYYY", 4) == 0) {
            t = pad4(gy);
            sb_adds(&b, t);
            i += 4;
        } else if (strncmp(u + i, "YY", 2) == 0) {
            if (setcentury == 1) t = pad4(gy);
            else t = pad2(gy % 100);
            sb_adds(&b, t);
            i += 2;
        } else if (strncmp(u + i, "MM", 2) == 0) {
            t = pad2(gm);
            sb_adds(&b, t);
            i += 2;
        } else if (strncmp(u + i, "DD", 2) == 0) {
            t = pad2(gd);
            sb_adds(&b, t);
            i += 2;
        } else {
            sb_addc(&b, fmt[i]);
            i++;
        }
    }
    return xstrdup(sb_str(&b));
}

char *datestr(long j)
{
    return datefmt(j, setdatefmt);
}

char *dtos(long j)
{
    if (j == 0) return xstrdup("        ");
    jdn2ymd(j);
    return xfmt("%04d%02d%02d", gy, gm, gd);
}

long parsedate(const char *s, const char *fmt)
{
    char *u = xupper(fmt);
    char ord[8];
    long nums[16], lens[16];
    int cnt = 0, no = 0, k, y = 0, m = 0, d = 0;
    long i, n;
    i = 0;
    n = (long)strlen(u);
    while (i < n && no < 7) {
        if (strncmp(u + i, "YYYY", 4) == 0) {
            ord[no++] = 'Y';
            i += 4;
        } else if (strncmp(u + i, "YY", 2) == 0) {
            ord[no++] = 'y';
            i += 2;
        } else if (strncmp(u + i, "MM", 2) == 0) {
            ord[no++] = 'M';
            i += 2;
        } else if (strncmp(u + i, "DD", 2) == 0) {
            ord[no++] = 'D';
            i += 2;
        } else i++;
    }
    ord[no] = 0;
    i = 0;
    n = (long)strlen(s);
    while (i < n && cnt < 16) {
        if (s[i] >= '0' && s[i] <= '9') {
            long j = i, v = 0;
            while (j < n && s[j] >= '0' && s[j] <= '9') {
                if (v < 100000000L) v = v * 10 + (s[j] - '0');
                j++;
            }
            nums[cnt] = v;
            lens[cnt] = j - i;
            cnt++;
            i = j;
        } else i++;
    }
    if (cnt < 3) return 0;
    for (k = 0; k < 3; k++) {
        char f = k < no ? ord[k] : ' ';
        if (f == 'Y') y = (int)nums[k];
        else if (f == 'y') {
            y = (int)nums[k];
            if (lens[k] <= 2) {
                int cen = setepoch - setepoch % 100;
                y = cen + y;
                if (y < setepoch) y += 100;
            }
        } else if (f == 'M') m = (int)nums[k];
        else if (f == 'D') d = (int)nums[k];
    }
    if (m < 1 || m > 12 || d < 1 || d > 31) return 0;
    {
        long j = ymd2jdn(y, m, d);
        jdn2ymd(j);
        if (gd != d || gm != m) return 0;
        return j;
    }
}

V argerr(const char *op)
{
    rterr("BASE", 1066, "Argument error", op);
    return NILV;
}

/* ---------------------------------------- numbers */
char *padleft(const char *s, int w)
{
    int n = (int)strlen(s);
    char *r;
    if (n >= w) return xstrdup(s);
    r = (char *)xmalloc(w + 1);
    memset(r, ' ', (size_t)(w - n));
    memcpy(r + (w - n), s, (size_t)n + 1);
    return r;
}

/* decimal digits of a number as the cREXX FORMAT function shows
   them: 16 significant digits, half up rounding at d decimals,
   exponent form for very large/small values */
char *fmtfloat(double x, int d)
{
    char buf[64], dig[40];
    Sb out;
    int neg = 0, k = 0, e10, i, n, zeros, sci, ilen, carry;
    char *ip, *fp, *m;
    if (x < 0) {
        neg = 1;
        x = -x;
    }
    if (x == 0.0) {
        dig[0] = '0';
        dig[1] = 0;
        k = 1;
        e10 = 0;
    } else {
        sprintf(buf, "%.15e", x);
        /* d.ddddddddddddddde+XX */
        dig[0] = buf[0];
        for (i = 0; i < 15; i++) dig[1 + i] = buf[2 + i];
        dig[16] = 0;
        e10 = atoi(buf + 18);
        k = 16;
        while (k > 1 && dig[k - 1] == '0') k--;
        dig[k] = 0;
    }
    sci = 0;
    if (x != 0.0) {
        if (e10 >= 0) {
            n = e10 + 1;
            if (k <= n) {
                zeros = n - k;
                if (zeros > 17) sci = 1;
            }
        } else if (e10 < -6) sci = 1;
    }
    sb_init(&out);
    if (sci) {
        int need = 1 + d;
        m = (char *)xmalloc(need + 2 > k + 2 ? need + 2 : k + 2);
        for (i = 0; i < need + 1; i++) m[i] = i < k ? dig[i] : '0';
        m[need + 1] = 0;
        /* m has need+1 digits; the last is the rounding digit */
        carry = m[need] >= '5';
        m[need] = 0;
        if (carry) {
            for (i = need - 1; i >= 0; i--) {
                if (m[i] == '9') m[i] = '0';
                else {
                    m[i]++;
                    carry = 0;
                    break;
                }
            }
            if (carry) {
                /* 9.99 -> 10.0 : mantissa 1.00, exponent + 1 */
                m[0] = '1';
                for (i = 1; i < need; i++) m[i] = '0';
                e10++;
            }
        }
        if (neg) sb_addc(&out, '-');
        sb_addc(&out, m[0]);
        if (d > 0) {
            sb_addc(&out, '.');
            sb_addn(&out, m + 1, d);
        }
        sb_adds(&out, e10 < 0 ? "E-" : "E+");
        sb_adds(&out, xfmt("%d", e10 < 0 ? -e10 : e10));
        return xstrdup(sb_str(&out));
    }
    /* plain notation */
    if (x == 0.0) {
        ip = xstrdup("0");
        fp = xstrdup("");
    } else if (e10 >= 0) {
        n = e10 + 1;
        if (k <= n) {
            ip = (char *)xmalloc(n + 1);
            memcpy(ip, dig, (size_t)k);
            memset(ip + k, '0', (size_t)(n - k));
            ip[n] = 0;
            fp = xstrdup("");
        } else {
            ip = xstrndup(dig, n);
            fp = xstrdup(dig + n);
        }
    } else {
        int lz = -e10 - 1;
        ip = xstrdup("0");
        fp = (char *)xmalloc(lz + k + 1);
        memset(fp, '0', (size_t)lz);
        memcpy(fp + lz, dig, (size_t)k + 1);
    }
    ilen = (int)strlen(ip);
    n = (int)strlen(fp);
    {
        /* all digits in one buffer: ip + first d of fp */
        char *all = (char *)xmalloc(ilen + d + 3);
        int dl = 0, allz = 1;
        carry = 0;
        memcpy(all + 1, ip, (size_t)ilen);
        for (i = 0; i < d; i++)
            all[1 + ilen + i] = i < n ? fp[i] : '0';
        all[1 + ilen + d] = 0;
        all[0] = '0';
        if (n > d && fp[d] >= '5') carry = 1;
        if (carry) {
            for (i = ilen + d; i >= 1; i--) {
                if (all[i] == '9') all[i] = '0';
                else {
                    all[i]++;
                    carry = 0;
                    break;
                }
            }
            if (carry) all[0] = '1';
        }
        for (i = 0; all[i]; i++)
            if (all[i] != '0') allz = 0;
        dl = (all[0] == '1') ? 0 : 1;
        if (neg && !allz) sb_addc(&out, '-');
        sb_addn(&out, all + dl, ilen + (all[0] == '1' ? 1 : 0));
        if (d > 0) {
            sb_addc(&out, '.');
            sb_addn(&out, all + 1 + ilen, d);
        }
    }
    return xstrdup(sb_str(&out));
}

char *numstr(V v)
{
    char *s;
    if (v.f) s = xfmt("%.0f", v.u.n);
    else s = fmtfloat(v.u.n, v.d);
    return padleft(s, v.f ? 10 : v.w);
}

char *numstrw(V v, int w, int d)
{
    char *s;
    if (w < 0) {
        w = numwid(v);
        if (d < 0) d = numdec(v);
    }
    if (d < 0) d = 0;
    s = fmtfloat(numval(v), d);
    if ((int)strlen(s) > w) {
        char *r = (char *)xmalloc(w + 1);
        memset(r, '*', (size_t)w);
        r[w] = 0;
        return r;
    }
    return padleft(s, w);
}

char *ntos(V v)
{
    if (v.f) return xfmt("%.0f", v.u.n);
    return fmtfloat(v.u.n, v.d);
}

V numadd(V a, V b)
{
    int d;
    if (a.f && b.f) return mkint(toint(a.u.n + b.u.n));
    d = numdec(a);
    if (numdec(b) > d) d = numdec(b);
    return mknum(numval(a) + numval(b), 10, d);
}

V numsub(V a, V b)
{
    int d;
    if (a.f && b.f) return mkint(toint(a.u.n - b.u.n));
    d = numdec(a);
    if (numdec(b) > d) d = numdec(b);
    return mknum(numval(a) - numval(b), 10, d);
}

V nummul(V a, V b)
{
    double f;
    if (a.f && b.f) {
        f = a.u.n * b.u.n;
        if (f < 9.0e18 && f > -9.0e18) return mkint(toint(f));
        return mknum(f, 20, 0);
    }
    return mknum(numval(a) * numval(b), 10,
                 numdec(a) + numdec(b));
}

V numdiv(V a, V b)
{
    double y = numval(b);
    if (y == 0.0) {
        rterr("BASE", 1340, "Zero divisor", "/");
        return NILV;
    }
    return mknum(numval(a) / y, 10, setdec);
}

V nummod(V a, V b)
{
    double x, y, r;
    long q;
    int d;
    if (a.f && b.f) {
        long ib = toint(b.u.n);
        if (ib == 0) {
            rterr("BASE", 1341, "Zero divisor", "%");
            return NILV;
        }
        return mkint(toint(a.u.n) % ib);
    }
    y = numval(b);
    if (y == 0.0) {
        rterr("BASE", 1341, "Zero divisor", "%");
        return NILV;
    }
    x = numval(a);
    q = toint(x / y);
    r = x - (double)q * y;
    d = numdec(a);
    if (numdec(b) > d) d = numdec(b);
    return mknum(r, 10, d);
}

double fpow(double x, double y)
{
    long yi;
    if (y == 0.0) return 1.0;
    if (x == 0.0) return 0.0;
    yi = toint(y);
    if ((double)yi == y) {
        int neg = 0;
        long e = yi;
        double r = 1.0, bb = x;
        if (e < 0) {
            neg = 1;
            e = -e;
        }
        while (e > 0) {
            if (e % 2 == 1) r = r * bb;
            bb = bb * bb;
            e = e / 2;
        }
        if (neg) r = 1.0 / r;
        return r;
    }
    if (x < 0.0) return 0.0;
    return lexp(y * lln(x));
}

V numpow(V a, V b)
{
    return mknum(fpow(numval(a), numval(b)), 10, setdec);
}

V numneg(V a)
{
    V r;
    if (a.f) return mkint(toint(-a.u.n));
    r = a;
    r.u.n = 0.0 - a.u.n;
    return r;
}

/* ------------------------------------------------------ comparison */
int strcmp_x(const char *a, long an, const char *b, long bn)
{
    long n = an < bn ? an : bn, i;
    for (i = 0; i < n; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca != cb) return ca < cb ? -1 : 1;
    }
    if (an < bn) return -1;
    if (an > bn) return 1;
    return 0;
}

int cmpv(V a, V b)
{
    if (a.t == T_NUM && b.t == T_NUM) {
        if (a.u.n < b.u.n) return -1;
        if (a.u.n > b.u.n) return 1;
        return 0;
    }
    if (a.t == T_STR && b.t == T_STR)
        return strcmp_x(SS(a), SL(a), SS(b), SL(b));
    if (a.t == T_DATE && b.t == T_DATE) {
        if (a.u.j < b.u.j) return -1;
        if (a.u.j > b.u.j) return 1;
        return 0;
    }
    if (a.t == T_LOG && b.t == T_LOG) {
        if (a.u.j < b.u.j) return -1;
        if (a.u.j > b.u.j) return 1;
        return 0;
    }
    return 2;
}

int equalv(V a, V b)
{
    int r;
    if (a.t == T_NIL && b.t == T_NIL) return 1;
    if (a.t == T_NIL || b.t == T_NIL) return 0;
    if (a.t == T_STR && b.t == T_STR) {
        long i, la = SL(a), lb = SL(b);
        if (setexact == 1) {
            while (la > 0 && SS(a)[la - 1] == ' ') la--;
            while (lb > 0 && SS(b)[lb - 1] == ' ') lb--;
            return la == lb && memcmp(SS(a), SS(b), (size_t)la) == 0;
        }
        if (lb == 0) return 1;
        for (i = 0; i < lb; i++) {
            char ca = i < la ? SS(a)[i] : ' ';
            if (ca != SS(b)[i]) return 0;
        }
        return 1;
    }
    r = cmpv(a, b);
    if (r == 2) {
        if (a.t == T_ARR && b.t == T_ARR) return a.u.a == b.u.a;
        if (a.t == T_HASH && b.t == T_HASH) return a.u.h == b.u.h;
        if (a.t == T_OBJ && b.t == T_OBJ) return a.u.o == b.u.o;
        if (a.t == T_BLK && b.t == T_BLK) return a.u.b == b.u.b;
        argerr("=");
        return 0;
    }
    return r == 0;
}

int exactv(V a, V b)
{
    int r;
    if (a.t == T_NIL && b.t == T_NIL) return 1;
    if (a.t == T_NIL || b.t == T_NIL) return 0;
    if (a.t == T_STR && b.t == T_STR)
        return SL(a) == SL(b) &&
               memcmp(SS(a), SS(b), (size_t)SL(a)) == 0;
    r = cmpv(a, b);
    if (r == 2) {
        if (vsame(a, b)) return 1;
        if (a.t == T_ARR || a.t == T_HASH || a.t == T_OBJ ||
            a.t == T_BLK) return 0;
        argerr("==");
        return 0;
    }
    return r == 0;
}

/* ------------------------------------------------------- operators */
V binadd(V a, V b)
{
    if (a.t == T_NUM && b.t == T_NUM) return numadd(a, b);
    if (a.t == T_STR && b.t == T_STR) {
        Sb s;
        sb_init(&s);
        sb_addn(&s, SS(a), SL(a));
        sb_addn(&s, SS(b), SL(b));
        return sb_val(&s);
    }
    if (a.t == T_DATE && b.t == T_NUM)
        return mkdate(DJ(a) + toint(numval(b)));
    if (a.t == T_NUM && b.t == T_DATE)
        return mkdate(DJ(b) + toint(numval(a)));
    return argerr("+");
}

V binsub(V a, V b)
{
    if (a.t == T_NUM && b.t == T_NUM) return numsub(a, b);
    if (a.t == T_STR && b.t == T_STR) {
        long n = SL(a), i;
        Sb s;
        while (n > 0 && SS(a)[n - 1] == ' ') n--;
        sb_init(&s);
        sb_addn(&s, SS(a), n);
        sb_addn(&s, SS(b), SL(b));
        for (i = n; i < SL(a); i++) sb_addc(&s, ' ');
        return sb_val(&s);
    }
    if (a.t == T_DATE && b.t == T_NUM)
        return mkdate(DJ(a) - toint(numval(b)));
    if (a.t == T_DATE && b.t == T_DATE)
        return mkint(DJ(a) - DJ(b));
    return argerr("-");
}

V binop(int t, V a, V b)
{
    if (t == K_ADD) return binadd(a, b);
    if (t == K_SUB) return binsub(a, b);
    if (t == K_MUL) {
        if (a.t == T_NUM && b.t == T_NUM) return nummul(a, b);
        return argerr("*");
    }
    if (t == K_DIV) {
        if (a.t == T_NUM && b.t == T_NUM) return numdiv(a, b);
        return argerr("/");
    }
    if (t == K_MOD) {
        if (a.t == T_NUM && b.t == T_NUM) return nummod(a, b);
        return argerr("%");
    }
    if (t == K_POW) {
        if (a.t == T_NUM && b.t == T_NUM) return numpow(a, b);
        return argerr("^");
    }
    return NILV;
}

V relop(int t, V a, V b)
{
    int r;
    if (t == K_EQ) return mklog(equalv(a, b));
    if (t == K_EXEQ) return mklog(exactv(a, b));
    if (t == K_NE) return mklog(!equalv(a, b));
    r = cmpv(a, b);
    if (r == 2) {
        if (t == K_LT) return argerr("<");
        if (t == K_LE) return argerr("<=");
        if (t == K_GT) return argerr(">");
        return argerr(">=");
    }
    if (t == K_LT) return mklog(r < 0);
    if (t == K_LE) return mklog(r <= 0);
    if (t == K_GT) return mklog(r > 0);
    return mklog(r >= 0);
}

/* ------------------------------------------------ value to string */
char typech(V v)
{
    switch (v.t) {
    case T_NIL: return 'U';
    case T_NUM: return 'N';
    case T_STR: return 'C';
    case T_LOG: return 'L';
    case T_DATE: return 'D';
    case T_ARR: return 'A';
    case T_HASH: return 'H';
    case T_BLK: return 'B';
    case T_OBJ: return 'O';
    }
    return 'U';
}

char *valstr(V v)
{
    switch (v.t) {
    case T_NIL: return "NIL";
    case T_NUM: return numstr(v);
    case T_LOG: return v.u.j ? ".T." : ".F.";
    case T_STR: return SS(v);
    case T_DATE: return datestr(DJ(v));
    }
    argerr("?");
    return "";
}

char *vtostr(V v)
{
    switch (v.t) {
    case T_NIL: return "NIL";
    case T_NUM: return ntos(v);
    case T_LOG: return v.u.j ? ".T." : ".F.";
    case T_STR: return SS(v);
    case T_DATE: return datestr(DJ(v));
    case T_ARR:
        return xfmt("{Array of %ld items}", arrlen(v));
    case T_HASH:
        return xfmt("{Hash of %ld items}", hashlen(v));
    case T_BLK: return "{|| ... }";
    case T_OBJ: return xfmt("%s Object", clname[v.u.o->cls]);
    }
    return "NIL";
}

/* ------------------------------------------- math (from the Lisp) */
double lsqrt(double x)
{
    double m, r;
    int e, i;
    if (x <= 0.0) return 0.0;
    m = x;
    e = 0;
    while (m >= 4.0) {
        m = m / 4.0;
        e++;
    }
    while (m < 0.25) {
        m = m * 4.0;
        e--;
    }
    r = (1.0 + m) / 2.0;
    for (i = 0; i < 8; i++) r = (r + m / r) / 2.0;
    while (e > 0) {
        r = r * 2.0;
        e--;
    }
    while (e < 0) {
        r = r / 2.0;
        e++;
    }
    return r;
}

double lsin(double x)
{
    double pi = 3.14159265358979323846;
    double twopi = 6.28318530717958647692;
    double r = x, r2, term, sum;
    int k;
    if (r > twopi || r < 0.0 - twopi) {
        long q = (long)(r / twopi);
        r = r - (double)q * twopi;
    }
    if (r > pi) r = r - twopi;
    if (r < 0.0 - pi) r = r + twopi;
    if (r > pi / 2.0) r = pi - r;
    if (r < 0.0 - pi / 2.0) r = 0.0 - pi - r;
    r2 = r * r;
    term = r;
    sum = r;
    for (k = 1; k <= 12; k++) {
        term = 0.0 - term * r2 / ((double)(2 * k) *
                                  (double)(2 * k + 1));
        sum = sum + term;
    }
    return sum;
}

double lcos(double x)
{
    return lsin(x + 1.57079632679489661923);
}

double lln(double x)
{
    double m, z, z2, term, sum;
    int e = 0, k;
    if (x <= 0.0) return 0.0;
    m = x;
    while (m >= 2.0) {
        m = m / 2.0;
        e++;
    }
    while (m < 0.5) {
        m = m * 2.0;
        e--;
    }
    z = (m - 1.0) / (m + 1.0);
    z2 = z * z;
    term = z;
    sum = z;
    for (k = 1; k <= 30; k++) {
        term = term * z2;
        sum = sum + term / (double)(2 * k + 1);
    }
    return 2.0 * sum + (double)e * 0.69314718055994530942;
}

double lexp(double x)
{
    int k = 0, i;
    double r = x, term = 1.0, sum = 1.0;
    while (r > 0.5 || r < -0.5) {
        r = r / 2.0;
        k++;
    }
    for (i = 1; i <= 20; i++) {
        term = term * r / (double)i;
        sum = sum + term;
    }
    for (i = 0; i < k; i++) sum = sum * sum;
    return sum;
}
