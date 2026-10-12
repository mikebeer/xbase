/* lib2.c -- builtin functions, part 2 (port of lib2.tpl):
   dates and time, arrays, hashes, evaluation, environment, misc */
#include "xb.h"

/* helpers defined in io.c */
V rx_substr(const char *s, long sl, long st, long ln);
V rx_strip(V s, int mode);
long rx_pos(const char *f, long fl, const char *h, long hl,
            long from);
V xaspace(long n);
char *vtoexp(V v);
V strformat(int base, int n);

#define P(k) fv[base + (k)]

static const char *dnames[] = { "Sunday", "Monday", "Tuesday",
    "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *mnames[] = { "January", "February", "March",
    "April", "May", "June", "July", "August", "September",
    "October", "November", "December" };

static struct tm *nowtm(void)
{
    time_t t = time(NULL);

    return localtime(&t);
}

/* DATE */
static V p_date(int base, int n)
{
    struct tm *tm = nowtm();

    (void)base;
    (void)n;
    return mkdate(ymd2jdn(tm->tm_year + 1900, tm->tm_mon + 1,
                          tm->tm_mday));
}

/* TIME */
static V p_time(int base, int n)
{
    struct tm *tm = nowtm();

    (void)base;
    (void)n;
    return mkstr(xfmt("%02d:%02d:%02d", tm->tm_hour, tm->tm_min,
                      tm->tm_sec));
}

/* SECONDS */
static V p_seconds(int base, int n)
{
    struct tm *tm = nowtm();
    long secs = tm->tm_hour * 3600L + tm->tm_min * 60L + tm->tm_sec;

    (void)base;
    (void)n;
    return mknum(secs * 1.0, 10, 2);
}

/* HB_MILLISECONDS: milliseconds since midnight; the sub-second
   part comes from clock() since the last change of time() */
static V p_millis(int base, int n)
{
    static time_t lastt = 0;
    static clock_t lastc = 0;
    time_t t = time(NULL);
    struct tm *tm;
    long ms, sub;

    (void)base;
    (void)n;
    if (t != lastt) {
        lastt = t;
        lastc = clock();
    }
    sub = (long)(((double)(clock() - lastc) * 1000.0) /
                 (double)CLOCKS_PER_SEC);
    if (sub < 0) sub = 0;
    if (sub > 999) sub = 999;
    tm = localtime(&t);
    ms = (tm->tm_hour * 3600L + tm->tm_min * 60L + tm->tm_sec) *
         1000L + sub;
    return mkint(ms);
}

/* YEAR */
static V p_year(int base, int n)
{
    (void)n;
    if (!isdate(P(1))) return argerr("YEAR");
    if (DJ(P(1)) == 0) return mkint(0);
    jdn2ymd(DJ(P(1)));
    return mkint(gy);
}

/* MONTH */
static V p_month(int base, int n)
{
    (void)n;
    if (!isdate(P(1))) return argerr("MONTH");
    if (DJ(P(1)) == 0) return mkint(0);
    jdn2ymd(DJ(P(1)));
    return mkint(gm);
}

/* DAY */
static V p_day(int base, int n)
{
    (void)n;
    if (!isdate(P(1))) return argerr("DAY");
    if (DJ(P(1)) == 0) return mkint(0);
    jdn2ymd(DJ(P(1)));
    return mkint(gd);
}

/* DOW */
static V p_dow(int base, int n)
{
    (void)n;
    if (!isdate(P(1))) return argerr("DOW");
    if (DJ(P(1)) == 0) return mkint(0);
    return mkint((DJ(P(1)) + 1) % 7 + 1);
}

/* CDOW */
static V p_cdow(int base, int n)
{
    long k;

    (void)n;
    if (!isdate(P(1))) return argerr("CDOW");
    if (DJ(P(1)) == 0) return mkstr("");
    k = (DJ(P(1)) + 1) % 7 + 1;
    return mkstr(dnames[k - 1]);
}

/* CMONTH */
static V p_cmonth(int base, int n)
{
    (void)n;
    if (!isdate(P(1))) return argerr("CMONTH");
    if (DJ(P(1)) == 0) return mkstr("");
    jdn2ymd(DJ(P(1)));
    return mkstr(mnames[gm - 1]);
}

