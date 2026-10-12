/* cmd.c -- execution of the database and console commands (port of
   src/72_cmd.crexx) */
#include "xb.h"

/* defined by core (privates) and by io (KEYBOARD buffer) */
extern void pushprivate(int sym);
void keyboard_push(const char *t);

/* state of the scoped commands */
static int sc_a = 0;
static long sc_lim = 0;
static int sc_single = 0;
static int sc_done = 0;
static int sc_for = 0;
static int sc_while = 0;
static int atrow = -1;

/* the GET table (1-based) */
static int *gt_node = NULL;
static int *gt_pic = NULL;
static int *gt_valid = NULL;
static int *gt_when = NULL;
static int *gt_row = NULL;
static int *gt_col = NULL;
static long gtcap = 0;
static long ngets = 0;

void assignnew(int n, V val);
int areaarg(V v);

static void gt_grow(long need)
{
    long nc2;

    if (need < gtcap) return;
    nc2 = need * 2 + 16;
    gt_node = (int *)xrealloc(gt_node, nc2 * (long)sizeof(int));
    gt_pic = (int *)xrealloc(gt_pic, nc2 * (long)sizeof(int));
    gt_valid = (int *)xrealloc(gt_valid, nc2 * (long)sizeof(int));
    gt_when = (int *)xrealloc(gt_when, nc2 * (long)sizeof(int));
    gt_row = (int *)xrealloc(gt_row, nc2 * (long)sizeof(int));
    gt_col = (int *)xrealloc(gt_col, nc2 * (long)sizeof(int));
    gtcap = nc2;
}

static char *itos(long n)
{
    return xfmt("%ld", n);
}

/* s padded or cut to exactly n characters (REXX left()) */
static char *leftn(const char *s, int n)
{
    char *r;
    int l;

    r = (char *)xmalloc(n + 1);
    l = (int)strlen(s);
    if (l > n) l = n;
    memcpy(r, s, l);
    memset(r + l, ' ', n - l);
    r[n] = 0;
    return r;
}

/* REXX: a string of blanks equals the empty string */
static int blankstr(const char *s)
{
    while (*s) {
        if (*s != ' ') return 0;
        s++;
    }
    return 1;
}

/* a number held in an integer cell of the cREXX version? */
static int isheap(V v)
{
    double x;

    x = v.u.n;
    if (v.d == 0 && x < 9.0e15 && x > -9.0e15
        && x == (double)toint(x)) return 0;
    return 1;
}

static char *evstr(int n)
{
    V v;

    if (n == 0) return "";
    v = evalnode(n);
    if (ctl != 0) return "";
    if (!isstr(v)) return xstrdup(valstr(v));
    return SS(v);
}

static int evint(int n, int dflt)
{
    V v;

    if (n == 0) return dflt;
    v = evalnode(n);
    if (ctl != 0) return dflt;
    if (!isnum(v)) return dflt;
    return (int)toint(numval(v));
}

static int needarea(void)
{
    if (curarea < 1) {
        (void)rterr("DBFNTX", 2001, "Workarea not in use", "");
        return 0;
    }
    if (wa_used[curarea] == 0) {
        (void)rterr("DBFNTX", 2001, "Workarea not in use", "");
        return 0;
    }
    return curarea;
}

static int areaof(int an)
{
    int a;

    if (an == 0) return needarea();
    a = resolvealias(an);
    if (ctl != 0) return 0;
    if (a < 1 || a > 40) {
        (void)rterr("BASE", 1002, "Alias does not exist", "");
        return 0;
    }
    return a;
}

