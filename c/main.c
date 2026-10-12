/* main.c -- driver, start up and shut down.
   C port of src/08_main.crexx and src/90_init.crexx */
#include "xb.h"

void initall(void)
{
    TRUEV.u.j = 1;
    FALSEV.u.j = 0;
    intern("NIL");
    initerrclass();
    initprims();
    initareas();
    definemacro("__HARBOUR__", "", "1");
    definemacro("__XBASE__", "", "1");
    definemacro("__PLATFORM__", "", "\"LINUX\"");
    definemacro("HB_VERSION", "", "\"3.2.0\"");
}

void runfn(int f, int withargs)
{
    long base = ftop, slots, i;
    int cnt = 0;
    if (withargs == 1) cnt = nargvs;
    slots = fnslots[f];
    if (cnt > slots) slots = cnt;
    SETFTOP(base + slots);
    for (i = 1; i <= slots; i++) fv[base + i] = NILV;
    if (withargs == 1)
        for (i = 1; i <= nargvs; i++)
            fv[base + i] = mkstr(argvs[i]);
    invoke(f, base, cnt, NILV, 0);
}

void runprogram(void)
{
    long i;
    int fn;
    for (i = 1; i <= nsvinit; i++) {
        V v = evalnode(svinit[i]);
        sv[svslot[i]] = v;
        if (ctl != 0) break;
    }
    ctl = 0;
    fn = mainfn;
    if (fn == 0) fn = firstfn;
    if (fn == 0) {
        errout("Error: no entry point");
        exitcode = 1;
        return;
    }
    for (i = 1; i <= ninit; i++) runfn(initfns[i], 0);
    runfn(fn, 1);
    if (ctl == E_BRK) ctl = 0;
    for (i = 1; i <= nexitp; i++) runfn(exitfns[i], 0);
}

void shutdown_all(void)
{
    closeallareas();
    flushout();
    if (outcol > 0) {
        fputc('\n', stdout);
        outcol = 0;
    }
    fflush(stdout);
}

void doquit(int code)
{
    long i;
    exitcode = code;
    for (i = 1; i <= nexitp; i++) runfn(exitfns[i], 0);
    shutdown_all();
    exit(code);
}

int main(int argc, char **argv)
{
    int ai;
    fvgrow();
    ftop = 0;
    initall();
    if (argc < 2) {
        fputs("usage: xbase program.prg [arguments]\n", stderr);
        return 2;
    }
    progname = argv[1];
    progdir = dirof(progname);
    incpath = progdir;
    nargvs = argc - 2;
    argvs = (char **)xmalloc((long)(argc + 1) * (long)sizeof(char *));
    for (ai = 2; ai < argc; ai++) argvs[ai - 1] = argv[ai];
    if (lexfile(progname) != 0) {
        fprintf(stderr, "Error: cannot open file %s\n", progname);
        return 1;
    }
    addtok(E_EOL, "", 0);
    addtok(E_EOF, "", 0);
    tp = 1;
    parseprogram();
    if (perrflag == 1) {
        errout(xfmt("%s(%d) Error: %s", progname, perrline,
                    perrmsg));
        return 1;
    }
    runprogram();
    shutdown_all();
    return exitcode;
}
