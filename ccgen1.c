/*	CCGEN1.C - Generate code for parse-tree statement execution
**
**	(c) Copyright Ken Harrenstien 1989
**		All changes after v.198, 9-Mar-1988
**	(c) Copyright Ken Harrenstien, SRI International 1985, 1986
**		All changes after v.112, 8-Aug-1985
**
**	Original version (C) 1981  K. Chen
*/

#include "cc.h"
#include "ccgen.h"
#include <ctype.h>	/* isdigit */

/* Imported functions */
extern void vrfree(VREG *);
extern int vrtoreal(VREG *);
extern VREG *vrget(void);		/* CCREG */
extern void code_debugcall(NODE *);	/* CCDBUG */
extern NODE *ndeflr(int op, NODE *l, NODE *r);		/* CCSTMT */
extern VREG *genexpr(NODE *);		/* CCGEN2 for expressions */
extern void genxrelease(NODE *);	/* CCGEN2 for discarded expressions */
extern VREG *getmem(VREG *, TYPE *, int, int), 
	*stomem(VREG *, VREG *, INT, int);	/* CCGEN2 */
extern void gswitch(NODE *);		/* CCGSWI for switch statement */
extern void codfallthrough(SYMBOL *);	/* CCCODE */
extern SYMBOL *newlabel(void);	/* CCSYM */
extern INT sizetype(TYPE *), sizeptobj(TYPE *);		/* CCSYM */
extern TYPE *findtype(int, TYPE *);
extern void outlab(SYMBOL *), outstr(char *), outnum(INT);	/* CCOUT */
extern void relflush(VREG *), gboolean(NODE *, SYMBOL *, int), freelabel(SYMBOL *);
extern INT istrue(NODE *, NODE *);		/* CCEVAL */
extern NODE *evalexpr(NODE *);
extern NODE *convbinary(NODE *), *convasgn(TYPE *, NODE *);
extern int sideffp(NODE *);
extern int deadjump(void);		/* CCJSKP */
extern void killstack(void);	/* CCOPT */
extern void codemdx(int, int, SYMBOL *, INT, int), code00(int, int, int),
    code4(int, VREG *, VREG *);
extern SYMBOL *vlabase_v11(SYMBOL *), *vlaboundsym_v11(TYPE *),
    *vlaobjmarkget_v12(SYMBOL *); /* CCDECL */
extern NODE *vlaboundexpr_v11(TYPE *);
extern int vlaboundcaptured_v12(TYPE *);
extern void vlaboundsetcaptured_v12(TYPE *);

extern void outepilog(SYMBOL *);	/* CCOUT */
extern VREG *gmuuo(NODE *);	/* CCGEN2 for imuuo key word; KAR 1/91 */

extern
void	    outiepilog (void);		/* FW 2A(52) */

/* Exported functions defined here */
void genstmt(NODE *), genretinit(NODE *), genretepilog(int), genadata(NODE *);

/* Internal functions */

static void gdo(NODE *), gfor(NODE *), gif(NODE *), 
	gwhile(NODE *), greturn(NODE *);
static SYMBOL *gtoplab(void); 
static int labchk(NODE *);
static int indloopmatch(NODE *, NODE **, SYMBOL **);
static int indbodywrites(NODE *, SYMBOL *);
static NODE *laststmt(NODE *);
static int izruntime(NODE *);
static void genaggcopy(SYMBOL *, NODE *);
static void genautoiz(NODE *, NODE *, TYPE *);
static NODE *genizindex(NODE *, TYPE *, INT);

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
static void cbprepass(NODE *, struct constbind *);
static int cbbranchsafe(NODE *, struct constbind *, int);
static int cbifkeep(NODE *, struct constbind *, int);
static int dsedeadonce(NODE *);
static SYMBOL *retlabel;
static NODE *retfall;
static INT autooff1_v11(SYMBOL *);
static int frameindex1_v11(void);
#if 0
static void genadata();
static void genvla_v11(SYMBOL *);
static void gdo(), gfor(), gif(), gwhile(), greturn();
static SYMBOL *gtoplab();
static int labchk();
static NODE *laststmt();
#endif


/* CONSTBIND helpers - very small straight-line constant propagation.
**
** This deliberately tracks only integral one-word auto/register locals.
** Bindings live only within one lexical N_STATEMENT list and are discarded
** across control-flow or side-effecting statement boundaries.  Thus this is
** not alias analysis; it is merely a cheap way to expose constants to the
** existing expression folder.
*/
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
    int i;

    i = cbfind(b, *np, s);
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
            /* This identifier occurrence disappears from the generated
            ** program.  Keep Srefs in step with the rewritten tree so the
            ** later cheap dead-store test can see a now-unread local.
            */
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
        /* Never rewrite an lvalue hidden beneath & or *. */
        if (n->Nop != N_ADDR && n->Nop != N_PTR
          && n->Nop != N_PREINC && n->Nop != N_PREDEC
          && n->Nop != N_POSTINC && n->Nop != N_POSTDEC)
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

    default:
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

/* CBPREPASS - Expose straight-line constants before statement emission.
**
** The normal one-pass generator used to discover that later reads were
** constants only after it had already emitted the defining stores.  Walk the
** same statement list once first, using the existing bounded constant-binding
** table, so eliminated identifier reads reduce Srefs before DSEDEADONCE sees
** their definitions.  This mutates only the existing tree and adds no
** whole-function dataflow state.
*/
static void
cbprepass(NODE *n, struct constbind *binds)
{
    int nbind = 0;
    NODE *st;
    SYMBOL *bsym;

    for (; n != NULL; n = n->Nright) {
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
        if (st->Nop == Q_ASGN && st->Nleft != NULL
          && st->Nleft->Nop == Q_IDENT && cbtrack(st->Nleft->Nid)) {
            bsym = st->Nleft->Nid;
            st->Nright = cbfold(st->Nright, binds, nbind);
        } else if (st->Nop == Q_IF) {
            st->Nleft = cbfold(st->Nleft, binds, nbind);
        } else if (st->Nop == Q_RETURN) {
            st->Nright = cbfold(st->Nright, binds, nbind);
        }

        if (bsym != NULL && st->Nright != NULL
          && st->Nright->Nop == N_ICONST)
            cbset(binds, &nbind, bsym, st->Nright->Niconst);
        else if (st->Nop == Q_IF && cbifkeep(st, binds, nbind))
            ;
        else
            nbind = 0;
    }
}

