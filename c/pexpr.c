/* pexpr.c -- parser: scopes and expressions
   (port of src/30_pexpr.crexx and compileexpr of src/51_exec) */
#include "xb_parse.h"

int perrflag = 0;
int perrline = 0;
char *perrmsg = "";
int argcount = 0;

/* ------------------------------------------- parse-time scope data */
/* shared with pstmt.c and pclass.c (not part of xb_parse.h) */
int px_curcx = 0;       /* current context */
int px_infunc = 0;
int px_curfn = 0;
int px_nvar = 0;
int *px_cxnslot = 0;    /* slots in use per context */
int px_nmst = 0;        /* module level statics */
int *px_mstname = 0;
int *px_mstidx = 0;

static int *cxkind = 0;         /* 0 function, 1 block */
static int *cxpar = 0;
static int *cxncap = 0;
static long cxcap = 0, cxscap = 0, cxccap = 0, cxpcap = 0;
static int ncx = 0;
static int *vname = 0, *vctx = 0, *vkind = 0, *vidx = 0;
static int *vsrck = 0, *vsrci = 0;
static long vncap = 0, vcxcap = 0, vkcap = 0, vicap = 0;
static long vskcap = 0, vsicap = 0;
static long mncap = 0, micap = 0;
static int rk = 0, ri = 0;

static int parseor(void);
static int parseand(void);
static int parsenot(void);
static int parsecmp(void);
static int parseadd(void);
static int parsemul(void);
static int parsepow(void);
static int parsealiased(int al);
static int parseident(void);
static int parsebrace(void);
static int parseblock(void);
int parseprimary(void);
int isassignable(int n);

/* concatenation without a length limit */
char *px_cat(const char *a, const char *b)
{
    long na1 = (long) strlen(a), nb1 = (long) strlen(b);
    char *r = (char *) xmalloc(na1 + nb1 + 1);

    memcpy(r, a, (size_t) na1);
    memcpy(r + na1, b, (size_t) nb1 + 1);
    return r;
}

void perr(const char *msg)
{
    if (perrflag == 0) {
        perrflag = 1;
        perrmsg = xstrdup(msg);
        perrline = tl[tp];
    }
    tp = ntok;
}

int curk(void)
{
    return tk[tp];
}

void adv(void)
{
    if (tp < ntok) tp++;
}

int isop(const char *s)
{
    if (tk[tp] == E_OP) {
        if (strcmp(ts[tp], s) == 0) return 1;
    }
    return 0;
}

int isword(const char *s)
{
    if (tk[tp] == E_IDENT) {
        if (strcmp(ts[tp], s) == 0) return 1;
    }
    return 0;
}

int nextisop(const char *s)
{
    if (tp < ntok) {
        if (tk[tp + 1] == E_OP) {
            if (strcmp(ts[tp + 1], s) == 0) return 1;
        }
    }
    return 0;
}

int nextisword(const char *s)
{
    if (tp < ntok) {
        if (tk[tp + 1] == E_IDENT) {
            if (strcmp(ts[tp + 1], s) == 0) return 1;
        }
    }
    return 0;
}

void expectop(const char *s)
{
    if (isop(s) == 1) adv();
    else perr(px_cat(xfmt("Syntax error: expected %s near ", s),
                     ts[tp]));
}

void skipeol(void)
{
    while (tk[tp] == E_EOL) adv();
}

int atend(void)
{
    if (tk[tp] == E_EOL || tk[tp] == E_EOF) return 1;
    return 0;
}

/* ------------------------------------------------------- contexts */
/* make the context tables hold index n (element 0 is zero) */
static void cxensure(int n)
{
    long old = cxcap;

    growi(&cxkind, &cxcap, n + 2);
    growi(&cxpar, &cxscap, n + 2);
    growi(&px_cxnslot, &cxccap, n + 2);
    growi(&cxncap, &cxpcap, n + 2);
    if (old == 0) {
        cxkind[0] = 0;
        cxpar[0] = 0;
        px_cxnslot[0] = 0;
        cxncap[0] = 0;
    }
}

int px_newctx(int kind, int par)
{
    ncx++;
    cxensure(ncx);
    cxkind[ncx] = kind;
    cxpar[ncx] = par;
    px_cxnslot[ncx] = 0;
    cxncap[ncx] = 0;
    return ncx;
}

