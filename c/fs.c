/* fs.c -- the full-screen console through GDSM (mikebeer/gdsm)
 *
 * With XB_GDSM (linked with libgdsm.a) SET GDSM ON puts the console on a
 * GDSM screen, as the cREXX version's 80_gdsm does: @ r, c SAY / GET are
 * GDSM fields (ASDFLD, ASCPUT), READ is a full-screen read (ASREAD) that
 * puts the fields back into the GET variables (ASCGET), LastKey() and
 * Updated() follow it, CLEAR / CLS clear the page.  BROWSE, EDIT (CHANGE)
 * and APPEND are dBASE's full-screen table commands on top of it; they
 * open the screen themselves and give it back after.
 *
 * Without XB_GDSM, or where GDSM cannot open a full screen (a console
 * that is not a 3270), the console stays line oriented and BROWSE / EDIT
 * / APPEND work line by line.
 *
 * On CMS the full-screen driver (GDSM's "cms": DIAGNOSE X'58', format-0
 * CCWs) needs its buffers below 16 MB; an XL image gets a block there
 * from the XL loader (cms_lowblock, M5f cmsrt) and hands it to GDSM.
 * Copyright (c) 2026 Mike Beer. */
#include "xb.h"
#ifdef XB_GDSM
#include "gdsm.h"
#endif

int fsmode = 0;          /* 1: the console is a GDSM screen */
int xb_fslkey = 0;          /* LastKey() after a full-screen READ */
int xb_fsupd = 0;           /* Updated() */
static int fsrows = 24, fscols = 80;

/* -------------------------------------------------- values from input */

static int blankstr(const char *s)
{
    while (*s == ' ') s++;
    return *s == 0;
}

/* the text typed for a field, as a value of the old value's type */
V fs_convert(V old, const char *line, int upper)
{
    V nv = old;
    char *ln = xstrdup(line), *u;
    long w, ll;
    Sb b;

    if (upper) ln = xupper(ln);
    if (isstr(old)) {
        w = SL(old);
        ll = (long)strlen(ln);
        if (ll >= w) nv = mkstrn(ln, w);
        else {
            sb_init(&b);
            sb_adds(&b, ln);
            while (b.n < w) sb_addc(&b, ' ');
            nv = sb_val(&b);
        }
    } else if (isnum(old)) {
        if (!blankstr(ln)) {
            nv = valnum(ln);
            if (isnum(nv) && old.t == T_NUM)
                nv = mknum(numval(nv), numwid(old), numdec(old));
        }
    } else if (isdate(old)) {
        if (!blankstr(ln)) nv = mkdate(parsedate(xstrip(ln), setdatefmt));
    } else if (islog(old)) {
        u = xupper(xstrip(ln));
        if (u[0] == 'Y' || u[0] == 'T') nv = mklog(1);
        else if (u[0] == 'N' || u[0] == 'F') nv = mklog(0);
    } else if (!blankstr(ln)) nv = mkstr(xstrip(ln));
    return nv;
}

#ifdef XB_GDSM
static int fsfid = 1000;
#ifdef __CMS__
/* M5f cmsrt: a block below 16 MB from the XL loader (0 in a MODULE,
   whose own storage is below 16 MB anyway) */
extern unsigned char *cms_lowblock(unsigned int *size) __attribute__((weak));
extern unsigned long gd_cms_lowsize(void);
extern int gd_cms_lowstore(void *p, unsigned long n);
#endif

int fs_available(void) { return 1; }

/* SET GDSM ON: 1 when the console is a full screen now */
int fs_on(void)
{
    int err = 0;
    char drv[16];

    if (fsmode == 1) return 1;
#ifdef __CMS__
    if (getenv("GDSM_DRIVER") == NULL) {
        unsigned int n = 0;
        unsigned char *p = cms_lowblock ? cms_lowblock(&n) : NULL;
        if (p == NULL || gd_cms_lowstore(p, n) == 0) gxdrv("cms");
    }
#endif
    fsinit();
    fsqerr(&err);
    gxqdrv(sizeof drv - 1, drv);
    drv[sizeof drv - 1] = 0;
    if (err != 0 || strncmp(drv, "text", 4) == 0 || strncmp(drv, "null", 4) == 0) {
        /* no full screen here: GDSM's line mode is not xBase's */
        if (getenv("GDSM_DRIVER") == NULL || strncmp(drv, "null", 4) != 0) {
            fsterm();
            return 0;
        }
    }
    fsmode = 1;
    fsfid = 1000;
    return 1;
}

void fs_off(void)
{
    if (fsmode == 0) return;
    fsterm();
    fsmode = 0;
}

void fs_clear(void)
{
    if (fsmode == 0) return;
    fspclr();
    fsfid = 1000;
}

