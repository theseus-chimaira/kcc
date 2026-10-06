/* CCPPIN.C - compact cooked-token stream reader for split native KCC */

#include "cc.h"
#include "cclex.h"
#include "ccphase.h"
#include <stdlib.h>

extern SYMBOL *symfind(char *, int);

static char *tokbuf;
static unsigned int tokcap;
static int pushed;

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
    eof = 0;
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

    for (;;) {
        tag = getbyte(in);
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
            jerr("Corrupt token type in KCC preprocessor stream");
        if (get16(in, &n) != 0 || ensure_tokbuf(n) != 0 ||
            getchars(in, tokbuf, n) != 0)
            jerr("Corrupt token record in KCC preprocessor stream");

        curpp = tag;
        curptr = NULL;
        curval.cp = n != 0U ? tokbuf : NULL;
        cursym = curpp == T_IDENT && curval.cp != NULL
            ? symfind(curval.cp, 1) : NULL;
        if (curpp == T_EOF)
            eof = 1;
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