/* Return nonzero if executing N cannot invalidate any current binding.
** This is intentionally narrower than a general side-effect analysis.
** Direct stores to other named objects are harmless, but calls, indirect
** stores, address escape, and control-flow constructs end propagation.
*/
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
        if (i >= 0)
            return 0;
        return cbbranchsafe(n->Nright, b, nb);

    case N_PREINC:
    case N_PREDEC:
    case N_POSTINC:
    case N_POSTDEC:
        if (n->Nleft == NULL || n->Nleft->Nop != Q_IDENT)
            return 0;
        return cbfind(b, nb, n->Nleft->Nid) < 0;

    case N_ADDR:
        if (n->Nleft != NULL && n->Nleft->Nop == Q_IDENT
          && cbfind(b, nb, n->Nleft->Nid) >= 0)
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
        return cbbranchsafe(n->Nleft, b, nb)
            && cbbranchsafe(n->Nright, b, nb);

    case TKTY_TERNARY:
        if (!cbbranchsafe(n->Nleft, b, nb))
            return 0;
        if (n->Nright == NULL)
            return 1;
        return cbbranchsafe(n->Nright->Nleft, b, nb)
            && cbbranchsafe(n->Nright->Nright, b, nb);

    default:
        return 1;
    }
}

/* A constant IF is transparent to the current bindings only when GIF() can
** really discard the unselected arm and the selected arm cannot invalidate
** a binding.
*/
static int
cbifkeep(NODE *n, struct constbind *b, int nb)
{
    NODE *body, *nthen, *nelse;

    if (n == NULL || n->Nop != Q_IF || n->Nleft == NULL
      || n->Nleft->Nop != N_ICONST || n->Nright == NULL)
        return 0;
    body = n->Nright;
    nthen = body->Nleft;
    nelse = body->Nright;
    if (n->Nleft->Niconst) {
        if (labchk(nelse))
            return 0;
        return cbbranchsafe(nthen, b, nb);
    }
    if (labchk(nthen))
        return 0;
    return cbbranchsafe(nelse, b, nb);
}


