/* CCKIRREAD.C - KIR1 typed graph reader. */

#include "cckir.h"
#include <stdlib.h>
#include <string.h>

struct tref { int v0kind; INT v0[KIR_INT_CHUNKS]; int v1kind; unsigned v1; };
struct sref {
    SYMBOL *sym;
    unsigned id;
    int valkind;
    INT val[KIR_INT_CHUNKS];
    unsigned type, next;
    int tagkind;
    unsigned tag;
};
struct nref { unsigned type, left, right; int mode; INT v0[KIR_INT_CHUNKS]; };
static TYPE **last_types; static unsigned last_nt;
static SYMBOL **module_syms; static unsigned module_ns, module_cs;
static SYMBOL **local_syms; static unsigned local_ns;
static NODE **last_nodes; static unsigned last_nn;
static int last_mainf;
static SYMBOL root_symbol;

static int get36(FILE *, INT *);
static int readrec(FILE *, unsigned *, INT *, unsigned, unsigned *);
static INT unpackint(const INT *);
static void clear_graph(void);
static void clear_module(void);
static int nodelinks(int);
static SYMBOL *getsymid(unsigned);
static SYMBOL *ensuresym(unsigned);

static int
nodelinks(int op)
{
    switch (op) {
    case Q_IDENT: case N_VLA: case N_VLARST:
    case N_ICONST: case N_PCONST: case N_ECONST:
    case N_FCONST: case N_SCONST: case N_VCONST:
        return 0;
    default:
        return 1;
    }
}

static int
get36(FILE *fp, INT *v)
{
    int a,b,c,d,e; unsigned INT u;
    a=getc(fp); b=getc(fp); c=getc(fp); d=getc(fp); e=getc(fp);
    if (a==EOF||b==EOF||c==EOF||d==EOF||e==EOF) return -1;
    u=((unsigned INT)(a&017)<<32)|((unsigned INT)(b&0377)<<24)
      |((unsigned INT)(c&0377)<<16)|((unsigned INT)(d&0377)<<8)|(unsigned INT)(e&0377);
    *v=(INT)u; return 0;
}

static int
readrec(FILE *fp, unsigned *kind, INT *w, unsigned cap, unsigned *np)
{
    INT k,n; unsigned i;
    if (get36(fp,&k)||get36(fp,&n)||k<=0||n<0||(unsigned INT)n>cap) return -1;
    *kind=(unsigned)k; *np=(unsigned)n;
    for(i=0;i<*np;++i) if(get36(fp,&w[i])) return -1;
    return 0;
}

static INT
unpackint(const INT *s)
{
    unsigned INT u=0; int i;
    for(i=KIR_INT_CHUNKS-1;i>=0;--i) u=(u<<18)|((unsigned INT)s[i]&0777777U);
    return (INT)u;
}

static SYMBOL *
getsymid(unsigned id)
{
    unsigned idx;
    if (id == 0U)
        return NULL;
    if (((unsigned INT)id & KIR_LOCAL_ID_FLAG) != 0) {
        idx = (unsigned)((unsigned INT)id & KIR_LOCAL_ID_MASK);
        return idx <= local_ns ? local_syms[idx] : NULL;
    }
    return id <= module_ns ? module_syms[id] : NULL;
}

static SYMBOL *
ensuresym(unsigned id)
{
    unsigned idx, nc;
    SYMBOL **nv;
    SYMBOL *s;

    if (id == 0U)
        return NULL;
    if (((unsigned INT)id & KIR_LOCAL_ID_FLAG) != 0) {
        idx = (unsigned)((unsigned INT)id & KIR_LOCAL_ID_MASK);
        if (idx == 0U || idx > local_ns)
            return NULL;
        if (local_syms[idx] == NULL)
            local_syms[idx] = (SYMBOL *)calloc(1, sizeof(SYMBOL));
        return local_syms[idx];
    }
    if (id > module_cs) {
        nc = module_cs ? module_cs : 64U;
        while (nc < id) nc *= 2U;
        nv = (SYMBOL **)realloc(module_syms, (nc + 1U) * sizeof(SYMBOL *));
        if (nv == NULL) return NULL;
        memset(nv + module_cs + 1U, 0,
            (nc - module_cs) * sizeof(SYMBOL *));
        module_syms = nv;
        module_cs = nc;
    }
    if (id > module_ns) module_ns = id;
    s = module_syms[id];
    if (s == NULL) {
        s = (SYMBOL *)calloc(1, sizeof(SYMBOL));
        if (s == NULL) return NULL;
        module_syms[id] = s;
    }
    return s;
}