/* DTOC */
static V p_dtoc(int base, int n)
{
    (void)n;
    if (!isdate(P(1))) return argerr("DTOC");
    return mkstr(datestr(DJ(P(1))));
}

/* DTOS */
static V p_dtos(int base, int n)
{
    (void)n;
    if (!isdate(P(1))) return argerr("DTOS");
    return mkstr(dtos(DJ(P(1))));
}

/* CTOD */
static V p_ctod(int base, int n)
{
    char *fmt = xstrdup(setdatefmt), *u;
    Sb b;
    long i, l;

    (void)n;
    if (!isstr(P(1))) return argerr("CTOD");
    u = xupper(fmt);
    if (setcentury == 1 && strstr(u, "YYYY") == NULL) {
        sb_init(&b);
        l = (long)strlen(fmt);
        for (i = 0; i < l; i++) {
            if (fmt[i] == 'y' && i + 1 < l && fmt[i + 1] == 'y') {
                sb_adds(&b, "yyyy");
                i++;
            } else sb_addc(&b, fmt[i]);
        }
        fmt = sb_str(&b);
    }
    return mkdate(parsedate(SS(P(1)), fmt));
}

/* the number in t[st .. st+len-1] (REXX: substr(...) as .int) */
static long subint(const char *t, int st, int len)
{
    char *p = xstrndup(t + st, len);

    return strtol(p, NULL, 10);
}

/* date from "YYYYMMDD" */
static V datelit(const char *t)
{
    long y, m, d;

    if (strlen(t) < 8) return mkdate(0);
    y = subint(t, 0, 4);
    m = subint(t, 4, 2);
    d = subint(t, 6, 2);
    if (y == 0 && m == 0 && d == 0) return mkdate(0);
    return mkdate(ymd2jdn((int)y, (int)m, (int)d));
}

/* STOD HB_STOD */
static V p_stod(int base, int n)
{
    if (n < 1) return mkdate(0);
    if (!isstr(P(1))) return argerr("STOD");
    return datelit(xstrip(SS(P(1))));
}

/* HB_DATE */
static V p_hbdate(int base, int n)
{
    (void)n;
    return mkdate(ymd2jdn((int)iarg(P(1), 1900), (int)iarg(P(2), 1),
                          (int)iarg(P(3), 1)));
}

/* ARRAY */
static V p_array(int base, int n)
{
    if (n == 0) return mkarray(0);
    return mkmulti(base, 1, n);
}

/* AADD */
static V p_aadd(int base, int n)
{
    (void)n;
    if (!isarr(P(1))) return argerr("AADD");
    arrpush(P(1), P(2));
    return P(2);
}

/* ADEL */
static V p_adel(int base, int n)
{
    V a = P(1);
    long p, len, i;

    (void)n;
    if (!isarr(a) || !isnum(P(2))) return argerr("ADEL");
    p = toint(numval(P(2)));
    len = arrlen(a);
    if (p < 1 || p > len) return a;
    for (i = p; i <= len - 1; i++) arrset(a, i, arrget(a, i + 1));
    arrset(a, len, NILV);
    return a;
}

/* AINS */
static V p_ains(int base, int n)
{
    V a = P(1);
    long p, len, i;

    (void)n;
    if (!isarr(a) || !isnum(P(2))) return argerr("AINS");
    p = toint(numval(P(2)));
    len = arrlen(a);
    if (p < 1 || p > len) return a;
    for (i = len; i >= p + 1; i--) arrset(a, i, arrget(a, i - 1));
    arrset(a, p, NILV);
    return a;
}

/* HB_AINS */
static V p_hbains(int base, int n)
{
    V a = P(1);
    long p, len, i;

    if (!isarr(a) || !isnum(P(2))) return argerr("HB_AINS");
    p = toint(numval(P(2)));
    len = arrlen(a);
    if (n >= 4) {
        if (truthy(P(4))) {
            arrresize(a, len + 1);
            len = len + 1;
        }
    }
    if (p < 1 || p > len) return a;
    for (i = len; i >= p + 1; i--) arrset(a, i, arrget(a, i - 1));
    arrset(a, p, P(3));
    return a;
}