/* a Clipper colour "W+/B" as a GDSM colour (the foreground) */
static int clipcol(const char *s)
{
    char f[16];
    int i = 0, j = 0;

    while (s[i] != 0 && s[i] != '/' && j < 15) {
        char c = s[i++];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (c != ' ' && c != '+' && c != '*') f[j++] = c;
    }
    f[j] = 0;
    if (strcmp(f, "B") == 0) return GD_BLUE;
    if (strcmp(f, "G") == 0) return GD_GREEN;
    if (strcmp(f, "BG") == 0 || strcmp(f, "GB") == 0) return GD_TURQUOISE;
    if (strcmp(f, "R") == 0) return GD_RED;
    if (strcmp(f, "RB") == 0 || strcmp(f, "BR") == 0) return GD_PINK;
    if (strcmp(f, "GR") == 0 || strcmp(f, "RG") == 0)
        return strchr(s, '+') != NULL ? GD_YELLOW : GD_BROWN;
    if (strcmp(f, "W") == 0) return GD_NEUTRAL;
    return GD_DEFAULT;
}

/* @ r, c SAY: a protected field; the column after it */
int fs_say(int r, int c, const char *txt, const char *color)
{
    int w = (int)strlen(txt), id;

    if (w == 0) return c;
    if (c + w > fscols) w = fscols - c;
    if (w <= 0 || r < 0 || r >= fsrows) return c;
    id = ++fsfid;
    asdfld(id, r + 1, c + 1, 1, w, GD_PROT);
    ascput(id, w, txt);
    if (color != NULL && color[0] != 0) {
        asfcol(id, clipcol(color));
        if (strchr(color, '+') != NULL) asfint(id, GD_BRIGHT);
    }
    return c + w;
}

/* @ r, c GET: an input field showing txt; its id */
int fs_get(int r, int c, const char *txt, int numeric, const char *color)
{
    int w = (int)strlen(txt), id;

    if (w < 1) w = 1;
    if (c + w > fscols) w = fscols - c;
    if (w <= 0 || r < 0 || r >= fsrows) return 0;
    id = ++fsfid;
    asdfld(id, r + 1, c + 1, 1, w, numeric ? GD_NUMERIC : GD_UNPROT);
    ascput(id, w, txt);
    asfhlt(id, GD_UNDERSCORE);
    if (color != NULL && color[0] != 0) asfcol(id, clipcol(color));
    return id;
}

void fs_protect(int id) { asftyp(id, GD_PROT); }
void fs_cursor(int id) { asfcur(id, 1, 1); }
void fs_alarm(void) { fsalrm(); }

/* the text in field id (trailing blanks off) */
char *fs_text(int id)
{
    int r, c, d, w, t;
    char *buf;

    asqfld(id, &r, &c, &d, &w, &t);
    if (w < 1) w = 1;
    buf = (char *)xmalloc((long)w + 1);
    memset(buf, ' ', (size_t)w);
    ascget(id, w, buf);
    buf[w] = 0;
    while (w > 0 && (buf[w - 1] == ' ' || buf[w - 1] == 0)) buf[--w] = 0;
    return buf;
}

/* the attention key as a Clipper key code (LastKey, Inkey) */
static int keycode(int t, int v)
{
    if (t == GD_ENTER || t == GD_CURSEL) return 13;
    if (t == GD_PF) {
        if (v == 1) return 28;
        if (v >= 2 && v <= 10) return 1 - v;
        if (v == 11) return -40;
        if (v == 12) return -41;
        if (v >= 13 && v <= 22) return 3 - v;
        if (v == 23) return -42;
        if (v == 24) return -43;
    }
    return 27;
}

/* show the page and wait for a key: *type GD_ENTER / GD_PF / ..., *pf
   the PF key number; the number of fields changed */
int fs_readscreen(int *type, int *pf)
{
    int t = 0, v = 0, m = 0;

    asread(&t, &v, &m);
    *type = t;
    *pf = t == GD_PF ? v : 0;
    xb_fslkey = keycode(t, v);
    return m;
}

#else  /* no GDSM: line mode only */
int fs_available(void) { return 0; }
int fs_on(void) { return 0; }
void fs_off(void) { }
void fs_clear(void) { }
int fs_say(int r, int c, const char *t, const char *col) { (void)r; (void)t; (void)col; return c; }
int fs_get(int r, int c, const char *t, int n, const char *col) { (void)r; (void)c; (void)t; (void)n; (void)col; return 0; }
void fs_protect(int id) { (void)id; }
void fs_cursor(int id) { (void)id; }
void fs_alarm(void) { }
char *fs_text(int id) { (void)id; return xstrdup(""); }
int fs_readscreen(int *type, int *pf) { *type = 5; *pf = 0; return 0; }
#endif

/* ------------------------------------------- BROWSE, EDIT and APPEND */

#define FS_ENTER 0
#define FS_PF 1
#define FS_CLOSE 5