int px_declvar(int sym, int kind, int idx)
{
    px_nvar++;
    growi(&vname, &vncap, px_nvar + 2);
    growi(&vctx, &vcxcap, px_nvar + 2);
    growi(&vkind, &vkcap, px_nvar + 2);
    growi(&vidx, &vicap, px_nvar + 2);
    growi(&vsrck, &vskcap, px_nvar + 2);
    growi(&vsrci, &vsicap, px_nvar + 2);
    vname[px_nvar] = sym;
    vctx[px_nvar] = px_curcx;
    vkind[px_nvar] = kind;
    vidx[px_nvar] = idx;
    vsrck[px_nvar] = 0;
    vsrci[px_nvar] = 0;
    return px_nvar;
}

int px_newslot(void)
{
    px_cxnslot[px_curcx]++;
    return px_cxnslot[px_curcx];
}

void px_addmst(int sym, int idx)
{
    px_nmst++;
    growi(&px_mstname, &mncap, px_nmst + 2);
    growi(&px_mstidx, &micap, px_nmst + 2);
    px_mstname[px_nmst] = sym;
    px_mstidx[px_nmst] = idx;
}

static int findvarin(int cx, int sym)
{
    int i = px_nvar;

    while (i >= 1) {
        if (vname[i] == sym) {
            if (vctx[i] == cx) return i;
        }
        i--;
    }
    return 0;
}

static int resolvein(int cx, int sym)
{
    int v, par, sk, si, idx;

    v = findvarin(cx, sym);
    if (v > 0) {
        rk = vkind[v];
        ri = vidx[v];
        return 1;
    }
    if (cxkind[cx] == 1) {
        par = cxpar[cx];
        if (resolvein(par, sym) == 1) {
            if (rk == 2) return 1;
            sk = rk;
            si = ri;
            if (sk == 3) sk = 2;
            idx = cxncap[cx] + 1;
            cxncap[cx] = idx;
            px_nvar++;
            growi(&vname, &vncap, px_nvar + 2);
            growi(&vctx, &vcxcap, px_nvar + 2);
            growi(&vkind, &vkcap, px_nvar + 2);
            growi(&vidx, &vicap, px_nvar + 2);
            growi(&vsrck, &vskcap, px_nvar + 2);
            growi(&vsrci, &vsicap, px_nvar + 2);
            vname[px_nvar] = sym;
            vctx[px_nvar] = cx;
            vkind[px_nvar] = 3;
            vidx[px_nvar] = idx;
            vsrck[px_nvar] = sk;
            vsrci[px_nvar] = si;
            rk = 3;
            ri = idx;
            return 1;
        }
    }
    return 0;
}

/* a variable reference node, or 0 if not a LOCAL or STATIC */
int resolve(int sym)
{
    int i;

    if (px_curcx > 0) {
        if (resolvein(px_curcx, sym) == 1) {
            if (rk == 1) return mknode(K_LOCAL, ri, 0, 0);
            if (rk == 2) return mknode(K_STATIC, ri, 0, 0);
            return mknode(K_CAPV, ri, 0, 0);
        }
    }
    for (i = px_nmst; i >= 1; i--) {
        if (px_mstname[i] == sym)
            return mknode(K_STATIC, px_mstidx[i], 0, 0);
    }
    return 0;
}

/* ----------------------------------------------- expressions */
int parseexpr(void)
{
    return parseassign();
}

int isassignable(int n)
{
    int k = nt[n];

    if (k == K_LOCAL || k == K_CAPV || k == K_STATIC
        || k == K_IDENT || k == K_MEMVAR || k == K_FIELDX
        || k == K_ALIASFLD || k == K_INDEX || k == K_MACRO)
        return 1;
    if (k == K_SEND) {
        if (ne[n] == 1) return 1;
    }
    return 0;
}

int parseassign(void)
{
    int lhs, rhs, bk, t;
    const char *op;

    lhs = parseor();
    if (tk[tp] != E_OP) return lhs;
    op = ts[tp];
    if (strcmp(op, ":=") == 0) {
        if (isassignable(lhs) == 0) {
            perr("Syntax error: invalid assignment target");
            return lhs;
        }
        adv();
        rhs = parseassign();
        return mknode(K_ASSIGN, lhs, rhs, 0);
    }
    bk = 0;
    if (strcmp(op, "+=") == 0) bk = K_ADD;
    else if (strcmp(op, "-=") == 0) bk = K_SUB;
    else if (strcmp(op, "*=") == 0) bk = K_MUL;
    else if (strcmp(op, "/=") == 0) bk = K_DIV;
    else if (strcmp(op, "%=") == 0) bk = K_MOD;
    else if (strcmp(op, "^=") == 0 || strcmp(op, "**=") == 0)
        bk = K_POW;
    if (bk == 0) return lhs;
    if (isassignable(lhs) == 0) {
        perr("Syntax error: invalid assignment target");
        return lhs;
    }
    adv();
    rhs = parseassign();
    t = mknode(bk, lhs, rhs, 0);
    return mknode(K_ASSIGN, lhs, t, 0);
}

