/* dot.c -- the dot prompt: xbase without a program, as dBASE's "."
 *
 * Each line typed is lexed and parsed as the body of a function of its
 * own and run at once; work areas, indexes, SET values and memory
 * variables stay from line to line.  An assignment to a name that is
 * neither a variable nor a field makes a memory variable (dotmode, as at
 * dBASE's dot prompt).  A line that opens a block (IF, DO WHILE, FOR,
 * DO CASE, BEGIN SEQUENCE ...) is continued on the next lines, prompt
 * "..", until the block is complete.  An error is reported and the
 * prompt comes back; QUIT (or the end of the input) ends xbase.
 * Copyright (c) 2026 Mike Beer. */
#include "xb.h"

int dotmode = 0;

#define DOTMAX 16384

static void dothelp(void)
{
    static const char *h[] = {
        "The dot prompt runs each line at once; QUIT ends it.",
        "  USE file   USE file INDEX ix   CLOSE   CLOSE ALL   SELECT n",
        "  LIST   LIST fields FOR cond   DISPLAY   DISPLAY STRUCTURE",
        "  GO TOP   GO BOTTOM   GO n   SKIP   SKIP n   ? expr, expr",
        "  APPEND BLANK   REPLACE field WITH value   DELETE   PACK",
        "  INDEX ON key TO file   SET INDEX TO file   SEEK key",
        "  LOCATE FOR cond   COUNT TO var   SUM field TO var",
        "  x := expr   STORE expr TO var   SET FILTER TO cond",
        "  DO prog      runs prog.prg; its procedures stay known",
        "  IF, DO WHILE, FOR ... go on over the next lines up to the END",
        "Functions: dbCreate(), RecCount(), Upper(), Str() ... (the manual)",
        NULL
    };
    int i;
    for (i = 0; h[i] != NULL; i++) printf("%s\n", h[i]);
}

static char *upword(const char *s)
{
    static char w[16];
    int i = 0;
    while (*s == ' ' || *s == '\t') s++;
    while (*s != 0 && *s != ' ' && *s != '\t' && i < 15) {
        w[i++] = (char)((*s >= 'a' && *s <= 'z') ? *s - 32 : *s);
        s++;
    }
    w[i] = 0;
    return w;
}

/* a syntax error at the end of the input continues the statement on the
   next line only when the first line opened a block */
static int opensblock(const char *ln)
{
    static const char *op[] = { "IF", "DO", "WHILE", "FOR", "BEGIN",
                                "SWITCH", "TRY", "WITH", NULL };
    const char *w = upword(ln);
    int i;
    for (i = 0; op[i] != NULL; i++)
        if (strcmp(w, op[i]) == 0) return 1;
    return 0;
}

static int incomplete(const char *first)
{
    return perrflag == 1 && tk[tp] == E_EOF && opensblock(first);
}

/* DO name, where name is no procedure yet but name.prg exists: load the
   program file (its procedures stay known) and run it; 1 if done */
