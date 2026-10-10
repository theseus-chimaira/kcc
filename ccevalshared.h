/* Shared side-effect and bounded boolean evaluation for KPARSE/KGEN.
 * Keep one implementation without linking the full expression folder in KGEN.
 * Included by exactly one evaluator translation unit per phase.
 */
#ifndef CCEVAL_SHARED_H
#define CCEVAL_SHARED_H
static INT value(NODE *, NODE *);
static NODE *lookup(SYMBOL *, NODE *);
static int unset;

int
sideffp(NODE *n)
{
    if (n == NULL)
        return 0;
    switch (tok[n->Nop].tktype) {
    case TKTY_RWOP:
        return 1;
    case TKTY_PRIMARY:
        switch (n->Nop) {
        case N_FNCALL:
            return 1;
        case Q_DOT:
            if (!(n->Nflag & NF_LVALUE))
                return sideffp(n->Nleft);
            /* FALLTHROUGH */
        case Q_MEMBER:
            if (tisanyvolat(n->Ntype))
                return 1;
            return sideffp(n->Nleft);
        case Q_IDENT:
            return tisanyvolat(n->Ntype);
        default:
            return 0;
        }
    case TKTY_UNOP:
        switch (n->Nop) {
        case N_POSTINC:
        case N_POSTDEC:
        case N_PREINC:
        case N_PREDEC:
            return 1;
        case N_PTR:
            if (tisvolatile(n->Ntype))
                return 1;
            break;
        case N_ADDR:
            switch (n->Nleft->Nop) {
            case Q_IDENT:
                return 0;
            case Q_DOT:
            case Q_MEMBER:
            case N_PTR:
                return sideffp(n->Nleft->Nleft);
            }
            break;
        }
        return sideffp(n->Nleft);
    case TKTY_BOOLUN:
        return sideffp(n->Nleft);
    case TKTY_BINOP:
    case TKTY_BOOLOP:
        return sideffp(n->Nright) || sideffp(n->Nleft);
    case TKTY_ASOP:
        return 1;
    case TKTY_TERNARY:
        if (sideffp(n->Nleft))
            return 1;
        n = n->Nright;
        return (n->Nright != NULL && sideffp(n->Nright)) ||
            (n->Nleft != NULL && sideffp(n->Nleft));
    case TKTY_SEQ:
        do {
            if (sideffp(n->Nright))
                return 1;
        } while ((n = n->Nleft) != NULL);
        return 0;
    default:
        int_warn("sideffp: bad op %N", n);
        return 1;
    }
}

INT
istrue(NODE *test, NODE *bindings)
{
    unset = 0;
    return value(test, bindings) && !unset;
}

static INT
value(NODE *test, NODE *bindings)
{
    if (test == NULL)
        return 0;
    switch (test->Nop) {
    case N_ICONST:
        return test->Niconst;
    case Q_IDENT:
        test = lookup(test->Nid, bindings);
        if (test != NULL)
            return value(test, NULL);
        break;
    case Q_LAND:
        return value(test->Nleft, bindings) && value(test->Nright, bindings);
    case Q_LOR:
        return value(test->Nleft, bindings) || value(test->Nright, bindings);
    case Q_NOT:
        return !value(test->Nleft, bindings);
    case Q_EQUAL:
        return value(test->Nleft, bindings) == value(test->Nright, bindings);
    case Q_LESS:
        return value(test->Nleft, bindings) < value(test->Nright, bindings);
    case Q_GREAT:
        return value(test->Nleft, bindings) > value(test->Nright, bindings);
    case Q_NEQ:
        return value(test->Nleft, bindings) != value(test->Nright, bindings);
    case Q_LEQ:
        return value(test->Nleft, bindings) <= value(test->Nright, bindings);
    case Q_GEQ:
        return value(test->Nleft, bindings) >= value(test->Nright, bindings);
    case Q_PLUS:
        return value(test->Nleft, bindings) + value(test->Nright, bindings);
    case Q_MINUS:
        return value(test->Nleft, bindings) - value(test->Nright, bindings);
    case Q_MPLY:
        return value(test->Nleft, bindings) * value(test->Nright, bindings);
    case Q_DIV:
        return value(test->Nleft, bindings) / value(test->Nright, bindings);
    case Q_MOD:
        return value(test->Nleft, bindings) % value(test->Nright, bindings);
    }
    unset = 1;
    return 0;
}

static NODE *
lookup(SYMBOL *var, NODE *bindings)
{
    NODE *result;

    if (bindings == NULL)
        return NULL;
    switch (bindings->Nop) {
    case N_EXPRLIST:
        result = lookup(var, bindings->Nright);
        return result != NULL ? result : lookup(var, bindings->Nleft);
    case Q_ASGN:
        return bindings->Nleft->Nop == Q_IDENT &&
            bindings->Nleft->Nid == var ? bindings->Nright : NULL;
    default:
        return NULL;
    }
}

#endif
