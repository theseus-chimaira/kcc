/*	CCGEN.C - Generate code for parse-tree data declarations
**
**	(c) Copyright Ken Harrenstien 1989
**		All changes after v.221, 25-Apr-1988
**	(c) Copyright Ken Harrenstien, SRI International 1985, 1986
**		All changes after v.84, 8-Aug-1985
**
**	Original version (C) 1981  K. Chen
*/

#include "cc.h"
#include "ccgen.h"
#include "ccchar.h"
#include <string.h>
#if HOST_UNIX
#include <stdlib.h>
#endif

/* Imported (external) functions used herein */
extern NODE *labeldiff_v61o(NODE *);
extern void ridlsym(SYMBOL *);
extern void vlaclear_v12(void);				/* CCSYM */
extern SYMBOL *newlabel(void);
extern void freelabel(SYMBOL *);
extern INT sizearray(TYPE *), sizetype(TYPE *), sizeptobj(TYPE *);	/* CCSYM */
extern int elembsize(TYPE *);
extern void code5(int, VREG *), 
	code6(int, VREG *, SYMBOL *), codemdx(int, int, SYMBOL *, INT, int), 
        code00 (int, int, int), code12 (int, VREG *, INT),
	code8(int, VREG *, INT), codestr(char *, int), flushcode(void),
	codefnreset(void);	/* CCCODE */
extern VREG *vrget(void);
extern void genstmt(NODE *), genretinit(NODE *), genretepilog(int);
extern void genxrelease(NODE *);
extern void				/* CCOUT */
	outmidef(SYMBOL *), outmiref(SYMBOL *), outid(char *), 
	outptr(SYMBOL *, int, INT), outscon(char *, int, int), 
	outlab(SYMBOL *), outnum(INT), outnl(void), 
	outstr(char *);
extern int				/* CCOUT */
	outflt(int, INT *, int), codeseg(void), dataseg(void), bssseg(void),
	prevseg(int);
extern void vrinit(void);			/* CCREG.C */
extern void vrendchk (void);
extern void vrfree (VREG *);
extern void gccabi_dimode_normalize_reg(int);
extern void gccabi_dimode_encode_regs(int, int);

#if SYS_CSI
extern
void	    outiprolog (void);		/* FW 2A(52) */
#endif

extern void outpghdr(void);				/* CCOUT */
#if SYS_CSI
extern void outprolog(SYMBOL *);			/* CCOUT */
#endif

/* Exported functions defined here */
void gencode(NODE *n);		/* Called by CC mainline */

/* Internal Functions */
static void inicode(void),
	    endcode(void), 
	    gccabi_arg_prologue(void),
	    gendata(NODE *), 
	    genfunct(NODE *), 
	    gliterals(void), 
	    giz(NODE *, TYPE *, SYMBOL *), 
	    gizword(NODE *, TYPE *, SYMBOL *), 
	    giznull(TYPE *), 
	    gizexpr(NODE *, TYPE *);
static int  gizptr(NODE *), fn_needs_scrreg(NODE *),
    fn_direct_call_no_scrreg(NODE *), fn_dimode_direct_expr(NODE *);
static int  fn_addr_input(NODE *, NODE **);
static INT  gizconst(NODE *);

static void gizlist(NODE *, TYPE *, SYMBOL *);
static int gizpackedtype(TYPE *);
static INT gizpackedbytes(TYPE *);
static INT gizpackedbits(TYPE *);
static void gizpacked(NODE *, TYPE *, SYMBOL *);
static void gizpackedscalar(NODE *, TYPE *, SYMBOL *);
static void gizpackedputbits(unsigned INT, int);
static void gizfieldbits_v61i(NODE *, int);
static void outdiff_v61o(NODE *, int);
static void outbyteprefix_v61o(int);
static void outdiffbits_v61o(NODE *, int);
static void gizbytes(NODE *, TYPE *, SYMBOL *, int);
static void bytbeg(int), 
	    wdalign(void), 
	    outval(INT),
	    outbyte(INT, int),
	    outzbs(INT), 
	    outzwds(INT);
static INT  bytend(void);
#if 0
static void inicode(), endcode(), gendata(), genfunct(), gliterals();
static void giz(), gizword(), giznull(), gizexpr();
static int gizptr(), gizconst();

static void gizlist();
static void gizbytes();
static void bytbeg(), wdalign(), outval(), outbyte(),
	outzbs(), outzwds();
static INT bytend();
#endif

/* OUT data emission vars. */
static int bsiz;	/* 0 if in word mode, else byte size in bits */
static int bpw;		/* # bytes per word */
static int bpos;	/* P of last byte deposited (TGSIZ_WORD at beg of wd)*/
static INT savlct;	/* Saved loc ctr at start of byte mode */
static INT locctr;	/* Location counter (only for tracking # wds output) */

/* Number of incoming GCC-ABI argument words reconstructed below the
** return PC for the current KCC-generated function.  The epilogue removes
** exactly these shim words before returning to the caller.
*/


/* Normalize GCC-ABI DImode argument pairs for KCC's internal
** high36:low35 representation.  GCC duplicates the sign into bit 35 of
** the low word; KCC arithmetic keeps that bit clear.  Stack-resident low
** words are normalized when loaded by gdimemload().
*/
static void
gccabi_normalize_dimode_regs(void)
{
    TYPE *p, *t;
    int cum, siz, lowreg;

    cum = (sizetype(curfn->Stype->Tsubt) > GCCABI_RET_REGS) ? 1 : 0;
    p = curfn->Stype->Tproto ? curfn->Stype->Tproto : curfn->Shproto;
    while (p && p->Tspec == TS_PARAM) {
        t = p->Tsubt;
        siz = sizetype(t);
        if (siz == 2 && tisdimode(t)) {
            lowreg = 0;
            if (cum < GCCABI_ARG_REGS - 1)
                lowreg = cum + 2;
            else if (cum == GCCABI_ARG_REGS - 1)
                lowreg = GCCABI_ARG_REGS;
            if (lowreg)
                gccabi_dimode_normalize_reg(lowreg);
        }
        cum += siz;
        p = p->Tproto;
    }
    flushcode();
}

/* GCCABI_ARG_PROLOGUE - Reconstruct KCC's internal parameter stack view.
**
** The external C ABI is the PDP-10 GCC ABI: the first four named scalar
** argument words arrive in AC1..AC4 and remaining words arrive on the
** stack.  KCC's existing expression generator still addresses parameters
** through its historical stack offsets.  Until that internal representation
** is removed, insert the register words below the return PC on entry.
**
** This is not an alternate ABI.  It is a private callee implementation
** detail; callers and externally visible functions use only the GCC ABI.
*/
static void
gccabi_arg_prologue(void)
{
    TYPE *p, *t;
    int regat[8];
    int nreg, cum, siz, slot, oldidx, last, src, i;

    memset(regat, 0, sizeof(regat));
    nreg = 0;
    cum = 0;
    last = -1;

    /* Large aggregate results use a hidden destination pointer in AC1.
    ** In KCC's historical private stack view this is argument word zero.
    */
    if (sizetype(curfn->Stype->Tsubt) > GCCABI_RET_REGS) {
        regat[0] = 1;
        nreg = 1;
        cum = 1;
        last = 0;
    }

    p = curfn->Stype->Tproto ? curfn->Stype->Tproto : curfn->Shproto;
    while (p && p->Tspec == TS_PARAM) {
        t = p->Tsubt;
        siz = sizetype(t);

        /* Aggregates larger than two words are entirely stack-passed.
        ** Other arguments use each ABI register slot that lies in AC1..AC4.
        ** oldidx is the word's position in KCC's historical top-down stack
        ** layout; multiword objects therefore appear in reverse word order.
        */
        if (!((t->Tspec == TS_STRUCT || t->Tspec == TS_UNION) && siz > 2)) {
            if (siz == 2 && cum == GCCABI_ARG_REGS - 1) {
                /* GCC splits a two-word argument at the AC4 boundary by
                ** putting its second word in AC4 and its first on the stack.
                */
                oldidx = cum;
                regat[oldidx] = 4;
                ++nreg;
                if (oldidx > last)
                    last = oldidx;
            } else {
                for (i = 0; i < siz; ++i) {
                    slot = cum + i;
                    if (slot >= GCCABI_ARG_REGS)
                        break;
                    oldidx = cum + siz - 1 - i;
                    if (oldidx >= (int)(sizeof(regat) / sizeof(regat[0]))) {
                        int_error("gccabi_arg_prologue: bad register word index");
                        break;
                    }
                    regat[oldidx] = slot + 1;
                    ++nreg;
                    if (oldidx > last)
                        last = oldidx;
                }
            }
        }
        cum += siz;
        p = p->Tproto;
    }

    fnargregs = nreg;
    if (!nreg)
        return;

    /* At entry the ABI stack contains only stack-passed argument words,
    ** with the return PC on top.  Remove the PC into the reserved scratch
    ** register, make room for the missing register words, then merge AC1..AC4
    ** into their exact old KCC stack positions.  Processing from the top
    ** down makes each overlapping stack move safe.  Words below the last
    ** inserted register stay put.
    */
    code00(P_POP, R_SP, R_ABITMP);
    flushcode();
    code8(P_ADJSP, VR_SP, nreg);
    flushcode();
    src = 0;
    for (i = 0; i <= last; ++i) {
        if (regat[i]) {
            /* KCC's private stack view lists a two-word ABI argument low
            ** word first.  When the next slot contains the preceding AC,
            ** store the complete pair directly.  CCOUT expands DMOVEM for
            ** PDP-6/KA10 and emits the native instruction on KI10 and later.
            */
            if (i < last && regat[i + 1] != 0
              && regat[i] == regat[i + 1] + 1) {
                codemdx(P_DMOVEM, regat[i + 1], (SYMBOL *)NULL,
                        -(i + 1), R_SP);
                flushcode();
                ++i;
            } else {
                codemdx(P_MOVEM, regat[i], (SYMBOL *)NULL, -i, R_SP);
                flushcode();
            }
        } else {
            codemdx(P_MOVE, R_MAX_NOPRESERVE, (SYMBOL *)NULL,
                    -(nreg + src), R_SP);
            flushcode();
            codemdx(P_MOVEM, R_MAX_NOPRESERVE, (SYMBOL *)NULL, -i, R_SP);
            flushcode();
            ++src;
        }
    }
    code00(P_PUSH, R_SP, R_ABITMP);
    flushcode();
}