int
kir_read_header(FILE *fp)
{
    char m[KIR_MAGIC_BYTES]; INT v;
    clear_module();
    if (fread(m,1,KIR_MAGIC_BYTES,fp)!=KIR_MAGIC_BYTES || memcmp(m,KIR_MAGIC,KIR_MAGIC_BYTES)!=0) return -1;
    if (get36(fp,&v)||v!=KIR_VERSION) return -1;
    memset(&root_symbol, 0, sizeof(root_symbol));
    symbol = &root_symbol;
    return 0;
}

static int
readgraph(FILE *fp, INT *head, unsigned hn, NODE **root)
{
    unsigned rootid,nt,ns,nn,i,kind,n; INT w[KIR_MAX_RECORD_WORDS];
    struct tref *tr=NULL; struct sref *sr=NULL; struct nref *nr=NULL;
    if(hn!=4) return -1; rootid=(unsigned)head[0]; nt=(unsigned)head[1]; ns=(unsigned)head[2]; nn=(unsigned)head[3];
    clear_graph();
    last_types=(TYPE**)calloc(nt+1,sizeof(TYPE*)); tr=(struct tref*)calloc(nt+1,sizeof(*tr));
    local_syms=(SYMBOL**)calloc(ns+1,sizeof(SYMBOL*)); sr=(struct sref*)calloc(ns,sizeof(*sr));
    last_nodes=(NODE**)calloc(nn+1,sizeof(NODE*)); nr=(struct nref*)calloc(nn+1,sizeof(*nr));
    if((nt&&(!last_types||!tr))||(ns&&(!local_syms||!sr))||(nn&&(!last_nodes||!nr))) goto bad;
    last_nt=nt; local_ns=ns; last_nn=nn;
    for(i=1;i<=nt;++i){ unsigned id,j=0; TYPE*t;
        if(readrec(fp,&kind,w,KIR_MAX_RECORD_WORDS,&n)||kind!=KIR_REC_TYPE||n<14) goto bad;
        id=(unsigned)w[j++]; if(id==0||id>nt||last_types[id]) goto bad;
        t=(TYPE*)calloc(1,sizeof(TYPE)); if(!t) goto bad; last_types[id]=t;
        t->Tspec=(unsigned char)w[j++]; t->Tbytes=(unsigned short)w[j++]; t->Tflag=unpackint(&w[j]); j+=KIR_INT_CHUNKS;
        tr[id].v0kind=(int)w[j++]; memcpy(tr[id].v0,&w[j],KIR_INT_CHUNKS*sizeof(INT)); j+=KIR_INT_CHUNKS;
        tr[id].v1kind=(int)w[j++]; tr[id].v1=(unsigned)w[j++];
    }
    for(i=0;i<ns;++i){ unsigned id,j=0,k; SYMBOL*s; INT refs;
        if(readrec(fp,&kind,w,KIR_MAX_RECORD_WORDS,&n)||kind!=KIR_REC_SYMBOL||n<54) goto bad;
        id=(unsigned)w[j++]; if(id==0) goto bad;
        s=ensuresym(id); if(!s) goto bad;
        sr[i].sym=s; sr[i].id=id;
        s->Sreg=(char)w[j++]; s->Sclass=(char)w[j++]; s->Sflags=unpackint(&w[j]); j+=KIR_INT_CHUNKS;
        for(k=0;k<IDENTSIZE;++k) s->Sname[k]=(char)w[j++]; s->Sname[IDENTSIZE-1]='\0';
        sr[i].valkind=(int)w[j++];
        memcpy(sr[i].val,&w[j],KIR_INT_CHUNKS*sizeof(INT)); j+=KIR_INT_CHUNKS;
        sr[i].type=(unsigned)w[j++]; sr[i].next=(unsigned)w[j++]; sr[i].tagkind=(int)w[j++]; sr[i].tag=(unsigned)w[j++];
        refs=unpackint(&w[j]); j+=KIR_INT_CHUNKS;
        if (((unsigned INT)id & KIR_LOCAL_ID_FLAG) != 0) s->Srefs=(int)refs;
        else s->Srefs += (int)refs;
        s->Sinit=(char)w[j++]; s->Sused=(char)w[j++];
    }
    for(i=1;i<=nn;++i){ unsigned id,j=0,k; NODE*p; int extra;
        if(readrec(fp,&kind,w,KIR_MAX_RECORD_WORDS,&n)||kind!=KIR_REC_NODE||n<17) goto bad;
        id=(unsigned)w[j++]; if(id==0||id>nn||last_nodes[id]) goto bad;
        p=(NODE*)calloc(1,sizeof(NODE)); if(!p) goto bad; last_nodes[id]=p;
        nr[id].type=(unsigned)w[j++]; p->Nflag=(int)unpackint(&w[j]); j+=KIR_INT_CHUNKS;
        p->Nop=(unsigned char)w[j++]; p->Nreg=(char)w[j++]; p->sfline=(short)w[j++];
        nr[id].left=(unsigned)w[j++]; nr[id].right=(unsigned)w[j++]; nr[id].mode=(int)w[j++]; memcpy(nr[id].v0,&w[j],KIR_INT_CHUNKS*sizeof(INT)); j+=KIR_INT_CHUNKS;
        extra=(int)w[j++];
        if(p->Nop==N_ICONST||p->Nop==N_PCONST||p->Nop==N_ECONST){ p->Niconst=unpackint(&w[j]); j+=KIR_INT_CHUNKS; p->n_var1.n_int=unpackint(&w[j]); j+=KIR_INT_CHUNKS; }
        else if(p->Nop==N_FCONST){ union{double d; unsigned char b[sizeof(double)];}du; memset(&du,0,sizeof(du)); for(k=0;k<(unsigned)extra&&k<sizeof(double);++k)du.b[k]=(unsigned char)w[j++]; p->Nfconst=du.d; }
        else if(p->Nop==N_SCONST){ p->Nsclen=extra; if(extra>0){ p->Nsconst=(char*)calloc((unsigned)extra,1); if(!p->Nsconst)goto bad; } }
    }
    for(;;){
        if(readrec(fp,&kind,w,KIR_MAX_RECORD_WORDS,&n)) goto bad;
        if(kind==KIR_REC_EXTEND) break;
        if(kind!=KIR_REC_STRING||n<2) goto bad;
        { unsigned id=(unsigned)w[0],off=(unsigned)w[1],k; NODE*p; if(id==0||id>nn||(p=last_nodes[id])==NULL||p->Nop!=N_SCONST||off+n-2>(unsigned)p->Nsclen)goto bad; for(k=2;k<n;++k)p->Nsconst[off+k-2]=(char)w[k]; }
    }
    for(i=1;i<=nt;++i){ TYPE*t=last_types[i]; if(tr[i].v0kind)t->t_v0.t_subt=(unsigned)tr[i].v0[0]<=nt?last_types[(unsigned)tr[i].v0[0]]:NULL; else t->Tsize=(unsigned INT)unpackint(tr[i].v0); if(tr[i].v1kind==2)t->Tsmtag=getsymid(tr[i].v1); else t->Tsubt=tr[i].v1<=nt?last_types[tr[i].v1]:NULL; }
    for(i=0;i<ns;++i){ SYMBOL*s=sr[i].sym; s->Stype=sr[i].type<=nt?last_types[sr[i].type]:NULL; s->Ssmnext=getsymid(sr[i].next); if(sr[i].valkind)s->Ssym=getsymid((unsigned)sr[i].val[0]); else s->Svalue=unpackint(sr[i].val); if(sr[i].tagkind==1)s->Ssmtag=getsymid(sr[i].tag); else if(sr[i].tagkind==2)s->Shproto=sr[i].tag<=nt?last_types[sr[i].tag]:NULL; }
    for(i=1;i<=nn;++i){ NODE*p=last_nodes[i]; p->Ntype=nr[i].type<=nt?last_types[nr[i].type]:NULL; if(nodelinks(p->Nop)){ p->Nleft=nr[i].left<=nn?last_nodes[nr[i].left]:NULL; p->Nright=nr[i].right<=nn?last_nodes[nr[i].right]:NULL; } if(nr[i].mode==1)p->n_var0.n_int=unpackint(nr[i].v0); else if(nr[i].mode==2)p->n_var0.n_sym=getsymid((unsigned)nr[i].v0[0]); else if(nr[i].mode==3)p->n_var0.n_node=(unsigned)nr[i].v0[0]<=nn?last_nodes[(unsigned)nr[i].v0[0]]:NULL; }
    free(tr); free(sr); free(nr); *root=rootid<=nn?last_nodes[rootid]:NULL; return 0;
bad:
    free(tr); free(sr); free(nr); clear_graph(); return -1;
}

