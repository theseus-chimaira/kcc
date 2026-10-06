/* CCKOPT.C - minimal KPCODE1 replay driver for the KOPT phase. */

#include "ccsite.h"
#include "cckpcode.h"
#include <stdlib.h>
#include <string.h>

extern void outinit(void), outdone(int), realcode(PCODE *);
extern void outlab(SYMBOL *), outmidef(SYMBOL *), outmiref(SYMBOL *);
extern void outptr(SYMBOL *, int, INT), outid(char *), outscon(char *, int, int);
extern void outnum(INT), outstr(char *), outnl(void), outpghdr(void);
extern int outflt(int, INT *, int), codeseg(void), dataseg(void), bssseg(void),
    prevseg(int);

char mainname[FNAMESIZE];

static SYMBOL **idtab;
static unsigned int idcap;
static SYMBOL rootsym;
static SYMBOL *globaltail;

/* Mixed-listing input does not exist in KOPT; this satisfies CCOUT's
 * historical optional page-header reference without pulling CCPP in. */
void dmpmlbuf(void) {}

static int
ensure_id(unsigned int id)
{
    SYMBOL **p;
    unsigned int cap, i;

    if (id == 0U)
        return 0;
    if (id < idcap && idtab[id] != NULL)
        return 0;
    if (id >= idcap) {
        cap = idcap != 0U ? idcap : 64U;
        while (cap <= id)
            cap *= 2U;
        p = (SYMBOL **)realloc((char *)idtab, cap * sizeof(SYMBOL *));
        if (p == NULL)
            return -1;
        for (i = idcap; i < cap; ++i)
            p[i] = NULL;
        idtab = p;
        idcap = cap;
    }
    if (idtab[id] == NULL) {
        idtab[id] = (SYMBOL *)calloc(1, sizeof(SYMBOL));
        if (idtab[id] == NULL)
            return -1;
    }
    return 0;
}

static SYMBOL *
getsym(unsigned int id)
{
    if (id == 0U)
        return NULL;
    if (ensure_id(id) != 0)
        return NULL;
    return idtab[id];
}

static int
apply_symbol(const INT *w, unsigned int n)
{
    SYMBOL tmp, *s;
    SYMBOL *next, *prev, *related;
    unsigned int id;

    if (n < 8U || w[0] <= 0)
        return -1;
    id = (unsigned int)w[0];
    s = getsym(id);
    if (s == NULL || kpcode_decode_symbol(w, n, &tmp) != 0)
        return -1;
    next = s->Snext;
    prev = s->Sprev;
    related = s->Ssym;
    *s = tmp;
    s->Snext = next;
    s->Sprev = prev;
    if (s->Sclass == SC_ISTATIC)
        s->Ssym = related;
    return 0;
}

static char *
unpack_string(const INT *w, unsigned int n, unsigned int prefix)
{
    char *s;
    unsigned int len, i, j;
    unsigned INT packed;

    if (prefix >= n || w[prefix] < 0)
        return NULL;
    len = (unsigned int)w[prefix];
    if (prefix + 1U + (len + 3U)/4U > n)
        return NULL;
    s = (char *)calloc(len + 1U, sizeof(char));
    if (s == NULL)
        return NULL;
    for (i = 0; i < len; i += 4U) {
        packed = (unsigned INT)w[prefix + 1U + i/4U];
        for (j = 0; j < 4U && i + j < len; ++j)
            s[i+j] = (char)((packed >> (24 - 8*j)) & 0377U);
    }
    s[len] = '\0';
    return s;
}

static void
set_header_state(const struct kpcode_header *h)
{
    INT f;

    tgcpu = (int)h->target_cpu;
    tgarch = (int)h->target_arch;
    asmdialect = (int)h->asm_dialect;
    f = h->target_flags;
    tgmachuse.mapdbl = (int)(f & KPCODE_TF_MAPDBL_MASK) - 1;
    tgmachuse.dmovx = (f & KPCODE_TF_DMOVX) != 0;
    tgmachuse.adjsp = (f & KPCODE_TF_ADJSP) != 0;
    tgmachuse.adjbp = (f & KPCODE_TF_ADJBP) != 0;
    tgmachuse.fltr = (f & KPCODE_TF_FLTR) != 0;
    tgmachuse.fpimm = (f & KPCODE_TF_FPIMM) != 0;
    tgits = (f & KPCODE_TF_ITS) != 0;
    optobj = (h->compile_flags & KPCODE_CF_OPTOBJ) != 0;
    delete = (h->compile_flags & KPCODE_CF_DELETE) != 0;
    longidents = (h->compile_flags & KPCODE_CF_LONGIDENTS) != 0;
}