/* USE ... : open a table in a work area */
static void douse(int s)
{
    int fl, a, ro, x;
    char *f, *al;

    fl = nf[s];
    if (nb[s] == 0) {
        if (curarea > 0) closearea(curarea);
        return;
    }
    f = evstr(nb[s]);
    if (ctl != 0) return;
    if (fl % 2 == 1 || curarea < 1) a = findfreearea();
    else a = curarea;
    if (a == 0) {
        (void)rterr("DBFNTX", 1001, "No free work area", f);
        return;
    }
    if (wa_used[a] == 1) closearea(a);
    al = "";
    if (nc[s] > 0) al = xupper(xstrip(evstr(nc[s])));
    if (strcmp(al, "") == 0) al = dbfstem(f);
    if (aliasarea(al) > 0) {
        (void)rterr("DBFNTX", 1001, "Alias already in use", al);
        return;
    }
    ro = 0;
    if ((fl / 8) % 2 == 1) ro = 1;
    if (openarea(a, f, al, ro) == 0) return;
    curarea = a;
    x = nd[s];
    while (x > 0) {
        (void)openindex(a, evstr(x));
        if (ctl != 0) return;
        x = nx[x];
    }
}

static void doclose(int s)
{
    int a;

    if (nf[s] == 1) {
        closeallareas();
        curarea = 0;
        return;
    }
    if (nf[s] == 2) {
        if (curarea > 0) {
            wa_nix[curarea] = 0;
            wa_order[curarea] = 0;
        }
        return;
    }
    if (nb[s] > 0) {
        a = areaof(nb[s]);
        if (ctl == 0) closearea(a);
        return;
    }
    if (curarea > 0) closearea(curarea);
}

static void doselect(int s)
{
    int an, a, k;
    V v;

    an = nb[s];
    if (nt[an] == K_CONST) {
        v = nk[an];
        if (isnum(v)) {
            k = (int)toint(numval(v));
            if (k == 0) k = findfreearea();
            curarea = k;
            return;
        }
        if (isstr(v)) {
            a = aliasarea(xupper(xstrip(SS(v))));
            if (a == 0) {
                if (strcmp(xupper(xstrip(SS(v))), "NEW") == 0)
                    curarea = findfreearea();
                else
                    (void)rterr("BASE", 1002,
                                "Alias does not exist",
                                xupper(SS(v)));
                return;
            }
            curarea = a;
            return;
        }
    }
    v = evalnode(an);
    if (ctl != 0) return;
    if (isnum(v)) {
        k = (int)toint(numval(v));
        if (k == 0) k = findfreearea();
        curarea = k;
        return;
    }
    if (isstr(v)) {
        a = aliasarea(xupper(xstrip(SS(v))));
        if (a == 0)
            (void)rterr("BASE", 1002, "Alias does not exist",
                        xupper(SS(v)));
        else curarea = a;
    }
}

static void doindex(int s)
{
    int a, fl;
    char *ktxt, *ftxt, *fname, *p;

    a = needarea();
    if (ctl != 0) return;
    ktxt = xstrdup(ns[s]);
    ftxt = "";
    p = strchr(ktxt, 1);
    if (p != NULL) {
        ftxt = p + 1;
        *p = 0;
    }
    fname = "";
    if (nc[s] > 0) fname = evstr(nc[s]);
    if (ctl != 0) return;
    fl = nf[s];
    if (setunique == 1) fl = fl % 2 + (fl / 2) * 2 + (1 - fl % 2);
    (void)createindex(a, nb[s], fname, nd[s], fl, ktxt, ftxt);
}

