/* exec.c -- statements, errors, objects, macros.
   C port of src/51_exec.crexx */
#include "xb.h"

void execstmts(int n)
{
    int s = na[n];
    while (s > 0) {
        exec(s);
        if (ctl != 0) return;
        s = nx[s];
    }
}

/* condition value: .T. or .F.; others raise an error */
int condval(int n)
{
    V c = evalnode(n);
    if (ctl != 0) return 0;
    if (islog(c)) return truthy(c) ? 1 : 0;
    argerr("IF");
    return 0;
}

static void declmem(int p)
{
    int mleft = ne[p];
    while (p > 0 && mleft > 0) {
        int sym = na[p];
        V init = NILV, v;
        mleft--;
        if (nf[p] == 1) {
            v = evalnode(nb[p]);
            if (isstr(v)) sym = intern(xupper(xstrip(SS(v))));
        }
        if (nb[p] > 0 && nf[p] == 0) init = evalnode(nb[p]);
        if (ctl != 0) return;
        if (nt[p] == K_PRIVATE) {
            pushprivate(sym);
            smv[sym] = init;
            smvd[sym] = 1;
        } else {
            if (smvd[sym] == 0 || nb[p] > 0) {
                if (nb[p] == 0) smv[sym] = mklog(0);
                else smv[sym] = init;
                smvd[sym] = 1;
            }
        }
        p = nx[p];
    }
}

static void execfor(int s)
{
    int v = na[s], body = ne[s], neg = 0;
    V start, stop, step, cur, nv2;
    start = evalnode(nb[s]);
    stop = evalnode(nc[s]);
    step = mkint(1);
    if (nd[s] > 0) step = evalnode(nd[s]);
    if (ctl != 0) return;
    if ((!isnum(start) && !isdate(start)) ||
        (!isnum(stop) && !isdate(stop)) || !isnum(step)) {
        argerr("FOR");
        return;
    }
    assignto(v, start);
    if (numval(step) < 0.0) neg = 1;
    for (;;) {
        int r;
        cur = evalnode(v);
        if (ctl != 0) return;
        r = cmpv(cur, stop);
        if (r == 2) {
            argerr("FOR");
            return;
        }
        if (neg == 0 && r > 0) break;
        if (neg == 1 && r < 0) break;
        execstmts(body);
        if (ctl != 0) {
            if (ctl == E_EXIT) {
                ctl = 0;
                break;
            }
            if (ctl == E_LOOP) ctl = 0;
            else break;
        }
        cur = evalnode(v);
        nv2 = binadd(cur, step);
        if (ctl != 0) return;
        assignto(v, nv2);
    }
}

static void execforeach(int s)
{
    int v = na[s], desc = nd[s], body = nc[s];
    V coll = evalnode(nb[s]), el, nw;
    long n, i;
    if (ctl != 0) return;
    if (isarr(coll)) {
        n = arrlen(coll);
        i = 1;
        if (desc == 1) i = n;
        for (;;) {
            if (desc == 1) {
                if (i < 1) break;
            } else if (i > arrlen(coll)) break;
            el = arrget(coll, i);
            assignto(v, el);
            execstmts(body);
            if (ctl == 0) {
                nw = evalnode(v);
                if (!vsame(nw, el) && i <= arrlen(coll))
                    arrset(coll, i, nw);
            }
            if (ctl != 0) {
                if (ctl == E_EXIT) {
                    ctl = 0;
                    break;
                }
                if (ctl == E_LOOP) ctl = 0;
                else break;
            }
            if (desc == 1) i--;
            else i++;
        }
        return;
    }
    if (ishash(coll)) {
        n = hashlen(coll);
        i = 1;
        if (desc == 1) i = n;
        for (;;) {
            if (desc == 1) {
                if (i < 1) break;
            } else if (i > hashlen(coll)) break;
            assignto(v, hval(coll, i));
            execstmts(body);
            if (ctl != 0) {
                if (ctl == E_EXIT) {
                    ctl = 0;
                    break;
                }
                if (ctl == E_LOOP) ctl = 0;
                else break;
            }
            if (desc == 1) i--;
            else i++;
        }
        return;
    }
    if (isstr(coll)) {
        n = SL(coll);
        for (i = 0; i < n; i++) {
            assignto(v, mkstrn(SS(coll) + i, 1));
            execstmts(body);
            if (ctl != 0) {
                if (ctl == E_EXIT) {
                    ctl = 0;
                    break;
                }
                if (ctl == E_LOOP) ctl = 0;
                else break;
            }
        }
        return;
    }
    rterr("BASE", 1066, "Argument error", "FOR EACH");
}

