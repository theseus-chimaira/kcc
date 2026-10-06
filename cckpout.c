/* CCKPOUT.C - KGEN output shim writing the KPCODE1 stream. */

#include "cckpcode.h"
#include <stdlib.h>
#include <string.h>

extern int foldtrna(PCODE *);

struct kp_symmap {
    SYMBOL *sym;
    unsigned int id;
};

static struct kp_symmap *symmap;
static unsigned int nsymmap;
static unsigned int capsymmap;
static int kpseg;

int _word_cnt;

static int
write_words(unsigned int kind, const INT *words, unsigned int nwords)
{
    if (kpcode_write_record(out, kind, words, nwords) != 0) {
        jerr("Cannot write KPCODE1 record %u", kind);
        return -1;
    }
    return 0;
}

static int
write_string(unsigned int kind, const INT *prefix, unsigned int nprefix,
    const char *s, unsigned int len)
{
    INT *words;
    unsigned int i, j, nwords;
    unsigned INT packed;
    int rc;

    nwords = nprefix + 1U + (len + 3U) / 4U;
    if (nwords > KPCODE_MAX_RECORD_WORDS)
        return -1;
    words = (INT *)calloc(nwords != 0U ? nwords : 1U, sizeof(INT));
    if (words == NULL)
        return -1;
    for (i = 0; i < nprefix; ++i)
        words[i] = prefix[i];
    words[nprefix] = (INT)len;
    for (i = 0; i < len; i += 4U) {
        packed = 0;
        for (j = 0; j < 4U && i + j < len; ++j)
            packed |= (unsigned INT)(unsigned char)s[i+j] << (24 - 8*j);
        words[nprefix + 1U + i/4U] = (INT)packed;
    }
    rc = write_words(kind, words, nwords);
    free((char *)words);
    return rc;
}

static unsigned int
symid_raw(SYMBOL *s)
{
    struct kp_symmap *p;
    unsigned int i, cap;

    if (s == NULL)
        return 0;
    for (i = 0; i < nsymmap; ++i)
        if (symmap[i].sym == s)
            return symmap[i].id;
    if (nsymmap == capsymmap) {
        cap = capsymmap != 0U ? capsymmap * 2U : 64U;
        p = (struct kp_symmap *)realloc((char *)symmap,
            cap * sizeof(struct kp_symmap));
        if (p == NULL)
            efatal("Out of memory for KPCODE1 symbol map");
        symmap = p;
        capsymmap = cap;
    }
    symmap[nsymmap].sym = s;
    symmap[nsymmap].id = nsymmap + 1U;
    ++nsymmap;
    return nsymmap;
}

static unsigned int
emit_symbol(SYMBOL *s)
{
    INT link[2];
    unsigned int id, sid;

    if (s == NULL)
        return 0;
    id = symid_raw(s);
    if (s->Sclass == SC_ISTATIC && s->Ssym != NULL) {
        sid = emit_symbol(s->Ssym);
        link[0] = (INT)id;
        link[1] = (INT)sid;
        if (kpcode_write_symbol(out, id, s) != 0 ||
            write_words(KPCODE_REC_SYMBOL_LINK, link, 2) != 0)
            jerr("Cannot write KPCODE1 symbol");
    } else if (kpcode_write_symbol(out, id, s) != 0)
        jerr("Cannot write KPCODE1 symbol");
    return id;
}

static INT
pack_target_flags(void)
{
    INT f;

    f = (INT)((tgmachuse.mapdbl + 1) & KPCODE_TF_MAPDBL_MASK);
    if (tgmachuse.dmovx) f |= KPCODE_TF_DMOVX;
    if (tgmachuse.adjsp) f |= KPCODE_TF_ADJSP;
    if (tgmachuse.adjbp) f |= KPCODE_TF_ADJBP;
    if (tgmachuse.fltr) f |= KPCODE_TF_FLTR;
    if (tgmachuse.fpimm) f |= KPCODE_TF_FPIMM;
    if (tgits) f |= KPCODE_TF_ITS;
    return f;
}

void
outinit(void)
{
    struct kpcode_header h;
    INT prefix[1];
    INT cf;

    h.target_cpu = (INT)tgcpu;
    h.target_arch = (INT)tgarch;
    h.target_flags = pack_target_flags();
    h.asm_dialect = (INT)asmdialect;
    cf = 0;
    if (optobj) cf |= KPCODE_CF_OPTOBJ;
    if (delete) cf |= KPCODE_CF_DELETE;
    if (longidents) cf |= KPCODE_CF_LONGIDENTS;
    h.compile_flags = cf;
    if (kpcode_write_header(out, &h) != 0)
        jerr("Cannot write KPCODE1 header");
    prefix[0] = (INT)debcsi;
    if (write_string(KPCODE_REC_MODULE_BEGIN, prefix, 1, inpfname,
            (unsigned int)strlen(inpfname)) != 0)
        jerr("Cannot write KPCODE1 module header");
    kpseg = 1;
    _word_cnt = 0;
}

