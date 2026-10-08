/* CCKIRWRITE.C - KIR1 typed graph writer. */

#include "cckir.h"
#include "ccvla.h"
#include <stdlib.h>
#include <string.h>

struct kir_wgraph {
    TYPE **types; unsigned nt, ct;
    SYMBOL **syms; unsigned *symids; unsigned ns, cs, nlocal;
    NODE **nodes; unsigned nn, cn;
};

struct kir_wmodule {
    SYMBOL **syms;
    INT *lastrefs;
    unsigned ns, cs;
};

static struct kir_wmodule module_syms;
static int parser_labmax;

extern int labmax(void);

static int put36(FILE *, INT);
static int writerec(FILE *, unsigned, const INT *, unsigned);
static void packint(INT *, INT);
static unsigned addtype(struct kir_wgraph *, TYPE *);
static unsigned addsym(struct kir_wgraph *, SYMBOL *);
static unsigned addnode(struct kir_wgraph *, NODE *);
static unsigned findptr(void **, unsigned, void *);
static int grow(void ***, unsigned *, unsigned);
static void freegraph(struct kir_wgraph *);
static void freemodule(void);
static unsigned globalsymid(SYMBOL *);
static int persistent_symbol(SYMBOL *);

static int
put36(FILE *fp, INT v)
{
    unsigned INT u = (unsigned INT)v & (unsigned INT)0777777777777;
    if (putc((int)((u >> 32) & 017), fp) == EOF) return -1;
    if (putc((int)((u >> 24) & 0377), fp) == EOF) return -1;
    if (putc((int)((u >> 16) & 0377), fp) == EOF) return -1;
    if (putc((int)((u >> 8) & 0377), fp) == EOF) return -1;
    return putc((int)(u & 0377), fp) == EOF ? -1 : 0;
}

static int
writerec(FILE *fp, unsigned kind, const INT *w, unsigned n)
{
    unsigned i;
    if (n > KIR_MAX_RECORD_WORDS) return -1;
    if (put36(fp, (INT)kind) || put36(fp, (INT)n)) return -1;
    for (i = 0; i < n; ++i) if (put36(fp, w[i])) return -1;
    return 0;
}

static void
packint(INT *d, INT v)
{
    unsigned INT u = (unsigned INT)v;
    int i;
    for (i = 0; i < KIR_INT_CHUNKS; ++i) {
        d[i] = (INT)(u & 0777777U);
        u >>= 18;
    }
}

static unsigned
findptr(void **v, unsigned n, void *p)
{
    unsigned i;
    for (i = 0; i < n; ++i) if (v[i] == p) return i + 1;
    return 0;
}

static int
grow(void ***vp, unsigned *cap, unsigned need)
{
    void **nv;
    unsigned nc;
    if (need <= *cap) return 0;
    nc = *cap ? *cap * 2 : 64;
    while (nc < need) nc *= 2;
    nv = (void **)realloc(*vp, nc * sizeof(void *));
    if (nv == NULL) return -1;
    *vp = nv; *cap = nc;
    return 0;
}

static int
persistent_symbol(SYMBOL *s)
{
    return s != NULL && s->Sclass != SC_ILABEL && (s->Sflags & SF_LOCAL) == 0;
}

static unsigned
globalsymid(SYMBOL *s)
{
    unsigned id;
    SYMBOL **nv;
    INT *nr;
    unsigned nc;

    if (s == NULL)
        return 0;
    if ((id = findptr((void **)module_syms.syms, module_syms.ns, s)) != 0)
        return id;
    if (module_syms.ns == module_syms.cs) {
        nc = module_syms.cs ? module_syms.cs * 2U : 64U;
        nv = (SYMBOL **)realloc(module_syms.syms, nc * sizeof(SYMBOL *));
        if (nv == NULL)
            return 0;
        nr = (INT *)realloc(module_syms.lastrefs, nc * sizeof(INT));
        if (nr == NULL) {
            module_syms.syms = nv;
            return 0;
        }
        module_syms.syms = nv;
        module_syms.lastrefs = nr;
        module_syms.cs = nc;
    }
    module_syms.syms[module_syms.ns] = s;
    module_syms.lastrefs[module_syms.ns] = 0;
    ++module_syms.ns;
    return module_syms.ns;
}