/* HB_ADEL */
static V p_hbadel(int base, int n)
{
    V a = P(1);
    long p, len, i;

    if (!isarr(a) || !isnum(P(2))) return argerr("HB_ADEL");
    p = toint(numval(P(2)));
    len = arrlen(a);
    if (p < 1 || p > len) return a;
    for (i = p; i <= len - 1; i++) arrset(a, i, arrget(a, i + 1));
    if (n >= 3) {
        if (truthy(P(3))) {
            arrresize(a, len - 1);
            return a;
        }
    }
    arrset(a, len, NILV);
    return a;
}

/* ASIZE */
static V p_asize(int base, int n)
{
    long k;

    (void)n;
    if (!isarr(P(1)) || !isnum(P(2))) return argerr("ASIZE");
    k = toint(numval(P(2)));
    if (k < 0) k = 0;
    arrresize(P(1), k);
    return P(1);
}

/* ATAIL */
static V p_atail(int base, int n)
{
    (void)n;
    if (!isarr(P(1))) return argerr("ATAIL");
    if (arrlen(P(1)) == 0) return NILV;
    return arrget(P(1), arrlen(P(1)));
}

/* ASCAN HB_ASCAN */
static V p_ascan(int base, int n)
{
    V a = P(1), x = P(2), el, r;
    long st = 1, len, cnt, en, i;

    if (!isarr(a)) return argerr("ASCAN");
    if (n >= 3) st = iarg(P(3), 1);
    len = arrlen(a);
    cnt = len - st + 1;
    if (n >= 4) cnt = iarg(P(4), cnt);
    if (st < 1) st = 1;
    en = st + cnt - 1;
    if (en > len) en = len;
    for (i = st; i <= en; i++) {
        el = arrget(a, i);
        if (isblk(x)) {
            r = evalblock1(x, el);
            if (ctl != 0) return NILV;
            if (truthy(r)) return mkint(i);
        } else {
            if (isstr(x) && isstr(el)) {
                if (SL(x) == SL(el) &&
                    memcmp(SS(x), SS(el), SL(x)) == 0)
                    return mkint(i);
            } else {
                if (keyeq(el, x)) return mkint(i);
                if (isnum(el) && isnum(x)) {
                    if (numval(el) == numval(x)) return mkint(i);
                }
            }
        }
    }
    return mkint(0);
}

/* AEVAL */
static V p_aeval(int base, int n)
{
    V a = P(1), b = P(2);
    long st = 1, len, cnt, en, i;

    if (!isarr(a) || !isblk(b)) return argerr("AEVAL");
    if (n >= 3) st = iarg(P(3), 1);
    len = arrlen(a);
    cnt = len - st + 1;
    if (n >= 4) cnt = iarg(P(4), cnt);
    if (st < 1) st = 1;
    en = st + cnt - 1;
    if (en > len) en = len;
    for (i = st; i <= en; i++) {
        evalblock2(b, arrget(a, i), mkint(i));
        if (ctl != 0) return NILV;
    }
    return a;
}

/* ASORT */
static V p_asort(int base, int n)
{
    V a = P(1), blk = NILV;
    long st = 1, len, cnt;

    if (!isarr(a)) return argerr("ASORT");
    if (n >= 2) st = iarg(P(2), 1);
    len = arrlen(a);
    cnt = len - st + 1;
    if (n >= 3) cnt = iarg(P(3), cnt);
    if (n >= 4) {
        if (isblk(P(4))) blk = P(4);
    }
    if (st < 1) st = 1;
    if (st + cnt - 1 > len) cnt = len - st + 1;
    if (cnt > 1) msort_arr(a, st, cnt, blk);
    return a;
}

/* ACLONE */
static V p_aclone(int base, int n)
{
    (void)n;
    if (!isarr(P(1))) return argerr("ACLONE");
    return cloneval(P(1));
}

/* ACOPY */
static V p_acopy(int base, int n)
{
    V a = P(1), b = P(2);
    long st = 1, len, cnt, dp = 1, i;

    if (!isarr(a) || !isarr(b)) return argerr("ACOPY");
    if (n >= 3) st = iarg(P(3), 1);
    len = arrlen(a);
    cnt = len - st + 1;
    if (n >= 4) cnt = iarg(P(4), cnt);
    if (n >= 5) dp = iarg(P(5), 1);
    for (i = 0; i <= cnt - 1; i++) {
        if (st + i <= len && dp + i <= arrlen(b))
            arrset(b, dp + i, arrget(a, st + i));
    }
    return b;
}