/* DSEDEADONCE - Recognize a dead, discarded simple local assignment.
**
** KCC's parser counts each identifier occurrence in Srefs.  For an automatic
** or register local which occurs only as the left side of one assignment,
** Srefs is exactly one.  If its address is not taken and the RHS is pure,
** the store and RHS computation are unobservable and can be omitted.
**
** This deliberately handles only the single-write/zero-read case.  It is
** cheap, needs no dataflow pass, and works equally for one- and two-word
** objects.  More general overwritten-store elimination is left separate.
*/
static int
dsedeadonce(NODE *n)
{
    SYMBOL *s;

    if (n == NULL || n->Nop != Q_ASGN || !(n->Nflag & NF_DISCARD)
      || n->Nleft == NULL || n->Nleft->Nop != Q_IDENT
      || n->Nright == NULL)
        return 0;
    s = n->Nleft->Nid;
    if (s == NULL || s->Stype == NULL || s->Srefs != 1
      || tisvolatile(s->Stype)
      || (s->Sflags & SF_ADDRTAKEN) || sideffp(n->Nright))
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


/* ------------------------------------- */
/*      generate code for statement      */
/* ------------------------------------- */

extern void outstr(char *);

void
genstmt(NODE *n)
{
    VREG	*muuo_ac;

    if (n == NULL) return;
    switch (n->Nop) {

    case N_STATEMENT:
	{ NODE *beg, *next, *st;
	  struct constbind binds[CONSTBIND_MAX];
	  int nbind = 0;
	  SYMBOL *bsym;
	if (n->Nleft && n->Nleft->Nop == N_DATA) { /* Check for auto inits */
	    genadata(n->Nleft);		/* Yep, do them */
	    n = n->Nright;		/* then move on to real statements */
	}
	if (optgen)
	    cbprepass(n, binds);
	for(beg = n; n != NULL; n = n->Nright) {
	    if(n->Nop != N_STATEMENT)
		int_error("genstmt: bad stmt %N", n);
	    if(n->Nleft == NULL) continue;
	    if(n->Nleft->Nop == N_DATA) {
		genadata(n->Nleft);
		nbind = 0;
		continue;
	    }

	    /* Propagate simple straight-line integral constants into pure
	    ** expressions before the normal expression folder/code generator.
	    ** Anything with control flow or unknown side effects ends the run.
	    */
	    st = n->Nleft;
	    if (optgen && dsedeadonce(st)) {
		nbind = 0;
		continue;
	    }
	    bsym = NULL;
	    if (optgen) {
		if (st->Nop == Q_ASGN && st->Nleft != NULL
		  && st->Nleft->Nop == Q_IDENT
		  && cbtrack(st->Nleft->Nid)) {
		    bsym = st->Nleft->Nid;
		    st->Nright = cbfold(st->Nright, binds, nbind);
		} else if (st->Nop == Q_IF) {
		    st->Nleft = cbfold(st->Nleft, binds, nbind);
		} else if (st->Nop == Q_RETURN) {
		    st->Nright = cbfold(st->Nright, binds, nbind);
		}
	    }

	    /* Check out following stmt for possible optimizations */
	    if(n->Nright && (next = n->Nright->Nleft) != NULL && optgen) {
		switch(next->Nop) {

		/* Hack to encourage tail recursion */
		case Q_RETURN:			/* If next will be RETURN */
		    if(next->Nright == NULL) {	/* and has no return val */
			NODE *v;		/* Then try to optimize */
			if((v = laststmt(n->Nleft)) != NULL && v->Nop == N_FNCALL)
			    v->Nflag |= NF_RETEXPR;
		    }
		    break;

		/* If next stmt is a GOTO, ensure that any jumps
		 * within current stmt to end of stmt will
		 * instead go directly to object of the GOTO.
		 * Avoids jumping to jumps...
		 * We do a similar hack for BREAK and CONTINUE,
		 * which are similar to GOTOs except that their
		 * destination is kept in variables global to the
		 * code generation routines.
		 */
		case Q_CASE:	/* Not sure about this one yet */
		case N_LABEL:

		case Q_GOTO:
		    n->Nleft->Nendlab = next->Nxfsym;
		    break;
		case Q_BREAK:
		    n->Nleft->Nendlab = brklabel;
		    break;
		case Q_CONTINUE:
		    n->Nleft->Nendlab = looplabel;
		    break;
	        default:
	            ;	/* do nothing */
	    }	/* end of Nop switch */
	    }	/* end of next-stmt check */

	    /* Optimize label usage */
	    if(n->Nright == NULL 	/* If this is last stmt in list */
		&& optgen)
		n->Nleft->Nendlab = beg->Nendlab;	/* Copy from 1st */

	    genstmt(n->Nleft);

	    if (optgen && bsym != NULL && st->Nright != NULL
	      && st->Nright->Nop == N_ICONST)
		cbset(binds, &nbind, bsym, st->Nright->Niconst);
	    else if (optgen && st->Nop == Q_IF
	      && cbifkeep(st, binds, nbind))
		;			/* Constant branch kept bindings valid */
	    else
		nbind = 0;
	}
	break;
	} /* end of N_STATEMENT case block */

    case Q_CASE:
	codlabel(n->Nxfsym);		/* send forward label */
	if (n->Nleft)
	    n->Nleft->Nendlab = n->Nendlab;	/* propagate end label */
	genstmt(n->Nleft);		/* finish rest of body */
	break;

    case N_LABEL:
	if (n->Nxfsym->Sname[0] == '%' && isdigit(n->Nxfsym->Sname[1]))
	    code_debugcall(n);
	else
	    codgolab(n->Nxfsym);		/* send goto label */
	if (n->Nleft)
	    n->Nleft->Nendlab = n->Nendlab;	/* propagate end label */
	genstmt(n->Nleft);		/* finish rest of body */
	break;

    case N_VLARST:
        if (n->Nid != NULL) {
            INT off = autooff1_v11(n->Nid);
            codemdx(P_MOVE, R_SP, (SYMBOL *)NULL, off, frameindex1_v11());
            flushcode();
        }
        break;
    case Q_BREAK:
        code6(P_JRST, NULL, brklabel);
        break;
    case Q_GOTO:
	if (n->Nleft)			/* computed goto *expr */
	    {
	    VREG *r = genexpr(n->Nleft);

	    code4(P_JRST, (VREG *)NULL, r);
	    vrfree(r);
	    }
	else
	    code6(P_JRST, NULL, n->Nxfsym);
	break;
    case Q_CONTINUE:
        code6(P_JRST, NULL, looplabel);
        break;
    case Q_DO:		gdo(n);		break;
    case Q_FOR:		gfor(n);	break;
    case Q_IF:		gif(n);		break;
    case Q_RETURN:      greturn(n);	break;
    case Q_SWITCH:	gswitch(n);	break;
    case Q_WHILE:	gwhile(n);	break;

#if SYS_CSI	/* Added 1/91 for in-line monitor calls; KAR */
    case Q_MUUO:
	muuo_ac = gmuuo(n);
	vrfree(muuo_ac);
	break;
#endif

    case N_EXPRLIST:		/* Same as expression stmt */
    default:			/* None of above, assume expression stmt */
	genxrelease(n);		/* Generate it and flush any result */
	break;
    }
}

/* Hack for statement optimization.  Find last statement in list. */
static NODE *
laststmt(struct node * n)
{
	while(n && n->Nop == N_STATEMENT) {
		while(n->Nright) n = n->Nright;
		n = n->Nleft;
	}
	return(n);
}

static INT
autooff1_v11(SYMBOL *s)
{
    return fnvla_v11 ? (s->Svalue + 1 - maxauto)
                     : ((s->Svalue + 1) + fnframesave - stackoffset);
}

static int
frameindex1_v11(void)
{
    return fnvla_v11 ? R_MAXREG : R_SP;
}

static NODE *
vla_bin_v11(int op, NODE *l, NODE *r)
{
    NODE *n = ndeflr(op, l, r);
    return convbinary(n);
}

static INT
vla_basebytes_v11(TYPE *t)
{
    INT bits;

    if (t == NULL)
        return 0;
    if (tispacked(t))
        return t->Tbytes;
    switch (t->Tspec) {
    case TS_CHAR:
    case TS_UCHAR:
        return 1;
    case TS_STRUCT:
    case TS_UNION:
        return sizetype(t) * (TGSIZ_WORD / TGSIZ_CHAR);
    default:
        if (tisscalar(t)) {
            bits = tbitsize(t);
            return (bits + TGSIZ_CHAR - 1) / TGSIZ_CHAR;
        }
        return sizetype(t) * (TGSIZ_WORD / TGSIZ_CHAR);
    }
}

static NODE *
vla_bytesexpr_v11(TYPE *t)
{
    NODE *count, *sub;
    SYMBOL *bs;

    if (t == NULL)
        return ndeficonst(0);
    if (t->Tspec != TS_ARRAY)
        return ndeficonst(vla_basebytes_v11(t));

    if (tisvla(t)) {
        bs = vlaboundsym_v11(t);
        if (bs == NULL) {
            int_error("vla_bytesexpr_v11: unbound VLA dimension");
            count = ndeficonst(1);
        } else
            count = ndefident(bs);
    } else
        count = ndeficonst(t->Tsize);

    sub = vla_bytesexpr_v11(t->Tsubt);
    return vla_bin_v11(Q_MPLY, count, sub);
}

static void
vla_evalbounds_v11(TYPE *t)
{
    NODE *as, *rhs;
    SYMBOL *bs;

    if (t == NULL || t->Tspec != TS_ARRAY)
        return;
    if (tisvla(t)) {
        bs = vlaboundsym_v11(t);
        rhs = vlaboundexpr_v11(t);
        if (bs == NULL || rhs == NULL) {
            int_error("vla_evalbounds_v11: missing bound state");
            return;
        }
        if (!vlaboundcaptured_v12(t)) {
            rhs = convasgn(bs->Stype, rhs);
            as = ndeflr(Q_ASGN, ndefident(bs), rhs);
            as->Ntype = bs->Stype;
            genxrelease(as);
            vlaboundsetcaptured_v12(t);
        }
    }
    vla_evalbounds_v11(t->Tsubt);
}

static void
genvla_v11(SYMBOL *s)
{
    SYMBOL *base;
    NODE *bytes, *words;
    VREG *rb, *rw;
    INT off;

    base = vlabase_v11(s);
    if (base == NULL) {
        TYPE *pt = s->Stype;
        if (s->Sclass == SC_TYPEDEF) {
            vla_evalbounds_v11(s->Stype);
            return;
        }
        /* Parameter array declarators have already adjusted the outermost
        ** array to a pointer.  Capture any remaining variably-modified row
        ** bounds at function entry; no dynamic object is allocated here. */
        if ((s->Sclass == SC_ARG || s->Sclass == SC_RARG)
          && pt != NULL && pt->Tspec == TS_PTR) {
            vla_evalbounds_v11(pt->Tsubt);
            return;
        }
        int_error("genvla_v11: missing base symbol");
        return;
    }

    {
        SYMBOL *mark = vlaobjmarkget_v12(s);
        if (mark != NULL) {
            INT moff = autooff1_v11(mark);
            codemdx(P_MOVEM, R_SP, (SYMBOL *)NULL, moff, frameindex1_v11());
            flushcode();
        }
    }

    /* Each variably-modified dimension is evaluated exactly once at the
    ** declaration point and retained in a hidden fixed-frame local. */
    vla_evalbounds_v11(s->Stype);

    bytes = vla_bytesexpr_v11(s->Stype);
    bytes = vla_bin_v11(Q_PLUS, bytes, ndeficonst(
                        (TGSIZ_WORD / TGSIZ_CHAR) - 1));
    words = vla_bin_v11(Q_RSHFT, bytes, ndeficonst(2));

    /* Form the address of the first newly allocated word before ADJSP. */
    rb = vrget();
    codemdx(P_MOVEI, vrtoreal(rb), (SYMBOL *)NULL, 1, R_SP);
    rw = genexpr(words);
    if (rw == NULL) {
        vrfree(rb);
        int_error("genvla_v11: null runtime size");
        return;
    }
    {
    int sr = vrtoreal(rw);
    codemdx(P_ADJSP, R_SP, (SYMBOL *)NULL, 0, sr);
    vrfree(rw);
    }

    off = autooff1_v11(base);
    codemdx(P_MOVEM, vrtoreal(rb), (SYMBOL *)NULL, off, frameindex1_v11());
    vrfree(rb);
}

/* GENADATA - Generate auto data initializations
**	Should be called only for N_DATA nodes.
**
** Includes "gizlist" - Generate a struct/union/array constant.
**	This can only happen for brace-enclosed initializers.
** Node will be a N_IZLIST, with Ntype set to desired type of the
** constant.
** For the time being, our approach is to chicken out by just inventing
** a new internal label and returning that.  This label and the N_IZLIST
** are put together to form a N_LITIZ list and chained onto "litnodes"
** for eventual output as a static data literal.
*/
static int
izruntime(NODE *n)
{
    for (; n != NULL; n = (n->Nop == N_IZLIST) ? n->Nright : NULL) {
        NODE *e = (n->Nop == N_IZLIST) ? n->Nleft : n;
        if (e == NULL)
            continue;
        if (e->Nop == N_IZLIST) {
            if (izruntime(e))
                return 1;
            continue;
        }
        switch (e->Nop) {
        case N_ICONST:
        case N_FCONST:
        case N_SCONST:
        case N_PCONST:
        case N_ECONST:
            break;
        default:
            return 1;
        }
    }
    return 0;
}

static void
genaggcopy(SYMBOL *s, NODE *l)
{
    TYPE *t = l->Ntype;
    VREG *ra, *r;

    litnodes = ndeflr(N_LITIZ, l, litnodes);
    litnodes->Nendlab = newlabel();

    r = vrget();
    ra = vrget();
    code3(P_MOVE, r, litnodes->Nendlab);
    r = getmem(r, t, 0, 0);
    if (fnvla_v11)
        codemdx(P_MOVEI, vrtoreal(ra), (SYMBOL *)NULL,
                autooff1_v11(s), R_MAXREG);
    else
        code13(P_MOVE, ra, autooff1_v11(s));
    r = stomem(r, ra, sizetype(t), 0);
    relflush(r);
}

static NODE *
genizindex(NODE *base, TYPE *et, INT idx)
{
    TYPE *pt;
    NODE *p, *q;

    pt = findtype(TS_PTR, et);
    p = ndef(N_ADDR, pt, base->Nflag & ~NF_LVALUE, base, (NODE *)NULL);
    q = ndeflr(Q_PLUS, p, ndeficonst(idx));
    q->Ntype = pt;
    return ndef(N_PTR, et, NF_LVALUE, q, (NODE *)NULL);
}

static void
genautoiz(NODE *base, NODE *iz, TYPE *t)
{
    SYMBOL *sm;
    NODE *slot, *dst, *as;
    INT idx;

    if (iz == NULL)
        return;
    if (iz->Nop != N_IZLIST) {
        as = ndeflr(Q_ASGN, base, iz);
        as->Ntype = base->Ntype;
        genxrelease(as);
        return;
    }

    switch (t->Tspec) {
    case TS_ARRAY:
        slot = iz;
        for (idx = 0; slot != NULL && idx < (INT)t->Tsize;
             ++idx, slot = slot->Nright) {
            if (slot->Nleft == NULL)
                continue;
            dst = genizindex(base, t->Tsubt, idx);
            genautoiz(dst, slot->Nleft, t->Tsubt);
        }
        return;

    case TS_UNION:
        sm = iz->Nizmem ? iz->Nizmem : t->Tsmtag->Ssmnext;
        if (sm == NULL || iz->Nleft == NULL)
            return;
        dst = ndef(Q_DOT, sm->Stype, base->Nflag | NF_LVALUE,
                   base, (NODE *)NULL);
        dst->Nxoff = sm->Ssmoff;
        genautoiz(dst, iz->Nleft, sm->Stype);
        return;

    case TS_STRUCT:
        sm = t->Tsmtag->Ssmnext;
        slot = iz;
        for (; sm != NULL && slot != NULL;
             sm = sm->Ssmnext, slot = slot->Nright) {
            if (slot->Nleft == NULL)
                continue;
            dst = ndef(Q_DOT, sm->Stype, base->Nflag | NF_LVALUE,
                       base, (NODE *)NULL);
            dst->Nxoff = sm->Ssmoff;
            genautoiz(dst, slot->Nleft, sm->Stype);
        }
        return;

    default:
        int_error("genautoiz: aggregate list for non-aggregate type");
        return;
    }
}

void
genadata(struct node * n)
{
    if (n->Nop != N_DATA) {
        int_error("genadata: node not N_DATA %N", n);
        return;
    }
#if SYS_CSI
    if (Register_Id(n))
        int_error ("genadata: register var of non-arithmetic type");
#endif
    for (; n && n->Nop == N_DATA; n = n->Nright) {
        if (n->Nleft != NULL && n->Nleft->Nright != NULL) {
            if (n->Nleft->Nright->Nop == N_VLA) {
                genvla_v11(n->Nleft->Nleft->Nid);
                continue;
            }
            if (n->Nleft->Nright->Nop == N_IZLIST) {
                SYMBOL *s = n->Nleft->Nleft->Nid;
                NODE *l = n->Nleft->Nright;

                if (izruntime(l)) {
                    NODE *z = ndeftl(N_IZLIST, l->Ntype, NULL);
                    genaggcopy(s, z);
                    genautoiz(ndefident(s), l, l->Ntype);
                } else
                    genaggcopy(s, l);
                continue;
            }

            n->Nleft->Nop = Q_ASGN;
            n->Nleft->Ntype = n->Nleft->Nleft->Ntype;
            genxrelease(n->Nleft);
        }
    }
}


/* ---------------------- */
/*	if statement      */
/* ---------------------- */
static void
gif(struct node * n)
{
    SYMBOL *true, *false;
    NODE *nthen, *nelse, *body, *l;

    body = n->Nright;
    nthen = body->Nleft;
    nelse = body->Nright;
    l = n->Nleft;

    /* optimize if to a jump */
    if (nelse == NULL) {
	if (nthen == NULL) {		/* no body of either kind?? */
	    genxrelease(l);		/* yes, just produce condition */
	    return;			/* and return */
	} else if (optgen) switch (nthen->Nop) {
	case Q_BREAK:
	    l->Nendlab = n->Nendlab;
	    gboolean(l, brklabel, 1);
	    return;
	case Q_GOTO:
	    l->Nendlab = n->Nendlab;
	    gboolean(l, nthen->Nxfsym, 1);
	    return;
	case Q_CONTINUE:
	    l->Nendlab = n->Nendlab;
	    gboolean(l, looplabel, 1);
	    return;
	default:
	    ;	/* do nothing */
	}
    }

    /* Try to optimize when conditional expression is a constant.
    ** We have to be careful about flushing the then/else clauses because
    ** control could jump into them using either case or goto labels.
    ** Hence the labchk() to see whether the parse tree contains such labels...
    */
    if (l->Nop == N_ICONST && optgen) {		/* If cond is constant */
	if (l->Niconst && !labchk(nelse)) {	/* if (1) & "else" flushable */
	    if (nthen) {
		nthen->Nendlab = n->Nendlab;
		genstmt(nthen);
	    }
	    return;
	}
	if (!l->Niconst && !labchk(nthen)) {	/* if (0) & "then" flushable */
	    if (nelse) {
		nelse->Nendlab = n->Nendlab;
		genstmt(nelse);
	    }
	    return;
	}
    }

    /* do unoptimized if statement - first get exit label */
    true = ((n->Nendlab == NULL)? newlabel() : n->Nendlab);

    /* then emit code for test and clauses */
    if (nthen) {
	if (nelse == NULL) false = true;
	else switch (nelse->Nop) {
	case Q_GOTO:
	    false = nelse->Nxfsym;
	    nelse = NULL;
	    break;
	case Q_CONTINUE:
	    false = looplabel;
	    nelse = NULL;
	    break;
	case Q_BREAK:
	    false = brklabel;
	    nelse = NULL;
	    break;
	default:
	    false = newlabel();
	}
	switch(nthen->Nop) {		/* we could invert the boolean here, */
	case Q_GOTO:			/* but instead we merely set label */
	case N_LABEL:			/* at the end of the condition. */
	case Q_CASE:			/* fixes gotos in both clauses. */
	    l->Nendlab = nthen->Nxfsym;
	    break;
	case Q_CONTINUE:
	    l->Nendlab = looplabel;
	    break;
	case Q_BREAK:
	    l->Nendlab = brklabel;
	default:
	    ;	/* do nothing */
	}
	gboolean(l, false, 0);
	nthen->Nendlab = true;
	genstmt(nthen);
	if (nelse) {
	    code6(P_JRST, NULL, true);
	    codlabel(false);
	    nelse->Nendlab = true;
	    genstmt(nelse);
	}
    } else if (nelse) {
	gboolean(l, true, 1);
	nelse->Nendlab = true;
	genstmt(nelse);
    }

    /* then emit exit label */
    if (!n->Nendlab) codlabel(true);	/* emit exit label */
}

/* LABCHK - returns true if parse tree contains any labels.
*/
static int
labchk(struct node * n)
{
    if (n == NULL) return 0;
    switch (n->Nop) {
	case Q_CASE:		/* These three are all labels */
	case N_LABEL:
	case Q_DEFAULT:
	    return 1;

	case N_STATEMENT:		/* Compound statement, scan it. */
	    if (n->Nleft && n->Nleft->Nop == N_DATA)	/* Skip auto inits */
		n = n->Nright;		/* move on to real statements */
	    for(; n != NULL; n = n->Nright) {
		if(n->Nop != N_STATEMENT)
		    int_error("labchk: bad stmt %N", n);
		if(n->Nleft && labchk(n->Nleft))
		    return 1;
	    }
	    return 0;

	case Q_IF:		/* Has two substatements */
	    return labchk(n->Nright->Nleft) || labchk(n->Nright->Nright);

	case Q_DO:		/* Standard substatements */
	case Q_FOR:
	case Q_SWITCH:
	case Q_WHILE:
	    return labchk(n->Nright);

	case Q_BREAK:		/* Not labels and no substatements */
	case Q_GOTO:
	case Q_CONTINUE:
	case Q_RETURN:
	case N_EXPRLIST:	/* Same as expression stmt */
	default:		/* None of above, assume expression stmt */
	    return 0;
    }
}

/* ------------------------- */
/*	while statement      */
/* ------------------------- */
static void
gwhile(struct node * n)
{
    SYMBOL *saveb, *savel;

    /* ok, we do, so we need to make a label for the top */
    savel = looplabel;
    looplabel = gtoplab();		/* Get new label and emit */

    /* now, see if there is a body or just the test */
    if (n->Nright == NULL) {
	n->Nleft->Nendlab = n->Nendlab;	/* propagate exit point */
	gboolean(n->Nleft, looplabel, 1); /* no body, just test */
    } else {
	saveb = brklabel;		/* full body, need another label */
	brklabel = (n->Nendlab != NULL)? n->Nendlab : newlabel();
	n->Nright->Nendlab = looplabel;	/* exit from body is to loop top */
	gboolean(n->Nleft, brklabel, 0);	/* first the test, if any */
	genstmt(n->Nright);		/* then the actual body */
	code6(P_JRST, NULL, looplabel);	/* body jumps back to test */
	if (n->Nendlab == NULL) codlabel(brklabel); /* emit end label */
	brklabel = saveb;		/* restore label for outer loop */
    }

    /* in either case we need to restore the outer loop top label */
    freelabel (looplabel);
    looplabel = savel;			/* fix the label */
}

/* GTOPLAB - auxiliary for loops which need to generate a label at top.
*/
static SYMBOL *
gtoplab(void)
{
    SYMBOL *lab;
    flushcode();		/* Ensure all previous code forced out */
    lab = newlabel();		/* Get new label */
    outlab(lab);		/* and emit it directly */
    return lab;
}

/* ---------------------- */
/*	do statement      */
/* ---------------------- */
static void
gdo(struct node * n)
{
    SYMBOL *saveb, *savel, *toplabel;

    toplabel = gtoplab();		/* Get new label and emit */
    saveb = brklabel;
    brklabel = (n->Nendlab != NULL) ? n->Nendlab : newlabel();

    if (n->Nright) {
	savel = looplabel;
	n->Nright->Nendlab = looplabel = newlabel();
	genstmt(n->Nright);
	codlabel(looplabel);
	looplabel = savel;		/* restore outer loop label */
    }

    if ((n->Nleft->Nop) != N_ICONST) {
	n->Nleft->Nendlab = brklabel;
	gboolean(n->Nleft, toplabel, 1);
    } else if (n->Nleft->Niconst) code6(P_JRST, NULL, toplabel);

    if (n->Nendlab == NULL) codlabel(brklabel);
    brklabel = saveb;			/* restore for outer breaks */
    freelabel (toplabel);		/* no more use for this one */
}

/* INDREF - Recognize the simple word-array reference array[i]. */
static int
indref(NODE *n, SYMBOL *iv, NODE **basep, SYMBOL **basesp)
{
    NODE *a, *b, *t;
    SYMBOL *s;

    if (n == NULL || n->Nop != N_PTR || n->Nleft == NULL
      || n->Nleft->Nop != Q_PLUS)
        return 0;
    a = n->Nleft->Nleft;
    b = n->Nleft->Nright;
    if (a == NULL || b == NULL)
        return 0;
    if (a->Nop == Q_IDENT && a->Nid == iv) {
        t = a; a = b; b = t;
    }
    if (a->Nop != Q_IDENT || b->Nop != Q_IDENT || b->Nid != iv)
        return 0;
    s = a->Nid;
    if (s == NULL || s->Stype == NULL || s->Stype->Tspec != TS_ARRAY
      || s->Stype->Tsubt == NULL || tisanyvolat(s->Stype->Tsubt)
      || a->Ntype == NULL
      || a->Ntype->Tspec != TS_PTR || sizeptobj(a->Ntype) != 1
      || tisbytearray(s->Stype))
        return 0;
    *basep = a;
    *basesp = s;
    return 1;
}

/* INDFINDREF - Find one suitable array[i] reference in a loop body. */
static int
indfindref(NODE *n, SYMBOL *iv, NODE **basep, SYMBOL **basesp, int *budget)
{
    if (n == NULL || --*budget < 0)
        return 0;
    if (indref(n, iv, basep, basesp))
        return 1;
    if (n->Nop == Q_IDENT || n->Nop == N_ICONST || n->Nop == N_FCONST
      || n->Nop == N_PCONST || n->Nop == N_SCONST || n->Nop == N_VCONST)
        return 0;
    return indfindref(n->Nleft, iv, basep, basesp, budget)
        || indfindref(n->Nright, iv, basep, basesp, budget);
}

/* INDBODYWRITES - Reject loops which can desynchronize the scalar IV from
** the maintained address.  Address-taken IVs are rejected by indloopmatch().
*/
static int
indbodywrites(NODE *n, SYMBOL *iv)
{
    if (n == NULL)
        return 0;
    switch (n->Nop) {
    case Q_ASGN:
    case Q_ASPLUS:
    case Q_ASMINUS:
    case Q_ASMPLY:
    case Q_ASDIV:
    case Q_ASMOD:
    case Q_ASLSH:
    case Q_ASRSH:
    case Q_ASAND:
    case Q_ASOR:
    case Q_ASXOR:
        if (n->Nleft && n->Nleft->Nop == Q_IDENT && n->Nleft->Nid == iv)
            return 1;
        break;
    case N_PREINC:
    case N_PREDEC:
    case N_POSTINC:
    case N_POSTDEC:
        if (n->Nleft && n->Nleft->Nop == Q_IDENT && n->Nleft->Nid == iv)
            return 1;
        break;
    default:
        break;
    }
    if (n->Nop == Q_IDENT || n->Nop == N_ICONST || n->Nop == N_FCONST
      || n->Nop == N_PCONST || n->Nop == N_SCONST || n->Nop == N_VCONST)
        return 0;
    return indbodywrites(n->Nleft, iv) || indbodywrites(n->Nright, iv);
}

/* INDBODYBRANCHES - Does a loop body contain control flow that can merge
** different register-spill states?
**
** The induction-pointer optimization deliberately keeps one VREG live across
** the complete loop body.  The ordinary statement generator predates that
** optimization and assumes no VREG is live across statement-level branches.
** A call in only one arm of an IF can therefore spill the induction VREG on
** that arm and leave the common continuation with an unmatched stack restore.
**
** Keep the optimization away from statement-level merge points.  Preserve
** the existing terminal IF optimization ("if (...) continue/break/goto"),
** because gif() lowers those directly to one conditional jump and creates no
** branch join.  This is a correctness precondition, not a profitability test.
*/
static int
indbodybranches(NODE *n)
{
    NODE *thenpart, *elsepart;

    if (n == NULL)
        return 0;
    switch (n->Nop) {
    case Q_IF:
        if (n->Nright == NULL)
            return 1;
        thenpart = n->Nright->Nleft;
        elsepart = n->Nright->Nright;
        if (elsepart == NULL && thenpart != NULL
          && (thenpart->Nop == Q_CONTINUE || thenpart->Nop == Q_BREAK
              || thenpart->Nop == Q_GOTO))
            return indbodybranches(n->Nleft);
        return 1;
    case Q_FOR:
    case Q_WHILE:
    case Q_DO:
    case Q_SWITCH:
        return 1;
    default:
        break;
    }
    if (n->Nop == Q_IDENT || n->Nop == N_ICONST || n->Nop == N_FCONST
      || n->Nop == N_PCONST || n->Nop == N_SCONST || n->Nop == N_VCONST)
        return 0;
    return indbodybranches(n->Nleft) || indbodybranches(n->Nright);
}

/* INDLOOPMATCH - Match the deliberately narrow strength-reduction case:
**
**     for (i = 0; i < constant; ++i) ... array[i] ...
**
** i must be a one-word nonvolatile local whose address never escapes.  The
** array element must occupy exactly one full word.  Labels in the body are
** excluded because a goto could enter below the derived-pointer initializer.
*/
static int
indloopmatch(NODE *n, NODE **basep, SYMBOL **basesp)
{
    NODE *pre, *pair, *init, *cond, *incr, *lhs;
    SYMBOL *iv;
    int budget;

    if (!optgen || n == NULL || n->Nop != Q_FOR || labchk(n->Nright))
        return 0;
    pre = n->Nleft;
    if (pre == NULL || pre->Nleft == NULL || pre->Nright == NULL)
        return 0;
    pair = pre->Nleft;
    init = pair->Nleft;
    cond = pair->Nright;
    incr = pre->Nright->Nleft;
    if (init == NULL || init->Nop != Q_ASGN || init->Nleft == NULL
      || init->Nleft->Nop != Q_IDENT || init->Nright == NULL
      || init->Nright->Nop != N_ICONST || init->Nright->Niconst != 0)
        return 0;
    iv = init->Nleft->Nid;
    if (iv == NULL || iv->Stype == NULL || (iv->Sflags & SF_ADDRTAKEN)
      || tisanyvolat(iv->Stype) || !tisinteg(iv->Stype)
      || sizetype(iv->Stype) != 1)
        return 0;
    if (cond == NULL || cond->Nop != Q_LESS || cond->Nleft == NULL
      || cond->Nleft->Nop != Q_IDENT || cond->Nleft->Nid != iv
      || cond->Nright == NULL || cond->Nright->Nop != N_ICONST)
        return 0;
    if (incr == NULL)
        return 0;
    lhs = incr->Nleft;
    if (!((incr->Nop == N_PREINC || incr->Nop == N_POSTINC)
          && lhs && lhs->Nop == Q_IDENT && lhs->Nid == iv)
      && !(incr->Nop == Q_ASPLUS && lhs && lhs->Nop == Q_IDENT
           && lhs->Nid == iv && incr->Nright
           && incr->Nright->Nop == N_ICONST && incr->Nright->Niconst == 1))
        return 0;
    if (indbodywrites(n->Nright, iv) || indbodybranches(n->Nright))
        return 0;
    budget = 1024;
    if (!indfindref(n->Nright, iv, basep, basesp, &budget))
        return 0;
    indvarsym = iv;
    return 1;
}

/* ----------------------- */
/*	for statement      */
/* ----------------------- */
static void
gfor(struct node * n)
{
    NODE *cond, *body, *incr, *init, *indbase;
    SYMBOL *saveb, *savel, *toplabel, *saveiv, *savebase, *ibase;
    VREG *saveptr, *newptr;
    int endtest, indactive;		/* safe to move test to end of loop */

    saveiv = indvarsym;
    savebase = indbasesym;
    saveptr = indptrreg;
    indbase = NULL;
    ibase = NULL;
    newptr = NULL;
    indactive = indloopmatch(n, &indbase, &ibase);

    cond = n->Nleft;
    body = n->Nright;
    incr = cond->Nright->Nleft;
    cond = cond->Nleft;
    init = cond->Nleft;
    cond = cond->Nright;

    /* See if conditional test can be put at the end of the loop, which is
    ** usually more efficient.  endtest is true if so.  Note that it
    ** will not be true if no test exists.
    */
    endtest = optgen &&			/* Never, if not optimizing. */
	cond &&				/* And only if have a condition. */
	((body == NULL && incr == NULL)	/* OK if no body or increment */
	 || istrue(cond, init));	/* or if test exists & is TRUE */

    if (init)
	{
	if (init->Nop == N_DATA)
	    genadata(init);
	else
	    genxrelease(init);
	}

    if (indactive) {
        newptr = genexpr(indbase);
        indbasesym = ibase;
        indptrreg = newptr;
    }

    toplabel = gtoplab();	/* Generate top-of-loop label and emit */
    saveb = brklabel;
    brklabel = (n->Nendlab != NULL) ? n->Nendlab : newlabel();

    savel = looplabel;			/* remember prev outer label */
    looplabel = (body == NULL || (incr == NULL && !endtest)) ?
		toplabel : newlabel();

     /* Normally generate test at start of loop */
    if (cond && !endtest)		/* If have condition, and at beg, */
	gboolean(cond, brklabel, 0);	/* generate test at start of loop */

    /* Now generate body */
    if (body != NULL) {			/* If we have one, of course */
	body->Nendlab = looplabel;
	genstmt(body);
	if (looplabel != toplabel) codlabel(looplabel);
    }

    /* Now generate increment */
    if (incr != NULL) {
	if (!endtest) incr->Nendlab = toplabel;
	genxrelease(incr);
        if (indactive)
            code1(P_ADD, indptrreg, 1);
    }

    /* Finally generate loop back to condition test at top, unless actually
    ** doing the test here.
    */
    if (endtest) {			/* If test comes at end of loop */
	cond->Nendlab = brklabel;	/* set end label for it */
	gboolean(cond,toplabel,1);	/* and gen the conditional jump */
    }
    else code6(P_JRST, NULL, toplabel);	/* Otherwise just loop */

    if (n->Nendlab == NULL)
	codlabel(brklabel);
    brklabel = saveb;			/* restore old break label */
    looplabel = savel;			/* restore outer loop continuation */
    freelabel(toplabel);		/* don't need top label any more */
    if (indactive) {
        vrfree(newptr);
        indvarsym = saveiv;
        indbasesym = savebase;
        indptrreg = saveptr;
    }
}

/*
 * greturn ()
 */

static
void
greturn (NODE *n)
    {
    INT		siz;
    VREG*	r;
    VREG*	r2;
    int		i = n->Nreg;
    if (optobj && deadjump ())		/* If in dead code, do nothing */
	return;

    if ((n = n->Nright) != NULL)
	{				/* If returning a value, set it */
	if (n->Ntype->Tspec == TS_ARRAY)
	    {				/* Just in case */
	    int_error("greturn: returning array");
	    return;
	    }

	siz = sizetype(n->Ntype);	/* Remember size */
	n->Nflag |= NF_RETEXPR;		/* Tell genexpr this is return value */

	if ((r = genexpr(n)) == NULL)
	    {				/* Make return expression */
	    if (!deadjump())		/* If no expr, must be in dead code */
		int_error("greturn: null vreg");

	    return;			/* Assume tail-recursed, so win! */
	    }

	if ((n->Ntype->Tspec == TS_STRUCT || n->Ntype->Tspec == TS_UNION)
	  && siz > 2 && siz <= GCCABI_RET_REGS)
	    {				/* GCC ABI: 3/4-word aggregate in AC1..AC4 */
	    int rr;
	    rr = vrreal(r);
	    code00(P_MOVE, R_SCRREG, rr);
	    flushcode();
	    for (rr = 0; rr < siz; ++rr) {
		codemdx(P_MOVE, R_RETVAL + rr, (SYMBOL *)NULL, rr, R_SCRREG);
		flushcode();
		}
	    vrfree(r);
	    }
	else if (siz == 1) 		/* Return one word, scalar or small aggregate */
	    coderetmove(r);
	else if (siz == 2)		/* Return two words, scalar or small aggregate */
	    {
	    if (tisdimode(n->Ntype))
		{
		if (vrreal(r) == R_RETVAL
		  && vrreal(VR2(r)) == R_RETDBL)
		    vrfree(r);
		else
		    code0 (P_DMOVE, VR_RETVAL, r);
                if (!tisunsign(n->Ntype)) {
                    /* KCC keeps live DImode low35-canonical.  Returning it
                    ** only needs to copy the high sign into the ABI bit.
                    */
                    code8(P_TLN+POF_ISSKIP+POS_SKPE, VR_RETVAL, 0400000L);
                    code8(P_TLO, VR_RETDBL, 0400000L);
                }
		}
	    else
		code0 (P_DMOVE, VR_RETVAL, r);
	    }
	else
	    {				/* Larger aggregate: hidden result ptr in AC1 */
	    r2 = vrget();
	    code13(P_MOVE, r2, -1 - stackoffset); /* Get addr of arg 0 */
	    code4(P_MOVE, r2, r2);	/* Get ptr to place to return block */
	    code4s(P_SMOVE, r2, r, 0, siz);	/* Copy the block */
	    vrfree(r2);
	    }
	}

    if (optobj)
	killstack ();			/* Flush spurious MOVEMs */

    if (retlabel)
	{
	/* The exact final lexical return falls directly into the epilogue
	** emitted after the function body.  Other returns still jump to the
	** shared label.
	*/
	if (n != retfall)
	    code6(P_JRST, NULL, retlabel);
	return;
	}

    genretepilog(i);
    }

/* GENRETINIT - Set up a shared epilogue for fixed-frame functions.
** Any normal, non-VLA function that has real frame teardown work can share
** that teardown among all returns.  A one-instruction POPJ-only leaf is left
** alone: adding a common label there would cost one extra word overall.
**
** VLA functions are deliberately excluded because return paths may arrive
** with different dynamic stack depths; their epilogue first restores SP from
** the fixed-frame anchor and remains on the historical direct-return path.
*/
void
genretinit(NODE *body)
{
    retlabel = (!isr && !fnvla_v11 &&
        (maxauto > 0 || _reg_count > 0 || fnsavescr || fnargregs > 0))
        ? newlabel() : NULL;
    retfall = retlabel ? laststmt(body) : NULL;
    if (retfall && retfall->Nop != Q_RETURN)
	retfall = NULL;
}

/* GENRETEPILOG - Emit the current function return sequence.
** If a shared return label is active, this is called once after the body.
** Otherwise greturn() calls it directly, preserving historical behavior.
*/
void
genretepilog(int i)
{
    int j;

    if (retlabel)
	{
	/* A shared epilogue can have many incoming return jumps.  Flush them
	** before emitting the label; the forward-label optimizer assumes a
	** much smaller local jump set and cannot safely fold this fan-in.
	*/
	codfallthrough(retlabel);
	flushcode();
	outlab(retlabel);
	freelabel(retlabel);
	retlabel = NULL;
	retfall = NULL;
	}

#if 0	/* qq Reg linkage */
    if (!fn_main && R_PRESERVE_COUNT >= i && i > 0)	/* Reg linkage */
	stackoffset -= i;

    printf ("stackoffset=%d i=%d\n", stackoffset, i);
#endif

    if (!isr)
	{
        if (fnvla_v11)
            {
            /* Discard all dynamic VLA storage before touching fixed save
            ** slots.  AC14 points at the top of the fixed frame. */
            code00(P_MOVE, R_SP, R_MAXREG);
            flushcode();
            }
	/* Restore allocated call-preserved registers directly from their
	** fixed save slots before dropping the frame.  The old epilogue first
	** discarded the frame and then POPed these registers, which read from
	** the caller's stack instead of the save slots whenever register
	** variables were actually used.
	*/
	if (R_PRESERVE_COUNT >= i) {
	    if (i == 4) {
		int off = 1 + fnsavescr + (fnvla_v11 ? 1 : 0) - stackoffset;

		/* Four consecutive preserved ACs are cheaper to restore with
		** one BLT than with four individual MOVEs.  AC0 is caller-
		** clobbered and is free at a normal C return boundary.
		*/
		flushcode();
		outstr("\tMOVEI\t0,");
		outnum(r_maxnopreserve + 1);
		outstr("\n\tHRLI\t0,");
		outnum(off);
		outstr("(17)\n\tBLT\t0,");
		outnum(r_maxnopreserve + i);
		outstr("\n");
	    } else
		for (j = 0; j < i; ++j) {
		    codemdx(P_MOVE, j + r_maxnopreserve + 1, (SYMBOL *)NULL,
			    1 + fnsavescr + (fnvla_v11 ? 1 : 0) + j - stackoffset, R_SP);
		    flushcode();
		}
	}

	if (fnsavescr)
	    {
	    /* Restore AC16, saved immediately above the return PC. */
	    codemdx(P_MOVE, R_SCRREG, (SYMBOL *)NULL,
		    1 - stackoffset, R_SP);
	    flushcode();
	    }
        if (fnvla_v11)
            {
            codemdx(P_MOVE, R_MAXREG, (SYMBOL *)NULL,
                    1 + fnsavescr - stackoffset, R_SP);
            flushcode();
            }
	}

    code8 (P_ADJSP, VR_SP, -stackoffset); /* flush local vars from stk */

#if !HOST_DAIMOS
    if (profbliss)
	{				/* for BLISS profiler */
	flushcode ();
	outepilog (curfn);		/* added 09/15/89 by MVS */
	}
#endif

    if (isr)				/* FW 2A(52) */
	{
	flushcode ();			
	outiepilog ();
	}
    else if (fnargregs)
	{
	/* The external ABI leaves register arguments in AC1..AC4.  The
	** KCC callee prologue inserted private stack copies below the return
	** PC so the old parameter-addressing code could remain unchanged.
	** Remove those copies before transferring control to the caller.
	*/
	code00(P_POP, R_SP, R_ABITMP);
	flushcode();
	code8(P_ADJSP, VR_SP, -fnargregs);
	flushcode();
	codemdx(P_JRST, 0, (SYMBOL *)NULL, 0, R_ABITMP);
	}
    else
	code5 (P_POPJ, VR_SP);		/* emit the return */
    }
