/* lex.c -- tokenizer and preprocessor (port of src/20_lex.crexx) */
#include "xb_parse.h"

/* token tables (1-based), defined here */
long ntok = 0, tp = 1;
int *tk = 0;
char **ts = 0;
int *tl = 0;
int *tsp = 0;
int *tv = 0;
char **tor = 0;
int inblk = 0;
int curline = 0;
char *srcname = "";
char *srcdir = ".";
char *incpath = ".";

static long tkcap = 0, tscap = 0, tlcap = 0, tspcap = 0, tvcap = 0;
static long torcap = 0;

/* preprocessor state */
static int ifdepth = 0;
static int ifskip = 0;
static int *ifstate = 0;
static long ifcap = 0;
static char **dname = 0, **dval = 0, **dpar = 0;
static long ndef = 0, dncap = 0, dvcap = 0, dpcap = 0;
static long lexpos = -1;

static void lexlogical(const char *ln);
static void lexstring(const char *text, const char *nm);

int isidstart(int c)
{
    c &= 255;
    if (c >= 65 && c <= 90) return 1;
    if (c >= 97 && c <= 122) return 1;
    if (c == 95) return 1;
    if (c >= 192) return 1;
    return 0;
}

int isidchar(int c)
{
    c &= 255;
    if (c >= 65 && c <= 90) return 1;
    if (c >= 97 && c <= 122) return 1;
    if (c >= 48 && c <= 57) return 1;
    if (c == 95) return 1;
    if (c >= 192) return 1;
    return 0;
}

int isdigitc(int c)
{
    c &= 255;
    if (c >= 48 && c <= 57) return 1;
    return 0;
}

/* ------------------------------------------------ string helpers */
/* character i (0-based) of the n byte string s, 0 outside */
static int chat(const char *s, long n, long i)
{
    if (i < 0 || i >= n) return 0;
    return (unsigned char) s[i];
}

/* copy of at most len bytes from position from (0-based) */
static char *substrz(const char *s, long n, long from, long len)
{
    if (from < 0) from = 0;
    if (from >= n || len <= 0) return xstrdup("");
    if (from + len > n) len = n - from;
    return xstrndup(s + from, len);
}

/* position (0-based) of needle at or after from, -1 if none */
static long findfrom(const char *s, long n, const char *needle,
                     long from)
{
    const char *r;

    if (from < 0) from = 0;
    if (from > n) return -1;
    r = strstr(s + from, needle);
    if (r == NULL) return -1;
    return (long) (r - s);
}

/* the k-th (1-based) blank delimited word, "" if none */
static char *wordn(const char *s, int k)
{
    long i = 0, st;
    int cnt = 0;

    for (;;) {
        while (s[i] == ' ') i++;
        if (s[i] == 0) return xstrdup("");
        st = i;
        while (s[i] != 0 && s[i] != ' ') i++;
        cnt++;
        if (cnt == k) return xstrndup(s + st, i - st);
    }
}

static char *striptrail(const char *s)
{
    long n = (long) strlen(s);

    while (n > 0 && s[n - 1] == ' ') n--;
    return xstrndup(s, n);
}

void addtok(int kind, const char *text, int sp)
{
    char *t;

    ntok++;
    growi(&tk, &tkcap, ntok + 2);
    grows(&ts, &tscap, ntok + 2);
    growi(&tl, &tlcap, ntok + 2);
    growi(&tsp, &tspcap, ntok + 2);
    growi(&tv, &tvcap, ntok + 2);
    grows(&tor, &torcap, ntok + 2);
    t = xstrdup(text);
    tk[ntok] = kind;
    ts[ntok] = t;
    tl[ntok] = curline;
    tsp[ntok] = sp;
    tv[ntok] = 0;
    tor[ntok] = t;
}

/* replace whole-word occurrences of name by repl */
static char *replword(const char *body, const char *name,
                      const char *repl)
{
    Sb res;
    long i = 0, j, n = (long) strlen(body);
    char *w, *u;

    sb_init(&res);
    while (i < n) {
        if (isidstart((unsigned char) body[i])) {
            j = i;
            while (j < n && isidchar((unsigned char) body[j])) j++;
            w = xstrndup(body + i, j - i);
            u = xupper(w);
            if (strcmp(u, name) == 0) sb_adds(&res, repl);
            else sb_adds(&res, w);
            i = j;
        } else {
            sb_addc(&res, (unsigned char) body[i]);
            i++;
        }
    }
    return xstrdup(sb_str(&res));
}