/* FN_DIRECT_CALL_NO_SCRREG - True when direct call setup cannot use AC16.
** The direct GCC ABI path handles up to four one-word arguments entirely in
** AC1..AC4.  Register-copy cycles use a normal KCC virtual register, so the
** call setup itself no longer needs the dedicated AC16 output scratch.
** Argument expressions are still traversed by fn_needs_scrreg().
*/
static int
fn_direct_call_no_scrreg(NODE *n)
{
    NODE *a, *work[16];
    TYPE *p;
    int sp, nargs;

    if (n == NULL || n->Nleft == NULL || n->Nleft->Ntype == NULL)
	return 0;
    /* Indirect calls pin their target in AC16 while AC1..AC4 are loaded.
    ** The containing function must therefore preserve the ABI scratch AC.
    */
    if (n->Nleft->Nop != Q_IDENT)
	return 0;
    if (n->Nleft->Nop == Q_IDENT
      && (n->Nleft->Nid->Sflags & (TF_BLISS | TF_FORTRAN | TF_INTERRUPT)))
	return 0;

    p = n->Nleft->Ntype->Tproto;
    while (p && p->Tspec == TS_PARAM)
	p = p->Tproto;
    if (p && p->Tspec == TS_PARINF)
	return 0;

    a = n->Nright;
    if (a == NULL)
	return 1;

    sp = 0;
    nargs = 0;
    work[sp++] = a;
    while (sp > 0)
	{
	a = work[--sp];
	if (a == NULL)
	    continue;
	if (a->Nop == N_EXPRLIST)
	    {
	    if (sp + 2 > (int)(sizeof(work) / sizeof(work[0])))
		return 0;
	    work[sp++] = a->Nright;
	    work[sp++] = a->Nleft;
	    continue;
	    }
	if (++nargs > 12 || a->Ntype == NULL
	  || sizetype(a->Ntype) != 1)
	    return 0;
	}
    return 1;
}

/* FN_ADDR_INPUT - Find the expression gaddress() actually evaluates.
**
** Address formation for an aggregate does not evaluate the aggregate value.
** Return the one scalar expression that gaddress() feeds to genexpr(), or
** NULL when a plain identifier supplies the address directly.  Unknown forms
** stay conservative.
*/
static int
fn_addr_input(NODE *n, NODE **expr)
{
    while (n)
	{
	switch (n->Nop)
	    {
	    case Q_IDENT:
		*expr = NULL;
		return 1;

	    case Q_DOT:
		n = n->Nleft;
		continue;

	    case Q_MEMBER:
	    case N_PTR:
	    case Q_ASPLUS:
	    case Q_PLUS:
	    case Q_ASMINUS:
	    case Q_MINUS:
		*expr = n->Nleft;
		return 1;

	    default:
		return 0;
	    }
	}
    return 0;
}

/* FN_DIMODE_DIRECT_EXPR - Classify one AC16-free direct DImode tree.
** Return 1 when the tree is composed only of a consumed ABI pair, constants,
** and overlap-safe binary operations.  Return 0 for a constant-only tree and
** -1 for every other form.  The caller skips an accepted tree atomically.
*/
static int
fn_dimode_direct_expr(NODE *n)
{
    int l, r;

    if (n == NULL || n->Ntype == NULL || sizetype(n->Ntype) != 2)
	return -1;
    switch (n->Nop)
	{
	case Q_IDENT:
	    return (n->Nid && (n->Nid->Sflags & SF_ABICONSUME)) ? 1 : -1;
	case N_ICONST:
	    return 0;
	case Q_LSHFT:
	case Q_RSHFT:
	    l = fn_dimode_direct_expr(n->Nleft);
	    if (l <= 0 || n->Nright == NULL
	      || (n->Nright->Nop != Q_IDENT && n->Nright->Nop != N_ICONST))
		return -1;
	    return l;
	case Q_PLUS:
	case Q_MINUS:
	case Q_MPLY:
	case Q_ANDT:
	case Q_XORT:
	case Q_OR:
	    l = fn_dimode_direct_expr(n->Nleft);
	    r = fn_dimode_direct_expr(n->Nright);
	    if (l < 0 || r < 0)
		return -1;
	    return l || r;
	default:
	    return -1;
	}
}


/* Return nonzero when a DImode constant divisor is handled without the
** general restoring divider. */
static int
fn_dimode_simple_divisor(NODE *n)
{
    INT hi, lo, mhi, mlo;

    if (n == NULL || n->Nop != N_ICONST)
        return 0;
    dimode_iconst_words(n, &hi, &lo);
    hi &= dimode_hi36mask();
    lo &= dimode_lo35mask();
    if ((hi == 0 && (lo == 1 || (lo && (lo & (lo - 1)) == 0))))
        return 1;
    if (hi == dimode_hi36mask() && lo == dimode_lo35mask())
        return 1;

    /* Convert a negative target value to magnitude and recognize -2^n. */
    if ((hi & ((INT)1 << 35)) != 0)
        {
        mhi = -hi;
        if (lo != 0)
            --mhi;
        mlo = -lo;
        mhi &= dimode_hi36mask();
        mlo &= dimode_lo35mask();
        if ((mhi == 0 && mlo && (mlo & (mlo - 1)) == 0)
          || (mlo == 0 && mhi && (mhi & (mhi - 1)) == 0))
            return 1;
        }
    return lo == 0 && hi && (hi & (hi - 1)) == 0;
}

/* Count general DImode divide/remainder nodes in one function.  This bounded
** tree walk is only a size-selection prepass; it creates no persistent
** dataflow or liveness state. */
static int
fn_dimode_divmod_count(NODE *root)
{
    NODE *work[128], *n;
    int sp, visits, counts, op, kind, shift, value;

    if (root == NULL)
        return 0;
    sp = 0;
    visits = 0;
    counts = 0;
    work[sp++] = root;
    while (sp > 0)
        {
        n = work[--sp];
        if (n == NULL)
            continue;
        if (++visits > 4096)
            return 0;       /* Stay conservative: keep dividers inline. */
        op = n->Nop;
        if ((op == Q_DIV || op == Q_MOD || op == Q_ASDIV || op == Q_ASMOD)
          && n->Ntype && tisdimode(n->Ntype)
          && !fn_dimode_simple_divisor(n->Nright))
            {
            kind = ((op == Q_MOD || op == Q_ASMOD) ? 1 : 0)
                 + (tspisunsigned(n->Ntype->Tspec) ? 2 : 0);
            shift = kind * 4;
            value = (counts >> shift) & 017;
            if (value < 017)
                counts += 1 << shift;
            }

        switch (op)
            {
            case Q_IDENT:
            case N_ICONST:
            case N_PCONST:
            case N_ECONST:
            case N_VCONST:
            case N_RETADDR:
            case N_FCONST:
            case N_SCONST:
            case Q_BREAK:
            case Q_CONTINUE:
            case Q_GOTO:
                continue;
            default:
                break;
            }
        if (n->Nleft)
            {
            if (sp >= (int)(sizeof(work) / sizeof(work[0])))
                return 0;
            work[sp++] = n->Nleft;
            }
        if (n->Nright)
            {
            if (sp >= (int)(sizeof(work) / sizeof(work[0])))
                return 0;
            work[sp++] = n->Nright;
            }
        }
    return counts;
}

/* FN_NEEDS_SCRREG - Conservatively decide whether a function may use AC16.
**
** This is deliberately not a data-flow pass.  It uses a fixed work stack and
** a hard visit budget, so both host memory and compile time stay bounded when
** KCC self-hosts.  Any uncertain construct simply keeps the normal AC16 save.
*/
static int
fn_needs_scrreg(NODE *root)
{
    NODE *work[128], *n;
    int sp, visits, op;

    if (root == NULL)
	return 0;
    sp = 0;
    visits = 0;
    work[sp++] = root;
    while (sp > 0)
	{
	n = work[--sp];
	if (n == NULL)
	    continue;
	if (++visits > 4096)
	    return 1;

	/* N_DATA nodes inside a function describe automatic initializers.
	** The declaration wrapper itself emits no expression code and may carry
	** an aggregate type that would otherwise look like a multiword operation.
	** Scan only ordinary scalar initializer expressions.  Brace-enclosed or
	** multiword initializers stay conservative because genadata() uses bulk
	** load/store machinery for them.
	*/
	if (n->Nop == N_DATA)
	    {
	    NODE *d, *iz;

	    for (d = n; d && d->Nop == N_DATA; d = d->Nright)
		{
		if (d->Nleft == NULL || d->Nleft->Nright == NULL)
		    continue;
		iz = d->Nleft->Nright;
		if (iz->Nop == N_IZLIST || d->Nleft->Nleft == NULL
		  || d->Nleft->Nleft->Ntype == NULL
		  || sizetype(d->Nleft->Nleft->Ntype) != 1)
		    return 1;
		if (sp >= (int)(sizeof(work) / sizeof(work[0])))
		    return 1;
		work[sp++] = iz;
		}
	    continue;
	    }

	/* Address formation does not evaluate an aggregate value.  Follow the
	** same base-address cases as gaddress() so &array[i] and scalar members
	** reached through aggregate lvalues do not force an AC16 save merely
	** because their addressable object spans several words.
	*/
	if (n->Nop == N_ADDR ||
	    (n->Nop == Q_DOT && n->Nleft
	     && (n->Nleft->Nflag & NF_LVALUE) && n->Ntype
	     && sizetype(n->Ntype) == 1))
	    {
	    NODE *a = NULL;
	    NODE *addr = (n->Nop == N_ADDR) ? n->Nleft : n;

	    if (!fn_addr_input(addr, &a))
		return 1;
	    if (a)
		{
		if (sp >= (int)(sizeof(work) / sizeof(work[0])))
		    return 1;
		work[sp++] = a;
		}
	    if (n->Nop == Q_DOT && n->Nright)
		{
		if (sp >= (int)(sizeof(work) / sizeof(work[0])))
		    return 1;
		work[sp++] = n->Nright;
		}
	    continue;
	    }

	op = n->Nop;
	if (n->Ntype && sizetype(n->Ntype) > 1)
	    {
	    /* Accepted direct-pair expressions cannot reach an AC16-using
	    ** expansion.  Skip the complete tree so its constant children are
	    ** not mistaken for unrelated multiword operations.
	    */
	    if (fnabidirect && fn_dimode_direct_expr(n) > 0)
		continue;
	    return 1;
	    }

	if (op == N_FNCALL)
	    {
	    if (!fn_direct_call_no_scrreg(n))
		return 1;
	    /* A direct function identifier itself generates no code.  An
	    ** indirect target does, so scan that expression as well as the
	    ** argument tree for real AC16 users.
	    */
	    if (n->Nright)
		{
		if (sp >= (int)(sizeof(work) / sizeof(work[0])))
		    return 1;
		work[sp++] = n->Nright;
		}
	    if (n->Nleft->Nop != Q_IDENT)
		{
		NODE *a = NULL;

		if (!fn_addr_input(n->Nleft, &a))
		    return 1;
		if (a)
		    {
		    if (sp >= (int)(sizeof(work) / sizeof(work[0])))
			return 1;
		    work[sp++] = a;
		    }
		}
	    continue;
	    }
	switch (op)
	    {
	    case N_NODE:
	    case N_STATEMENT:
	    case Q_RETURN:
	    case Q_BREAK:
	    case Q_CONTINUE:
	    case Q_GOTO:
	    case Q_IF:
	    case Q_FOR:
	    case Q_DO:
	    case Q_WHILE:
	    case Q_CASE:
	    case Q_DEFAULT:
	    case N_LABEL:
	    case N_EXPRLIST:
	    case Q_IDENT:
	    case N_ICONST:
	    case N_PCONST:
	    case N_ECONST:
	    case N_VCONST:
	    case N_RETADDR:
	    case N_CAST:
	    case N_ADDR:
	    case N_PREINC:
	    case N_PREDEC:
	    case N_POSTINC:
	    case N_POSTDEC:
	    case Q_COMPL:
	    case Q_NOT:
	    case N_NEG:
	    case N_PTR:
	    case Q_DOT:
	    case Q_MEMBER:
	    case Q_PLUS:
	    case Q_MINUS:
	    case Q_LESS:
	    case Q_GREAT:
	    case Q_LEQ:
	    case Q_GEQ:
	    case Q_EQUAL:
	    case Q_NEQ:
	    case Q_ANDT:
	    case Q_XORT:
	    case Q_OR:
	    case Q_LAND:
	    case Q_LOR:
	    case Q_QUERY:
	    case Q_ASGN:
	    case Q_ASPLUS:
	    case Q_ASMINUS:
	    case Q_ASAND:
	    case Q_ASXOR:
	    case Q_ASOR:
	    case Q_LSHFT:
	    case Q_RSHFT:
	    case Q_ASLSH:
	    case Q_ASRSH:
	    case Q_MPLY:
	    case Q_ASMPLY:
	    case Q_DIV:
	    case Q_ASDIV:
	    case Q_MOD:
	    case Q_ASMOD:
	    case Q_SWITCH:
		if ((op == Q_DIV || op == Q_ASDIV
		  || op == Q_MOD || op == Q_ASMOD)
		  && n->Ntype && tspisunsigned(n->Ntype->Tspec))
		    return 1;
		break;
	    default:
		return 1;
	    }

	if ((op == N_PREINC || op == N_PREDEC ||
	     op == N_POSTINC || op == N_POSTDEC) &&
	    n->Ntype && tisbytepointer(n->Ntype) &&
	    n->Nleft && Register_Id(n->Nleft))
	    {
	    int steps = (int)sizeptobj(n->Ntype);

	    /* gincdec() emits small positive register byte-pointer steps as
	    ** IBP directly, so those trees do not touch the reserved AC16.
	    ** Negative and larger adjustments still use ADJBP and remain
	    ** conservative here.
	    */
	    if ((op == N_PREINC || op == N_POSTINC) && steps > 0
	      && (steps == 1 || (tgcpu <= TGCPU_KI && steps <= 3)))
		;
	    else
		return 1;
	    }

	if ((op == Q_PLUS || op == Q_MINUS ||
	     op == Q_ASPLUS || op == Q_ASMINUS) &&
	    ((n->Nleft && n->Nleft->Ntype &&
	      tisbytepointer(n->Nleft->Ntype)) ||
	     (n->Nright && n->Nright->Ntype &&
	      tisbytepointer(n->Nright->Ntype))))
	    return 1;

	if (n->Nleft)
	    {
	    if (sp >= (int)(sizeof(work) / sizeof(work[0])))
		return 1;
	    work[sp++] = n->Nleft;
	    }
	if (n->Nright)
	    {
	    if (sp >= (int)(sizeof(work) / sizeof(work[0])))
		return 1;
	    work[sp++] = n->Nright;
	    }
	}
    return 0;
}

