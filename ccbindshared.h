/* Shared bounded constant propagation used by integrated and split KCC. */
#ifndef CCBINDSHARED_H
#define CCBINDSHARED_H
#define CONSTBIND_MAX 16

struct constbind {
    SYMBOL *sym;
    INT val;
};

static int cbtrack(SYMBOL *);
static int cbfind(struct constbind *, int, SYMBOL *);
static void cbset(struct constbind *, int *, SYMBOL *, INT);
static void cbsubst(NODE *, struct constbind *, int);
static NODE *cbfold(NODE *, struct constbind *, int);

static int
cbtrack(SYMBOL *s)
{
    if (s == NULL || s->Stype == NULL)
        return 0;
    if (tisvolatile(s->Stype) || !tisinteg(s->Stype) || sizetype(s->Stype) != 1)
        return 0;
    switch (s->Sclass) {
    case SC_AUTO:
    case SC_RAUTO:
    case SC_REGISTER:
        return 1;
    default:
        return 0;
    }
}

static int
cbfind(struct constbind *b, int n, SYMBOL *s)
{
    int i;
    for (i = n - 1; i >= 0; --i)
        if (b[i].sym == s)
            return i;
    return -1;
}

static void
cbset(struct constbind *b, int *np, SYMBOL *s, INT v)
{
    int i = cbfind(b, *np, s);
    if (i >= 0) {
        b[i].val = v;
        return;
    }
    if (*np < CONSTBIND_MAX) {
        b[*np].sym = s;
        b[*np].val = v;
        ++*np;
    }
}

static void
cbsubst(NODE *n, struct constbind *b, int nb)
{
    int i, typ;

    if (n == NULL)
        return;
    if (n->Nop == Q_IDENT) {
        i = cbfind(b, nb, n->Nid);
        if (i >= 0) {
            if (n->Nid != NULL && n->Nid->Srefs > 0)
                --n->Nid->Srefs;
            n->Nop = N_ICONST;
            n->Niconst = b[i].val;
            n->Nflag &= ~(NF_LVALUE | NF_STKREF | NF_GLOBAL);
        }
        return;
    }
    typ = tok[n->Nop].tktype;
    switch (typ) {
    case TKTY_UNOP:
    case TKTY_BOOLUN:
        if (n->Nop != N_ADDR && n->Nop != N_PTR &&
            n->Nop != N_PREINC && n->Nop != N_PREDEC &&
            n->Nop != N_POSTINC && n->Nop != N_POSTDEC)
            cbsubst(n->Nleft, b, nb);
        break;
    case TKTY_BINOP:
    case TKTY_BOOLOP:
        cbsubst(n->Nleft, b, nb);
        cbsubst(n->Nright, b, nb);
        break;
    case TKTY_TERNARY:
        cbsubst(n->Nleft, b, nb);
        if (n->Nright != NULL) {
            cbsubst(n->Nright->Nleft, b, nb);
            cbsubst(n->Nright->Nright, b, nb);
        }
        break;
    }
}

static NODE *
cbfold(NODE *n, struct constbind *b, int nb)
{
    if (n == NULL || sideffp(n))
        return n;
    cbsubst(n, b, nb);
    return evalexpr(n);
}

#endif