/* expand a function-like #define; p is just after the name.  Sets
   lexpos (position after the call, -1 if no call follows) */
static char *expandfn(int di, const char *ln, long p)
{
    long n = (long) strlen(ln), q = p, dep = 1, acap = 0;
    long nargs = 0, k;
    int inq = 0;
    char **actual = 0;
    char c, *body, *pn, *rep, *tmp, *u;
    const char *pl, *comma;
    Sb arg1;

    while (ln[q] == ' ') q++;
    if (ln[q] != '(') {
        lexpos = -1;
        return xstrdup("");
    }
    q++;
    sb_init(&arg1);
    while (q < n) {
        c = ln[q];
        if (inq != 0) {
            sb_addc(&arg1, c);
            if (c == inq) inq = 0;
        } else if (c == '"' || c == '\'') {
            inq = c;
            sb_addc(&arg1, c);
        } else if (c == '(' || c == '{' || c == '[') {
            dep++;
            sb_addc(&arg1, c);
        } else if (c == ')' || c == '}' || c == ']') {
            dep--;
            if (dep == 0) {
                nargs++;
                grows(&actual, &acap, nargs + 2);
                actual[nargs] = xstrip(sb_str(&arg1));
                q++;
                break;
            }
            sb_addc(&arg1, c);
        } else if (c == ',' && dep == 1) {
            nargs++;
            grows(&actual, &acap, nargs + 2);
            actual[nargs] = xstrip(sb_str(&arg1));
            sb_init(&arg1);
        } else {
            sb_addc(&arg1, c);
        }
        q++;
    }
    body = dval[di];
    pl = dpar[di];
    k = 0;
    while (*pl != 0) {
        comma = strchr(pl, ',');
        if (comma == NULL) {
            pn = xstrip(pl);
            pl = pl + strlen(pl);
        } else {
            tmp = xstrndup(pl, (long) (comma - pl));
            pn = xstrip(tmp);
            pl = comma + 1;
        }
        k++;
        rep = "";
        if (k <= nargs) rep = actual[k];
        u = xupper(pn);
        body = replword(body, u, rep);
    }
    lexpos = q;
    return body;
}