static unsigned
addtype(struct kir_wgraph *g, TYPE *t)
{
    unsigned id;
    NODE *bound;
    SYMBOL *bs;
    if (t == NULL) return 0;
    if ((id = findptr((void **)g->types, g->nt, t)) != 0) return id;
    if (grow((void ***)&g->types, &g->ct, g->nt + 1)) return 0;
    g->types[g->nt++] = t; id = g->nt;
    if (t->Tspec == TS_FUNCT || t->Tspec == TS_PARAM
      || t->Tspec == TS_PARVOID || t->Tspec == TS_PARINF)
        (void)addtype(g, t->t_v0.t_subt);
    if (t->Tspec == TS_STRUCT || t->Tspec == TS_UNION || t->Tspec == TS_ENUM)
        (void)addsym(g, t->Tsmtag);
    else
        (void)addtype(g, t->Tsubt);
    if (tisvla(t)) {
        bound = vlaboundexpr_v11(t);
        bs = vlaboundsym_v11(t);
        if (bound != NULL) (void)addnode(g, bound);
        if (bs != NULL) (void)addsym(g, bs);
    }
    return id;
}

static unsigned
addsym(struct kir_wgraph *g, SYMBOL *s)
{
    unsigned id, i;
    SYMBOL *base, *mark;
    if (s == NULL) return 0;
    for (i = 0; i < g->ns; ++i)
        if (g->syms[i] == s)
            return g->symids[i];
    if (g->ns == g->cs) {
        unsigned nc = g->cs ? g->cs * 2U : 64U;
        SYMBOL **sv = (SYMBOL **)realloc(g->syms, nc * sizeof(SYMBOL *));
        unsigned *iv = (unsigned *)realloc(g->symids, nc * sizeof(unsigned));
        if (sv == NULL || iv == NULL) {
            if (sv != NULL) g->syms = sv;
            if (iv != NULL) g->symids = iv;
            return 0;
        }
        g->syms = sv;
        g->symids = iv;
        g->cs = nc;
    }
    if (persistent_symbol(s))
        id = globalsymid(s);
    else
        id = (unsigned)KIR_LOCAL_ID_FLAG | ++g->nlocal;
    if (id == 0U) return 0;
    g->syms[g->ns] = s;
    g->symids[g->ns] = id;
    ++g->ns;
    (void)addtype(g, s->Stype);
    (void)addsym(g, s->Ssmnext);
    if (s->Sclass == SC_ISTATIC) (void)addsym(g, s->Ssym);
    if (s->Sclass == SC_MEMBER || s->Sclass == SC_ENUM)
        (void)addsym(g, s->Ssmtag);
    if ((s->Sclass == SC_EXTDEF || s->Sclass == SC_INTDEF)
      && s->Stype != NULL && s->Stype->Tspec == TS_FUNCT)
        (void)addtype(g, s->Shproto);
    base = vlabase_v11(s);
    mark = vlaobjmarkget_v12(s);
    if (base != NULL) (void)addsym(g, base);
    if (mark != NULL) (void)addsym(g, mark);
    return id;
}

static void
addnormal(struct kir_wgraph *g, NODE *n)
{
    (void)addnode(g, n->Nleft);
    (void)addnode(g, n->Nright);
}

static unsigned
addnode(struct kir_wgraph *g, NODE *n)
{
    unsigned id;
    if (n == NULL) return 0;
    if ((id = findptr((void **)g->nodes, g->nn, n)) != 0) return id;
    if (grow((void ***)&g->nodes, &g->cn, g->nn + 1)) return 0;
    g->nodes[g->nn++] = n; id = g->nn;
    (void)addtype(g, n->Ntype);
    switch (n->Nop) {
    case Q_IDENT: case N_VLA: case N_VLARST:
        (void)addsym(g, n->Nid); break;
    case N_ACONST: case T_JFFO: case N_LABEL:
        (void)addsym(g, n->Nxfsym); addnormal(g, n); break;
    case Q_GOTO:
        (void)addsym(g, n->Nxfsym); addnormal(g, n); break;
    case Q_SWITCH:
        (void)addnode(g, n->Nxswlist); addnormal(g, n); break;
    case Q_CASE: case Q_DEFAULT: case Q_DOT: case Q_MEMBER: case N_CAST:
        addnormal(g, n); break;
    case N_FNCALL:
        (void)addsym(g, n->Nretstruct); addnormal(g, n); break;
    case N_IZLIST:
        (void)addsym(g, n->Nizmem); addnormal(g, n); break;
    case N_ICONST: case N_PCONST: case N_ECONST:
    case N_FCONST: case N_SCONST: case N_VCONST:
        break;
    default:
        addnormal(g, n); break;
    }
    return id;
}

