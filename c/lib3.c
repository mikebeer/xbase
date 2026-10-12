/* lib3.c -- builtin functions, part 3 (port of lib3.tpl, first
   half): Transform, console output and input, SET.  The file
   functions of lib3.tpl are in lib4.c. */
#include "xb.h"

#define P(k) fv[base + (k)]

/* the text of all arguments separated by blanks */
static void joinargs(Sb *b, int base, int n)
{
    int i;
    V v;

    sb_init(b);
    for (i = 1; i <= n; i++) {
        if (i > 1) sb_addc(b, ' ');
        v = P(i);
        if (isstr(v)) sb_addn(b, SS(v), SL(v));
        else sb_adds(b, valstr(v));
    }
}

/* TRANSFORM */
static V p_transform(int base, int n)
{
    (void)n;
    if (!isstr(P(2))) return argerr("TRANSFORM");
    return mkstr(xtransform(P(1), SS(P(2))));
}

/* QOUT */
static V p_qout(int base, int n)
{
    Sb b;

    joinargs(&b, base, n);
    if (ctl != 0) return NILV;
    if (outstarted == 1) emitnl();
    if (b.n > 0) emitn(sb_str(&b), b.n);
    return NILV;
}

/* QQOUT */
static V p_qqout(int base, int n)
{
    Sb b;

    joinargs(&b, base, n);
    if (ctl != 0) return NILV;
    if (b.n > 0) emitn(sb_str(&b), b.n);
    return NILV;
}

/* OUTSTD */
static V p_outstd(int base, int n)
{
    Sb b;

    joinargs(&b, base, n);
    if (b.n > 0) emitn(sb_str(&b), b.n);
    return NILV;
}

/* OUTERR */
static V p_outerr(int base, int n)
{
    Sb b;

    joinargs(&b, base, n);
    flushout();
    if (b.n > 0) fwrite(b.p, 1, (size_t)b.n, stderr);
    fflush(stderr);
    return NILV;
}

/* DEVOUT */
static V p_devout(int base, int n)
{
    V v = P(1);

    (void)n;
    if (isstr(v)) {
        if (SL(v) > 0) emitn(SS(v), SL(v));
    } else emit(valstr(v));
    return NILV;
}

/* DEVPOS SETPOS */
static V p_devpos(int base, int n)
{
    (void)n;
    gotopos((int)iarg(P(1), 0), (int)iarg(P(2), 0));
    return NILV;
}

/* ROW */
static V p_row(int base, int n)
{
    (void)base;
    (void)n;
    return mkint(scrrow);
}

/* COL */
static V p_col(int base, int n)
{
    (void)base;
    (void)n;
    return mkint(outcol);
}

/* MAXROW */
static V p_maxrow(int base, int n)
{
    (void)base;
    (void)n;
    return mkint(24);
}

/* MAXCOL */
static V p_maxcol(int base, int n)
{
    (void)base;
    (void)n;
    return mkint(79);
}

/* SETCOLOR COLORSELECT SETCURSOR SETBLINK HB_GTINFO SETMODE
   SCROLL DISPBEGIN DISPEND SETKEY HB_SETKEYSAVE HB_KEYPUT */
static V p_setcolor(int base, int n)
{
    (void)base;
    (void)n;
    return mkstr("W/N");
}

/* INKEY HB_KEYSTD HB_KEYINKEY */
static V p_inkey(int base, int n)
{
    (void)base;
    (void)n;
    return mkint(readkey());
}

/* NEXTKEY LASTKEY */
static V p_nextkey(int base, int n)
{
    (void)base;
    (void)n;
    return mkint(0);
}

/* ALERT */
static V p_alert(int base, int n)
{
    long i, k;
    Sb o;
    char *s, *t, *e;
    int u, c;

    (void)n;
    flushout();
    if (outcol > 0) emitnl();
    emit(vtostr(P(1)));
    emitnl();
    sb_init(&o);
    if (n >= 2 && isarr(P(2))) {
        for (i = 1; i <= arrlen(P(2)); i++) {
            if (i > 1) sb_adds(&o, "  ");
            sb_adds(&o, xfmt("[%ld] %s", i,
                             vtostr(arrget(P(2), i))));
        }
    }
    if (o.n > 0) {
        emit(sb_str(&o));
        emitnl();
        s = readcon();
        k = 1;
        t = xstrip(s);
        if (ineof == 0 && t[0] != 0) {
            if (isdigitc((unsigned char)t[0])) k = strtol(t, NULL, 10);
            else {
                u = toupper((unsigned char)t[0]);
                for (i = 1; i <= arrlen(P(2)); i++) {
                    e = vtostr(arrget(P(2), i));
                    c = e[0] ? (unsigned char)e[0] : ' ';
                    if (toupper(c) == u) {
                        k = i;
                        break;
                    }
                }
            }
        }
        return mkint(k);
    }
    return mkint(1);
}

/* SET */
static V p_set(int base, int n)
{
    long k = iarg(P(1), 0);
    V old = NILV;

    if (k == 1) {
        old = mklog(setexact);
        if (n >= 2) setexact = truthy(P(2));
    } else if (k == 2) {
        old = mklog(setfixed);
        if (n >= 2) setfixed = truthy(P(2));
    } else if (k == 3) {
        old = mkint(setdec);
        if (n >= 2) setdec = (int)iarg(P(2), 2);
    } else if (k == 4) {
        old = mkstr(setdatefmt);
        if (n >= 2) setdate(sarg_z(P(2)));
    } else if (k == 5) {
        old = mkint(setepoch);
        if (n >= 2) setepoch = (int)iarg(P(2), 1900);
    } else if (k == 9) {
        old = mklog(setsoft);
        if (n >= 2) setsoft = truthy(P(2));
    } else if (k == 11) {
        old = mklog(setdeleted);
        if (n >= 2) setdeleted = truthy(P(2));
    } else if (k == 17) {
        old = mklog(setconsole);
        if (n >= 2) setconsole = truthy(P(2));
    }
    return old;
}

/* __SETCENTURY SETCENTURY */
static V p_setcentury(int base, int n)
{
    V old = mklog(setcentury);

    if (n >= 1) {
        if (islog(P(1))) setcentury = truthy(P(1));
    }
    return old;
}

void lib3_init(void)
{
    addprim("TRANSFORM", p_transform, 2, 2);
    addprim("QOUT", p_qout, 0, -1);
    addprim("QQOUT", p_qqout, 0, -1);
    addprim("OUTSTD", p_outstd, 0, -1);
    addprim("OUTERR", p_outerr, 0, -1);
    addprim("DEVOUT", p_devout, 1, 2);
    addprim("DEVPOS|SETPOS", p_devpos, 2, 2);
    addprim("ROW", p_row, 0, 0);
    addprim("COL", p_col, 0, 0);
    addprim("MAXROW", p_maxrow, 0, 1);
    addprim("MAXCOL", p_maxcol, 0, 1);
    addprim("SETCOLOR|COLORSELECT|SETCURSOR|SETBLINK|HB_GTINFO|"
            "SETMODE|SCROLL|DISPBEGIN|DISPEND|SETKEY|HB_SETKEYSAVE|"
            "HB_KEYPUT", p_setcolor, 0, -1);
    addprim("INKEY|HB_KEYSTD|HB_KEYINKEY", p_inkey, 0, 2);
    addprim("NEXTKEY|LASTKEY", p_nextkey, 0, 0);
    addprim("ALERT", p_alert, 1, 4);
    addprim("SET", p_set, 1, 3);
    addprim("__SETCENTURY|SETCENTURY", p_setcentury, 0, 1);
}