/* Static inline bodies cannot be emitted eagerly: a function may never
** become reachable, and its calls or data references must not become
** link-time dependencies.  On hosted cross builds, spool each candidate's
** generated assembly to disk and keep only a small symbol-reference graph
** in RAM.  The graph contains references from ACTUALLY emitted machine
** operands, not the parser's pre-optimization Srefs counters.  This is
** particularly important for function-address comparisons and dead code.
** Native KGEN output remains unchanged until its own file-service spool
** integration is available; it must not gain an implicit hosted tmpfile.
*/
#if HOST_UNIX
struct inline_ref_v32 {
    struct inline_ref_v32 *next;
    SYMBOL *symbol;
};

struct inline_def_v32 {
    struct inline_def_v32 *next;
    struct inline_ref_v32 *refs;
    SYMBOL *symbol;
    long start;
    long length;
    int live;
};

static FILE *inline_spool_v32;
static FILE *inline_parent_v32;
static struct inline_def_v32 *inline_head_v32, *inline_tail_v32;
static struct inline_def_v32 *inline_active_v32;
static int inline_parent_segment_v32;

extern int kcc_inline_segment_v32(void);
extern void kcc_inline_set_segment_v32(int);

/* Called by CCOUT only for symbols appearing in actual assembly operands.
** References inside an inactive candidate must not mark global symbols
** live until that candidate itself becomes reachable.
*/
void
kcc_inline_ref_v32(SYMBOL *s)
{
    struct inline_ref_v32 *ref;

    if (s == NULL || (s->Sclass != SC_INTDEF && s->Sclass != SC_INTREF
      && s->Sclass != SC_EXTREF && s->Sclass != SC_XEXTREF))
        return;

    if (inline_active_v32 == NULL) {
        s->Sflags |= SF_ASMREF;
        return;
    }
    if (s == inline_active_v32->symbol)
        return;
    for (ref = inline_active_v32->refs; ref; ref = ref->next)
        if (ref->symbol == s)
            return;
    ref = (struct inline_ref_v32 *)malloc(sizeof(*ref));
    if (ref == NULL) {
        int_error("KCC inline spool: cannot allocate dependency entry");
        return;
    }
    ref->symbol = s;
    ref->next = inline_active_v32->refs;
    inline_active_v32->refs = ref;
}

static int
kcc_inline_begin_v32(SYMBOL *s)
{
    struct inline_def_v32 *rec;

    if (inline_spool_v32 == NULL) {
        inline_spool_v32 = tmpfile();
        if (inline_spool_v32 == NULL) {
            int_error("KCC inline spool: cannot create temporary file");
            return 0;
        }
    }
    rec = (struct inline_def_v32 *)calloc(1, sizeof(*rec));
    if (rec == NULL || fseek(inline_spool_v32, 0L, SEEK_END) != 0) {
        free(rec);
        int_error("KCC inline spool: cannot append definition");
        return 0;
    }
    rec->start = ftell(inline_spool_v32);
    if (rec->start < 0) {
        free(rec);
        int_error("KCC inline spool: cannot determine output offset");
        return 0;
    }
    rec->symbol = s;
    if (inline_tail_v32)
        inline_tail_v32->next = rec;
    else
        inline_head_v32 = rec;
    inline_tail_v32 = rec;

    inline_parent_v32 = out;
    inline_parent_segment_v32 = kcc_inline_segment_v32();
    inline_active_v32 = rec;
    out = inline_spool_v32;
    kcc_inline_set_segment_v32(1);
    outstr("\n\t.text\n");
    return 1;
}

static void
kcc_inline_end_v32(void)
{
    long end;

    if (fflush(inline_spool_v32) != 0)
        int_error("KCC inline spool: failed writing definition");
    end = ftell(inline_spool_v32);
    if (end < inline_active_v32->start)
        int_error("KCC inline spool: invalid definition length");
    else
        inline_active_v32->length = end - inline_active_v32->start;
    out = inline_parent_v32;
    kcc_inline_set_segment_v32(inline_parent_segment_v32);
    inline_active_v32 = NULL;
}

/* Called just before OUTDONE: emit a closure of genuinely referenced
** candidate functions and their own references.  No AST is retained,
** and a recursive/cyclic static-inline call graph is handled by the
** monotone visited flag.  Replaying a definition is a raw disk copy;
** its reference edges were already recorded during code generation.
*/
void
kcc_inline_finish_v32(void)
{
    struct inline_def_v32 *rec, *next;
    struct inline_ref_v32 *ref, *rnext;
    int progress, emitted;
    char buf[1024];

    if (inline_spool_v32 == NULL)
        return;

    do {
        progress = 0;
        for (rec = inline_head_v32; rec; rec = rec->next) {
            if (rec->live || !(rec->symbol->Sflags & SF_ASMREF))
                continue;
            rec->live = 1;
            ++progress;
            for (ref = rec->refs; ref; ref = ref->next)
                ref->symbol->Sflags |= SF_ASMREF;
        }
    } while (progress);

    emitted = 0;
    for (rec = inline_head_v32; rec; rec = rec->next) {
        long left;

        if (!rec->live)
            continue;
        if (fseek(inline_spool_v32, rec->start, SEEK_SET) != 0) {
            int_error("KCC inline spool: cannot read definition");
            break;
        }
        left = rec->length;
        while (left > 0) {
            size_t count = left > (long)sizeof(buf)
                ? sizeof(buf) : (size_t)left;
            if (fread(buf, 1, count, inline_spool_v32) != count
              || fwrite(buf, 1, count, out) != count) {
                int_error("KCC inline spool: failed replaying definition");
                left = 0;
                break;
            }
            left -= (long)count;
        }
        emitted = 1;
    }
    if (emitted) {
        outstr("\n\t.text\n");
        kcc_inline_set_segment_v32(1);
    }
    for (rec = inline_head_v32; rec; rec = next) {
        next = rec->next;
        for (ref = rec->refs; ref; ref = rnext) {
            rnext = ref->next;
            free(ref);
        }
        free(rec);
    }
    fclose(inline_spool_v32);
    inline_spool_v32 = NULL;
    inline_head_v32 = inline_tail_v32 = inline_active_v32 = NULL;
}
#endif /* HOST_UNIX */

/* GENCODE - Generate code/data from parse-tree node
*/
void
gencode(NODE *n)
{
    if (n)				/* Ignore null stmts/defs */
	switch (n->Nop) {
	    case N_DATA:
		if (!nerrors) gendata(n); 	/* Generate data definition */
		break;
	    case N_FUNCTION:
		if (!nerrors) {
#if HOST_UNIX
		    SYMBOL *s = n->Nleft->Nleft->Nid;
		    int deferred = (s->Sclass == SC_INTDEF
		        && (s->Sflags & SF_INLINE))
		        ? kcc_inline_begin_v32(s) : 0;
#endif
		    genfunct(n);
#if HOST_UNIX
		    if (deferred)
		        kcc_inline_end_v32();
#endif
		}
                vlaclear_v12();          /* Release per-function VLA metadata */
		ridlsym((SYMBOL *)NULL);	/* Flush any local symbols */
		break;
	    default:
		int_error("gencode: bad node %N", n);
 	}
}

/*
 * genfunct ()
 *
 * - Generate machine instruction code for function
 */