static unsigned
typeid(struct kir_wgraph *g, TYPE *p) { return findptr((void **)g->types, g->nt, p); }
static unsigned
symid(struct kir_wgraph *g, SYMBOL *p)
{
    unsigned i;
    if (p == NULL) return 0;
    for (i = 0; i < g->ns; ++i)
        if (g->syms[i] == p)
            return g->symids[i];
    return 0;
}
static unsigned
nodeid(struct kir_wgraph *g, NODE *p) { return findptr((void **)g->nodes, g->nn, p); }

static int
writetype(FILE *fp, struct kir_wgraph *g, unsigned id, TYPE *t)
{
    INT w[20]; unsigned n = 0;
    w[n++] = id; w[n++] = t->Tspec; w[n++] = t->Tbytes;
    packint(&w[n], t->Tflag); n += KIR_INT_CHUNKS;
    if (t->Tspec == TS_FUNCT || t->Tspec == TS_PARAM
      || t->Tspec == TS_PARVOID || t->Tspec == TS_PARINF) {
        w[n++] = 1; w[n++] = typeid(g, t->t_v0.t_subt);
        w[n++] = 0; w[n++] = 0; w[n++] = 0;
    } else {
        w[n++] = 0; packint(&w[n], (INT)t->Tsize); n += KIR_INT_CHUNKS;
    }
    if (t->Tspec == TS_STRUCT || t->Tspec == TS_UNION || t->Tspec == TS_ENUM) {
        w[n++] = 2; w[n++] = symid(g, t->Tsmtag);
    } else {
        w[n++] = 1; w[n++] = typeid(g, t->Tsubt);
    }
    return writerec(fp, KIR_REC_TYPE, w, n);
}

static int
writesym(FILE *fp, struct kir_wgraph *g, unsigned id, SYMBOL *s)
{
    INT w[68]; unsigned n = 0, i, gid;
    INT refs;
    w[n++] = id; w[n++] = s->Sreg; w[n++] = s->Sclass;
    packint(&w[n], s->Sflags); n += KIR_INT_CHUNKS;
    for (i = 0; i < IDENTSIZE; ++i) w[n++] = (unsigned char)s->Sname[i];
    if (s->Sclass == SC_ISTATIC) {
        w[n++] = 1;
        w[n++] = symid(g, s->Ssym); w[n++] = 0; w[n++] = 0; w[n++] = 0;
    } else {
        w[n++] = 0; packint(&w[n], s->Svalue); n += KIR_INT_CHUNKS;
    }
    w[n++] = typeid(g, s->Stype);
    w[n++] = symid(g, s->Ssmnext);
    if (s->Sclass == SC_MEMBER || s->Sclass == SC_ENUM) {
        w[n++] = 1; w[n++] = symid(g, s->Ssmtag);
    } else if ((s->Sclass == SC_EXTDEF || s->Sclass == SC_INTDEF)
      && s->Stype != NULL && s->Stype->Tspec == TS_FUNCT) {
        w[n++] = 2; w[n++] = typeid(g, s->Shproto);
    } else { w[n++] = 0; w[n++] = 0; }
    if (((unsigned INT)id & KIR_LOCAL_ID_FLAG) == 0) {
        gid = id;
        refs = (INT)s->Srefs - module_syms.lastrefs[gid - 1U];
        module_syms.lastrefs[gid - 1U] = (INT)s->Srefs;
    } else refs = (INT)s->Srefs;
    packint(&w[n], refs); n += KIR_INT_CHUNKS;
    w[n++] = s->Sinit; w[n++] = s->Sused;
    return writerec(fp, KIR_REC_SYMBOL, w, n);
}

