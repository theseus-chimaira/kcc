/* CCKPREAD.C - bounded KPCODE1 stream reader */

#include "cckpcode.h"
#include <string.h>

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

INT
kpcode_unpack_int(const INT *src)
{
    unsigned INT u;
    int i;

    u = 0;
    for (i = 3; i >= 0; --i)
        u = (u << 18) | ((unsigned INT)src[i] & 0777777U);
    return (INT)u;
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

int
kpcode_decode_symbol(const INT *words, unsigned int nwords, SYMBOL *s)
{
    unsigned int len, i, j;
    unsigned INT packed;

    if (words == NULL || s == NULL || nwords < 8U)
        return -1;
    len = (unsigned int)words[7];
    if (len >= IDENTSIZE || 8U + (len + 3U)/4U > nwords)
        return -1;
    memset((char *)s, 0, sizeof(*s));
    s->Sclass = (char)words[1];
    s->Sflags = words[2];
    s->Sreg = (char)words[3];
    s->Srefs = (int)words[4];
    s->Sinit = (char)words[5];
    s->Sused = (char)words[6];
    for (i = 0; i < len; i += 4U) {
        packed = (unsigned INT)words[8U + i/4U];
        for (j = 0; j < 4U && i + j < len; ++j)
            s->Sname[i+j] = (char)((packed >> (24 - 8*j)) & 0377U);
    }
    s->Sname[len] = '\0';
    return 0;
}

int
kpcode_decode_pcode(const INT *words, unsigned int nwords, PCODE *p,
    SYMBOL *sym)
{
#ifndef __COMPILER_KCC__
    int mode;
#endif

    if (words == NULL || p == NULL || nwords != KPCODE_PCODE_WORDS)
        return -1;
    memset((char *)p, 0, sizeof(*p));
    p->Ptype = (unsigned char)words[KPCODE_PCODE_PTYPE];
    p->Pop = (short)words[KPCODE_PCODE_POP];
    p->Preg = (unsigned char)words[KPCODE_PCODE_PREG];
    p->p_reg2 = (short)words[KPCODE_PCODE_REG2];
    p->Pptr = sym;
    p->p_off = kpcode_unpack_int(&words[KPCODE_PCODE_OFFSET0]);
#ifdef __COMPILER_KCC__
    p->p_u.p_di[0] = kpcode_unpack_int(&words[KPCODE_PCODE_AUX00]);
    p->p_u.p_di[1] = kpcode_unpack_int(&words[KPCODE_PCODE_AUX10]);
#else
    mode = p->Ptype & PTF_ADRMODE;
    if (mode == PTA_FCONST) {
        unsigned char b[4];
        unsigned INT v = (unsigned INT)kpcode_unpack_int(&words[KPCODE_PCODE_AUX00]);
        b[0] = (unsigned char)(v >> 24);
        b[1] = (unsigned char)(v >> 16);
        b[2] = (unsigned char)(v >> 8);
        b[3] = (unsigned char)v;
        memcpy((char *)&p->Pfloat, (char *)b, 4);
    } else if (mode == PTA_DCONST || mode == PTA_DCONST1 ||
               mode == PTA_DCONST2) {
        unsigned char b[8];
        unsigned INT a = (unsigned INT)kpcode_unpack_int(&words[KPCODE_PCODE_AUX00]);
        unsigned INT c = (unsigned INT)kpcode_unpack_int(&words[KPCODE_PCODE_AUX10]);
        b[0] = (unsigned char)(a >> 24);
        b[1] = (unsigned char)(a >> 16);
        b[2] = (unsigned char)(a >> 8);
        b[3] = (unsigned char)a;
        b[4] = (unsigned char)(c >> 24);
        b[5] = (unsigned char)(c >> 16);
        b[6] = (unsigned char)(c >> 8);
        b[7] = (unsigned char)c;
        memcpy((char *)&p->Pdouble, (char *)b, 8);
    } else {
        p->p_u.p_int = kpcode_unpack_int(&words[KPCODE_PCODE_AUX00]);
    }
#endif
    return 0;
}