static
void
genfunct (NODE* n)
    {
    extern
    int		_word_cnt;	/* KAR-2/91, from CCOUT # wds in function */
    int		i;
    int		bltsave, nsave;
    VREG*	r;



    isr = 0;
    fnvla_v11 = (n->Nflag & NF_VLA) != 0;
    fndimodcalls = fn_dimode_divmod_count(n->Nright);
    fnsavescr = fn_needs_scrreg(n->Nright);
    fnframesave = 0;
    /* Reserve one otherwise unused preserved AC for all eligible loops.
    ** It is not a VREG and never appears on the spill list. */
    fnindreg = 0;
    indptrreg = 0;
    indvarsym = indbasesym = NULL;
    if (_reg_count < R_PRESERVE_COUNT - (fnvla_v11 != 0)
      && fn_has_indloop(n->Nright)) {
        fnindreg = r_maxnopreserve + 1 + _reg_count;
    }
    nsave = n->Nreg + (fnindreg != 0);

    if (n->Nleft->Nright)		/* Any local-scope static data defs? */
	gendata(n->Nleft->Nright);	/* Yes, generate them first */

    codeseg ();				/* Ensure in code segment */
    inicode ();				/* Start making code */

    if (KCC_MLIST)
	{
	/* Emit definitions for register variables. */

	for (i = 0; i < _reg_count; i++)
	    {
	    outstr ("\t");
	    outid (Reg_Id[i]->Sname);
	    outstr ("=");
	    outnum (i + r_maxnopreserve + 1); /* FW 2A(47) */
	    outnl ();
	    }
	}

    outmidef(n->Nleft->Nleft->Nid);	/* Output function label */

    if (curfn != n->Nleft->Nleft->Nid)
	int_error("Bad funct Nid \"%S\"", n->Nleft->Nleft->Nid);

    fnargregs = 0;
    bltsave = 0;
#if SYS_CSI
    if (n->Nleft->Nleft->Nid->Sflags & TF_INTERRUPT)
	{				/* FW 2A(52) */
	isr = 1;
	outiprolog ();
	}
    else
#endif
	{
	/* AC16 is KCC's reserved output scratch register and is call-preserved
	** by the external ABI.  Save only AC16 here.  Expansions that need AC15
	** preserve it locally, avoiding two extra prologue/epilogue words in the
	** common case on PDP-6, which has no native DMOVE/DMOVEM.
	*/
        if (fnabidirect)
            gccabi_normalize_dimode_regs();
        else
            gccabi_arg_prologue();
	if (fnsavescr)
	    {
	    code00(P_PUSH, R_SP, R_SCRREG);
	    flushcode();
	    ++stackoffset;
	    ++fnframesave;
	    }
	if (fnvla_v11)
	    {
	    code00(P_PUSH, R_SP, R_MAXREG);
	    flushcode();
	    ++stackoffset;
	    ++fnframesave;
	    }
	}

#if !HOST_DAIMOS
    if (profbliss)			/* for BLISS profiler */
	outprolog (curfn);		/* added 09/15/89 by MVS */
#endif

    if (nsave <= R_PRESERVE_COUNT)	/* Reg linkage */
	{
        /* If four consecutive preserved ACs are used and this
        ** function also has automatic storage, allocate both areas with
        ** one stack adjustment and save the AC block with BLT.  For four
        ** ACs without autos this would merely replace four PUSHes by four
        ** instructions, so keep the simpler/faster historical sequence.
        */
        bltsave = !isr && nsave == 4 && maxauto > 0;
	if (bltsave)
	    {
	    int first = 1 - nsave - maxauto;
	    int last = -maxauto;

	    code8(P_ADJSP, VR_SP, nsave + maxauto);
	    flushcode();
	    outstr("\tMOVEI\t0,");
	    outnum(first);
	    outstr("(17)\n\tHRLI\t0,");
	    outnum(r_maxnopreserve + 1);
	    outstr("\n\tBLT\t0,");
	    outnum(last);
	    outstr("(17)\n");
	    stackoffset += nsave + maxauto;
	    fnframesave += nsave;
	    }

        for (i = 0; i < nsave;  i++)
	    {
	    /* Preserve allocated AC10-AC14 for every normal C function.
	    ** Interrupt entry already saves the complete interrupted register
	    ** context in outiprolog().
	    */

	    if (!isr && !bltsave)
		{	
		code00 (P_PUSH, R_SP, i + r_maxnopreserve + 1); /* FW 2A(47) */
		flushcode();
	        ++stackoffset;
	        ++fnframesave;
	        oline++;
	        }
		
	    /* Initialize all reg parameters (often optimized away later) */

	    if (i < _reg_count && Reg_Id[i]->Sclass == SC_RARG)
	        {
		if (Reg_Id[i]->Sflags & SF_ABIREG)
		    code00(P_MOVE, Reg_Id[i]->Sreg, Reg_Id[i]->Svalue);
		else
		    {
		    r_preserve = i + r_maxnopreserve + 1; /* FW 2A(47) */
		    r = vrget();

		    /* ex:  MOVE argc,-1(SP) */

		    code12(P_MOVE, r, -(Reg_Id[i]->Svalue) - stackoffset);
		    vrfree(r);
		    }
	        }
	    }
	}
    
    if (maxauto && !bltsave)
	{				/* If any auto vars, */
	code8(P_ADJSP, VR_SP, maxauto);	/* make room for them on stack */
	stackoffset += maxauto;		/* and remember stack bumped */
        }

    if (fnvla_v11)
        {
        code00(P_MOVE, R_MAXREG, R_SP);
        flushcode();
        }

    genretinit(n->Nright);
    genstmt (n->Nright);		/* Generate code for body */
    if (!isr && !fnvla_v11 &&
        (maxauto > 0 || _reg_count > 0 || fnindreg || fnsavescr || fnargregs > 0))
	genretepilog(nsave);
    endcode ();				/* Wrap up code */

    if (vrbfun) 
	fprintf (outmsgs, "%d words\n", _word_cnt);

    _word_cnt = 0;

    if (KCC_MLIST && (oline > HDR_LINES))
	outpghdr();

}

/* INICODE - Common code generation inits
*/
static void
inicode(void)
{
    previous = NULL;
    codefnreset();
    litstrings = NULL;
    litnodes = NULL;
    looplabel = brklabel = NULL;
    stackoffset = maxcode = mincode = 0;
    vrinit();
}

/* ENDCODE - Common code generation wrap-ups
*/

static void
endcode(void)
{
    flushcode();	/* Flush out peephole buffer */
    gliterals();	/* Generate any accumulated literals */
    vrendchk();		/* Check to make sure no regs active */
}

/* GENDATA - Generate data definitions
**
** This routine is only called to process static-extent data definitions
** of global or local scope, as opposed to local-extent (automatic) defs
** which are generated by genadata() in CCGEN1.
** Note that the Ntype of the symbol's Q_IDENT node is never examined here;
** the symbol's Stype is used instead.  They are identical except for
** array and function names, when the Ntype is "pointer to <Stype>".
*/

static void
gendata(NODE *n)
{
    NODE *var;
    SYMBOL *s;

    for (; n != NULL; n = n->Nright) {
	if (n->Nop != N_DATA) {
	    int_error("gendata: bad N_DATA %N", n);
	    break;
	}
	if ((var = n->Nleft) != NULL) {	/* For each item on N_DATA list */
	    TYPE *t;
	    if (var->Nop != N_IZ) {
		int_error("gendata: bad datum %N", n);
		break;
	    }
	    s = var->Nleft->Nid;		/* get symbol */
	    for (t = s->Stype; t->Tspec == TS_ARRAY; t = t->Tsubt)
		 ; /* Get to bottom of array */
	    if (asmdialect == ASM_GAS && var->Nright == NULL)
		bssseg();			/* zero-filled data */
	    else if (!tisanyvolat(t) && tisconst(t))	/* If obj can be pure, */
		codeseg();			/* put it in pure code seg */
	    else dataseg();			/* Else ensure in data seg */
	    outmidef(s);			/* make label for variable */
	    giz(var->Nright, s->Stype, s);	/* do the initialization */
	}
    }
    gliterals();		/* Put literals into code (pure) segment */
}

/* GLITERALS - Emit all accumulated literals
**	Forces use of code segment as literals are expected to be pure,
**	although this is not mandatory.
*/
static void
gliterals(void)
{
    if (litstrings || litnodes) {		
	codeseg();
	flushcode();			/* Make sure all code forced out */
    }
    /* Do node literals first since they may generate more string literals! */
    while (litnodes != NULL) {
	outlab(litnodes->Nendlab);	/* Emit internal label */
	giz(litnodes->Nleft, litnodes->Nleft->Ntype, litnodes->Nendlab);
	if (!(litnodes->Nflag & NF_GLOBAL))
	    freelabel(litnodes->Nendlab);
	litnodes = litnodes->Nright;
    }
    while (litstrings != NULL) {	/* Output literal strings */
	outlab(litstrings->Nsclab);	/* Emit generated label */
	freelabel(litstrings->Nsclab);	/* and then can free it. */
	outstr("\t");			/* spaced out from string. */
        if (litstrings->Nflag & NF_STRWIDE) {
            int i;
            for (i = 0; i < litstrings->Nsclen; ++i)
                outval((INT)strlit_get(litstrings, i));
        } else {
            int i, width = elembsize(litstrings->Ntype);
            bytbeg(width);
            for (i = 0; i < litstrings->Nsclen; ++i) {
                unsigned INT v = strlit_get(litstrings, i);
                if (width == 6) v = tosixbit((int)v);
                outval((INT)v);
            }
            bytend();
        }
	outnl();				/* End with final newline */
	litstrings = litstrings->Nscnext;	/* chain through list */
    }
}

/* GIZ - Generate initialization value for an object
*/
static void
giz(NODE *n, TYPE *t, SYMBOL *s)
{
    if (n == NULL) {
	giznull(t);	/* nothing there, just make block */
	return;
    }

    switch (t->Tspec) {
    case TS_ARRAY:
    case TS_STRUCT:
    case TS_UNION:
	gizlist(n, t, s);
	return;

    default:				/* initializing simple object */
	gizword(n, t, s);		/* make just one or two words */
	return;
    }
}

/* GIZWORD - emit initialization for a simple var (not array or structure)
**	This closely follows the nisconst() routine in CCDECL which
**	checked for legality while parsing.
*/
static void
gizword(NODE *n, TYPE *t, SYMBOL *s)
{
    if (n->Nop == N_IZLIST) {		/* something in brackets? */
	if (n->Nright != NULL)		/* no more than one allowed */
	    int_error("gizword: izer mismatch for %S %N", s, n);
	gizword(n->Nleft, t, s);	/* Just use inner part */
    } else
	if (!gizconst(n))		/* Try new stuff.  If not constant, */
	    gizexpr(n, t);		/* sigh, make at runtime. */
}

/* GIZCONST - Returns true if expression is an allowable initializer constant,
**	with appropriate code generated.  Otherwise, caller must generate.
*/
/* Return value indicates something about the type of constant: */
#define CT_NOTCON 0	/* not a constant, caller must generate. */
#define CT_CON	1	/* definitely a constant (arith, or a cast pointer) */
#define CT_ADDR	2	/* address of some kind */
#define CT_FUNC	3	/* function address (cannot add or sub from this) */

static struct pointerval {
	SYMBOL *pv_id;		/* Identifier (if any) */
	INT pv_off;		/* Offset from identifier (words or bytes) */
	int pv_bsize;		/* Byte size of pointer (0 = word) */
} pv;