static void doset(int s)
{
    const char *nm;
    int onoff, a, n, x, k, al, ch;

    nm = ns[s];
    onoff = nc[s];
    if (strcmp(nm, "DELETED") == 0) {
        setdeleted = (onoff == 1);
        return;
    }
    if (strcmp(nm, "SOFTSEEK") == 0) {
        setsoft = (onoff == 1);
        return;
    }
    if (strcmp(nm, "EXACT") == 0) {
        setexact = (onoff == 1);
        return;
    }
    if (strcmp(nm, "FIXED") == 0) {
        setfixed = (onoff == 1);
        return;
    }
    if (strcmp(nm, "CENTURY") == 0) {
        setcentury = (onoff == 1);
        return;
    }
    if (strcmp(nm, "CONSOLE") == 0) {
        if (onoff == 1) setconsole = 1;
        else if (onoff == 2) setconsole = 0;
        return;
    }
    if (strcmp(nm, "UNIQUE") == 0) {
        setunique = (onoff == 1);
        return;
    }
    if (strcmp(nm, "DECIMALS") == 0) {
        setdec = evint(nb[s], 2);
        return;
    }
    if (strcmp(nm, "EPOCH") == 0) {
        setepoch = evint(nb[s], 1900);
        return;
    }
    if (strcmp(nm, "DATE") == 0) {
        if (nb[s] > 0) setdate(evstr(nb[s]));
        return;
    }
    if (strcmp(nm, "DEFAULT") == 0 || strcmp(nm, "PATH") == 0) {
        if (nb[s] > 0) setdefault = xstrdup(evstr(nb[s]));
        return;
    }
    if (strcmp(nm, "FILTER") == 0) {
        a = needarea();
        if (ctl != 0) return;
        wa_filter[a] = nb[s];
        dbgotop(a);
        return;
    }
    if (strcmp(nm, "ORDER") == 0) {
        a = needarea();
        if (ctl != 0) return;
        n = evint(nb[s], 0);
        if (n < 0 || n > wa_nix[a]) n = 0;
        if (n == 0) wa_order[a] = 0;
        else wa_order[a] = waix[(a - 1) * 8 + n];
        dbgotop(a);
        return;
    }
    if (strcmp(nm, "INDEX") == 0) {
        a = needarea();
        if (ctl != 0) return;
        wa_nix[a] = 0;
        wa_order[a] = 0;
        x = nd[s];
        while (x > 0) {
            (void)openindex(a, evstr(x));
            if (ctl != 0) return;
            x = nx[x];
        }
        if (wa_nix[a] > 0) dbgotop(a);
        return;
    }
    if (strcmp(nm, "RELATION") == 0) {
        a = needarea();
        if (ctl != 0) return;
        wa_nrel[a] = 0;
        k = nd[s];
        while (k > 0) {
            al = nx[k];
            ch = resolvealias(al);
            if (ctl != 0) return;
            wa_nrel[a] = wa_nrel[a] + 1;
            wa_relchild[(a - 1) * 8 + wa_nrel[a]] = ch;
            wa_relkey[(a - 1) * 8 + wa_nrel[a]] = k;
            k = nx[al];
        }
        relsync(a);
        return;
    }
}

static void clearmem(void)
{
    long i;

    for (i = 1; i <= nsym; i++) {
        smvd[i] = 0;
        smv[i] = NILV;
    }
}

static void doclear(int s)
{
    if (nf[s] == 2 || nf[s] == 3) clearmem();
    if (nf[s] == 3) closeallareas();
}

static void dorelease(int s)
{
    int x, sym;

    if (nf[s] == 1) {
        clearmem();
        return;
    }
    x = nb[s];
    while (x > 0) {
        if (nt[x] == K_MEMVAR || nt[x] == K_IDENT) {
            sym = na[x];
            smvd[sym] = 0;
            smv[sym] = NILV;
        }
        x = nx[x];
    }
}

static void dostore(int s)
{
    V v;
    int x;

    v = evalnode(nb[s]);
    if (ctl != 0) return;
    x = nc[s];
    while (x > 0) {
        assignnew(x, cloneval(v));
        if (ctl != 0) return;
        x = nx[x];
    }
}

static void doprompt(int s)
{
    char *txt;

    if (nb[s] > 0) {
        txt = evstr(nb[s]);
        if (ctl != 0) return;
        emit(txt);
    }
}

static void doaccept(int s)
{
    char *line;

    doprompt(s);
    line = xstrdup(readcon());
    emitnl();
    if (nc[s] > 0) assignnew(nc[s], mkstr(line));
}