static int
writenode(FILE *fp, struct kir_wgraph *g, unsigned id, NODE *p)
{
    INT w[40]; unsigned n = 0; int mode = 0; INT v0 = 0;
    union { double d; unsigned char b[sizeof(double)]; } du;
    unsigned i;
    w[n++] = id; w[n++] = typeid(g, p->Ntype);
    packint(&w[n], (INT)p->Nflag); n += KIR_INT_CHUNKS;
    w[n++] = p->Nop; w[n++] = p->Nreg; w[n++] = p->sfline;
    w[n++] = nodeid(g, p->Nleft); w[n++] = nodeid(g, p->Nright);
    switch (p->Nop) {
    case Q_IDENT: case N_VLA: case N_VLARST: mode = 2; v0 = symid(g, p->Nid); break;
    case N_ACONST: case T_JFFO: case N_LABEL: case Q_GOTO:
        mode = 2; v0 = symid(g, p->Nxfsym); break;
    case Q_SWITCH: mode = 3; v0 = nodeid(g, p->Nxswlist); break;
    case Q_CASE: mode = 1; v0 = p->Nxfint; break;
    case Q_DOT: case Q_MEMBER: mode = 1; v0 = p->Nxoff; break;
    case N_CAST: mode = 1; v0 = p->Ncast; break;
    case N_FNCALL: mode = 2; v0 = symid(g, p->Nretstruct); break;
    case N_IZLIST: mode = 2; v0 = symid(g, p->Nizmem); break;
    default:
        if (p->Nop >= Q_ASGN && p->Nop <= Q_ASOR) { mode = 1; v0 = p->Nascast; }
        break;
    }
    w[n++] = mode;
    if (mode == 1) { packint(&w[n], v0); n += KIR_INT_CHUNKS; }
    else { w[n++] = v0; w[n++] = 0; w[n++] = 0; w[n++] = 0; }
    if (p->Nop == N_ICONST || p->Nop == N_PCONST || p->Nop == N_ECONST) {
        w[n++] = 0;
        packint(&w[n], p->Niconst); n += KIR_INT_CHUNKS;
        packint(&w[n], (p->Nflag & NF_WIDE) ? p->n_var1.n_int : 0); n += KIR_INT_CHUNKS;
    } else if (p->Nop == N_FCONST) {
        du.d = p->Nfconst; w[n++] = sizeof(double);
        for (i = 0; i < sizeof(double); ++i) w[n++] = du.b[i];
    } else if (p->Nop == N_SCONST) {
        w[n++] = p->Nsclen;
    } else w[n++] = 0;
    return writerec(fp, KIR_REC_NODE, w, n);
}

static int
writestring(FILE *fp, unsigned id, NODE *p)
{
    unsigned i;
    if (p->Nop == N_SCONST && p->Nsclen > 0) {
        unsigned off = 0;
        while (off < (unsigned)p->Nsclen) {
            INT sw[KIR_MAX_RECORD_WORDS]; unsigned cnt = (unsigned)p->Nsclen - off;
            if (cnt > KIR_MAX_RECORD_WORDS - 2) cnt = KIR_MAX_RECORD_WORDS - 2;
            sw[0] = id; sw[1] = off;
            for (i = 0; i < cnt; ++i) sw[i+2] = (unsigned INT)(unsigned char)p->Nsconst[off+i];
            if (writerec(fp, KIR_REC_STRING, sw, cnt + 2)) return -1;
            off += cnt;
        }
    }
    return 0;
}

static int
writevlatype(FILE *fp, struct kir_wgraph *g, TYPE *t)
{
    INT w[4];
    NODE *bound;
    SYMBOL *bs;

    if (!tisvla(t)) return 0;
    bound = vlaboundexpr_v11(t);
    bs = vlaboundsym_v11(t);
    w[0] = (INT)typeid(g, t);
    w[1] = (INT)nodeid(g, bound);
    w[2] = (INT)symid(g, bs);
    w[3] = (INT)vlaboundcaptured_v12(t);
    return writerec(fp, KIR_REC_VLA_TYPE, w, 4);
}

static int
writevlaobject(FILE *fp, struct kir_wgraph *g, SYMBOL *s)
{
    SYMBOL *base, *mark;
    INT w[3];

    base = vlabase_v11(s);
    mark = vlaobjmarkget_v12(s);
    if (base == NULL && mark == NULL) return 0;
    w[0] = (INT)symid(g, s);
    w[1] = (INT)symid(g, base);
    w[2] = (INT)symid(g, mark);
    return writerec(fp, KIR_REC_VLA_OBJECT, w, 3);
}

int
kir_write_header(FILE *fp)
{
    const char *p;
    freemodule();
    parser_labmax = labmax();
    for (p = KIR_MAGIC; *p != '\0'; ++p)
        if (putc((unsigned char)*p, fp) == EOF) return -1;
    return put36(fp, KIR_VERSION);
}