/* AFILL */
static V p_afill(int base, int n)
{
    V a = P(1), x = P(2);
    long st = 1, len, cnt, i;

    if (!isarr(a)) return argerr("AFILL");
    if (n >= 3) st = iarg(P(3), 1);
    len = arrlen(a);
    cnt = len - st + 1;
    if (n >= 4) cnt = iarg(P(4), cnt);
    for (i = st; i <= st + cnt - 1; i++) {
        if (i >= 1 && i <= len) arrset(a, i, x);
    }
    return a;
}

/* HB_ATOKENS */
static V p_atokens(int base, int n)
{
    V s = P(1), r;
    const char *d = " ", *p;
    long dl = 1, sl, off = 0, q;

    if (!isstr(s)) return argerr("HB_ATOKENS");
    if (n >= 2 && isstr(P(2))) {
        d = SS(P(2));
        dl = SL(P(2));
    }
    if (dl == 0) {
        d = " ";
        dl = 1;
    }
    r = mkarray(0);
    p = SS(s);
    sl = SL(s);
    for (;;) {
        q = rx_pos(d, dl, p + off, sl - off, 1);
        if (q == 0) {
            arrpush(r, mkstrn(p + off, sl - off));
            break;
        }
        arrpush(r, mkstrn(p + off, q - 1));
        off += q - 1 + dl;
    }
    return r;
}

/* HB_HASH: pairs of key and value (the cREXX version used the
   first pair every time; here pair i is used) */
static V p_hbhash(int base, int n)
{
    V h = mkhash();
    int i = 1;

    while (i < n) {
        hset(h, P(i), P(i + 1));
        i += 2;
    }
    return h;
}

/* HB_HSET */
static V p_hset(int base, int n)
{
    (void)n;
    if (!ishash(P(1))) return argerr("HB_HSET");
    hset(P(1), P(2), P(3));
    return P(1);
}

/* HB_HGET */
static V p_hget(int base, int n)
{
    long p;

    (void)n;
    if (!ishash(P(1))) return argerr("HB_HGET");
    p = hfind(P(1), P(2));
    if (p == 0)
        return rterr("BASE", 1132, "Bound error", "hash access");
    return hval(P(1), p);
}

/* HB_HGETDEF */
static V p_hgetdef(int base, int n)
{
    long p;

    (void)n;
    if (!ishash(P(1))) return argerr("HB_HGETDEF");
    p = hfind(P(1), P(2));
    if (p == 0) return P(3);
    return hval(P(1), p);
}

/* HB_HHASKEY */
static V p_hhaskey(int base, int n)
{
    (void)n;
    if (!ishash(P(1))) return argerr("HB_HHASKEY");
    return mklog(hfind(P(1), P(2)) > 0);
}

/* HB_HDEL */
static V p_hdel(int base, int n)
{
    long p;

    (void)n;
    if (!ishash(P(1))) return argerr("HB_HDEL");
    p = hfind(P(1), P(2));
    if (p > 0) hdel(P(1), p);
    return P(1);
}

/* HB_HKEYS */
static V p_hkeys(int base, int n)
{
    V r, h = P(1);
    long i;

    (void)n;
    if (!ishash(h)) return argerr("HB_HKEYS");
    r = mkarray(0);
    for (i = 1; i <= hashlen(h); i++) arrpush(r, hkey(h, i));
    return r;
}

/* HB_HVALUES */
static V p_hvalues(int base, int n)
{
    V r, h = P(1);
    long i;

    (void)n;
    if (!ishash(h)) return argerr("HB_HVALUES");
    r = mkarray(0);
    for (i = 1; i <= hashlen(h); i++) arrpush(r, hval(h, i));
    return r;
}

/* HB_HPOS */
static V p_hpos(int base, int n)
{
    (void)n;
    if (!ishash(P(1))) return argerr("HB_HPOS");
    return mkint(hfind(P(1), P(2)));
}

/* HB_HKEYAT */
static V p_hkeyat(int base, int n)
{
    long p;

    (void)n;
    if (!ishash(P(1)) || !isnum(P(2))) return argerr("HB_HKEYAT");
    p = toint(numval(P(2)));
    if (p < 1 || p > hashlen(P(1)))
        return rterr("BASE", 1187, "Bound error", "hash access");
    return hkey(P(1), p);
}

