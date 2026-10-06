/* CCKPWRITE.C - bounded KPCODE1 stream writer */

#include "cckpcode.h"

static int
putoctet(FILE *fp, unsigned int v)
{
    return fputc((int)(v & 0377U), fp) == EOF ? -1 : 0;
}

static int
putword36(FILE *fp, INT value)
{
    unsigned INT v;

    v = ((unsigned INT)value) & KPCODE_WORD_MASK;
    if (putoctet(fp, (unsigned int)(v >> 32)) != 0 ||
        putoctet(fp, (unsigned int)(v >> 24)) != 0 ||
        putoctet(fp, (unsigned int)(v >> 16)) != 0 ||
        putoctet(fp, (unsigned int)(v >> 8)) != 0 ||
        putoctet(fp, (unsigned int)v) != 0)
        return -1;
    return 0;
}

int
kpcode_write_header(FILE *fp, const struct kpcode_header *h)
{
    const char *p;

    for (p = KPCODE_MAGIC; *p != '\0'; ++p)
        if (putoctet(fp, (unsigned int)(unsigned char)*p) != 0)
            return -1;
    if (putword36(fp, (INT)KPCODE_VERSION) != 0 ||
        putword36(fp, h->target_cpu) != 0 ||
        putword36(fp, h->target_arch) != 0 ||
        putword36(fp, h->target_flags) != 0 ||
        putword36(fp, h->asm_dialect) != 0 ||
        putword36(fp, h->compile_flags) != 0)
        return -1;
    return 0;
}

int
kpcode_write_record(FILE *fp, unsigned int kind, const INT *words,
    unsigned int nwords)
{
    unsigned int i;

    if (kind == 0U)
        return -1;
    if (putword36(fp, (INT)kind) != 0 || putword36(fp, (INT)nwords) != 0)
        return -1;
    for (i = 0; i < nwords; ++i)
        if (putword36(fp, words[i]) != 0)
            return -1;
    return 0;
}

int
kpcode_write_symbol(FILE *fp, unsigned int id, const SYMBOL *s)
{
    INT words[8 + ((IDENTSIZE + 3) / 4)];
    unsigned int n, i, j;
    unsigned INT packed;

    if (id == 0U || s == NULL)
        return -1;
    n = 0;
    while (n < IDENTSIZE && s->Sname[n] != '\0')
        ++n;
    words[0] = (INT)id;
    words[1] = (INT)s->Sclass;
    words[2] = (INT)s->Sflags;
    words[3] = (INT)s->Sreg;
    words[4] = (INT)s->Srefs;
    words[5] = (INT)s->Sinit;
    words[6] = (INT)s->Sused;
    words[7] = (INT)n;
    for (i = 0; i < n; i += 4) {
        packed = 0;
        for (j = 0; j < 4 && i + j < n; ++j)
            packed |= (unsigned INT)(unsigned char)s->Sname[i+j]
                << (24 - 8*j);
        words[8 + i/4] = (INT)packed;
    }
    return kpcode_write_record(fp, KPCODE_REC_SYMBOL, words,
        8 + (n + 3)/4);
}

int
kpcode_write_pcode(FILE *fp, const PCODE *p, unsigned int symid)
{
    INT words[KPCODE_PCODE_WORDS];

    if (p == NULL || (p->Pptr != NULL && symid == 0U))
        return -1;
    words[KPCODE_PCODE_PTYPE] = (INT)p->Ptype;
    words[KPCODE_PCODE_POP] = (INT)p->Pop;
    words[KPCODE_PCODE_PREG] = (INT)p->Preg;
    words[KPCODE_PCODE_REG2] = (INT)p->p_reg2;
    words[KPCODE_PCODE_SYMID] = (INT)symid;
    words[KPCODE_PCODE_OFFSET] = p->p_off;
    words[KPCODE_PCODE_AUX0] = p->p_u.p_di[0];
    words[KPCODE_PCODE_AUX1] = p->p_u.p_di[1];
    return kpcode_write_record(fp, KPCODE_REC_PCODE, words,
        KPCODE_PCODE_WORDS);
}
