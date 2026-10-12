/* pcmd.c -- parser for the database and console commands (port of
   src/32_pcmd.crexx) */
#include "xb_parse.h"

static char *toktext(long a, long b);
static int parseset(void);
static int fieldref(void);
static int parsescopedw(const char *w);

static int mkcmd(int cmd)
{
    return mknode(K_CMD, cmd, 0, 0);
}

static int mkscoped(int cmd)
{
    return mknode(K_SCOPED, cmd, 0, 0);
}

/* scope, FOR and WHILE clauses; fills nf (kind) nd (n) nb (for)
   nc (while) */
static void parsescope(int n)
{
    for (;;) {
        if (isword("ALL")) {
            adv();
            nf[n] = 1;
            continue;
        }
        if (isword("NEXT")) {
            adv();
            nf[n] = 2;
            { int t_ = parseexpr(); nd[n] = t_; }
            continue;
        }
        if (isword("RECORD")) {
            adv();
            nf[n] = 3;
            { int t_ = parseexpr(); nd[n] = t_; }
            continue;
        }
        if (isword("REST")) {
            adv();
            nf[n] = 4;
            continue;
        }
        if (isword("FOR")) {
            adv();
            { int t_ = parseexpr(); nb[n] = t_; }
            continue;
        }
        if (isword("WHILE")) {
            adv();
            { int t_ = parseexpr(); nc[n] = t_; }
            continue;
        }
        break;
    }
}

/* a comma separated list of file names, linked through nx */
static int filelist(void)
{
    int h, t, x;

    h = filearg();
    t = h;
    while (isop(",")) {
        adv();
        x = filearg();
        nx[t] = x;
        t = x;
    }
    return h;
}

static int isscopedw(const char *w)
{
    return strcmp(w, "DELETE") == 0 || strcmp(w, "RECALL") == 0
        || strcmp(w, "COUNT") == 0 || strcmp(w, "SUM") == 0
        || strcmp(w, "AVERAGE") == 0 || strcmp(w, "LIST") == 0
        || strcmp(w, "DISPLAY") == 0 || strcmp(w, "LOCATE") == 0
        || strcmp(w, "REPLACE") == 0 || strcmp(w, "COPY") == 0
        || strcmp(w, "TOTAL") == 0;
}

static int parse_use(void)
{
    int n, fl, x;

    adv();
    n = mkcmd(C_USE);
    if (atend()) return n;
    if (!isword("INDEX") && !isword("ALIAS") && !isword("NEW"))
        { int t_ = filearg(); nb[n] = t_; }
    fl = 0;
    for (;;) {
        if (isword("INDEX")) {
            adv();
            { int t_ = filelist(); nd[n] = t_; }
            continue;
        }
        if (isword("ALIAS")) {
            adv();
            { int t_ = aliasarg(); nc[n] = t_; }
            continue;
        }
        if (isword("NEW")) {
            adv();
            fl = fl + 1;
            continue;
        }
        if (isword("EXCLUSIVE")) {
            adv();
            continue;
        }
        if (isword("SHARED")) {
            adv();
            continue;
        }
        if (isword("READONLY")) {
            adv();
            fl = fl + 8;
            continue;
        }
        if (isword("VIA")) {
            adv();
            x = parseexpr();
            (void)x;
            continue;
        }
        break;
    }
    nf[n] = fl;
    return n;
}