static void execswitch(int s)
{
    V v = evalnode(na[s]), w;
    int c, e, hit = 0;
    if (ctl != 0) return;
    c = nb[s];
    while (c > 0) {
        if (hit == 0) {
            e = na[c];
            while (e > 0) {
                w = evalnode(e);
                if (ctl != 0) return;
                if (exactv(v, w)) {
                    hit = 1;
                    break;
                }
                e = nx[e];
            }
        }
        if (hit == 1) {
            execstmts(nb[c]);
            if (ctl != 0) {
                if (ctl == E_EXIT) ctl = 0;
                return;
            }
        }
        c = nx[c];
    }
    if (nc[s] > 0) execstmts(nc[s]);
    if (ctl == E_EXIT) ctl = 0;
}

static void execsequence(int s)
{
    int caught;
    V val;
    seqdepth++;
    execstmts(na[s]);
    seqdepth--;
    caught = 0;
    if (ctl == E_BRK) {
        caught = 1;
        val = brkval;
        ctl = 0;
        if (ne[s] == 1) {
            if (nc[s] > 0) assignto(nc[s], val);
            execstmts(nb[s]);
        }
    }
    (void)caught;
    if (nd[s] > 0) {
        int sc = ctl;
        ctl = 0;
        execstmts(nd[s]);
        if (ctl == 0) ctl = sc;
    }
}

static void execprint(int s)
{
    int e = na[s], first = 1;
    Sb txt;
    V v;
    sb_init(&txt);
    while (e > 0) {
        v = evalnode(e);
        if (ctl != 0) return;
        if (first == 0) sb_addc(&txt, ' ');
        if (isstr(v)) sb_addn(&txt, SS(v), SL(v));
        else sb_adds(&txt, valstr(v));
        if (ctl != 0) return;
        first = 0;
        e = nx[e];
    }
    if (setconsole == 0) return;
    if (nb[s] == 1) {
        if (outstarted == 1) emitnl();
    }
    emitn(txt.p ? txt.p : "", txt.n);
}

void exectext(int s)
{
    V v = evalnode(na[s]);
    if (!isstr(v)) return;
    if (nb[s] > 0) {
        assignto(nb[s], v);
        return;
    }
    if (outstarted == 1 && outcol > 0) emitnl();
    emitn(SS(v), SL(v));
    emitnl();
}

