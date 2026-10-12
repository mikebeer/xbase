/* pclass.c -- parser: functions, classes and the whole program
   (port of src/33_pclass.crexx; newclass and adddata are in core.c) */
#include "xb_parse.h"

/* from pexpr.c (private to the parser files) */
extern int px_curcx, px_infunc, px_curfn, px_nvar;
extern int *px_cxnslot;
int px_newctx(int kind, int par);
int px_declvar(int sym, int kind, int idx);
int px_newslot(void);
char *px_cat(const char *a, const char *b);

static int savenvar = 0;

static int parsefuncdef(void);
static int parseclass(void);
int parsestatic(void);
static int parsemethoddef(void);

/* start a function: parses the optional parameter list; leaves the
   context open */
static int beginfunc(const char *name)
{
    int fid, np, sym, sl;

    fid = newfn(name);
    px_curfn = fid;
    px_infunc = 1;
    savenvar = px_nvar;
    px_curcx = px_newctx(0, 0);
    np = 0;
    if (isop("(") == 1) {
        adv();
        while (isop(")") == 0) {
            if (tk[tp] == E_OP) {
                if (strcmp(ts[tp], "@") == 0) adv();
            }
            if (isop(".") == 1) {
                /* ... */
                adv();
                adv();
                adv();
                continue;
            }
            if (tk[tp] != E_IDENT) {
                perr("Syntax error in parameter list");
                break;
            }
            sym = tv[tp];
            adv();
            np++;
            sl = px_newslot();
            px_declvar(sym, 1, sl);
            if (isword("AS") == 1) {
                adv();
                adv();
            }
            if (isop(",") == 1) adv();
        }
        expectop(")");
    }
    fnpar[fid] = np;
    return fid;
}

static void endfunc(int fid, int body, int oldnvar)
{
    fnbody[fid] = body;
    fnslots[fid] = px_cxnslot[px_curcx];
    if (fnslots[fid] < fnpar[fid]) fnslots[fid] = fnpar[fid];
    px_nvar = oldnvar;
    px_curcx = 0;
    px_infunc = 0;
    px_curfn = 0;
}

/* at FUNCTION / PROCEDURE (STATIC / INIT / EXIT already consumed) */
static int parsefuncdef(void)
{
    const char *name;
    int sym, fid, oldnvar, body;

    adv();
    if (tk[tp] != E_IDENT) {
        perr("Syntax error: function name expected");
        return 0;
    }
    name = ts[tp];
    sym = tv[tp];
    adv();
    fid = beginfunc(name);
    oldnvar = savenvar;
    skipline();
    body = parsestmts();
    endfunc(fid, body, oldnvar);
    sfn[sym] = fid;
    if (firstfn == 0) firstfn = fid;
    if (strcmp(name, "MAIN") == 0) mainfn = fid;
    return fid;
}

