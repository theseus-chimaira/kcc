/* CCPPIN.C - compact cooked-token stream reader for split native KCC */

#include "cc.h"
#include "cclex.h"
#include "ccphase.h"
#include <stdlib.h>

extern SYMBOL *symfind(char *, int);

static char *tokbuf;
static unsigned int tokcap;
static int pushed;
static int stream_eof;
#if HOST_DAIMOS
static int native_trace_count;
extern unsigned long dsys_brk(unsigned long);
extern int dsys_getpid(void);
struct native_procinfo {
    unsigned long pid, ppid, state, words, comm;
};
extern int dsys_procinfo(unsigned int, struct native_procinfo *);
#endif

static int
getbyte(FILE *fp)
{
    int c;

    c = getc(fp);
    return c == EOF ? -1 : (c & 0377);
}

static int
get16(FILE *fp, unsigned int *vp)
{
    int a;
    int b;

    a = getbyte(fp);
    b = getbyte(fp);
    if (a < 0 || b < 0)
        return -1;
    *vp = ((unsigned int)a << 8) | (unsigned int)b;
    return 0;
}

static int
get18(FILE *fp, unsigned int *vp)
{
    int a;
    int b;
    int c;

    a = getbyte(fp);
    b = getbyte(fp);
    c = getbyte(fp);
    if (a < 0 || b < 0 || c < 0)
        return -1;
    *vp = (((unsigned int)a & 3U) << 16) |
        ((unsigned int)b << 8) | (unsigned int)c;
    return 0;
}

static int
getchars(FILE *fp, char *dst, unsigned int n)
{
    int c;

    while (n-- != 0U) {
        c = getbyte(fp);
        if (c < 0)
            return -1;
        *dst++ = (char)c;
    }
    *dst = 0;
    return 0;
}

static int
ensure_tokbuf(unsigned int n)
{
    char *p;
    unsigned int cap;

    if (n + 1U <= tokcap)
        return 0;
    cap = tokcap != 0U ? tokcap : 64U;
    while (cap < n + 1U) {
        if (cap > KCC_PHASE_MAX_PAYLOAD / 2U) {
            cap = KCC_PHASE_MAX_PAYLOAD + 1U;
            break;
        }
        cap *= 2U;
    }
    p = (char *)realloc(tokbuf, (size_t)cap);
#if HOST_DAIMOS
    fprintf(stderr, "KPIN: realloc n=%u cap=%u p=%lo brk=%lo\n",
        n, cap, (unsigned long)p, dsys_brk(0UL));
#endif
    if (p == NULL)
        return -1;
    tokbuf = p;
    tokcap = cap;
    return 0;
}

void
ppinit(void)
{
    int a;
    int b;
    int c;
    int d;

    pushed = 0;
    stream_eof = 0;
    eof = 0;
#if HOST_DAIMOS
    native_trace_count = 0;
    {
        struct native_procinfo pi;
        int pid = dsys_getpid();
        pi.words = 0UL;
        (void)dsys_procinfo((unsigned int)pid, &pi);
        fprintf(stderr, "KPIN: init pid=%d words=%lo brk=%lo\n",
            pid, pi.words, dsys_brk(0UL));
    }
#endif
    a = getbyte(in);
    b = getbyte(in);
    c = getbyte(in);
    d = getbyte(in);
    if (a != KCC_PHASE_MAGIC_0 || b != KCC_PHASE_MAGIC_1 ||
        c != KCC_PHASE_MAGIC_2 || d != KCC_PHASE_MAGIC_3)
        jerr("Input is not a KCC KPT4 preprocessor stream");
}

int
nextpp(void)
{
    int tag;
    unsigned int n;
    unsigned int fl;
    unsigned int tl;
    unsigned int pg;
    unsigned int ln;

    if (pushed) {
        pushed = 0;
        return curpp;
    }

    /* T_EOF is a terminal lexer state, not merely another record.  Native
     * DAIMOS files contain complete 36-bit words, so a byte-oriented KPT4
     * stream can have up to three zero padding bytes after its final record.
     * Parsers are allowed to request EOF repeatedly; never expose that
     * physical container padding as token data. */
    if (stream_eof)
        return T_EOF;

    for (;;) {
        tag = getbyte(in);
#if HOST_DAIMOS
        if (native_trace_count < 24) {
            fprintf(stderr, "KPIN: tag[%d]=%d brk=%lo\n",
                native_trace_count, tag, dsys_brk(0UL));
            ++native_trace_count;
        }
#endif
        if (tag < 0)
            jerr("Unexpected EOF in KCC preprocessor stream");

        if (tag == KCC_PHASE_LOCATION) {
            if (get18(in, &fl) != 0 || get18(in, &tl) != 0 ||
                get18(in, &pg) != 0 || get18(in, &ln) != 0 ||
                get16(in, &n) != 0 || n >= FNAMESIZE ||
                ensure_tokbuf(n) != 0 || getchars(in, tokbuf, n) != 0)
                jerr("Corrupt location record in KCC preprocessor stream");
            fline = (int)fl;
            tline = (int)tl;
            page = (int)pg;
            line = (int)ln;
            _ch_cpy = '\n';
            {
                unsigned int i;
                for (i = 0U; i <= n; ++i)
                    inpfname[i] = tokbuf[i];
            }
            continue;
        }

        if (tag <= 0 || tag >= NTOKDEFS)
            jerr("Corrupt token type %d in KCC preprocessor stream", tag);
        if (get16(in, &n) != 0)
            jerr("Corrupt token record in KCC preprocessor stream");
#if HOST_DAIMOS
        if (native_trace_count < 48) {
            fprintf(stderr, "KPIN: token=%d n=%u brk=%lo\n",
                tag, n, dsys_brk(0UL));
        }
#endif
        if (ensure_tokbuf(n) != 0 || getchars(in, tokbuf, n) != 0)
            jerr("Corrupt token record in KCC preprocessor stream");

        curpp = tag;
        curptr = NULL;
        curval.cp = n != 0U ? tokbuf : NULL;
        cursym = curpp == T_IDENT && curval.cp != NULL
            ? symfind(curval.cp, 1) : NULL;
        if (curpp == T_EOF) {
            stream_eof = 1;
            eof = 1;
        }
        return curpp;
    }
}

void
pushpp(void)
{
    if (pushed)
        jerr("KCC preprocessor stream pushback overflow");
    pushed = 1;
}