static char *fieldtext(int a, int f)
{
    int k = fidx(a, f);
    V v = fieldval(a, f);
    char *t = valstr(v);
    long w = flen[k];
    Sb b;

    if (ftyp[k][0] == 'M') return xstrdup("<memo>");
    if (ftyp[k][0] == 'L') return xstrdup(truthy(v) ? "T" : "F");
    if ((ftyp[k][0] == 'N' || ftyp[k][0] == 'F') && isnum(v))
        return xfmt("%*.*f", flen[k], fdec[k], numval(v));
    sb_init(&b);
    sb_adds(&b, t);
    if (ftyp[k][0] == 'D' && w < 10) w = 10;
    while (b.n < w) sb_addc(&b, ' ');
    if (b.n > w) b.n = w;
    return xstrdup(sb_str(&b));
}

/* the text typed into field f of the current record goes back into it */
static int putfield(int a, int f, const char *txt)
{
    int k = fidx(a, f);
    V old = fieldval(a, f), nv;

    if (ftyp[k][0] == 'M') return 0;
    nv = fs_convert(old, txt, 0);
    fieldput(a, f, nv);
    return ctl == 0;
}

static int fwidth(int a, int f)
{
    int k = fidx(a, f), w = flen[k];
    if (ftyp[k][0] == 'D') w = 10;
    if (ftyp[k][0] == 'M') w = 6;
    if (ftyp[k][0] == 'L') w = 1;
    return w;
}

/* the line-mode form: each field's name and value, a new value typed for
   it (empty keeps it); "." ends the form early.  0 when "." was typed */
static int lineform(int a)
{
    int f;
    char *line;

    for (f = 1; f <= wa_nfld[a]; f++) {
        int k = fidx(a, f);
        if (ftyp[k][0] == 'M') continue;
        printf("%-10s %s : ", fnam[k], fieldtext(a, f));
        fflush(stdout);
        line = readcon();
        if (ineof == 1) return 0;
        if (strcmp(line, ".") == 0) return 0;
        if (line[0] != 0) putfield(a, f, line);
        if (ctl != 0) return 0;
    }
    return 1;
}

/* EDIT / CHANGE in line mode: the current record, then (Enter) the next */
static void edit_line(int a)
{
    for (;;) {
        if (wa_eof[a] == 1) return;
        printf("Record %d of %d (\".\" ends)\n", wa_recno[a], wa_nrec[a]);
        if (lineform(a) == 0) return;
        dbskip(a, 1);
    }
}

/* the full-screen form of one record: field names on the left, values
   in input fields; PF7/PF8 previous/next, PF3 ends; 1 = go on */
static int fs_form(int a, const char *title, int *move)
{
    int f, n = wa_nfld[a], row, t, pf, ids[64], first = 0, nf = 0;
    char hdr[100];

    *move = 0;
    fs_clear();
    sprintf(hdr, "%s  record %d of %d%s", title, wa_recno[a], wa_nrec[a],
            isdeleted(a) ? "  (deleted)" : "");
    fs_say(0, 0, hdr, "W+");
    row = 2;
    for (f = 1; f <= n && f <= 64; f++) {
        int k = fidx(a, f);
        ids[f - 1] = 0;
        if (row >= fsrows - 2) break;
        fs_say(row, 1, fnam[k], "G");
        ids[f - 1] = fs_get(row, 13, fieldtext(a, f), ftyp[k][0] == 'N', "");
        if (ftyp[k][0] == 'M') fs_protect(ids[f - 1]);
        else if (first == 0) first = ids[f - 1];
        row++;
        nf = f;
    }
    fs_say(fsrows - 1, 0, "Enter=save  PF3=end  PF7=previous  PF8=next", "B");
    if (first) fs_cursor(first);
    xb_fsupd = fs_readscreen(&t, &pf) > 0;
    if (t == FS_ENTER || t == FS_PF) {
        for (f = 1; f <= nf; f++)
            if (ids[f - 1] > 0) putfield(a, f, fs_text(ids[f - 1]));
    }
    if (t == FS_PF && (pf == 3 || pf == 15)) return 0;
    if (t != FS_ENTER && t != FS_PF) return 0;
    if (t == FS_PF && (pf == 7 || pf == 19)) *move = -1;
    else *move = 1;
    return 1;
}

/* EDIT [n] / CHANGE: one record at a time on the screen */
void fs_edit(int a)
{
    int was = fsmode, mv;

    if (wa_nrec[a] == 0) {
        errout("EDIT: the table has no records");
        return;
    }
    if (wa_eof[a] == 1) dbgotop(a);
    if (fs_on() == 0) {
        edit_line(a);
        return;
    }
    while (fs_form(a, "EDIT", &mv) == 1) {
        if (ctl != 0) break;
        if (mv < 0) {
            dbskip(a, -1);
            if (wa_bof[a] == 1) dbgotop(a);
        } else {
            dbskip(a, 1);
            if (wa_eof[a] == 1) dbgobottom(a);
        }
    }
    flushdbf(a);
    if (was == 0) fs_off();
}