static int
replay(FILE *fp)
{
    struct kpcode_header h;
    INT w[KPCODE_MAX_RECORD_WORDS];
    unsigned int kind, n;
    char *s;
    SYMBOL *sym;
    PCODE p;

    if (kpcode_read_header(fp, &h) != 0)
        return -1;
    set_header_state(&h);
    memset((char *)&rootsym, 0, sizeof(rootsym));
    symbol = &rootsym;
    globaltail = &rootsym;

    while (kpcode_read_record(fp, &kind, w, KPCODE_MAX_RECORD_WORDS, &n) == 0) {
        switch (kind) {
        case KPCODE_REC_MODULE_BEGIN:
            if (n < 2U) return -1;
            debcsi = (int)w[0];
            s = unpack_string(w, n, 1);
            if (s == NULL || strlen(s) >= FNAMESIZE) return -1;
            strcpy(inpfname, s);
            free(s);
            outinit();
            break;
        case KPCODE_REC_SYMBOL:
            if (apply_symbol(w, n) != 0) return -1;
            break;
        case KPCODE_REC_SYMBOL_LINK:
            if (n != 2U || w[0] <= 0 || w[1] <= 0) return -1;
            sym = getsym((unsigned int)w[0]);
            if (sym == NULL) return -1;
            sym->Ssym = getsym((unsigned int)w[1]);
            if (sym->Ssym == NULL) return -1;
            break;
        case KPCODE_REC_GLOBAL_SYMBOL:
            if (n != 1U || w[0] <= 0) return -1;
            sym = getsym((unsigned int)w[0]);
            if (sym == NULL) return -1;
            if (sym->Sprev == NULL && sym != symbol) {
                sym->Sprev = globaltail;
                globaltail->Snext = sym;
                globaltail = sym;
            }
            break;
        case KPCODE_REC_PCODE:
            if (n != KPCODE_PCODE_WORDS) return -1;
            sym = w[KPCODE_PCODE_SYMID] != 0
                ? getsym((unsigned int)w[KPCODE_PCODE_SYMID]) : NULL;
            if (w[KPCODE_PCODE_SYMID] != 0 && sym == NULL) return -1;
            if (kpcode_decode_pcode(w, n, &p, sym) != 0) return -1;
            realcode(&p);
            break;
        case KPCODE_REC_OUTLAB:
            if (n != 1U || (sym = getsym((unsigned int)w[0])) == NULL) return -1;
            outlab(sym);
            break;
        case KPCODE_REC_OUTMIDEF:
            if (n != 1U || (sym = getsym((unsigned int)w[0])) == NULL) return -1;
            outmidef(sym);
            break;
        case KPCODE_REC_OUTMIREF:
            if (n != 1U || (sym = getsym((unsigned int)w[0])) == NULL) return -1;
            outmiref(sym);
            break;
        case KPCODE_REC_OUTPTR:
            if (n != 3U) return -1;
            sym = w[0] != 0 ? getsym((unsigned int)w[0]) : NULL;
            if (w[0] != 0 && sym == NULL) return -1;
            outptr(sym, (int)w[1], w[2]);
            break;
        case KPCODE_REC_OUTID:
        case KPCODE_REC_TEXT:
            s = unpack_string(w, n, 0);
            if (s == NULL) return -1;
            if (kind == KPCODE_REC_OUTID) outid(s); else outstr(s);
            free(s);
            break;
        case KPCODE_REC_OUTSCON:
            if (n < 3U) return -1;
            s = unpack_string(w, n, 2);
            if (s == NULL) return -1;
            outscon(s, (int)w[0], (int)w[1]);
            free(s);
            break;
        case KPCODE_REC_OUTNUM:
            if (n != KPCODE_INT_CHUNKS) return -1;
            outnum(kpcode_unpack_int(w));
            break;
        case KPCODE_REC_OUTFLT:
            if (n != 4U) return -1;
#ifdef __COMPILER_KCC__
            {
                INT fw[2]; fw[0] = w[2]; fw[1] = w[3];
                outflt((int)w[0], fw, (int)w[1]);
            }
#else
            {
                unsigned char b[8];
                unsigned INT a = (unsigned INT)w[2], c = (unsigned INT)w[3];
                double d;
                b[0]=(unsigned char)(a>>24); b[1]=(unsigned char)(a>>16);
                b[2]=(unsigned char)(a>>8); b[3]=(unsigned char)a;
                b[4]=(unsigned char)(c>>24); b[5]=(unsigned char)(c>>16);
                b[6]=(unsigned char)(c>>8); b[7]=(unsigned char)c;
                memcpy((char *)&d, (char *)b, 8);
                outflt((int)w[0], (INT *)&d, (int)w[1]);
            }
#endif
            break;
        case KPCODE_REC_OUTNL: outnl(); break;
        case KPCODE_REC_OUTPGHDR: outpghdr(); break;
        case KPCODE_REC_SEGMENT:
            if (n != 2U) return -1;
            switch ((int)w[0]) {
            case KPCODE_SEG_CODE: codeseg(); break;
            case KPCODE_SEG_DATA: dataseg(); break;
            case KPCODE_SEG_BSS: bssseg(); break;
            case KPCODE_SEG_PREV: prevseg((int)w[1]); break;
            default: return -1;
            }
            break;
        case KPCODE_REC_MODULE_END:
            if (n != 1U) return -1;
            outdone((int)w[0]);
            return 0;
        default:
            return -1;
        }
    }
    return -1;
}

int
main(int argc, char **argv)
{
    FILE *infile;
    char *inname = NULL, *outname = NULL;
    int i, rc;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc)
            outname = argv[++i];
        else if (inname == NULL)
            inname = argv[i];
        else {
            fprintf(stderr, "usage: kopt input.kp1 -o output.s\n");
            return 2;
        }
    }
    if (inname == NULL || outname == NULL) {
        fprintf(stderr, "usage: kopt input.kp1 -o output.s\n");
        return 2;
    }
    infile = fopen(inname, "rb");
    if (infile == NULL) {
        fprintf(stderr, "kopt: cannot open %s\n", inname);
        return 1;
    }
    out = fopen(outname, "w");
    if (out == NULL) {
        fclose(infile);
        fprintf(stderr, "kopt: cannot create %s\n", outname);
        return 1;
    }
    outmsgs = stderr;
    rc = replay(infile);
    fclose(infile);
    fclose(out);
    return rc == 0 ? 0 : 1;
}
