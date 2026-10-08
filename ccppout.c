/* CCPPOUT.C - compact cooked-token stream writer for split native KCC */

#include "cc.h"
#include "cclex.h"
#include "ccphase.h"
#include <string.h>

extern int nextpp(void);
static int
putbyte(FILE *fp, unsigned int v)
{
    return fputc((int)(v & 0377U), fp) == EOF ? -1 : 0;
}

static int
put16(FILE *fp, unsigned int v)
{
    return putbyte(fp, v >> 8) || putbyte(fp, v) ? -1 : 0;
}

static int
put18(FILE *fp, unsigned int v)
{
    return putbyte(fp, v >> 16) || putbyte(fp, v >> 8) || putbyte(fp, v)
        ? -1 : 0;
}

static int
putchars(FILE *fp, const char *s, unsigned int n)
{
    while (n-- != 0U)
        if (putbyte(fp, (unsigned int)(unsigned char)*s++) != 0)
            return -1;
    return 0;
}

static int
token_has_text(int tok)
{
    switch (tok) {
    case T_IDENT:
    case T_ICONST:
    case T_FCONST:
    case T_CCONST:
    case T_SCONST:
    case T_UNKNWN:
        return 1;
    default:
        return 0;
    }
}

static int
put_location(FILE *fp)
{
    unsigned int n;

    n = (unsigned int)strlen(inpfname);
    if (n > KCC_PHASE_MAX_PAYLOAD)
        return -1;
    if (putbyte(fp, KCC_PHASE_LOCATION) != 0 ||
        put18(fp, (unsigned int)fline) != 0 ||
        put18(fp, (unsigned int)tline) != 0 ||
        put18(fp, (unsigned int)page) != 0 ||
        put18(fp, (unsigned int)line) != 0 ||
        put16(fp, n) != 0)
        return -1;
    return putchars(fp, inpfname, n);
}

static int
put_token(FILE *fp, int tok)
{
    char *s;
    unsigned int n;

    s = NULL;
    if (token_has_text(tok))
        s = curval.cp;
    n = 0U;
    if (s != NULL)
        n = (unsigned int)strlen(s);
    if (tok <= 0 || tok >= NTOKDEFS || tok == KCC_PHASE_LOCATION ||
        n > KCC_PHASE_MAX_PAYLOAD)
        return -1;
    if (putbyte(fp, (unsigned int)tok) != 0 || put16(fp, n) != 0)
        return -1;
    return n != 0U ? putchars(fp, s, n) : 0;
}

int
ppstream_write(FILE *fp)
{
    char lastfile[FNAMESIZE];
    int lastfline;
    int lasttline;
    int lastpage;
    int lastline;
    int tok;

    if (putbyte(fp, KCC_PHASE_MAGIC_0) != 0 ||
        putbyte(fp, KCC_PHASE_MAGIC_1) != 0 ||
        putbyte(fp, KCC_PHASE_MAGIC_2) != 0 ||
        putbyte(fp, KCC_PHASE_MAGIC_3) != 0)
        return -1;

    lastfile[0] = 0;
    lastfline = -1;
    lasttline = -1;
    lastpage = -1;
    lastline = -1;
    for (;;) {
        tok = nextpp();
        if (fline != lastfline || tline != lasttline ||
            page != lastpage || line != lastline ||
            strcmp(lastfile, inpfname) != 0) {
            if (put_location(fp) != 0)
                return -1;
            strncpy(lastfile, inpfname, FNAMESIZE - 1);
            lastfile[FNAMESIZE - 1] = 0;
            lastfline = fline;
            lasttline = tline;
            lastpage = page;
            lastline = line;
        }
        if (put_token(fp, tok) != 0)
            return -1;
        if (tok == T_EOF)
            break;
    }
    return ferror(fp) ? -1 : 0;
}