void
outdone(int mainf)
{
    SYMBOL *s;
    INT rec[1];
    unsigned int id;

    for (s = symbol != NULL ? symbol->Snext : NULL; s != NULL; s = s->Snext) {
        id = emit_symbol(s);
        if (kpcode_write_symbol(out, id, s) != 0)
            jerr("Cannot update KPCODE1 global symbol");
        rec[0] = (INT)id;
        if (write_words(KPCODE_REC_GLOBAL_SYMBOL, rec, 1) != 0)
            break;
    }
    rec[0] = (INT)mainf;
    write_words(KPCODE_REC_MODULE_END, rec, 1);
    free((char *)symmap);
    symmap = NULL;
    nsymmap = capsymmap = 0;
}

void
realcode(PCODE *p)
{
    unsigned int sid;

    if ((p->Pop & POF_OPCODE) == P_TRN && foldtrna(p))
        return;
    sid = emit_symbol(p->Pptr);
    if (kpcode_write_pcode(out, p, sid) != 0)
        jerr("Cannot write KPCODE1 pseudo instruction");
    ++_word_cnt;
}

void
outlab(SYMBOL *s)
{
    INT rec[1];
    rec[0] = (INT)emit_symbol(s);
    write_words(KPCODE_REC_OUTLAB, rec, 1);
}

void
outmidef(SYMBOL *s)
{
    INT rec[1];
    rec[0] = (INT)emit_symbol(s);
    write_words(KPCODE_REC_OUTMIDEF, rec, 1);
}

void
outmiref(SYMBOL *s)
{
    INT rec[1];
    rec[0] = (INT)emit_symbol(s);
    write_words(KPCODE_REC_OUTMIREF, rec, 1);
}

void
outptr(SYMBOL *s, int bsize, INT offset)
{
    INT rec[3];
    rec[0] = (INT)emit_symbol(s);
    rec[1] = (INT)bsize;
    rec[2] = offset;
    write_words(KPCODE_REC_OUTPTR, rec, 3);
}

void
outid(char *s)
{
    if (write_string(KPCODE_REC_OUTID, NULL, 0, s,
            (unsigned int)strlen(s)) != 0)
        jerr("Cannot write KPCODE1 identifier");
}

void
outscon(char *s, int len, int bsiz)
{
    INT prefix[2];
    prefix[0] = (INT)len;
    prefix[1] = (INT)bsiz;
    if (write_string(KPCODE_REC_OUTSCON, prefix, 2, s,
            (unsigned int)len) != 0)
        jerr("Cannot write KPCODE1 string constant");
}

void
outnum(INT n)
{
    INT rec[KPCODE_INT_CHUNKS];
    kpcode_pack_int(rec, n);
    write_words(KPCODE_REC_OUTNUM, rec, KPCODE_INT_CHUNKS);
}

void
outstr(char *s)
{
    if (write_string(KPCODE_REC_TEXT, NULL, 0, s,
            (unsigned int)strlen(s)) != 0)
        jerr("Cannot write KPCODE1 text");
}

void
outnstr(char *s, int len)
{
    if (write_string(KPCODE_REC_TEXT, NULL, 0, s, (unsigned int)len) != 0)
        jerr("Cannot write KPCODE1 text");
}

void
outnl(void)
{
    write_words(KPCODE_REC_OUTNL, NULL, 0);
}

void
outpghdr(void)
{
    write_words(KPCODE_REC_OUTPGHDR, NULL, 0);
}

int
outflt(int typ, INT *ptr, int flags)
{
    INT rec[4];
#ifdef __COMPILER_KCC__
    rec[2] = ptr[0];
    rec[3] = ptr[1];
#else
    unsigned char b[8];
    double d;
    unsigned INT a, c;
    memcpy((char *)&d, (char *)ptr, sizeof(double));
    memcpy((char *)b, (char *)&d, 8);
    a = ((unsigned INT)b[0] << 24) | ((unsigned INT)b[1] << 16) |
        ((unsigned INT)b[2] << 8) | (unsigned INT)b[3];
    c = ((unsigned INT)b[4] << 24) | ((unsigned INT)b[5] << 16) |
        ((unsigned INT)b[6] << 8) | (unsigned INT)b[7];
    rec[2] = (INT)a;
    rec[3] = (INT)c;
#endif
    rec[0] = (INT)typ;
    rec[1] = (INT)flags;
    write_words(KPCODE_REC_OUTFLT, rec, 4);
    return typ == TS_FLOAT ? 1 : 2;
}

static int
setseg(int op, int seg)
{
    INT rec[2];
    int old;

    old = kpseg;
    rec[0] = (INT)op;
    rec[1] = (INT)seg;
    write_words(KPCODE_REC_SEGMENT, rec, 2);
    kpseg = seg;
    return old;
}

int codeseg(void) { return setseg(KPCODE_SEG_CODE, 1); }
int dataseg(void) { return setseg(KPCODE_SEG_DATA, -1); }
int bssseg(void) { return setseg(KPCODE_SEG_BSS, -2); }

int
prevseg(int seg)
{
    INT rec[2];
    int old;
    old = kpseg;
    rec[0] = KPCODE_SEG_PREV;
    rec[1] = (INT)seg;
    write_words(KPCODE_REC_SEGMENT, rec, 2);
    kpseg = seg == -2 ? -2 : (seg < 0 ? -1 : 1);
    return old;
}