static int parse_index(void)
{
    int n, fl;
    long t0;
    char *ktxt, *ftxt;
    Sb b;

    adv();
    n = mkcmd(C_INDEX);
    if (isword("ON")) adv();
    else perr("Syntax error: ON expected after INDEX");
    t0 = tp;
    { int t_ = parseexpr(); nb[n] = t_; }
    ktxt = toktext(t0, tp);
    ftxt = "";
    if (isword("TO") || isword("TAG")) {
        adv();
        { int t_ = filearg(); nc[n] = t_; }
    }
    fl = 0;
    for (;;) {
        if (isword("FOR")) {
            adv();
            t0 = tp;
            { int t_ = parseexpr(); nd[n] = t_; }
            ftxt = toktext(t0, tp);
            continue;
        }
        if (isword("UNIQUE")) {
            adv();
            fl = fl + 1;
            continue;
        }
        if (isword("DESCENDING")) {
            adv();
            fl = fl + 2;
            continue;
        }
        if (isword("ASCENDING") || isword("ADDITIVE")
            || isword("COMPACT") || isword("EVAL")
            || isword("EVERY")) {
            adv();
            continue;
        }
        break;
    }
    nf[n] = fl;
    sb_init(&b);
    sb_adds(&b, ktxt);
    sb_addc(&b, 1);
    sb_adds(&b, ftxt);
    ns[n] = xstrdup(sb_str(&b));
    return n;
}