static void doinput(int s)
{
    char *line;
    int nd2;
    V v;

    doprompt(s);
    for (;;) {
        line = xstrip(readcon());
        emitnl();
        if (ineof == 1) return;
        if (line[0] == 0) return;
        nd2 = compileexpr(line);
        if (nd2 == 0) {
            emit("Invalid expression, re-enter: ");
            continue;
        }
        v = evalnode(nd2);
        if (ctl != 0) return;
        if (nc[s] > 0) assignnew(nc[s], v);
        return;
    }
}

static void dowait(int s)
{
    int c;
    char buf[2];

    if (nb[s] > 0) doprompt(s);
    else emit("Press any key to continue...");
    c = readkey();
    buf[0] = 0;
    buf[1] = 0;
    if (c > 0 && c != 13) buf[0] = (char)c;
    if (nc[s] > 0) assignnew(nc[s], mkstr(buf));
    emitnl();
}

static void doappendfrom(int s);
static void docontinue(void);
static void doread(void);

void execcmd(int s)
{
    int cmd, a;
    V v;
    char *f, *g, *t, *data;
    long len;

    cmd = na[s];
    if (cmd == C_USE) douse(s);
    else if (cmd == C_SELECT) doselect(s);
    else if (cmd == C_GO) {
        a = areaof(nc[s]);
        if (ctl != 0) return;
        if (nf[s] == 1) dbgotop(a);
        else if (nf[s] == 2) dbgobottom(a);
        else dbgoto(a, evint(nb[s], 0));
    } else if (cmd == C_SKIP) {
        a = areaof(nc[s]);
        if (ctl != 0) return;
        dbskip(a, evint(nb[s], 1));
    } else if (cmd == C_SEEK) {
        a = needarea();
        if (ctl != 0) return;
        v = evalnode(nb[s]);
        if (ctl != 0) return;
        (void)dbseek(a, v, nf[s]);
    } else if (cmd == C_APPEND) {
        a = needarea();
        if (ctl != 0) return;
        if (nf[s] == 2) doappendfrom(s);
        else dbappend(a);
    } else if (cmd == C_CONTINUE) docontinue();
    else if (cmd == C_PACK) {
        a = needarea();
        if (ctl == 0) dbpack(a);
    } else if (cmd == C_ZAP) {
        a = needarea();
        if (ctl == 0) dbzap(a);
    } else if (cmd == C_COMMIT) {
        for (a = 1; a <= 40; a++) {
            if (wa_used[a] == 1) {
                flushdbf(a);
                flushindexes(a);
            }
        }
    } else if (cmd == C_REINDEX) {
        a = needarea();
        if (ctl == 0) reindexall(a);
    } else if (cmd == C_INDEX) doindex(s);
    else if (cmd == C_CLOSE) doclose(s);
    else if (cmd == C_SET) doset(s);
    else if (cmd == C_CLEAR) doclear(s);
    else if (cmd == C_ERASE) {
        f = evstr(nb[s]);
        if (ctl == 0 && fileexists(f) == 1) (void)remove(f);
    } else if (cmd == C_RENAME) {
        f = evstr(nb[s]);
        g = evstr(nc[s]);
        if (ctl == 0 && fileexists(f) == 1) {
            len = 0;
            data = bytesof(f, &len);
            if (data != NULL) (void)putbytes(g, data, len);
            (void)remove(f);
        }
    } else if (cmd == C_ACCEPT) doaccept(s);
    else if (cmd == C_INPUT) doinput(s);
    else if (cmd == C_WAIT) dowait(s);
    else if (cmd == C_READ) doread();
    else if (cmd == C_QUIT) doquit(0);
    else if (cmd == C_RELEASE) dorelease(s);
    else if (cmd == C_STORE) dostore(s);
    else if (cmd == C_KEYBOARD) {
        t = evstr(nb[s]);
        keyboard_push(t);
    }
}