static INT
gizconst(NODE *e)
{
    INT res;
    NODE *diff = labeldiff_v61o(e);

    if (diff) {
        int bits = tbitsize(e->Ntype);
        /* Match standalone byte scalars' right-justified native slot.
        ** Word integers are emitted directly. */
        if (tisbyte(e->Ntype)) {
            int pad = TGSIZ_WORD % bits;
            bytbeg(bits);
            outbyte(0, TGSIZ_WORD - bits - pad);
            outdiffbits_v61o(diff, bits);
            if (pad) outbyte(0, pad);
            bytend();
        } else {
            outstr(asmdialect == ASM_GAS ? "\t.word " : "\t");
            outdiff_v61o(diff, 0);
            if (bits < TGSIZ_WORD && tisunsign(e->Ntype)) {
                outstr("&");
                outnum(((unsigned INT)1 << bits) - 1);
            }
            outnl();
            ++locctr;
        }
        return CT_CON;
    }

    switch(e->Nop) {

	case N_ICONST:
	    if (tisbyte(e->Ntype)) {	/* Special handling for byte vals */
		res = e->Niconst & ((1<<tbitsize(e->Ntype))-1);	/* Mask off */
		res <<= (TGSIZ_WORD % tbitsize(e->Ntype));	/* Shift */
		outval(res);
		return CT_CON;
	    }
	    if (tisdimode(e->Ntype))
		{
		INT hi, lo;

		dimode_iconst_words(e, &hi, &lo);
		outval(hi);
		outval(dimode_lo_abi(hi, lo));
		return CT_CON;
		}
	    /* Normal word value, drop through */
	/* FALLTHROUGH */
	case N_PCONST:
#if __MSDOS__
	    if (e->Ntype->Tflag & TF_UNSIGN)
		unsign_int = 1;	   /* used in outnum() called by outval() */
	    outval(e->Niconst);		/* Just emit integer constant */
	    unsign_int = 0;
#else    
	    outval(e->Niconst);		/* Just emit integer constant */
#endif
	    return CT_CON;		/* Say simple constant generated */

	case N_FCONST:			/* Invoke rtn from CCOUT */
	    locctr += outflt(e->Ntype->Tspec, (INT *)&e->Nfconst, 0);
	    return CT_CON;		/* Say simple constant generated */


	/* Only the most likely cast conversions are supported here,
	** the others aren't common enough to be worth the
	** extra trouble.
	*/
	case N_CAST:
	    if (e->Ncast == CAST_NONE)
		return gizconst(e->Nleft); /*Most trivial cast just pass on */
	    /* Qualification of an absolute word pointer does not change its
	    ** representation. Preserve the constant instead of generating a
	    ** .LINK constructor (not supported by the GAS/DAS startup ABI).
	    ** Do not fold byte, packed or representation-polymorphic casts.
	    */
	    if (e->Ncast == CAST_PT_PT && e->Nleft->Nop == N_PCONST
	      && !tisbytepointer(e->Ntype) && !tisbytepointer(e->Nleft->Ntype)
	      && !tispackedptr(e->Ntype) && !tispackedptr(e->Nleft->Ntype)
	      && !tismaybitptr(e->Ntype) && !tismaybitptr(e->Nleft->Ntype)
	      && !tisbitptr(e->Ntype) && !tisbitptr(e->Nleft->Ntype)
	      && e->Ntype->Tsubt->Tspec != TS_VOID
	      && e->Nleft->Ntype->Tsubt->Tspec != TS_VOID)
		return gizconst(e->Nleft);
	    else if (e->Ncast != CAST_PT_PT && e->Ncast != CAST_IT_PT)
		return CT_NOTCON;	/* Not a constant */
	    /* Drop through to check for ptr (most likely cast) */

	/* FALLTHROUGH */
	default:
	    if (e->Ntype->Tspec == TS_PTR) {	/* Is this a pointer? */
		pv.pv_id = NULL;		/* Initialize arg struct */
		pv.pv_off = pv.pv_bsize = 0;
		if ((res = gizptr(e)) != 0) {	/* Fill in the struct */
		    /* Won, output pointer word. */
		    outstr("\t");			/* Won, output it. */
		    if (asmdialect == ASM_GAS)
			outstr(".word ");
		    outptr(pv.pv_id, pv.pv_bsize, pv.pv_off);
		    outnl();
		    ++locctr;
		    return res;
		}
	    }
    }
    return CT_NOTCON;		/* Must generate instructions */
}

/* GIZPTR - auxiliary for GIZCONST */
static int
gizptr(NODE *n)
{
    INT addoff, off;
    int i;
    TYPE *t;

	switch (n->Nop) {
	case N_PCONST:
	    /* The parser folds an explicit (void *)0 null pointer constant
	    ** into N_PCONST before an assignment conversion may wrap it in a
	    ** CAST_PT_PT node.  Preserve zero as a link-time constant instead
	    ** of falling back to a runtime initializer.
	    */
	    if (n->Niconst == 0) {
		pv.pv_id = NULL;
		pv.pv_off = 0;
		pv.pv_bsize = 0;
		return CT_ADDR;
	    }
	    return CT_NOTCON;

	case N_COMPLIT:
	    /* A file-scope compound literal is a static unnamed object.
	    ** Queue its initializer on the existing literal list the first
	    ** time its address is needed, using the symbol's internal label.
	    */
	    if (n->Nleft == NULL || n->Nleft->Nop == N_DATA
	      || n->Nright == NULL || n->Nright->Nop != Q_IDENT)
		return CT_NOTCON;
	    pv.pv_id = n->Nright->Nid;
	    if (!pv.pv_id->Sinit) {
		litnodes = ndeflr(N_LITIZ, n->Nleft, litnodes);
		litnodes->Nendlab = pv.pv_id->Ssym;
		litnodes->Nflag |= NF_GLOBAL;
		pv.pv_id->Sinit = 1;
	    }
	    if (tisbytearray(pv.pv_id->Stype))
		pv.pv_bsize = elembsize(pv.pv_id->Stype);
	    return CT_ADDR;

	case N_CAST:
	    switch ((int) n->Ncast) {
		case CAST_IT_PT:
		    /* A null pointer constant remains the all-zero pointer in
		    ** every KCC pointer representation.  This case matters for
		    ** hosted bootstrap builds because the self-host <stddef.h>
		    ** spells NULL as (void *)0.  Treating that as non-constant
		    ** forced GIZEXPR to emit a historical .LINK constructor.
		    */
		    if (n->Nleft != NULL
		      && (n->Nleft->Nop == N_ICONST || n->Nleft->Nop == N_PCONST)
		      && n->Nleft->Niconst == 0) {
			pv.pv_id = NULL;
			pv.pv_off = 0;
			pv.pv_bsize = 0;
			return CT_ADDR;
		    }
		    return CT_NOTCON;

		case CAST_PT_PT:	/* Only ptr-ptr supported */
		    i = gizptr(n->Nleft);	/* Get values for operand */
		    if (i == CT_FUNC		/* Function addr?  If so, */
		      && n->Ntype->Tspec == TS_PTR	/* and converting to */
		      && n->Ntype->Tsubt->Tspec == TS_FUNCT)	/* same, */
			return CT_FUNC;		/* No further conv needed! */

		    if (i != 2)		/* Only normal addrs allowed now */
			return CT_NOTCON;

		    /* A packed pointer entering a representation-polymorphic
		    ** object uses the existing S=1 logical address, not the
		    ** native element-size byte pointer.  Its offset is in bits.
		    */
		    if (tispackedptr(n->Nleft->Ntype) && tismaybitptr(n->Ntype)) {
			if (pv.pv_bsize == TGSIZ_CHAR) {
			    pv.pv_off *= TGSIZ_CHAR;
			    pv.pv_bsize = 1;
			}
			return CT_ADDR;
		    }
		    if (tismaybitptr(n->Ntype) && pv.pv_bsize == 1)
			return CT_ADDR;

		    /* First see whether a conversion is actually needed */
		    i = elembsize(n->Ntype);	/* Desired bytesize of ptr */
		    if (i == 0) {		/* Casting to (void *)? */
			if (tischarpointer(n->Nleft->Ntype)) /* from (char*)?*/
			    return CT_ADDR;	/* Yes, no change */
			i = TGSIZ_CHAR;		/* Else cvt to this bsize */
		    } else if (!elembsize(n->Nleft->Ntype)) { /* fm (void*)? */
			if (tischarpointer(n->Ntype))	      /* to (char*)?*/
			    return CT_ADDR;	/* Yes, no change */
		    }
		    if (i >= TGSIZ_WORD || (n->Ntype->Tsubt->Tspec != TS_VOID
		      && !tisbytepointer(n->Ntype))) i = 0;
		    if (i == pv.pv_bsize)	/* If already OK, */
			return CT_ADDR;		/* just return success */

		    /* Different sizes.  Check to see if boundaries match.
		    ** This takes care of 9<->18 bit conversions
		    ** (as well as any others)
		    */
		    if (i && pv.pv_bsize) {	/* Both are byte ptrs? */
			if (pv.pv_bsize < i && (i%pv.pv_bsize == 0)) {
			    pv.pv_off /= (i/pv.pv_bsize);
			    pv.pv_bsize = i;
			    return CT_ADDR;
			}
			if (i < pv.pv_bsize && (pv.pv_bsize%i == 0)) {
			    pv.pv_off *= (pv.pv_bsize/i);
			    pv.pv_bsize = i;
			    return CT_ADDR;
			}
		    }

		    /* Odd sizes.  First must always cvt to a word pointer */
		    if (pv.pv_bsize) {
			pv.pv_off /= (TGSIZ_WORD/pv.pv_bsize);
			pv.pv_bsize = 0;
		    }
		    /* Casting to byte ptr of some kind? */
		    if (i && (i < TGSIZ_WORD)) {
			pv.pv_off *= (TGSIZ_WORD/i);
			pv.pv_bsize = i;
		    }
		    return CT_ADDR;

		default:		/* Only ptr-to-ptr supported for now */
		    break;
	    }
	    return CT_NOTCON;


	case N_SCONST:
	    pv.pv_id = n->Nsclab = newlabel();	/* Get fwd lab for later use */
	    n->Nscnext = litstrings;	/* Link on string stack */
	    litstrings = n;		/* Now on stack */
	    pv.pv_bsize = (n->Nflag & NF_STRWIDE) ? 0 : elembsize(n->Ntype);
	    return CT_ADDR;			/* Say address generated */

	case N_ACONST:
	    pv.pv_id = n->Nxfsym;
	    /* GNU label values have type void *.  Use the same C-byte
	    ** coordinates as void-pointer arithmetic, also in static tables. */
	    pv.pv_off = 0;
	    pv.pv_bsize = TGSIZ_CHAR;
	    return CT_ADDR;

	case Q_IDENT:
		/* Identifier.  See documentation for Q_IDENT in cctoks.h
		** for explanation of this method of testing.
		*/
	    pv.pv_id = n->Nid;			/* Remember it */
	    switch (n->Nid->Stype->Tspec) {
		case TS_FUNCT:			/* Function address */
		    return CT_FUNC;		/* Say function address */
		case TS_ARRAY:			/* Array address */
		    if (tisbytearray(n->Nid->Stype))	/* If byte array, */
			pv.pv_bsize = elembsize(n->Nid->Stype);	/* set size */
		    return CT_ADDR;		/* Say array address */
	        default:
	            ;	/* do nothing */
	    }
	    return CT_NOTCON;			/* Barf */

	case N_ADDR:
	    switch (n->Nleft->Nop) {
		case N_SCONST:
        case N_COMPLIT:		/* address of static compound literal */
		    return gizptr(n->Nleft);

		case N_PTR:			/* &(*()) is no-op */
		    return gizptr(n->Nleft->Nleft);

#if 0
		/* Allow for conversion of arrays generated by subscripting */
		case Q_PLUS:
		    if (n->Nleft->Ntype->Tspec == TS_ARRAY)
			return gizptr(n->Nleft);	/* OK, continue */
		    return CT_NOTCON;			/* Not array, fail */
#endif

		/* Structure hair.
		** For MEMBER (->) the Nleft must be a constant address.
		**	Can just apply nisconst to this.
		** For DOT (.) the Nleft can be anything that evaluates into
		**	a static structure.  We assume this is only possible
		**	with either Q_IDENT, or N_PTR of a struct addr.
		*/
		case Q_DOT:
		    if (tisbitf(n->Nleft->Ntype))	/* No bitfield ptrs */
			return CT_NOTCON;
		    switch (n->Nleft->Nleft->Nop) {
			case Q_IDENT:
			    switch (n->Nleft->Nleft->Nid->Sclass) {
				case SC_XEXTREF: case SC_EXLINK:
				case SC_EXTDEF: case SC_EXTREF:
				case SC_INTDEF: case SC_INTREF:
				case SC_INLINK: case SC_ISTATIC:
				    pv.pv_id = n->Nleft->Nleft->Nid;
				    goto dostruct; /* Good address of object */
				default:
				    ;	/* do nothing */
			    }
			    break;
			case N_PTR:
			    if (gizptr(n->Nleft->Nleft->Nleft) == CT_ADDR)
				goto dostruct;
			    break;
			default:
			    ;	/* do nothing */
		    }
		    return CT_NOTCON;			/* Otherwise fail. */

		case Q_MEMBER:
		    if (tisbitf(n->Nleft->Ntype)	/* No bitfield ptrs */
		      || gizptr(n->Nleft->Nleft) != CT_ADDR)
			return CT_NOTCON;
		dostruct:
		    /* Decode packed layout markers before ordinary hardware P/S
		    ** fields.  074/075 encode bit offsets, while 077 encodes
		    ** C-byte offsets.  Their zero S field is not a byte size.
		    */
		    off = n->Nleft->Nxoff;
		    if (off < 0) {
			unsigned INT code = (unsigned INT)(-off);
			int marker = (int)(code & 07777L);
			if (marker == 07400L || marker == 07500L || marker == 07700L) {
			    INT bit = (INT)(code >> 12);
			    if (marker == 07700L)
				bit *= TGSIZ_CHAR;
			    /* A packed aggregate base is either a word address,
			    ** a native 9-bit address or an exact S=1 address.
			    */
			    if (pv.pv_bsize != 0 && pv.pv_bsize != TGSIZ_CHAR
			      && pv.pv_bsize != 1)
				return CT_NOTCON;
			    bit += pv.pv_off * (pv.pv_bsize ? pv.pv_bsize : TGSIZ_WORD);
			    if (marker == 07400L || tisbitptr(n->Ntype)
			      || tismaybitptr(n->Ntype) || bit % TGSIZ_CHAR) {
				pv.pv_bsize = 1;
				pv.pv_off = bit;
			    } else {
				pv.pv_bsize = TGSIZ_CHAR;
				pv.pv_off = bit / TGSIZ_CHAR;
			    }
			    return CT_ADDR;
			}
		    }
		    if (pv.pv_bsize)	/* Structaddr never a byteptr */
			return CT_NOTCON;
		    if (off < 0) {	/* Byte object? */
    /* Bug fix 'off' to '-off' by TEA, KAR 12/90 see fldsize() in ccdecl.c */
			pv.pv_bsize = (int) -off & 077;	/* Get byte size */
			pv.pv_off += (-off >> 12);	/* Add wd offset */
			pv.pv_off *= TGSIZ_WORD/pv.pv_bsize;
			pv.pv_off += (((-off)>>6)&077) / pv.pv_bsize;
		    } else if (tisbytearray(n->Nleft->Ntype)) {
			pv.pv_bsize = elembsize(n->Nleft->Ntype);
			pv.pv_off += off;	/* # of words offset */
			pv.pv_off *= TGSIZ_WORD/pv.pv_bsize;

		    } else {
			pv.pv_off += off;	/* # of words offset */
		    }
		    return CT_ADDR;

		case Q_IDENT:	/* Addr OK if of external or static */
			/* Needn't test type since parser checks it while
			** parsing "&" to verify not function or array.
			*/
		    switch (n->Nleft->Nid->Sclass) {
			case SC_XEXTREF: case SC_EXLINK:
			case SC_EXTDEF: case SC_EXTREF:
			case SC_INTDEF: case SC_INTREF:
			case SC_INLINK: case SC_ISTATIC:
			    pv.pv_id = n->Nleft->Nid;	/* Remember ident */
			    if (tisbyte(n->Nleft->Ntype)) {
				/* Single bytes are right-justified */
				pv.pv_bsize = (int) tbitsize(n->Nleft->Ntype);
				pv.pv_off = (TGSIZ_WORD/pv.pv_bsize) - 1;
			    }
			    return CT_ADDR;	/* Good address of object */
		    default:
			;	/* do nothing */
		    }
		    return CT_NOTCON;		/* Bad storage class */
	    default:
	        ;	/* do nothing */
	    }
	    return CT_NOTCON;			/* Bad use of & */

	/* Non-atomic expression checks, for plus and minus. */
	case Q_PLUS:
	    if (n->Nleft->Nop == N_ICONST		/* Integ constant */
		&& gizptr(n->Nright) == CT_ADDR) {	/* + address */
		    addoff = n->Nleft->Niconst;
		    t = n->Nright->Ntype;		/* Ptr has this type */
	     } else if (n->Nright->Nop == N_ICONST	/* Integ constant */
		&& gizptr(n->Nleft) == CT_ADDR) {	/* Address */
		    addoff = n->Nright->Niconst;
		    t = n->Nleft->Ntype;
	    } else return CT_NOTCON;

	    /* Use the same element stride as runtime pointer arithmetic.
	    ** A polymorphic constant already has a known representation.
	    */
	doadd:
	    if (pv.pv_bsize == 1 && (tisbitptr(t) || tismaybitptr(t))) {
		if (tispacked(t->Tsubt))
		    addoff *= t->Tsubt->Tbytes * TGSIZ_CHAR;
		else
		    addoff *= ((tbitsize(t->Tsubt) + TGSIZ_CHAR - 1)
			       / TGSIZ_CHAR) * TGSIZ_CHAR;
	    } else
		addoff *= sizeptobj(t);
	    pv.pv_off += addoff;
	    return CT_ADDR;

	case Q_MINUS:
	    if (n->Nright->Nop == N_ICONST	/* minus integ constant */
		&& gizptr(n->Nleft) == CT_ADDR) {	/* Address */
		addoff = - n->Nright->Niconst;
		t = n->Nleft->Ntype;
		goto doadd;
	    }
	    break;

	default:		/* Anything else just fails */
	    break;
    }
    return CT_NOTCON;
}