int parsecmd(void)
{
    const char *w;
    int n, cmd, k;

    w = ts[tp];
    if (strcmp(w, "USE") == 0) return parse_use();
    if (strcmp(w, "SELECT") == 0) {
        adv();
        n = mkcmd(C_SELECT);
        { int t_ = aliasarg(); nb[n] = t_; }
        return n;
    }
    if (strcmp(w, "GO") == 0 || strcmp(w, "GOTO") == 0) {
        adv();
        n = mkcmd(C_GO);
        if (isword("TOP")) {
            adv();
            nf[n] = 1;
        } else if (isword("BOTTOM")) {
            adv();
            nf[n] = 2;
        } else {
            if (isword("RECORD")) adv();
            { int t_ = parseexpr(); nb[n] = t_; }
        }
        if (isword("IN")) {
            adv();
            { int t_ = aliasarg(); nc[n] = t_; }
        }
        return n;
    }
    if (strcmp(w, "SKIP") == 0) {
        adv();
        n = mkcmd(C_SKIP);
        if (!atend() && !isword("ALIAS") && !isword("IN"))
            { int t_ = parseexpr(); nb[n] = t_; }
        if (isword("ALIAS") || isword("IN")) {
            adv();
            { int t_ = aliasarg(); nc[n] = t_; }
        }
        return n;
    }
    if (strcmp(w, "SEEK") == 0) {
        adv();
        n = mkcmd(C_SEEK);
        { int t_ = parseexpr(); nb[n] = t_; }
        for (;;) {
            if (isword("SOFTSEEK")) {
                adv();
                nf[n] = nf[n] + 1;
                continue;
            }
            if (isword("LAST")) {
                adv();
                continue;
            }
            break;
        }
        return n;
    }
    if (strcmp(w, "APPEND") == 0) {
        adv();
        n = mkcmd(C_APPEND);
        if (isword("BLANK")) {
            adv();
            nf[n] = 1;
            return n;
        }
        if (isword("FROM")) {
            adv();
            nf[n] = 2;
            { int t_ = filearg(); nb[n] = t_; }
            if (isword("FOR")) {
                adv();
                { int t_ = parseexpr(); nd[n] = t_; }
            }
            skipline();
            return n;
        }
        nf[n] = 1;
        return n;
    }
    if (isscopedw(w)) return parsescopedw(w);
    if (strcmp(w, "CONTINUE") == 0) {
        adv();
        return mkcmd(C_CONTINUE);
    }
    if (strcmp(w, "PACK") == 0) {
        adv();
        return mkcmd(C_PACK);
    }
    if (strcmp(w, "ZAP") == 0) {
        adv();
        return mkcmd(C_ZAP);
    }
    if (strcmp(w, "COMMIT") == 0) {
        adv();
        return mkcmd(C_COMMIT);
    }
    if (strcmp(w, "REINDEX") == 0) {
        adv();
        return mkcmd(C_REINDEX);
    }
    if (strcmp(w, "INDEX") == 0) return parse_index();
    if (strcmp(w, "CLOSE") == 0) {
        adv();
        n = mkcmd(C_CLOSE);
        if (!atend()) {
            if (isword("ALL") || isword("DATABASES")) {
                adv();
                nf[n] = 1;
            } else if (isword("INDEXES")) {
                adv();
                nf[n] = 2;
            } else { int t_ = aliasarg(); nb[n] = t_; }
        }
        skipline();
        return n;
    }
    if (strcmp(w, "CLEAR") == 0 || strcmp(w, "CLS") == 0) {
        k = (strcmp(w, "CLS") == 0);
        adv();
        n = mkcmd(C_CLEAR);
        if (k) nf[n] = 1;
        else if (isword("MEMORY")) nf[n] = 2;
        else if (isword("ALL")) nf[n] = 3;
        else nf[n] = 1;
        skipline();
        return n;
    }
    if (strcmp(w, "ERASE") == 0) {
        adv();
        n = mkcmd(C_ERASE);
        { int t_ = filearg(); nb[n] = t_; }
        return n;
    }
    if (strcmp(w, "RENAME") == 0) {
        adv();
        n = mkcmd(C_RENAME);
        { int t_ = filearg(); nb[n] = t_; }
        if (isword("TO")) adv();
        { int t_ = filearg(); nc[n] = t_; }
        return n;
    }
    if (strcmp(w, "ACCEPT") == 0 || strcmp(w, "INPUT") == 0
        || strcmp(w, "WAIT") == 0) {
        cmd = C_ACCEPT;
        if (strcmp(w, "INPUT") == 0) cmd = C_INPUT;
        if (strcmp(w, "WAIT") == 0) cmd = C_WAIT;
        adv();
        n = mkcmd(cmd);
        if (!atend() && !isword("TO")) {
            int t_ = parseexpr();
            nb[n] = t_;
        }
        if (isword("TO")) {
            adv();
            { int t_ = parsepostfix(); nc[n] = t_; }
        }
        return n;
    }
    if (strcmp(w, "READ") == 0) {
        adv();
        skipline();
        return mkcmd(C_READ);
    }
    if (strcmp(w, "QUIT") == 0 || strcmp(w, "CANCEL") == 0) {
        adv();
        skipline();
        return mkcmd(C_QUIT);
    }
    if (strcmp(w, "RELEASE") == 0) {
        adv();
        n = mkcmd(C_RELEASE);
        if (isword("ALL")) {
            adv();
            nf[n] = 1;
            skipline();
            return n;
        }
        { int t_ = parselist(); nb[n] = t_; }
        return n;
    }
    if (strcmp(w, "STORE") == 0) {
        adv();
        n = mkcmd(C_STORE);
        { int t_ = parseexpr(); nb[n] = t_; }
        if (isword("TO")) adv();
        else perr("Syntax error: TO expected after STORE");
        { int t_ = parselist(); nc[n] = t_; }
        return n;
    }
    if (strcmp(w, "KEYBOARD") == 0) {
        adv();
        n = mkcmd(C_KEYBOARD);
        { int t_ = parseexpr(); nb[n] = t_; }
        return n;
    }
    if (strcmp(w, "SET") == 0) return parseset();
    if (strcmp(w, "RUN") == 0) {
        adv();
        skipline();
        return mkcmd(C_RUN);
    }
    if (strcmp(w, "SORT") == 0 || strcmp(w, "UPDATE") == 0
        || strcmp(w, "JOIN") == 0 || strcmp(w, "SAVE") == 0
        || strcmp(w, "RESTORE") == 0) {
        perr(xfmt("Command %s is not supported", w));
        return 0;
    }
    if (strcmp(w, "CREATE") == 0) {
        perr("Command CREATE is not supported (use dbCreate())");
        return 0;
    }
    return 0;
}