/* APPEND: blank records filled in on the screen, one after the other;
   PF3 (or an untouched form) ends, an untouched last record goes away */
void fs_append(int a)
{
    int was = fsmode, mv, more = 1;

    if (fs_on() == 0) {
        for (;;) {
            dbappend(a);
            printf("New record %d (\".\" ends)\n", wa_recno[a]);
            if (lineform(a) == 0) {
                dbunappend(a);
                break;
            }
        }
        return;
    }
    while (more) {
        dbappend(a);
        more = fs_form(a, "APPEND", &mv);
        if (xb_fsupd == 0) {          /* nothing typed: the blank one goes */
            dbunappend(a);
            more = 0;
        }
        if (ctl != 0) break;
    }
    flushdbf(a);
    if (was == 0) fs_off();
}

/* BROWSE [FIELDS f1, f2 ...]: a page of records, one per line, the
   fields as input columns; PF7/PF8 page, Enter saves, PF3 ends */
void fs_browse(int a, int *flds, int nflds)
{
    int was = fsmode, page = fsrows - 4, i, j, t, pf, row, col, top;
    int ids[24][32], recs_[24], n, f, w;
    int all[32];
    Sb b;

    if (nflds == 0) {
        for (f = 1; f <= wa_nfld[a] && nflds < 32; f++)
            if (ftyp[fidx(a, f)][0] != 'M') all[nflds++] = f;
        flds = all;
    }
    if (wa_nrec[a] == 0) {
        errout("BROWSE: the table has no records");
        return;
    }
    if (wa_eof[a] == 1) dbgotop(a);
    if (fs_on() == 0) {            /* line mode: a listing */
        for (i = 0; i < page && wa_eof[a] == 0; i++) {
            sb_init(&b);
            sb_adds(&b, xfmt("%7d ", wa_recno[a]));
            for (j = 0; j < nflds; j++) {
                sb_adds(&b, fieldtext(a, flds[j]));
                sb_addc(&b, ' ');
            }
            printf("%s\n", sb_str(&b));
            dbskip(a, 1);
        }
        return;
    }
    if (page > 24) page = 24;
    for (;;) {
        fs_clear();
        top = wa_recno[a];
        sb_init(&b);
        col = 8;
        fs_say(0, 0, xfmt("BROWSE  %d records", wa_nrec[a]), "W+");
        for (j = 0; j < nflds; j++) {
            w = fwidth(a, flds[j]);
            if (col + w > fscols) break;
            fs_say(1, col, fnam[fidx(a, flds[j])], "G");
            col += w + 1;
        }
        n = 0;
        for (i = 0; i < page && wa_eof[a] == 0; i++) {
            row = i + 2;
            recs_[i] = wa_recno[a];
            fs_say(row, 0, xfmt("%6d%s", wa_recno[a], isdeleted(a) ? "*" : " "), "B");
            col = 8;
            for (j = 0; j < nflds && j < 32; j++) {
                w = fwidth(a, flds[j]);
                ids[i][j] = 0;
                if (col + w > fscols) break;
                ids[i][j] = fs_get(row, col, fieldtext(a, flds[j]),
                                   ftyp[fidx(a, flds[j])][0] == 'N', "");
                col += w + 1;
            }
            n++;
            dbskip(a, 1);
        }
        fs_say(fsrows - 1, 0, "Enter=save  PF3=end  PF7=back  PF8=forward", "B");
        if (n > 0 && ids[0][0] > 0) fs_cursor(ids[0][0]);
        xb_fsupd = fs_readscreen(&t, &pf) > 0;
        if (t == FS_ENTER || t == FS_PF) {
            for (i = 0; i < n; i++) {
                dbgoto(a, recs_[i]);
                for (j = 0; j < nflds && j < 32; j++)
                    if (ids[i][j] > 0) putfield(a, flds[j], fs_text(ids[i][j]));
            }
        }
        if (ctl != 0 || t == FS_CLOSE || (t != FS_ENTER && t != FS_PF)
            || (t == FS_PF && (pf == 3 || pf == 15)))
            break;
        if (t == FS_PF && (pf == 7 || pf == 19)) {
            dbgoto(a, top);
            dbskip(a, -page);
            if (wa_bof[a] == 1) dbgotop(a);
        } else if (t == FS_PF && (pf == 8 || pf == 20)) {
            dbgoto(a, recs_[n - 1]);
            dbskip(a, 1);
            if (wa_eof[a] == 1) dbgoto(a, top);
        } else dbgoto(a, top);
    }
    flushdbf(a);
    if (was == 0) fs_off();
}
