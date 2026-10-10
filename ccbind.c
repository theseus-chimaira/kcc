/* CCBIND.C - parser-side bounded statement-tree normalization.
 *
 * The split native compiler serializes the fully typed tree after KPARSE.
 * Keep straight-line constant binding and the associated dead-single-store
 * elimination on that side of KIR1 so KGEN does not need the full expression
 * evaluator merely to repeat parser-level tree folding.
 */
#include "cc.h"

extern NODE *evalexpr(NODE *);
extern int sideffp(NODE *);
extern INT sizetype(TYPE *);

#include "ccbindshared.h"
static int cbbranchsafe(NODE *, struct constbind *, int);
static int labchk(NODE *);
static int cbifkeep(NODE *, struct constbind *, int);
static int dsedeadonce(NODE *);
static void bindlist(NODE *);
static void bindopt_tree(NODE *);

static int
cbbranchsafe(NODE *n, struct constbind *b, int nb)
{
    int i, typ;

    if (n == NULL)
        return 1;
    switch (n->Nop) {
    case N_FNCALL:
    case Q_GOTO:
    case Q_BREAK:
    case Q_CONTINUE:
    case Q_CASE:
    case N_LABEL:
    case Q_IF:
    case Q_FOR:
    case Q_WHILE:
    case Q_DO:
    case Q_SWITCH:
        return 0;
    case Q_ASGN:
        if (n->Nleft == NULL || n->Nleft->Nop != Q_IDENT)
            return 0;
        i = cbfind(b, nb, n->Nleft->Nid);
        return i < 0 && cbbranchsafe(n->Nright, b, nb);
    case N_PREINC:
    case N_PREDEC:
    case N_POSTINC:
    case N_POSTDEC:
        return n->Nleft != NULL && n->Nleft->Nop == Q_IDENT &&
            cbfind(b, nb, n->Nleft->Nid) < 0;
    case N_ADDR:
        if (n->Nleft != NULL && n->Nleft->Nop == Q_IDENT &&
            cbfind(b, nb, n->Nleft->Nid) >= 0)
            return 0;
        return cbbranchsafe(n->Nleft, b, nb);
    }
    typ = tok[n->Nop].tktype;
    switch (typ) {
    case TKTY_UNOP:
    case TKTY_BOOLUN:
        return cbbranchsafe(n->Nleft, b, nb);
    case TKTY_BINOP:
    case TKTY_BOOLOP:
        return cbbranchsafe(n->Nleft, b, nb) &&
            cbbranchsafe(n->Nright, b, nb);
    case TKTY_TERNARY:
        if (!cbbranchsafe(n->Nleft, b, nb))
            return 0;
        return n->Nright == NULL ||
            (cbbranchsafe(n->Nright->Nleft, b, nb) &&
             cbbranchsafe(n->Nright->Nright, b, nb));
    default:
        return 1;
    }
}

static int
labchk(NODE *n)
{
    if (n == NULL)
        return 0;
    switch (n->Nop) {
    case Q_CASE:
    case N_LABEL:
    case Q_DEFAULT:
        return 1;
    case N_STATEMENT:
        if (n->Nleft != NULL && n->Nleft->Nop == N_DATA)
            n = n->Nright;
        for (; n != NULL; n = n->Nright) {
            if (n->Nop != N_STATEMENT) {
                int_error("labchk: bad stmt %N", n);
                return 1;
            }
            if (n->Nleft != NULL && labchk(n->Nleft))
                return 1;
        }
        return 0;
    case Q_IF:
        return n->Nright != NULL &&
            (labchk(n->Nright->Nleft) || labchk(n->Nright->Nright));
    case Q_DO:
    case Q_FOR:
    case Q_SWITCH:
    case Q_WHILE:
        return labchk(n->Nright);
    default:
        return 0;
    }
}

static int
cbifkeep(NODE *n, struct constbind *b, int nb)
{
    NODE *body, *nthen, *nelse;

    if (n == NULL || n->Nop != Q_IF || n->Nleft == NULL ||
        n->Nleft->Nop != N_ICONST || n->Nright == NULL)
        return 0;
    body = n->Nright;
    nthen = body->Nleft;
    nelse = body->Nright;
    if (n->Nleft->Niconst)
        return !labchk(nelse) && cbbranchsafe(nthen, b, nb);
    return !labchk(nthen) && cbbranchsafe(nelse, b, nb);
}