static int parsescopedw(const char *w)
{
    int n, cmd, hold, head, tail, fld, v;

    adv();
    cmd = 0;
    if (strcmp(w, "DELETE") == 0) cmd = C_DELETE;
    if (strcmp(w, "RECALL") == 0) cmd = C_RECALL;
    if (strcmp(w, "COUNT") == 0) cmd = C_COUNT;
    if (strcmp(w, "SUM") == 0) cmd = C_SUM;
    if (strcmp(w, "AVERAGE") == 0) cmd = C_AVERAGE;
    if (strcmp(w, "LIST") == 0) cmd = C_LIST;
    if (strcmp(w, "DISPLAY") == 0) cmd = C_DISPLAY;
    if (strcmp(w, "LOCATE") == 0) cmd = C_LOCATE;
    if (strcmp(w, "REPLACE") == 0) cmd = C_REPLACE;
    if (strcmp(w, "COPY") == 0) cmd = C_COPY;
    if (strcmp(w, "TOTAL") == 0) cmd = C_TOTAL;
    n = mkscoped(cmd);
    hold = mknode(K_NOP, 0, 0, 0);
    ne[n] = hold;
    if (cmd == C_REPLACE) {
        parsescope(n);
        head = 0;
        tail = 0;
        for (;;) {
            if (tk[tp] != E_IDENT) {
                perr("Syntax error in REPLACE");
                break;
            }
            fld = fieldref();
            if (isword("WITH")) adv();
            else perr("Syntax error: WITH expected");
            v = parseexpr();
            nx[fld] = v;
            if (head == 0) head = fld;
            else nx[tail] = fld;
            tail = v;
            if (isop(",")) {
                adv();
                continue;
            }
            break;
        }
        na[hold] = head;
    } else if (cmd == C_COUNT) {
        if (isword("TO")) {
            adv();
            { int t_ = parsepostfix(); na[hold] = t_; }
        }
    } else if (cmd == C_SUM || cmd == C_AVERAGE) {
        if (!isword("TO") && !isword("FOR") && !isword("WHILE")
            && !isword("ALL") && !isword("NEXT")
            && !isword("REST")) {
            { int t_ = parselist(); na[hold] = t_; }
            nc[hold] = listcount;
        }
        if (isword("TO")) {
            adv();
            { int t_ = parselist(); nb[hold] = t_; }
        }
    } else if (cmd == C_LIST || cmd == C_DISPLAY) {
        if (isword("STRUCTURE")) {
            adv();
            nf[hold] = 1;
        } else {
            for (;;) {
                if (isword("OFF")) {
                    adv();
                    nd[hold] = 1;
                    continue;
                }
                if (isword("FIELDS")) adv();
                break;
            }
            if (!atend() && !isword("ALL") && !isword("NEXT")
                && !isword("REST") && !isword("FOR")
                && !isword("WHILE") && !isword("TO")
                && !isword("RECORD")) {
                { int t_ = parselist(); na[hold] = t_; }
            }
        }
    } else if (cmd == C_COPY) {
        if (isword("STRUCTURE")) {
            adv();
            nf[hold] = 1;
            if (isword("EXTENDED")) adv();
        }
        if (isword("TO")) {
            adv();
            { int t_ = filearg(); nb[hold] = t_; }
        }
        if (isword("FIELDS")) {
            adv();
            { int t_ = parselist(); na[hold] = t_; }
        }
    }
    parsescope(n);
    if (cmd == C_LIST || cmd == C_DISPLAY) {
        if (isword("TO")) skipline();
    }
    if (cmd == C_COPY) {
        if (isword("TO")) {
            adv();
            { int t_ = filearg(); nb[hold] = t_; }
        }
    }
    if (cmd == C_SUM || cmd == C_AVERAGE) {
        if (isword("TO")) {
            adv();
            { int t_ = parselist(); nb[hold] = t_; }
        }
    }
    if (cmd == C_COUNT) {
        if (isword("TO")) {
            adv();
            { int t_ = parsepostfix(); na[hold] = t_; }
        }
    }
    return n;
}

int parsescopedcmd(void)
{
    return parsescopedw(ts[tp]);
}