static int
writegraph(FILE *fp, NODE *root)
{
    struct kir_wgraph g; INT h[KIR_EXT_WORDS]; unsigned i;
    memset(&g, 0, sizeof(g));
    if (root != NULL && addnode(&g, root) == 0) { freegraph(&g); return -1; }
    if (curfn != NULL && addsym(&g, curfn) == 0) { freegraph(&g); return -1; }
    for (i = 0; i < (unsigned)_reg_count; ++i)
        if (Reg_Id[i] != NULL && addsym(&g, Reg_Id[i]) == 0) {
            freegraph(&g); return -1;
        }
    memset(h, 0, sizeof(h));
    h[KIR_EXT_ROOT] = nodeid(&g, root);
    h[KIR_EXT_NTYPE] = g.nt;
    h[KIR_EXT_NSYM] = g.ns;
    h[KIR_EXT_NNODE] = g.nn;
    h[KIR_EXT_CURFN] = (INT)symid(&g, curfn);
    packint(&h[KIR_EXT_MAXAUTO0], maxauto);
    h[KIR_EXT_ABIDIRECT] = fnabidirect;
    h[KIR_EXT_ARGKEEP] = fnargkeepmask;
    h[KIR_EXT_ARGDROP] = fnargdropmask;
    h[KIR_EXT_ARGPREDROP] = fnargpredropmask;
    h[KIR_EXT_FNMAIN] = fn_main;
    h[KIR_EXT_REGCOUNT] = _reg_count;
    h[KIR_EXT_STACKREFS] = stackrefs;
    h[KIR_EXT_STKGOTO] = stkgoto;
    h[KIR_EXT_PARSELAB_START] = parser_labmax;
    h[KIR_EXT_PARSELAB_END] = labmax();
    parser_labmax = (int)h[KIR_EXT_PARSELAB_END];
    for (i = 0; i < KIR_EXT_REGIDS; ++i)
        h[KIR_EXT_REGID0 + i] = (INT)symid(&g, Reg_Id[i]);
    if (writerec(fp, KIR_REC_EXTDEF, h, KIR_EXT_WORDS)) { freegraph(&g); return -1; }
    for (i = 0; i < g.nt; ++i) if (writetype(fp, &g, i+1, g.types[i])) goto bad;
    for (i = 0; i < g.ns; ++i)
        if (writesym(fp, &g, g.symids[i], g.syms[i])) goto bad;
    for (i = 0; i < g.nn; ++i) if (writenode(fp, &g, i+1, g.nodes[i])) goto bad;
    for (i = 0; i < g.nn; ++i) if (writestring(fp, i+1, g.nodes[i])) goto bad;
    for (i = 0; i < g.nt; ++i) if (writevlatype(fp, &g, g.types[i])) goto bad;
    for (i = 0; i < g.ns; ++i) if (writevlaobject(fp, &g, g.syms[i])) goto bad;
    if (writerec(fp, KIR_REC_EXTEND, NULL, 0)) goto bad;
    freegraph(&g); return 0;
bad:
    freegraph(&g); return -1;
}

int kir_write_extdef(FILE *fp, NODE *n) { return writegraph(fp, n); }
int kir_write_tentative(FILE *fp, NODE *n) { return writegraph(fp, n); }

int
kir_write_globals(FILE *fp, SYMBOL *head)
{
    SYMBOL *s;
    INT w[12 + IDENTSIZE];
    unsigned id, n, i;
    INT delta;

    for (s = head != NULL ? head->Snext : NULL; s != NULL; s = s->Snext) {
        /* Reserved words are lexer/parser state, not module symbols.  Split
         * KGEN consumes an already parsed graph and must not spend heap or
         * KIR bandwidth reconstructing keyword entries that cannot be
         * referenced by generated code. */
        if (s->Sclass == SC_RW)
            continue;
        id = globalsymid(s);
        if (id == 0U)
            return -1;
        n = 0;
        w[n++] = (INT)id;
        w[n++] = (INT)s->Sclass;
        packint(&w[n], s->Sflags); n += KIR_INT_CHUNKS;
        delta = (INT)s->Srefs - module_syms.lastrefs[id - 1U];
        module_syms.lastrefs[id - 1U] = (INT)s->Srefs;
        packint(&w[n], delta); n += KIR_INT_CHUNKS;
        w[n++] = (INT)s->Sinit;
        w[n++] = (INT)s->Sused;
        for (i = 0; i < IDENTSIZE; ++i)
            w[n++] = (INT)(unsigned char)s->Sname[i];
        if (writerec(fp, KIR_REC_GLOBAL, w, n) != 0)
            return -1;
    }
    return 0;
}

int
kir_write_module_end(FILE *fp, int mainf)
{
    INT w[1]; int rc;
    w[0] = mainf;
    rc = writerec(fp, KIR_REC_MODULE_END, w, 1);
    freemodule();
    return rc;
}

static void
freegraph(struct kir_wgraph *g)
{
    free(g->types); free(g->syms); free(g->symids); free(g->nodes);
    memset(g, 0, sizeof(*g));
}

static void
freemodule(void)
{
    free(module_syms.syms);
    free(module_syms.lastrefs);
    memset(&module_syms, 0, sizeof(module_syms));
}