/* ------------------------------------------- scoped commands */
static void scbegin(int s, int defall)
{
    int a, k;

    a = needarea();
    sc_a = a;
    sc_lim = -1;
    sc_single = 0;
    sc_done = 0;
    sc_for = nb[s];
    sc_while = nc[s];
    if (ctl != 0) {
        sc_done = 1;
        return;
    }
    k = nf[s];
    if (k == 0 && sc_for > 0) k = 1;
    if (k == 0 && defall == 1) k = 1;
    if (k == 0) sc_single = 1;
    else if (k == 1) dbgotop(a);
    else if (k == 2) sc_lim = evint(nd[s], 0);
    else if (k == 3) {
        dbgoto(a, evint(nd[s], 0));
        sc_single = 1;
    }
}

static void scskip(void)
{
    if (sc_single == 1) {
        sc_done = 1;
        return;
    }
    if (sc_lim > 0) sc_lim = sc_lim - 1;
    dbskip(sc_a, 1);
}

static int scnext(void)
{
    for (;;) {
        if (sc_done == 1) return 0;
        if (wa_eof[sc_a] == 1) return 0;
        if (sc_lim == 0) return 0;
        if (sc_while > 0) {
            if (condval(sc_while) == 0) return 0;
            if (ctl != 0) return 0;
        }
        if (sc_for > 0) {
            if (condval(sc_for) == 0) {
                if (ctl != 0) return 0;
                scskip();
                continue;
            }
        }
        return 1;
    }
}

static void dolist(int s, int hold)
{
    int a, off, e, f, first;
    Sb line;
    V v;

    scbegin(s, 1);
    if (ctl != 0) return;
    if (nf[s] == 0 && nb[s] == 0 && na[s] == C_DISPLAY)
        sc_single = 1;
    a = sc_a;
    if (na[s] == C_DISPLAY && nf[s] == 0 && nb[s] == 0)
        sc_single = 1;
    off = nd[hold];
    while (scnext() == 1) {
        sb_init(&line);
        if (off == 0) {
            sb_adds(&line, padleft(itos(wa_recno[a]), 7));
            sb_addc(&line, ' ');
        }
        if (isdeleted(a) == 1) sb_addc(&line, '*');
        e = na[hold];
        if (e == 0) {
            for (f = 1; f <= wa_nfld[a]; f++) {
                if (f > 1) sb_addc(&line, ' ');
                sb_adds(&line, valstr(fieldval(a, f)));
            }
        } else {
            first = 1;
            while (e > 0) {
                v = evalnode(e);
                if (ctl != 0) return;
                if (first == 0) sb_addc(&line, ' ');
                sb_adds(&line, valstr(v));
                first = 0;
                e = nx[e];
            }
        }
        if (outstarted == 1) emitnl();
        emit(sb_str(&line));
        scskip();
    }
}

static void dostructure(void)
{
    int a, f, total, i;
    char *tn, *t, *dd;
    Sb b;

    a = needarea();
    if (ctl != 0) return;
    if (outstarted == 1) emitnl();
    emit(xfmt("Structure for database: %s", wa_file[a]));
    emitnl();
    emit(xfmt("Number of data records: %s",
              padleft(itos(wa_nrec[a]), 7)));
    emitnl();
    emit("Field  Field Name  Type       Width    Dec");
    total = 1;
    for (f = 1; f <= wa_nfld[a]; f++) {
        i = fidx(a, f);
        tn = "Character";
        t = ftyp[i];
        if (strcmp(t, "N") == 0) tn = "Numeric";
        if (strcmp(t, "F") == 0) tn = "Float";
        if (strcmp(t, "D") == 0) tn = "Date";
        if (strcmp(t, "L") == 0) tn = "Logical";
        if (strcmp(t, "M") == 0) tn = "Memo";
        dd = "";
        if (strcmp(t, "N") == 0 || strcmp(t, "F") == 0)
            dd = padleft(itos(fdec[i]), 3);
        emitnl();
        sb_init(&b);
        sb_adds(&b, padleft(itos(f), 5));
        sb_adds(&b, "  ");
        sb_adds(&b, leftn(fnam[i], 10));
        sb_adds(&b, "  ");
        sb_adds(&b, leftn(tn, 9));
        sb_adds(&b, padleft(itos(flen[i]), 7));
        sb_adds(&b, "    ");
        sb_adds(&b, dd);
        emit(sb_str(&b));
        total = total + flen[i];
    }
    emitnl();
    emit(xfmt("** Total **%s", padleft(itos(total), 21)));
}

