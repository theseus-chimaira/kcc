/* CCKPREAD.C - bounded KPCODE1 stream reader */

#include "cckpcode.h"

static int
getoctet(FILE *fp)
{
    int c;

    c = getc(fp);
    return c == EOF ? -1 : (c & 0377);
}

static int
getword36(FILE *fp, INT *value)
{
    int a, b, c, d, e;
    unsigned INT v;

    a = getoctet(fp);
    b = getoctet(fp);
    c = getoctet(fp);
    d = getoctet(fp);
    e = getoctet(fp);
    if (a < 0 || b < 0 || c < 0 || d < 0 || e < 0 || (a & ~017) != 0)
        return -1;
    v = ((unsigned INT)a << 32) |
        ((unsigned INT)b << 24) |
        ((unsigned INT)c << 16) |
        ((unsigned INT)d << 8) | (unsigned INT)e;
    if (v & ((unsigned INT)1 << 35))
        v |= ~KPCODE_WORD_MASK;
    *value = (INT)v;
    return 0;
}

int
kpcode_read_header(FILE *fp, struct kpcode_header *h)
{
    const char *p;
    int c;
    INT version;

    for (p = KPCODE_MAGIC; *p != '\0'; ++p) {
        c = getoctet(fp);
        if (c < 0 || c != (unsigned char)*p)
            return -1;
    }
    if (getword36(fp, &version) != 0 || version != (INT)KPCODE_VERSION ||
        getword36(fp, &h->target_cpu) != 0 ||
        getword36(fp, &h->target_arch) != 0 ||
        getword36(fp, &h->target_flags) != 0 ||
        getword36(fp, &h->asm_dialect) != 0 ||
        getword36(fp, &h->compile_flags) != 0)
        return -1;
    return 0;
}

int
kpcode_read_record(FILE *fp, unsigned int *kind, INT *words,
    unsigned int cap, unsigned int *nwords)
{
    INT k, n;
    unsigned int i, count;

    if (getword36(fp, &k) != 0 || getword36(fp, &n) != 0 ||
        k <= 0 || n < 0)
        return -1;
    count = (unsigned int)n;
    if (count > cap)
        return -1;
    for (i = 0; i < count; ++i)
        if (getword36(fp, &words[i]) != 0)
            return -1;
    *kind = (unsigned int)k;
    *nwords = count;
    return 0;
}