void exec(int s)
{
    int t = nt[s];
    V v;
    pcln[pctop] = nl[s];
    if (t == K_EXPRSTMT) {
        evalnode(na[s]);
        return;
    }
    if (t == K_IF) {
        if (condval(na[s]) == 1) execstmts(nb[s]);
        else if (ctl == 0 && nc[s] > 0) {
            if (nt[nc[s]] == K_IF) exec(nc[s]);
            else execstmts(nc[s]);
        }
        return;
    }
    if (t == K_WHILE) {
        for (;;) {
            if (condval(na[s]) == 0) break;
            if (ctl != 0) break;
            execstmts(nb[s]);
            if (ctl != 0) {
                if (ctl == E_EXIT) {
                    ctl = 0;
                    break;
                }
                if (ctl == E_LOOP) ctl = 0;
                else break;
            }
        }
        return;
    }
    if (t == K_FOR) {
        execfor(s);
        return;
    }
    if (t == K_STMTS) {
        execstmts(s);
        return;
    }
    if (t == K_RETURN) {
        v = NILV;
        if (na[s] > 0) v = evalnode(na[s]);
        if (ctl == 0) {
            retv = v;
            ctl = E_RET;
        }
        return;
    }
    if (t == K_PRINT) {
        execprint(s);
        return;
    }
    if (t == K_FOREACH) {
        execforeach(s);
        return;
    }
    if (t == K_EXIT) {
        ctl = E_EXIT;
        return;
    }
    if (t == K_LOOP) {
        ctl = E_LOOP;
        return;
    }
    if (t == K_DOCASE) {
        int c = na[s];
        while (c > 0) {
            if (na[c] == 0) {
                execstmts(nb[c]);
                return;
            }
            if (condval(na[c]) == 1) {
                execstmts(nb[c]);
                return;
            }
            if (ctl != 0) return;
            c = nx[c];
        }
        return;
    }
    if (t == K_SWITCH) {
        execswitch(s);
        return;
    }
    if (t == K_SEQUENCE) {
        execsequence(s);
        return;
    }
    if (t == K_BREAK) {
        v = NILV;
        if (na[s] > 0) v = evalnode(na[s]);
        if (ctl == 0) {
            brkval = v;
            ctl = E_BRK;
        }
        return;
    }
    if (t == K_PRIVATE || t == K_PUBLIC) {
        declmem(s);
        return;
    }
    if (t == K_PARAMETERS) {
        int c = na[s], i = 0;
        while (c > 0) {
            int sym = na[c];
            i++;
            pushprivate(sym);
            v = NILV;
            if (i <= fpc) v = fv[fbase + i];
            smv[sym] = v;
            smvd[sym] = 1;
            c = nx[c];
        }
        return;
    }
    if (t == K_CMD) {
        execcmd(s);
        return;
    }
    if (t == K_SCOPED) {
        execscoped(s);
        return;
    }
    if (t == K_SAYGET) {
        execsayget(s);
        return;
    }
    if (t == K_TEXT) {
        exectext(s);
        return;
    }
    if (t == K_NOP) return;
    rterr("BASE", 1066, "Internal error: unknown statement",
          xfmt("%d", t));
}

/* ---------------------------------------- errors */
void initerrclass(void)
{
    static const char *names[] = {
        "DESCRIPTION", "OPERATION", "SUBSYSTEM", "SUBCODE",
        "GENCODE", "SEVERITY", "FILENAME", "OSCODE", "TRIES",
        "ARGS", "CANRETRY", "CANDEFAULT", "CANSUBSTITUTE",
        "CARGO", NULL };
    int i;
    errcls = newclass("ERROR", 0);
    for (i = 0; names[i]; i++) adddata(errcls, intern(names[i]));
}

static void errset(V obj, const char *nm, V v)
{
    int sl = pm_get(2, errcls, intern(nm));
    arrset(obj.u.o->slots, sl, v);
}

static V errget(V obj, const char *nm)
{
    return arrget(obj.u.o->slots, pm_get(2, errcls, intern(nm)));
}

V mkerror(const char *sub, int code, const char *desc,
          const char *op)
{
    V obj = newobject(errcls);
    errset(obj, "DESCRIPTION", mkstr(desc));
    errset(obj, "OPERATION", mkstr(op));
    errset(obj, "SUBSYSTEM", mkstr(sub));
    errset(obj, "SUBCODE", mkint(code));
    errset(obj, "GENCODE", mkint(1));
    errset(obj, "SEVERITY", mkint(2));
    errset(obj, "FILENAME", mkstr(""));
    errset(obj, "OSCODE", mkint(0));
    errset(obj, "TRIES", mkint(0));
    errset(obj, "CANRETRY", mklog(1));
    errset(obj, "CANDEFAULT", mklog(1));
    errset(obj, "CANSUBSTITUTE", mklog(1));
    return obj;
}

