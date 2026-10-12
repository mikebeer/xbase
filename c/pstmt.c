/* pstmt.c -- parser: statements (port of src/31_pstmt.crexx) */
#include "xb_parse.h"

int listcount = 0;

/* from pexpr.c (private to the parser files) */
extern int px_curcx, px_infunc, px_curfn;
extern int *px_cxnslot;
int px_declvar(int sym, int kind, int idx);
int px_newslot(void);
void px_addmst(int sym, int idx);
char *px_cat(const char *a, const char *b);
int parseprimary(void);
int isassignable(int n);

static int parseprint(void);
static int parseif(void);
static int parsewhile(void);
static int parsedo(void);
static int parsedocase(void);
static int parseswitch(void);
static int parsefor(void);
static int parsebegin(void);
static int parsetry(void);
static int parseseqbody(void);
static int parselocal(void);
int parsestatic(void);
static int parsemem(void);
static int parseparameters(void);
static int parsetext(void);
static int parseat(void);
static int parseexprstmt(void);

/* comma separated expressions; returns head, sets listcount */
int parselist(void)
{
    int head = 0, tail = 0, cnt = 0, e;

    for (;;) {
        e = parseexpr();
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
    listcount = cnt;
    return head;
}

/* adjacent tokens joined into one string constant */
int bareword(void)
{
    Sb sb;

    sb_init(&sb);
    sb_adds(&sb, tor[tp]);
    adv();
    while (tsp[tp] == 0 && tk[tp] != E_EOL && tk[tp] != E_EOF) {
        sb_adds(&sb, tor[tp]);
        adv();
    }
    return constnode(mkstr(sb_str(&sb)));
}

/* a file name: string, (expression), macro or bare word */
int filearg(void)
{
    if (tk[tp] == E_STR || isop("(") == 1 || isop("&") == 1)
        return parseexpr();
    if (tk[tp] == E_IDENT || tk[tp] == E_OP) return bareword();
    return parseexpr();
}

/* a name that may be a bare word: alias, area */
int aliasarg(void)
{
    const char *s;

    if (tk[tp] == E_IDENT) {
        s = ts[tp];
        adv();
        return constnode(mkstr(s));
    }
    return parseexpr();
}

static int inlist(const char *w, const char *list)
{
    /* list: words separated by blanks, with a blank at both ends */
    char *key = xfmt(" %s ", w);

    if (strstr(list, key) != NULL) return 1;
    return 0;
}

int iscloser(void)
{
    const char *w;

    if (tk[tp] != E_IDENT) return 0;
    w = ts[tp];
    if (inlist(w, " ELSE ELSEIF ENDIF ENDDO NEXT ENDCASE CASE"
               " OTHERWISE END ENDSWITCH ENDWITH RECOVER ALWAYS"
               " CATCH FINALLY ENDTRY ENDCLASS FUNCTION PROCEDURE"
               " METHOD ENDTEXT "))
        return 1;
    if (strcmp(w, "STATIC") == 0 || strcmp(w, "INIT") == 0
        || strcmp(w, "EXIT") == 0) {
        if (nextisword("FUNCTION") == 1
            || nextisword("PROCEDURE") == 1) return 1;
    }
    if (strcmp(w, "CREATE") == 0) {
        if (nextisword("CLASS") == 1) return 1;
    }
    if (strcmp(w, "FUNC") == 0 || strcmp(w, "PROC") == 0) return 1;
    return 0;
}

/* statements until a closer keyword or the end of input */
int parsestmts(void)
{
    int head = 0, tail = 0, st;

    for (;;) {
        skipeol();
        if (tk[tp] == E_EOF) break;
        if (iscloser() == 1) break;
        if (perrflag == 1) break;
        st = parsestmt();
        if (st == 0) continue;
        if (head == 0) head = st;
        else nx[tail] = st;
        tail = st;
        while (nx[tail] > 0) tail = nx[tail];
    }
    return mknode(K_STMTS, head, 0, 0);
}

/* consume the rest of the line */
void skipline(void)
{
    while (tk[tp] != E_EOL && tk[tp] != E_EOF) adv();
}

/* END / ENDIF etc.: consume, optionally followed by a word */
void closewith(const char *specific)
{
    const char *w;

    if (isword(specific) == 1) {
        adv();
        return;
    }
    if (isword("END") == 1) {
        adv();
        if (tk[tp] == E_IDENT) {
            w = ts[tp];
            if (inlist(w, " IF DO CASE SEQUENCE SWITCH WITH TRY"
                       " CLASS TEXT "))
                adv();
        }
        return;
    }
    perr(xfmt("Syntax error: %s expected near line %d", specific,
              tl[tp]));
}

/* is the next token an assignment-like operator? */
static int isassignop(void)
{
    const char *o;

    if (tp >= ntok) return 0;
    if (tk[tp + 1] != E_OP) return 0;
    o = ts[tp + 1];
    if (inlist(o, " := += -= *= /= %= ^= **= = ++ -- -> : [ . "))
        return 1;
    return 0;
}

int parsestmt(void)
{
    int k, e, c;
    const char *o, *w;

    curline = tl[tp];
    k = tk[tp];
    if (k == E_OP) {
        o = ts[tp];
        if (strcmp(o, "?") == 0 || strcmp(o, "??") == 0)
            return parseprint();
        if (strcmp(o, "@") == 0) {
            if (tp < ntok) {
                if (tk[tp + 1] == E_IDENT) return parseat();
                if (tk[tp + 1] == E_OP) return parseexprstmt();
            }
            return parseat();
        }
        if (strcmp(o, "!") == 0) {
            skipline();
            return 0;
        }
        return parseexprstmt();
    }
    if (k != E_IDENT) {
        perr(px_cat("Syntax error near ", ts[tp]));
        return 0;
    }
    w = ts[tp];
    /* names followed by an operator are expressions, not commands */
    if (isassignop() == 1) return parseexprstmt();
    if (tp < ntok) {
        if (tk[tp + 1] == E_OP) {
            if (strcmp(ts[tp + 1], "(") == 0) {
                if (!inlist(w, " USE GO GOTO SKIP SELECT SEEK IF"
                            " WHILE RETURN DO FOR CASE ELSEIF"
                            " DELETE RECALL SWITCH ACCEPT INPUT"
                            " WAIT STORE ERASE RENAME REPLACE"
                            " LOCATE COUNT SUM LIST DISPLAY"
                            " PRIVATE PUBLIC LOCAL STATIC BREAK"
                            " RELEASE KEYBOARD "))
                    return parseexprstmt();
            }
        }
    }
    if (strcmp(w, "IF") == 0) return parseif();
    if (strcmp(w, "DO") == 0) return parsedo();
    if (strcmp(w, "WHILE") == 0) return parsewhile();
    if (strcmp(w, "FOR") == 0) return parsefor();
    if (strcmp(w, "LOCAL") == 0) return parselocal();
    if (strcmp(w, "RETURN") == 0) {
        adv();
        e = 0;
        if (atend() == 0) e = parseexpr();
        return mknode(K_RETURN, e, 0, 0);
    }
    if (strcmp(w, "EXIT") == 0) {
        adv();
        return mknode(K_EXIT, 0, 0, 0);
    }
    if (strcmp(w, "LOOP") == 0) {
        adv();
        return mknode(K_LOOP, 0, 0, 0);
    }
    if (strcmp(w, "BREAK") == 0) {
        adv();
        e = 0;
        if (atend() == 0) e = parseexpr();
        return mknode(K_BREAK, e, 0, 0);
    }
    if (strcmp(w, "BEGIN") == 0) return parsebegin();
    if (strcmp(w, "TRY") == 0) return parsetry();
    if (strcmp(w, "SWITCH") == 0) return parseswitch();
    if (strcmp(w, "STATIC") == 0) return parsestatic();
    if (strcmp(w, "PRIVATE") == 0 || strcmp(w, "PUBLIC") == 0)
        return parsemem();
    if (strcmp(w, "PARAMETERS") == 0
        || strcmp(w, "LPARAMETERS") == 0)
        return parseparameters();
    if (strcmp(w, "WITH") == 0) {
        perr("WITH OBJECT is not supported");
        return 0;
    }
    if (strcmp(w, "TEXT") == 0) return parsetext();
    if (inlist(w, " MEMVAR FIELD DECLARE EXTERNAL REQUEST DYNAMIC"
               " ANNOUNCE NOTE ")) {
        skipline();
        return 0;
    }
    if (strcmp(w, "ENDIF") == 0 || strcmp(w, "ENDDO") == 0
        || strcmp(w, "NEXT") == 0) {
        perr(px_cat("Syntax error: unexpected ", w));
        return 0;
    }
    c = parsecmd();
    if (c != 0) return c;
    if (perrflag == 1) return 0;
    return parseexprstmt();
}

/* expression statement: a call, an assignment, x++ ... */
static int parseexprstmt(void)
{
    int e, l, r;

    e = parseexpr();
    if (nt[e] == K_EQ) {
        l = na[e];
        if (isassignable(l) == 1) {
            r = nb[e];
            e = mknode(K_ASSIGN, l, r, 0);
        }
    }
    if (atend() == 0 && perrflag == 0)
        perr(px_cat("Syntax error near ", ts[tp]));
    return mknode(K_EXPRSTMT, e, 0, 0);
}

static int parseprint(void)
{
    int kind = 1, head = 0, n;

    if (strcmp(ts[tp], "??") == 0) kind = 2;
    adv();
    if (atend() == 0) head = parselist();
    n = mknode(K_PRINT, head, kind, 0);
    /* ignore trailing PICTURE etc. */
    skipline();
    return n;
}

static int parseif(void)
{
    int cond, thenb, elseb;

    adv();
    cond = parseexpr();
    skipline();
    thenb = parsestmts();
    elseb = 0;
    if (isword("ELSEIF") == 1) {
        elseb = parseif();
        return mknode(K_IF, cond, thenb, elseb);
    }
    if (isword("ELSE") == 1) {
        adv();
        skipline();
        elseb = parsestmts();
    }
    closewith("ENDIF");
    return mknode(K_IF, cond, thenb, elseb);
}

static int parsewhile(void)
{
    int cond, body;

    adv();
    cond = parseexpr();
    skipline();
    body = parsestmts();
    closewith("ENDDO");
    return mknode(K_WHILE, cond, body, 0);
}

static int parsedo(void)
{
    int sym, args, cnt, e;

    adv();
    if (isword("WHILE") == 1) return parsewhile();
    if (isword("CASE") == 1) {
        adv();
        skipline();
        return parsedocase();
    }
    /* DO proc [WITH args] */
    if (tk[tp] != E_IDENT) {
        perr("Syntax error after DO");
        return 0;
    }
    sym = tv[tp];
    adv();
    args = 0;
    cnt = 0;
    if (isword("WITH") == 1) {
        adv();
        args = parselist();
        cnt = listcount;
    }
    e = mknode(K_CALL, sym, args, cnt);
    return mknode(K_EXPRSTMT, e, 0, 0);
}

static int parsedocase(void)
{
    int head = 0, tail = 0, cond, body, cn;

    for (;;) {
        skipeol();
        if (isword("CASE") == 1) {
            adv();
            cond = parseexpr();
            skipline();
            body = parsestmts();
            cn = mknode(K_NOP, cond, body, 0);
            if (head == 0) head = cn;
            else nx[tail] = cn;
            tail = cn;
            continue;
        }
        if (isword("OTHERWISE") == 1 || isword("ELSE") == 1) {
            adv();
            skipline();
            body = parsestmts();
            cn = mknode(K_NOP, 0, body, 0);
            if (head == 0) head = cn;
            else nx[tail] = cn;
            tail = cn;
            continue;
        }
        break;
    }
    closewith("ENDCASE");
    return mknode(K_DOCASE, head, 0, 0);
}

static int parseswitch(void)
{
    int e, head = 0, tail = 0, other = 0, vals, body, cn;

    adv();
    e = parseexpr();
    skipline();
    for (;;) {
        skipeol();
        if (isword("CASE") == 1) {
            adv();
            vals = parselist();
            skipline();
            body = parsestmts();
            cn = mknode(K_NOP, vals, body, 0);
            if (head == 0) head = cn;
            else nx[tail] = cn;
            tail = cn;
            continue;
        }
        if (isword("OTHERWISE") == 1 || isword("DEFAULT") == 1) {
            adv();
            skipline();
            other = parsestmts();
            continue;
        }
        break;
    }
    closewith("ENDSWITCH");
    return mknode(K_SWITCH, e, head, other);
}

static int parsefor(void)
{
    int v, coll, desc, body, n, start, stop, step;

    adv();
    if (isword("EACH") == 1) {
        adv();
        v = parsepostfix();
        if (isword("IN") == 1) adv();
        else perr("Syntax error: IN expected");
        coll = parseexpr();
        desc = 0;
        if (isword("DESCEND") == 1) {
            adv();
            desc = 1;
        }
        skipline();
        body = parsestmts();
        closewith("NEXT");
        n = mknode(K_FOREACH, v, coll, body);
        nd[n] = desc;
        return n;
    }
    v = parsepostfix();
    if (isop(":=") == 1 || isop("=") == 1) adv();
    else perr("Syntax error: := expected in FOR");
    start = parseexpr();
    if (isword("TO") == 1) adv();
    else perr("Syntax error: TO expected in FOR");
    stop = parseexpr();
    step = 0;
    if (isword("STEP") == 1) {
        adv();
        step = parseexpr();
    }
    skipline();
    body = parsestmts();
    closewith("NEXT");
    n = mknode(K_FOR, v, start, stop);
    nd[n] = step;
    ne[n] = body;
    return n;
}

static int parsebegin(void)
{
    adv();
    if (isword("SEQUENCE") == 0) {
        perr("Syntax error: SEQUENCE expected after BEGIN");
        return 0;
    }
    adv();
    if (isword("WITH") == 1) {
        adv();
        parseexpr();
    }
    skipline();
    return parseseqbody();
}

static int parsetry(void)
{
    adv();
    skipline();
    return parseseqbody();
}

static int parseseqbody(void)
{
    int body, rec = 0, var = 0, has = 0, always = 0, n;

    body = parsestmts();
    if (isword("RECOVER") == 1 || isword("CATCH") == 1) {
        adv();
        has = 1;
        if (isword("USING") == 1) adv();
        if (tk[tp] == E_IDENT) var = parsepostfix();
        skipline();
        rec = parsestmts();
    }
    if (isword("ALWAYS") == 1 || isword("FINALLY") == 1) {
        adv();
        skipline();
        always = parsestmts();
    }
    closewith("ENDTRY");
    n = mknode(K_SEQUENCE, body, rec, var);
    nd[n] = always;
    ne[n] = has;
    return n;
}

/* local declarations */
static int parselocal(void)
{
    int head = 0, tail = 0, sym, sl, init, args, cnt, st, lv, asg;

    adv();
    if (px_infunc == 0 || px_curcx == 0) {
        perr("LOCAL outside of a function");
        return 0;
    }
    for (;;) {
        if (tk[tp] != E_IDENT) {
            perr("Syntax error in LOCAL");
            break;
        }
        sym = tv[tp];
        adv();
        sl = px_newslot();
        px_declvar(sym, 1, sl);
        init = 0;
        if (isop(":=") == 1 || isop("=") == 1) {
            adv();
            init = parseexpr();
        } else if (isop("[") == 1) {
            adv();
            args = parselist();
            cnt = listcount;
            expectop("]");
            init = mknode(K_CALL, intern("ARRAY"), args, cnt);
        }
        if (init > 0) {
            lv = mknode(K_LOCAL, sl, 0, 0);
            asg = mknode(K_ASSIGN, lv, init, 0);
            st = mknode(K_EXPRSTMT, asg, 0, 0);
            if (head == 0) head = st;
            else nx[tail] = st;
            tail = st;
        }
        if (isop(",") == 1) {
            adv();
            continue;
        }
        break;
    }
    if (head == 0) return 0;
    return head;
}

int parsestatic(void)
{
    int sym, idx, init, args, cnt;

    adv();
    if (isword("FUNCTION") == 1 || isword("PROCEDURE") == 1) {
        perr("Syntax error: nested function");
        return 0;
    }
    for (;;) {
        if (tk[tp] != E_IDENT) {
            perr("Syntax error in STATIC");
            break;
        }
        sym = tv[tp];
        adv();
        idx = newstatic();
        if (px_infunc == 1 && px_curcx > 0)
            px_declvar(sym, 2, idx);
        else px_addmst(sym, idx);
        init = 0;
        if (isop(":=") == 1 || isop("=") == 1) {
            adv();
            init = parseexpr();
        } else if (isop("[") == 1) {
            adv();
            args = parselist();
            cnt = listcount;
            expectop("]");
            init = mknode(K_CALL, intern("ARRAY"), args, cnt);
        }
        if (init > 0) add_svinit(init, idx);
        if (isop(",") == 1) {
            adv();
            continue;
        }
        break;
    }
    return 0;
}

static int parsemem(void)
{
    int kind = K_PRIVATE, head = 0, tail = 0, st = 0, mcnt = 0;
    int e, sym, init, args, cnt;

    if (strcmp(ts[tp], "PUBLIC") == 0) kind = K_PUBLIC;
    adv();
    for (;;) {
        if (isop("&") == 1) {
            adv();
            e = parseprimary();
            st = mknode(kind, 0, 0, 0);
            nb[st] = e;
            nf[st] = 1;
        } else {
            if (tk[tp] != E_IDENT) {
                perr("Syntax error in PRIVATE/PUBLIC");
                break;
            }
            sym = tv[tp];
            adv();
            init = 0;
            if (isop(":=") == 1 || isop("=") == 1) {
                adv();
                init = parseexpr();
            } else if (isop("[") == 1) {
                adv();
                args = parselist();
                cnt = listcount;
                expectop("]");
                init = mknode(K_CALL, intern("ARRAY"), args, cnt);
            }
            st = mknode(kind, sym, init, 0);
        }
        if (head == 0) head = st;
        else nx[tail] = st;
        tail = st;
        mcnt++;
        if (isop(",") == 1) {
            adv();
            continue;
        }
        break;
    }
    if (head > 0) ne[head] = mcnt;
    return head;
}

static int parseparameters(void)
{
    int head = 0, tail = 0, cnt = 0, cn;

    adv();
    for (;;) {
        if (tk[tp] != E_IDENT) break;
        cn = mknode(K_NOP, tv[tp], 0, 0);
        adv();
        cnt++;
        if (head == 0) head = cn;
        else nx[tail] = cn;
        tail = cn;
        if (isop(",") == 1) {
            adv();
            continue;
        }
        break;
    }
    if (px_curfn > 0) {
        if (fnpar[px_curfn] < cnt) fnpar[px_curfn] = cnt;
        if (px_cxnslot[px_curcx] < cnt) px_cxnslot[px_curcx] = cnt;
    }
    return mknode(K_PARAMETERS, head, cnt, 0);
}

static int parsetext(void)
{
    int var = 0, c;
    const char *s = "";

    adv();
    if (isword("TO") == 1) {
        adv();
        if (isword("FILE") == 1 || isword("PRINTER") == 1)
            skipline();
        else var = parsepostfix();
    }
    if (tk[tp] == E_STR) {
        s = ts[tp];
        adv();
    }
    c = constnode(mkstr(s));
    return mknode(K_TEXT, c, var, 0);
}

/* ------------------------------------------------------ @ SAY GET */
static int parseat(void)
{
    int r, c, n, hold, t;

    adv();
    r = parseexpr();
    expectop(",");
    c = parseexpr();
    n = mknode(K_SAYGET, r, c, 0);
    hold = mknode(K_NOP, 0, 0, 0);
    nf[n] = hold;
    for (;;) {
        if (atend() == 1) break;
        if (perrflag == 1) break;
        if (isword("SAY") == 1) {
            adv();
            t = parseexpr();
            nc[n] = t;
            continue;
        }
        if (isword("GET") == 1) {
            adv();
            t = parsepostfix();
            nd[n] = t;
            continue;
        }
        if (isword("PICTURE") == 1) {
            adv();
            t = parseexpr();
            if (nd[n] > 0) nb[hold] = t;
            else na[hold] = t;
            continue;
        }
        if (isword("VALID") == 1) {
            adv();
            t = parseexpr();
            nc[hold] = t;
            continue;
        }
        if (isword("WHEN") == 1) {
            adv();
            t = parseexpr();
            nd[hold] = t;
            continue;
        }
        if (isword("COLOR") == 1 || isword("MESSAGE") == 1
            || isword("RANGE") == 1 || isword("FONT") == 1
            || isword("SEND") == 1) {
            adv();
            parseexpr();
            if (isop(",") == 1) {
                adv();
                parseexpr();
            }
            continue;
        }
        if (isword("CLEAR") == 1) {
            adv();
            ne[n] = 1;
            if (isword("TO") == 1) {
                adv();
                parseexpr();
                expectop(",");
                parseexpr();
            }
            continue;
        }
        if (isword("TO") == 1) {
            adv();
            parseexpr();
            expectop(",");
            parseexpr();
            ne[n] = 2;
            continue;
        }
        if (isword("DOUBLE") == 1 || isword("BOX") == 1) {
            adv();
            if (atend() == 0) parseexpr();
            continue;
        }
        if (isop(",") == 1) {
            /* @ r1,c1,r2,c2 BOX ... */
            adv();
            parseexpr();
            continue;
        }
        perr(px_cat("Syntax error in @ command near ", ts[tp]));
        break;
    }
    return n;
}