static int parseor(void)
{
    int l, r;

    l = parseand();
    while (isop(".OR.") == 1) {
        adv();
        r = parseand();
        l = mknode(K_OR, l, r, 0);
    }
    return l;
}

static int parseand(void)
{
    int l, r;

    l = parsenot();
    while (isop(".AND.") == 1) {
        adv();
        r = parsenot();
        l = mknode(K_AND, l, r, 0);
    }
    return l;
}

static int parsenot(void)
{
    int e;

    if (isop(".NOT.") == 1 || isop("!") == 1) {
        adv();
        e = parsenot();
        return mknode(K_NOT, e, 0, 0);
    }
    return parsecmp();
}

static int parsecmp(void)
{
    int l, r, k;
    const char *op;

    l = parseadd();
    for (;;) {
        if (tk[tp] != E_OP) break;
        op = ts[tp];
        k = 0;
        if (strcmp(op, "=") == 0) k = K_EQ;
        else if (strcmp(op, "==") == 0) k = K_EXEQ;
        else if (strcmp(op, "!=") == 0 || strcmp(op, "<>") == 0
                 || strcmp(op, "#") == 0) k = K_NE;
        else if (strcmp(op, "<") == 0) k = K_LT;
        else if (strcmp(op, "<=") == 0) k = K_LE;
        else if (strcmp(op, ">") == 0) k = K_GT;
        else if (strcmp(op, ">=") == 0) k = K_GE;
        else if (strcmp(op, "$") == 0) k = K_IN;
        if (k == 0) break;
        adv();
        r = parseadd();
        l = mknode(k, l, r, 0);
    }
    return l;
}

static int parseadd(void)
{
    int l, r, k;
    const char *op;

    l = parsemul();
    for (;;) {
        if (tk[tp] != E_OP) break;
        op = ts[tp];
        if (strcmp(op, "+") == 0) k = K_ADD;
        else if (strcmp(op, "-") == 0) k = K_SUB;
        else break;
        adv();
        r = parsemul();
        l = mknode(k, l, r, 0);
    }
    return l;
}

static int parsemul(void)
{
    int l, r, k;
    const char *op;

    l = parseunary();
    for (;;) {
        if (tk[tp] != E_OP) break;
        op = ts[tp];
        if (strcmp(op, "*") == 0) k = K_MUL;
        else if (strcmp(op, "/") == 0) k = K_DIV;
        else if (strcmp(op, "%") == 0) k = K_MOD;
        else break;
        adv();
        r = parseunary();
        l = mknode(k, l, r, 0);
    }
    return l;
}

int parseunary(void)
{
    int e;
    const char *op;
    V v;

    if (tk[tp] == E_OP) {
        op = ts[tp];
        if (strcmp(op, "-") == 0) {
            adv();
            e = parseunary();
            /* fold literal numbers */
            if (nt[e] == K_CONST) {
                v = nk[e];
                if (isnum(v)) {
                    v.u.n = 0.0 - v.u.n;
                    return constnode(v);
                }
            }
            return mknode(K_NEG, e, 0, 0);
        }
        if (strcmp(op, "+") == 0) {
            adv();
            return parseunary();
        }
        if (strcmp(op, "++") == 0) {
            adv();
            e = parseunary();
            return mknode(K_PREINC, e, 0, 0);
        }
        if (strcmp(op, "--") == 0) {
            adv();
            e = parseunary();
            return mknode(K_PREDEC, e, 0, 0);
        }
    }
    return parsepow();
}

static int parsepow(void)
{
    int l, r;

    l = parsepostfix();
    while (isop("^") == 1 || isop("**") == 1) {
        adv();
        r = parseunary();
        l = mknode(K_POW, l, r, 0);
    }
    return l;
}

/* argument list inside ( ); the opening ( is consumed.  Returns the
   head, sets argcount */