/* copy of record r of area a */
static char *recimage(int a, long r)
{
    char *p;

    p = recs[ridx(a, (int)r)];
    if (p == NULL) return xstrdup("");
    return xstrdup(p);
}

static void docopy(int s, int hold)
{
    int a, i, t, n;
    char *f, *al;
    V stru, e;

    a = needarea();
    if (ctl != 0) return;
    f = evstr(nb[hold]);
    if (ctl != 0) return;
    stru = mkarray(0);
    for (i = 1; i <= wa_nfld[a]; i++) {
        e = mkarray(0);
        arrpush(e, mkstr(fnam[fidx(a, i)]));
        arrpush(e, mkstr(ftyp[fidx(a, i)]));
        arrpush(e, mkint(flen[fidx(a, i)]));
        arrpush(e, mkint(fdec[fidx(a, i)]));
        arrpush(stru, e);
    }
    (void)createdbf(f, stru);
    if (nf[hold] == 1) return;
    t = findfreearea();
    if (t == 0) return;
    al = xfmt("$COPY%d", t);
    if (openarea(t, f, al, 0) == 0) return;
    scbegin(s, 1);
    while (scnext() == 1) {
        n = wa_nrec[t] + 1;
        recs[ridx(t, n)] = recimage(a, wa_recno[a]);
        wa_nrec[t] = n;
        scskip();
    }
    wa_dirty[t] = 1;
    closearea(t);
}

static void doappendfrom(int s)
{
    int a, t, r, nr, ok, old, i, j;
    char *f;

    a = needarea();
    f = evstr(nb[s]);
    if (ctl != 0) return;
    t = findfreearea();
    if (t == 0) return;
    if (openarea(t, f, xfmt("$APPEND%d", t), 1) == 0) return;
    nr = wa_nrec[t];
    for (r = 1; r <= nr; r++) {
        wa_recno[t] = r;
        wa_eof[t] = 0;
        ok = 1;
        if (nd[s] > 0) {
            old = curarea;
            curarea = t;
            ok = condval(nd[s]);
            curarea = old;
        }
        if (ok == 1) {
            dbappend(a);
            for (i = 1; i <= wa_nfld[a]; i++) {
                j = fieldfind(t, fsym[fidx(a, i)]);
                if (j > 0) {
                    if (strcmp(ftyp[fidx(t, j)],
                               ftyp[fidx(a, i)]) == 0)
                        fieldput(a, i, fieldval(t, j));
                }
            }
        }
    }
    closearea(t);
}

static void docontinue(void)
{
    int a, fnode;

    a = needarea();
    if (ctl != 0) return;
    fnode = wa_locfor[a];
    if (fnode == 0 && wa_locwhile[a] == 0) {
        (void)rterr("DBFNTX", 1004, "CONTINUE without LOCATE", "");
        return;
    }
    sc_a = a;
    sc_lim = -1;
    sc_single = 0;
    sc_done = 0;
    sc_for = fnode;
    sc_while = wa_locwhile[a];
    dbskip(a, 1);
    if (scnext() == 1) wa_found[a] = 1;
    else wa_found[a] = 0;
}