static int doprogram(const char *ln)
{
    char w1[16], w2[64], fname[80];
    const char *p = ln;
    int i = 0, omain, ofirst, fn;
    long start;
    FILE *f;

    strcpy(w1, upword(ln));
    if (strcmp(w1, "DO") != 0) return 0;
    while (*p == ' ') p++;
    p += 2;
    while (*p == ' ') p++;
    while (*p != 0 && *p != ' ' && i < 60) w2[i++] = *p++;
    w2[i] = 0;
    while (*p == ' ') p++;
    if (w2[0] == 0 || *p != 0) return 0;          /* DO WHILE ..., DO x WITH */
    if (strcmp(xupper(w2), "WHILE") == 0 || strcmp(xupper(w2), "CASE") == 0)
        return 0;
    if (sfn[intern(xupper(w2))] != 0) return 0;   /* a known procedure */
    if (strchr(w2, '.') != NULL) sprintf(fname, "%s", w2);
    else sprintf(fname, "%s.prg", w2);
    f = fopen(fname, "r");
    if (f == NULL) return 0;
    fclose(f);
    start = ntok + 1;
    if (lexfile(fname) != 0) return 0;
    addtok(E_EOL, "", 0);
    addtok(E_EOF, "", 0);
    tp = start;
    omain = mainfn;
    ofirst = firstfn;
    mainfn = 0;
    firstfn = 0;
    perrflag = 0;
    parseprogram();
    fn = mainfn != 0 ? mainfn : firstfn;
    mainfn = omain;
    firstfn = ofirst;
    if (perrflag == 1) {
        errout(xfmt("%s(%d) Error: %s", fname, perrline, perrmsg));
        perrflag = 0;
        return 1;
    }
    if (fn == 0) {
        errout(xfmt("Error: %s has no procedure", fname));
        return 1;
    }
    ctl = 0;
    outstarted = 0;
    seqdepth++;
    runfn(fn, 0);
    seqdepth--;
    return 1;
}

void dotprompt(void)
{
    static char text[DOTMAX];
    char line[1024], first[1024];
    long start, n;
    int fid, more = 0;

    dotmode = 1;
    printf("xBase dot prompt (HELP, QUIT)\n");
    fflush(stdout);
    text[0] = 0;
    for (;;) {
        fputs(more ? ".. " : ". ", stdout);
        fflush(stdout);
        if (fgets(line, sizeof line, stdin) == NULL) break;
        n = (long)strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'
                         || line[n - 1] == ' '))
            line[--n] = 0;
        if (more == 0) {
            if (n == 0) continue;
            if (strcmp(upword(line), "HELP") == 0) {
                dothelp();
                continue;
            }
            text[0] = 0;
            strcpy(first, line);
            if (doprogram(line) == 1) {
                if (ctl == E_BRK && !isnil(brkval)) {
                    flushout();
                    if (outcol > 0) { fputc('\n', stdout); outcol = 0; }
                    errout(errmessage(brkval));
                }
                brkval = NILV;
                ctl = 0;
                flushout();
                if (outcol > 0) { fputc('\n', stdout); outcol = 0; }
                fflush(stdout);
                continue;
            }
        }
        if ((long)strlen(text) + n + 2 >= DOTMAX) {
            errout("Error: the block is too long for the dot prompt");
            more = 0;
            continue;
        }
        strcat(text, line);
        strcat(text, "\n");
        /* lex everything typed for this statement again, after the
           tokens of the earlier lines (they stay: functions refer to
           them) */
        start = ntok + 1;
        {
            char *p = text, *q;
            while (*p != 0) {
                q = strchr(p, '\n');
                *q = 0;
                lexline(p);
                *q = '\n';
                p = q + 1;
            }
        }
        addtok(E_EOL, "", 0);
        addtok(E_EOF, "", 0);
        tp = start;
        perrflag = 0;
        fid = parsedot();
        if (fid == 0) {
            if (incomplete(first)) {
                more = 1;
                perrflag = 0;
                continue;
            }
            errout(xfmt("Error: %s", perrmsg));
            perrflag = 0;
            more = 0;
            continue;
        }
        more = 0;
        /* run it; an error ends the line, not xbase (as BEGIN SEQUENCE) */
        ctl = 0;
        outstarted = 0;
        outcol = 0;
        seqdepth++;
        runfn(fid, 0);
        seqdepth--;
        if (ctl == E_BRK) {
            flushout();
            if (outcol > 0) {
                fputc('\n', stdout);
                outcol = 0;
            }
            if (!isnil(brkval)) errout(errmessage(brkval));
            brkval = NILV;
        }
        ctl = 0;
        flushout();
        if (outcol > 0) {
            fputc('\n', stdout);
            outcol = 0;
        }
        fflush(stdout);
    }
    dotmode = 0;
}