/* GIZNULL - Initialize an object to nothing.
*/
static void
giznull(TYPE *t)
{
    INT i;

    if ((i = sizetype(t)) <= 0)
#if SYS_CSI		/* 9/91, detect int a[]; that's never completed */
        if (i == 0)
            error("Missing size for definition of global non-external array");
        else
#endif
	    int_error("giznull: Bad BLOCK: %ld", (INT) i);
    else outzwds(i);
}

/* GIZEXPR - Generate code to initialize a static object at runtime.
**	Normally this should never be needed, but the capability is
**	kept here in case the need for cross-compiling ever comes up.
** Note: This will not work for initializing code-segment objects
** that are part of a larger object.  To work right, the init code gen
** needs to be deferred until the top-level object is done.  Don't bother
** fixing this unless it turns out we someday need it.
*/
static void
gizexpr(NODE *n, TYPE *t)
{
    static SYMBOL s;			/* Static to avoid re-initialization */
    SYMBOL *lnk;
    int oseg;

    s.Sclass = SC_ISTATIC;		/* Set up temp sym for loc to init */
    s.Stype = t;			/* Type for gaddress() & ndefident() */
    s.Ssym = newlabel();		/* Get an internal sym */
    outlab(s.Ssym);			/* and emit it directly */
    memcpy(s.Sname, s.Ssym->Sname, strlen(s.Ssym->Sname) + 1);	/* In case of debugging, copy name */
    giznull(t);				/* Emit space for the stuff to init */

    oseg = codeseg();			/* Switch to code segment */
    inicode();				/* Initialize for code generation */
    lnk = newlabel();			/* Get a label for linkage */
    outlab(lnk);			/* and emit it directly */
    if (asmdialect == ASM_GAS)
	outstr("\t.space 4\n");
    else
	outstr("\tBLOCK\t1\n");		/* Make space for linkage */

    /* Fake up an assignment expression setting this symbol */
    n = ndef(Q_ASGN, t, 0, ndefident(&s), n);	/* Use temp for Q_IDENT sym */
    genxrelease(n);			/* Generate code for assignment */

    code6(P_SKIP+POF_ISSKIP+POS_SKPE, VR_RETVAL, lnk); /* see if more inits */
    codemdx(P_JRST, 0, NULL, 1, R_RETVAL);	/* yes, chain to the next */
    code5(P_POPJ, VR_SP);			/* no, back to runtime init */
    endcode();				/* emit literals if any */

    outstr("\t.LINK\t1,");		/* start making link pseudo-op */
    outmiref(lnk);			/* linking through top of routine */
    outnl();				/* finish it off */
    prevseg(oseg);			/* back to previous segment */

    freelabel(s.Ssym);			/* no longer need labels */
    freelabel(lnk);			/* so give them back to freelist */
}