void execscoped(int s)
{
    int cmd, hold, a, defall, f, v, nxt, e, i, t, d, dd, ntgt;
    int nsum;
    long cnt;
    double *sums, x;
    int *decs;
    V val;

    cmd = na[s];
    hold = ne[s];
    if (cmd == C_LIST || cmd == C_DISPLAY) {
        if (nf[hold] == 1) {
            dostructure();
            return;
        }
        dolist(s, hold);
        return;
    }
    if (cmd == C_LOCATE) {
        scbegin(s, 1);
        a = sc_a;
        if (ctl != 0) return;
        wa_locfor[a] = nb[s];
        wa_locwhile[a] = nc[s];
        wa_found[a] = 0;
        if (scnext() == 1) {
            wa_found[a] = 1;
            return;
        }
        wa_found[a] = 0;
        return;
    }
    if (cmd == C_COPY) {
        docopy(s, hold);
        return;
    }
    if (cmd == C_TOTAL) {
        (void)rterr("BASE", 1001, "Command TOTAL is not supported",
                    "");
        return;
    }
    if (cmd == C_DELETE || cmd == C_RECALL || cmd == C_REPLACE
        || cmd == C_COUNT || cmd == C_SUM || cmd == C_AVERAGE) {
        defall = 0;
        if (cmd == C_COUNT || cmd == C_SUM || cmd == C_AVERAGE)
            defall = 1;
        scbegin(s, defall);
        if (ctl != 0) return;
        a = sc_a;
        cnt = 0;
        nsum = 0;
        ntgt = 0;
        sums = NULL;
        decs = NULL;
        if (cmd == C_SUM || cmd == C_AVERAGE) {
            nsum = nc[hold];
            for (t = nb[hold]; t > 0; t = nx[t]) ntgt++;
            if (ntgt < nsum) ntgt = nsum;
            sums = (double *)xmalloc((ntgt + 2)
                                     * (long)sizeof(double));
            decs = (int *)xmalloc((ntgt + 2)
                                  * (long)sizeof(int));
            for (i = 0; i <= ntgt + 1; i++) {
                sums[i] = 0.0;
                decs[i] = 0;
            }
        }
        while (scnext() == 1) {
            cnt = cnt + 1;
            if (cmd == C_DELETE) dbdelete(a, 1);
            else if (cmd == C_RECALL) dbdelete(a, 0);
            else if (cmd == C_REPLACE) {
                f = na[hold];
                while (f > 0) {
                    v = nx[f];
                    nxt = nx[v];
                    val = evalnode(v);
                    if (ctl != 0) return;
                    assignto(f, val);
                    if (ctl != 0) return;
                    f = nxt;
                }
            } else if (cmd == C_SUM || cmd == C_AVERAGE) {
                e = na[hold];
                i = 0;
                while (e > 0) {
                    i = i + 1;
                    val = evalnode(e);
                    if (ctl != 0) return;
                    if (isnum(val)) {
                        sums[i] = sums[i] + numval(val);
                        dd = numdec(val);
                        if (dd > decs[i]) decs[i] = dd;
                    }
                    e = nx[e];
                }
            }
            scskip();
        }
        if (cmd == C_COUNT) {
            if (na[hold] > 0) assignnew(na[hold], mkint(cnt));
        }
        if (cmd == C_SUM || cmd == C_AVERAGE) {
            t = nb[hold];
            i = 0;
            while (t > 0) {
                i = i + 1;
                x = sums[i];
                if (cmd == C_AVERAGE && cnt > 0) x = x / (double)cnt;
                d = decs[i];
                if (cmd == C_AVERAGE && d < setdec) d = setdec;
                assignnew(t, mknum(x, 15 + d, d));
                t = nx[t];
            }
        }
        return;
    }
}

/* ------------------------------------------------ @ SAY / GET */
static void atpos(int r, int c)
{
    if (outstarted == 1) {
        if (r != atrow || c < outcol) {
            if (outcol > 0) emitnl();
        }
    }
    /* the cREXX version drops the column padding; so do we */
    atrow = r;
}