/* tokenize one chunk of text */
void lexchunk(const char *ln, int depth)
{
    long n = (long) strlen(ln), p = 0, q, j, k;
    int sp = 0, c, c2, c3, ec, di, s;
    char *orig, *w, *ex, *d, *op, *tmp;
    char buf[4];
    Sb sbd;
    double hv;

    while (p < n) {
        c = chat(ln, n, p);
        if (inblk == 1) {
            q = findfrom(ln, n, "*/", p);
            if (q < 0) return;
            inblk = 0;
            p = q + 2;
            sp = 1;
            continue;
        }
        if (c == ' ' || c == 9 || c == 13) {
            sp = 1;
            p++;
            continue;
        }
        c2 = chat(ln, n, p + 1);
        if (c == '/' && c2 == '/') return;
        if (c == '&' && c2 == '&') return;
        if (c == '/' && c2 == '*') {
            q = findfrom(ln, n, "*/", p + 2);
            if (q < 0) {
                inblk = 1;
                return;
            }
            p = q + 2;
            sp = 1;
            continue;
        }
        if (isidstart(c)) {
            j = p;
            while (j < n && isidchar(chat(ln, n, j))) j++;
            orig = xstrndup(ln + p, j - p);
            w = xupper(orig);
            p = j;
            s = intern(w);
            di = sdef[s];
            if (di > 0 && depth < 8) {
                if (dpar[di][0] == 0) {
                    lexchunk(dval[di], depth + 1);
                } else {
                    ex = expandfn(di, ln, p);
                    if (lexpos >= 0) {
                        p = lexpos;
                        lexchunk(ex, depth + 1);
                    } else {
                        addtok(E_IDENT, w, sp);
                        tv[ntok] = s;
                        tor[ntok] = orig;
                    }
                }
                sp = 0;
                continue;
            }
            addtok(E_IDENT, w, sp);
            tv[ntok] = s;
            tor[ntok] = orig;
            sp = 0;
            continue;
        }
        if (isdigitc(c) || (c == '.' && isdigitc(c2))) {
            if (c == '0' && (c2 == 'x' || c2 == 'X')) {
                j = p + 2;
                hv = 0.0;
                while (j < n) {
                    c3 = toupper(chat(ln, n, j));
                    if (c3 >= '0' && c3 <= '9') {
                        hv = hv * 16.0 + (c3 - '0');
                    } else if (c3 >= 'A' && c3 <= 'F') {
                        hv = hv * 16.0 + (c3 - 'A' + 10);
                    } else {
                        break;
                    }
                    j++;
                }
                addtok(E_NUM, xfmt("%.0f", hv), sp);
                p = j;
                sp = 0;
                continue;
            }
            if (c == '0' && (c2 == 'd' || c2 == 'D')
                && isdigitc(chat(ln, n, p + 2))) {
                tmp = substrz(ln, n, p + 2, 8);
                addtok(E_DATE, tmp, sp);
                p = p + 10;
                sp = 0;
                continue;
            }
            j = p;
            while (j < n && isdigitc(chat(ln, n, j))) j++;
            if (chat(ln, n, j) == '.'
                && isdigitc(chat(ln, n, j + 1))) {
                j++;
                while (j < n && isdigitc(chat(ln, n, j))) j++;
            } else if (chat(ln, n, j) == '.' && j > p) {
                /* "5." is a number unless .AND. .OR. .NOT. */
                c3 = chat(ln, n, j + 1);
                if (c3 == 0) {
                    j++;
                } else {
                    c3 = toupper(c3);
                    if (strchr("AONTF", c3) == NULL) j++;
                }
            }
            ec = chat(ln, n, j);
            if ((ec == 'e' || ec == 'E') && j > p) {
                k = j + 1;
                if (chat(ln, n, k) == '-' || chat(ln, n, k) == '+')
                    k++;
                if (isdigitc(chat(ln, n, k))) {
                    while (k < n && isdigitc(chat(ln, n, k))) k++;
                    j = k;
                }
            }
            addtok(E_NUM, xstrndup(ln + p, j - p), sp);
            p = j;
            sp = 0;
            continue;
        }
        if (c == '"' || c == '\'') {
            q = p + 1;
            while (q < n && chat(ln, n, q) != c) q++;
            if (q >= n) {
                addtok(E_STR, substrz(ln, n, p + 1, n), sp);
                p = n;
            } else {
                addtok(E_STR, substrz(ln, n, p + 1, q - p - 1), sp);
                p = q + 1;
            }
            sp = 0;
            continue;
        }
        if (c == '.') {
            q = findfrom(ln, n, ".", p + 1);
            if (q > 0 && q - p <= 4) {
                w = xupper(substrz(ln, n, p + 1, q - p - 1));
                if (strcmp(w, "T") == 0 || strcmp(w, "Y") == 0) {
                    addtok(E_LOG, "T", sp);
                    p = q + 1;
                    sp = 0;
                    continue;
                }
                if (strcmp(w, "F") == 0 || strcmp(w, "N") == 0) {
                    addtok(E_LOG, "F", sp);
                    p = q + 1;
                    sp = 0;
                    continue;
                }
                if (strcmp(w, "AND") == 0 || strcmp(w, "OR") == 0
                    || strcmp(w, "NOT") == 0) {
                    addtok(E_OP, xfmt(".%s.", w), sp);
                    p = q + 1;
                    sp = 0;
                    continue;
                }
            }
            addtok(E_OP, ".", sp);
            p++;
            sp = 0;
            continue;
        }
        if (c == '{' && c2 == '^') {
            q = findfrom(ln, n, "}", p);
            if (q >= 0) {
                tmp = xstrip(substrz(ln, n, p + 2, q - p - 2));
                sb_init(&sbd);
                for (k = 0; tmp[k] != 0; k++) {
                    if (tmp[k] != '-' && tmp[k] != '/')
                        sb_addc(&sbd, (unsigned char) tmp[k]);
                }
                d = xstrdup(sb_str(&sbd));
                addtok(E_DATE, d, sp);
                p = q + 1;
                sp = 0;
                continue;
            }
        }
        /* operators */
        buf[0] = (char) c;
        buf[1] = (char) c2;
        buf[2] = (char) chat(ln, n, p + 2);
        buf[3] = 0;
        op = 0;
        if (c == '*' && c2 == '*' && buf[2] == '=') {
            op = xstrndup(buf, 3);
        } else if (c2 != 0 && strstr(" := += -= *= /= %= ^= == != <>"
                    " <= >= -> :: ++ -- ** => ?? ",
                    xfmt(" %c%c ", c, c2)) != NULL
                   && c2 != ' ' && c != ' ') {
            op = xstrndup(buf, 2);
        } else {
            op = xstrndup(buf, 1);
        }
        if (strcmp(op, ";") == 0) addtok(E_EOL, ";", sp);
        else addtok(E_OP, op, sp);
        p += (long) strlen(op);
        sp = 0;
    }
}

