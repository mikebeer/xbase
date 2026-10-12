/* eval.c -- expression evaluation, calls, assignment.
   C port of src/50_eval.crexx */
#include "xb.h"

V evsend(int n);
void sendassign(int n, V val);

static long findstr(const char *h, long hn, const char *nd, long nn)
{
    long i;
    if (nn == 0) return 0;
    for (i = 0; i + nn <= hn; i++)
        if (h[i] == nd[0] && memcmp(h + i, nd, (size_t)nn) == 0)
            return i + 1;
    return 0;
}

/* the reference cell of a variable node, boxing it if needed */
V mkref(int n)
{
    int t = nt[n];
    V v, r;
    long idx;
    if (t == K_LOCAL) {
        idx = fbase + na[n];
        v = fv[idx];
        if (v.t == T_REF) return v;
        r = mkboxed(v);
        fv[idx] = r;
        return r;
    }
    if (t == K_CAPV) {
        r.t = T_REF;
        r.w = r.d = r.f = 0;
        r.u.r = curblk->cap[na[n] - 1];
        return r;
    }
    if (t == K_STATIC) {
        idx = na[n];
        v = sv[idx];
        if (v.t == T_REF) return v;
        r = mkboxed(v);
        sv[idx] = r;
        return r;
    }
    if (t == K_IDENT || t == K_MEMVAR) {
        int sym = na[n];
        if (smvd[sym] == 0) {
            rterr("BASE", 1003, "Variable does not exist", snm[sym]);
            return NILV;
        }
        v = smv[sym];
        if (v.t == T_REF) return v;
        r = mkboxed(v);
        smv[sym] = r;
        return r;
    }
    return evalnode(n);
}

/* ---------------------------------------- variables */
V getmemvar(int sym)
{
    V v = smv[sym];
    if (v.t == T_REF) return v.u.r->v;
    return v;
}

void setmemvar(int sym, V val)
{
    V v = smv[sym];
    if (v.t == T_REF) {
        v.u.r->v = val;
        return;
    }
    smv[sym] = val;
    smvd[sym] = 1;
}

V getident(int sym)
{
    if (smvd[sym] == 1) return getmemvar(sym);
    if (curarea > 0) {
        int fi = fieldfind(curarea, sym);
        if (fi > 0) return fieldval(curarea, fi);
    }
    return rterr("BASE", 1003, "Variable does not exist", snm[sym]);
}

/* resolve an alias node to a work area number */
int resolvealias(int an)
{
    V v;
    int a;
    if (nt[an] == K_CONST) {
        v = nk[an];
        if (isstr(v)) {
            char *u = xstrip(SS(v));
            char *uu = xupper(u);
            a = aliasarea(uu);
            if (a == 0)
                rterr("BASE", 1002, "Alias does not exist",
                      xupper(SS(v)));
            return a;
        }
    }
    v = evalnode(an);
    if (ctl != 0) return 0;
    if (isnum(v)) return (int)toint(numval(v));
    if (isstr(v)) {
        char *uu = xupper(xstrip(SS(v)));
        a = aliasarea(uu);
        if (a == 0)
            rterr("BASE", 1002, "Alias does not exist",
                  xupper(SS(v)));
        return a;
    }
    rterr("BASE", 1002, "Alias does not exist", "");
    return 0;
}

/* ----------------------------------------------------------- eval */
static V evindex(int n)
{
    V b = evalnode(na[n]);
    V i = evalnode(nb[n]);
    long k;
    if (ctl != 0) return NILV;
    if (b.t == T_ARR) {
        if (!isnum(i))
            return rterr("BASE", 1068, "Argument error",
                         "array access");
        k = toint(numval(i));
        if (k < 1 || k > arrlen(b))
            return rterr("BASE", 1132, "Bound error",
                         "array access");
        return arrget(b, k);
    }
    if (b.t == T_HASH) {
        long p = hfind(b, i);
        if (p == 0)
            return rterr("BASE", 1132, "Bound error",
                         "array access");
        return hval(b, p);
    }
    return rterr("BASE", 1068, "Argument error", "array access");
}