void execsayget(int s)
{
    int hold, r, c;
    char *txt;
    V v;
    Sb b;

    hold = nf[s];
    r = evint(na[s], 0);
    c = evint(nb[s], 0);
    if (ctl != 0) return;
    atpos(r, c);
    if (nc[s] > 0) {
        v = evalnode(nc[s]);
        if (ctl != 0) return;
        txt = valstr(v);
        if (na[hold] > 0) txt = xtransform(v, evstr(na[hold]));
        emit(txt);
    }
    if (nd[s] > 0) {
        ngets = ngets + 1;
        gt_grow(ngets);
        gt_node[ngets] = nd[s];
        gt_pic[ngets] = nb[hold];
        gt_valid[ngets] = nc[hold];
        gt_when[ngets] = nd[hold];
        gt_row[ngets] = r;
        gt_col[ngets] = c;
        v = evalnode(nd[s]);
        if (ctl != 0) return;
        txt = valstr(v);
        if (nb[hold] > 0) txt = xtransform(v, evstr(nb[hold]));
        sb_init(&b);
        sb_addc(&b, '[');
        sb_adds(&b, txt);
        sb_addc(&b, ']');
        emit(sb_str(&b));
    }
}

/* READ: one input line per pending GET; an empty line keeps the
   value */
static void doread(void)
{
    long g, ng;
    int nd2;
    char *line, *u;
    V old, nv;
    long w, ll;
    Sb b;

    ng = ngets;
    for (g = 1; g <= ng; g++) {
        nd2 = gt_node[g];
        if (gt_when[g] > 0) {
            if (condval(gt_when[g]) == 0) continue;
        }
        old = evalnode(nd2);
        if (ctl != 0) return;
        for (;;) {
            line = xstrdup(readcon());
            if (ineof == 1) break;
            if (gt_pic[g] > 0) {
                if (strchr(evstr(gt_pic[g]), '!') != NULL)
                    line = xupper(line);
            }
            nv = old;
            if (!blankstr(line) || isstr(old)) {
                if (isstr(old)) {
                    if (line[0] == 0) nv = old;
                    else {
                        w = SL(old);
                        ll = (long)strlen(line);
                        if (ll >= w) nv = mkstrn(line, w);
                        else {
                            sb_init(&b);
                            sb_adds(&b, line);
                            while (b.n < w) sb_addc(&b, ' ');
                            nv = sb_val(&b);
                        }
                    }
                } else if (isnum(old)) nv = valnum(line);
                else if (isdate(old))
                    nv = mkdate(parsedate(xstrip(line), setdatefmt));
                else if (islog(old)) {
                    u = xupper(xstrip(line));
                    if (u[0] == 'Y' || u[0] == 'T') nv = mklog(1);
                    else nv = mklog(0);
                }
            }
            if (isnum(old) && isnum(nv) && isheap(old)) {
                /* keep the width and decimals of the original */
                nv = mknum(numval(nv), numwid(old), numdec(old));
            }
            assignto(nd2, nv);
            if (ctl != 0) return;
            if (gt_valid[g] > 0) {
                if (condval(gt_valid[g]) == 0) {
                    emitnl();
                    emit("Invalid entry, re-enter: ");
                    continue;
                }
            }
            break;
        }
        emitnl();
    }
    ngets = 0;
}

/* a work area given as number or alias value */
int areaarg(V v)
{
    if (isnum(v)) return (int)toint(numval(v));
    if (isstr(v)) return aliasarea(xupper(xstrip(SS(v))));
    return 0;
}

/* assignment target of STORE, COUNT TO, ACCEPT TO ...: a new
   variable becomes PRIVATE */
void assignnew(int n, V val)
{
    int sym, isfld;

    if (nt[n] == K_IDENT || nt[n] == K_MEMVAR) {
        sym = na[n];
        if (smvd[sym] == 0) {
            isfld = 0;
            if (curarea > 0) {
                if (fieldfind(curarea, sym) > 0) isfld = 1;
            }
            if (isfld == 0) {
                pushprivate(sym);
                smv[sym] = val;
                smvd[sym] = 1;
                return;
            }
        }
    }
    assignto(n, val);
}