/* a field reference in a command: name or alias->name */
static int fieldref(void)
{
    int sym, s2;
    char *name;

    sym = tv[tp];
    name = ts[tp];
    adv();
    if (isop("->")) {
        adv();
        if (tk[tp] == E_IDENT) {
            s2 = tv[tp];
            adv();
            return mknode(K_ALIASFLD, constnode(mkstr(name)), s2, 0);
        }
        perr("Syntax error after ->");
        return constnode(NILV);
    }
    return mknode(K_FIELDX, sym, 0, 0);
}

static int skipset(const char *name)
{
    static const char *names[] = {
        "COLOR", "CURSOR", "KEY", "FUNCTION", "MESSAGE", "WRAP",
        "SCOREBOARD", "INTENSITY", "TYPEAHEAD", "DEVICE", "PRINTER",
        "MARGIN", "CONFIRM", "EVENTMASK", "BELL", "ESCAPE", "CANCEL",
        "COUNT", "MULTILOCKS", "AUTOPEN", "AUTORDER", 0
    };
    int i;

    for (i = 0; names[i]; i++)
        if (strcmp(name, names[i]) == 0) return 1;
    return 0;
}

static int parseset(void)
{
    int n, k, al, head, tail;
    char *name;

    adv();
    if (tk[tp] != E_IDENT) {
        perr("Syntax error after SET");
        return 0;
    }
    name = xstrdup(ts[tp]);
    adv();
    n = mkcmd(C_SET);
    ns[n] = name;
    if (skipset(name)) {
        skipline();
        return n;
    }
    if (strcmp(name, "DATE") == 0 && isword("FORMAT")) adv();
    if (strcmp(name, "ALTERNATE") == 0 && !isword("TO")
        && !isword("ON") && !isword("OFF")) {
        skipline();
        return n;
    }
    if (isword("TO")) adv();
    if (isword("ON")) {
        adv();
        nc[n] = 1;
        return n;
    }
    if (isword("OFF")) {
        adv();
        nc[n] = 2;
        return n;
    }
    if (strcmp(name, "INDEX") == 0) {
        if (!atend()) { int t_ = filelist(); nd[n] = t_; }
        return n;
    }
    if (strcmp(name, "RELATION") == 0) {
        if (isword("ADDITIVE")) adv();
        head = 0;
        tail = 0;
        while (!atend()) {
            k = parseexpr();
            if (isword("INTO")) adv();
            al = aliasarg();
            nx[k] = al;
            if (head == 0) head = k;
            else nx[tail] = k;
            tail = al;
            if (isop(",")) adv();
            else break;
        }
        nd[n] = head;
        return n;
    }
    if (atend()) return n;
    if (tk[tp] == E_IDENT) {
        /* a bare word value such as SET DATE ANSI, SET DEFAULT TO
           c:\data */
        if (strcmp(name, "DATE") == 0 || strcmp(name, "DEFAULT") == 0
            || strcmp(name, "PATH") == 0
            || strcmp(name, "ALTERNATE") == 0) {
            { int t_ = bareword(); nb[n] = t_; }
            return n;
        }
    }
    if (strcmp(name, "DEFAULT") == 0 || strcmp(name, "PATH") == 0) {
        { int t_ = filearg(); nb[n] = t_; }
        return n;
    }
    { int t_ = parseexpr(); nb[n] = t_; }
    skipline();
    return n;
}

/* source text of tokens a .. b-1 */
static char *toktext(long a, long b)
{
    Sb r;
    long i;

    sb_init(&r);
    for (i = a; i < b; i++) {
        if (i > a && tsp[i] == 1) sb_addc(&r, ' ');
        if (tk[i] == E_STR) {
            if (strchr(ts[i], '"') == NULL) {
                sb_addc(&r, '"');
                sb_adds(&r, ts[i]);
                sb_addc(&r, '"');
            } else {
                sb_addc(&r, '\'');
                sb_adds(&r, ts[i]);
                sb_addc(&r, '\'');
            }
        } else sb_adds(&r, tor[i]);
    }
    return sb_str(&r);
}