V mkblock(int n)
{
    V c;
    struct XBlk *b;
    int cnt = nf[n], s, i;
    b = (struct XBlk *)xmalloc((long)sizeof(struct XBlk));
    b->node = n;
    b->self = curself;
    b->ncap = cnt;
    b->cap = NULL;
    if (cnt > 0) {
        b->cap = (struct XBoxT **)xmalloc(
            (long)cnt * (long)sizeof(struct XBoxT *));
        s = nd[n];
        i = 0;
        while (s > 0 && i < cnt) {
            int k = na[s], ix = nb[s];
            if (k == 1) {
                long idx = fbase + ix;
                V v = fv[idx];
                if (v.t != T_REF) {
                    v = mkboxed(v);
                    fv[idx] = v;
                }
                b->cap[i] = v.u.r;
            } else {
                b->cap[i] = curblk->cap[ix - 1];
            }
            i++;
            s = nx[s];
        }
    }
    c.t = T_BLK;
    c.w = c.d = c.f = 0;
    c.u.b = b;
    return c;
}

static V mkcodeblock(int n)
{
    V c;
    struct XBlk *b;
    b = (struct XBlk *)xmalloc((long)sizeof(struct XBlk));
    b->node = n;
    b->self = curself;
    b->ncap = 0;
    b->cap = NULL;
    c.t = T_BLK;
    c.w = c.d = c.f = 0;
    c.u.b = b;
    return c;
}

V evalnode(int n)
{
    int t = nt[n];
    V a, b, v;
    if (t == K_CONST) return nk[n];
    if (t == K_LOCAL) {
        v = fv[fbase + na[n]];
        if (v.t == T_REF) return v.u.r->v;
        return v;
    }
    if (t == K_CALL) return evcall(n);
    if (t == K_ADD) {
        a = evalnode(na[n]);
        b = evalnode(nb[n]);
        if (ctl != 0) return NILV;
        return binadd(a, b);
    }
    if (t == K_SUB) {
        a = evalnode(na[n]);
        b = evalnode(nb[n]);
        if (ctl != 0) return NILV;
        return binsub(a, b);
    }
    if (t == K_IN) {
        a = evalnode(na[n]);
        b = evalnode(nb[n]);
        if (ctl != 0) return NILV;
        if (!isstr(a) || !isstr(b))
            return rterr("BASE", 1109, "Argument error", "$");
        if (SL(a) == 0 || findstr(SS(b), SL(b), SS(a), SL(a)) > 0)
            return mklog(1);
        return mklog(0);
    }
    if (t == K_EQ || t == K_EXEQ || t == K_NE || t == K_LT ||
        t == K_LE || t == K_GT || t == K_GE) {
        a = evalnode(na[n]);
        b = evalnode(nb[n]);
        if (ctl != 0) return NILV;
        return relop(t, a, b);
    }
    if (t == K_STATIC) {
        v = sv[na[n]];
        if (v.t == T_REF) return v.u.r->v;
        return v;
    }
    if (t == K_ASSIGN) {
        v = evalnode(nb[n]);
        if (ctl != 0) return NILV;
        assignto(na[n], v);
        return v;
    }
    if (t == K_INDEX) return evindex(n);
    if (t == K_AND) {
        a = evalnode(na[n]);
        if (ctl != 0) return NILV;
        if (!islog(a)) return argerr(".AND.");
        if (!truthy(a)) return mklog(0);
        b = evalnode(nb[n]);
        if (ctl != 0) return NILV;
        if (!islog(b)) return argerr(".AND.");
        return b;
    }
    if (t == K_OR) {
        a = evalnode(na[n]);
        if (ctl != 0) return NILV;
        if (!islog(a)) return argerr(".OR.");
        if (truthy(a)) return mklog(1);
        b = evalnode(nb[n]);
        if (ctl != 0) return NILV;
        if (!islog(b)) return argerr(".OR.");
        return b;
    }
    if (t == K_NOT) {
        a = evalnode(na[n]);
        if (ctl != 0) return NILV;
        if (!islog(a)) return argerr(".NOT.");
        return mklog(!truthy(a));
    }
    if (t == K_MUL || t == K_DIV || t == K_MOD || t == K_POW) {
        a = evalnode(na[n]);
        b = evalnode(nb[n]);
        if (ctl != 0) return NILV;
        return binop(t, a, b);
    }
    if (t == K_CAPV) return curblk->cap[na[n] - 1]->v;
    if (t == K_IDENT) return getident(na[n]);
    if (t == K_PLUS) return evalnode(na[n]);
    if (t == K_IIF) {
        v = evalnode(na[n]);
        if (ctl != 0) return NILV;
        if (!islog(v)) return argerr("IIF");
        if (truthy(v)) return evalnode(nb[n]);
        return evalnode(nc[n]);
    }
    if (t == K_NEG) {
        a = evalnode(na[n]);
        if (ctl != 0) return NILV;
        if (!isnum(a)) return argerr("-");
        return numneg(a);
    }
    if (t == K_PREINC || t == K_PREDEC || t == K_POSTINC ||
        t == K_POSTDEC) {
        V old, nw;
        old = evalnode(na[n]);
        if (ctl != 0) return NILV;
        if (!isnum(old) && !isdate(old)) return argerr("++");
        if (t == K_PREINC || t == K_POSTINC)
            nw = binadd(old, mkint(1));
        else nw = binsub(old, mkint(1));
        if (ctl != 0) return NILV;
        assignto(na[n], nw);
        if (t == K_PREINC || t == K_PREDEC) return nw;
        return old;
    }
    if (t == K_SEND) return evsend(n);
    if (t == K_ARRAY) {
        long cnt = nb[n], i = 0;
        int e = na[n];
        V arr = mkarray(cnt);
        while (e > 0) {
            i++;
            v = evalnode(e);
            if (ctl != 0) return NILV;
            arrset(arr, i, v);
            e = nx[e];
        }
        return arr;
    }
    if (t == K_HASHLIT) {
        V h = mkhash();
        int e = na[n];
        while (e > 0) {
            V k = evalnode(e);
            v = evalnode(nx[e]);
            if (ctl != 0) return NILV;
            hset(h, k, v);
            e = nx[nx[e]];
        }
        return h;
    }
    if (t == K_BLOCK) return mkblock(n);
    if (t == K_SELF) return curself;
    if (t == K_MEMVAR) {
        int sym = na[n];
        if (smvd[sym] == 0)
            return rterr("BASE", 1003, "Variable does not exist",
                         xfmt("M->%s", snm[sym]));
        return getmemvar(sym);
    }
    if (t == K_FIELDX) {
        int sym = na[n];
        if (curarea > 0) {
            int fi = fieldfind(curarea, sym);
            if (fi > 0) return fieldval(curarea, fi);
        }
        return rterr("BASE", 1003, "Variable does not exist",
                     xfmt("FIELD->%s", snm[sym]));
    }
    if (t == K_ALIASFLD) {
        int ar = resolvealias(na[n]);
        int sym, fi;
        if (ctl != 0) return NILV;
        sym = nb[n];
        fi = fieldfind(ar, sym);
        if (fi == 0)
            return rterr("BASE", 1003, "Variable does not exist",
                         snm[sym]);
        return fieldval(ar, fi);
    }
    if (t == K_ALIASEXPR) {
        int ar = resolvealias(na[n]);
        int old;
        if (ctl != 0) return NILV;
        old = curarea;
        curarea = ar;
        v = evalnode(nb[n]);
        curarea = old;
        return v;
    }
    if (t == K_REFX) return mkref(na[n]);
    if (t == K_MACRO) {
        v = evalnode(na[n]);
        if (ctl != 0) return NILV;
        if (!isstr(v)) return v;
        return macroeval(SS(v));
    }
    if (t == K_CODEBLOCK) return mkcodeblock(n);
    return rterr("BASE", 1066,
                 "Internal error: unknown expression node",
                 xfmt("%d", t));
}