int parseargs(void)
{
    int head = 0, tail = 0, cnt = 0, e;

    if (isop(")") == 1) {
        adv();
        argcount = 0;
        return 0;
    }
    for (;;) {
        if (isop(",") == 1 || isop(")") == 1) e = constnode(NILV);
        else e = parseexpr();
        cnt++;
        if (head == 0) head = e;
        else nx[tail] = e;
        tail = e;
        if (isop(",") == 1) {
            adv();
            continue;
        }
        break;
    }
    expectop(")");
    argcount = cnt;
    return head;
}

int parsepostfix(void)
{
    int e, ix, msg, args, cnt;
    const char *op;

    e = parseprimary();
    for (;;) {
        if (tk[tp] != E_OP) break;
        op = ts[tp];
        if (strcmp(op, "[") == 0) {
            adv();
            ix = parseexpr();
            e = mknode(K_INDEX, e, ix, 0);
            while (isop(",") == 1) {
                adv();
                ix = parseexpr();
                e = mknode(K_INDEX, e, ix, 0);
            }
            expectop("]");
            continue;
        }
        if (strcmp(op, ":") == 0) {
            adv();
            if (tk[tp] != E_IDENT) {
                perr("Syntax error: message name expected");
                return e;
            }
            msg = tv[tp];
            adv();
            if (isop("(") == 1) {
                adv();
                args = parseargs();
                cnt = argcount;
                e = mknode(K_SEND, e, msg, args);
                nd[e] = cnt;
                ne[e] = 0;
            } else {
                e = mknode(K_SEND, e, msg, 0);
                nd[e] = 0;
                ne[e] = 1;
            }
            continue;
        }
        if (strcmp(op, "++") == 0) {
            adv();
            e = mknode(K_POSTINC, e, 0, 0);
            continue;
        }
        if (strcmp(op, "--") == 0) {
            adv();
            e = mknode(K_POSTDEC, e, 0, 0);
            continue;
        }
        break;
    }
    return e;
}

static V numconst(const char *t)
{
    long len = (long) strlen(t);
    const char *dot = strchr(t, '.');
    int d = 0, w, hasE = 0;
    long i;
    double x;
    V v;

    if (dot != NULL) d = (int) (len - ((long) (dot - t) + 1));
    for (i = 0; i < len; i++) {
        if (t[i] == 'e' || t[i] == 'E') hasE = 1;
    }
    if (d == 0 && hasE == 0 && len < 18)       /* 32-bit long too */
        return mkwhole(strtod(t, NULL));
    x = strtod(t, NULL);
    w = (int) len;
    if (w < 3) w = 3;
    if (hasE) {
        d = 0;
        w = 10;
    }
    if (w > 255) w = 255;
    if (d > 255) d = 255;
    v.t = T_NUM;
    v.w = (unsigned char) w;
    v.d = (unsigned char) d;
    v.f = 0;
    v.u.n = x;
    return v;
}

static int fieldn(const char *t, long from, long len)
{
    long i, n = (long) strlen(t);
    int r = 0;

    for (i = from; i < from + len && i < n; i++) {
        if (t[i] < '0' || t[i] > '9') break;
        r = r * 10 + (t[i] - '0');
    }
    return r;
}

static V datelit(const char *t)
{
    int y, m, d;

    if (strlen(t) < 8) return mkdate(0);
    y = fieldn(t, 0, 4);
    m = fieldn(t, 4, 2);
    d = fieldn(t, 6, 2);
    if (y == 0 && m == 0 && d == 0) return mkdate(0);
    return mkdate(ymd2jdn(y, m, d));
}