/* at CLASS */
static int parseclass(void)
{
    const char *name, *w, *mname;
    int par, cls, shared, sym, init, slot, st, msym, inl, fid;
    int oldnvar, e, body, jj;

    adv();
    if (tk[tp] != E_IDENT) {
        perr("Syntax error: class name expected");
        return 0;
    }
    name = ts[tp];
    adv();
    par = 0;
    if (isword("INHERIT") == 1 || isword("FROM") == 1
        || isword("INHERITS") == 1) {
        adv();
        if (tk[tp] == E_IDENT) {
            par = scls[tv[tp]];
            if (par == 0)
                perr(px_cat("Parent class not found: ", ts[tp]));
            adv();
        }
        while (isop(",") == 1) {
            adv();
            adv();
        }
    }
    skipline();
    cls = newclass(name, par);
    for (;;) {
        skipeol();
        if (tk[tp] == E_EOF || perrflag == 1) break;
        if (tk[tp] != E_IDENT) {
            skipline();
            continue;
        }
        w = ts[tp];
        if (strcmp(w, "ENDCLASS") == 0) {
            adv();
            break;
        }
        if (strcmp(w, "END") == 0) {
            adv();
            if (isword("CLASS") == 1) adv();
            break;
        }
        shared = 0;
        if (strcmp(w, "CLASS") == 0
            && (nextisword("VAR") == 1 || nextisword("DATA") == 1)) {
            adv();
            w = ts[tp];
            shared = 1;
        }
        if (strcmp(w, "CLASSDATA") == 0
            || strcmp(w, "CLASSVAR") == 0) shared = 1;
        if (strcmp(w, "VAR") == 0 || strcmp(w, "DATA") == 0
            || strcmp(w, "CLASSDATA") == 0
            || strcmp(w, "CLASSVAR") == 0
            || strcmp(w, "MEMBER") == 0) {
            adv();
            for (;;) {
                if (tk[tp] != E_IDENT) break;
                sym = tv[tp];
                adv();
                init = 0;
                for (;;) {
                    if (isword("AS") == 1) {
                        adv();
                        adv();
                        continue;
                    }
                    if (isword("INIT") == 1) {
                        adv();
                        init = parseexpr();
                        continue;
                    }
                    if (isword("READONLY") == 1
                        || isword("EXPORTED") == 1
                        || isword("VISIBLE") == 1
                        || isword("HIDDEN") == 1
                        || isword("PROTECTED") == 1
                        || isword("PERSISTENT") == 1
                        || isword("SHARED") == 1
                        || isword("EXPORT") == 1) {
                        adv();
                        continue;
                    }
                    break;
                }
                if (shared == 1) {
                    st = newstatic();
                    pm_put(3, cls, sym, st);
                    if (init > 0) add_svinit(init, st);
                } else {
                    slot = adddata(cls, sym);
                    if (init > 0) add_cinit(cls, slot, init);
                }
                if (isop(",") == 1) {
                    adv();
                    continue;
                }
                break;
            }
            skipline();
            continue;
        }
        if (strcmp(w, "METHOD") == 0 || strcmp(w, "ACCESS") == 0
            || strcmp(w, "ASSIGN") == 0
            || strcmp(w, "MESSAGE") == 0
            || strcmp(w, "CONSTRUCTOR") == 0
            || strcmp(w, "DESTRUCTOR") == 0) {
            adv();
            if (tk[tp] != E_IDENT) {
                skipline();
                continue;
            }
            mname = ts[tp];
            msym = tv[tp];
            adv();
            /* look ahead for INLINE on this line */
            inl = 0;
            jj = (int) tp;
            while (tk[jj] != E_EOL && tk[jj] != E_EOF) {
                if (tk[jj] == E_IDENT) {
                    if (strcmp(ts[jj], "INLINE") == 0) {
                        inl = jj;
                        break;
                    }
                }
                jj++;
            }
            if (inl > 0) {
                fid = beginfunc(mname);
                oldnvar = savenvar;
                if (isword("INLINE") == 1) adv();
                e = parseexpr();
                body = mknode(K_RETURN, e, 0, 0);
                body = mknode(K_STMTS, body, 0, 0);
                endfunc(fid, body, oldnvar);
                fnclass[fid] = cls;
                pm_put(1, cls, msym, fid);
                skipline();
            } else {
                pm_put(4, cls, msym, 1);
                skipline();
            }
            continue;
        }
        skipline();
    }
    return 0;
}

/* METHOD name( params ) CLASS cls  -- the definition with a body */
static int parsemethoddef(void)
{
    const char *mname, *clsname;
    int msym, fid, oldnvar, cls, body;
    char *u;

    adv();
    if (tk[tp] != E_IDENT) {
        perr("Syntax error: method name expected");
        return 0;
    }
    mname = ts[tp];
    msym = tv[tp];
    adv();
    clsname = "";
    if (isop(":") == 1) {
        /* METHOD Class:name(...) */
        clsname = mname;
        adv();
        mname = ts[tp];
        msym = tv[tp];
        adv();
    }
    fid = beginfunc(mname);
    oldnvar = savenvar;
    if (isword("CLASS") == 1) {
        adv();
        clsname = ts[tp];
        adv();
    }
    if (isword("CONSTRUCTOR") == 1) adv();
    u = xupper(clsname);
    { int sy_ = intern(u); cls = scls[sy_]; }
    if (cls == 0) {
        perr(px_cat("Class not found: ", clsname));
        return 0;
    }
    skipline();
    body = parsestmts();
    endfunc(fid, body, oldnvar);
    fnclass[fid] = cls;
    pm_put(1, cls, msym, fid);
    return fid;
}