/* ------------------------------------------------------ directives */
/* a non-empty file exists (an emptied file counts as absent) */
int fileexists(const char *fname)
{
    FILE *f;
    int r;

    f = fopen(fname, "rb");
    if (f == NULL) return 0;
    r = getc(f);
    fclose(f);
    if (r == EOF) return 0;
    return 1;
}

char *dirof(const char *f)
{
    const char *k = strrchr(f, '/');

    if (k == NULL) return xstrdup(".");
    if (k == f) return xstrdup("/");
    return xstrndup(f, (long) (k - f));
}

void definemacro(const char *name, const char *par, const char *val)
{
    char *u = xupper(name);
    int s = intern(u);
    int di = sdef[s];

    if (di == 0) {
        ndef++;
        di = (int) ndef;
        sdef[s] = di;
    }
    grows(&dname, &dncap, ndef + 2);
    grows(&dval, &dvcap, ndef + 2);
    grows(&dpar, &dpcap, ndef + 2);
    dname[di] = u;
    dval[di] = xstrdup(val);
    dpar[di] = xstrdup(par);
}

static void setifskip(void)
{
    ifskip = 0;
    if (ifdepth > 0) {
        if (ifstate[ifdepth] != 1) ifskip = 1;
    }
}

static const char *stdinclude(const char *nm);

static void includefile(const char *f)
{
    const char *std;
    char *cand, *u;
    long n;

    u = xupper(f);
    std = stdinclude(u);
    if (std[0] != 0) {
        lexstring(std, f);
        return;
    }
    cand = xfmt("%s/%s", srcdir, f);
    if (fileexists(f)) {
        cand = xstrdup(f);
    } else if (!fileexists(cand)) {
        cand = xfmt("%s/%s", incpath, f);
        if (!fileexists(cand)) {
            n = (long) strlen(u);
            if (n >= 3 && strcmp(u + n - 3, ".CH") == 0) return;
            errout(xfmt("Error: cannot open include file %s", f));
            exit(1);
        }
    }
    lexfile(cand);
}

static void ifpush(int st)
{
    ifdepth++;
    growi(&ifstate, &ifcap, ifdepth + 2);
    ifstate[ifdepth] = st;
}