/* ---------------------------------------- assignment */
void assignto(int n, V val)
{
    int t = nt[n];
    V v;
    long idx;
    if (t == K_LOCAL) {
        idx = fbase + na[n];
        v = fv[idx];
        if (v.t == T_REF) {
            v.u.r->v = val;
            return;
        }
        fv[idx] = val;
        return;
    }
    if (t == K_CAPV) {
        curblk->cap[na[n] - 1]->v = val;
        return;
    }
    if (t == K_STATIC) {
        idx = na[n];
        v = sv[idx];
        if (v.t == T_REF) {
            v.u.r->v = val;
            return;
        }
        sv[idx] = val;
        return;
    }
    if (t == K_IDENT) {
        int sym = na[n];
        if (smvd[sym] == 1) {
            setmemvar(sym, val);
            return;
        }
        if (curarea > 0) {
            int fi = fieldfind(curarea, sym);
            if (fi > 0) {
                fieldput(curarea, fi, val);
                return;
            }
        }
        rterr("BASE", 1003, "Variable does not exist", snm[sym]);
        return;
    }
    if (t == K_MEMVAR) {
        int sym = na[n];
        if (smvd[sym] == 0) {
            rterr("BASE", 1003, "Variable does not exist",
                  xfmt("M->%s", snm[sym]));
            return;
        }
        setmemvar(sym, val);
        return;
    }
    if (t == K_FIELDX) {
        int sym = na[n];
        if (curarea > 0) {
            int fi = fieldfind(curarea, sym);
            if (fi > 0) {
                fieldput(curarea, fi, val);
                return;
            }
        }
        rterr("BASE", 1003, "Variable does not exist",
              xfmt("FIELD->%s", snm[sym]));
        return;
    }
    if (t == K_ALIASFLD) {
        int ar = resolvealias(na[n]);
        int fi;
        if (ctl != 0) return;
        fi = fieldfind(ar, nb[n]);
        if (fi == 0) {
            rterr("BASE", 1003, "Variable does not exist",
                  snm[nb[n]]);
            return;
        }
        fieldput(ar, fi, val);
        return;
    }
    if (t == K_INDEX) {
        V b = evalnode(na[n]);
        V i = evalnode(nb[n]);
        long k;
        if (ctl != 0) return;
        if (b.t == T_ARR) {
            if (!isnum(i)) {
                rterr("BASE", 1069, "Argument error",
                      "array assign");
                return;
            }
            k = toint(numval(i));
            if (k < 1 || k > arrlen(b)) {
                rterr("BASE", 1133, "Bound error", "array assign");
                return;
            }
            arrset(b, k, val);
            return;
        }
        if (b.t == T_HASH) {
            hset(b, i, val);
            return;
        }
        rterr("BASE", 1069, "Argument error", "array assign");
        return;
    }
    if (t == K_SEND) {
        sendassign(n, val);
        return;
    }
    if (t == K_MACRO) {
        v = evalnode(na[n]);
        if (isstr(v)) {
            int sym = intern(xupper(xstrip(SS(v))));
            if (smvd[sym] == 1) setmemvar(sym, val);
            else if (curarea > 0) {
                int fi = fieldfind(curarea, sym);
                if (fi > 0) fieldput(curarea, fi, val);
                else rterr("BASE", 1003,
                           "Variable does not exist", snm[sym]);
            } else rterr("BASE", 1003, "Variable does not exist",
                         snm[sym]);
        }
        return;
    }
    rterr("BASE", 1066, "Argument error", "assignment");
}