static int
dsedeadonce(NODE *n)
{
    SYMBOL *s;

    if (n == NULL || n->Nop != Q_ASGN || !(n->Nflag & NF_DISCARD) ||
        n->Nleft == NULL || n->Nleft->Nop != Q_IDENT || n->Nright == NULL)
        return 0;
    s = n->Nleft->Nid;
    if (s == NULL || s->Stype == NULL || s->Srefs != 1 ||
        tisvolatile(s->Stype) || (s->Sflags & SF_ADDRTAKEN) ||
        sideffp(n->Nright))
        return 0;
    return s->Sclass == SC_AUTO || s->Sclass == SC_RAUTO ||
        s->Sclass == SC_REGISTER;
}

static void
bindlist(NODE *head)
{
    struct constbind binds[CONSTBIND_MAX];
    NODE *n, *st;
    SYMBOL *bsym;
    int nbind;

    nbind = 0;
    for (n = head; n != NULL; n = n->Nright) {
        if (n->Nop != N_STATEMENT)
            return;
        st = n->Nleft;
        if (st == NULL)
            continue;
        if (st->Nop == N_DATA) {
            nbind = 0;
            continue;
        }
        bsym = NULL;
        if (st->Nop == Q_ASGN && st->Nleft != NULL &&
            st->Nleft->Nop == Q_IDENT && cbtrack(st->Nleft->Nid)) {
            bsym = st->Nleft->Nid;
            st->Nright = cbfold(st->Nright, binds, nbind);
        } else if (st->Nop == Q_IF) {
            st->Nleft = cbfold(st->Nleft, binds, nbind);
        } else if (st->Nop == Q_RETURN) {
            st->Nright = cbfold(st->Nright, binds, nbind);
        }
        if (bsym != NULL && st->Nright != NULL && st->Nright->Nop == N_ICONST)
            cbset(binds, &nbind, bsym, st->Nright->Niconst);
        else if (!(st->Nop == Q_IF && cbifkeep(st, binds, nbind)))
            nbind = 0;
    }

    nbind = 0;
    for (n = head; n != NULL; n = n->Nright) {
        if (n->Nop != N_STATEMENT)
            return;
        st = n->Nleft;
        if (st == NULL)
            continue;
        if (st->Nop == N_DATA) {
            nbind = 0;
            continue;
        }
        if (dsedeadonce(st)) {
            n->Nleft = NULL;
            nbind = 0;
            continue;
        }
        bsym = NULL;
        if (st->Nop == Q_ASGN && st->Nleft != NULL &&
            st->Nleft->Nop == Q_IDENT && cbtrack(st->Nleft->Nid)) {
            bsym = st->Nleft->Nid;
            st->Nright = cbfold(st->Nright, binds, nbind);
        } else if (st->Nop == Q_IF) {
            st->Nleft = cbfold(st->Nleft, binds, nbind);
        } else if (st->Nop == Q_RETURN) {
            st->Nright = cbfold(st->Nright, binds, nbind);
        }
        if (bsym != NULL && st->Nright != NULL && st->Nright->Nop == N_ICONST)
            cbset(binds, &nbind, bsym, st->Nright->Niconst);
        else if (!(st->Nop == Q_IF && cbifkeep(st, binds, nbind)))
            nbind = 0;
        bindopt_tree(st);
    }
}

static void
bindopt_tree(NODE *n)
{
    if (n == NULL)
        return;
    switch (n->Nop) {
    case N_STATEMENT:
        bindlist(n);
        break;
    case Q_CASE:
    case N_LABEL:
        bindopt_tree(n->Nleft);
        break;
    case Q_IF:
        if (n->Nright != NULL) {
            bindopt_tree(n->Nright->Nleft);
            bindopt_tree(n->Nright->Nright);
        }
        break;
    case Q_DO:
    case Q_FOR:
    case Q_SWITCH:
    case Q_WHILE:
        bindopt_tree(n->Nright);
        break;
    }
}

void
bindopt(NODE *root)
{
    bindopt_tree(root);
}