/* GIZLIST - initialize static (not auto) array/struct/union from list
*/
static void
gizlist(NODE *n, TYPE *t, SYMBOL *s)	/* N_IZLIST to initialize from */
{
    SYMBOL *sm;
    INT nelts, elwds;
    INT wdsleft;
    INT savloc;

    /* GNU packed aggregates have an exact C-byte extent that may not be a
    ** whole 36-bit word.  Emit arrays of them as one continuous 9-bit byte
    ** stream; otherwise the ordinary recursive initializer would word-align
    ** every element and destroy the packed array stride.
    */
    if ((tispacked(t) && (t->Tspec == TS_STRUCT || t->Tspec == TS_UNION))
      || (t->Tspec == TS_ARRAY && gizpackedtype(t->Tsubt))) {
        int savmode = bsiz;
        if (!bsiz) bytbeg(TGSIZ_CHAR);
        gizpacked(n, t, s);
        if (!savmode) bytend();
        return;
    }

    /* KCC does not have a distinct complete zero-length array type:
    ** Tsize == 0 also denotes an incomplete array.  Reject an empty
    ** initializer that cannot determine its bound, rather than silently
    ** emitting a zero-sized object or reporting an internal error. */
    if (t->Tspec == TS_ARRAY && t->Tsize == 0 && n != NULL
      && n->Nop == N_IZLIST && n->Nleft == NULL
      && n->Nright == NULL) {
	error("Empty array initializer cannot determine array size");
	return;
    }

    if ((wdsleft = sizetype(t)) <= 0) {		/* Paranoia */
	int_error("gizlist: bad size: %ld %N", (INT) wdsleft, n);
	return;					/* Don't try to fill out */
    }
    if (n->Nop != N_IZLIST) {			/* More paranoia */
	int_error("gizlist: not N_IZLIST %N", n);
	gizword(n, t, s);			/* Emit object expr anyway */
	return;					/* Nothing left on list */
    }

    switch (t->Tspec) {
    case TS_ARRAY:
	if (tisbytearray(t)) {		/* Array of bytes? */
	    gizbytes(n, t, s, 0);	/* Yep, go handle top-lev bytearray */
	    return;			/* Nothing left */
	}
        /* Only a wide character array consumes the literal's contents.
        ** A pointer array instead needs relocatable literal addresses. */
        if (t->Tsubt->Tspec == TS_INT && n->Nleft
          && n->Nleft->Nop == N_SCONST && (n->Nleft->Nflag & NF_STRWIDE)) {
            INT i;
            NODE *str = n->Nleft;
            for (i = 0; i < (INT)t->Tsize && i < str->Nsclen; ++i)
                outval((INT)strlit_get(str, (int)i));
            outzwds(wdsleft - i);
            return;
        }
	nelts = t->Tsize;		/* Get # elements in array */
	t = t->Tsubt;			/* Use member type from now on */
	elwds = sizetype(t);		/* Find # wds per element */
	for (; n && --nelts >= 0; n = n->Nright, wdsleft -= elwds)
	    giz(n->Nleft, t, s);	/* Initialize the element */
	break;

    case TS_UNION:
	if (n->Nright) {	/* Union izer should have only 1 element! */
	    int_error("gizlist: > 1 union izer %N", n);
	    n->Nright = NULL;	/* Merciless clobberage to recover */
	}
	sm = n->Nizmem ? n->Nizmem : t->Tsmtag->Ssmnext;
	goto gizstruct;

    case TS_STRUCT:
	sm = t->Tsmtag->Ssmnext;	/* Struct has tag */
    gizstruct:
	savloc = locctr;		/* Remember current loc ctr */
	for (; n && sm; n = n->Nright, sm = sm->Ssmnext) {
	    INT w, o, woff;
	    int p = 0, s = 0, gap;

	    /* First ensure ready to emit right word for this object.
	    ** P=073,S=0 is the exact-bit-offset form used for an ordinary
	    ** bit-field whose layout crossed a word boundary.  It is not a
	    ** byte-pointer P/S encoding, so consume it as an exact bit stream.
	    ** This also handles fields that themselves straddle a word.
	    */
	    if ((o = sm->Ssmoff) < 0) {	/* Byte or bitf object? */
                unsigned INT code = (unsigned INT)(-o);

                if ((code & 07777L) == 07700L) {
                    int_error("gizlist: packed cross-word member escaped packed initializer path");
                    continue;
                }
                if ((code & 07777L) == 07300L) {
                    INT bitoff = (INT)(code >> 12);
                    INT curbit;

                    s = tbitsize(sm->Stype);
                    if (!bsiz) bytbeg(1);
                    curbit = (locctr - savloc) * TGSIZ_WORD
                           + (TGSIZ_WORD - bpos);
                    if (curbit > bitoff) {
                        int_error("gizlist: exact-bit offset clash for %S", sm);
                        continue;
                    }
                    while (curbit < bitoff) {
                        int take = (int)(bitoff - curbit);
                        if (take > TGSIZ_WORD) take = TGSIZ_WORD;
                        gizpackedputbits(0, take);
                        curbit += take;
                    }
                    gizfieldbits_v61i(n->Nleft, s);
                    continue;
                }
		w = (-o) >> 12;		/* Decode word offset */
		p = (int) (((-o)&07700) >> 6);/* Byte pos within word, in bits */
		s = (int) ((-o) & 077);		/* Size of object, in bits */
	    } else bytend(), w = o;	/* Word object, leave byte mode */
	    woff = locctr - savloc;	/* Find current offset */
	    if (w != woff) {
		bytend();		/* Align again in case byte mode */
		woff = locctr - savloc;	/* Current offset may have changed */
		if (woff > w)		/* Offset mustn't go backwards!!! */
		    int_error("gizlist: offset clash for %S", sm);
		else outzwds(w - woff);
	    }
	    /* Right word offset, now see if word or byte/bit object */
	    if (o >= 0) {		/* If word object, */
		giz(n->Nleft, sm->Stype, sm);	/* Simply initialize it */
		continue;
	    }

	    /* Handle bitfield (or byte) objects differently from word objs */
	    if (!bsiz) bytbeg(1);	/* Ensure in byte mode */
	    gap = bpos - (p + s);	/* Get space between */
	    if (gap) {
		if (gap < 0) int_error("gizlist: -gap for %S", sm);
		else outbyte(0L, gap);	/* Space out to right place */
	    }
	    if (tisbytearray(sm->Stype))
		gizbytes(n->Nleft, sm->Stype, sm, 1);
	    else if (n->Nleft == NULL)
		outbyte(0L, s);
	    else {
                NODE *diff = labeldiff_v61o(n->Nleft);
                if (diff) outdiffbits_v61o(diff, s);
                else {
                    if (n->Nleft->Nop != N_ICONST)
                        int_error("gizlist: bitf izer not iconst %N", n);
                    outbyte(n->Nleft->Niconst, s);
                }
	    }
	}
	bytend();			/* Done, ensure out of byte mode */
	wdsleft -= (locctr - savloc);	/* Find # words left if any */
	break;

    default:
	int_error("gizlist: bad izer type: %d %N", t->Tspec, n);
	return;
    }

    /*
    ** Fill out remains of initializer.
    **
    ** We might have run off the end of our initializer before coming to
    ** the end of the array or structure we were initializing.  In that
    ** case, we are supposed to fill the rest with zeros; this is done
    ** by counting how much space we have and making a BLOCK that long.
    */
    if (n || wdsleft < 0) {
	int_error("gizlist: too many izers (wlft: %ld) %N", (INT) wdsleft, n);
	return;
    }
    if (wdsleft)
	outzwds(wdsleft);
}

/* GIZPACKEDTYPE - true if a static initializer can be emitted as one exact
** 9-bit byte stream.  The current packed layout accepts integral scalar
** members; arrays of such packed structs inherit the same representation.
*/
static int
gizpackedtype(TYPE *t)
{
    if (t == NULL) return 0;
    if (tispacked(t) && (t->Tspec == TS_STRUCT || t->Tspec == TS_UNION)) return 1;
    if (t->Tspec == TS_ARRAY)
        return tisinteg(t->Tsubt) || gizpackedtype(t->Tsubt);
    return 0;
}

/* GIZPACKEDBYTES - exact number of 9-bit C address units occupied by a
** packed initializer object.
*/
static INT
gizpackedbytes(TYPE *t)
{
    if (t == NULL) return 0;
    if (tispacked(t) && (t->Tspec == TS_STRUCT || t->Tspec == TS_UNION)) return t->Tbytes;
    if (t->Tspec == TS_ARRAY)
        return t->Tsize * gizpackedbytes(t->Tsubt);
    return (tbitsize(t) + TGSIZ_CHAR - 1) / TGSIZ_CHAR;
}

/* GIZPACKEDBITS - exact number of bits emitted for one packed object.
** Bit-fields consume exactly their declared width.  Ordinary scalar values
** retain their ceil(width/9) storage extent.  Packed aggregates include
** their trailing padding up to Tbytes so arrays preserve their C stride.
*/
static INT
gizpackedbits(TYPE *t)
{
    INT bits;

    if (t == NULL) return 0;
    if (tisbitf(t)) return tbitsize(t);
    if (tispacked(t) && (t->Tspec == TS_STRUCT || t->Tspec == TS_UNION))
        return t->Tbytes * TGSIZ_CHAR;
    if (t->Tspec == TS_ARRAY)
        return t->Tsize * gizpackedbits(t->Tsubt);
    bits = tbitsize(t);
    return ((bits + TGSIZ_CHAR - 1) / TGSIZ_CHAR) * TGSIZ_CHAR;
}

/* GIZPACKEDPUTBITS - append an exact bit string to the current packed
** initializer stream.  OUTBYTE cannot itself span a 36-bit word boundary,
** so split the value at both the current word boundary and 9-bit chunks.
** Bits are emitted most-significant first, matching the PDP-10 packed
** member load/store paths.
*/
static void
gizpackedputbits(unsigned INT v, int bits)
{
    int take, shift;
    unsigned INT mask, frag;

    while (bits > 0) {
        if (bpos <= 0) wdalign();
        take = bits;
        if (take > TGSIZ_CHAR) take = TGSIZ_CHAR;
        if (take > bpos) take = bpos;
        shift = bits - take;
        mask = ((unsigned INT)1 << take) - 1;
        frag = (v >> shift) & mask;
        outbyte((INT)frag, take);
        bits -= take;
    }
}

/* A wide field is a contiguous physical stream, not the two-word
** arithmetic ABI representation. Emit high payload then low35,
** omitting the duplicated ABI sign bit entirely. */
static void
gizfieldbits_v61i(NODE *n, int bits)
{
    INT hi = 0, lo = 0;

    if (n != NULL) {
        NODE *diff = labeldiff_v61o(n);
        if (diff && bits <= TGSIZ_WORD) {
            outdiffbits_v61o(diff, bits);
            return;
        }
        if (n->Nop != N_ICONST) {
            int_error("bitfield initializer not integer constant %N", n);
        } else if (bits > TGSIZ_WORD) {
            dimode_iconst_words(n, &hi, &lo);
        } else
            lo = n->Niconst;
    }
    if (bits > TGSIZ_WORD) {
        gizpackedputbits((unsigned INT)hi, bits - 35);
        bits = 35;
    }
    gizpackedputbits((unsigned INT)lo, bits);
}

/* GIZPACKEDSCALAR - emit one integral packed member in the exact bit stream.
** Bit-fields occupy exactly their declared width.  Ordinary scalar members
** occupy ceil(width/9) C address units, so any spare low bits in the last
** unit are emitted as zero padding.  This also works when the member itself
** starts at a non-byte-aligned position.
*/
static void
gizpackedscalar(NODE *n, TYPE *t, SYMBOL *s)
{
    unsigned INT v;
    int bits, storebits;

    if (n != NULL && n->Nop == N_IZLIST) {
        if (n->Nright != NULL)
            int_error("gizpackedscalar: izer mismatch for %S %N", s, n);
        n = n->Nleft;
    }

    bits = tbitsize(t);
    if (tisbitf(t)) {
        gizfieldbits_v61i(n, bits);
        return;
    }
    storebits = tisbitf(t) ? bits
                           : ((bits + TGSIZ_CHAR - 1) / TGSIZ_CHAR) * TGSIZ_CHAR;
    if (n == NULL) {
        gizpackedputbits(0, storebits);
        return;
    }
    {
        NODE *diff = labeldiff_v61o(n);
        if (diff) {
            outdiffbits_v61o(diff, bits);
            if (storebits > bits) gizpackedputbits(0, storebits - bits);
            return;
        }
    }
    if (n->Nop != N_ICONST && n->Nop != N_PCONST) {
        error("GNU packed static scalar initializer must be an integer constant");
        gizpackedputbits(0, storebits);
        return;
    }

    v = (unsigned INT)n->Niconst;
    gizpackedputbits(v, bits);
    if (storebits > bits)
        gizpackedputbits(0, storebits - bits);
}