/* ---------------------------------------- call sites */
int evalargs(int a, long base, int mx)
{
    int i = 0;
    V v;
    while (a > 0) {
        i++;
        if (i <= mx) {
            if (nt[a] == K_REFX) v = mkref(na[a]);
            else v = evalnode(a);
            fv[base + i] = v;
        } else evalnode(a);
        if (ctl != 0) return 0;
        a = nx[a];
    }
    return 1;
}

V evcall(int n)
{
    int sym = na[n], cnt = nc[n], f, bi, cl, i;
    long base, slots;
    V r;
    f = sfn[sym];
    if (f > 0) {
        base = ftop;
        slots = fnslots[f];
        if (cnt > slots) slots = cnt;
        SETFTOP(base + slots);
        for (i = 1; i <= slots; i++) fv[base + i] = NILV;
        evalargs(nb[n], base, fnpar[f]);
        if (ctl != 0) {
            ftop = base;
            return NILV;
        }
        return invoke(f, base, cnt, NILV, n);
    }
    bi = sbi[sym];
    if (bi > 0) {
        base = ftop;
        SETFTOP(base + cnt + 8);
        for (i = cnt + 1; i <= cnt + 8; i++) fv[base + i] = NILV;
        evalargs(nb[n], base, cnt);
        if (ctl != 0) {
            ftop = base;
            return NILV;
        }
        r = callprim(bi, (int)base, cnt);
        ftop = base;
        return r;
    }
    cl = scls[sym];
    if (cl > 0) {
        evalargs(nb[n], ftop, 0);
        return newobject(cl);
    }
    return rterr("BASE", 1001, "Undefined function", snm[sym]);
}