/* t starts with # */
static void dodirective(const char *t)
{
    char *body, *cmd, *rest, *w, *name, *par, *val, *f, *tmp;
    long sp, j, q, i, k, n;
    int has, st, fn;
    char c;
    Sb sb;

    body = xstrip(t + 1);
    sp = -1;
    for (i = 0; body[i] != 0; i++) {
        if (body[i] == ' ') {
            sp = i;
            break;
        }
    }
    if (sp < 0) {
        for (i = 0; body[i] != 0; i++) {
            if (body[i] == 9) {
                sp = i;
                break;
            }
        }
    }
    if (sp < 0) {
        cmd = xupper(body);
        rest = xstrdup("");
    } else {
        cmd = xupper(xstrndup(body, sp));
        rest = xstrip(body + sp + 1);
    }
    /* conditionals first: they work when skipping */
    if (strcmp(cmd, "IFDEF") == 0 || strcmp(cmd, "IFNDEF") == 0) {
        if (ifskip == 1) {
            ifpush(4);
        } else {
            w = xupper(wordn(rest, 1));
            has = 0;
            { int sy_ = intern(w); if (sdef[sy_] > 0) has = 1; }
            if (strcmp(cmd, "IFNDEF") == 0) has = 1 - has;
            if (has == 1) ifpush(1);
            else ifpush(2);
        }
        setifskip();
        return;
    }
    if (strcmp(cmd, "IF") == 0) {
        if (ifskip == 1) {
            ifpush(4);
        } else {
            w = xstrip(rest);
            if (strcmp(w, "0") == 0 || strcmp(w, ".F.") == 0)
                ifpush(2);
            else ifpush(1);
        }
        setifskip();
        return;
    }
    if (strcmp(cmd, "ELSE") == 0) {
        if (ifdepth > 0) {
            st = ifstate[ifdepth];
            if (st == 1) ifstate[ifdepth] = 3;
            else if (st == 2) ifstate[ifdepth] = 1;
        }
        setifskip();
        return;
    }
    if (strcmp(cmd, "ENDIF") == 0) {
        if (ifdepth > 0) ifdepth--;
        setifskip();
        return;
    }
    if (ifskip == 1) return;
    if (strcmp(cmd, "DEFINE") == 0) {
        /* NAME or NAME(args) value */
        n = (long) strlen(rest);
        j = 0;
        while (j < n && isidchar((unsigned char) rest[j])) j++;
        name = xstrndup(rest, j);
        par = xstrdup("");
        fn = 0;
        if (rest[j] == '(') {
            q = findfrom(rest, n, ")", j);
            if (q >= 0) {
                par = xstrndup(rest + j + 1, q - j - 1);
                if (xstrip(par)[0] == 0) par = xstrdup(" ");
                fn = 1;
                j = q + 1;
            }
        }
        val = xstrip(rest + j);
        if (fn == 1 && xstrip(par)[0] == 0) par = xstrdup("");
        definemacro(name, par, val);
        return;
    }
    if (strcmp(cmd, "UNDEF") == 0) {
        w = xupper(wordn(rest, 1));
        { int sy_ = intern(w); sdef[sy_] = 0; }
        return;
    }
    if (strcmp(cmd, "INCLUDE") == 0) {
        tmp = xstrip(rest);
        sb_init(&sb);
        for (k = 0; tmp[k] != 0; k++) {
            c = tmp[k];
            if (c != '"' && c != '<' && c != '>' && c != '\'')
                sb_addc(&sb, (unsigned char) c);
        }
        f = xstrdup(sb_str(&sb));
        includefile(f);
        return;
    }
    /* #pragma, #command, #translate, #stdout, #error ... ignored */
}

/* tokenize a block of text with line feeds */
static void lexstring(const char *text, const char *nm)
{
    const char *rest = text;
    const char *k;
    char *ln;

    (void) nm;
    while (*rest != 0) {
        k = strchr(rest, '\n');
        if (k == NULL) {
            ln = xstrdup(rest);
            rest = rest + strlen(rest);
        } else {
            ln = xstrndup(rest, (long) (k - rest));
            rest = k + 1;
        }
        lexlogical(ln);
    }
}

/* one logical line */
static void lexlogical(const char *ln)
{
    const char *t = ln;
    char *w1;
    int c;

    while (*t == ' ') t++;
    if (inblk == 1) {
        lexchunk(ln, 0);
        if (inblk == 0 && ifskip == 0) addtok(E_EOL, "", 0);
        return;
    }
    if (*t == 0) return;
    c = (unsigned char) t[0];
    if (c == '#') {
        dodirective(t);
        return;
    }
    if (ifskip == 1) return;
    if (c == '*') return;
    w1 = xupper(wordn(t, 1));
    if (strcmp(w1, "NOTE") == 0) return;
    if (t[0] == '&' && t[1] == '&') return;
    lexchunk(ln, 0);
    addtok(E_EOL, "", 0);
}

/* the lines of a file, 1-based, in a new array; NULL if it cannot be
   opened.  *cnt receives the number of lines */