/* GIZPACKED - emit a packed struct, or an array of packed structs, without
** inserting word alignment between members/elements.
*/
static void
gizpacked(NODE *n, TYPE *t, SYMBOL *s)
{
    SYMBOL *sm;
    INT leftbits;

    if (t->Tspec == TS_ARRAY) {
        INT i, nelts = t->Tsize;
        TYPE *et = t->Tsubt;
        NODE *q = n;

        if (q != NULL && q->Nop != N_IZLIST) {
            int_error("gizpacked: array izer not list %N", q);
            outzbs(gizpackedbytes(t));
            return;
        }
        if (et->Tspec == TS_INT && q && q->Nleft
          && q->Nleft->Nop == N_SCONST && (q->Nleft->Nflag & NF_STRWIDE)) {
            NODE *str = q->Nleft;
            for (i = 0; i < nelts; ++i)
                gizpackedputbits(i < str->Nsclen
                    ? strlit_get(str, (int)i) : 0, TGSIZ_WORD);
            return;
        }
        for (i = 0; i < nelts; ++i) {
            if (q != NULL) {
                gizpacked(q->Nleft, et, s);
                q = q->Nright;
            } else
                outzbs(gizpackedbytes(et));
        }
        if (q != NULL)
            int_error("gizpacked: too many array initializers %N", q);
        return;
    }

    if (!tispacked(t) || (t->Tspec != TS_STRUCT && t->Tspec != TS_UNION)) {
        gizpackedscalar(n, t, s);
        return;
    }
    if (n != NULL && n->Nop != N_IZLIST) {
        int_error("gizpacked: struct izer not list %N", n);
        outzbs(t->Tbytes);
        return;
    }

    if (t->Tspec == TS_UNION) {
        NODE *q = n;
        sm = (q != NULL && q->Nizmem != NULL)
             ? q->Nizmem : t->Tsmtag->Ssmnext;
        if (sm == NULL) {
            outzbs(t->Tbytes);
            return;
        }
        if (q != NULL)
            q = q->Nleft;
        if (gizpackedtype(sm->Stype))
            gizpacked(q, sm->Stype, sm);
        else
            gizpackedscalar(q, sm->Stype, sm);
        leftbits = t->Tbytes * TGSIZ_CHAR - gizpackedbits(sm->Stype);
        if (leftbits > 0)
            gizpackedputbits(0, (int)leftbits);
        return;
    }

    leftbits = t->Tbytes * TGSIZ_CHAR;
    sm = t->Tsmtag->Ssmnext;
    for (; sm != NULL; sm = sm->Ssmnext) {
        INT mbits = gizpackedbits(sm->Stype);
        if (n != NULL) {
            if (gizpackedtype(sm->Stype))
                gizpacked(n->Nleft, sm->Stype, sm);
            else
                gizpackedscalar(n->Nleft, sm->Stype, sm);
            n = n->Nright;
        } else
            gizpackedputbits(0, (int)mbits);
        leftbits -= mbits;
    }
    if (n != NULL)
        int_error("gizpacked: too many struct initializers %N", n);
    if (leftbits > 0) gizpackedputbits(0, (int)leftbits);
}

/* GIZBYTES - Initialize byte array
**	May already be in byte mode.
*/
static void
gizbytes(NODE *izl, TYPE *t, SYMBOL *s, int lev) /* Level (0 is top level) */
{
    register NODE *n = izl;
    int savmode = bsiz;		/* Remember initial mode */
    INT nbs = sizearray(t);	/* # of bottom elements (bytes) in array */
    INT i;

    /* A missing designated slot means zero-initialize the complete byte
    ** object at this level.  Sparse designated arrays deliberately carry
    ** such NULL slots in their normalized N_IZLIST representation.
    */
    if (n == NULL) {
	if (lev == 0) bytbeg(elembsize(t));
	outzbs(nbs);
	if (lev == 0 && !savmode) bytend();
	return;
    }

    if (n->Nop != N_IZLIST) {
	int_error("gizbytes: izer not list %N", n);
	return;
    }
    if (lev == 0)
	bytbeg(elembsize(t));		/* Get into byte mode with this size */
    for (n = izl; n; n = n->Nright) {
	if (n->Nleft == NULL) {
	    i = sizearray(t->Tsubt);
	    outzbs(i);
	    nbs -= i;
	    continue;
	}
	switch (n->Nleft->Nop) {
	    case N_ICONST:		/* Single byte */
		outval(n->Nleft->Niconst);
		nbs--;			/* count off */
		break;
	    case N_SCONST:		/* String literal */
		if (izl != n || n->Nright) {	/* Must be only thing! */
		    int_error("gizbytes: str not sole node %N", n);
		}
                i = nbs > n->Nleft->Nsclen ? n->Nleft->Nsclen : nbs;
                nbs -= i;
                {
                    int j;
                    for (j = 0; j < i; ++j) {
                        unsigned INT v = strlit_get(n->Nleft, j);
                        outval(bsiz == 6 ? (INT)tosixbit((int)v) : (INT)v);
                    }
                }
		break;
	    case N_IZLIST:		/* Subarray */
		gizbytes(n->Nleft, t->Tsubt,s, lev+1);	/* Do recursively */
		nbs -= sizearray(t->Tsubt);	/* Done with subarray bytes */
		break;
	    default:
                {
                    NODE *diff = labeldiff_v61o(n->Nleft);
                    if (diff) {
                        outbyteprefix_v61o(bsiz);
                        outdiff_v61o(diff, 0);
                        nbs--;
                    } else int_error("gizbytes: bad izer for %S %N", s, n);
                }
	}
    }

    /*
    ** Initialization done, fill out rest of array.
    ** Our array might be a subarray of some other char array,
    ** so we must be prepared to leave a ragged end.
    */
    if (nbs > 0)		/* Not enough elements? */
	outzbs(nbs);		/* Fill up this many zero bytes */
    else if (nbs < 0)
	int_error("gizbytes: too many izers, %S", s);

    if (lev == 0 && !savmode)	/* If at top level, leave byte mode now */
	bytend();
    return;
}

/* OUT Data emission stuff.  Tracks whether in byte or word mode,
**	plus count of # words emitted so far.
*/

/* BYTBEG - Initializes to output bytes of given size.
**	If already in byte mode, does nothing except change bytesize.
*/
static void
bytbeg(int siz)
{
    if (!bsiz) {
	bpos = TGSIZ_WORD;
	bpw = TGSIZ_WORD/siz;
	savlct = locctr;
    }
    bsiz = siz;
}

/* BYTEND - Leaves byte mode, returns # words output
**	since byte mode was entered (0 if never entered)
*/
static INT
bytend(void)
{
    if (!bsiz) return 0;
    wdalign();			/* Force output to word boundary */
    bsiz = 0;			/* Leave byte mode */
    return locctr - savlct;	/* And return # words done so far */
}

/* WDALIGN - Aligns output to word boundary, doesn't change mode.
*/
static void
wdalign(void)
{
    if (bsiz && bpos != TGSIZ_WORD) {
	outnl();		/* Force out current word */
	++locctr;		/* and account for it */
	bpos = TGSIZ_WORD;	/* Now at start of new word */
    }
}

/* OUTVAL - Output value in either byte or word mode.
*/
static void
outval(INT v)
{
    if (!bsiz) {
	outstr("\t");
	if (asmdialect == ASM_GAS)
	    outstr(".word ");
	outnum(v);
	outnl();
	++locctr;
    } else outbyte(v, bsiz);
}

/* OUTBYTE - like OUTVAL but byte size is specified (changes default).
**	Must already be in byte mode.
*/
static void
outbyteprefix_v61o(int siz)
{
    char nbuf[40];

    if (bpos < siz)
	wdalign();		/* If not enough room, get new wd */
    if (bpos == TGSIZ_WORD) {	/* If at start of word, */
	sprintf(nbuf, asmdialect == ASM_GAS ? "\t.byte %d," : "\tBYTE (%d) ", siz);
	outstr(nbuf);
	bsiz = siz;			/* and remember the active size */
    }
    else if (siz == bsiz) {		/* can skip size if no change */
	outstr(",");
    }
    else {
	sprintf(nbuf, " (%d) ", siz);	/* Else just output it */
	outstr(nbuf);
	bsiz = siz;
    }
    bpos -= siz;
}

static void
outbyte(INT v, int siz)
{
    outbyteprefix_v61o(siz);
    outnum(v & (siz == TGSIZ_WORD ? ~(unsigned INT)0 : (((unsigned INT)1 << siz) - 1)));
}

/* Both labels are in the same function's text section. Their word
** difference is absolute at assembly time; scale to C-byte units. */
static void
outdiff_v61o(NODE *n, int shift)
{
    outnum(TGSIZ_WORD / TGSIZ_CHAR);
    outstr("*(");
    outmiref(n->Nleft->Nxfsym);
    outstr("-");
    outmiref(n->Nright->Nxfsym);
    outstr(")");
    if (shift) {
        outstr(">>");
        outnum(shift);
    }
}

/* Emit the low bits of a deferred value, splitting only when the
** physical field crosses a word. DAS masks each absolute fragment. */
static void
outdiffbits_v61o(NODE *n, int bits)
{
    while (bits > 0) {
        int take = bpos ? bpos : TGSIZ_WORD;
        if (take > bits) take = bits;
        bits -= take;
        outbyteprefix_v61o(take);
        outdiff_v61o(n, bits);
    }
}

#if 0	/* 5/91 KCC size */
/* OUTZVALS - Output zero values to fill up space.
*/
static void
outzvals(INT zeros)
{
    if (bsiz) outzbs(zeros);
    else outzwds(zeros);
}
#endif

/* OUTZBS - Output zero bytes to fill up space.
*/
static void
outzbs(INT zeros)
{
    if (zeros <= 0) return;
    while (bpos != TGSIZ_WORD && bpos >= bsiz && --zeros >= 0)
	outval(0L);		/* Add filler til at word boundary */
    if (zeros >= (bpw = (TGSIZ_WORD/bsiz))) {	/* Full words left? */
	wdalign();			/* Ensure properly aligned */
	outzwds(zeros/bpw);		/* Zap those words */
	zeros %= bpw;			/* Get remaining # bytes */
    }
    while (--zeros >= 0)		/* Finish off */
	outval(0L);
}

/* OUTZWDS - Output zero words to fill up space.
*/
static void
outzwds(INT nwds)
{
    if (nwds > 0) {
	char nbuf[64];
	if (asmdialect == ASM_GAS)
	    sprintf(nbuf, "\t.space %lu\n", (unsigned long)nwds * 4UL);
	else
	    sprintf(nbuf, "\tBLOCK %" INT_OFMT "\n", (unsigned INT)nwds); /* This many zero wds */
	outstr(nbuf);
	locctr += nwds;
    }
}