/* HB_HVALUEAT */
static V p_hvalueat(int base, int n)
{
    long p;

    if (!ishash(P(1)) || !isnum(P(2)))
        return argerr("HB_HVALUEAT");
    p = toint(numval(P(2)));
    if (p < 1 || p > hashlen(P(1)))
        return rterr("BASE", 1187, "Bound error", "hash access");
    if (n >= 3) arrset(P(1).u.h->vals, p, P(3));
    return hval(P(1), p);
}

/* HB_HCLONE */
static V p_hclone(int base, int n)
{
    (void)n;
    if (!ishash(P(1))) return argerr("HB_HCLONE");
    return cloneval(P(1));
}

/* HB_HMERGE */
static V p_hmerge(int base, int n)
{
    V h = P(1), g = P(2);
    long i;

    (void)n;
    if (!ishash(h) || !ishash(g)) return argerr("HB_HMERGE");
    for (i = 1; i <= hashlen(g); i++) hset(h, hkey(g, i), hval(g, i));
    return h;
}

/* EVAL */
static V p_eval(int base, int n)
{
    V blk = P(1);
    long b, k, i;

    if (!isblk(blk))
        return rterr("BASE", 1004, "Argument error", "EVAL");
    b = ftop;
    k = n - 1;
    SETFTOP(b + k);
    for (i = 1; i <= k; i++) fv[b + i] = fv[base + i + 1];
    return callblock(blk, b, (int)k);
}

/* DO */
static V p_do(int base, int n)
{
    V nm = P(1), a;
    long b, k, i;

    k = n - 1;
    if (isblk(nm)) {
        b = ftop;
        SETFTOP(b + k);
        for (i = 1; i <= k; i++) fv[b + i] = fv[base + i + 1];
        return callblock(nm, b, (int)k);
    }
    if (!isstr(nm)) return argerr("DO");
    a = mkarray(k);
    for (i = 1; i <= k; i++) arrset(a, i, fv[base + i + 1]);
    return callsym(intern(xupper(xstrip(SS(nm)))), a);
}

/* BREAK THROW HB_THROW */
static V p_break(int base, int n)
{
    (void)n;
    brkval = P(1);
    if (ctl == 0) ctl = E_BRK;
    return NILV;
}

/* ERRORBLOCK */
static V p_errorblock(int base, int n)
{
    V old = errblk;

    if (n >= 1 && isblk(P(1))) errblk = P(1);
    return old;
}

/* ERRORNEW */
static V p_errornew(int base, int n)
{
    (void)base;
    (void)n;
    return mkerror("BASE", 0, "", "");
}

/* PROCNAME */
static V p_procname(int base, int n)
{
    long k = 0, idx;

    if (n >= 1) k = iarg(P(1), 0);
    idx = pctop - k;
    if (idx < 1) return mkstr("");
    return mkstr(pcname[idx]);
}

/* PROCLINE */
static V p_procline(int base, int n)
{
    long k = 0, idx;

    if (n >= 1) k = iarg(P(1), 0);
    idx = pctop - k;
    if (idx < 1) return mkint(0);
    return mkint(pcln[idx]);
}

/* HB_ARGC */
static V p_argc(int base, int n)
{
    (void)base;
    (void)n;
    return mkint(nargvs);
}

/* HB_ARGV */
static V p_argv(int base, int n)
{
    long k;

    (void)n;
    k = iarg(P(1), 0);
    if (k < 1 || k > nargvs) return mkstr("");
    return mkstr(argvs[k]);
}

/* HB_PVALUE PVALUE */
static V p_pvalue(int base, int n)
{
    long k;

    (void)n;
    k = iarg(P(1), 0);
    if (k < 1 || k > fpc) return NILV;
    return deref(fv[fbase + k]);
}

/* VERSION HB_VERSION */
static V p_version(int base, int n)
{
    (void)base;
    (void)n;
    return mkstr("xBase for cREXX 1.0");
}

/* OS */
static V p_os(int base, int n)
{
    (void)base;
    (void)n;
    return mkstr("Linux");
}

/* GETENV HB_GETENV */
static V p_getenv(int base, int n)
{
    (void)base;
    (void)n;
    return mkstr("");
}

/* ERRORLEVEL */
static V p_errorlevel(int base, int n)
{
    long old = exitcode;

    if (n >= 1) exitcode = (int)iarg(P(1), 0);
    return mkint(old);
}