static char **readlines(const char *fname, long *cnt)
{
    FILE *f;
    char *buf;
    char **fl;
    long len = 0, cap = 4096, got, p, st, e, nl = 0, lcap = 64;

    f = fopen(fname, "rb");
    if (f == NULL) return NULL;
    buf = (char *) xmalloc(cap + 1);
    for (;;) {
        if (len + 4096 > cap) {
            cap = cap * 2 + 4096;
            buf = (char *) xrealloc(buf, cap + 1);
        }
        got = (long) fread(buf + len, 1, 4096, f);
        if (got <= 0) break;
        len += got;
    }
    fclose(f);
    buf[len] = 0;
    fl = (char **) xmalloc(lcap * (long) sizeof(char *));
    p = 0;
    while (p < len) {
        st = p;
        e = p;
        while (e < len && buf[e] != '\n') e++;
        p = e + 1;
        if (e < len) buf[e] = 0;
        if (e > st && buf[e - 1] == '\r') buf[e - 1] = 0;
        nl++;
        if (nl + 2 > lcap) {
            lcap = lcap * 2;
            fl = (char **) xrealloc(fl, lcap * (long) sizeof(char *));
        }
        fl[nl] = buf + st;
    }
    *cnt = nl;
    return fl;
}

int lexfile(const char *fname)
{
    char **fl;
    long nlines = 0, i;
    char *oldsrc, *oldname, *ln, *tt, *tl1, *w2, *rest2, *tl2;
    char *u1, *t3;
    Sb body2;
    long m;

    fl = readlines(fname, &nlines);
    if (fl == NULL) return 1;
    oldsrc = srcdir;
    srcdir = dirof(fname);
    oldname = srcname;
    srcname = xstrdup(fname);
    i = 1;
    while (i <= nlines) {
        ln = fl[i];
        curline = (int) i;
        for (;;) {
            tt = striptrail(ln);
            m = (long) strlen(tt);
            if (m > 0 && tt[m - 1] == ';' && i < nlines
                && inblk == 0) {
                ln = (char *) xmalloc(m + (long) strlen(fl[i + 1])
                                      + 2);
                memcpy(ln, tt, (size_t) (m - 1));
                ln[m - 1] = ' ';
                strcpy(ln + m, fl[i + 1]);
                i++;
            } else {
                break;
            }
        }
        tl1 = xstrip(ln);
        u1 = xupper(wordn(tl1, 1));
        if (strcmp(u1, "TEXT") == 0 && ifskip == 0 && inblk == 0) {
            w2 = wordn(tl1, 2);
            t3 = xupper(w2);
            if (w2[0] == 0 || strcmp(t3, "TO") == 0
                || strcmp(t3, "TOFILE") == 0) {
                addtok(E_IDENT, "TEXT", 0);
                tv[ntok] = intern("TEXT");
                rest2 = xstrip(tl1 + 4);
                if (rest2[0] != 0) lexchunk(rest2, 0);
                sb_init(&body2);
                i++;
                while (i <= nlines) {
                    tl2 = xstrip(fl[i]);
                    if (strlen(tl2) >= 7
                        && strcmp(xupper(xstrndup(tl2, 7)),
                                  "ENDTEXT") == 0)
                        break;
                    if (body2.n > 0) sb_addc(&body2, '\n');
                    sb_adds(&body2, fl[i]);
                    i++;
                }
                addtok(E_STR, sb_str(&body2), 1);
                addtok(E_EOL, "", 0);
                i++;
                continue;
            }
        }
        lexlogical(ln);
        i++;
    }
    srcdir = oldsrc;
    srcname = oldname;
    return 0;
}