int
kir_read_next(FILE *fp, int *kindp, NODE **root)
{
    unsigned kind,n,i,j,id; INT w[KIR_MAX_RECORD_WORDS]; SYMBOL *s;
    for (;;) {
        if(readrec(fp,&kind,w,KIR_MAX_RECORD_WORDS,&n)) return -1;
        if(kind==KIR_REC_EXTDEF){ if(readgraph(fp,w,n,root))return -1; *kindp=KIR_REC_EXTDEF; return 0; }
        if(kind==KIR_REC_GLOBAL){
            if(n != 12U + IDENTSIZE) return -1;
            j=0; id=(unsigned)w[j++];
            if(id==0 || ((unsigned INT)id & KIR_LOCAL_ID_FLAG)!=0 || (s=ensuresym(id))==NULL) return -1;
            s->Sclass=(char)w[j++]; s->Sflags=unpackint(&w[j]); j+=KIR_INT_CHUNKS;
            s->Srefs += (int)unpackint(&w[j]); j+=KIR_INT_CHUNKS;
            s->Sinit=(char)w[j++]; s->Sused=(char)w[j++];
            for(i=0;i<IDENTSIZE;++i) s->Sname[i]=(char)w[j++];
            s->Sname[IDENTSIZE-1]='\0';
            if (s->Sprev == NULL && root_symbol.Snext != s) {
                SYMBOL *tail=&root_symbol;
                while(tail->Snext!=NULL) tail=tail->Snext;
                tail->Snext=s; s->Sprev=tail; s->Snext=NULL;
            }
            continue;
        }
        if(kind==KIR_REC_MODULE_END&&n==1){ last_mainf=(int)w[0]; *root=NULL; *kindp=KIR_REC_MODULE_END; return 0; }
        return -1;
    }
}

int kir_read_mainflag(void){ return last_mainf; }

static void
clear_graph(void)
{
    unsigned i;
    for(i=1;i<=last_nn;++i){ if(last_nodes&&last_nodes[i]){ if(last_nodes[i]->Nop==N_SCONST)free(last_nodes[i]->Nsconst); free(last_nodes[i]); } }
    for(i=1;i<=local_ns;++i) if(local_syms&&local_syms[i]) free(local_syms[i]);
    for(i=1;i<=last_nt;++i) if(last_types&&last_types[i]) free(last_types[i]);
    free(last_nodes); free(local_syms); free(last_types);
    last_nodes=NULL; local_syms=NULL; last_types=NULL; last_nn=local_ns=last_nt=0;
}

static void
clear_module(void)
{
    unsigned i;
    clear_graph();
    for(i=1;i<=module_ns;++i) if(module_syms&&module_syms[i]) free(module_syms[i]);
    free(module_syms); module_syms=NULL; module_ns=module_cs=0;
    memset(&root_symbol,0,sizeof(root_symbol));
    if (symbol == &root_symbol) symbol = NULL;
}

void kir_free_graph(NODE *root){ (void)root; clear_graph(); }
void kir_free_module(void){ clear_module(); }