/* HB_DIRSEPARATOR HB_PS */
static V p_dirsep(int base, int n)
{
    (void)base;
    (void)n;
    return mkstr("/");
}

/* HB_CWD */
static V p_cwd(int base, int n)
{
    (void)base;
    (void)n;
    return mkstr(".");
}

/* VALTOPRG HB_VALTOEXP */
static V p_valtoprg(int base, int n)
{
    (void)n;
    return mkstr(vtoexp(P(1)));
}

/* REXX changestr(f, t, rep) */
static V chgstr(V f, V t, V rep)
{
    Sb b;
    long off = 0, q;

    sb_init(&b);
    for (;;) {
        q = rx_pos(SS(f), SL(f), SS(t) + off, SL(t) - off, 1);
        if (q == 0) break;
        sb_addn(&b, SS(t) + off, q - 1);
        sb_addn(&b, SS(rep), SL(rep));
        off += q - 1 + SL(f);
    }
    sb_addn(&b, SS(t) + off, SL(t) - off);
    return sb_val(&b);
}

/* HB_STRREPLACE */
static V p_strreplace(int base, int n)
{
    V t = P(1), a = P(2), f, rep;
    long i, k;
    int ch;
    Sb b;

    if (!isstr(t)) return argerr("HB_STRREPLACE");
    if (n >= 3 && isstr(a) && isstr(P(3))) {
        sb_init(&b);
        for (i = 0; i < SL(t); i++) {
            ch = (unsigned char)SS(t)[i];
            k = rx_pos(SS(t) + i, 1, SS(a), SL(a), 1);
            if (k > 0 && k <= SL(P(3)))
                ch = (unsigned char)SS(P(3))[k - 1];
            sb_addc(&b, ch);
        }
        return sb_val(&b);
    }
    if (isarr(a)) {
        for (k = 1; k <= arrlen(a); k++) {
            f = arrget(a, k);
            if (isstr(f)) {
                rep = mkstrn("", 0);
                if (n >= 3) {
                    if (isarr(P(3))) {
                        if (k <= arrlen(P(3))) {
                            rep = arrget(P(3), k);
                            if (!isstr(rep)) rep = mkstrn("", 0);
                        }
                    } else if (isstr(P(3))) rep = P(3);
                }
                if (SL(f) > 0) t = chgstr(f, t, rep);
            }
        }
    }
    return t;
}

/* HB_STRFORMAT */
static V p_strformat(int base, int n)
{
    if (!isstr(P(1))) return argerr("HB_STRFORMAT");
    return strformat(base, n);
}

/* DESCEND */
static V p_descend(int base, int n)
{
    V v = P(1), r;
    long i;
    char *p;

    (void)n;
    if (isstr(v)) {
        r = mkstrn(SS(v), SL(v));
        p = SS(r);
        for (i = 0; i < SL(r); i++)
            p[i] = (char)(255 - (unsigned char)p[i]);
        return r;
    }
    if (isnum(v)) return numneg(v);
    return v;
}

/* SOUNDEX */
static V p_soundex(int base, int n)
{
    static const char *codes = "01230120022455012623010202";
    V s;
    char *u;
    char r[8];
    long i, ul;
    int rn, last = 0, cd, q, ch;

    (void)n;
    if (!isstr(P(1))) return argerr("SOUNDEX");
    s = rx_strip(P(1), 0);
    ul = SL(s);
    if (ul == 0) return mkstr("0000");
    u = xupper(SS(s));
    ul = (long)strlen(u);
    if (ul == 0) return mkstr("0000");
    r[0] = u[0];
    rn = 1;
    q = (unsigned char)u[0] - 64;
    if (q >= 1 && q <= 26) last = codes[q - 1] - '0';
    for (i = 1; i < ul; i++) {
        ch = (unsigned char)u[i];
        q = ch - 64;
        if (q < 1 || q > 26) continue;
        cd = codes[q - 1] - '0';
        if (cd > 0 && cd != last) r[rn++] = (char)('0' + cd);
        if (ch != 'H' && ch != 'W') last = cd;
        if (rn == 4) break;
    }
    while (rn < 4) r[rn++] = '0';
    return mkstrn(r, 4);
}

/* HB_CSTR HB_VALTOSTR2 */
static V p_cstr(int base, int n)
{
    (void)n;
    return mkstr(vtostr(P(1)));
}