char *errmessage(V obj)
{
    V d = errget(obj, "DESCRIPTION"), op = errget(obj, "OPERATION");
    V sub = errget(obj, "SUBSYSTEM"), code = errget(obj, "SUBCODE");
    Sb m;
    sb_init(&m);
    sb_adds(&m, "Error ");
    if (isstr(sub)) sb_addn(&m, SS(sub), SL(sub));
    sb_adds(&m, xfmt("/%ld  ", isnum(code) ? intval(code) : 0L));
    if (isstr(d)) sb_addn(&m, SS(d), SL(d));
    if (isstr(op) && SL(op) > 0) {
        sb_adds(&m, ": ");
        sb_addn(&m, SS(op), SL(op));
    }
    return xstrdup(sb_str(&m));
}

void fatalerror(V obj)
{
    long i;
    flushout();
    if (outcol > 0) {
        fputc('\n', stdout);
        outcol = 0;
    }
    errout(errmessage(obj));
    for (i = pctop; i >= 1; i--)
        errout(xfmt("Called from %s(%d)", pcname[i], pcln[i]));
    closeallareas();
    exit(1);
}

V rterr(const char *sub, int code, const char *desc, const char *op)
{
    V obj;
    if (ctl == E_BRK) return NILV;
    obj = mkerror(sub, code, desc, op);
    if (!isnil(errblk) && errdepth == 0) {
        errdepth++;
        evalblock1(errblk, obj);
        errdepth--;
        return NILV;
    }
    if (seqdepth > 0) {
        brkval = obj;
        ctl = E_BRK;
        return NILV;
    }
    fatalerror(obj);
    return NILV;
}

/* ---------------------------------------- objects */
int findmethod(int cls, int sym)
{
    int c = cls, f;
    while (c > 0) {
        f = pm_get(1, c, sym);
        if (f > 0) return f;
        c = clpar[c];
    }
    return 0;
}

int findslot(int cls, int sym)
{
    int c = cls, s;
    while (c > 0) {
        s = pm_get(2, c, sym);
        if (s > 0) return s;
        c = clpar[c];
    }
    return 0;
}

int findcdata(int cls, int sym)
{
    int c = cls, s;
    while (c > 0) {
        s = pm_get(3, c, sym);
        if (s > 0) return s;
        c = clpar[c];
    }
    return 0;
}

int isderived(int cls, int anc)
{
    int c = cls;
    while (c > 0) {
        if (c == anc) return 1;
        c = clpar[c];
    }
    return 0;
}

V newobject(int cl)
{
    V obj;
    struct XObj *o;
    int chain[64], nchain = 0, c, k;
    long i;
    o = (struct XObj *)xmalloc((long)sizeof(struct XObj));
    o->cls = cl;
    o->slots = mkarray(clnslot[cl]);
    obj.t = T_OBJ;
    obj.w = obj.d = obj.f = 0;
    obj.u.o = o;
    c = cl;
    while (c > 0 && nchain < 64) {
        chain[nchain++] = c;
        c = clpar[c];
    }
    for (k = nchain - 1; k >= 0; k--) {
        c = chain[k];
        for (i = 1; i <= ncinit; i++)
            if (cinitcls[i] == c) {
                V v = evalnode(cinitnode[i]);
                arrset(o->slots, cinitslot[i], v);
            }
    }
    return obj;
}

static V nosuchmethod(V obj, int sym)
{
    char *cn;
    if (isobj(obj)) cn = clname[obj.u.o->cls];
    else {
        cn = (char *)xmalloc(2);
        cn[0] = typech(obj);
        cn[1] = 0;
    }
    return rterr("BASE", 1004, "No exported method",
                 xfmt("%s:%s", cn, snm[sym]));
}