int parseprimary(void)
{
    int k, e, sym, v, n, selfn, msg, args, cnt;
    const char *op;
    V cv;

    k = tk[tp];
    if (k == E_NUM) {
        cv = numconst(ts[tp]);
        adv();
        return constnode(cv);
    }
    if (k == E_STR) {
        cv = mkstr(ts[tp]);
        adv();
        return constnode(cv);
    }
    if (k == E_LOG) {
        cv = mklog(strcmp(ts[tp], "T") == 0);
        adv();
        return constnode(cv);
    }
    if (k == E_DATE) {
        cv = datelit(ts[tp]);
        adv();
        return constnode(cv);
    }
    if (k == E_IDENT) return parseident();
    if (k == E_OP) {
        op = ts[tp];
        if (strcmp(op, "(") == 0) {
            adv();
            e = parseexpr();
            expectop(")");
            if (isop("->") == 1) {
                adv();
                return parsealiased(e);
            }
            /* a parenthesised expression is never a target */
            return mknode(K_PLUS, e, 0, 0);
        }
        if (strcmp(op, "{") == 0) return parsebrace();
        if (strcmp(op, "@") == 0) {
            adv();
            if (tk[tp] == E_IDENT) {
                sym = tv[tp];
                if (nextisop("(") == 0) {
                    e = parsepostfix();
                    return mknode(K_REFX, e, 0, 0);
                }
                /* @Func() : function pointer */
                adv();
                adv();
                expectop(")");
                return mknode(K_CODEBLOCK, sym, 0, 0);
            }
            e = parsepostfix();
            return mknode(K_REFX, e, 0, 0);
        }
        if (strcmp(op, "&") == 0) {
            adv();
            if (isop("(") == 1) {
                adv();
                e = parseexpr();
                expectop(")");
                return mknode(K_MACRO, e, 0, 0);
            }
            if (tk[tp] == E_IDENT) {
                sym = tv[tp];
                adv();
                v = resolve(sym);
                if (v == 0) v = mknode(K_IDENT, sym, 0, 0);
                n = mknode(K_MACRO, v, 0, 0);
                /* &var.text : trailing dot ends the macro name */
                if (isop(".") == 1 && tsp[tp] == 0) adv();
                return n;
            }
            perr("Syntax error: macro operator");
            return constnode(NILV);
        }
        if (strcmp(op, "::") == 0) {
            adv();
            if (tk[tp] != E_IDENT) {
                perr("Syntax error: message name expected after ::");
                return constnode(NILV);
            }
            msg = tv[tp];
            adv();
            selfn = mknode(K_SELF, 0, 0, 0);
            if (isop(":") == 1 && tp < ntok) {
                if (tk[tp + 1] == E_IDENT) {
                    /* ::Class:Method( ) calls that class's method */
                    na[selfn] = 2;
                    nb[selfn] = msg;
                    adv();
                    msg = tv[tp];
                    adv();
                }
            }
            if (isop("(") == 1) {
                adv();
                args = parseargs();
                cnt = argcount;
                e = mknode(K_SEND, selfn, msg, args);
                nd[e] = cnt;
                ne[e] = 0;
                return e;
            }
            e = mknode(K_SEND, selfn, msg, 0);
            nd[e] = 0;
            ne[e] = 1;
            return e;
        }
    }
    perr(px_cat("Syntax error near ", ts[tp]));
    return constnode(NILV);
}

/* the thing after "alias->" : field name or (expression) */
static int parsealiased(int al)
{
    int sym, e;

    if (tk[tp] == E_IDENT) {
        sym = tv[tp];
        adv();
        return mknode(K_ALIASFLD, al, sym, 0);
    }
    if (isop("(") == 1) {
        adv();
        e = parseexpr();
        expectop(")");
        return mknode(K_ALIASEXPR, al, e, 0);
    }
    perr("Syntax error after ->");
    return constnode(NILV);
}

static int parseident(void)
{
    const char *name = ts[tp];
    int sym = tv[tp];
    int s2, al, args, cnt, c1n, c2n, c3n, v;

    adv();
    if (isop("->") == 1) {
        adv();
        if (strcmp(name, "M") == 0 || strcmp(name, "MEMVAR") == 0) {
            if (tk[tp] == E_IDENT) {
                s2 = tv[tp];
                adv();
                return mknode(K_MEMVAR, s2, 0, 0);
            }
        }
        if (strcmp(name, "FIELD") == 0
            || strcmp(name, "_FIELD") == 0) {
            if (tk[tp] == E_IDENT) {
                s2 = tv[tp];
                adv();
                return mknode(K_FIELDX, s2, 0, 0);
            }
        }
        al = constnode(mkstr(name));
        return parsealiased(al);
    }
    if (isop("(") == 1) {
        adv();
        args = parseargs();
        cnt = argcount;
        if ((strcmp(name, "IIF") == 0 || strcmp(name, "IF") == 0)
            && cnt == 3) {
            c1n = args;
            c2n = nx[c1n];
            c3n = nx[c2n];
            return mknode(K_IIF, c1n, c2n, c3n);
        }
        return mknode(K_CALL, sym, args, cnt);
    }
    if (strcmp(name, "NIL") == 0) return constnode(NILV);
    if (strcmp(name, "SELF") == 0) return mknode(K_SELF, 0, 0, 0);
    if (strcmp(name, "SUPER") == 0) return mknode(K_SELF, 1, 0, 0);
    v = resolve(sym);
    if (v > 0) return v;
    return mknode(K_IDENT, sym, 0, 0);
}