/* run user function f whose arguments are in fv[base+1..] */
V invoke(int f, long base, int cnt, V obj, int site)
{
    long ofb, opc, opv;
    V osf, r;
    struct XBlk *ocap;
    int omc;
    (void)site;
    if (pctop > 2400) {
        ftop = base;
        return rterr("BASE", 1002, "Stack overflow", fnname[f]);
    }
    ofb = fbase;
    opc = fpc;
    osf = curself;
    ocap = curblk;
    omc = curmcls;
    opv = pvtop;
    fbase = base;
    fpc = cnt;
    curself = obj;
    curmcls = fnclass[f];
    pushcall(fnname[f], nl[fnbody[f]]);
    execstmts(fnbody[f]);
    r = NILV;
    if (ctl == E_RET) {
        r = retv;
        ctl = 0;
    } else if (ctl == E_EXIT || ctl == E_LOOP) ctl = 0;
    while (pvtop > opv) {
        int s = pvs[pvtop];
        smv[s] = pvv[pvtop];
        smvd[s] = pvd[pvtop];
        pvtop--;
    }
    pctop--;
    fbase = ofb;
    fpc = opc;
    curself = osf;
    curblk = ocap;
    curmcls = omc;
    ftop = base;
    return r;
}

/* evaluate a block; arguments in fv[base+1..base+cnt] */
V callblock(V blk, long base, int cnt)
{
    struct XBlk *bk = blk.u.b;
    int bn = bk->node, sym, f, bi, e, i;
    long slots, ofb;
    struct XBlk *ocap;
    V osf, r;
    if (nt[bn] == K_CODEBLOCK) {
        sym = na[bn];
        f = sfn[sym];
        if (f > 0) {
            slots = fnslots[f];
            if (cnt > slots) slots = cnt;
            SETFTOP(base + slots);
            for (i = cnt + 1; i <= slots; i++) fv[base + i] = NILV;
            return invoke(f, base, cnt, NILV, 0);
        }
        bi = sbi[sym];
        if (bi > 0) {
            SETFTOP(base + cnt + 8);
            for (i = cnt + 1; i <= cnt + 8; i++)
                fv[base + i] = NILV;
            r = callprim(bi, (int)base, cnt);
            ftop = base;
            return r;
        }
        ftop = base;
        return rterr("BASE", 1001, "Undefined function", snm[sym]);
    }
    slots = nc[bn];
    if (cnt > slots) slots = cnt;
    SETFTOP(base + slots);
    for (i = cnt + 1; i <= slots; i++) fv[base + i] = NILV;
    ofb = fbase;
    ocap = curblk;
    osf = curself;
    fbase = base;
    curblk = bk;
    curself = bk->self;
    e = na[bn];
    r = NILV;
    while (e > 0) {
        r = evalnode(e);
        if (ctl != 0) break;
        e = nx[e];
    }
    fbase = ofb;
    curblk = ocap;
    curself = osf;
    ftop = base;
    return r;
}

V evalblock0(V blk)
{
    long base = ftop;
    return callblock(blk, base, 0);
}

V evalblock1(V blk, V a)
{
    long base = ftop;
    SETFTOP(base + 1);
    fv[base + 1] = a;
    return callblock(blk, base, 1);
}

V evalblock2(V blk, V a, V b)
{
    long base = ftop;
    SETFTOP(base + 2);
    fv[base + 1] = a;
    fv[base + 2] = b;
    return callblock(blk, base, 2);
}

V evalblock3(V blk, V a, V b, V c)
{
    long base = ftop;
    SETFTOP(base + 3);
    fv[base + 1] = a;
    fv[base + 2] = b;
    fv[base + 3] = c;
    return callblock(blk, base, 3);
}

/* call a function by symbol with arguments in an array */
V callsym(int sym, V arr)
{
    long cnt = 0, base, slots, i;
    int f, bi, cl;
    V r;
    if (arr.t == T_ARR) cnt = arrlen(arr);
    base = ftop;
    SETFTOP(base + cnt);
    for (i = 1; i <= cnt; i++) fv[base + i] = arrget(arr, i);
    f = sfn[sym];
    if (f > 0) {
        slots = fnslots[f];
        if (cnt > slots) slots = cnt;
        SETFTOP(base + slots);
        for (i = cnt + 1; i <= slots; i++) fv[base + i] = NILV;
        return invoke(f, base, (int)cnt, NILV, 0);
    }
    bi = sbi[sym];
    if (bi > 0) {
        SETFTOP(base + cnt + 8);
        for (i = cnt + 1; i <= cnt + 8; i++) fv[base + i] = NILV;
        r = callprim(bi, (int)base, (int)cnt);
        ftop = base;
        return r;
    }
    ftop = base;
    cl = scls[sym];
    if (cl > 0) return newobject(cl);
    return rterr("BASE", 1001, "Undefined function", snm[sym]);
}