void lib2_init(void)
{
    addprim("DATE", p_date, 0, 0);
    addprim("TIME", p_time, 0, 0);
    addprim("SECONDS", p_seconds, 0, 0);
    addprim("HB_MILLISECONDS", p_millis, 0, 0);
    addprim("YEAR", p_year, 1, 1);
    addprim("MONTH", p_month, 1, 1);
    addprim("DAY", p_day, 1, 1);
    addprim("DOW", p_dow, 1, 1);
    addprim("CDOW", p_cdow, 1, 1);
    addprim("CMONTH", p_cmonth, 1, 1);
    addprim("DTOC", p_dtoc, 1, 1);
    addprim("DTOS", p_dtos, 1, 1);
    addprim("CTOD", p_ctod, 1, 1);
    addprim("STOD|HB_STOD", p_stod, 0, 1);
    addprim("HB_DATE", p_hbdate, 3, 3);
    addprim("ARRAY", p_array, 0, -1);
    addprim("AADD", p_aadd, 2, 2);
    addprim("ADEL", p_adel, 2, 2);
    addprim("AINS", p_ains, 2, 2);
    addprim("HB_AINS", p_hbains, 3, 4);
    addprim("HB_ADEL", p_hbadel, 2, 3);
    addprim("ASIZE", p_asize, 2, 2);
    addprim("ATAIL", p_atail, 1, 1);
    addprim("ASCAN|HB_ASCAN", p_ascan, 2, 4);
    addprim("AEVAL", p_aeval, 2, 4);
    addprim("ASORT", p_asort, 1, 4);
    addprim("ACLONE", p_aclone, 1, 1);
    addprim("ACOPY", p_acopy, 2, 5);
    addprim("AFILL", p_afill, 2, 4);
    addprim("HB_ATOKENS", p_atokens, 1, 3);
    addprim("HB_HASH", p_hbhash, 0, -1);
    addprim("HB_HSET", p_hset, 3, 3);
    addprim("HB_HGET", p_hget, 2, 2);
    addprim("HB_HGETDEF", p_hgetdef, 2, 3);
    addprim("HB_HHASKEY", p_hhaskey, 2, 2);
    addprim("HB_HDEL", p_hdel, 2, 2);
    addprim("HB_HKEYS", p_hkeys, 1, 1);
    addprim("HB_HVALUES", p_hvalues, 1, 1);
    addprim("HB_HPOS", p_hpos, 2, 2);
    addprim("HB_HKEYAT", p_hkeyat, 2, 2);
    addprim("HB_HVALUEAT", p_hvalueat, 2, 3);
    addprim("HB_HCLONE", p_hclone, 1, 1);
    addprim("HB_HMERGE", p_hmerge, 2, 2);
    addprim("EVAL", p_eval, 1, -1);
    addprim("DO", p_do, 1, -1);
    addprim("BREAK|THROW|HB_THROW", p_break, 0, 1);
    addprim("ERRORBLOCK", p_errorblock, 0, 1);
    addprim("ERRORNEW", p_errornew, 0, 0);
    addprim("PROCNAME", p_procname, 0, 1);
    addprim("PROCLINE", p_procline, 0, 1);
    addprim("HB_ARGC", p_argc, 0, 0);
    addprim("HB_ARGV", p_argv, 1, 1);
    addprim("HB_PVALUE|PVALUE", p_pvalue, 1, 1);
    addprim("VERSION|HB_VERSION", p_version, 0, 1);
    addprim("OS", p_os, 0, 0);
    addprim("GETENV|HB_GETENV", p_getenv, 1, 2);
    addprim("ERRORLEVEL", p_errorlevel, 0, 1);
    addprim("HB_DIRSEPARATOR|HB_PS", p_dirsep, 0, 0);
    addprim("HB_CWD", p_cwd, 0, 0);
    addprim("VALTOPRG|HB_VALTOEXP", p_valtoprg, 1, 1);
    addprim("HB_STRREPLACE", p_strreplace, 2, 3);
    addprim("HB_STRFORMAT", p_strformat, 1, -1);
    addprim("DESCEND", p_descend, 1, 1);
    addprim("SOUNDEX", p_soundex, 1, 1);
    addprim("HB_CSTR|HB_VALTOSTR2", p_cstr, 1, 1);
}