/* { ... } : array, hash or code block */
static int parsebrace(void)
{
    int head = 0, tail = 0, cnt = 0, ish = 0, e, v;

    adv();
    if (isop("|") == 1) return parseblock();
    /* empty hash {=>} */
    if (isop("=>") == 1) {
        adv();
        expectop("}");
        return mknode(K_HASHLIT, 0, 0, 0);
    }
    if (isop("}") == 1) {
        adv();
        return mknode(K_ARRAY, 0, 0, 0);
    }
    for (;;) {
        e = parseexpr();
        if (cnt == 0 && isop("=>") == 1) ish = 1;
        if (ish == 1) {
            expectop("=>");
            v = parseexpr();
            nx[e] = v;
            if (head == 0) head = e;
            else nx[tail] = e;
            tail = v;
        } else {
            if (head == 0) head = e;
            else nx[tail] = e;
            tail = e;
        }
        cnt++;
        if (isop(",") == 1) {
            adv();
            continue;
        }
        break;
    }
    expectop("}");
    if (ish == 1) return mknode(K_HASHLIT, head, cnt, 0);
    return mknode(K_ARRAY, head, cnt, 0);
}

static int parseblock(void)
{
    int oldcx, cx, np, s, sl, head, tail, e, chead, ctail, cn, b;
    int i, j;

    /* current token is the opening | */
    adv();
    oldcx = px_curcx;
    cx = px_newctx(1, oldcx);
    px_curcx = cx;
    np = 0;
    while (isop("|") == 0) {
        if (tk[tp] != E_IDENT) {
            perr("Syntax error in code block parameters");
            px_curcx = oldcx;
            return constnode(NILV);
        }
        s = tv[tp];
        adv();
        np++;
        sl = px_newslot();
        px_declvar(s, 1, sl);
        if (isop(",") == 1) adv();
    }
    adv();
    head = 0;
    tail = 0;
    if (isop("}") == 0) {
        for (;;) {
            e = parseexpr();
            if (head == 0) head = e;
            else nx[tail] = e;
            tail = e;
            if (isop(",") == 1) {
                adv();
                continue;
            }
            break;
        }
    } else {
        head = constnode(NILV);
    }
    expectop("}");
    /* capture list */
    chead = 0;
    ctail = 0;
    for (i = 1; i <= cxncap[cx]; i++) {
        for (j = px_nvar; j >= 1; j--) {
            if (vctx[j] == cx && vkind[j] == 3 && vidx[j] == i) {
                cn = mknode(K_NOP, vsrck[j], vsrci[j], 0);
                if (chead == 0) chead = cn;
                else nx[ctail] = cn;
                ctail = cn;
                break;
            }
        }
    }
    px_curcx = oldcx;
    b = mknode(K_BLOCK, head, np, px_cxnslot[cx]);
    nd[b] = chead;
    nf[b] = cxncap[cx];
    return b;
}

/* ---------------------------------------------------------- macros */
int compileexpr(const char *s)
{
    char *nm;
    int key, otp, ontok, ocx, oinf, onv, ocf, ofl, oblk, start;
    int node, bad;

    nm = (char *) xmalloc((long) strlen(s) + 3);
    strcpy(nm, "M$");
    strcpy(nm + 2, s);
    key = intern(nm);
    if (smacro[key] > 0) return smacro[key];
    otp = (int) tp;
    ontok = (int) ntok;
    ocx = px_curcx;
    oinf = px_infunc;
    onv = px_nvar;
    ocf = px_curfn;
    ofl = curline;
    oblk = inblk;
    start = (int) ntok + 1;
    perrflag = 0;
    inblk = 0;
    lexchunk(s, 0);
    addtok(E_EOL, "", 0);
    addtok(E_EOF, "", 0);
    tp = start;
    px_curcx = 0;
    px_infunc = 0;
    node = parseexpr();
    if (tk[tp] != E_EOL && perrflag == 0)
        perr(px_cat("Syntax error in macro near ", ts[tp]));
    bad = perrflag;
    perrflag = 0;
    tp = otp;
    ntok = ontok;
    px_curcx = ocx;
    px_infunc = oinf;
    px_nvar = onv;
    px_curfn = ocf;
    curline = ofl;
    inblk = oblk;
    if (bad == 1) return 0;
    smacro[key] = node;
    return node;
}