/* the whole program */
int parseprogram(void)
{
    const char *w;
    char *stem, *k, *u;
    int fid, oldnvar, body, junk, isinit, isexit;

    px_curcx = 0;
    px_infunc = 0;
    /* implicit main procedure for statements before the first
       function */
    skipeol();
    if (tk[tp] == E_IDENT) {
        w = ts[tp];
        if (strcmp(w, "FUNCTION") != 0
            && strcmp(w, "PROCEDURE") != 0
            && strcmp(w, "STATIC") != 0 && strcmp(w, "INIT") != 0
            && strcmp(w, "EXIT") != 0 && strcmp(w, "CREATE") != 0
            && strcmp(w, "CLASS") != 0 && strcmp(w, "METHOD") != 0
            && strcmp(w, "REQUEST") != 0
            && strcmp(w, "EXTERNAL") != 0
            && strcmp(w, "ANNOUNCE") != 0
            && strcmp(w, "DYNAMIC") != 0 && strcmp(w, "FUNC") != 0
            && strcmp(w, "PROC") != 0 && strcmp(w, "MEMVAR") != 0
            && strcmp(w, "FIELD") != 0
            && strcmp(w, "DECLARE") != 0) {
            stem = srcname;
            k = strrchr(stem, '/');
            if (k != NULL) stem = k + 1;
            stem = xstrdup(stem);
            k = strrchr(stem, '.');
            if (k != NULL && k > stem) *k = 0;
            u = xupper(stem);
            fid = beginfunc(u);
            oldnvar = savenvar;
            body = parsestmts();
            endfunc(fid, body, oldnvar);
            { int sy_ = intern(u); sfn[sy_] = fid; }
            if (firstfn == 0) firstfn = fid;
        }
    }
    for (;;) {
        skipeol();
        if (tk[tp] == E_EOF || perrflag == 1) break;
        if (tk[tp] != E_IDENT) {
            perr(px_cat("Syntax error near ", ts[tp]));
            break;
        }
        w = ts[tp];
        if (strcmp(w, "FUNCTION") == 0
            || strcmp(w, "PROCEDURE") == 0
            || strcmp(w, "FUNC") == 0 || strcmp(w, "PROC") == 0) {
            parsefuncdef();
            continue;
        }
        if (strcmp(w, "STATIC") == 0 || strcmp(w, "INIT") == 0
            || strcmp(w, "EXIT") == 0) {
            if (nextisword("FUNCTION") == 1
                || nextisword("PROCEDURE") == 1
                || nextisword("FUNC") == 1
                || nextisword("PROC") == 1) {
                isinit = strcmp(w, "INIT") == 0;
                isexit = strcmp(w, "EXIT") == 0;
                adv();
                junk = parsefuncdef();
                if (isinit) add_initfn(junk);
                if (isexit) add_exitfn(junk);
                continue;
            }
            if (strcmp(w, "STATIC") == 0) {
                parsestatic();
                skipline();
                continue;
            }
        }
        if (strcmp(w, "CREATE") == 0 && nextisword("CLASS") == 1) {
            adv();
            parseclass();
            continue;
        }
        if (strcmp(w, "CLASS") == 0) {
            parseclass();
            continue;
        }
        if (strcmp(w, "METHOD") == 0) {
            parsemethoddef();
            continue;
        }
        if (strcmp(w, "REQUEST") == 0 || strcmp(w, "EXTERNAL") == 0
            || strcmp(w, "ANNOUNCE") == 0
            || strcmp(w, "DYNAMIC") == 0
            || strcmp(w, "MEMVAR") == 0 || strcmp(w, "FIELD") == 0
            || strcmp(w, "DECLARE") == 0
            || strcmp(w, "NOTE") == 0) {
            skipline();
            continue;
        }
        perr(px_cat("Statement outside of a function: ", w));
    }
    return 0;
}