/* standard headers: constants only */
static const char *stdinclude(const char *nm)
{
    const char *base = nm;
    const char *k = strrchr(nm, '/');

    if (k != NULL) base = k + 1;
    if (strcmp(base, "INKEY.CH") == 0) {
        return "#define K_ESC 27\n#define K_ENTER 13\n"
            "#define K_RETURN 13\n#define K_SPACE 32\n"
            "#define K_TAB 9\n#define K_BS 8\n#define K_UP 5\n"
            "#define K_DOWN 24\n#define K_LEFT 19\n"
            "#define K_RIGHT 4\n#define K_HOME 1\n"
            "#define K_END 6\n#define K_PGUP 18\n"
            "#define K_PGDN 3\n#define K_INS 22\n"
            "#define K_DEL 7\n#define K_CTRL_C 3\n"
            "#define K_F1 28\n#define K_F2 -1\n"
            "#define K_F10 -9";
    }
    if (strcmp(base, "COMMON.CH") == 0
        || strcmp(base, "STD.CH") == 0
        || strcmp(base, "HBCLASS.CH") == 0
        || strcmp(base, "HBVER.CH") == 0
        || strcmp(base, "SIMPLEIO.CH") == 0
        || strcmp(base, "DBINFO.CH") == 0
        || strcmp(base, "TBROWSE.CH") == 0
        || strcmp(base, "BOX.CH") == 0
        || strcmp(base, "SETCURS.CH") == 0
        || strcmp(base, "ACHOICE.CH") == 0
        || strcmp(base, "DIRECTRY.CH") == 0
        || strcmp(base, "MEMOEDIT.CH") == 0
        || strcmp(base, "DBEDIT.CH") == 0
        || strcmp(base, "RDDSYS.CH") == 0
        || strcmp(base, "HBDYN.CH") == 0
        || strcmp(base, "HBGTINFO.CH") == 0
        || strcmp(base, "HBINKEY.CH") == 0) {
        if (strcmp(base, "COMMON.CH") == 0)
            return "#define HB_COMMON_CH_";
        if (strcmp(base, "SETCURS.CH") == 0)
            return "#define SC_NONE 0\n#define SC_NORMAL 1\n"
                "#define SC_INSERT 2\n#define SC_SPECIAL1 3\n"
                "#define SC_SPECIAL2 4";
        if (strcmp(base, "BOX.CH") == 0)
            return "#define B_SINGLE \""
                "\332\304\277\263\331\304\300\263\"";
        if (strcmp(base, "DIRECTRY.CH") == 0)
            return "#define F_NAME 1\n#define F_SIZE 2\n"
                "#define F_DATE 3\n#define F_TIME 4\n"
                "#define F_ATT 5";
        return "#define HB_STD_CH_";
    }
    if (strcmp(base, "FILEIO.CH") == 0) {
        return "#define FO_READ 0\n#define FO_WRITE 1\n"
            "#define FO_READWRITE 2\n#define FO_COMPAT 0\n"
            "#define FO_EXCLUSIVE 16\n#define FO_DENYWRITE 32\n"
            "#define FO_DENYREAD 48\n#define FO_DENYNONE 64\n"
            "#define FO_SHARED 64\n#define FC_NORMAL 0\n"
            "#define FS_SET 0\n#define FS_RELATIVE 1\n"
            "#define FS_END 2\n#define F_ERROR -1";
    }
    if (strcmp(base, "DBSTRUCT.CH") == 0) {
        return "#define DBS_NAME 1\n#define DBS_TYPE 2\n"
            "#define DBS_LEN 3\n#define DBS_DEC 4";
    }
    if (strcmp(base, "ERROR.CH") == 0) {
        return xfmt("%s%s",
            "#define ES_WHOCARES 0\n#define ES_WARNING 1\n"
            "#define ES_ERROR 2\n#define ES_CATASTROPHIC 3\n"
            "#define EG_ARG 1\n#define EG_BOUND 2\n"
            "#define EG_STROVERFLOW 3\n"
            "#define EG_NUMOVERFLOW 4\n#define EG_ZERODIV 5\n"
            "#define EG_NUMERR 6\n#define EG_SYNTAX 7\n"
            "#define EG_COMPLEXITY 8\n#define EG_MEM 11\n"
            "#define EG_NOFUNC 12\n#define EG_NOMETHOD 13\n"
            "#define EG_NOVAR 14\n#define EG_NOALIAS 15\n",
            "#define EG_NOVARMETHOD 16\n"
            "#define EG_BADALIAS 17\n#define EG_DUPALIAS 18\n"
            "#define EG_CREATE 20\n#define EG_OPEN 21\n"
            "#define EG_CLOSE 22\n#define EG_READ 23\n"
            "#define EG_WRITE 24\n#define EG_PRINT 25\n"
            "#define EG_UNSUPPORTED 30\n#define EG_LIMIT 31\n"
            "#define EG_CORRUPTION 32\n#define EG_DATATYPE 33\n"
            "#define EG_DATAWIDTH 34\n#define EG_NOTABLE 35\n"
            "#define EG_NOORDER 36\n#define EG_SHARED 37\n"
            "#define EG_UNLOCKED 38\n#define EG_READONLY 39\n"
            "#define EG_APPENDLOCK 40");
    }
    return "";
}