/* messages of every object */
static V objbuiltin(V obj, int sym, long base, int cnt)
{
    const char *nm = snm[sym];
    int cls = obj.u.o->cls;
    (void)cnt;
    if (strcmp(nm, "NEW") == 0 || strcmp(nm, "INIT") == 0)
        return obj;
    if (strcmp(nm, "CLASSNAME") == 0) return mkstr(clname[cls]);
    if (strcmp(nm, "ISDERIVEDFROM") == 0 ||
        strcmp(nm, "ISKINDOF") == 0) {
        V a = fv[base + 1];
        if (isstr(a)) {
            int sy_ = intern(xupper(SS(a)));
            int c2 = scls[sy_];
            return mklog(isderived(cls, c2));
        }
        return mklog(1);
    }
    if (strcmp(nm, "CLONE") == 0) {
        V o2, s1 = obj.u.o->slots, s2;
        struct XObj *p;
        long i;
        p = (struct XObj *)xmalloc((long)sizeof(struct XObj));
        p->cls = cls;
        s2 = mkarray(arrlen(s1));
        for (i = 1; i <= arrlen(s1); i++)
            arrset(s2, i, arrget(s1, i));
        p->slots = s2;
        o2.t = T_OBJ;
        o2.w = o2.d = o2.f = 0;
        o2.u.o = p;
        return o2;
    }
    if (strcmp(nm, "ISOBJECT") == 0) return mklog(1);
    return nosuchmethod(obj, sym);
}

V evsend(int n)
{
    int onode = na[n], superm = 0, supcls = 0, sym, cnt, f, sc, cls;
    int sl, cd, i;
    long base, slots;
    V obj, r;
    if (nt[onode] == K_SELF) {
        obj = curself;
        if (na[onode] == 1) superm = 1;
        if (na[onode] == 2) {
            superm = 2;
            supcls = scls[nb[onode]];
        }
    } else obj = evalnode(onode);
    if (ctl != 0) return NILV;
    sym = nb[n];
    cnt = nd[n];
    if (!isobj(obj)) {
        evalargs(nc[n], ftop, 0);
        return nosuchmethod(obj, sym);
    }
    cls = obj.u.o->cls;
    sc = cls;
    if (superm == 1) sc = clpar[curmcls];
    if (superm == 2) sc = supcls;
    f = findmethod(sc, sym);
    if (f > 0) {
        base = ftop;
        slots = fnslots[f];
        if (cnt > slots) slots = cnt;
        SETFTOP(base + slots);
        for (i = 1; i <= slots; i++) fv[base + i] = NILV;
        evalargs(nc[n], base, fnpar[f]);
        if (ctl != 0) {
            ftop = base;
            return NILV;
        }
        return invoke(f, base, cnt, obj, n);
    }
    sl = findslot(cls, sym);
    if (sl > 0) {
        evalargs(nc[n], ftop, 0);
        return arrget(obj.u.o->slots, sl);
    }
    cd = findcdata(cls, sym);
    if (cd > 0) {
        evalargs(nc[n], ftop, 0);
        return deref(sv[cd]);
    }
    base = ftop;
    SETFTOP(base + cnt + 8);
    for (i = 1; i <= cnt + 8; i++) fv[base + i] = NILV;
    evalargs(nc[n], base, cnt);
    r = NILV;
    if (ctl == 0) r = objbuiltin(obj, sym, base, cnt);
    ftop = base;
    return r;
}

void sendassign(int n, V val)
{
    int onode = na[n], sym, cls, sl, cd;
    V obj, v;
    if (nt[onode] == K_SELF) obj = curself;
    else obj = evalnode(onode);
    if (ctl != 0) return;
    sym = nb[n];
    if (!isobj(obj)) {
        rterr("BASE", 1005, "No exported variable", snm[sym]);
        return;
    }
    cls = obj.u.o->cls;
    sl = findslot(cls, sym);
    if (sl > 0) {
        arrset(obj.u.o->slots, sl, val);
        return;
    }
    cd = findcdata(cls, sym);
    if (cd > 0) {
        v = sv[cd];
        if (v.t == T_REF) {
            v.u.r->v = val;
            return;
        }
        sv[cd] = val;
        return;
    }
    rterr("BASE", 1005, "No exported variable",
          xfmt("%s:%s", clname[cls], snm[sym]));
}

/* ---------------------------------------- macros */
V macroeval(const char *s)
{
    int node;
    char *t = xstrip(s);
    if (t[0] == 0) return NILV;
    node = compileexpr(s);
    if (node == 0)
        return rterr("BASE", 1449, "Syntax error",
                     xfmt("macro: %s", s));
    return evalnode(node);
}
