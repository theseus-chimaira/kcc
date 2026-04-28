/*	CCGEN2.C - Generate code for parse-tree expressions
**
**	(c) Copyright Ken Harrenstien 1989
**		All changes after v.295, 14-May-1989
**	(c) Copyright Ken Harrenstien, SRI International 1985, 1986
**		All changes after v.79, 8-Aug-1985
**
**	Original version (C) 1981  K. Chen
*/
#define NEWTERN 1	/* Try new ternary code */

#include "cc.h"
#include "ccgen.h"
#include <string.h>
#include <stdarg.h>

/*
** KCCFMT - bounded formatter for generated assembly fragments.
**
** ccgen2 only needs a deliberately small printf subset here.  Keeping the
** formatter local avoids unbounded sprintf() calls and does not introduce a
** C99 snprintf() dependency into this old host compiler.  Supported
** conversions are %d, %o, %lo, %s, and %%.
*/
static void
kccfmt_putc(char *buf, size_t size, size_t *used, int ch)
{
    if (*used + 1 >= size)
        fatal("ccgen2: generated assembly buffer overflow");
    buf[(*used)++] = (char)ch;
}

static void
kccfmt_unsigned(char *buf, size_t size, size_t *used,
                unsigned long value, unsigned int base)
{
    char digits[3 * sizeof(unsigned long) + 1];
    size_t n;

    n = 0;
    do {
        digits[n++] = (char)('0' + (value % base));
        value /= base;
    } while (value != 0);
    while (n != 0)
        kccfmt_putc(buf, size, used, digits[--n]);
}

static int
kccfmt(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    size_t used;
    int ch;

    if (size == 0)
        fatal("ccgen2: zero-sized generated assembly buffer");

    used = 0;
    va_start(ap, fmt);
    while ((ch = (unsigned char)*fmt++) != '\0') {
        int islong;

        if (ch != '%') {
            kccfmt_putc(buf, size, &used, ch);
            continue;
        }
        ch = (unsigned char)*fmt++;
        if (ch == '%') {
            kccfmt_putc(buf, size, &used, '%');
            continue;
        }

        islong = 0;
        if (ch == 'l') {
            islong = 1;
            ch = (unsigned char)*fmt++;
        }

        switch (ch) {
        case 'd': {
            long value;
            unsigned long magnitude;

            value = islong ? va_arg(ap, long) : (long)va_arg(ap, int);
            if (value < 0) {
                kccfmt_putc(buf, size, &used, '-');
                magnitude = (unsigned long)(-(value + 1)) + 1;
            } else
                magnitude = (unsigned long)value;
            kccfmt_unsigned(buf, size, &used, magnitude, 10);
            break;
        }
        case 'o':
            if (islong)
                kccfmt_unsigned(buf, size, &used,
                                va_arg(ap, unsigned long), 8);
            else
                kccfmt_unsigned(buf, size, &used,
                                (unsigned long)va_arg(ap, unsigned int), 8);
            break;
        case 's': {
            const char *str;

            if (islong)
                fatal("ccgen2: unsupported generated assembly format");
            str = va_arg(ap, const char *);
            while (*str != '\0')
                kccfmt_putc(buf, size, &used, (unsigned char)*str++);
            break;
        }
        default:
            fatal("ccgen2: unsupported generated assembly format");
        }
    }
    va_end(ap);
    buf[used] = '\0';
    return (int)used;
}

void
gccabi_dimode_normalize_reg(int lo)
{
    char buf[48];
    int n;
    n = kccfmt(buf, sizeof(buf), "\tTLZ\t%o,0400000\n", lo);
    codestr(buf, n);
}

void
gccabi_dimode_encode_regs(int hi, int lo)
{
    char buf[112];
    int n;
    n = kccfmt(buf, sizeof(buf),
        "\tTLZ\t%o,0400000\n"
        "\tTLNE\t%o,0400000\n"
        "\t TLO\t%o,0400000\n",
        lo, hi, lo);
    codestr(buf, n);
}


/* Imported functions */
extern SYMBOL *newlabel(void);		/* CCSYM */
extern SYMBOL *vlabase_v11(SYMBOL *), *vlaboundsym_v11(TYPE *); /* CCDECL */
extern NODE *vlaboundexpr_v11(TYPE *); /* CCDECL */
extern SYMBOL *symfidstr(char *), *symgcreat(char *);
extern int elembsize(TYPE *);			/* CCSYM */
extern INT sizetype(TYPE *), sizeptobj(TYPE *);
extern int cmptype(TYPE *, TYPE *);
extern void foldhalfstore(void);		/* CCCODE */
extern void genadata(NODE *), genstmt(NODE *);	/* CCGEN1 */
extern int sideffp(NODE *);		/* CCEVAL */
extern NODE *convbinary(NODE *);		/* CCTYPE */
extern void folddiv(VREG *);
extern int unjump(SYMBOL *);
extern SYMBOL *cregupto(SYMBOL *);	/* CCCREG for gternary() */
extern VREG *vrdget(void);
extern void vrfree (VREG *);
extern VREG *vrget(void);
extern int vrispair(VREG *);
extern void vrnarrow (VREG *);
extern int vrreal (VREG *);
extern int vrstoreal (VREG *, VREG *);
extern int vrtoreal(VREG *);
extern void vrufcreg(VREG *);
extern void vralspill(void);
extern void vrlowiden (VREG *);
extern VREG *vrretdget(void);
extern VREG *vrretget(void);
extern void vrallspill(void);	/* VERY non-optimal!! */
extern void vrspillothers(VREG *, VREG *);
extern void vrunspillall(void);
extern int rfree(int);

/* Exported functions */
VREG *genexpr(NODE *);
void genxrelease(NODE *);
void relflush(VREG *reg);		/* Maybe move to CCOPT */
void gboolean(NODE *n, SYMBOL *false, int reverse);
VREG *getmem(VREG *reg, TYPE *t, int byte, int keep),
  *stomem(VREG *reg, VREG *ra, INT siz, int byteptr); /* CCGEN1 auto inits */
VREG *gmuuo(NODE *);

static INT
autooff_v11(SYMBOL *s)
{
    return fnvla_v11 ? (s->Svalue + 1 - maxauto)
                     : ((s->Svalue + 1) + fnframesave - stackoffset);
}

static INT
argoff_v11(SYMBOL *s)
{
    return fnvla_v11 ? (-s->Svalue - fnframesave - maxauto)
                     : (-s->Svalue - stackoffset);
}

static int
frameindex_v11(void)
{
    return fnvla_v11 ? R_MAXREG : R_SP;
}

/* Internal functions */
static VREG *gexpr(NODE *),
	*gternary(NODE *);
static void gor(NODE *, SYMBOL *, int),
	gand(NODE *, SYMBOL *, int),
	gboolop(NODE *, int);
static VREG *gassign(NODE *),
	*grotate(NODE *),
	*gbinary(NODE *),
	*garithop(int, VREG *, VREG *, int),
	*gptrop(int, VREG *, VREG *, TYPE *, TYPE *),
	*gptraddend(TYPE *, NODE *),
	*gmaybitadjust(VREG *, VREG *, TYPE *, int),
	*gmaybitsub(VREG *, VREG *, TYPE *),
	*gptrcanoncmp(VREG *, TYPE *),
	*glogical(NODE *),
	*gunary(NODE *),
	*gcast(NODE *),
	*gcastr(int, VREG *, TYPE *, TYPE *, NODE *),
	*gintwiden(VREG *, TYPE *, TYPE *, NODE *),
	*guintwiden(VREG *, int, NODE *),
	*gincdec(NODE *, int, int),
	*gprimary(NODE *),
	*gcall(NODE *);
static int vlatype_v11(TYPE *);
static NODE *vlastride_v11(TYPE *);
static void emit_blissargs(NODE *);
static INT sizeargs(NODE *);
static int gccabi_direct_reg_args(NODE *, int, TYPE *, int);
static int gccabi_direct_tail_ok(NODE *);
static void gfnarg(NODE *);
static VREG *gaddress(NODE *);
static void pitopc(VREG *, int, int, int);
static  int bptrref(NODE *);
static INT packedmembyte(INT);
static INT packedmembit(INT);
static int packedcross(INT);
static int packedbit(INT);
static int crossbit(INT);
static INT crossbitoff(INT);
static int packedbitscalar(INT);
static int packedbitagg(INT);
static int packedptrderef(NODE *), bitptrderef(NODE *), maybitptrderef(NODE *), bitptrmember(NODE *);
static VREG *gmaybitload(NODE *);
static VREG *gmaybitloaddepth(NODE *, int);
static VREG *gmaybitstore(VREG *, NODE *);
static VREG *gmaybitstoredepth(VREG *, NODE *, int);
static INT packedoffbit(INT);
static VREG *gpackedload(NODE *);
static VREG *gpackedloadat(NODE *, VREG *);
static VREG *gpackedstore(VREG *, NODE *);
static VREG *gpackedstoreat(VREG *, NODE *, VREG *);
static VREG *gpackedcopy(NODE *, NODE *, TYPE *);
static VREG *gpackedvalue(NODE *, TYPE *);
static VREG *gpackedbitbase(NODE *, INT *);
static VREG *gpackedbitvalue(NODE *, TYPE *);
static VREG *gpackedregscalar(NODE *, VREG *);
static VREG *gpackedbitstorereg(NODE *, VREG *, TYPE *);
static VREG *gpackedbitcopy(NODE *, NODE *, TYPE *);
static VREG *gpackedcopyreg(NODE *, VREG *, TYPE *);
static void gasm(NODE *), gjffo(NODE *);
static void gdimemload(VREG *, VREG *, TYPE *, int);
static void gdimemstore(VREG *, VREG *);
static void gdimove(VREG *, VREG *);
static VREG *gdimode_from_int(VREG *, TYPE *, TYPE *, NODE *);
static void gretmove(TYPE *, VREG *, VREG *);
static VREG *gdimodeadd(VREG *, VREG *);
static VREG *gdimodesub(VREG *, VREG *);
static VREG *gdimodemul(VREG *, VREG *);
static VREG *gdimodedivmod(VREG *, VREG *, int, int);
static VREG *gdimodehelper(VREG *, VREG *, int, int);
static void gdimodeneg(VREG *);
static VREG *gdimodebitwise(int, VREG *, VREG *);
static void gdimodecompl(VREG *);
static int gdimodezero(NODE *, int, int);
static int dimode_power2_exp(INT, INT);
static int dimode_negative_power2_exp(INT, INT);
static VREG *gdimodeshift(int, VREG *, VREG *, int);

#if 0
static VREG *rgetmem(VREG *reg, TYPE *t, int byte, int keep);
#else
static VREG *rgetmem(VREG *reg, TYPE *t, int keep);
#endif
#if 0
static VREG* rstomem(VREG *reg, int ra, INT siz, int byteptr);
static VREG *gexpr(), *gternary();
static void gor(), gand(), gboolop();
static VREG *gassign(), *gbinary(), *garithop(), *gptrop(), *gptraddend(),
       *glogical(), *gunary(), *gcast(), *gcastr(),
       *gintwiden(), *guintwiden(), *gincdec(),  *gprimary(),  *gcall();
static void emit_blissargs();
static INT sizeargs();
static void gfnarg();
static VREG *gaddress();
static void pitopc();
static int bptrref();
static void gasm(), gjffo();
#endif

/* GENEXPR - Main function for expression code generation.
**	Argument is pointer to a parse-tree node expression;
**	Result is pointer to a virtual register.
** NOTE: the result may be NULL if the expression was marked
**	to be discarded, or was cast to (void), or an error
**	was encountered.
*/
VREG *
genexpr(NODE *n)
{
    if (!n)
	return NULL;		/* Check for null exprs here */
    if (n->Nflag & NF_DISCARD)	/* If discarding result, */
	{
	relflush(gexpr(n));		/* flush any resulting register(s) */
	return NULL;
	}
    return gexpr(n);		/* Normal case, generate code & return reg */
}

/* GENXRELEASE - Auxiliary like genexpr but called when we want to make
**	sure that the resulting value is forced to be discarded.
*/
void
genxrelease(NODE *n)
{
    n->Nflag |= NF_DISCARD;	/* Will be discarding this value */
    genexpr(n);
}

/* GEXPR - workhorse routine for genexpr().  Note arg is guaranteed non-null.
*/
static VREG *
gexpr(NODE *n)
{
    switch (tok[n->Nop].tktype)
	{
	case TKTY_ASOP:
	    return gassign(n);
	case TKTY_TERNARY:
	    return gternary(n);
	case TKTY_BINOP:
	    return gbinary(n);
	case TKTY_BOOLOP:
	case TKTY_BOOLUN:
	    return glogical(n);
	case TKTY_UNOP:
	    return gunary(n);
	case TKTY_RWOP:				/* For now, RW's go below */
	case TKTY_PRIMARY:
	    return gprimary(n);
	case TKTY_SEQ:				/* Comma operator */
	    if (n->Nleft)
		genxrelease(n->Nleft);	/* Flush result of left */
	    return genexpr(n->Nright);	/* and return that of right */
	}

    int_error("gexpr: bad op %N", n);
    return NULL;
}

/*
** RELFLUSH - Flush no-longer-wanted register value
**	Mainly called by genexpr(); also called by gcastr() when casting
** a value to (void).
**	Note that the reg argument may be NULL.  This can happen for
** a generated value of type "void" (size 0).
*/
void
relflush (VREG *reg)
{
    int r;			/* get physical register */
    PCODE *p;

    if (reg == NULL)
	return;
    r = vrtoreal(reg);		/* Save physical reg # */
    vrfree(reg);		/* Now release virtual reg or reg pair */

    /* This should be moved to someplace in CCOPT */
    if (optobj)
	for (p = previous; p != NULL; p = before (p))
	    {
	    if (p->Pop == P_ADJSP)
		continue; /* skip back across P_ADJSP */
	    else if (p->Pop != P_MOVE
		     || p->Preg != r
		     || prevskips(p))
		break; /* not flushable */
	    else 
	/* Do NOT discard preserved reg values.
	 * Later, add optimization at end of basic block to do it.
	 */ if (Register_Nopreserve(p->Preg))
		{
		p->Pop = P_NOP;		/* drop pointless NOP */
		fixprev();			/* fix up for drop */
		}
	    }
}

/* GTERNARY - generate code for ternary operators
**	Note: handles case where one or both result expression pointers may be
** NULL.  The overall value had better be "void" if so.
**
** There is some suboptimal code here, namely the call to "vrallspill()" to
** save any registers that are active at the time this operand is executed.
** This is necessary because we don't know at this point whether either the
** true or false path will require saving registers (eg if a function call is
** done), but control still has to merge back to the same place.  If one
** path saves regs and the other doesn't, the stack is going to be confused
** at the point where control merges (ie at end of ternary expression).
** The active registers have to be in the same state afterwards as they
** were before, and at the moment the only safe way of doing this is to
** bite the bullet and save them all prior to doing the ternary expression.
**	A similar problem exists for the logical operands; anything that
** branches during expression evaluation.  Conditional statements like "if"
** are not affected because there are never any registers active across a
** statement.
*/

/* GTERNARY_NORMAL_OK - Can this marked scalar ?: avoid AC1?
** Keep the expression-shape check in sync with CCDECL.
*/
static int
gternary_normal_expr(NODE *n, int depth)
{
    int op;

    if (n == NULL)
        return 1;
    if (depth > 128)
        return 0;
    op = n->Nop;
    switch (op) {
    case Q_IDENT:
    case N_ICONST:
    case N_PCONST:
    case N_ECONST:
    case N_FCONST:
    case N_SCONST:
    case N_VCONST:
        return 1;
    case N_CAST:
    case N_ADDR:
    case N_PTR:
    case N_NEG:
    case Q_COMPL:
    case Q_NOT:
        return gternary_normal_expr(n->Nleft, depth + 1);
    case Q_DOT:
    case Q_MEMBER:
    case Q_PLUS:
    case Q_MINUS:
    case Q_MPLY:
    case Q_LSHFT:
    case Q_RSHFT:
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
        return gternary_normal_expr(n->Nleft, depth + 1)
            && gternary_normal_expr(n->Nright, depth + 1);
    default:
        return 0;
    }
}

static int
gternary_normal_ok(NODE *n)
{
    if (!n || !(n->Nflag & NF_QUERYNORMAL) || n->Nop != Q_QUERY
      || sizetype(n->Ntype) != 1 || !n->Nright)
        return 0;
    return gternary_normal_expr(n->Nleft, 0)
        && gternary_normal_expr(n->Nright->Nleft, 0)
        && gternary_normal_expr(n->Nright->Nright, 0);
}

/* GABSQUERY - recognize the canonical signed absolute-value ternary.
**
** Fold only the deliberately narrow, side-effect-safe forms
**
**     x < 0 ? -x : x
**     0 > x ? -x : x
**
** for a non-volatile one-word signed identifier or a simple *p dereference.
*/
static int
gsamepureexpr(NODE *a, NODE *b, int depth)
{
    if (a == b)
        return 1;
    if (!a || !b || a->Nop != b->Nop || depth > 12)
        return 0;

    switch (a->Nop) {
    case Q_IDENT:
        return a->Nid == b->Nid;
    case N_ICONST:
        return a->Niconst == b->Niconst;
    case N_PTR:
    case N_ADDR:
        return gsamepureexpr(a->Nleft, b->Nleft, depth + 1);
    case N_CAST:
        return a->Ncast == b->Ncast
            && gsamepureexpr(a->Nleft, b->Nleft, depth + 1);
    case Q_DOT:
    case Q_MEMBER:
        return a->Nxoff == b->Nxoff
            && gsamepureexpr(a->Nleft, b->Nleft, depth + 1);
    case Q_PLUS:
    case Q_MINUS:
    case Q_MPLY:
    case Q_LSHFT:
        return gsamepureexpr(a->Nleft, b->Nleft, depth + 1)
            && gsamepureexpr(a->Nright, b->Nright, depth + 1);
    default:
        return 0;
    }
}

static int
gsamepure(NODE *a, NODE *b)
{
    if (!a || !b || sideffp(a) || sideffp(b))
        return 0;
    return gsamepureexpr(a, b, 0);
}

static int
gabszero(NODE *n)
{
    return n && n->Nop == N_ICONST && n->Niconst == 0;
}

static NODE *
gabsquery_id(NODE *n)
{
    NODE *cond, *yes, *no, *id;

    if (!optgen || !n || n->Nop != Q_QUERY || sizetype(n->Ntype) != 1
      || !n->Nright)
        return NULL;

    cond = n->Nleft;
    yes = n->Nright->Nleft;
    no = n->Nright->Nright;
    if (!cond || !yes || !no)
        return NULL;

    /* Accept both equivalent branch orientations:
    **
    **     negative-or-zero ? -x : x
    **     positive-or-zero ?  x : -x
    **
    ** Strict comparisons are also valid because -0 == 0.
    */
    if (yes->Nop == N_NEG && yes->Nleft
      && gsamepure(yes->Nleft, no)) {
        id = no;
        if (!((cond->Nop == Q_LESS || cond->Nop == Q_LEQ)
              && gsamepure(cond->Nleft, id) && gabszero(cond->Nright))
          && !((cond->Nop == Q_GREAT || cond->Nop == Q_GEQ)
              && gabszero(cond->Nleft) && gsamepure(cond->Nright, id)))
            return NULL;
    } else if (no->Nop == N_NEG && no->Nleft
      && gsamepure(yes, no->Nleft)) {
        id = yes;
        if (!((cond->Nop == Q_GREAT || cond->Nop == Q_GEQ)
              && gsamepure(cond->Nleft, id) && gabszero(cond->Nright))
          && !((cond->Nop == Q_LESS || cond->Nop == Q_LEQ)
              && gabszero(cond->Nleft) && gsamepure(cond->Nright, id)))
            return NULL;
    } else
        return NULL;

    if (tisunsign(id->Ntype) || tisanyvolat(id->Ntype) || sideffp(id))
        return NULL;

    /* Restrict the value itself to addressable scalar lvalues.  The address
    ** expression beneath N_PTR may be more complex (for example a[i]), but
    ** gsamepure() requires it to be structurally identical and side-effect
    ** free at every occurrence.
    */
    if (id->Nop != Q_IDENT && id->Nop != N_PTR
      && id->Nop != Q_DOT && id->Nop != Q_MEMBER)
        return NULL;

    return id;
}

static VREG *
gabsquery(NODE *n)
{
    NODE *id;
    VREG *r;

    id = gabsquery_id(n);
    if (!id)
        return NULL;

    if (!Register_Id(id)) {
        r = vrget();
        r->Vrtype = id->Ntype;
        code4(P_MOVM, r, gaddress(id));
        return r;
    }

    r = genexpr(id);
    code0(P_MOVM, r, r);
    return r;
}

static VREG *
gternary (NODE *n)
{
    SYMBOL *false, *done;
    int siz;
    NODE *nfirst, *nsecond;
    VREG *reg;
    int normalmerge;
    VREG *absreg;
#if NEWTERN
    int uptof = 0;
    SYMBOL *savupto;
#endif

    absreg = gabsquery(n);
    if (absreg)
        return absreg;

    /* find the pieces of code we're going to use */
    siz = sizetype(n->Ntype);		/* Find size of overall result */
    if (n->Nflag & NF_DISCARD)		/* If result being discarded, */
	siz = 0;			/* pretend size is 0 (void) */
    nfirst = n->Nright->Nleft;		/* First (if-true) result expr */
    nsecond = n->Nright->Nright;	/* Second (if-false) result expr */

    /* Check for (very unlikely) case of both results non-existent.
    ** CCEVAL's evaldiscard() should have substituted some other node op, 
    ** so this may be a bug.  We handle this mainly to ensure following
    ** code is guaranteed of having at least one result expr.
    */
    if (!nfirst && !nsecond)		/* Check for unlikely case */
	{
	if (siz != 0)			/* Overall result must be void! */
	    int_error("gternary: no operands %N", n);
	genxrelease(n->Nleft);		/* Generate condition */
	return NULL;
	}
    if (n->Ntype->Tspec == TS_ARRAY)	/* Another just-in-case check */
	{
	int_error("gternary: array type %N", n);
	siz = 0;
	}

    normalmerge = optgen && siz == 1 && gternary_normal_ok(n);
#if 1
    /* Clean up previously allocated return registers unless liveness
    ** analysis marked this scalar conditional for a normal-AC merge.
    */
    if (siz == 2)
	vrfree(vrretdget());		/* Make sure ACs 1 & 2 free */
    else if (siz >= 1 && !normalmerge)
	vrfree(vrretget());	/* else just ensure AC1 free */
    /* Else void return value */
#endif

    false = (nfirst && nsecond)			/* If we'll need it, */
		 ? newlabel() : NULL;		/* get a new label for false */
    done = (n->Nendlab ?			/* Get overall end label */
		n->Nendlab : newlabel());
    if (nfirst)
	nfirst->Nendlab = done;		/* Make that be expr endlab */
    if (nsecond)
	nsecond->Nendlab = done;
    if (!false)				/* If don't have both exprs */
	false = done;			/* then use endlab as false jump */

    /* Now just before generating the code to test a condition and branch,
    ** we have to ensure that any active registers are saved.  See note at
    ** top of page.
    */
    vrallspill();
    if (normalmerge) {
        reg = vrget();
        reg->Vrtype = n->Ntype;
    }

    /* There are three possible configurations:
    ** (1) Both nfirst and nsecond exist.  Failing test jumps to "false".
    **		Both results must be moved to a common register.
    ** (2) Only nfirst exists.  Failing test jumps to "done".
    **		We return the register nfirst gives, if any.
    ** (3) Only nsecond exists.  Test is REVERSED; if fails, jumps to "done".
    **		We return the register nsecond gives, if any.
    ** We've already set up the "false" label to be the same as "done"
    ** if either of the latter two cases holds.
    */
    if (!normalmerge)
        reg = NULL;		/* Ensure no return reg initially */
    gboolean(n->Nleft, false,		/* Generate code to test condition */
		nfirst == NULL);	/* (reverse sense if 1st is gone) */

    if (nfirst)
	{
	if (siz > 0)
	    {
#if 0 /*NEWTERN*/
	    reg = genexpr(nfirst);	/* Just return what we get */
	    if (optgen && nsecond		/* If optimizing, and other */
	     && (nsecond->Nop == N_FNCALL	/* val is a function call, */
		|| (nsecond->Nop == N_CAST &&
		    nsecond->Nleft->Nop == N_FNCALL))
	     && vrtoreal(reg) != R_RETVAL)	/* and 1st val in diff reg, */
		{
		gretmove(n->Ntype, VR_RETVAL, reg);
		reg = (siz == 2 ? vrdget() : vrget());
		}
	    else if (optgen)		/* One more optimization try */
		backreg(reg);		/* Flush a MOVE R,S as we don't care */
					/* at this point what phys reg is */
#else
            if (normalmerge) {
                if (Register_Id(nfirst))
                    code00(P_MOVE, reg->Vrloc, nfirst->Nid->Sreg);
                else
                    gretmove(n->Ntype, reg, genexpr(nfirst));
            } else if (Register_Id(nfirst))
		code00(P_MOVE, R_RETVAL, nfirst->Nid->Sreg);
	    else
		gretmove(n->Ntype, VR_RETVAL, genexpr(nfirst));
#endif
	    }
	else
	    genxrelease(nfirst);
	if (nsecond)
	    {
	    code6(P_JRST, (VREG *)NULL, done);	/* skip over the hard way */
	    codlabel(false);			/* now start second part */
	    }
	}

    if (nsecond)
	{
	if (siz > 0)
	    {
#if NEWTERN
	    if (nfirst)
		{
		savupto = cregupto(done);	/* Set fence for changereg */
		uptof++;			/* say fence set */
		}
            if (normalmerge) {
                if (Register_Id(nsecond))
                    code00(P_MOVE, reg->Vrloc, nsecond->Nid->Sreg);
                else
                    gretmove(n->Ntype, reg, genexpr(nsecond));
            } else if (Register_Id(nsecond))
		code00(P_MOVE, R_RETVAL, nsecond->Nid->Sreg);
	    else
		gretmove(n->Ntype, VR_RETVAL, genexpr(nsecond));
#else
            if (normalmerge)
                gretmove(n->Ntype, reg, genexpr(nsecond));
            else
	        gretmove(n->Ntype, VR_RETVAL, genexpr(nsecond));
#endif
	    }
	else
	    genxrelease(nsecond);
	}

    if (n->Nendlab == NULL)
	{
	codlabel(done);		/* second clause done here */
#if NEWTERN
	if (uptof)
	    {
	    cregupto(savupto);	/* Restore saved fence value */
	    uptof = 0;
	    }
#if 1
	/* If only one result register was used, try to make it a "normal"
	** (non-return) reg to avoid hogging reg 1 and interfering with
	** common sub-expression matching.
	*/
	if (!normalmerge && siz > 0 && siz != 2	/* Only one register? */
		 && optobj)
	    {
	    reg = vrget();		/* Get normal reg */
	    reg->Vrtype = n->Ntype;	/* Set C type of result */
	    if (codcreg(reg, VR_RETVAL))
		return reg;
	    vrfree(reg);		/* didn't work, put back that reg. */
	    }
#endif
#endif
	}

#if NEWTERN
    if (uptof)
	cregupto(savupto);	/* Restore saved fence if one */
#if 0
    if (reg)
#else
    if (siz <= 0)
	return NULL;		/* Void */
    if (!normalmerge)
        reg = (tisdimode(n->Ntype) || siz == 2 ? vrretdget() : vrretget());
#endif
#endif
    reg->Vrtype = n->Ntype;		/* Set C type of result obj */
    return reg;
}

/* Split a long long constant node into high/low 36-bit words. */

/* Return the bit number of an exact positive 71-bit power of two. */
static int
dimode_power2_exp(INT hi, INT lo)
{
    int n;

    hi &= dimode_hi36mask();
    lo &= dimode_lo35mask();
    if (hi != 0 && lo != 0)
        return -1;
    if (lo != 0) {
        if ((lo & (lo - 1)) != 0)
            return -1;
        for (n = 0; lo > 1; lo >>= 1)
            ++n;
        return n;
    }
    if (hi != 0) {
        if ((hi & (hi - 1)) != 0)
            return -1;
        for (n = 35; hi > 1; hi >>= 1)
            ++n;
        return n;
    }
    return -1;
}

/* Return the bit number of an exact negative 71-bit power of two. */
static int
dimode_negative_power2_exp(INT hi, INT lo)
{
    hi &= dimode_hi36mask();
    lo &= dimode_lo35mask();
    if ((hi & ((INT)1 << 35)) == 0)
        return -1;

    hi = -hi;
    if (lo != 0)
        --hi;
    lo = -lo;
    return dimode_power2_exp(hi, lo);
}

/* Load integral DImode value from memory at idx into pair q. */
static void
gdimemload(VREG *q, VREG *idx, TYPE *t, int keep)
{
    int ar;

    ar = vrreal(idx);
    codek4(P_MOVE, q, idx);
    codemdx(P_MOVE, vrreal(VR2(q)), NULL, 1, ar);
    if (!tisunsign(t))
        code8(P_TLZ, VR2(q), 0400000L);
    if (!keep)
	vrfree(idx);
}

/* Store integral DImode pair reg at memory address ra. */
static void
gdimemstore(VREG *reg, VREG *ra)
{
    int ar, hi, lo, n;
    int hflags, lflags;
    char buf[40];

    /*
     * Keep the two DImode value ACs stable while the destination address is
     * forced into a real AC.  Non-optimized code can otherwise reload the
     * address into the AC recorded for the low DImode half and then emit a
     * store of the address word instead of the saved low value.
     */
    (void) vrstoreal(reg, VR2(reg));
    hi = vrreal(reg);
    lo = vrreal(VR2(reg));
    hflags = reg->Vrflags;
    lflags = VR2(reg)->Vrflags;
    reg->Vrflags |= VRF_LOCK;
    VR2(reg)->Vrflags |= VRF_LOCK;
    ar = vrtoreal(ra);
    reg->Vrflags = hflags;
    VR2(reg)->Vrflags = lflags;
    flushcode();
    /*
     * Keep this sequence out of the peephole/allocator path: AR and the
     * DImode pair were fixed above and must remain fixed through both stores.
     * Emit one short line at a time rather than building the whole signed
     * sequence in a fixed text buffer.  The latter exceeded 80 bytes once
     * GCC-compatible low-word sign copying was added.
     */
    if (reg->Vrtype && !tisunsign(reg->Vrtype)) {
        n = kccfmt(buf, sizeof(buf), "\tTLZ\t%o,0400000\n", lo);
        codestr(buf, n);
        n = kccfmt(buf, sizeof(buf), "\tTLNE\t%o,0400000\n", hi);
        codestr(buf, n);
        n = kccfmt(buf, sizeof(buf), "\t TLO\t%o,0400000\n", lo);
        codestr(buf, n);
    }
    n = kccfmt(buf, sizeof(buf), "\tMOVEM\t%o,0(%o)\n", hi, ar);
    codestr(buf, n);
    n = kccfmt(buf, sizeof(buf), "\tMOVEM\t%o,1(%o)\n", lo, ar);
    codestr(buf, n);
    if (reg->Vrtype && !tisunsign(reg->Vrtype)) {
        n = kccfmt(buf, sizeof(buf), "\tTLZ\t%o,0400000\n", lo);
        codestr(buf, n);
    }
    vrfree(ra);
}


/* Widen a one-word integral value into KCC's two-word DImode form. */
static VREG *
gdimode_from_int(VREG *r, TYPE *tfrom, TYPE *tto, NODE *ln)
{
    VREG *q;
    int hi, lo, n;
    int rflags;
    char buf[160];

    if (!r)
	return (VREG *)-1;
    if (tisdimode(tfrom))
	return r;

    if (tisunsign(tfrom))
	r = gintwiden(r, tfrom, uinttype, ln);
    else
	r = gintwiden(r, tfrom, inttype, ln);
    if (!r)
	return (VREG *)-1;

    /*
     * Do not widen the existing scalar VREG in place.  The low-direction
     * vrwiden path can leave stale virtual-register tracking in -n output,
     * so later DImode stores use the wrong AC for the low word.  Allocate a
     * fresh pair, copy the scalar into the low half, and then synthesize the
     * sign/zero high half in the tracked pair.
     */
    rflags = r->Vrflags;
    r->Vrflags |= VRF_LOCK;
    q = vrdget();
    r->Vrflags = rflags;
    q->Vrtype = tto;
    VR2(q)->Vrtype = tto;

    (void) vrstoreal(q, VR2(q));
    (void) vrtoreal(r);
    hi = vrreal(q);
    lo = vrreal(VR2(q));
    n = kccfmt(buf, sizeof(buf),
	"\tMOVE\t%o,0%o\n"
	"\tMOVE\t%o,0%o\n"
	"\t%s\t%o,-043\n"
	"\tAND\t%o,[0377777777777]\n",
	lo, vrreal(r),
	hi, vrreal(r),
	(tisunsign(tfrom) ? "LSH" : "ASH"), hi,
	lo);
    codestr(buf, n);
    vrfree(r);
    return q;
}

/* Copy integral DImode pair src into pair dest. */
static void
gdimove(VREG *dest, VREG *src)
{
    code0(P_MOVE, dest, src);
    code0(P_MOVE, VR2(dest), VR2(src));
}

/* Move expression result into return-value register(s). */
static void
gretmove(TYPE *t, VREG *dst, VREG *src)
{
    if (tisdimode(t))
	gdimove(dst, src);
    else if (sizetype(t) == 2)
	code0(P_DMOVE, dst, src);
    else
	code0(P_MOVE, dst, src);
}

/* Return a free physical AC not listed in avoid[0..navoid-1], or 0. */
static int
gdimode_try_ac(int avoid[], int navoid)
{
    int r, i, conflict;

    for (r = 1; r < NREGS; r++)
	{
	if (!rfree(r))
	    continue;
	conflict = 0;
	for (i = 0; i < navoid; i++)
	    if (avoid[i] == r)
		{
		conflict = 1;
		break;
		}
	if (!conflict)
	    return r;
	}
    return 0;
}

/* Return the high AC of a free adjacent pair outside the avoid set. */
static int
gdimode_try_pair(int avoid[], int navoid)
{
    int r, i, conflict;

    for (r = 1; r + 1 < NREGS; r++)
	{
	if (!rfree(r) || !rfree(r + 1))
	    continue;
	conflict = 0;
	for (i = 0; i < navoid; i++)
	    if (avoid[i] == r || avoid[i] == r + 1)
		{
		conflict = 1;
		break;
		}
	if (!conflict)
	    return r;
	}
    return 0;
}

/* Pick a free physical AC not listed in avoid[0..navoid-1]. */
static int
gdimode_pick_ac(int avoid[], int navoid)
{
    int r;

    if ((r = gdimode_try_ac(avoid, navoid)) != 0)
	return r;
    int_error("gdimode_pick_ac: no scratch AC");
    return 1;
}


/* Emit a final true-jump for signed DImode relational comparisons.
** KCC's DImode value is carried as a signed high word plus a masked low
** word.  Signed ordering therefore compares the high word first, and only
** compares the low word when the high words are equal.  The low word is
** kept below 2^32, so a signed subtract is safe for the equality case on
** the PDP-6/KA10 baseline.  The generated JUMPN skips the following JRST
** emitted by gboolean() when the relation is true. */

static int
gdimode_signed_relop_skip(VREG *r1, VREG *r2, int op)
{
    static int labno = 0;
    int rhi, rlo;
    int th, tl, flag;
    int avoid[5], navoid;
    char buf[1400];
    int n, lab;
    char *hrel, *lrel;

    switch (op)
	{
	case P_CAM+POF_ISSKIP+POS_SKPL:
	    hrel = "CAML";
	    lrel = "CAML";
	    break;
	case P_CAM+POF_ISSKIP+POS_SKPLE:
	    hrel = "CAML";
	    lrel = "CAMLE";
	    break;
	case P_CAM+POF_ISSKIP+POS_SKPG:
	    hrel = "CAMG";
	    lrel = "CAMG";
	    break;
	case P_CAM+POF_ISSKIP+POS_SKPGE:
	    hrel = "CAMG";
	    lrel = "CAMGE";
	    break;
	default:
	    return 0;
	}

    /* Fast path: compare two resident, non-overlapping pairs directly.
    ** Only one flag AC is needed; both operands die after the comparison. */
    if (!(r1->Vrflags & VRF_SPILLED)
      && !(VR2(r1)->Vrflags & VRF_SPILLED)
      && !(r2->Vrflags & VRF_SPILLED)
      && !(VR2(r2)->Vrflags & VRF_SPILLED))
	{
	int a[5], na, f;
	int h1 = vrreal(r1), l1 = vrreal(VR2(r1));
	int h2 = vrreal(r2), l2 = vrreal(VR2(r2));

	if (h1 != l1 && h1 != h2 && h1 != l2
	  && l1 != h2 && l1 != l2 && h2 != l2)
	    {
	    na = 0;
	    a[na++] = h1; a[na++] = l1;
	    a[na++] = h2; a[na++] = l2;
	    if ((f = gdimode_try_ac(a, na)) != 0)
		{
		lab = labno++;
		n = kccfmt(buf, sizeof(buf),
		    "\tSETZ\t%o,\n"
		    "\t%s\t%o,%o\n"
		    "\tJRST\t%%DICMP%dH\n"
		    "\tJRST\t%%DICMP%dT\n"
		    "%%DICMP%dH:\n"
		    "\tCAME\t%o,%o\n"
		    "\tJRST\t%%DICMP%dD\n"
		    "\tJRST\t%%DICMP%dE\n"
		    "%%DICMP%dE:\n"
		    "\t%s\t%o,%o\n"
		    "\tJRST\t%%DICMP%dD\n"
		    "%%DICMP%dT:\n"
		    "\tMOVEI\t%o,1\n"
		    "%%DICMP%dD:\n"
		    "\tJUMPN\t%o,.+2\n",
		    f, hrel, h1, h2, lab, lab, lab,
		    h1, h2, lab, lab, lab, lrel, l1, l2,
		    lab, lab, f, lab, f);
		codestr(buf, n);
		vrfree(r1);
		vrfree(r2);
		return 1;
		}
	    }
	}

    (void) vrstoreal(r1, VR2(r1));
    flushcode();
    n = kccfmt(buf, sizeof(buf),
	"\tPUSH\t17,%o\n"
	"\tPUSH\t17,%o\n",
	vrreal(r1), vrreal(VR2(r1)));
    codestr(buf, n);
    stackoffset += 2;

    (void) vrstoreal(r2, VR2(r2));
    flushcode();
    rhi = vrreal(r2);
    rlo = vrreal(VR2(r2));

    navoid = 0;
    avoid[navoid++] = rhi;
    avoid[navoid++] = rlo;
    th = gdimode_pick_ac(avoid, navoid);
    avoid[navoid++] = th;
    tl = gdimode_pick_ac(avoid, navoid);
    avoid[navoid++] = tl;
    flag = gdimode_pick_ac(avoid, navoid);
    lab = labno++;

    n = kccfmt(buf, sizeof(buf),
	"\tSETZ\t%o,\n"
	"\tMOVE\t%o,-1(17)\n"
	"\tMOVE\t%o,0(17)\n"
	"\t%s\t%o,%o\n"
	"\tJRST\t%%DICMP%dH\n"
	"\tJRST\t%%DICMP%dT\n"
	"%%DICMP%dH:\n"
	"\tCAME\t%o,%o\n"
	"\tJRST\t%%DICMP%dD\n"
	"\tJRST\t%%DICMP%dE\n"
	"%%DICMP%dE:\n"
	"\t%s\t%o,%o\n"
	"\tJRST\t%%DICMP%dD\n"
	"%%DICMP%dT:\n"
	"\tMOVEI\t%o,1\n"
	"%%DICMP%dD:\n"
	"\tSUB\t17,[2,,2]\n"
	"\tJUMPN\t%o,.+2\n",
	flag,
	th,
	tl,
	hrel, th, rhi,
	lab,
	lab,
	lab,
	th, rhi,
	lab,
	lab,
	lab,
	lrel, tl, rlo,
	lab,
	lab,
	flag,
	lab,
	flag);
    codestr(buf, n);
    stackoffset -= 2;
    vrfree(r1);
    vrfree(r2);
    vrunspillall();
    return 1;
}

/* Emit a final true-jump for unsigned DImode relational comparisons.
** Both halves are compared in unsigned order by toggling the 36-bit sign
** bit before using the machine's signed CAM skip instructions.  The final
** JUMPN has the same "skip if true" contract as gdimode_signed_relop_skip(). */
static int
gdimode_unsigned_relop_skip(VREG *r1, VREG *r2, int op)
{
    static int labno = 0;
    int rhi, rlo;
    int th, tl, flag;
    int avoid[5], navoid;
    char buf[1600];
    int n, lab;
    char *hrel, *lrel;

    switch (op)
	{
	case P_CAM+POF_ISSKIP+POS_SKPL:
	    hrel = "CAML";
	    lrel = "CAML";
	    break;
	case P_CAM+POF_ISSKIP+POS_SKPLE:
	    hrel = "CAML";
	    lrel = "CAMLE";
	    break;
	case P_CAM+POF_ISSKIP+POS_SKPG:
	    hrel = "CAMG";
	    lrel = "CAMG";
	    break;
	case P_CAM+POF_ISSKIP+POS_SKPGE:
	    hrel = "CAMG";
	    lrel = "CAMGE";
	    break;
	default:
	    return 0;
	}

    /* Fast path: compare resident pairs directly.  Unsigned high-word
    ** ordering is obtained by toggling the sign bit in place; low words
    ** are already masked to 35 bits and therefore compare as nonnegative. */
    if (!(r1->Vrflags & VRF_SPILLED)
      && !(VR2(r1)->Vrflags & VRF_SPILLED)
      && !(r2->Vrflags & VRF_SPILLED)
      && !(VR2(r2)->Vrflags & VRF_SPILLED))
	{
	int a[5], na, f;
	int h1 = vrreal(r1), l1 = vrreal(VR2(r1));
	int h2 = vrreal(r2), l2 = vrreal(VR2(r2));

	if (h1 != l1 && h1 != h2 && h1 != l2
	  && l1 != h2 && l1 != l2 && h2 != l2)
	    {
	    na = 0;
	    a[na++] = h1; a[na++] = l1;
	    a[na++] = h2; a[na++] = l2;
	    if ((f = gdimode_try_ac(a, na)) != 0)
		{
		lab = labno++;
		n = kccfmt(buf, sizeof(buf),
		    "\tTLC\t%o,0400000\n"
		    "\tTLC\t%o,0400000\n"
		    "\tSETZ\t%o,\n"
		    "\t%s\t%o,%o\n"
		    "\tJRST\t%%DIUCMP%dH\n"
		    "\tJRST\t%%DIUCMP%dT\n"
		    "%%DIUCMP%dH:\n"
		    "\tCAME\t%o,%o\n"
		    "\tJRST\t%%DIUCMP%dD\n"
		    "\tJRST\t%%DIUCMP%dE\n"
		    "%%DIUCMP%dE:\n"
		    "\t%s\t%o,%o\n"
		    "\tJRST\t%%DIUCMP%dD\n"
		    "%%DIUCMP%dT:\n"
		    "\tMOVEI\t%o,1\n"
		    "%%DIUCMP%dD:\n"
		    "\tJUMPN\t%o,.+2\n",
		    h1, h2, f, hrel, h1, h2, lab, lab, lab,
		    h1, h2, lab, lab, lab, lrel, l1, l2,
		    lab, lab, f, lab, f);
		codestr(buf, n);
		vrfree(r1);
		vrfree(r2);
		return 1;
		}
	    }
	}

    (void) vrstoreal(r1, VR2(r1));
    flushcode();
    n = kccfmt(buf, sizeof(buf),
	"\tPUSH\t17,%o\n"
	"\tPUSH\t17,%o\n",
	vrreal(r1), vrreal(VR2(r1)));
    codestr(buf, n);
    stackoffset += 2;

    (void) vrstoreal(r2, VR2(r2));
    flushcode();
    rhi = vrreal(r2);
    rlo = vrreal(VR2(r2));

    navoid = 0;
    avoid[navoid++] = rhi;
    avoid[navoid++] = rlo;
    th = gdimode_pick_ac(avoid, navoid);
    avoid[navoid++] = th;
    tl = gdimode_pick_ac(avoid, navoid);
    avoid[navoid++] = tl;
    flag = gdimode_pick_ac(avoid, navoid);
    lab = labno++;

    n = kccfmt(buf, sizeof(buf),
	"\tSETZ\t%o,\n"
	"\tMOVE\t%o,-1(17)\n"
	"\tTLC\t%o,0400000\n"
	"\tMOVE\t%o,0(17)\n"
	"\tTLC\t%o,0400000\n"
	"\tTLC\t%o,0400000\n"
	"\t%s\t%o,%o\n"
	"\tJRST\t%%DIUCMP%dH\n"
	"\tJRST\t%%DIUCMP%dT\n"
	"%%DIUCMP%dH:\n"
	"\tCAME\t%o,%o\n"
	"\tJRST\t%%DIUCMP%dD\n"
	"\tJRST\t%%DIUCMP%dE\n"
	"%%DIUCMP%dE:\n"
	"\t%s\t%o,%o\n"
	"\tJRST\t%%DIUCMP%dD\n"
	"%%DIUCMP%dT:\n"
	"\tMOVEI\t%o,1\n"
	"%%DIUCMP%dD:\n"
	"\tSUB\t17,[2,,2]\n"
	"\tJUMPN\t%o,.+2\n",
	flag,
	th, th,
	tl, tl,
	rhi,
	hrel, th, rhi,
	lab,
	lab,
	lab,
	th, rhi,
	lab,
	lab,
	lab,
	lrel, tl, rlo,
	lab,
	lab,
	flag,
	lab,
	flag);
    codestr(buf, n);
    stackoffset -= 2;
    vrfree(r1);
    vrfree(r2);
    vrunspillall();
    return 1;
}

/* 71-bit DImode add/sub in one codestr (refs adddi_reg_reg / subdi_reg). */
static VREG *
gdimode_addsub(VREG *r1, VREG *r2, int is_sub)
{
    int r1hi, r1lo, r2hi, r2lo;
    char buf[384];
    int n;

    /* The low halves are canonical unsigned 35-bit values.  Their sign bit
    ** after ADD/SUB is therefore exactly the carry/borrow bit; apply it
    ** directly to the high result instead of allocating a scratch AC or
    ** spilling all live registers under pressure. */
    if (!(r1->Vrflags & VRF_SPILLED)
      && !(VR2(r1)->Vrflags & VRF_SPILLED)
      && !(r2->Vrflags & VRF_SPILLED)
      && !(VR2(r2)->Vrflags & VRF_SPILLED))
        {
        r1hi = vrreal(r1);
        r1lo = vrreal(VR2(r1));
        r2hi = vrreal(r2);
        r2lo = vrreal(VR2(r2));
        if (r1hi == r2hi || r1hi == r2lo
          || r1lo == r2hi || r1lo == r2lo)
            r1hi = 0;
        }
    else
        r1hi = 0;

    if (r1hi == 0)
        {
        vrallspill();
        (void) vrstoreal(r1, VR2(r1));
        (void) vrstoreal(r2, VR2(r2));
        (void) vrstoreal(r2, r1);
        r1hi = vrreal(r1);
        r1lo = vrreal(VR2(r1));
        r2hi = vrreal(r2);
        r2lo = vrreal(VR2(r2));
        }

    if (is_sub)
        {
        static int sublab;
        int lab = sublab++;
        n = kccfmt(buf, sizeof(buf),
            "\tSUB\t%o,%o\n"
            "\tJUMPGE\t%o,%%DISUB%d\n"
            "\tADD\t%o,[0400000000000]\n"
            "\tSUBI\t%o,1\n"
            "%%DISUB%d:\n"
            "\tSUB\t%o,%o\n",
            r1lo, r2lo, r1lo, lab, r1lo, r1hi, lab, r1hi, r2hi);
        }
    else
        {
        static int addlab;
        int lab = addlab++;
        n = kccfmt(buf, sizeof(buf),
            "\tADD\t%o,%o\n"
            "\tJUMPGE\t%o,%%DIADD%d\n"
            "\tAND\t%o,[0377777777777]\n"
            "\tADDI\t%o,1\n"
            "%%DIADD%d:\n"
            "\tADD\t%o,%o\n",
            r1lo, r2lo, r1lo, lab, r1lo, r1hi, lab, r1hi, r2hi);
        }
    codestr(buf, n);
    vrfree(r2);
    return r1;
}

/* 71-bit DImode add: r1 += r2, release r2. */
static VREG *
gdimodeadd(VREG *r1, VREG *r2)
{
    return gdimode_addsub(r1, r2, 0);
}

/* 71-bit DImode subtract: r1 -= r2, release r2. */
static VREG *
gdimodesub(VREG *r1, VREG *r2)
{
    return gdimode_addsub(r1, r2, 1);
}

/* 71-bit DImode multiply: r1 *= r2, release r2 (muldi3_71_no_dmul). */
static VREG *
gdimodemul(VREG *r1, VREG *r2)
{
    int r1hi, r1lo, r2hi, r2lo;
    int ahi, alo, bhi, blo;
    int scrlo, prodhi, prodlo;
    int avoid[9], navoid;
    char buf[512];
    int n;

    (void) vrstoreal(r1, VR2(r1));
    (void) vrstoreal(r2, VR2(r2));
    (void) vrstoreal(r2, r1);

    ahi = r1hi = vrreal(r1);
    alo = r1lo = vrreal(VR2(r1));
    bhi = r2hi = vrreal(r2);
    blo = r2lo = vrreal(VR2(r2));

    navoid = 0;
    avoid[navoid++] = r1hi;
    avoid[navoid++] = r1lo;
    avoid[navoid++] = r2hi;
    avoid[navoid++] = r2lo;
    scrlo = gdimode_pick_ac(avoid, navoid);
    avoid[navoid++] = scrlo;

    /* Both input pairs remain live and unmodified until the final copy-out.
    ** The old four-AC snapshot was only needed by the former vrallspill path. */
    prodhi = gdimode_try_pair(avoid, navoid);
    if (prodhi == 0)
	int_error("gdimodemul: no product AC pair");
    prodlo = prodhi + 1;
    avoid[navoid++] = prodhi;
    avoid[navoid++] = prodlo;

    /* ACs are addressable as memory locations 0-17.  The operand
    ** snapshots can therefore feed MUL/IMUL directly; do not round-trip
    ** them through the stack merely to obtain a memory operand. */
    n = kccfmt(buf, sizeof(buf),
	"\tMOVE\t%o,0%o\n"
	"\tMUL\t%o,0%o\n"
	"\tAND\t%o,[0377777777777]\n",
	prodhi, alo,
	prodhi, blo,
	prodlo);
    codestr(buf, n);

    /* Cross terms: ahi*blo and alo*bhi. */
    n = kccfmt(buf, sizeof(buf),
	"\tMOVE\t%o,0%o\n"
	"\tIMUL\t%o,0%o\n"
	"\tADD\t%o,0%o\n"
	"\tMOVE\t%o,0%o\n"
	"\tIMUL\t%o,0%o\n"
	"\tADD\t%o,0%o\n",
	scrlo, ahi, scrlo, blo, prodhi, scrlo,
	scrlo, bhi, scrlo, alo, prodhi, scrlo);
    codestr(buf, n);

    /* The result is computed modulo 2^71.  ahi and bhi are already the
    ** upper two's-complement words, so adding the two cross products to
    ** the carry from alo*blo gives the correct high 36 bits for both
    ** signed and unsigned multiplication.  No separate sign correction
    ** is needed. */
    n = kccfmt(buf, sizeof(buf),
	"\tAND\t%o,[0377777777777]\n",
	prodlo);
    codestr(buf, n);

    r1hi = vrreal(r1);
    r1lo = vrreal(VR2(r1));
    /* prodhi/prodlo were allocated with both destination words in the
    ** avoid set, so the final copy has no overlap and needs no snapshots. */
    n = kccfmt(buf, sizeof(buf),
	"\tMOVE\t%o,0%o\n"
	"\tMOVE\t%o,0%o\n",
	r1hi, prodhi, r1lo, prodlo);
    codestr(buf, n);

    vrfree(r2);
    return r1;
}

/* 71-bit DImode divide/modulo.  This is a compact restoring divider used
** for the PDP-6/KA10 baseline where there is no useful native DImode DIV.
** Values are represented as high36:low35. */

/* Call a shared runtime DImode divider.  The helper uses the canonical
** AC1:AC2 / AC3:AC4 argument convention and returns AC1:AC2.  Other live
** volatile values are spilled, but the two operands remain resident until
** copied through four temporary stack words; this avoids destructive
** parallel-copy cycles without allocator state. */
static VREG *
gdimodehelper(VREG *r1, VREG *r2, int ts, int wantmod)
{
    SYMBOL *s;
    VREG *res;
    TYPE *rtype;
    char *name;
    int a1, a2, a3, a4;

    rtype = r1->Vrtype;
    vrspillothers(r1, r2);
    (void) vrstoreal(r1, r2);
    a1 = vrreal(r1);
    a2 = vrreal(VR2(r1));
    a3 = vrreal(r2);
    a4 = vrreal(VR2(r2));

    code00(P_PUSH, R_SP, a1);
    code00(P_PUSH, R_SP, a2);
    code00(P_PUSH, R_SP, a3);
    code00(P_PUSH, R_SP, a4);
    stackoffset += 4;
    vrfree(r1);
    vrfree(r2);

    codemdx(P_MOVE, 1, (SYMBOL *)NULL, -3, R_SP);
    codemdx(P_MOVE, 2, (SYMBOL *)NULL, -2, R_SP);
    codemdx(P_MOVE, 3, (SYMBOL *)NULL, -1, R_SP);
    codemdx(P_MOVE, 4, (SYMBOL *)NULL, 0, R_SP);

    if (tspisunsigned(ts))
        name = wantmod ? "__kcc_umoddi3" : "__kcc_udivdi3";
    else
        name = wantmod ? "__kcc_moddi3" : "__kcc_divdi3";
    s = symfidstr(name);
    if (s == NULL)
        {
        s = symgcreat(name);
        s->Sclass = SC_EXTREF;
        }
    ++s->Srefs;
    code6(P_PUSHJ, VR_SP, s);
    code8(P_ADJSP, VR_SP, -4);
    stackoffset -= 4;

    res = vrretdget();
    res->Vrtype = rtype;
    return res;
}

static VREG *
gdimodedivmod(VREG *r1, VREG *r2, int ts, int wantmod)
{
    int nhi, nlo, dhi, dlo;
    int qhi, qlo, rhi, rlo;
    int cnt, tmp1, tmp2, sgn;
    int avoid[16], navoid;
    int is_signed;
    static int divlab;
    int lab;
    char buf[4000];
    int n;

    int fast;

    is_signed = (ts == TS_LONGLONG);

    {
    int helperkind = (wantmod ? 1 : 0) + (tspisunsigned(ts) ? 2 : 0);
    if (((fndimodcalls >> (helperkind * 4)) & 017) > 1)
        return gdimodehelper(r1, r2, ts, wantmod);
    }

    /* Try the common register-resident case before disturbing any live
    ** virtual register.  Raw divider scratch ACs are safe only when every
    ** one is currently unassigned; if the complete set is unavailable, use
    ** the old spill path unchanged.  Operands have already been evaluated,
    ** so choosing either path cannot duplicate their side effects. */
    fast = !(r1->Vrflags & VRF_SPILLED)
        && !(VR2(r1)->Vrflags & VRF_SPILLED)
        && !(r2->Vrflags & VRF_SPILLED)
        && !(VR2(r2)->Vrflags & VRF_SPILLED);

retry_alloc:
    if (!fast)
        {
        vrallspill();
        (void) vrstoreal(r1, VR2(r1));
        (void) vrstoreal(r2, VR2(r2));
        (void) vrstoreal(r2, r1);
        }

    nhi = vrreal(r1);
    nlo = vrreal(VR2(r1));
    dhi = vrreal(r2);
    dlo = vrreal(VR2(r2));

    /* Distinct resident operand words are required because the restoring
    ** loop destructively shifts the numerator and normalizes the divisor. */
    if (fast && (nhi == nlo || nhi == dhi || nhi == dlo
        || nlo == dhi || nlo == dlo || dhi == dlo))
        {
        fast = 0;
        goto retry_alloc;
        }

    navoid = 0;
    avoid[navoid++] = nhi;
    avoid[navoid++] = nlo;
    avoid[navoid++] = dhi;
    avoid[navoid++] = dlo;
#define DIVAC(v) ((v = gdimode_try_ac(avoid, navoid)) != 0 \
                  ? (avoid[navoid++] = v, 1) : 0)
    qhi = qlo = 0;
    if ((!wantmod && (!DIVAC(qhi) || !DIVAC(qlo)))
        || !DIVAC(rhi) || !DIVAC(rlo) || !DIVAC(cnt)
        || !DIVAC(tmp1) || !DIVAC(tmp2)
        || (is_signed && !DIVAC(sgn)))
        {
        if (fast)
            {
            fast = 0;
            goto retry_alloc;
            }
        int_error("gdimodedivmod: no scratch AC");
        }
#undef DIVAC

    /* A signed operation returns either quotient or remainder, never both.
    ** Keep only the sign needed by that result: numerator XOR divisor for a
    ** quotient, numerator alone for a remainder. */
    if (!is_signed)
        sgn = 0;

    lab = divlab++;

    if (is_signed)
        {
        n = kccfmt(buf, sizeof(buf),
            "\tSETZ\t%o,\n"
            "\tMOVE\t%o,0%o\n"
            "\tLSH\t%o,-043\n"
            "\tANDI\t%o,1\n"
            "\tJUMPE\t%o,%%DIDIV%dN\n"
            "\tMOVN\t%o,0%o\n"
            "\tSKIPE\t%o\n"
            "\tSUBI\t%o,1\n"
            "\tMOVN\t%o,0%o\n"
            "\tAND\t%o,[0377777777777]\n"
            "%%DIDIV%dN:\n",
            sgn,
            sgn, nhi,
            sgn,
            sgn,
            sgn, lab,
            nhi, nhi,
            nlo,
            nhi,
            nlo, nlo,
            nlo,
            lab);
        codestr(buf, n);

        if (wantmod)
            n = kccfmt(buf, sizeof(buf),
                "\tMOVE\t%o,0%o\n"
                "\tLSH\t%o,-043\n"
                "\tANDI\t%o,1\n"
                "\tJUMPE\t%o,%%DIDIV%dD\n"
                "\tMOVN\t%o,0%o\n"
                "\tSKIPE\t%o\n"
                "\tSUBI\t%o,1\n"
                "\tMOVN\t%o,0%o\n"
                "\tAND\t%o,[0377777777777]\n"
                "%%DIDIV%dD:\n",
                tmp2, dhi,
                tmp2,
                tmp2,
                tmp2, lab,
                dhi, dhi,
                dlo,
                dhi,
                dlo, dlo,
                dlo,
                lab);
        else
            n = kccfmt(buf, sizeof(buf),
                "\tMOVE\t%o,0%o\n"
                "\tLSH\t%o,-043\n"
                "\tANDI\t%o,1\n"
                "\tXOR\t%o,0%o\n"
                "\tJUMPE\t%o,%%DIDIV%dD\n"
                "\tMOVN\t%o,0%o\n"
                "\tSKIPE\t%o\n"
                "\tSUBI\t%o,1\n"
                "\tMOVN\t%o,0%o\n"
                "\tAND\t%o,[0377777777777]\n"
                "%%DIDIV%dD:\n",
                tmp2, dhi,
                tmp2,
                tmp2,
                sgn, tmp2,
                tmp2, lab,
                dhi, dhi,
                dlo,
                dhi,
                dlo, dlo,
                dlo,
                lab);
        codestr(buf, n);
        }

    if (wantmod)
        n = kccfmt(buf, sizeof(buf),
            "\tAND\t%o,[0377777777777]\n"
            "\tAND\t%o,[0377777777777]\n"
            "\tSETZ\t%o,\n"
            "\tSETZ\t%o,\n"
            "\tMOVEI\t%o,0107\n"
            "%%DIDIV%dL:\n"
            /* bit = top bit of numerator; numerator <<= 1 */
            "\tMOVE\t%o,0%o\n"
            "\tLSH\t%o,-043\n"
            "\tANDI\t%o,1\n"
            "\tMOVE\t%o,0%o\n"
            "\tLSH\t%o,-042\n"
            "\tANDI\t%o,1\n"
            "\tLSH\t%o,1\n"
            "\tAND\t%o,[0377777777777]\n"
            "\tLSH\t%o,1\n"
            "\tIOR\t%o,0%o\n"
            "\tAND\t%o,[0777777777777]\n"
            /* rem = (rem << 1) | bit */
            "\tMOVE\t%o,0%o\n"
            "\tLSH\t%o,-042\n"
            "\tANDI\t%o,1\n"
            "\tLSH\t%o,1\n"
            "\tAND\t%o,[0377777777777]\n"
            "\tIOR\t%o,0%o\n"
            "\tLSH\t%o,1\n"
            "\tIOR\t%o,0%o\n"
            "\tAND\t%o,[0777777777777]\n"
            /* if rem < den, do not subtract */
            "\tMOVE\t%o,0%o\n"
            "\tTLC\t%o,0400000\n"
            "\tMOVE\t%o,0%o\n"
            "\tTLC\t%o,0400000\n"
            "\tCAML\t%o,0%o\n"
            "\t JRST\t%%DIDIV%dHC\n"
            "\tJRST\t%%DIDIV%dNS\n"
            "%%DIDIV%dHC:\n"
            "\tCAME\t%o,0%o\n"
            "\t JRST\t%%DIDIV%dSUB\n"
            "\tCAML\t%o,0%o\n"
            "\t JRST\t%%DIDIV%dSUB\n"
            "\tJRST\t%%DIDIV%dNS\n"
            "%%DIDIV%dSUB:\n"
            "\tSUB\t%o,0%o\n"
            "\tJUMPGE\t%o,%%DIDIV%dSB0\n"
            "\tADD\t%o,[0400000000000]\n"
            "\tSUBI\t%o,1\n"
            "%%DIDIV%dSB0:\n"
            "\tSUB\t%o,0%o\n"
            "%%DIDIV%dNS:\n"
            "\tSOJG\t%o,%%DIDIV%dL\n",
            nlo, dlo,
            rhi, rlo,
            cnt,
            lab,
            tmp2, nhi,
            tmp2,
            tmp2,
            tmp1, nlo,
            tmp1,
            tmp1,
            nlo,
            nlo,
            nhi,
            nhi, tmp1,
            nhi,
            tmp1, rlo,
            tmp1,
            tmp1,
            rlo,
            rlo,
            rlo, tmp2,
            rhi,
            rhi, tmp1,
            rhi,
            tmp1, rhi,
            tmp1,
            tmp2, dhi,
            tmp2,
            tmp1, tmp2,
            lab,
            lab,
            lab,
            tmp1, tmp2,
            lab,
            rlo, dlo,
            lab,
            lab,
            lab,
            rlo, dlo,
            rlo, lab,
            rlo,
            rhi,
            lab,
            rhi, dhi,
            lab,
            cnt, lab);
    else
        {
        n = kccfmt(buf, sizeof(buf),
        "\tAND\t%o,[0377777777777]\n"
        "\tAND\t%o,[0377777777777]\n"
        "\tSETZ\t%o,\n"
        "\tSETZ\t%o,\n"
        "\tSETZ\t%o,\n"
        "\tSETZ\t%o,\n"
        "\tMOVEI\t%o,0107\n"
        "%%DIDIV%dL:\n"
        /* q <<= 1 */
        "\tMOVE\t%o,0%o\n"
        "\tLSH\t%o,-042\n"
        "\tANDI\t%o,1\n"
        "\tLSH\t%o,1\n"
        "\tAND\t%o,[0377777777777]\n"
        "\tLSH\t%o,1\n"
        "\tIOR\t%o,0%o\n"
        "\tAND\t%o,[0777777777777]\n"
        /* bit = top bit of numerator; numerator <<= 1 */
        "\tMOVE\t%o,0%o\n"
        "\tLSH\t%o,-043\n"
        "\tANDI\t%o,1\n"
        "\tMOVE\t%o,0%o\n"
        "\tLSH\t%o,-042\n"
        "\tANDI\t%o,1\n"
        "\tLSH\t%o,1\n"
        "\tAND\t%o,[0377777777777]\n"
        "\tLSH\t%o,1\n"
        "\tIOR\t%o,0%o\n"
        "\tAND\t%o,[0777777777777]\n"
        /* rem = (rem << 1) | bit */
        "\tMOVE\t%o,0%o\n"
        "\tLSH\t%o,-042\n"
        "\tANDI\t%o,1\n"
        "\tLSH\t%o,1\n"
        "\tAND\t%o,[0377777777777]\n"
        "\tIOR\t%o,0%o\n"
        "\tLSH\t%o,1\n"
        "\tIOR\t%o,0%o\n"
        "\tAND\t%o,[0777777777777]\n"
        /* if rem < den, do not subtract */
        "\tMOVE\t%o,0%o\n"
        "\tTLC\t%o,0400000\n"
        "\tMOVE\t%o,0%o\n"
        "\tTLC\t%o,0400000\n"
        "\tCAML\t%o,0%o\n"
        "\t JRST\t%%DIDIV%dHC\n"
        "\tJRST\t%%DIDIV%dNS\n"
        "%%DIDIV%dHC:\n"
        "\tCAME\t%o,0%o\n"
        "\t JRST\t%%DIDIV%dSUB\n"
        "\tCAML\t%o,0%o\n"
        "\t JRST\t%%DIDIV%dSUB\n"
        "\tJRST\t%%DIDIV%dNS\n"
        "%%DIDIV%dSUB:\n"
        "\tSUB\t%o,0%o\n"
        "\tJUMPGE\t%o,%%DIDIV%dSB0\n"
        "\tADD\t%o,[0400000000000]\n"
        "\tSUBI\t%o,1\n"
        "%%DIDIV%dSB0:\n"
        "\tSUB\t%o,0%o\n"
        "\tIORI\t%o,1\n"
        "%%DIDIV%dNS:\n"
        "\tSOJG\t%o,%%DIDIV%dL\n",
        nlo, dlo,
        qhi, qlo,
        rhi, rlo,
        cnt,
        lab,
        tmp1, qlo,
        tmp1,
        tmp1,
        qlo,
        qlo,
        qhi,
        qhi, tmp1,
        qhi,
        tmp2, nhi,
        tmp2,
        tmp2,
        tmp1, nlo,
        tmp1,
        tmp1,
        nlo,
        nlo,
        nhi,
        nhi, tmp1,
        nhi,
        tmp1, rlo,
        tmp1,
        tmp1,
        rlo,
        rlo,
        rlo, tmp2,
        rhi,
        rhi, tmp1,
        rhi,
        tmp1, rhi,
        tmp1,
        tmp2, dhi,
        tmp2,
        tmp1, tmp2,
        lab,
        lab,
        lab,
        tmp1, tmp2,
        lab,
        rlo, dlo,
        lab,
        lab,
        lab,
        rlo, dlo,
        rlo, lab,
        rlo,
        rhi,
        lab,
        rhi, dhi,
        qlo,
        lab,
        cnt, lab);
        }
    codestr(buf, n);

    if (is_signed)
        {
        if (wantmod)
            {
            n = kccfmt(buf, sizeof(buf),
                "\tJUMPE\t%o,%%DIDIV%dMR\n"
                "\tMOVN\t%o,0%o\n"
                "\tSKIPE\t%o\n"
                "\tSUBI\t%o,1\n"
                "\tMOVN\t%o,0%o\n"
                "\tAND\t%o,[0377777777777]\n"
                "%%DIDIV%dMR:\n",
                sgn, lab,
                rhi, rhi,
                rlo,
                rhi,
                rlo, rlo,
                rlo,
                lab);
            codestr(buf, n);
            }
        else
            {
            n = kccfmt(buf, sizeof(buf),
                "\tJUMPE\t%o,%%DIDIV%dMQ\n"
                "\tMOVN\t%o,0%o\n"
                "\tSKIPE\t%o\n"
                "\tSUBI\t%o,1\n"
                "\tMOVN\t%o,0%o\n"
                "\tAND\t%o,[0377777777777]\n"
                "%%DIDIV%dMQ:\n",
                sgn, lab,
                qhi, qhi,
                qlo,
                qhi,
                qlo, qlo,
                qlo,
                lab);
            codestr(buf, n);
            }
        }

    n = kccfmt(buf, sizeof(buf),
        "\tMOVE\t%o,0%o\n"
        "\tMOVE\t%o,0%o\n",
        vrreal(r1), (wantmod ? rhi : qhi),
        vrreal(VR2(r1)), (wantmod ? rlo : qlo));
    codestr(buf, n);

    vrfree(r2);
    return r1;
}

/* 71-bit DImode negation in place (negdi2 / __negdi2). */
static void
gdimodeneg(VREG *r)
{
    int hi, lo;
    char buf[160];
    int n;

    (void) vrstoreal(r, VR2(r));
    hi = vrreal(r);
    lo = vrreal(VR2(r));

    /* Negate high36:low35 in place.  The high word needs one extra
    ** decrement exactly when the low 35-bit word is nonzero.  Masking the
    ** negated low word supplies modulo-2^35 reduction without a scratch AC. */
    n = kccfmt(buf, sizeof(buf),
	"\tMOVN\t%o,%o\n"
	"\tSKIPE\t%o\n"
	"\tSUBI\t%o,1\n"
	"\tMOVN\t%o,%o\n"
	"\tAND\t%o,[0377777777777]\n",
	hi, hi,
	lo,
	hi,
	lo, lo,
	lo);
    codestr(buf, n);
}

/* 71-bit DImode bitwise and/or/xor: independent ops on both words (anddi_reg_reg). */
static VREG *
gdimodebitwise(int op, VREG *r1, VREG *r2)
{
    int r1hi, r1lo, r2hi, r2lo;
    const char *mn;
    char buf[128];
    int n;

    /* Bitwise DImode operations need no scratch AC.  Preserve already
    ** resident, non-overlapping pairs instead of spilling every live value.
    ** Fall back to the historical path when either pair is spilled or the
    ** physical pairs overlap. */
    if (!(r1->Vrflags & VRF_SPILLED)
      && !(VR2(r1)->Vrflags & VRF_SPILLED)
      && !(r2->Vrflags & VRF_SPILLED)
      && !(VR2(r2)->Vrflags & VRF_SPILLED))
        {
        r1hi = vrreal(r1);
        r1lo = vrreal(VR2(r1));
        r2hi = vrreal(r2);
        r2lo = vrreal(VR2(r2));
        if (r1hi == r2hi || r1hi == r2lo
          || r1lo == r2hi || r1lo == r2lo)
            r1hi = 0;
        }
    else
        r1hi = 0;

    if (r1hi == 0)
        {
        vrallspill();
        (void) vrstoreal(r1, VR2(r1));
        (void) vrstoreal(r2, VR2(r2));
        (void) vrstoreal(r2, r1);

        r1hi = vrreal(r1);
        r1lo = vrreal(VR2(r1));
        r2hi = vrreal(r2);
        r2lo = vrreal(VR2(r2));
        }

    switch (op)
	{
	case Q_ANDT:
	case Q_ASAND:
	    mn = "AND";
	    break;
	case Q_OR:
	case Q_ASOR:
	    mn = "IOR";
	    break;
	case Q_XORT:
	case Q_ASXOR:
	    mn = "XOR";
	    break;
	default:
	    int_error("gdimodebitwise: bad op %d", op);
	    mn = "AND";
	}

    n = kccfmt(buf, sizeof(buf),
	"\t%s\t%o,%o\n"
	"\t%s\t%o,%o\n",
	mn, r1hi, r2hi,
	mn, r1lo, r2lo);
    codestr(buf, n);

    vrfree(r2);
    return r1;
}

/* 71-bit DImode ones-complement (~) on both words (onecmpldi_reg). */
static void
gdimodecompl(VREG *r)
{
    int hi, lo;
    char buf[64];
    int n;

    (void) vrstoreal(r, VR2(r));
    hi = vrreal(r);
    lo = vrreal(VR2(r));
    /* Both words carry the sign bit in the GCC ABI representation.
    ** Complement both complete words so ~(DImode)0 becomes (-1, -1). */
    n = kccfmt(buf, sizeof(buf),
        "\tSETCA\t%o,\n"
        "\tSETCA\t%o,\n",
        hi, lo);
    codestr(buf, n);
}

/* 71-bit DImode shift via LSHC/ASHC (ashldi3 / ashrdi3 / lshrdi3). */
static VREG *
gdimodeshift(int op, VREG *r1, VREG *r2, int ts)
{
    int hi, lo;
    int shop;
    int direct_ashc = 0;
    char buf[384];
    int n;

    (void) vrstoreal(r1, VR2(r1));
    (void) vrtoreal(r2);
    hi = vrreal(r1);
    lo = vrreal(VR2(r1));

    shop = P_LSHC;
    switch (op)
        {
        case Q_LSHFT:
        case Q_ASLSH:
            break;
        case Q_RSHFT:
        case Q_ASRSH:
            code0(P_MOVN, r2, r2); /* same as garithop scalar >> */
            if (ts != TS_ULONGLONG)
                {
                shop = P_ASHC;
                direct_ashc = 1;
                }
            break;
        default:
            int_error("gdimodeshift: bad op %d", op);
        }

    /* KCC DImode is a canonical high36:low35 pair.  PDP-10 ASHC already
    ** treats its low accumulator as a 35-bit continuation for arithmetic
    ** right shifts, so signed right shift can operate on that form directly.
    ** LSHC instead sees a contiguous 72-bit pair; left shifts and unsigned
    ** right shifts therefore need a temporary representation conversion.
    */
    if (!direct_ashc)
        {
        n = kccfmt(buf, sizeof(buf),
            "\tTLZ\t%o,0400000\n"
            "\tTRNE\t%o,1\n"
            "\t TLO\t%o,0400000\n"
            "\tLSH\t%o,-1\n",
            lo,
            hi,
            lo,
            hi);
        codestr(buf, n);
        }

    code4(shop, r1, r2);

    hi = vrreal(r1);
    lo = vrreal(VR2(r1));
    if (direct_ashc)
        {
        n = kccfmt(buf, sizeof(buf),
            "\tAND\t%o,[0377777777777]\n",
            lo);
        }
    else
        {
        n = kccfmt(buf, sizeof(buf),
            "\tLSH\t%o,1\n"
            "\tJUMPGE\t%o,.+2\n"
            "\t TRO\t%o,1\n"
            "\tAND\t%o,[0377777777777]\n",
            hi,
            lo,
            hi,
            lo);
        }
    codestr(buf, n);
    vrfree(r2);
    return r1;
}

/* GBOOLEAN - Generate code for boolean expressions
**	jump to false label if expr not true
**	reverse sense of test if reverse bit set
*/
void
gboolean(NODE *n, SYMBOL *false, int reverse)
{
    VREG *r;
    int op;

    if (n == NULL)		/* Paranoia: bug catcher */
	{
	int_error("gboolean: null arg");
	return;
	}

    /*
    ** The big switch.  Either we call some handler routine such as
    ** gor or gand, or we make a skip and then a jump.  If the former,
    ** we are done, and the call should tail recurse.  Otherwise, we
    ** need to add the jump to the given label, so we break from the switch.
    */

    switch (n->Nop)
	{
	case Q_NOT:
	    n->Nleft->Nendlab = n->Nendlab;	/* set up variables */
	    n = n->Nleft;			/* with parity switched */
	    reverse = !reverse;		/* for tail recursive call */
	    gboolean(n, false, reverse);	/* to self */
	    return;

	case Q_LAND:
	    if (reverse)
		gor(n, false, reverse); /* more tail recursion */
	    else
		gand(n, false, reverse);
	    return;

	case Q_LOR:
	    if (reverse)
		gand(n, false, reverse); /* still more */
	    else
		gor(n, false, reverse);
	    return;

	case Q_NEQ:
	case Q_LEQ:
	case Q_GEQ:
	case Q_LESS:
	case Q_GREAT:
	case Q_EQUAL:
	    gboolop(n, reverse);		/* comparison, make skip */
	    break;				/* followed by GOTO */

	case N_ICONST:
	case N_PCONST:
	    op = n->Niconst;		/* unconditional condition */
	    if (reverse && op)
		break;	/* jump when true and true? */
	    if (!reverse && !op)
		break;	/* jump when false and false? */
	    return;

	default:
	    n->Nendlab = NULL;	/* cond endlab is not expr endlab */
	    if ((r = genexpr(n)) != NULL)	/* get expression into reg (may be discarded)*/
		{
		int bits = tbitsize(n->Ntype);	/* Find # bits of value */
		if (bits < TGSIZ_WORD)		/* If not full wd, then */
		    r = guintwiden(r, bits, n);	/* widen it unsignedly! */
		code6(reverse? P_JUMP+POS_SKPN : P_JUMP+POS_SKPE,
			    r, false);	/* test and jump */
		vrfree(r);			/* now done with register */
		}
	    return;				/* don't make spurious P_JRST */
	}
    code6(P_JRST, (VREG *)NULL, false);	/* broke out, want a GOTO */
}

/* GOR - Generate || expression
*/
static void
gor(NODE *n, SYMBOL *false, int reverse)
{
    SYMBOL *lab;

    if ((lab = n->Nendlab) == 0)
	lab = newlabel(); /* get label */
    gboolean(n->Nleft, lab, !reverse);	/* output first clause */
    n->Nright->Nendlab = lab;		/* no more labels in second clause */
    gboolean(n->Nright, false, reverse);
    if (n->Nendlab == 0)
	codlabel(lab); /* send out made label */
}

/* GAND - Generate && expression
*/
static void
gand(NODE *n, SYMBOL *false, int reverse)
{
    n->Nright->Nendlab = n->Nendlab;
    gboolean(n->Nleft, false, reverse);
    gboolean(n->Nright, false, reverse);
}


/* GBOOLOPBASE - Map a relational tree operator to its CAM skip form. */
static int
gboolopbase(int nop)
{
    switch (nop) {
    case Q_EQUAL: return P_CAM+POF_ISSKIP+POS_SKPE;
    case Q_NEQ:   return P_CAM+POF_ISSKIP+POS_SKPN;
    case Q_LEQ:   return P_CAM+POF_ISSKIP+POS_SKPLE;
    case Q_GEQ:   return P_CAM+POF_ISSKIP+POS_SKPGE;
    case Q_LESS:  return P_CAM+POF_ISSKIP+POS_SKPL;
    case Q_GREAT: return P_CAM+POF_ISSKIP+POS_SKPG;
    default:
        int_error("gboolop: bad op %d", nop);
        return P_CAM+POF_ISSKIP+POS_SKPE;
    }
}

/* GBOOLMASKZERO - Emit a direct halfword test for (value & mask) ==/!= 0. */
static int
gboolmaskzero(NODE *n, int op, int reverse)
{
    NODE *a, *v, *mc;
    INT mask, low, high;
    VREG *r;

    if (n->Nop != Q_EQUAL && n->Nop != Q_NEQ)
        return 0;
    a = v = mc = NULL;
    if (n->Nright && (n->Nright->Nop == N_ICONST
      || n->Nright->Nop == N_PCONST) && n->Nright->Niconst == 0)
        a = n->Nleft;
    else if (n->Nleft && (n->Nleft->Nop == N_ICONST
      || n->Nleft->Nop == N_PCONST) && n->Nleft->Niconst == 0)
        a = n->Nright;

    if (a && a->Nop == Q_ANDT) {
        if (a->Nright && a->Nright->Nop == N_ICONST) {
            v = a->Nleft;
            mc = a->Nright;
        } else if (a->Nleft && a->Nleft->Nop == N_ICONST) {
            v = a->Nright;
            mc = a->Nleft;
        }
    }
    if (!v || !mc || sizetype(v->Ntype) != 1 || tisbytepointer(v->Ntype))
        return 0;

    mask = mc->Niconst & ((INT)0777777777777);
    low = mask & 0777777L;
    high = (mask >> 18) & 0777777L;
    if ((low == 0) == (high == 0))
        return 0;

    r = genexpr(v);
    if (reverse)
        op = revop(op);
    if (high == 0)
        code8(op ^ (P_CAM ^ P_TRN), r, low);
    else
        code8(op ^ (P_CAM ^ P_TLN), r, high);
    vrfree(r);
    return 1;
}

/* GBOOLZERO - Emit a direct CAI for a legal one-word zero comparison. */
static int
gboolzero(NODE *n, int op, int reverse)
{
    NODE *v;
    VREG *r;
    int zright;

    v = NULL;
    zright = 0;
    if (n->Nright
      && (n->Nright->Nop == N_ICONST || n->Nright->Nop == N_PCONST)
      && n->Nright->Niconst == 0) {
        v = n->Nleft;
        zright = 1;
    } else if ((n->Nop == Q_EQUAL || n->Nop == Q_NEQ)
      && n->Nleft
      && (n->Nleft->Nop == N_ICONST || n->Nleft->Nop == N_PCONST)
      && n->Nleft->Niconst == 0)
        v = n->Nright;

    if (!v || sizetype(v->Ntype) != 1 || tisbytepointer(v->Ntype))
        return 0;
    if (!(((n->Nop == Q_EQUAL || n->Nop == Q_NEQ)
          && (tisinteg(v->Ntype) || v->Ntype->Tspec == TS_PTR))
       || (zright && tisinteg(v->Ntype) && !tisunsign(v->Ntype))))
        return 0;

    r = genexpr(v);
    if (reverse)
        op = revop(op);
    code8(op ^ (P_CAM ^ P_CAI), r, 0);
    vrfree(r);
    return 1;
}

/* GDIMODEZERO - Emit a direct two-word zero test for == and !=. */
static int
gdimodezero(NODE *n, int op, int reverse)
{
    NODE *v;
    VREG *r;
    int hi, lo;

    if (n->Nop != Q_EQUAL && n->Nop != Q_NEQ)
        return 0;

    v = NULL;
    if (n->Nright
      && (n->Nright->Nop == N_ICONST || n->Nright->Nop == N_PCONST)
      && n->Nright->Niconst == 0)
        v = n->Nleft;
    else if (n->Nleft
      && (n->Nleft->Nop == N_ICONST || n->Nleft->Nop == N_PCONST)
      && n->Nleft->Niconst == 0)
        v = n->Nright;

    if (!v || !tisdimode(v->Ntype))
        return 0;

    r = genexpr(v);
    vrunspillall();
    (void) vrstoreal(r, VR2(r));
    flushcode();
    hi = vrreal(r);
    lo = vrreal(VR2(r));
    if (reverse)
        op = revop(op);

    /* The surrounding boolean generator expects the final instruction to
    ** skip exactly when the relation is true.  If low is nonzero, bypass
    ** the high-word equality test.  != needs TRNA to turn either nonzero
    ** half into the final skip. */
    code00(P_SKIP+POF_ISSKIP+POS_SKPN, lo, lo);
    code00(P_SKIP+POF_ISSKIP+POS_SKPE, hi, hi);
    if (op == (P_CAM+POF_ISSKIP+POS_SKPN)) {
        flushcode();
        codestr("\tTRNA\n", 6);
    }
    vrfree(r);
    vrunspillall();
    return 1;
}

/* GPTRCANONCMP - Canonicalize a pointer for mixed-representation compare.
**
** Function-boundary exact-width pointers may be either native byte pointers
** or KCC's S=1 logical bit addresses.  Equality must compare addresses, not
** their incidental representation.  Convert every non-NULL operand to the
** S=1 form naming the same first bit.  Spill across the branch because a
** label may flush VREG state.
*/
static VREG *
gptrcanoncmp(VREG *r, TYPE *t)
{
    VREG *tmp, *v;
    SYMBOL *done;
    int fsiz;

    if (!r)
        return (VREG *)-1;
    if (t == NULL || t->Tspec != TS_PTR)
        return r;
    if (tisbitptr(t)) {
        r->Vrtype = voidptrtype;
        return r;                     /* Already canonical S=1. */
    }

    fsiz = elembsize(t);
    if (!fsiz)
        fsiz = TGSIZ_CHAR;

    code0(P_PUSH, VR_SP, r);
    ++stackoffset;
    vrfree(r);
    done = newlabel();

    tmp = vrget();
    codemdx(P_MOVE, vrtoreal(tmp), (SYMBOL *)NULL, 0, R_SP);
    code6(P_JUMP+POS_SKPE, tmp, done);       /* NULL stays NULL. */

    if (tismaybitptr(t)) {
        code0(P_HLRZ, tmp, tmp);
        code8(P_LSH, tmp, -6);
        code1(P_AND, tmp, 077);
        code8(P_CAI+POF_ISSKIP+POS_SKPN, tmp, 1);
        code6(P_JRST, (VREG *)NULL, done);   /* Already S=1. */
    }
    vrfree(tmp);

    v = vrget();
    codemdx(P_MOVE, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
    if (tisbytepointer(t))
        code10(P_PTRCNV, v, (SYMBOL *)NULL, 1, -fsiz);
    else
        pitopc(v, 1, 0, 0);
    codemdx(P_MOVEM, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
    vrfree(v);
    flushcode();
    codlabel(done);

    r = vrget();
    r->Vrtype = voidptrtype;
    codemdx(P_MOVE, vrtoreal(r), (SYMBOL *)NULL, 0, R_SP);
    code8(P_ADJSP, VR_SP, -1);
    --stackoffset;
    return r;
}

/* GBOOLOP - Generate code for == > < <= >= !=
**
*/
static void
gboolop(NODE *n, int reverse)
{
    int op;
    VREG *r1, *r2;

    op = gboolopbase(n->Nop);
    if (gboolmaskzero(n, op, reverse) || gboolzero(n, op, reverse)
      || gdimodezero(n, op, reverse))
        return;

    /* Equality of a representation-polymorphic exact-width pointer and
    ** another pointer is an address comparison.  Canonicalize both raw
    ** pointer words to S=1 so native-vs-logical representation differences
    ** cannot make equal pointers compare unequal.
    */
    if ((n->Nop == Q_EQUAL || n->Nop == Q_NEQ)
      && n->Nleft->Ntype->Tspec == TS_PTR
      && n->Nright->Ntype->Tspec == TS_PTR
      && (tismaybitptr(n->Nleft->Ntype)
        || tismaybitptr(n->Nright->Ntype))) {
        r1 = gptrcanoncmp(genexpr(n->Nleft), n->Nleft->Ntype);
        r2 = gptrcanoncmp(genexpr(n->Nright), n->Nright->Ntype);
        if (reverse)
            op = revop(op);
        code0(op, r1, r2);
        vrfree(r1);
        vrfree(r2);
        return;
    }

    /* May need to munch on char pointers to get into comparable form */
    switch (n->Nop)
	{
	case Q_LEQ:
	case Q_GEQ:
	case Q_LESS:
	case Q_GREAT:
	    if (tisunsign(n->Nleft->Ntype) && !tisdimode(n->Nleft->Ntype))
		{
		/* A constant right operand can be biased at compile time.
		** Avoid materializing it in a second AC merely to toggle the
		** sign bit before CAM.  Keep the variable operand on the
		** established TLC path so unsigned ordering remains signed CAM
		** ordering after the bias.
		*/
		if (n->Nright->Nop == N_ICONST)
		    {
		    INT c = n->Nright->Niconst & ((INT)0777777777777);

		    r1 = genexpr(n->Nleft);
		    code8(P_TLC, r1, 0400000L);
		    if (reverse)
			op = revop(op);
		    code1(op, r1, c ^ ((INT)0400000000000));
		    vrfree(r1);
		    return;
		    }
		r1 = genexpr(n->Nleft);		/* Get operand 1 */
		code8(P_TLC, r1, 0400000L);	/* and flip sign bit */
		r2 = genexpr(n->Nright);	/* Ditto for operand 2 */
		code8(P_TLC, r2, 0400000L);
		break;
		}
	    else if (tisbytepointer(n->Nleft->Ntype))
		{
		/* If operands are byte pointers */
		/* Note that:
		** OWGBPs can omit the SKIP+TLC, or use this:
		**	MOVE R,PTR1
		**	SUB R,PTR2
		**	ROT R,6
		**	CAIx R,0
		** Local-fmt BPs can use the sequence:
		**	MOVE R,PTR1
		**	MOVE R+1,PTR2	; Needs double reg
		**	ROTC R,6	; Yes this really works!
		**	CAMx R,R+1
		*/
		r1 = genexpr(n->Nleft);		/* Get operand 1 */
		code0(P_SKIP+POF_ISSKIP+POS_SKPL, r1, r1);
		code8(P_TLC, r1, 0770000L);	/* Zap P bits if local */
		code8(P_ROT, r1, 6);		/* Get P or PS into low bits */

		/* Repeat for 2nd operand */
		r2 = genexpr(n->Nright);	/* Get operand 2 */
		code0(P_SKIP+POF_ISSKIP+POS_SKPL, r2, r2);
		code8(P_TLC, r2, 0770000L);	/* Zap P bits if local */
		code8(P_ROT, r2, 6);		/* Get P or PS into low bits */

		/* Now can compare the registers with normal CAM! */
		break;
		}
	    /* Else just fall through for normal expression evaluation */

	/* FALLTHROUGH */
	case Q_EQUAL:
	case Q_NEQ:
	    r1 = genexpr (n->Nleft);	/* calculate values to compare */
	    r2 = genexpr (n->Nright);
	    break;
	}

    if (reverse)
	op = revop (op);	/* maybe invert test */

    /*
    ** Generate and optimize the test.
    **
    ** If we are comparing double precision floating point we need
    ** to look at both pairs of words, so we use a cascaded pair or
    ** trio of comparisons.
    */

    if (tisdimode(n->Nleft->Ntype))
	{
	int unsig = tisunsign(n->Nleft->Ntype);

	vrunspillall();
	switch (op)
	    {
	    case P_CAM+POF_ISSKIP+POS_SKPE:
		{
		int lhi, llo, rhi, rlo;

		(void) vrstoreal(r1, VR2(r1));
		(void) vrstoreal(r2, VR2(r2));
		flushcode();
		lhi = vrreal(r1);
		llo = vrreal(VR2(r1));
		rhi = vrreal(r2);
		rlo = vrreal(VR2(r2));
		code00(P_CAM+POF_ISSKIP+POS_SKPN, llo, rlo);
		code00(P_CAM+POF_ISSKIP+POS_SKPE, lhi, rhi);
		vrfree(r1);
		vrfree(r2);
		vrunspillall();
		return;
		}
	    case P_CAM+POF_ISSKIP+POS_SKPN:
		{
		int lhi, llo, rhi, rlo;

		(void) vrstoreal(r1, VR2(r1));
		(void) vrstoreal(r2, VR2(r2));
		flushcode();
		lhi = vrreal(r1);
		llo = vrreal(VR2(r1));
		rhi = vrreal(r2);
		rlo = vrreal(VR2(r2));
		code00(P_CAM+POF_ISSKIP+POS_SKPN, llo, rlo);
		code00(P_CAM+POF_ISSKIP+POS_SKPE, lhi, rhi);
		flushcode();
		codestr("\tTRNA\n", 6);
		vrfree(r1);
		vrfree(r2);
		vrunspillall();
		return;
		}
	    case P_CAM+POF_ISSKIP+POS_SKPL:
	    case P_CAM+POF_ISSKIP+POS_SKPLE:
	    case P_CAM+POF_ISSKIP+POS_SKPG:
	    case P_CAM+POF_ISSKIP+POS_SKPGE:
		if (!unsig && gdimode_signed_relop_skip(r1, r2, op))
		    return;
		if (unsig && gdimode_unsigned_relop_skip(r1, r2, op))
		    return;
		{
		int llo, rlo, t1, t2, th1, th2;
		int avoid[8], navoid;
		char buf[128];
		int n, lcmp, hcmp, h2cmp;
		int savehi, needstk, usestk;
		int lhi;

		savehi = (!unsig
		    && (op == (P_CAM+POF_ISSKIP+POS_SKPG)
		     || op == (P_CAM+POF_ISSKIP+POS_SKPGE)));

		(void) vrstoreal(r1, VR2(r1));
		flushcode();
		llo = vrreal(VR2(r1));
		lhi = vrreal(r1);
		navoid = 0;
		avoid[navoid++] = lhi;
		avoid[navoid++] = llo;
		t1 = gdimode_pick_ac(avoid, navoid);
		n = kccfmt(buf, sizeof(buf),
		    "\tMOVE\t%o,%o\n"
		    "\tTLC\t%o,0400000\n",
		    t1, llo, t1);
		codestr(buf, n);

		(void) vrstoreal(r2, VR2(r2));
		flushcode();
		rlo = vrreal(VR2(r2));
		needstk = (vrreal(VR2(r1)) != llo) || (vrreal(r1) != lhi);
		usestk = needstk;
		navoid = 0;
		avoid[navoid++] = vrreal(r1);
		avoid[navoid++] = vrreal(r2);
		avoid[navoid++] = t1;
		avoid[navoid++] = rlo;
		if (usestk)
		    {
		    avoid[navoid++] = vrreal(VR2(r1));
		    avoid[navoid++] = vrreal(VR2(r2));
		    th1 = gdimode_pick_ac(avoid, navoid);
		    avoid[navoid++] = th1;
		    th2 = gdimode_pick_ac(avoid, navoid);
		    avoid[navoid++] = th2;
		    }
		t2 = gdimode_pick_ac(avoid, navoid);
		n = kccfmt(buf, sizeof(buf),
		    "\tMOVE\t%o,%o\n"
		    "\tTLC\t%o,0400000\n",
		    t2, rlo, t2);
		codestr(buf, n);

		if (usestk)
		    {
		    n = kccfmt(buf, sizeof(buf),
			"\tMOVE\t%o,37777777777(17)\n"
			"\tMOVE\t%o,0(17)\n",
			th1, th2);
		    codestr(buf, n);
		    }

		switch (op)
		    {
		    case P_CAM+POF_ISSKIP+POS_SKPL:
			hcmp = unsig ? P_CAM+POF_ISSKIP+POS_SKPL
				     : P_CAM+POF_ISSKIP+POS_SKPLE;
			lcmp = P_CAM+POF_ISSKIP+POS_SKPG;
			h2cmp = P_CAM+POF_ISSKIP+POS_SKPN;
			break;
		    case P_CAM+POF_ISSKIP+POS_SKPLE:
			hcmp = P_CAM+POF_ISSKIP+POS_SKPL;
			lcmp = P_CAM+POF_ISSKIP+POS_SKPG;
			h2cmp = P_CAM+POF_ISSKIP+POS_SKPLE;
			break;
		    case P_CAM+POF_ISSKIP+POS_SKPG:
			hcmp = P_CAM+POF_ISSKIP+POS_SKPG;
			lcmp = P_CAM+POF_ISSKIP+POS_SKPLE;
			h2cmp = P_CAM+POF_ISSKIP+POS_SKPGE;
			break;
		    default:
			hcmp = P_CAM+POF_ISSKIP+POS_SKPG;
			lcmp = P_CAM+POF_ISSKIP+POS_SKPL;
			h2cmp = P_CAM+POF_ISSKIP+POS_SKPGE;
			break;
		    }

		if (savehi && op == (P_CAM+POF_ISSKIP+POS_SKPGE))
		    {
		    if (needstk)
			code00(P_CAM+POF_ISSKIP+POS_SKPLE, th2, th1);
		    else
			code00(P_CAM+POF_ISSKIP+POS_SKPLE,
			    vrreal(r2), vrreal(r1));
		    }
		else if (savehi && op == (P_CAM+POF_ISSKIP+POS_SKPG))
		    {
		    if (needstk)
			code00(P_CAM+POF_ISSKIP+POS_SKPG, th1, th2);
		    else
			code0(P_CAM+POF_ISSKIP+POS_SKPG, r1, r2);
		    }
		else if (!unsig && hcmp == (P_CAM+POF_ISSKIP+POS_SKPLE))
		    code00(hcmp, vrreal(r2), vrreal(r1));
		else if (!unsig && hcmp == (P_CAM+POF_ISSKIP+POS_SKPG) && needstk)
		    code00(hcmp, th2, th1);
		else if (!unsig && hcmp == (P_CAM+POF_ISSKIP+POS_SKPG))
		    code0(hcmp, r1, r2);
		else
		    code0(hcmp, r1, r2);
		if (!unsig && op == (P_CAM+POF_ISSKIP+POS_SKPL))
		    code00(P_CAM+POF_ISSKIP+POS_SKPN, vrreal(r2), vrreal(r1));
		code00(lcmp, t1, t2);
		if (savehi && needstk)
		    {
		    code00(P_CAM+POF_ISSKIP+POS_SKPN, th1, th2);
		    code00(h2cmp, th1, th2);
		    }
		else
		    code0(h2cmp, r1, r2);
		}
		vrfree(r1);
		vrfree(r2);
		vrunspillall();
		return;
	    }
	(void) vrstoreal(r1, VR2(r1));
	(void) vrstoreal(r2, VR2(r2));
	flushcode();
	code0(op, r1, r2);
	vrfree(r1);
	vrfree(r2);
	vrunspillall();
	return;
	}

    if (   n->Nleft->Ntype->Tspec == TS_DOUBLE
	|| n->Nleft->Ntype->Tspec == TS_LNGDBL )
	{
	switch (op)
	    {
	    case P_CAM+POF_ISSKIP+POS_SKPL:
		flushcode();		/* don't confuse peepholer */
		code0(P_CAM+POF_ISSKIP+POS_SKPL, r1, r2);
		code0(P_CAM+POF_ISSKIP+POS_SKPGE, VR2(r1), VR2(r2));
		op = P_CAM+POF_ISSKIP+POS_SKPLE;
		break;
	    case P_CAM+POF_ISSKIP+POS_SKPLE:
		flushcode();		/* don't confuse peepholer */
		code0(P_CAM+POF_ISSKIP+POS_SKPL, r1, r2);
		code0(P_CAM+POF_ISSKIP+POS_SKPG, VR2(r1), VR2(r2));
		break;
	    case P_CAM+POF_ISSKIP+POS_SKPG:
		flushcode();		/* don't confuse peepholer */
		code0(P_CAM+POF_ISSKIP+POS_SKPG, r1, r2);
		code0(P_CAM+POF_ISSKIP+POS_SKPLE, VR2(r1), VR2(r2));
		op = P_CAM+POF_ISSKIP+POS_SKPGE;
		break;
	    case P_CAM+POF_ISSKIP+POS_SKPGE:
		flushcode();		/* don't confuse peepholer */
		code0(P_CAM+POF_ISSKIP+POS_SKPG, r1, r2);
		code0(P_CAM+POF_ISSKIP+POS_SKPL, VR2(r1), VR2(r2));
		break;
	    case P_CAM+POF_ISSKIP+POS_SKPE:
		code0(P_CAM+POF_ISSKIP+POS_SKPN, VR2(r1), VR2(r2));
		break;
	    case P_CAM+POF_ISSKIP+POS_SKPN:
		/*
		** Overall skip for double != must be true if either word
		** differs.  Keep the final TRNA out of the peepholer,
		** otherwise the following JRST/MOVEI pair can be folded into
		** a SKIPA shape that skips the update on the true path.
		*/
		code0(P_CAM+POF_ISSKIP+POS_SKPN, r1, r2);
		code0(P_CAM+POF_ISSKIP+POS_SKPE, VR2(r1), VR2(r2));
		flushcode();
		codestr("\tTRNA\n", 6);
		vrfree(r1);
		return;
	    }
	}
    code0(op, r1, r2);			/* generate and optimize test */
    vrfree(r1);
}

/* GASSIGNABS - Handle direct absolute-value assignment forms. */
static int
gassignabs(NODE *n, NODE *nod, INT siz, int ptr, int volat, int lconv,
    VREG **result)
{
    NODE *absid;
    VREG *r, *ra;

    absid = gabsquery_id(n->Nright);
    if (!absid || siz != 1 || ptr || volat || lconv != CAST_NONE
      || n->Nascast != CAST_NONE || Register_Id(nod) || sideffp(nod))
        return 0;

    if (gsamepure(nod, absid)) {
        r = vrget();
        r->Vrtype = n->Ntype;
        code4(P_MOVM + POF_BOTH, r, gaddress(nod));
        *result = r;
        return 1;
    }
    if (!(n->Nflag & NF_DISCARD))
        return 0;

    if (Register_Id(absid) && nod->Nop == N_PTR) {
        ra = gaddress(nod);
        code40(P_MOVMM, absid->Nid->Sreg, vrreal(ra), 0);
        vrfree(ra);
        *result = NULL;
        return 1;
    }
    r = genexpr(absid);
    code4(P_MOVMM, r, gaddress(nod));
    vrfree(r);
    *result = NULL;
    return 1;
}

/* GASSIGNREG - Handle narrow word assignments whose RHS is a register var. */
static int
gassignreg(NODE *n, NODE *nod, INT siz, int ptr, int volat, int lconv,
    VREG **result)
{
    VREG *r, *ra;
    SYMBOL *msym;
    int c, src, ar;

    if (!optgen || siz != 1 || ptr || volat || lconv != CAST_NONE
      || n->Nascast != CAST_NONE || !Register_Id(n->Nright))
        return 0;

    if ((n->Nflag & NF_DISCARD) && nod->Nop == Q_IDENT
      && !tisanyvolat(n->Nright->Ntype)) {
        c = nod->Nid->Sclass;
        if (c == SC_ISTATIC || c == SC_XEXTREF || c == SC_EXLINK
          || c == SC_EXTDEF || c == SC_EXTREF || c == SC_INTDEF
          || c == SC_INTREF || c == SC_INLINK) {
            msym = nod->Nid;
            if (c == SC_ISTATIC)
                msym = msym->Ssym;
            codemdx(P_MOVEM, n->Nright->Nid->Sreg, msym, 0, 0);
            *result = NULL;
            return 1;
        }
    }

    if (nod->Nop != N_PTR || sideffp(nod))
        return 0;
    src = n->Nright->Nid->Sreg;
    ra = gaddress(nod);
    ar = vrtoreal(ra);
    code40(P_MOVEM, src, ar, 0);
    vrfree(ra);
    if (n->Nflag & NF_DISCARD) {
        *result = NULL;
        return 1;
    }
    r = (n->Nflag & NF_RETEXPR) ? vrretget() : vrget();
    r->Vrtype = n->Ntype;
    code00(P_MOVE, r->Vrloc, src);
    *result = r;
    return 1;
}

/* GASSIGN - Generate assignment expression.
**	Various tricky stuff involved.
** Note the hair needed for handling compound assignment, because the f*ed-up
** peepholer zaps index registers with wild abandon.  We have to compensate
** for this by being careful how we generate the address of the destination.
**
** Also note hair for storing into volatile objects!  This is the counterpart
** to the fetch checking in gprimary() and gunary().  The other store code is
** in gincdec().
*/

static VREG *
gassign(NODE *n)
{
    VREG *r1, *r2, *ra = NULL;
    int ptr, savaddr;
    INT siz;
    NODE *nod;		/* Points to lvalue (without conversion) */
    int lconv;		/* Holds lvalue conversion op if any */
    int volat;		/* True if obj is volatile */
    TYPE *fromt, *tot;

    nod = n->Nleft;
    if (nod->Nop == N_CAST)	/* If lvalue needs conversion before the op */
	{
	lconv = nod->Ncast;	/* Remember conversion op for lvalue */
	tot = nod->Ntype;	/* cast to this type */
	nod = nod->Nleft;	/* Then get ptr to real lvalue */
	fromt = nod->Ntype;	/* cast from this type */
	}
    else
	lconv = CAST_NONE;

    siz = sizetype(n->Ntype);	/* Get size of result type, in words */

    /* See if object will be referenced via a byte pointer */
    if ((ptr = bptrref(nod)) < 0)
	{
	int_error("gassign: bad op %N", nod);
	ptr = 0;
	}
    if ((volat = tisanyvolat(nod->Ntype)) != 0)
	flushcode();		/* Barf, foil peepholer if volatile obj */

    if (n->Nop == Q_ASGN && maybitptrderef(nod))
        {
        r1 = genexpr(n->Nright);
        r1 = gmaybitstore(r1, nod);
        if (volat)
            flushcode();
        return r1;
        }

    if (n->Nop == Q_ASGN)	/* Simple assignment? */
	{
        if (tispacked(n->Ntype) && (n->Ntype->Tspec == TS_STRUCT || n->Ntype->Tspec == TS_UNION))
            {
            r1 = gpackedcopy(nod, n->Nright, n->Ntype);
            if (volat)
                flushcode();
            return r1;
            }

        if (gassignabs(n, nod, siz, ptr, volat, lconv, &r1))
            return r1;

        /* Two-word aggregate values may be materialized in AC1/AC2.
        ** Preserve the destination address before evaluating the RHS, since
        ** the destination itself may be an ABI argument resident in AC1.
        ** C does not impose an evaluation order between these two operands.
        */
        if (siz == 2 && !ptr && !Register_Id(nod)
          && (n->Ntype->Tspec == TS_STRUCT || n->Ntype->Tspec == TS_UNION))
            {
            int ar;

            ra = gaddress(nod);
            code0(P_PUSH, VR_SP, ra);
            ++stackoffset;
            r1 = genexpr(n->Nright);
            ra = vrget();
            ra->Vrtype = nod->Ntype;
            ar = vrstoreal(ra, r1);
            code00(P_POP, R_SP, ar);
            --stackoffset;
            r1 = stomem(r1, ra, siz, 0);
            if (volat)
                flushcode();
            return r1;
            }

        if (gassignreg(n, nod, siz, ptr, volat, lconv, &r1))
            return r1;

	r1 = genexpr(n->Nright);	/* Generate value first */
        if (tisinteg(nod->Ntype)
          && (((nod->Nop == Q_MEMBER || nod->Nop == Q_DOT)
               && (packedcross(nod->Nxoff) || packedbit(nod->Nxoff) || packedbitscalar(nod->Nxoff) || crossbit(nod->Nxoff)))
              || packedptrderef(nod) || bitptrmember(nod)))
            {
            r1 = gpackedstore(r1, nod);
            if (volat)
                flushcode();
            return r1;
            }
	if (siz == 2 && r1 && r1->Vrtype && tisdimode(r1->Vrtype)
	  && !Register_Id(nod) && !ptr)
	    {
	    int ar, hi, lo, nout, aflags;
	    char buf[160];

	    code0(P_PUSH, VR_SP, r1);
	    code0(P_PUSH, VR_SP, VR2(r1));
	    stackoffset += 2;
	    vrfree(r1);
	    ra = gaddress(nod);
	    ar = vrtoreal(ra);
	    aflags = ra->Vrflags;
	    ra->Vrflags |= VRF_LOCK;
	    r2 = vrdget();
	    ra->Vrflags = aflags;
	    r2->Vrtype = n->Ntype;
	    VR2(r2)->Vrtype = n->Ntype;
	    hi = vrreal(r2);
	    lo = vrreal(VR2(r2));
	    flushcode();
	    if (tgcpu >= TGCPU_KI)
		nout = kccfmt(buf, sizeof(buf),
		    "\tDMOVE\t%o,-1(17)\n"
		    "\tSUB\t17,[2,,2]\n"
                    "\tTLZ\t%o,0400000\n"
                    "\tTLNE\t%o,0400000\n"
                    "\t TLO\t%o,0400000\n"
		    "\tDMOVEM\t%o,0(%o)\n"
                    "\tTLZ\t%o,0400000\n",
		    hi, lo, hi, lo, hi, ar, lo);
	    else
		nout = kccfmt(buf, sizeof(buf),
		    "\tMOVE\t%o,-1(17)\n"
		    "\tMOVE\t%o,0(17)\n"
		    "\tSUB\t17,[2,,2]\n"
                    "\tTLZ\t%o,0400000\n"
                    "\tTLNE\t%o,0400000\n"
                    "\t TLO\t%o,0400000\n"
		    "\tMOVEM\t%o,0(%o)\n"
		    "\tMOVEM\t%o,1(%o)\n"
                    "\tTLZ\t%o,0400000\n",
		    hi, lo, lo, hi, lo, hi, ar, lo, ar, lo);
	    codestr(buf, nout);
	    stackoffset -= 2;
	    vrfree(ra);
	    if (volat)
		flushcode();
	    return r2;
	    }
	/* Special check for doing IDPB.  Safer to do here instead of
	** in peephole, at least until peepholer fixed to allow keeping
	** an index reg around!
	*/
#if 0
/* Later, add many cases here when new MACRO instructions are defined.
 * Imitate case N_PTR: in gunary() below.
 */
#endif
	if (optgen && ptr			/* If a byte ptr */
		&& nod->Nop == N_PTR		/* and op is "*++(exp)" */
		&& nod->Nleft->Nop == N_PREINC)
	    {
#if 0		/* Later, fix Reg linkage for pointers */
	    if (Register_Id(nod->Nleft->Nleft))
		code40(P_IDPB, r1->Vrloc, nod->Nleft->Nleft->Nid->Sreg, 0);
	    else
#endif
		code4(P_IDPB, r1, gaddress(nod->Nleft->Nleft));
	    return r1;
	    }
	else if (Register_Id(nod))	/* approximate stomem for registers */
	    {
	    r_preserve = nod->Nid->Sreg;
	    if (siz == 1)
		{
		ra = vrget();
		ra->Vrtype = nod->Ntype;
		code00(P_MOVE, ra->Vrloc, r1->Vrloc);
		}
	    else
		int_error ("gassign: reg argument size > 1");
	    vrfree (r1);
	    return ra;
	    }
	else
	    {
            /* A plain one-word automatic object already has a known
            ** SP-relative address.  Store to it directly so address
            ** materialization cannot reuse the physical AC holding r1.
            */
            if (siz == 1 && !ptr && lconv == CAST_NONE
              && nod->Nop == Q_IDENT
              && (nod->Nid->Sclass == SC_AUTO || nod->Nid->Sclass == SC_ARG))
                {
                INT off;

                if (nod->Nid->Sclass == SC_AUTO)
                    off = autooff_v11(nod->Nid);
                else
                    off = argoff_v11(nod->Nid);
                codemdx(P_MOVEM, vrtoreal(r1), (SYMBOL *)NULL, off,
                        frameindex_v11());
                }
            else
                r1 = stomem(r1,		/* Store the value */
                    gaddress(nod),		/* into address of lvalue */
                    /* Operand and operation types are same, so siz is correct */
                    siz,
                    ptr);			/* and flag saying if addr is ptr */
	    if (n->Nflag & NF_DISCARD)
		foldhalfstore();
	    }
	if (volat)
	    flushcode();
	return r1;
	}

    /* Some compound assignment type.
    ** First, generate the right operand, including any conversions.
    */
    vrunspillall();

    /* A terminal commutative compound assignment with a register RHS can
    ** use that physical AC as the PDP-10 BOTH result directly.  The lvalue
    ** address is consumed before the return copy, so no general liveness
    ** machinery is required.
    **
    **     return (*p += x) -> ADDB x,0(p); MOVE 1,x
    */
    if (optgen && (n->Nflag & NF_RETEXPR) && siz == 1 && !ptr && !volat
      && lconv == CAST_NONE && n->Nascast == CAST_NONE
      && Register_Id(n->Nright) && nod->Nop == N_PTR
      && !sideffp(nod)
      && (n->Nleft->Ntype->Tspec == TS_INT
       || n->Nleft->Ntype->Tspec == TS_UINT
       || n->Nleft->Ntype->Tspec == TS_LONG
       || n->Nleft->Ntype->Tspec == TS_ULONG)) {
        int mop, src, ar;

        switch (n->Nop) {
        case Q_ASPLUS: mop = P_ADD; break;
        case Q_ASAND:  mop = P_AND; break;
        case Q_ASOR:   mop = P_IOR; break;
        case Q_ASXOR:  mop = P_XOR; break;
        default:       mop = 0; break;
        }
        if (mop != 0) {
            src = n->Nright->Nid->Sreg;
            ra = gaddress(nod);
            ar = vrtoreal(ra);
            code40(mop + POF_BOTH, src, ar, 0);
            vrfree(ra);
            r1 = vrretget();
            r1->Vrtype = n->Ntype;
            code00(P_MOVE, r1->Vrloc, src);
            return r1;
        }
    }

    /* A compound assignment through a representation-polymorphic exact-width
    ** pointer must evaluate a side-effecting pointer expression exactly once.
    ** Anchor both the raw pointer and RHS in real stack memory before the
    ** dynamic load: PDP-6 ADJBP expansion may spill several ACs.
    */
    if (tisinteg(nod->Ntype) && maybitptrderef(nod) && sideffp(nod))
        {
        int pdepth;

        ra = genexpr(nod->Nleft);
        code0(P_PUSH, VR_SP, ra);
        ++stackoffset;
        pdepth = stackoffset;
        vrfree(ra);

        r2 = genexpr(n->Nright);
        code0(P_PUSH, VR_SP, r2);
        ++stackoffset;
        vrfree(r2);

        r1 = gmaybitloaddepth(nod, pdepth);
        r2 = vrget();
        r2->Vrtype = n->Nright->Ntype;
        codemdx(P_MOVE, vrtoreal(r2), (SYMBOL *)NULL, 0, R_SP);
        if (lconv != CAST_NONE)
            r1 = gcastr(lconv, r1, fromt, tot, nod);
        r1 = garithop(n->Nop, r1, r2, n->Nleft->Ntype->Tspec);
        if (n->Nascast != CAST_NONE)
            r1 = gcastr(n->Nascast, r1, n->Nleft->Ntype, n->Ntype,
                        (NODE *)NULL);
        r1 = gmaybitstoredepth(r1, nod, pdepth);
        code8(P_ADJSP, VR_SP, -2);
        stackoffset -= 2;
        if (volat)
            flushcode();
        return r1;
        }

    r2 = (n->Ntype->Tspec == TS_PTR) ?		/* Doing pointer arith? */
	gptraddend(n->Nleft->Ntype, n->Nright)	/* Operand for ptr arith */
	: genexpr(n->Nright);			/* General-type operand */

    if (tisinteg(nod->Ntype)
      && (((nod->Nop == Q_MEMBER || nod->Nop == Q_DOT)
           && (packedcross(nod->Nxoff) || packedbit(nod->Nxoff) || packedbitscalar(nod->Nxoff) || crossbit(nod->Nxoff)))
          || packedptrderef(nod) || bitptrmember(nod) || maybitptrderef(nod)))
        {
        ra = NULL;
        if (sideffp(nod))
            {
            int raflags;
            ra = gaddress(nod);
            raflags = ra->Vrflags;
            ra->Vrflags |= VRF_LOCK;
            r1 = gpackedloadat(nod, ra);
            ra->Vrflags = raflags;
            }
        else
            r1 = maybitptrderef(nod) ? gmaybitload(nod) : gpackedload(nod);
        if (lconv != CAST_NONE)
            r1 = gcastr(lconv, r1, fromt, tot, nod);
        r1 = garithop(n->Nop, r1, r2, n->Nleft->Ntype->Tspec);
        if (n->Nascast != CAST_NONE)
            r1 = gcastr(n->Nascast, r1, n->Nleft->Ntype, n->Ntype,
                        (NODE *)NULL);
        if (ra != NULL)
            {
            int raflags = ra->Vrflags;
            ra->Vrflags |= VRF_LOCK;
            r1 = gpackedstoreat(r1, nod, ra);
            ra->Vrflags = raflags;
            vrfree(ra);
            }
        else
            r1 = maybitptrderef(nod) ? gmaybitstore(r1, nod)
                                      : gpackedstore(r1, nod);
        if (volat)
            flushcode();
        return r1;
        }

    /* A discarded one-word compound assignment can use the PDP-10
    ** memory-result form directly.  This avoids fetching the old value
    ** into a second register and storing the result back afterwards:
    **
    **     MOVE  R,M          ADDB  S,M
    **     ADD   R,S    ->
    **     MOVEM R,M
    **
    ** Only commutative operations qualify.  Keep the case deliberately
    ** narrow: no byte pointers, volatile objects, conversions, or lvalue
    ** side effects.  The discarded-result flag proves that the updated
    ** register value need not survive the operation.
    */
    if (optgen && (n->Nflag & NF_DISCARD) && siz == 1 && !ptr && !volat
      && lconv == CAST_NONE && n->Nascast == CAST_NONE && !sideffp(nod)
      && (n->Nleft->Ntype->Tspec == TS_INT
       || n->Nleft->Ntype->Tspec == TS_UINT
       || n->Nleft->Ntype->Tspec == TS_LONG
       || n->Nleft->Ntype->Tspec == TS_ULONG)) {
        int mop;

        switch (n->Nop) {
        case Q_ASPLUS: mop = P_ADD; break;
        case Q_ASAND:  mop = P_AND; break;
        case Q_ASOR:   mop = P_IOR; break;
        case Q_ASXOR:  mop = P_XOR; break;
        default:       mop = 0; break;
        }
        if (mop != 0) {
            /* A register variable is a PDP-10 accumulator used as memory,
            ** not 0(AC).  No address calculation is needed, so even a
            ** computed RHS is safe here: the BOTH form updates the RHS AC
            ** and the preserved accumulator in one instruction.
            */
            if (Register_Id(nod)) {
                code14(mop + POF_BOTH, r2, nod->Nid->Sreg);
                return r2;
            }

            /* For ordinary memory keep the direct form to leaf RHS values.
            ** More complex expressions can leave a computed temporary live
            ** across address generation/peephole processing.
            */
            if (n->Nright->Nop == Q_IDENT || n->Nright->Nop == N_ICONST
              || n->Nright->Nop == N_PCONST) {
                code4(mop + POF_BOTH, r2, gaddress(nod));
                return r2;
            }
        }
    }

    /* Then generate the left operand.  For the time being, the peephole
    ** optimizer is so screwed up that we can't keep the address around
    ** and have to generate it twice.
    */
    savaddr = sideffp(nod);     /* Warn user if we'll fail */

    if (Register_Id(nod))	/* approximate getmem() for registers */
	{
	r_preserve = nod->Nid->Sreg;
	ra = vrget();
	ra->Vrtype = nod->Ntype;
#if 0
	r1 = rgetmem(ra, nod->Ntype, ptr, savaddr);
#else
	r1 = rgetmem(ra, nod->Ntype, savaddr);
#endif
	}
    else
	r1 = getmem(ra=gaddress(nod), /* Get left operand, WITHOUT releasing addr!*/
	    nod->Ntype,	/* using its real type */
	    ptr,			/* addr may be a byte pointer */
	    savaddr);			/* Keep the address reg! */

    /* Now have left operand in R1.  Convert it for operation, if needed. */
    if (lconv != CAST_NONE)	/* Convert left operand if necessary */
	r1 = gcastr(lconv, r1, fromt, tot, nod);

    /* Apply the arithmetic operation, checking to make sure pointer
    ** arithmetic is handled properly.  r2 is released.
    */
    if (n->Ntype->Tspec == TS_PTR)	/* If doing pointer arith */
	r1 = gptrop(n->Nop, r1, r2, n->Ntype, n->Nright->Ntype);
    else
	r1 = garithop(n->Nop, r1, r2, n->Nleft->Ntype->Tspec);

    /* Now see if there's any assignment conversion to perform on
    ** the result of the operation.
    */
    if (n->Nascast != CAST_NONE)
	{
	r1 = gcastr(n->Nascast, r1, n->Nleft->Ntype, n->Ntype, (NODE *)NULL);
	}

    if (Register_Id(nod))	/* approximate stomem for registers */
	{
	if (siz == 1)
	    {
	    code00(P_MOVE, nod->Nid->Sreg, r1->Vrloc);
	    if (Register_Nopreserve(ra->Vrloc)) /* garithop changed it!? */
		vrfree (ra);
	    return r1;
	    }
	else
	    int_error ("gassign: reg argument size > 1");
	return ra;
	}
    else
	{
	/* Finally, can store the value back. We either use the
	** saved address, if one, or generate it all over again.
	*/
	if (!savaddr)
	    ra = gaddress(nod);		/* Else re-use saved addr */
	r1 = stomem(r1, ra, siz, ptr);
	}
    if (volat)
	flushcode();			/* Barf bletch */
    return r1;
}

/* GROTATE - Generate a native rotate for the canonical C rotate idiom.
**
** Match only the deliberately narrow, side-effect-free forms
**
**     (x << n) | (x >> (36 - n))
**     (x << c) | (x >> (36 - c))
**
** where x and n are simple identifiers and x is unsigned.  Keeping this
** check here avoids adding a new tree opcode or a general data-flow pass.
** The source expression evaluates x twice, so folding is safe for a plain
** identifier; volatile objects are excluded because their two reads are
** observable.
*/
static VREG *
grotate(NODE *n)
{
    NODE *ls, *rs, *lc, *rc, *var, *sub;
    VREG *r1, *r2;
    INT count;
    int neg;

    if (!optgen || n->Nop != Q_OR || sizetype(n->Ntype) != 1)
        return NULL;

    ls = n->Nleft;
    rs = n->Nright;
    if (ls && rs && ls->Nop == Q_RSHFT && rs->Nop == Q_LSHFT) {
        NODE *t;

        t = ls;
        ls = rs;
        rs = t;
    }
    if (!ls || !rs || ls->Nop != Q_LSHFT || rs->Nop != Q_RSHFT)
        return NULL;
    if (!ls->Nleft || !rs->Nleft
      || ls->Nleft->Nop != Q_IDENT || rs->Nleft->Nop != Q_IDENT
      || ls->Nleft->Nid != rs->Nleft->Nid
      || !tisunsign(rs->Nleft->Ntype)
      || sizetype(rs->Nleft->Ntype) != 1
      || tisvolatile(ls->Nleft->Nid->Stype))
        return NULL;

    lc = ls->Nright;
    rc = rs->Nright;
    if (!lc || !rc)
        return NULL;

    if (lc->Nop == N_ICONST && rc->Nop == N_ICONST
      && lc->Niconst >= 0 && lc->Niconst < TGSIZ_WORD
      && rc->Niconst == TGSIZ_WORD - lc->Niconst) {
        r1 = genexpr(ls->Nleft);
        count = lc->Niconst;
        code8(P_ROT, r1, count);
        return r1;
    }

    if (lc->Nop == Q_IDENT) {
        var = lc;
        sub = rc;
        neg = 0;
    } else if (rc->Nop == Q_IDENT) {
        var = rc;
        sub = lc;
        neg = 1;
    } else
        return NULL;

    if (sub->Nop != Q_MINUS
      || !sub->Nleft || sub->Nleft->Nop != N_ICONST
      || sub->Nleft->Niconst != TGSIZ_WORD
      || !sub->Nright || sub->Nright->Nop != Q_IDENT
      || var->Nid != sub->Nright->Nid
      || tisvolatile(var->Nid->Stype))
        return NULL;

    r1 = genexpr(ls->Nleft);
    r2 = genexpr(var);
    if (neg)
        code0(P_MOVN, r2, r2);
    code4(P_ROT, r1, r2);
    return r1;
}

/* GBINARY - Generate code for binary operators.
**
*/
static VREG *
gbinary(NODE *n)
{
    VREG *r1, *r2;
    NODE *masked, *maskconst, *inda, *indb, *indt;
    static int p2lab, p2modlab;

    if ((r1 = grotate(n)) != NULL)
        return r1;

    /* A DImode AND with a constant confined to the canonical 35-bit low
    ** word does not need a second register pair.  This is the form produced
    ** by unsigned remainder modulo a low-word power of two, and also covers
    ** explicit masks.  Evaluate the variable operand once, clear its high
    ** word, and apply the low mask directly. */
    if (optgen && n->Nop == Q_ANDT && n->Ntype && tisdimode(n->Ntype)) {
        NODE *val = NULL, *con = NULL;
        INT chi, clo;

        if (n->Nright && n->Nright->Nop == N_ICONST) {
            val = n->Nleft;
            con = n->Nright;
        } else if (n->Nleft && n->Nleft->Nop == N_ICONST) {
            val = n->Nright;
            con = n->Nleft;
        }
        if (val && con) {
            dimode_iconst_words(con, &chi, &clo);
            if (chi == 0) {
                int hi, lo;
                char abuf[128];
                int an;

                r1 = genexpr(val);
                (void) vrstoreal(r1, VR2(r1));
                hi = vrreal(r1);
                lo = vrreal(VR2(r1));
                an = kccfmt(abuf, sizeof(abuf),
                    "\tSETZ\t%o,\n"
                    "\tAND\t%o,[%lo]\n",
                    hi, lo, (long)clo);
                codestr(abuf, an);
                r1->Vrtype = n->Ntype;
                return r1;
            }
        }
    }

    /* Unsigned division and remainder by a high-word power of two need
    ** neither the restoring divider nor scratch ACs.  Division is a logical
    ** pair shift.  Remainder keeps the low word and masks the high word to
    ** the bits below the divisor. */
    if (optgen && n->Ntype && n->Ntype->Tspec == TS_ULONGLONG
      && (n->Nop == Q_DIV || n->Nop == Q_MOD)
      && n->Nright && n->Nright->Nop == N_ICONST) {
        INT chi, clo, mask;
        int sh, hi, lo;
        char abuf[128];
        int an;

        dimode_iconst_words(n->Nright, &chi, &clo);
        sh = dimode_power2_exp(chi, clo);
        if (sh >= 35 && sh <= 70) {
            r1 = genexpr(n->Nleft);
            (void) vrstoreal(r1, VR2(r1));
            hi = vrreal(r1);
            lo = vrreal(VR2(r1));
            if (n->Nop == Q_DIV)
                an = kccfmt(abuf, sizeof(abuf),
                    "\tLSHC\t%o,-%o\n"
                    "\tAND\t%o,[0377777777777]\n",
                    hi, sh, lo);
            else if (sh == 35)
                an = kccfmt(abuf, sizeof(abuf), "\tSETZ\t%o,\n", hi);
            else {
                mask = (((INT)1 << (sh - 35)) - 1)
                     & dimode_hi36mask();
                an = kccfmt(abuf, sizeof(abuf), "\tAND\t%o,[%lo]\n",
                    hi, (long)mask);
            }
            codestr(abuf, an);
            r1->Vrtype = n->Ntype;
            return r1;
        }
    }

    /* Signed division and remainder by a wide positive power of two use
    ** the same magnitude-and-sign rules as the low-word forms.  The bias
    ** or mask spans the complete canonical 36+35-bit pair, but still needs
    ** no divider scratch ACs. */
    if (optgen && n->Ntype && n->Ntype->Tspec == TS_LONGLONG
      && (n->Nop == Q_DIV || n->Nop == Q_MOD)
      && n->Nright && n->Nright->Nop == N_ICONST) {
        INT chi, clo, himask;
        int sh, negdiv;

        dimode_iconst_words(n->Nright, &chi, &clo);
        sh = dimode_power2_exp(chi, clo);
        negdiv = 0;
        if (sh < 0) {
            sh = dimode_negative_power2_exp(chi, clo);
            negdiv = 1;
        }
        if (sh >= 35 && sh <= 70) {
            int hi, lo, lab;
            char abuf[512];
            int an;

            himask = sh == 35 ? 0
                : (((INT)1 << (sh - 35)) - 1) & dimode_hi36mask();
            r1 = genexpr(n->Nleft);
            (void) vrstoreal(r1, VR2(r1));
            hi = vrreal(r1);
            lo = vrreal(VR2(r1));
            lab = n->Nop == Q_DIV ? p2lab++ : p2modlab++;
            if (n->Nop == Q_DIV)
                an = kccfmt(abuf, sizeof(abuf),
                    "\tJUMPGE\t%o,%%DIWP%d\n"
                    "\tADD\t%o,[0377777777777]\n"
                    "\tTLZE\t%o,400000\n"
                    "\t ADDI\t%o,1\n"
                    "\tADD\t%o,[%lo]\n"
                    "%%DIWP%d:\n"
                    "\tASHC\t%o,-%o\n"
                    "\tAND\t%o,[0377777777777]\n",
                    hi, lab,
                    lo,
                    lo,
                    hi,
                    hi, (long)himask,
                    lab,
                    hi, sh,
                    lo);
            else
                an = kccfmt(abuf, sizeof(abuf),
                    "\tJUMPGE\t%o,%%DIWM%dP\n"
                    "\tMOVN\t%o,%o\n"
                    "\tSKIPE\t%o\n"
                    "\t SUBI\t%o,1\n"
                    "\tMOVN\t%o,%o\n"
                    "\tAND\t%o,[0377777777777]\n"
                    "\tAND\t%o,[%lo]\n"
                    "\tSKIPE\t%o\n"
                    "\t SETO\t%o,\n"
                    "\tMOVN\t%o,%o\n"
                    "\tAND\t%o,[0377777777777]\n"
                    "\tJRST\t%%DIWM%dD\n"
                    "%%DIWM%dP:\n"
                    "\tAND\t%o,[%lo]\n"
                    "%%DIWM%dD:\n",
                    hi, lab,
                    hi, hi, lo, hi, lo, lo, lo,
                    hi, (long)himask,
                    lo, hi, lo, lo, lo,
                    lab, lab,
                    hi, (long)himask,
                    lab);
            codestr(abuf, an);
            if (n->Nop == Q_DIV && negdiv)
                gdimodeneg(r1);
            r1->Vrtype = n->Ntype;
            return r1;
        }
    }

    /* Signed DImode remainder by a positive or negative power of two is the masked
    ** magnitude with the original sign restored.  This preserves C
    ** truncation-toward-zero semantics and needs no divider scratch ACs.
    ** Restrict the fold to masks wholly within the canonical low word. */
    if (optgen && n->Nop == Q_MOD && n->Ntype
      && n->Ntype->Tspec == TS_LONGLONG
      && n->Nright && n->Nright->Nop == N_ICONST) {
        INT chi, clo;
        int sh;

        dimode_iconst_words(n->Nright, &chi, &clo);
        sh = dimode_power2_exp(chi, clo);
        if (sh < 0)
            sh = dimode_negative_power2_exp(chi, clo);
        if (sh > 0 && sh < 35) {
            int hi, lo, lab;
            char abuf[320];
            int an;

            r1 = genexpr(n->Nleft);
            (void) vrstoreal(r1, VR2(r1));
            hi = vrreal(r1);
            lo = vrreal(VR2(r1));
            lab = p2modlab++;
            an = kccfmt(abuf, sizeof(abuf),
                "\tJUMPGE\t%o,%%DIM2%dP\n"
                "\tMOVN\t%o,%o\n"
                "\tSKIPE\t%o\n"
                "\t SUBI\t%o,1\n"
                "\tMOVN\t%o,%o\n"
                "\tAND\t%o,[0377777777777]\n"
                "\tSETZ\t%o,\n"
                "\tAND\t%o,[%lo]\n"
                "\tSKIPE\t%o\n"
                "\t SETO\t%o,\n"
                "\tMOVN\t%o,%o\n"
                "\tAND\t%o,[0377777777777]\n"
                "\tJRST\t%%DIM2%dD\n"
                "%%DIM2%dP:\n"
                "\tSETZ\t%o,\n"
                "\tAND\t%o,[%lo]\n"
                "%%DIM2%dD:\n",
                hi, lab,
                hi, hi, lo, hi, lo, lo, lo,
                hi, lo, (long)(clo - 1), lo, hi, lo, lo, lo,
                lab, lab, hi, lo, (long)(clo - 1), lab);
            codestr(abuf, an);
            r1->Vrtype = n->Ntype;
            return r1;
        }
    }

    /* Signed DImode division by a positive or negative power of two can use an
    ** arithmetic pair shift once negative dividends have been biased by
    ** divisor-1.  This implements C truncation toward zero rather than the
    ** floor-like result of a bare ASHC.  Restrict this fold to powers whose
    ** mask is wholly in the canonical 35-bit low word; wider constants are
    ** left to the general divider until wide constant folding is repaired. */
    if (optgen && n->Nop == Q_DIV && n->Ntype
      && n->Ntype->Tspec == TS_LONGLONG
      && n->Nright && n->Nright->Nop == N_ICONST) {
        INT chi, clo;
        int sh, negdiv;

        dimode_iconst_words(n->Nright, &chi, &clo);
        sh = dimode_power2_exp(chi, clo);
        negdiv = 0;
        if (sh < 0) {
            sh = dimode_negative_power2_exp(chi, clo);
            negdiv = 1;
        }
        if (sh > 0 && sh < 35) {
            {
                int hi, lo, lab;
                char abuf[256];
                int an;

                r1 = genexpr(n->Nleft);
                (void) vrstoreal(r1, VR2(r1));
                hi = vrreal(r1);
                lo = vrreal(VR2(r1));
                lab = p2lab++;
                an = kccfmt(abuf, sizeof(abuf),
                    "\tJUMPGE\t%o,%%DIP2%d\n"
                    "\tADD\t%o,[%lo]\n"
                    "\tTLZE\t%o,400000\n"
                    "\t ADDI\t%o,1\n"
                    "%%DIP2%d:\n"
                    "\tASHC\t%o,-%o\n"
                    "\tAND\t%o,[0377777777777]\n",
                    hi, lab,
                    lo, (long)(clo - 1),
                    lo,
                    hi,
                    lab,
                    hi, sh,
                    lo);
                codestr(abuf, an);
                if (negdiv)
                    gdimodeneg(r1);
                r1->Vrtype = n->Ntype;
                return r1;
            }
        }
    }

    /* DImode division and remainder by one are exact identities for both
    ** signed and unsigned values.  Evaluate the dividend once so calls and
    ** other side effects are preserved; modulo then replaces only the value
    ** with canonical zero. */
    if (optgen && n->Ntype && tisdimode(n->Ntype)
      && (n->Nop == Q_DIV || n->Nop == Q_MOD)
      && n->Nright && n->Nright->Nop == N_ICONST) {
        INT chi, clo;

        dimode_iconst_words(n->Nright, &chi, &clo);
        if (chi == 0 && clo == 1) {
            r1 = genexpr(n->Nleft);
            if (n->Nop == Q_MOD) {
                vrfree(r1);
                r1 = vrdget();
                code5(P_SETZ, r1);
                code5(P_SETZ, VR2(r1));
            }
            r1->Vrtype = n->Ntype;
            return r1;
        }
    }

    /* Signed DImode division and remainder by -1 do not need the restoring
    ** divider.  C leaves the minimum/-1 overflow case undefined, so division
    ** is exactly the existing in-place 71-bit negation.  Remainder is always
    ** zero, but the dividend must still be evaluated for side effects. */
    if (optgen && n->Ntype && n->Ntype->Tspec == TS_LONGLONG
      && (n->Nop == Q_DIV || n->Nop == Q_MOD)
      && n->Nright && n->Nright->Nop == N_ICONST) {
        INT chi, clo;

        dimode_iconst_words(n->Nright, &chi, &clo);
        if (chi == dimode_hi36mask() && clo == dimode_lo35mask()) {
            r1 = genexpr(n->Nleft);
            if (n->Nop == Q_DIV)
                gdimodeneg(r1);
            else {
                code5(P_SETZ, r1);
                code5(P_SETZ, VR2(r1));
            }
            r1->Vrtype = n->Ntype;
            return r1;
        }
    }

    /* Fold DImode subtraction by zero before materializing a two-word zero
    ** and entering the carry/borrow generator.  Addition by zero is already
    ** removed by earlier tree folding; subtraction had escaped that pass.
    ** Evaluating only the left operand preserves all side effects. */
    if (optgen && n->Nop == Q_MINUS && n->Ntype && tisdimode(n->Ntype)
      && n->Nright && n->Nright->Nop == N_ICONST
      && n->Nright->Niconst == 0) {
        r1 = genexpr(n->Nleft);
        if (r1)
            r1->Vrtype = n->Ntype;
        return r1;
    }

    /* Canonical loop strength reduction.  The loop generator keeps
    ** &array[i] current in indptrreg; replace repeated pointer arithmetic
    ** with a disposable copy of that derived pointer.
    */
    if (indptrreg && indvarsym && indbasesym && n->Nop == Q_PLUS
      && n->Nleft && n->Nright && n->Ntype
      && n->Ntype->Tspec == TS_PTR) {
        inda = n->Nleft;
        indb = n->Nright;

        if (inda->Nop == Q_IDENT && inda->Nid == indvarsym) {
            indt = inda; inda = indb; indb = indt;
        }
        if (inda->Nop == Q_IDENT && inda->Nid == indbasesym
          && indb->Nop == Q_IDENT && indb->Nid == indvarsym) {
            r1 = vrget();
            r1->Vrtype = n->Ntype;
            (void)vrstoreal(indptrreg, r1);
            code00(P_MOVE, vrreal(r1), vrreal(indptrreg));
            return r1;
        }
    }

    /* A packing helper may mask a field, cast it through an unsigned
    ** full-word type, and mask it with the identical constant again.
    ** The intervening casts do not change the representation, so the
    ** outer AND is redundant.  Keep this deliberately narrow: only
    ** identical right-hand constants and unsigned full-word casts qualify.
    */
    if (n->Nop == Q_ANDT && n->Nright && n->Nright->Nop == N_ICONST
      && (n->Nright->Niconst & ~0777777L) == 0) {
        masked = n->Nleft;
        while (masked && masked->Nop == N_CAST
          && masked->Ntype && tisunsign(masked->Ntype)
          && tbitsize(masked->Ntype) == TGSIZ_WORD)
            masked = masked->Nleft;
        maskconst = (masked && masked->Nop == Q_ANDT)
            ? masked->Nright : NULL;
        if (maskconst && maskconst->Nop == N_ICONST
          && maskconst->Niconst == n->Nright->Niconst) {
            r1 = genexpr(n->Nleft);
            if (r1)
                r1->Vrtype = n->Ntype;
            return r1;
        }
    }

    /* Use native PDP-10 halfword extraction for the canonical packed-word
    ** idioms.  This is exact for one-word integral values and avoids the
    ** MOVE/LSH/AND sequences otherwise generated for kernel metadata.
    */
    if (n->Nop == Q_ANDT
      && n->Nright && n->Nright->Nop == N_ICONST
      && n->Nright->Niconst == 0777777L
      && sizetype(n->Nleft->Ntype) == 1)
	{
	if (n->Nleft->Nop == Q_RSHFT
	  && n->Nleft->Nright
	  && n->Nleft->Nright->Nop == N_ICONST
	  && n->Nleft->Nright->Niconst == 18
	  && sizetype(n->Nleft->Nleft->Ntype) == 1)
	    {
	    r1 = genexpr(n->Nleft->Nleft);
	    code0(P_HLRZ, r1, r1);
	    return r1;
	    }
	r1 = genexpr(n->Nleft);
	code0(P_HRRZ, r1, r1);
	return r1;
	}

    /* Reuse an identical side-effect-free indexed fetch in a simple
    ** one-word integer operation.  This is deliberately tree-local CSE:
    ** no value table, basic-block walk, or persistent data-flow state.
    **
    **     p[i] + p[i]     -> load p[i] once; LSH R,1
    **
    ** Restrict the fold to dereferences and operators for which using the
    ** same loaded value is exact.  Addition uses a one-bit logical shift;
    ** for defined signed additions and for modulo unsigned arithmetic this is
    ** identical to x+x, while avoiding an unsafe MOVE/ADD-R,R peephole path.
    */
    if (optgen && sizetype(n->Ntype) == 1 && tisinteg(n->Ntype)
      && n->Nleft && n->Nright
      && n->Nleft->Nop == N_PTR && n->Nright->Nop == N_PTR
      && gsamepure(n->Nleft, n->Nright)) {
        switch (n->Nop) {
        case Q_PLUS:
            r1 = genexpr(n->Nleft);
            code8(P_LSH, r1, 1);
            return r1;
        case Q_ANDT:
        case Q_OR:
            return genexpr(n->Nleft);
        default:
            ;
        }
    }

    /* A literal minus a one-word integer needs no literal temporary.
    ** Negate the variable and add the literal instead:
    **
    **     MOVEI T,C       MOVN T,X
    **     SUB   T,X   ->  ADDI T,C
    **
    ** At a return boundary, a register-resident RHS can be written directly
    ** into AC1, avoiding the final return copy as well.
    */
    if (optgen && n->Nop == Q_MINUS && n->Nleft
      && n->Nleft->Nop == N_ICONST && n->Nright
      && sizetype(n->Ntype) == 1 && tisinteg(n->Ntype)) {
        if ((n->Nflag & NF_RETEXPR) && Register_Id(n->Nright)) {
            r1 = vrretget();
            r1->Vrtype = n->Ntype;
            code00(P_MOVN, r1->Vrloc, n->Nright->Nid->Sreg);
        } else {
            r1 = genexpr(n->Nright);
            code0(P_MOVN, r1, r1);
        }
        code1(P_ADD, r1, n->Nleft->Niconst);
        return r1;
    }

    /*
    ** First, check for pointer arithmetic.  Legal operations are:
    **	Operation	Result
    **	(1) num + ptr	ptr
    **	(2) ptr + num	ptr
    **	(3) ptr - num	ptr
    **	(4) ptr - ptr	int or long
    **
    **	If the pointer is a byte pointer, we always make the number first.
    ** This is only because the current optimizer is too stupid to recognize
    ** certain patterns any other way.
    */
    if (n->Ntype->Tspec == TS_PTR		/* Catch cases 1, 2, 3 */
	|| n->Nleft->Ntype->Tspec == TS_PTR)	/* Catch case 4 */
	{
	if (n->Nop == Q_MINUS)	/* Cases 3 and 4 */
	    {
	    if (n->Nright->Ntype->Tspec == TS_PTR)	/* Case 4: ptr-ptr */
		{
		r1 = genexpr(n->Nleft);		/* Make the left operand 1st */
		return gptrop(n->Nop, r1, genexpr(n->Nright),
			n->Nleft->Ntype, n->Nright->Ntype);
		}
	    else					/* Case 3: ptr-num */
		{
		r1 = genexpr(n->Nleft);				/* Make ptr */
		r2 = gptraddend(n->Nleft->Ntype, n->Nright);	/* Make num */
		return gptrop(n->Nop, r1, r2,
			n->Nleft->Ntype, n->Nright->Ntype);
		}
	    }
	/* Cases 1 and 2 */
	if (n->Nleft->Ntype->Tspec != TS_PTR)	/* Do case 1: num+ptr */
	    {
	    r2 = gptraddend(n->Nright->Ntype,n->Nleft);	/* Make num 1st */
	    return gptrop(n->Nop, genexpr(n->Nright), r2,
			n->Nright->Ntype, n->Nleft->Ntype);	/* reversed */
	    }
	else				/* Do case 2: ptr+num */
	    {
	    r1 = genexpr(n->Nleft);			/* Make ptr 1st */
	    r2 = gptraddend(n->Nleft->Ntype,n->Nright);	/* num 2nd */
	    return gptrop(n->Nop, r1, r2,
			n->Nleft->Ntype, n->Nright->Ntype);
	    }
	}

    /* Keep literal bitwise operands literal.  Going through genexpr() would
    ** first materialize the constant in an AC and rely on a later peephole
    ** to recover the immediate form.  code1() already selects IORI/ANDI/XORI
    ** or a literal as appropriate, so avoid the temporary entirely.
    */
    if (optgen && n->Nright->Nop == N_ICONST
      && sizetype(n->Ntype) == 1 && tisinteg(n->Ntype)
      && (n->Nop == Q_OR || n->Nop == Q_ANDT || n->Nop == Q_XORT))
        {
        r1 = genexpr(n->Nleft);
        code1(n->Nop == Q_OR ? P_IOR :
              (n->Nop == Q_ANDT ? P_AND : P_XOR),
              r1, n->Nright->Niconst);
        return r1;
        }

    /* No pointer arithmetic involved, can just generate arithmetic stuff.
    ** Normally we generate the left operand first, but if the right operand
    ** is a function call then we reverse the order so as to avoid
    ** saving/restoring registers across the call.
    ** Also, if using normal ordering, we check to see whether the left
    ** operand will need to be widened (since integer division requires
    ** a doubleword register), and if so widen it ahead of time so that
    ** the generation of the right operand won't suboptimally seize the
    ** 2nd register and then have to be shuffled around later.
    */
    if (n->Nright->Nop == N_FNCALL && optgen)
	{
	r2 = genexpr(n->Nright);	/* Do function call first */
	r1 = genexpr(n->Nleft);		/* then left operand */
	}
    else
	{
	r1 = genexpr(n->Nleft);		/* Normal order, left first */
	if (tisdimode(n->Ntype))
	    (void) vrstoreal(r1, VR2(r1)); /* keep left pair across right gen */
	if ((n->Nop == Q_DIV || n->Nop == Q_MOD) && tisinteg(n->Ntype)
		&& !tisdimode(n->Ntype) && optgen)
	    vrlowiden(r1);		/* Widen in preparation for div */
	r2 = genexpr(n->Nright);	/* Now generate right operand */
	}
    return garithop(n->Nop, r1, r2, n->Ntype->Tspec);
}

/* GARITHOP - Generate code for binary arithmetic operators
**	given values in registers.
** The only types permitted are:
**		TS_FLOAT, TS_DOUBLE, TS_LNGDBL
**		TS_INT, TS_UINT
**		TS_LONG, TS_ULONG
**	Note that types "char" and "short" should already have been converted
** (via usual unary/binary conversions) to "int" before the operation
** is performed.
*/

static VREG *
garithop(int op, struct vreg * r1, struct vreg * r2, int ts)
{
    switch(op)
	{
	case Q_ASPLUS:
	case Q_PLUS:
	    switch (ts)
		{
		default:
		    int_error("garithop: bad +");
		/* FALLTHROUGH */
		case TS_INT:
		case TS_UINT:
		case TS_LONG:
		case TS_ULONG:
		    code0(P_ADD,  r1, r2);
		    break;
		case TS_LONGLONG:
		case TS_ULONGLONG:
		    return gdimodeadd(r1, r2);
		case TS_FLOAT:
		    code0(P_FADR, r1, r2);
		    break;
		case TS_DOUBLE:
		case TS_LNGDBL:
		    code0(P_DFAD, r1, r2);
		    break;
		}
	    break;

	case Q_ASMINUS:
	case Q_MINUS:
	    switch (ts)
		{
		default:
		    int_error("garithop: bad -");
		/* FALLTHROUGH */
		case TS_INT:
		case TS_UINT:
		case TS_LONG:
		case TS_ULONG:
		    code0(P_SUB,  r1, r2);
		    break;
		case TS_LONGLONG:
		case TS_ULONGLONG:
		    return gdimodesub(r1, r2);
		case TS_FLOAT:
		    code0(P_FSBR, r1, r2);
		    break;
		case TS_DOUBLE:
		case TS_LNGDBL:
		    code0(P_DFSB, r1, r2);
		    break;
		}
	    break;

	/*	* Unsigned Multiplication
	**	MUL R,E
	**	TRNE R,1	or	LSH R+1,1	or	LSH R+1,1
	**	 TLOA R+1,400000	LSHC R,-1		LSHC R,-43.
	**	  TLZ R+1,400000
	**	result in R+1		result in R+1		result in R
	*/
	case Q_ASMPLY:
	case Q_MPLY:
	    switch (ts)
		{
		default:
		    int_error("garithop: bad *");
		/* FALLTHROUGH */
		case TS_LONGLONG:
		case TS_ULONGLONG:
		    return gdimodemul(r1, r2);
		case TS_UINT:
		case TS_ULONG:
		    if (!vrispair(r1))	/* Unless already widened, */
			vrlowiden(r1);	/* grab two words for the multiply */
		    code0(P_MUL, r1, r2);
		    code8(P_TRN+POF_ISSKIP+POS_SKPE, r1, 1);
		    code8(P_TLO+POF_ISSKIP+POS_SKPA, VR2(r1), 0400000L);
		    code8(P_TLZ, VR2(r1), 0400000L);
		    vrnarrow(r1 = VR2(r1));	/* Narrow back, keep 2nd wd */
		    break;
		case TS_INT:
		case TS_LONG:
		    code0(P_IMUL, r1, r2);
		    break;
		case TS_FLOAT:
		    code0(P_FMPR, r1, r2);
		    break;
		case TS_DOUBLE:
		case TS_LNGDBL:
		    code0(P_DFMP, r1, r2);
		    break;
		}
	    break;

    /* Integer division is done differently from other integer operations
    ** because the IDIV instruction produces a doubleword result.
    ** Note that we can't do the apparent optimization of using ASH or AND
    ** when the divisor is a constant power of two, because they perform
    ** inconsistently with IDIV on negative numbers.
    */
	case Q_ASDIV:
	case Q_DIV:
	    switch (ts)
		{
		default:
		    int_error("garithop: bad /");
		/* FALLTHROUGH */
		case TS_LONGLONG:
		case TS_ULONGLONG:
		    return gdimodedivmod(r1, r2, ts, 0);
		case TS_INT:
		case TS_UINT:
		case TS_LONG:
		case TS_ULONG: /* Hair for integer division */
			{
			int save_reg = r1->Vrloc;
			if (!vrispair(r1))	/*Unless already widened by gbinary,*/
			    vrlowiden(r1);	/* grab two words for the division. */
			code0((tspisunsigned(ts) ? P_UIDIV : P_IDIV), r1, r2);
			vrnarrow(r1);		/* Narrow back, keep 1st word */
			if (Register_Preserve(save_reg) && save_reg != r1->Vrloc)
			    {
			    code00(P_MOVE, save_reg, r1->Vrloc);
			    r1->Vrloc = save_reg;
			    }
			folddiv(r1);		/* Do cse on result */
			break;
			}
		case TS_FLOAT:
		    code0(P_FDVR, r1, r2);
		    break;
		case TS_DOUBLE:
		case TS_LNGDBL:
		    code0(P_DFDV, r1, r2);
		    break;
		}
	    break;

	case Q_ASMOD:
	case Q_MOD:
	    switch (ts)
		{
		default:
		    int_error("garithop: bad %%");
		/* FALLTHROUGH */
		case TS_LONGLONG:
		case TS_ULONGLONG:
		    return gdimodedivmod(r1, r2, ts, 1);
		case TS_INT:
		case TS_UINT:
		case TS_LONG:
		case TS_ULONG:
					/* Hair for integer remainder */
		    if (!vrispair(r1))	/* Unless already widened by gbinary,*/
			vrlowiden(r1);	/* grab two words for the division. */
		    code0((tspisunsigned(ts) ? P_UIDIV : P_IDIV), r1, r2);
		    vrnarrow(r1 = VR2(r1)); /* Narrow back, keep 2nd word */
		    folddiv(r1);		/* Do cse on result */
		    break;
		}
	    break;

	case Q_ASRSH:
	case Q_RSHFT:
	    if (ts == TS_LONGLONG || ts == TS_ULONGLONG)
		return gdimodeshift(op, r1, r2, ts);
	    code0(P_MOVN, r2, r2);		/* negate arg to make right shift */
					/* Then drop through to do shift */
	/* FALLTHROUGH */
	case Q_ASLSH:
	case Q_LSHFT:
	    switch (ts)
		{
		default:
		    int_error("garithop: bad shift");

		/* FALLTHROUGH */
		case TS_LONGLONG:
		case TS_ULONGLONG:
		    return gdimodeshift(op, r1, r2, ts);

		case TS_INT:	/* Signed values use arith shift for >> */
		case TS_LONG:
		    if (op == Q_ASRSH || op == Q_RSHFT)
			{
			code4(P_ASH, r1, r2);
			break;
			}
		/* Drop thru if <<, for logical shift. */
		/* According to CARM, << is always logical even if signed */

		/* FALLTHROUGH */
		case TS_UINT:		/* Unsigned values use logical shift */
		case TS_ULONG:
		    code4(P_LSH, r1, r2);	/* this takes arg as if PTA_RCONST */
		    break;
		}
	    break;

	case Q_ASOR:
	case Q_OR:
	    switch (ts)
		{
		default:
		    int_error("garithop: bad |");
		/* FALLTHROUGH */
		case TS_LONGLONG:
		case TS_ULONGLONG:
		    return gdimodebitwise(op, r1, r2);
		case TS_INT:
		case TS_UINT:
		case TS_LONG:
		case TS_ULONG:
		    code0 (P_IOR, r1, r2);
		    break;
		}
	    break;

	case Q_ASAND:
	case Q_ANDT:
	    switch (ts)
		{
		default:
		    int_error("garithop: bad &");
		/* FALLTHROUGH */
		case TS_LONGLONG:
		case TS_ULONGLONG:
		    return gdimodebitwise(op, r1, r2);
		case TS_INT:
		case TS_UINT:
		case TS_LONG:
		case TS_ULONG:
		    code0 (P_AND, r1, r2);
		    break;
		}
	    break;

	case Q_ASXOR:
	case Q_XORT:
	    switch (ts)
		{
		default:
		    int_error("garithop: bad ^");
		/* FALLTHROUGH */
		case TS_LONGLONG:
		case TS_ULONGLONG:
		    return gdimodebitwise(op, r1, r2);
		case TS_INT:
		case TS_UINT:
		case TS_LONG:
		case TS_ULONG:
		    code0 (P_XOR, r1, r2);
		    break;
		}
	    break;

	default:
	    int_error("garithop: bad op %d", op);
	    vrfree(r2);
	}
    return r1;
}

/* GPTROP - Generate code for pointer arithmetic operations.
**	Legal pointer arithmetic operations are:
**		Operation	Result
**		* (1) num + ptr	ptr
**		(2) ptr + num	ptr
**		(3) ptr - num	ptr	
**		(4) ptr - ptr	int or long
**
** NOTE: It is the caller's responsibility to swap the operands of case 1 to
** transform it into case 2.  It is up to the caller to decide which one
** to generate first; however, for case 4 it is probably best to do the
** left operand first.
**	If the 2nd operand is a number it must have been generated by
** gptraddend (rather than genexpr).  In this case, r2 may be NULL if
** gptraddend has determined that the number is zero and nothing needs
** to be added or subtracted.
*/
static VREG *
gptrop(int op, struct vreg * r1, struct vreg * r2, struct type * lt, struct type * rt)
{
    INT size;

    switch (op)
	{
	case Q_ASMINUS:
	case Q_MINUS:
	    if (rt->Tspec == TS_PTR)	/* Handle case 4 */
	    /* Handle case 4: ptr-ptr (make left operand first) */
		{
		if (tismaybitptr(lt))
		    return gmaybitsub(r1, r2, lt);
		if (tisbytepointer(lt))
		    {
		    vrlowiden(r1);			/* Must widen */
		    code0(P_SUBBP, r1, r2);		/* Do the sub */
		    if (previous && previous->Pop == P_SUBBP)
			previous->Pbsize = tisbitptr(lt) ? 1 : (tispackedptr(lt) ? TGSIZ_CHAR : elembsize(lt));
		    vrnarrow(r1 = VR2(r1));		/* Result in 2nd word */
		    }
		else
		    code0(P_SUB, r1, r2);
                if (lt->Tspec == TS_PTR && vlatype_v11(lt->Tsubt))
                    {
                    NODE *sn = vlastride_v11(lt);
                    VREG *sr = genexpr(sn);
                    if (sr == NULL)
                        int_error("gptrop: null VLA subtraction stride");
                    else {
                        vrlowiden(r1);
                        code0(P_IDIV, r1, sr);
                        vrfree(sr);
                        vrnarrow(r1);
                    }
                    }
		else if ((size = sizeptobj(lt)) > 1)
		    {
		    vrlowiden(r1);		/* Ugh, must adjust result */
		    code1(P_IDIV, r1, size);
		    vrnarrow(r1);		/* Narrow to get result in 1st wd */
		    folddiv(r1);
		    }
		break;
		}

	/* Handle case 3: ptr-num.  Num must be generated by gptraddend. */
	    if (r2 == NULL)
		return r1;	/* Ensure have something to subtract */
            if (tismaybitptr(lt))
                return gmaybitadjust(r1, r2, lt, 1);
	    if (tisbytepointer(lt))
		{
		code0(P_MOVN, r2, r2);
		code0(P_ADJBP, r2, r1);	/* Adjust char pointer */
		return r2;
		}
	    code0(P_SUB, r1, r2);		/* Adjust word pointer */
	    break;

	case Q_ASPLUS:
	case Q_PLUS:
	/* Handle case 2: ptr+num.  Num must be generated by gptraddend. */
	/* Note that case 1 should be transformed into case 2 by caller. */
	    if (r2 == NULL)
		return r1;	/* Ensure something to add */
            if (tismaybitptr(lt))
                return gmaybitadjust(r1, r2, lt, 0);
	    if (tisbytepointer(lt))		/* If ptr is a char ptr */
		{
		code0(P_ADJBP, r2, r1);	/* Adjust char pointer */
		return r2;
		}
	    code0(P_ADD, r2, r1);		/* Adjust word pointer */
	    return r2;

	default:
	    int_error("gptrop: bad op %d", op);
	}
    return r1;
}

/* GMAYBITADJUST - Adjust a representation-polymorphic exact-width pointer.
**
** The one-word pointer is self-describing.  An ordinary native byte pointer
** advances one native byte per C element.  S=1 denotes KCC's continuous
** packed bit stream, where one C object occupies its packed storage rounded
** to 9-bit address units.  P_ADJBP is intentionally used for both paths:
** PDP-6 expands it through KCC's software helper, while CPUs which provide
** ADJBP may use the hardware instruction.
*/
static VREG *
gmaybitadjust(VREG *ptr, VREG *count, TYPE *t, int neg)
{
    VREG *tmp, *p, *c, *r;
    SYMBOL *bitlab, *done;
    INT bits, stride;

    bits = (t != NULL && t->Tsubt != NULL) ? tbitsize(t->Tsubt) : 0;
    stride = ((bits + TGSIZ_CHAR - 1) / TGSIZ_CHAR) * TGSIZ_CHAR;
    if (stride <= 0)
        stride = TGSIZ_CHAR;

    /* Spill the two live inputs into real stack memory.  The PDP-6 ADJBP
    ** expansion may need scratch ACs, so virtual-register liveness across
    ** the two runtime-selected paths is deliberately avoided.
    */
    code0(P_PUSH, VR_SP, ptr);
    code0(P_PUSH, VR_SP, count);
    stackoffset += 2;
    vrfree(ptr);
    vrfree(count);

    tmp = vrget();
    codemdx(P_MOVE, vrtoreal(tmp), (SYMBOL *)NULL, -1, R_SP);
    code0(P_HLRZ, tmp, tmp);
    code8(P_LSH, tmp, -6);
    code1(P_AND, tmp, 077);
    bitlab = newlabel();
    done = newlabel();
    code8(P_CAI+POF_ISSKIP+POS_SKPN, tmp, 1);
    code6(P_JRST, (VREG *)NULL, bitlab);
    vrfree(tmp);

    /* Native byte-pointer representation. */
    p = vrget();
    c = vrget();
    codemdx(P_MOVE, vrtoreal(p), (SYMBOL *)NULL, -1, R_SP);
    codemdx(P_MOVE, vrtoreal(c), (SYMBOL *)NULL, 0, R_SP);
    if (neg)
        code0(P_MOVN, c, c);
    code0(P_ADJBP, c, p);
    codemdx(P_MOVEM, vrtoreal(c), (SYMBOL *)NULL, -1, R_SP);
    vrfree(p);
    vrfree(c);
    code6(P_JRST, (VREG *)NULL, done);
    flushcode();

    /* Logical packed representation. */
    codlabel(bitlab);
    p = vrget();
    c = vrget();
    codemdx(P_MOVE, vrtoreal(p), (SYMBOL *)NULL, -1, R_SP);
    codemdx(P_MOVE, vrtoreal(c), (SYMBOL *)NULL, 0, R_SP);
    if (stride != 1)
        code1(P_IMUL, c, stride);
    if (neg)
        code0(P_MOVN, c, c);
    code0(P_ADJBP, c, p);
    codemdx(P_MOVEM, vrtoreal(c), (SYMBOL *)NULL, -1, R_SP);
    vrfree(p);
    vrfree(c);
    flushcode();

    codlabel(done);
    r = vrget();
    r->Vrtype = t;
    codemdx(P_MOVE, vrtoreal(r), (SYMBOL *)NULL, -1, R_SP);
    code8(P_ADJSP, VR_SP, -2);
    stackoffset -= 2;
    return r;
}


/* GMAYBITSUB - Subtract representation-polymorphic exact-width pointers.
**
** Both pointers must designate elements of the same array, as required by C,
** so their runtime representations are the same.  Native pointers use their
** encoded byte size; logical packed pointers use S=1 and return a bit count,
** which is divided by the packed element stride to obtain a C element count.
*/
static VREG *
gmaybitsub(VREG *left, VREG *right, TYPE *t)
{
    VREG *tmp, *l, *r, *res;
    SYMBOL *bitlab, *done;
    INT bits, stride, nsize;

    bits = (t != NULL && t->Tsubt != NULL) ? tbitsize(t->Tsubt) : 0;
    stride = ((bits + TGSIZ_CHAR - 1) / TGSIZ_CHAR) * TGSIZ_CHAR;
    if (stride <= 0)
        stride = TGSIZ_CHAR;
    nsize = elembsize(t);
    if (nsize <= 0)
        nsize = TGSIZ_CHAR;

    code0(P_PUSH, VR_SP, left);
    code0(P_PUSH, VR_SP, right);
    stackoffset += 2;
    vrfree(left);
    vrfree(right);

    tmp = vrget();
    codemdx(P_MOVE, vrtoreal(tmp), (SYMBOL *)NULL, -1, R_SP);
    code0(P_HLRZ, tmp, tmp);
    code8(P_LSH, tmp, -6);
    code1(P_AND, tmp, 077);
    bitlab = newlabel();
    done = newlabel();
    code8(P_CAI+POF_ISSKIP+POS_SKPN, tmp, 1);
    code6(P_JRST, (VREG *)NULL, bitlab);
    vrfree(tmp);

    l = vrget();
    r = vrget();
    codemdx(P_MOVE, vrtoreal(l), (SYMBOL *)NULL, -1, R_SP);
    codemdx(P_MOVE, vrtoreal(r), (SYMBOL *)NULL, 0, R_SP);
    vrlowiden(l);
    code0(P_SUBBP, l, r);
    if (previous && previous->Pop == P_SUBBP)
        previous->Pbsize = nsize;
    vrnarrow(l = VR2(l));
    codemdx(P_MOVEM, vrtoreal(l), (SYMBOL *)NULL, -1, R_SP);
    vrfree(l);
    vrfree(r);
    code6(P_JRST, (VREG *)NULL, done);
    flushcode();

    codlabel(bitlab);
    l = vrget();
    r = vrget();
    codemdx(P_MOVE, vrtoreal(l), (SYMBOL *)NULL, -1, R_SP);
    codemdx(P_MOVE, vrtoreal(r), (SYMBOL *)NULL, 0, R_SP);
    vrlowiden(l);
    code0(P_SUBBP, l, r);
    if (previous && previous->Pop == P_SUBBP)
        previous->Pbsize = 1;
    vrnarrow(l = VR2(l));
    if (stride > 1)
        {
        vrlowiden(l);
        code1(P_IDIV, l, stride);
        vrnarrow(l);
        folddiv(l);
        }
    codemdx(P_MOVEM, vrtoreal(l), (SYMBOL *)NULL, -1, R_SP);
    vrfree(l);
    vrfree(r);
    flushcode();

    codlabel(done);
    res = vrget();
    res->Vrtype = ptrdifftype;
    codemdx(P_MOVE, vrtoreal(res), (SYMBOL *)NULL, -1, R_SP);
    code8(P_ADJSP, VR_SP, -2);
    stackoffset -= 2;
    return res;
}

static int
vlatype_v11(TYPE *t)
{
    while (t != NULL && t->Tspec == TS_ARRAY) {
        if (tisvla(t))
            return 1;
        t = t->Tsubt;
    }
    return 0;
}

static NODE *
vlastride_bin_v11(int op, NODE *l, NODE *r)
{
    return convbinary(ndeflr(op, l, r));
}

/* Return the pointer arithmetic stride for a pointer to a variably-sized
** array.  Byte pointers are measured in their native element units; normal
** pointers are measured in PDP-10 words, matching sizeptobj().
*/
static NODE *
vlastride_v11(TYPE *pt)
{
    TYPE *t, *base;
    NODE *n, *count;
    SYMBOL *bs;
    INT unit;

    if (pt == NULL || pt->Tspec != TS_PTR || !vlatype_v11(pt->Tsubt))
        return NULL;

    t = pt->Tsubt;
    base = t;
    while (base != NULL && base->Tspec == TS_ARRAY)
        base = base->Tsubt;

    if (tisbytepointer(pt)) {
        if (base != NULL && tispacked(base))
            unit = base->Tbytes;
        else
            unit = 1;
    } else
        unit = sizetype(base);
    n = ndeficonst(unit);

    while (t != NULL && t->Tspec == TS_ARRAY) {
        if (tisvla(t)) {
            bs = vlaboundsym_v11(t);
            if (bs != NULL)
                count = ndefident(bs);
            else {
                count = vlaboundexpr_v11(t);
                if (count == NULL) {
                    int_error("vlastride_v11: missing VLA bound");
                    count = ndeficonst(0);
                }
            }
        } else
            count = ndeficonst(t->Tsize);
        n = vlastride_bin_v11(Q_MPLY, n, count);
        t = t->Tsubt;
    }
    return n;
}

/* GPTRADDEND - Auxiliary to GPTROP.  This routine generates the
**	proper value for adding or subtracting from a pointer.
**	Note that it may return NULL if it determines that the value
**	is zero; that is, no value (and no operation) is necessary.
*/
static VREG *
gptraddend(TYPE *t, NODE *n)
/* Type of the pointer this value is being added to and Addend(or subtrahend)
 * expression */
{
    VREG *r;
    INT size;

    /* A function-boundary exact-width pointer is self-describing.  Keep
    ** its C element count unscaled here; gmaybitadjust() selects native
    ** byte-pointer stride versus logical S=1 bit stride at runtime.
    */
    if (tismaybitptr(t))
        {
        if (n->Nop == N_ICONST && n->Niconst == 0)
            return NULL;
        return genexpr(n);
        }

    if (t->Tspec == TS_PTR && vlatype_v11(t->Tsubt)) {
        NODE *sn;
        VREG *sr;

        if (n->Nop == N_ICONST && n->Niconst == 0)
            return NULL;
        r = genexpr(n);
        sn = vlastride_v11(t);
        sr = genexpr(sn);
        if (r == NULL || sr == NULL) {
            if (r != NULL) vrfree(r);
            if (sr != NULL) vrfree(sr);
            int_error("gptraddend: null VLA stride");
            return NULL;
        }
        code0(P_IMUL, r, sr);
        vrfree(sr);
        return r;
    }

    if (n->Nop == N_ICONST && optgen)		/* Do optimization */
	{
	size = sizeptobj(t) * n->Niconst;	/* If num is a constant */
	if (size == 0)
	    return NULL;		/* Zero value, gen nothing! */
	r = vrget();
	code1(P_MOVE, r, size);
	r->Vrtype = n->Ntype;		/* Set C type of object in reg */
	return r;
	}
    r = genexpr(n);			/* First generate value as given */
    if ((size = sizeptobj(t)) > 1)	/* Then check to see if mult needed */
	code1(P_IMUL, r, size);	/* Yeah, multiply it by size of obj */
    return r;
}

/* GLOGICAL - Generate code for boolean binary & unary operators
*/

static VREG *
glogical(NODE *n)
{
    VREG *reg;
    SYMBOL *false, *true, *temp;
    int reverse;

    reverse = (optgen && n->Nop == Q_LOR);
    n->Nendlab = true = newlabel();	/* get label for true case */
    false = newlabel();			/* get label for false case */

    /*
    ** See gternary() for an explanation of why this call is needed.
    */
    vrallspill();

    gboolean (n, false, reverse);	/* make the boolean code */
    if (optgen && unjump (false))	/* can put false case first? */
	{
	temp = false;			/* yes, swap meaning of false */
	false = true;			/* and true, so labels go out */
	true = temp;			/* in correct order. */
	reverse = !reverse;		/* also invert reversal switch */
	}

    if (n->Nflag & NF_RETEXPR)
	reg = vrretget(); /* get value in return reg */
    else
	reg = vrget();		/* not for return, use normal reg */
    reg->Vrtype = n->Ntype;		/* Set C type of object in reg */
    codlabel(true);			/* true label goes here */
    if (reverse)
	code0(P_TDZ+POF_ISSKIP+POS_SKPA, reg, reg); /* make zero, skip */
    else
	code1(P_SKIP+POF_ISSKIP+POS_SKPA, reg, 1); /* make one, skip */

    codlabel(false);			/* now make false label */
    if (reverse)
	code1(P_MOVE, reg, 1);	/* reversed false makes one */
    else
	code5(P_SETZ, reg);		/* normal false makes zero */
    return reg;				/* return the register */
}

extern int _chnl;

/* GUNARY - Generate code for unary operators
*/

static VREG *
gunary(NODE *n)
{
    VREG *r;
    int volat;

    switch (n->Nop)
	{
	case N_PREINC:
	    return gincdec(n,  1, 1);
	case N_PREDEC:
	    return gincdec(n, -1, 1);
	case N_POSTINC:
	    return gincdec(n,  1, 0);
	case N_POSTDEC:
	    return gincdec(n, -1, 0);

	case N_CAST:
	    return gcast(n);
	case N_ADDR:
	    return gaddress(n->Nleft);

	case N_PTR:
	/* See comments at gprimary() about volatile objects. */
	    if ((volat = tisvolatile(n->Ntype)) != 0)
		flushcode();		/* Obj is volatile, avoid optimiz */

	    if (debcsi == KCC_DBG_NULL)
		{
		_chnl = n->sfline;
		switch (n->Nleft->Nop)
		    {
		    case Q_IDENT:
			code4 (P_NULPTR, (VREG *) NULL, gaddress (n->Nleft));
			break;
		    case N_PREINC:
		    case N_PREDEC:
		    case N_POSTINC:
		    case N_POSTDEC:
			code4 (P_NULPTR, (VREG *) NULL, gaddress (n->Nleft->Nleft));
			break;
		    default:
			_chnl = -1;
			break;
		    }
		}

        if (tisinteg(n->Ntype) && maybitptrderef(n))
            {
            r = gmaybitload(n);
            if (volat)
                flushcode();
            return r;
            }

        if (tisinteg(n->Ntype) && (packedptrderef(n) || bitptrmember(n)))
            {
            r = gpackedload(n);
            if (volat)
                flushcode();
            return r;
            }

	/* A register-resident pointer still points at memory.  The old
	** Register_Id shortcut treated the pointed-to object itself as a
	** register object, which fails for structures and is semantically
	** wrong for every dereference.  Evaluate the pointer value normally
	** and load through it.
	*/
	    if (optgen && !Register_Id(n->Nleft)
		&& tisbytepointer(n->Nleft->Ntype)
		&& n->Nleft->Nop == N_PREINC)
		{
		r = vrget();
		r->Vrtype = n->Ntype;
		if (Register_Id(n->Nleft->Nleft))
		    code14(P_ILDB, r, n->Nleft->Nleft->Nid->Sreg);
		else
		    code4(P_ILDB, r, gaddress(n->Nleft->Nleft));
		}
	    else
		r = getmem(genexpr(n->Nleft), n->Ntype,
			    tisbytepointer(n->Nleft->Ntype), 0);
	    if (volat)
		flushcode();
	    return r;

	case Q_MUUO:
	    return gmuuo(n);

	case N_NEG:
	    if (Register_Id(n->Nleft))
		{
#if 0
		if ( n->Ntype->Tspec == TS_DOUBLE
			|| n->Ntype->Tspec == TS_LNGDBL)
		    {
		    r = vrdget();
		    r->Vrtype = n->Nleft->Ntype;
		    code00(P_DMOVN, r->Vrloc, n->Nleft->Nid->Sreg);
		    }
		else
#endif
		    {
		    r = vrget();
		    r->Vrtype = n->Nleft->Ntype;
		    code00(P_MOVN, r->Vrloc, n->Nleft->Nid->Sreg);
		    }
		return r;
		}
	    r = genexpr(n->Nleft);
	    if (tisdimode(n->Ntype))
		{
		if (!vrispair(r))
		    int_error("gunary N_NEG: non-pair dimode %N", n);
		flushcode();
		gdimodeneg(r);
		}
	    else if ( n->Ntype->Tspec == TS_DOUBLE
		    || n->Ntype->Tspec == TS_LNGDBL)
		code0(P_DMOVN, r, r);
	    else
		code0(P_MOVN, r, r);
	    return r;

	case Q_COMPL:
	    if (Register_Id(n->Nleft))
		{
		r = vrget();
		r->Vrtype = n->Nleft->Ntype;
		code00(P_SETCM, r->Vrloc, n->Nleft->Nid->Sreg);
		return r;
		}

	    r = genexpr(n->Nleft);
	    if (tisdimode(n->Ntype))
		{
		if (!vrispair(r))
		    int_error("gunary Q_COMPL: non-pair dimode %N", n);
		gdimodecompl(r);
		}
	    else
		code0(P_SETCM, r, r);
	    return r;

	default:
	    int_error("gunary: bad op %N", n);
	    return 0;
	}
}

/* GCAST - Generate code for type conversion (cast)
**
**	Note that the way we manage the task of keeping char values
** masked off is NOT by implementing a mask for casts to (char) type.
** Rather, we mask the register value only when widening.  This works
** because a value of type (char) is always either assigned to a (char) object
** (in which case a byte pointer is used and the mask is automatic) or
** it is used in an expression -- and always promoted to an int or u_int.
** The masking would be wasteful and unnecessary for the first case, and
** the second case will always have an explicit N_CAST to widen the integer.
** See the INTERN.DOC file for a better explanation.
*/

static VREG *
gcast(NODE *n)
{
    VREG *r;
    /* If this expression is a return value, see if we can pass on
    ** the flag which marks it thusly.  This basically benefits
    ** gcall() which uses the flag to do tail recursion; we want to ensure
    ** that a no-op cast won't prevent this optimization.
    */
    if ((n->Nflag & NF_RETEXPR)		/* This expr is a return val? */
	&& gcastr(n->Ncast, (VREG *)NULL,	/* and cast is a no-op? */
			n->Nleft->Ntype, n->Ntype, n->Nleft) == NULL)
	{
	n->Nleft->Nflag |= NF_RETEXPR;	/* Yes, pass flag on! */
	if ((r = genexpr(n->Nleft)) != NULL) /* No cast, just generate expr */
	    r->Vrtype = n->Ntype;	/* and reflect correct type */
	return r;
	}

    return gcastr(n->Ncast, genexpr(n->Nleft),
			n->Nleft->Ntype, n->Ntype, n->Nleft);
}

static VREG *
gcastptr(VREG *r, TYPE *tfrom, TYPE *tto)
{
    int fsiz, tsiz;

    /* Erasing the pointed-to type must leave one canonical opaque form.
    ** A TF_MAYBITPTR value may arrive either as a native byte pointer or as
    ** KCC's S=1 logical bit address.  Preserve S=1 and NULL; convert every
    ** native form to S=1 before the value becomes an ordinary void *.
    **
    ** The value is spilled because codlabel() may flush the virtual-register
    ** state.  Never return a VREG which lived across such a control-flow join.
    */
    if (tismaybitptr(tfrom) && tto != NULL && tto->Tspec == TS_PTR
      && tto->Tsubt != NULL && tto->Tsubt->Tspec == TS_VOID) {
        VREG *tmp, *v;
        SYMBOL *done;

        if (!r)
            return (VREG *)-1;
        fsiz = elembsize(tfrom);
        if (!fsiz)
            fsiz = TGSIZ_CHAR;

        code0(P_PUSH, VR_SP, r);
        ++stackoffset;
        vrfree(r);

        done = newlabel();
        tmp = vrget();
        codemdx(P_MOVE, vrtoreal(tmp), (SYMBOL *)NULL, 0, R_SP);
        code6(P_JUMP+POS_SKPE, tmp, done);       /* NULL stays NULL. */
        code0(P_HLRZ, tmp, tmp);
        code8(P_LSH, tmp, -6);
        code1(P_AND, tmp, 077);
        code8(P_CAI+POF_ISSKIP+POS_SKPN, tmp, 1);
        code6(P_JRST, (VREG *)NULL, done);      /* Already canonical S=1. */
        vrfree(tmp);

        v = vrget();
        codemdx(P_MOVE, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
        code10(P_PTRCNV, v, (SYMBOL *)NULL, 1, -fsiz);
        codemdx(P_MOVEM, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
        vrfree(v);
        flushcode();
        codlabel(done);

        r = vrget();
        r->Vrtype = tto;
        codemdx(P_MOVE, vrtoreal(r), (SYMBOL *)NULL, 0, R_SP);
        code8(P_ADJSP, VR_SP, -1);
        --stackoffset;
        return r;
    }

    /* A void pointer may contain either an ordinary word address or the
    ** canonical S=1 packed bit address above.  Preserve S=1 verbatim; only
    ** ordinary word pointers need normal word-to-byte conversion.
    */
    if (tfrom != NULL && tfrom->Tspec == TS_PTR
      && tfrom->Tsubt != NULL && tfrom->Tsubt->Tspec == TS_VOID
      && tismaybitptr(tto) && tisbytepointer(tto)) {
        VREG *tmp, *v;
        SYMBOL *done;

        if (!r)
            return (VREG *)-1;
        tsiz = elembsize(tto);
        if (!tsiz)
            tsiz = TGSIZ_CHAR;

        code0(P_PUSH, VR_SP, r);
        ++stackoffset;
        vrfree(r);
        done = newlabel();

        tmp = vrget();
        codemdx(P_MOVE, vrtoreal(tmp), (SYMBOL *)NULL, 0, R_SP);
        code6(P_JUMP+POS_SKPE, tmp, done);       /* NULL stays NULL. */
        code0(P_HLRZ, tmp, tmp);
        code8(P_LSH, tmp, -6);
        code1(P_AND, tmp, 077);
        code8(P_CAI+POF_ISSKIP+POS_SKPN, tmp, 1);
        code6(P_JRST, (VREG *)NULL, done);      /* Canonical S=1 survives. */
        vrfree(tmp);

        v = vrget();
        codemdx(P_MOVE, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
        pitopc(v, tsiz, 0, 0);
        codemdx(P_MOVEM, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
        vrfree(v);
        flushcode();
        codlabel(done);

        r = vrget();
        r->Vrtype = tto;
        codemdx(P_MOVE, vrtoreal(r), (SYMBOL *)NULL, 0, R_SP);
        code8(P_ADJSP, VR_SP, -1);
        --stackoffset;
        return r;
    }

    /* Converting a representation-polymorphic pointer to an ordinary word
    ** pointer discards byte/logical pointer metadata.  Every supported
    ** representation keeps the containing word address in the RH.
    */
    if (tismaybitptr(tfrom) && tto != NULL && tto->Tspec == TS_PTR
      && tto->Tsubt != NULL && tto->Tsubt->Tspec != TS_VOID
      && !tisbytepointer(tto)) {
        if (!r)
            return (VREG *)-1;
        code10(P_TDZ, r, (SYMBOL *)NULL, -1, 0);
        r->Vrtype = tto;
        return r;
    }

    /* Casting between two non-native exact-width pointer types keeps the
    ** self-describing function-boundary representation.  Native pointers
    ** must change their encoded byte size; S=1 already denotes the exact
    ** first bit and therefore only changes C type.  Anchor the value across
    ** the runtime-selected join so codlabel() cannot invalidate its VREG.
    */
    if (tismaybitptr(tfrom) && tismaybitptr(tto)
      && tisbytepointer(tto)) {
        VREG *tmp, *v;
        SYMBOL *done;

        fsiz = elembsize(tfrom);
        tsiz = elembsize(tto);
        if (!fsiz)
            fsiz = TGSIZ_CHAR;
        if (!tsiz)
            tsiz = TGSIZ_CHAR;
        if (fsiz == tsiz) {
            if (r)
                r->Vrtype = tto;
            return r;
        }
        if (!r)
            return (VREG *)-1;

        code0(P_PUSH, VR_SP, r);
        ++stackoffset;
        vrfree(r);
        done = newlabel();

        tmp = vrget();
        codemdx(P_MOVE, vrtoreal(tmp), (SYMBOL *)NULL, 0, R_SP);
        code6(P_JUMP+POS_SKPE, tmp, done);       /* NULL stays NULL. */
        code0(P_HLRZ, tmp, tmp);
        code8(P_LSH, tmp, -6);
        code1(P_AND, tmp, 077);
        code8(P_CAI+POF_ISSKIP+POS_SKPN, tmp, 1);
        code6(P_JRST, (VREG *)NULL, done);      /* S=1 changes type only. */
        vrfree(tmp);

        v = vrget();
        codemdx(P_MOVE, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
        code10(P_PTRCNV, v, (SYMBOL *)NULL, tsiz, -fsiz);
        codemdx(P_MOVEM, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
        vrfree(v);
        flushcode();
        codlabel(done);

        r = vrget();
        r->Vrtype = tto;
        codemdx(P_MOVE, vrtoreal(r), (SYMBOL *)NULL, 0, R_SP);
        code8(P_ADJSP, VR_SP, -1);
        --stackoffset;
        return r;
    }

    /* Materialize an ordinary destination byte-pointer representation from
    ** a function-boundary representation-polymorphic pointer.  S=1 and
    ** native source pointers require different source sizes, so select the
    ** conversion at runtime.  The result is anchored on the stack across
    ** the join to avoid stale VREG state.
    */
    if (tismaybitptr(tfrom) && tisbytepointer(tto)
      && !tismaybitptr(tto)) {
        VREG *tmp, *v;
        SYMBOL *bitlab, *done;

        fsiz = elembsize(tfrom);
        tsiz = elembsize(tto);
        if (!fsiz)
            fsiz = TGSIZ_CHAR;
        if (!tsiz)
            tsiz = TGSIZ_CHAR;
        if (!r)
            return (VREG *)-1;

        code0(P_PUSH, VR_SP, r);
        ++stackoffset;
        vrfree(r);
        bitlab = newlabel();
        done = newlabel();

        tmp = vrget();
        codemdx(P_MOVE, vrtoreal(tmp), (SYMBOL *)NULL, 0, R_SP);
        code6(P_JUMP+POS_SKPE, tmp, done);       /* NULL stays NULL. */
        code0(P_HLRZ, tmp, tmp);
        code8(P_LSH, tmp, -6);
        code1(P_AND, tmp, 077);
        code8(P_CAI+POF_ISSKIP+POS_SKPN, tmp, 1);
        code6(P_JRST, (VREG *)NULL, bitlab);
        vrfree(tmp);

        /* Native source representation. */
        if (fsiz != tsiz) {
            v = vrget();
            codemdx(P_MOVE, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
            code10(P_PTRCNV, v, (SYMBOL *)NULL, tsiz, -fsiz);
            codemdx(P_MOVEM, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
            vrfree(v);
        }
        code6(P_JRST, (VREG *)NULL, done);
        flushcode();

        /* Canonical logical source representation. */
        codlabel(bitlab);
        v = vrget();
        codemdx(P_MOVE, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
        code10(P_PTRCNV, v, (SYMBOL *)NULL, tsiz, -1);
        codemdx(P_MOVEM, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
        vrfree(v);
        flushcode();

        codlabel(done);
        r = vrget();
        r->Vrtype = tto;
        codemdx(P_MOVE, vrtoreal(r), (SYMBOL *)NULL, 0, R_SP);
        code8(P_ADJSP, VR_SP, -1);
        --stackoffset;
        return r;
    }


    if (tisbytepointer(tfrom)) {
        if (tisbytepointer(tto)) {
            fsiz = elembsize(tfrom);
            tsiz = elembsize(tto);
            if (!fsiz) {
                if (tischarpointer(tto))
                    return r;
                fsiz = TGSIZ_CHAR;
            }
            if (!tsiz) {
                if (tischarpointer(tfrom))
                    return r;
                tsiz = TGSIZ_CHAR;
            }
            if (fsiz == tsiz)
                return r;
            if (!r)
                return (VREG *)-1;
            if ((fsiz == TGSIZ_CHAR && tsiz == TGSIZ_SHORT)
              || (fsiz == TGSIZ_SHORT && tsiz == TGSIZ_CHAR)) {
                code10(P_PTRCNV, r, (SYMBOL *)NULL, tsiz, fsiz);
                return r;
            }
            code10(P_TDZ+POF_ISSKIP+POS_SKPE, r, (SYMBOL *)NULL, -1, 0);
            code10(P_IOR, r, (SYMBOL *)NULL, tsiz, 0);
            return r;
        }
        if (!r)
            return (VREG *)-1;
        code10(P_TDZ, r, (SYMBOL *)NULL, -1, 0);
        return r;
    }

    if (tisbytepointer(tto)) {
        if (!r)
            return (VREG *)-1;
        tsiz = elembsize(tto);
        if (!tsiz)
            tsiz = TGSIZ_CHAR;
        pitopc(r, tsiz, 0, 0);
    }
    return r;
}

static VREG *
gcastr(int cop, struct vreg * r, struct type * tfrom, struct type * tto, struct node * ln)
{
    switch (cop)
	{
	case CAST_NONE:		/* No actual action required */
	    break;

	case CAST_VOID:		/* Throwing away the value */
	    if (r)
		relflush(r);	/* Release the register */
	    return NULL;

	case CAST_BOOL:
	    if (!r)
		return (VREG *)-1;
	    /* Pair-valued scalar zero is represented by two zero words. */
	    if (vrispair(r)) {
		code0(P_IOR, r, VR2(r));
		vrnarrow(r);
	    }
	    /* SKIPE leaves zero unchanged and skips the MOVEI; any nonzero
	    ** value falls through and becomes the canonical value 1.
	    */
	    code0(P_SKIP+POF_ISSKIP+POS_SKPE, r, r);
	    code1(P_MOVE, r, 1);
	    r->Vrtype = tto;
	    break;

	case CAST_IT_PT:
	    if (!r)					/* Just checking? */
		return gintwiden(r, tfrom, uinttype, ln);
	    else
		r = gintwiden(r, tfrom, uinttype, ln); /* Widen int to uint */
	    break;

	case CAST_IT_EN:
	case CAST_IT_IT:
	    if (tisdimode(tto) && !tisdimode(tfrom))
		{
		if (!r)
		    return (VREG *)-1;
		r = gdimode_from_int(r, tfrom, tto, ln);
		}
	    else if (tisdimode(tfrom) && !tisdimode(tto))
		{
		int bits;

		if (!r)
		    return (VREG *)-1;
		bits = tbitsize(tto);

		/* A normalized 71-bit value has only 35 independent low bits.
		** Reconstruct destination bit 35 from bit 0 of the high word
		** before discarding the high word.  This implements modulo 2^36
		** conversion for both signed and unsigned full-word targets.
		*/
		if (bits == TGSIZ_WORD)
		    {
		    (void) vrstoreal(r, VR2(r));
		    code8(P_TLZ, VR2(r), 0400000L);
		    code8(P_TRN+POF_ISSKIP+POS_SKPE, r, 1);
		    code8(P_TLO, VR2(r), 0400000L);
		    }
		vrnarrow(r = VR2(r));
		r->Vrtype = tto;

		/* For smaller targets, apply the same truncation and signedness
		** conversion used by assignment and ordinary explicit casts.
		*/
		if (bits < TGSIZ_WORD)
		    {
		    if (tisunsign(tto))
			code1(P_AND, r, ((INT)1 << bits) - 1);
		    else if (bits == TGSIZ_HALFWD)
			code0(P_HRRE, r, r);
		    else
			{
			code8(P_TRN+POF_ISSKIP+POS_SKPE, r,
			      ((INT)1 << (bits-1)));
			code8(P_TRO+POF_ISSKIP+POS_SKPA, r,
			      -((INT)1 << bits));
			code1(P_AND, r, ((INT)1 << bits) - 1);
			}
		    }
		}
	    else if (tbitsize(tto) < tbitsize(tfrom))
		{
		int bits = tbitsize(tto);

		/* Match assignment conversion for explicit narrow casts. */
		if (!r)
		    return (VREG *)-1;
		if (tisunsign(tto))
		    code1(P_AND, r, ((INT)1 << bits) - 1);
		else if (bits == TGSIZ_HALFWD)
		    code0(P_HRRE, r, r);
		else
		    {
		    code8(P_TRN+POF_ISSKIP+POS_SKPE, r,
			  ((INT)1 << (bits-1)));
		    code8(P_TRO+POF_ISSKIP+POS_SKPA, r,
			  -((INT)1 << bits));
		    code1(P_AND, r, ((INT)1 << bits) - 1);
		    }
		}
	    else if (!r)				/* Just checking? */
		return gintwiden(r, tfrom, tto, ln);
	    else
#if 0	/* Later, Reg linkage */	
	    if (Register_Nopreserve (r->Vrloc) && unsigned)
#endif
		r = gintwiden(r, tfrom, tto, ln); /*Widen integer if needed */
	    break;

	case CAST_EN_EN:
	case CAST_EN_IT:
	case CAST_PT_IT:			/* No representation change needed */
	    break;

	case CAST_PT_PT:			/* General ptr to ptr conversion */
	    r = gcastptr(r, tfrom, tto);
	    if (r == (VREG *)-1)
		return r;
	    break;

	case CAST_FP_IT:
	    if (!r)
		return (VREG *)-1;	/* Stop if just checking. */
	    switch (tfrom->Tspec)
		{
		case TS_FLOAT:
		    code0(P_FIX, r, r);	/* just use that! */
		    break;
		case TS_DOUBLE:
		case TS_LNGDBL:
		    code0(P_DFIX, r, r);	/* r must be a register pair */
		    if (vrispair(r))
			vrnarrow(r);		/* Use 1st AC as result */
		    break;
		}
	/* Narrow the int here if needed */
	    break;

	case CAST_FP_FP:
	    switch (castidx(tfrom->Tspec,tto->Tspec))
		{
		case castidx(TS_DOUBLE,TS_FLOAT):
		case castidx(TS_LNGDBL,TS_FLOAT):
		    if (!r)
			return (VREG *)-1;	/* Stop if just checking. */
		    code0(P_DSNGL, r, r);	/* r must be a register pair! */
		    if (vrispair(r))
			vrnarrow(r);		/* Forget about the second word */
		    break;
		case castidx(TS_FLOAT,TS_DOUBLE):
		case castidx(TS_FLOAT,TS_LNGDBL):
		    if (!r)
			return (VREG *)-1;	/* Stop if just checking. */
		    vrlowiden(r);
		    code5(P_SETZ, VR2(r));
		    break;
		case castidx(TS_LNGDBL,TS_DOUBLE):
		case castidx(TS_DOUBLE,TS_LNGDBL):
		    break;
		}
	    break;

	case CAST_IT_FP:
	    if (!r)
		return (VREG *)-1;	/* Stop if just checking. */
	    r = gintwiden(r, tfrom,		/* Ensure widened to int or unsigned */
		    tissigned(tfrom) ? inttype : uinttype,
		    ln);
	    switch (tto->Tspec)
		{
		case TS_FLOAT:
		/* Although FLTR and UFLTR are always supported by CCOUT,
		** on KA-10s they are inefficient enough that it is worth
		** checking for the opportunity to use a simple FSC, which
		** is limited to integers of 27 bits or less.
		*/
		    if (tissigned(tfrom) || tbitsize(tfrom) < TGSIZ_WORD)
			{
		    /* Signed or known positive */
			code0(P_FLTR, r, r); /* Use FLTR instr or macro */
			break;
			}
		/* Ugh, unsigned full word value, must use hairy UFLTR. */
		    code0(P_UFLTR, r, r); /* Use UFLTR simulated op */
		    break;

		case TS_DOUBLE:
		case TS_LNGDBL:
		    vrlowiden(r);	/* Make into register pair */
		    code5(P_SETZ, VR2(r)); /* zero the next reg */
		    if ((tgcpu == TGCPU_PDP6 || tgcpu == TGCPU_KA)
		      && (tissigned(tfrom) || tbitsize(tfrom) < TGSIZ_WORD))
			{
			/* PDP-6/KA10 have no FLTR instruction and the old
			** ASHC/TLC/DFAD-zero normalization sequence is not
			** reliable with the DAIMON helper path.  A single-float
			** conversion in the high word plus a zero low word gives
			** the expected double value for signed/small integer inputs.
			*/
			code0(P_FLTR, r, r);
			break;
			}
		    if (tissigned(tfrom) || tbitsize(tfrom) < TGSIZ_WORD)
			{
			code8(P_ASHC, r, -8); /* shift out mantissa*/
			code8(P_TLC, r, 0243000L); /* put exponent in */
			}
		    else			/* Unsigned conversion */
			{
			code8(P_LSHC, r, -9);		/* Shift unsigned */
			code8(P_LSH, VR2(r), -1); /* Fix up lo wd */
			code8(P_TLC, r, 0244000L); /* exp (note 1 bigger!) */
			}
		    code9(P_DFAD, r, 0.0, 1);	/* Normalize the result */
		    break;
		}
	    break;

	default:
	    int_error("gcastr: bad cast %d", cop);
	    return NULL;
	}

    /* Cast done, now set new type of object in virtual register! */
    if (r)
	r->Vrtype = tto;
    return r;
}

/* GINTWIDEN and GUINTWIDEN - Auxiliaries for GCAST to widen integral values.
**	Always widens to full word even if new type is smaller, because
**	it's just as easy and makes no difference to handling of new type.
** NOTE: treats a VREG arg of NULL just as gcastr() does, i.e. only checks
**	to see whether a conversion would be necessary or not.
** GUINTWIDEN is a subroutine just so gboolean() can invoke it to force
**	an unsigned-type widen.
*/
static VREG *
gintwiden(VREG *r, TYPE *tfrom, TYPE *tto, NODE *n)		
/* Node that R was generated from (if any) */
{
    /* A narrowing N_CAST is normalized by gcastr().  Do not emit the same
    ** mask/sign extension again for the implicit promotion of its result.
    */
    if (n && n->Nop == N_CAST
      && tbitsize(n->Ntype) < tbitsize(n->Nleft->Ntype))
	return r;

    if (tbitsize(tto) > tbitsize(tfrom))
	{
	if (tisunsign(tfrom))	/* Handle unsigned.  Easy, just mask off */
	    {
	    r = guintwiden(r, tbitsize(tfrom), n);
	    }
	else		/* Handle signed.  Harder, must extend the sign bit. */
	    {
	    int fbits = tbitsize(tfrom);

	    if (!r)
		return (VREG *)-1;		/* Stop if just checking. */
	    if (fbits == TGSIZ_HALFWD)	/* Special case */
		{
		code0(P_HRRE, r, r);		/* Extend sign of halfwd */
		return r;
		}
	    if (fbits == 32 && tbitsize(tto) == TGSIZ_WORD) {
		/* Exact 32-bit integers occupy either a packed 32-bit field or
		** a full word.  Discard any storage padding, then arithmetic
		** shift back so target bit 31 becomes the PDP-10 sign bit.
		*/
		code8(P_LSH, r, TGSIZ_WORD - fbits);
		code8(P_ASH, r, -(TGSIZ_WORD - fbits));
		return r;
	    }
	    code8(P_TRN+POF_ISSKIP+POS_SKPE, r, ((INT)1 << (fbits-1)));
	    code8(P_TRO+POF_ISSKIP+POS_SKPA, r, -((INT)1 << fbits));
	    code1(P_AND, r, ((INT)1 << fbits)-1);	/* Positive, zap! */
	    }
	}
    return r;
}

static VREG *
guintwiden(VREG *r, int fbitsize, NODE *n)
/* # bits of value in R and Node that R was generated from (if any) */
{
    /* Must zap high-order bits.  Try to avoid doing this by
    ** seeing whether those bits are known to already be zero.
    ** Primary case is that of an LDB data fetch.
    */
    if (!(n &&
      (bptrref(n) > 0			/* Win if LDB fetch */
      || (n->Nop == Q_ASGN		/* Or if an assignment of a */
	&& bptrref(n->Nright) > 0	/* LDB also of safe size */
	&& tbitsize(n->Nright->Ntype) <= fbitsize))) )
	{
	if (!r)
	    return (VREG *)-1;		/* Stop if just checking. */
	code1(P_AND, r, ((INT) 1 << fbitsize)-1);	/* Zap! */
	}
    return r;
}

/* GINCDEC - Generate code for prefix/postfix increment/decrement.
**	This is special-cased (instead of being handled by general
**	arith code) both for efficiency and because the address is
**	only supposed to be evaluated once.  The code also checks
**	for NF_DISCARD to see whether the result value is needed or not;
**	if not, it forces the operation to be prefix instead of postfix,
**	so that all fixup work can be avoided!
*/

static VREG *
gincdec(NODE *n, int inc, int pre)
/* The inc/dec expression node, +1 for increment, -1 for decrement, and 
 * True if prefix, else postfix.
 */
{
    VREG *r, *ra, *r2;
    INT size = 1;		/* Default size for most common case */
    int savaddr;
    int volat;
    int wantret = ((n->Nflag & NF_RETEXPR) && fnargkeepmask);

    if (n->Nflag & NF_DISCARD)	/* Will result be discarded? */
	pre = 1;		/* If so, prefix form is always better! */
    n = n->Nleft;		/* Mainly interested in operand */
    if ((volat = tisvolatile(n->Ntype)) != 0)
	flushcode();		/* Barfo, avoid optimiz of volatile obj */

    /* _Bool increment/decrement applies the arithmetic operation after
    ** integer promotion and then converts the result back to _Bool.  Since
    ** a stored bool is canonical 0/1, ++ always stores 1 and -- toggles it.
    ** Keep the original value separately for postfix expressions.
    */
    if (tisbool(n->Ntype))
	{
	if (Register_Id(n))
	    {
	    int sr = n->Nid->Sreg;
	    r = vrget();
	    r->Vrtype = n->Ntype;
	    if (!pre)
		code00(P_MOVE, r->Vrloc, sr);
	    if (inc > 0)
		codr1(P_MOVE, sr, 1);
	    else
		codr1(P_XOR, sr, 1);
	    if (pre)
		code00(P_MOVE, r->Vrloc, sr);
	    }
	else
	    {
	    ra = gaddress(n);
	    r = getmem(ra, n->Ntype, 1, 1);
	    r2 = NULL;
	    if (!pre)
		{
		r2 = vrget();
		r2->Vrtype = n->Ntype;
		codek0(P_MOVE, r2, r);
		}
	    if (inc > 0)
		code1(P_MOVE, r, 1);
	    else
		code1(P_XOR, r, 1);
	    stomem(r, ra, 1, 0);
	    if (!pre)
		{
		vrfree(r);
		r = r2;
		}
	    }
	if (volat)
	    flushcode();
	return r;
	}

    if (Register_Id(n))
	{
	void codr1(int, int, INT);
	if (pre)	/* r->Vrloc = n->Nid->Sreg, if preserve reg */
	    r_preserve = n->Nid->Sreg;

	switch(n->Ntype->Tspec)
	    {
	    case TS_FLOAT:
		r = vrget();
		r->Vrtype = n->Ntype;	/* Set C type of object in reg */
		if (!pre)
		    code00(P_MOVE, r->Vrloc, n->Nid->Sreg);
		codr1(P_FADR, n->Nid->Sreg,(INT) ((inc > 0)? 1.0 : -1.0)); // FW KCC-NT
		if (pre)
		    code00(P_MOVE, r->Vrloc, n->Nid->Sreg);
		break;
#if 0	/* for next version of KCC regs */
	    case TS_DOUBLE:
	    case TS_LNGDBL:
		r = vrdget();
		r->Vrtype = n->Ntype;	/* Set C type of object in reg */
		if (!pre)
		    {
		    code00(P_DMOVE, r->Vrloc, n->Nid->Sreg);
		    r_preserve = n->Nid->Sreg;
		    ra = vrdget();
		    ra->Vrtype = n->Ntype;/* Set C type of object in reg */
		    code9(P_DFAD, ra, ((inc > 0)? 1.0 : -1.0), 1);
		    vrfree(ra);
		    }
		else
		    code9(P_DFAD, r, ((inc > 0)? 1.0 : -1.0), 1);
		break;
	    case TS_ENUM:
	    case TS_BITF:
	    case TS_UBITF:
#endif
	    case TS_PTR:
		size = sizeptobj(n->Ntype);
		if (!size)
		    int_error("gincdec: 0-size reg ptr %N", n);
		if (tisbytepointer(n->Ntype))
		    {
		    if (!pre)
			{
			r = vrget();
			r->Vrtype = n->Ntype;
			code00(P_MOVE, r->Vrloc, n->Nid->Sreg);
			}

		    /* ADJBP needs separate count/result and pointer operands.
		    ** Use KCC's reserved scratch AC and flush around the
		    ** sequence so register coalescing cannot make them alias.
		    */
		    flushcode();
		    codr1(P_MOVE, R_SCRREG, inc * size);
		    code00(P_ADJBP, R_SCRREG, n->Nid->Sreg);
		    code00(P_MOVE, n->Nid->Sreg, R_SCRREG);
		    flushcode();

		    if (pre)
			{
			r = vrget();
			r->Vrtype = n->Ntype;
			code00(P_MOVE, r->Vrloc, n->Nid->Sreg);
			}
		    }
		else
		    {
		    r = vrget();
		    r->Vrtype = n->Ntype;
		    if (!pre)
			code00(P_MOVE, r->Vrloc, n->Nid->Sreg);
		    codr1(P_ADD, n->Nid->Sreg,
			  (INT)((inc > 0) ? size : -size));
		    if (pre)
			code00(P_MOVE, r->Vrloc, n->Nid->Sreg);
		    }
		break;

	    case TS_INT:
	    case TS_UINT:
	    case TS_LONG:
	    case TS_ULONG:
	    case TS_CHAR:
	    case TS_UCHAR:
	    case TS_SHORT:
	    case TS_USHORT:
		r = vrget();
		r->Vrtype = n->Ntype;	/* Set C type of object in reg */
		if (!pre)
		    code00(P_MOVE, r->Vrloc, n->Nid->Sreg);
		codr1(P_ADD, n->Nid->Sreg,((inc > 0)? 1 : -1));
		if (pre)
		    code00(P_MOVE, r->Vrloc, n->Nid->Sreg);
		break;
	    default:
		int_error("gincdec: bad reg type %N", n);
		return NULL;
	    }
	}
    else
	{
	/* DImode is represented by an ordinary two-word integral type, so it
	** has no unique Tspec switch arm.  Handle it before the scalar switch.
	** Keep the lvalue address alive across the load/update/store sequence;
	** this also guarantees that indirect and indexed lvalues are evaluated
	** exactly once.
	*/
	if (tisdimode(n->Ntype))
	    {
	    static int dimodeinclab;
	    int lab;
	    int rflags, r2flags;
	    char buf[192];
	    int len;

	    ra = gaddress(n);
	    r = getmem(ra, n->Ntype, 0, 1);
	    if (!pre)
		{
		rflags = r->Vrflags;
		r2flags = VR2(r)->Vrflags;
		r->Vrflags |= VRF_LOCK;
		VR2(r)->Vrflags |= VRF_LOCK;
		r2 = vrdget();
		r->Vrflags = rflags;
		VR2(r)->Vrflags = r2flags;
		r2->Vrtype = n->Ntype;
		VR2(r2)->Vrtype = n->Ntype;
		gdimove(r2, r);
		}

	    (void) vrstoreal(r, VR2(r));
	    lab = dimodeinclab++;
	    if (inc > 0)
		len = kccfmt(buf, sizeof(buf),
		    "\tADDI\t%o,1\n"
		    "\tJUMPGE\t%o,%%DIINC%d\n"
		    "\tAND\t%o,[0377777777777]\n"
		    "\tADDI\t%o,1\n"
		    "%%DIINC%d:\n",
		    vrreal(VR2(r)), vrreal(VR2(r)), lab,
		    vrreal(VR2(r)), vrreal(r), lab);
	    else
		len = kccfmt(buf, sizeof(buf),
		    "\tSUBI\t%o,1\n"
		    "\tJUMPGE\t%o,%%DIDEC%d\n"
		    "\tADD\t%o,[0400000000000]\n"
		    "\tSUBI\t%o,1\n"
		    "%%DIDEC%d:\n",
		    vrreal(VR2(r)), vrreal(VR2(r)), lab,
		    vrreal(VR2(r)), vrreal(r), lab);
	    codestr(buf, len);
	    stomem(r, ra, 2, 0);
	    if (!pre)
		{
		vrfree(r);
		r = r2;
		}
	    if (volat)
		flushcode();
	    return r;
	    }


        /* Increment/decrement through a function-boundary exact-width pointer
        ** must preserve the pointer's runtime representation.  If the pointer
        ** expression has side effects, save its raw word once and reuse it for
        ** the dynamic load and store.
        */
        if (maybitptrderef(n))
            {
            int saved, pdepth;
            TYPE *optype;

            saved = sideffp(n);
            pdepth = 0;
            if (saved)
                {
                ra = genexpr(n->Nleft);
                code0(P_PUSH, VR_SP, ra);
                ++stackoffset;
                pdepth = stackoffset;
                vrfree(ra);
                r = gmaybitloaddepth(n, pdepth);
                }
            else
                r = gmaybitload(n);

            if (!pre)
                {
                r2 = vrget();
                r2->Vrtype = n->Ntype;
                codek0(P_MOVE, r2, r);
                }
            code1((inc > 0 ? P_ADD : P_SUB), r, 1);
            optype = (tbitsize(n->Ntype) < TGSIZ_WORD) ? inttype : n->Ntype;
            if (optype != n->Ntype)
                r = gcastr(CAST_IT_IT, r, optype, n->Ntype, n);
            if (saved)
                {
                (void) gmaybitstoredepth(r, n, pdepth);
                code8(P_ADJSP, VR_SP, -1);
                --stackoffset;
                }
            else
                (void) gmaybitstore(r, n);
            if (!pre)
                {
                vrfree(r);
                r = r2;
                }
            if (volat)
                flushcode();
            return r;
            }

        if (((n->Nop == Q_MEMBER || n->Nop == Q_DOT)
             && (packedcross(n->Nxoff) || packedbit(n->Nxoff) || packedbitscalar(n->Nxoff) || crossbit(n->Nxoff)))
          || packedptrderef(n) || bitptrmember(n))
            {
            TYPE *optype;

            ra = NULL;
            if (sideffp(n))
                {
                int raflags;
                ra = gaddress(n);
                raflags = ra->Vrflags;
                ra->Vrflags |= VRF_LOCK;
                r = gpackedloadat(n, ra);
                ra->Vrflags = raflags;
                }
            else
                r = gpackedload(n);
            if (!pre)
                {
                r2 = vrget();
                r2->Vrtype = n->Ntype;
                codek0(P_MOVE, r2, r);
                }
            code1((inc > 0 ? P_ADD : P_SUB), r, 1);
            optype = (tbitsize(n->Ntype) < TGSIZ_WORD) ? inttype : n->Ntype;
            if (optype != n->Ntype)
                r = gcastr(CAST_IT_IT, r, optype, n->Ntype, n);
            if (ra != NULL)
                {
                int raflags = ra->Vrflags;
                ra->Vrflags |= VRF_LOCK;
                (void) gpackedstoreat(r, n, ra);
                ra->Vrflags = raflags;
                vrfree(ra);
                }
            else
                (void) gpackedstore(r, n);
            if (!pre)
                {
                vrfree(r);
                r = r2;
                }
            if (volat)
                flushcode();
            return r;
            }

        /* Function-boundary exact-width pointers may be either native byte
        ** pointers or logical S=1 pointers.  Increment/decrement must retain
        ** that runtime representation and therefore uses the same dynamic
        ** adjustment path as p +/- 1.  Keep the lvalue address across the
        ** load/adjust/store sequence so it is evaluated exactly once.
        */
        if (tismaybitptr(n->Ntype))
            {
            VREG *cnt, *old;

            ra = gaddress(n);
            r = getmem(ra, n->Ntype, 0, 1);
            old = NULL;
            if (!pre)
                {
                old = vrget();
                old->Vrtype = n->Ntype;
                codek0(P_MOVE, old, r);
                }
            cnt = vrget();
            cnt->Vrtype = inttype;
            code1(P_MOVE, cnt, 1);
            r = gmaybitadjust(r, cnt, n->Ntype, inc < 0);
            stomem(r, ra, 1, 0);
            if (!pre)
                {
                vrfree(r);
                r = old;
                }
            if (volat)
                flushcode();
            return r;
            }

	switch (n->Ntype->Tspec)
	    {
	    case TS_FLOAT:
		r = vrget();
		r->Vrtype = n->Ntype;	/* Set C type of object in reg */
		code9(P_MOVE, r, (inc > 0 ? 1.0 : -1.0), 0);
		code4(P_FADR+POF_BOTH, r, gaddress(n));
		if (!pre)
		    code9(P_FSBR, r, (inc > 0 ? 1.0 : -1.0), 0);
		break;

	    case TS_DOUBLE:
	    case TS_LNGDBL:
		r = vrdget();
		r->Vrtype = n->Ntype;	/* Set C type of object in reg */
		if ((savaddr = sideffp(n)) != 0) /* See if lvalue has side effects */
		    {
		    ra = gaddress(n);		/* Yes, make address first */
		    code9(P_DMOVE, r, (inc > 0 ? 1.0 : -1.0), 1);
		    codek4(P_DFAD, r, ra);	/* Do op, keep address reg around */
		    code4(P_DMOVEM, r, ra);
		    }
		else
		    {
		    code9(P_DMOVE, r, (inc > 0 ? 1.0 : -1.0), 1);
		    code4(P_DFAD, r, gaddress(n));
		    code4(P_DMOVEM, r, gaddress(n));
		    }
		if (!pre)
		    code9(P_DFSB, r, (inc > 0 ? 1.0 : -1.0), 1);
		break;

	    case TS_PTR:			/* Hacking pointer? */
		size = sizeptobj(n->Ntype);	/* Find size of obj */
		if (!size)
		    int_error("gincdec: 0-size obj %N", n);
		if (tisbytepointer(n->Ntype))	/* Special if a (char *) */
		    {
		    if (inc < 0)
			size = -size;
		    if ((savaddr = sideffp(n)) != 0) /* See addr has side effs */
			ra = gaddress(n);		/* Ugh, find & save it */
		    r = vrget();
		    r->Vrtype = n->Ntype;		/* Set C type of obj in reg */

		/* If doing post-increment, save orig pointer value */
		    if (!pre)
			{
			r2 = vrget();
			r->Vrtype = n->Ntype;	/* Set C type of obj in reg */
			if (savaddr)
			    codek4(P_MOVE, r2, ra);	/* Save ptr */
			else
			    code4(P_MOVE, r2, gaddress(n));
			}

		/* Now perform the increment.  If the address of the pointer
		** was saved in ra, it is released in this process.  r has
		** a copy of the new pointer value.
		*/
		    if (size == 1)		/* Special case */
			{
			if (savaddr)
			    codek4(P_IBP, 0, ra);
			else
			    code4(P_IBP, (VREG *)NULL, gaddress(n));
			if (pre)			/* If will need val, get it. */
			    code4(P_MOVE, r, (savaddr ? ra : gaddress(n)));
			}
		    else			/* General case */
			{
			code1(P_MOVE, r, size);	/* get how much */
			if (savaddr)
			    codek4(P_ADJBP, r, ra);
			else
			    code4(P_ADJBP, r, gaddress(n));
			code4(P_MOVEM, r,		/* store back in memory */
				    (savaddr ? ra : gaddress(n)));
			}

		/* Now, if doing postincrement, flush r and use r2 instead */
		    if (!pre)
			{
			vrfree(r);
			r = r2;
			}

		/* Consecutive byte-pointer inc/dec operations can otherwise be
		** folded incorrectly by the peephole pass because simulated ADJBP
		** sequences update a memory byte pointer through helper code rather
		** than a single ordinary memory op.  Keep the update boundary exact.
		*/
		    flushcode();
		    break;		/* Break out to return R */
		    }
	    /* Drop through to handle non-char pointer as integer */


	    /* FALLTHROUGH */
	    case TS_ENUM:
	    case TS_INT:
	    case TS_UINT:
	    case TS_LONG:
	    case TS_ULONG:
		r = wantret ? vrretget() : vrget();
		r->Vrtype = n->Ntype;	/* Set C type of obj in reg */
		if (size == 1)
		    code4((inc > 0 ? P_AOS : P_SOS), r, gaddress(n));
		else				/* inc/dec by non-1 integer */
		    {
		    code1(P_MOVE, r, (inc > 0 ? size : -size));
		    code4(P_ADD+POF_BOTH, r, gaddress(n));
		    }
		if (!pre)			/* For postincrement, undo reg */
		    code1((inc > 0 ? P_SUB : P_ADD), r, size); /* undo change */
		break;

	    case TS_BITF:
	    case TS_UBITF:
	    case TS_CHAR:
	    case TS_UCHAR:
	    case TS_SHORT:
	    case TS_USHORT:
		if (inc < 0)
		    size = -size;
		savaddr = sideffp(n);       /* See if addr has side effs */
		ra = gaddress(n);		/* Ugh, find & save it */
	    /* Fetch byte, save addr if savaddr != 0 */
		r = getmem(ra, n->Ntype, 1, savaddr);

		code1(P_ADD, r, size);	/* Add inc/dec value */

	    /* Now store byte back */
		if (!savaddr)
		    ra = gaddress(n);	/* else, re-use ra */
		stomem(r, ra, 1, 1);

		if (!pre)				/* For postfix, undo reg */
		    code1(P_SUB, r, size);		/* undo change */
		break;

	    default:
		int_error("gincdec: bad type %N", n);
		return NULL;
	    }
	}
    if (volat)
	flushcode();		/* Finish up after volatile obj */
    return r;
}

/* GPRIMARY - Generate primary expression.
**
** This handles all primary expressions, which are composed of node ops
**	N_FNCALL,
**	Q_DOT,		(may be lvalue)
**	Q_MEMBER,	(always lvalue)
**	Q_IDENT,	(always lvalue)
**	N_ICONST, N_FCONST, N_PCONST, N_SCONST, N_VCONST, Q_ASM.
** The first three of those are not terminal nodes and may have further
** sub-expressions.
** Note that array subscripting is done as pointer arithmetic rather than
** using a specific operator.  Similarly, parenthesized expressions have
** no specific op since the parse tree structure reflects any parenthesizing.
**	This is where array and function names are caught and turned into
** pointers instead.  Arrays and functions are the only Q_IDENTs for which
** the node type (Ntype) is different from the symbol type (Stype)!  The
** symbol type will have the actual type of the name, whereas the node type
** will be that of "pointer to <Stype>".
**	Note special checking for fetching a value from "volatile"-qualified
** lvalues.  There are only four nodes that can be lvalues -- the three above,
** plus N_PTR which is handled in gunary().  Storing into those lvalues is
** handled by gassign() and gincdec().  Because the peephole optimizer is
** such a mess, we can't easily tell it to avoid volatile objects; instead
** we simply flush out all peephole code before and after generating the
** fetch from (or store into) a volatile object!  Crude, but should work.
*/
/* GMAYBITLOAD - Load through a function-boundary exact-width pointer.
** The raw pointer is self-describing: S=1 denotes KCC's logical packed bit
** stream; any other S retains the ordinary native byte-pointer semantics.
*/
static VREG *
gmaybitload(NODE *n)
{
    VREG *base, *r;
    int depth;

    base = genexpr(n->Nleft);
    code0(P_PUSH, VR_SP, base);
    ++stackoffset;
    depth = stackoffset;
    vrfree(base);
    r = gmaybitloaddepth(n, depth);
    code8(P_ADJSP, VR_SP, -1);
    --stackoffset;
    return r;
}

/* GMAYBITLOADDEPTH - Load through a representation-polymorphic pointer
** whose raw pointer word is already saved at the given stack depth.
*/
static VREG *
gmaybitloaddepth(NODE *n, int depth)
{
    VREG *tmp, *r, *p, *q, *b;
    SYMBOL *bitlab, *done;
    int bits, i;
    INT off;

    bits = tbitsize(n->Ntype);
    off = depth - stackoffset;

    tmp = vrget();
    codemdx(P_MOVE, vrtoreal(tmp), (SYMBOL *)NULL, off, R_SP);
    code0(P_HLRZ, tmp, tmp);
    code8(P_LSH, tmp, -6);
    code1(P_AND, tmp, 077);
    bitlab = newlabel();
    done = newlabel();
    code8(P_CAI+POF_ISSKIP+POS_SKPN, tmp, 1);
    code6(P_JRST, (VREG *)NULL, bitlab);
    vrfree(tmp);

    r = vrget();
    r->Vrtype = uinttype;
    p = vrget();
    off = depth - stackoffset;
    codemdx(P_MOVE, vrtoreal(p), (SYMBOL *)NULL, off, R_SP);
    code0(P_LDB, r, p);
    code6(P_JRST, (VREG *)NULL, done);
    flushcode();

    codlabel(bitlab);
    code5(P_SETZ, r);
    for (i = 0; i < bits; ++i)
        {
        p = vrget();
        off = depth - stackoffset;
        if (i == 0)
            codemdx(P_MOVE, vrtoreal(p), (SYMBOL *)NULL, off, R_SP);
        else
            {
            b = vrget();
            codemdx(P_MOVE, vrtoreal(b), (SYMBOL *)NULL, off, R_SP);
            code1(P_MOVE, p, i);
            code0(P_ADJBP, p, b);
            flushcode();
            }
        q = vrget();
        q->Vrtype = uinttype;
        code0(P_LDB, q, p);
        if (i != 0)
            code8(P_LSH, r, 1);
        code0(P_IOR, r, q);
        }
    flushcode();
    codlabel(done);
    return gcastr(CAST_IT_IT, r, uinttype, n->Ntype, (NODE *)NULL);
}

/* GMAYBITSTORE - Store through a representation-polymorphic pointer.
** Native byte pointers use one DPB.  S=1 pointers store the exact-width
** value one bit at a time into the continuous packed stream.  The pointer
** and value are anchored in stack memory because PDP-6 ADJBP simulation
** may consume scratch accumulators.
*/
static VREG *
gmaybitstore(VREG *reg, NODE *n)
{
    VREG *base, *r;
    int depth;

    base = genexpr(n->Nleft);
    code0(P_PUSH, VR_SP, base);
    ++stackoffset;
    depth = stackoffset;
    vrfree(base);
    r = gmaybitstoredepth(reg, n, depth);
    code8(P_ADJSP, VR_SP, -1);
    --stackoffset;
    return r;
}

/* GMAYBITSTOREDEPTH - Store through a representation-polymorphic pointer
** whose raw pointer word is already saved at the given stack depth.
*/
static VREG *
gmaybitstoredepth(VREG *reg, NODE *n, int depth)
{
    VREG *tmp, *p, *q, *v, *r;
    SYMBOL *bitlab, *done;
    int bits, i, shift;
    INT off;

    bits = tbitsize(n->Ntype);
    code0(P_PUSH, VR_SP, reg);
    ++stackoffset;
    vrfree(reg);

    off = depth - stackoffset;
    tmp = vrget();
    codemdx(P_MOVE, vrtoreal(tmp), (SYMBOL *)NULL, off, R_SP);
    code0(P_HLRZ, tmp, tmp);
    code8(P_LSH, tmp, -6);
    code1(P_AND, tmp, 077);
    bitlab = newlabel();
    done = newlabel();
    code8(P_CAI+POF_ISSKIP+POS_SKPN, tmp, 1);
    code6(P_JRST, (VREG *)NULL, bitlab);
    vrfree(tmp);

    p = vrget();
    v = vrget();
    off = depth - stackoffset;
    codemdx(P_MOVE, vrtoreal(p), (SYMBOL *)NULL, off, R_SP);
    codemdx(P_MOVE, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
    code0(P_DPB, v, p);
    vrfree(p);
    vrfree(v);
    code6(P_JRST, (VREG *)NULL, done);
    flushcode();

    codlabel(bitlab);
    for (i = 0; i < bits; ++i)
        {
        p = vrget();
        off = depth - stackoffset;
        codemdx(P_MOVE, vrtoreal(p), (SYMBOL *)NULL, off, R_SP);
        if (i != 0)
            {
            q = vrget();
            code1(P_MOVE, q, i);
            code0(P_ADJBP, q, p);
            vrfree(p);
            p = q;
            }
        v = vrget();
        codemdx(P_MOVE, vrtoreal(v), (SYMBOL *)NULL, 0, R_SP);
        shift = bits - i - 1;
        if (shift != 0)
            code8(P_LSH, v, -shift);
        code1(P_AND, v, 1);
        code0(P_DPB, v, p);
        vrfree(v);
        vrfree(p);
        }
    flushcode();

    codlabel(done);
    r = vrget();
    r->Vrtype = n->Ntype;
    codemdx(P_MOVE, vrtoreal(r), (SYMBOL *)NULL, 0, R_SP);
    code8(P_ADJSP, VR_SP, -1);
    --stackoffset;
    return r;
}

/* GPACKEDLOAD - Load a scalar packed member that crosses a word.
** Keep the address as a native 9-bit PDP-10 byte pointer throughout.
** A packed scalar occupies ceil(bits/9) C address units; if the final unit
** is partial, its value bits occupy the high part and its low bits are
** padding.  Assemble at most four fragments into one 36-bit accumulator.
*/
static VREG *
gpackedload(NODE *n)
{
    return gpackedloadat(n, (VREG *)NULL);
}

static VREG *
gpackedloadat(NODE *n, VREG *savedbase)
{
    VREG *base, *p, *q, *r;
    int bits, units, tail, i, nbits, saveflags;

    bits = tbitsize(n->Ntype);
    if (bitptrderef(n) || bitptrmember(n) || crossbit(n->Nxoff))
        {
        int done, bflags;
        VREG *acc;

        if (savedbase != NULL)
            {
            base = vrget();
            base->Vrtype = savedbase->Vrtype;
            codek0(P_MOVE, base, savedbase);
            }
        else
            base = gaddress(n);
        bflags = base->Vrflags;
        base->Vrflags |= VRF_LOCK;
        acc = vrget();
        acc->Vrtype = uinttype;
        code5(P_SETZ, acc);
        for (done = 0; done < bits; ++done)
            {
            p = vrget();
            if (done == 0)
                codek0(P_MOVE, p, base);
            else
                {
                code1(P_MOVE, p, done);
                codek0(P_ADJBP, p, base);
                }
            q = vrget();
            q->Vrtype = uinttype;
            code0(P_LDB, q, p);
            vrfree(p);
            if (done != 0)
                code8(P_LSH, acc, 1);
            code0(P_IOR, acc, q);
            }
        base->Vrflags = bflags;
        vrfree(base);
        return gcastr(CAST_IT_IT, acc, uinttype, n->Ntype, (NODE *)NULL);
        }
    if (packedbit(n->Nxoff) || packedbitscalar(n->Nxoff))
        {
        INT bitoff = packedmembit(n->Nxoff);
        int done = 0;
        VREG *acc = NULL;
        if (savedbase != NULL)
            {
            base = vrget();
            base->Vrtype = savedbase->Vrtype;
            codek0(P_MOVE, base, savedbase);
            }
        else
            base = gaddress(n);
        saveflags = base->Vrflags;
        base->Vrflags |= VRF_LOCK;
        while (done < bits)
            {
            INT apos = bitoff + done;
            int boff = (int)(apos / TGSIZ_CHAR)
                     - (int)(bitoff / TGSIZ_CHAR);
            int skip = (int)(apos % TGSIZ_CHAR);
            int take = TGSIZ_CHAR - skip;
            int rshift;
            if (take > bits - done) take = bits - done;
            p = vrget();
            if (boff == 0) codek0(P_MOVE, p, base);
            else { code1(P_MOVE, p, boff); codek0(P_ADJBP, p, base); }
            r = vrget(); r->Vrtype = uinttype; code0(P_LDB, r, p);
            rshift = TGSIZ_CHAR - skip - take;
            if (rshift) code8(P_LSH, r, -rshift);
            if (take < TGSIZ_WORD)
                code1(P_AND, r, (((INT)1 << take) - 1));
            if (acc == NULL) acc = r;
            else
                {
                code8(P_LSH, acc, take);
                code0(P_IOR, acc, r);
                }
            done += take;
            }
        base->Vrflags = saveflags;
        vrfree(base);
        acc->Vrtype = uinttype;
        return gcastr(CAST_IT_IT, acc, uinttype, n->Ntype, (NODE *)NULL);
        }
    units = (bits + TGSIZ_CHAR - 1) / TGSIZ_CHAR;
    tail = bits % TGSIZ_CHAR;
    if (savedbase != NULL)
            {
            base = vrget();
            base->Vrtype = savedbase->Vrtype;
            codek0(P_MOVE, base, savedbase);
            }
        else
            base = gaddress(n);
    saveflags = base->Vrflags;
    base->Vrflags |= VRF_LOCK;
    q = NULL;

    for (i = 0; i < units; ++i)
        {
        p = vrget();
        if (i == 0)
            codek0(P_MOVE, p, base);
        else
            {
            code1(P_MOVE, p, i);
            codek0(P_ADJBP, p, base);
            }
        r = vrget();
        r->Vrtype = uinttype;
        code0(P_LDB, r, p);
        nbits = (i == units-1 && tail != 0) ? tail : TGSIZ_CHAR;
        if (nbits != TGSIZ_CHAR)
            code8(P_LSH, r, -(TGSIZ_CHAR - nbits));
        if (q == NULL)
            q = r;
        else
            {
            code8(P_LSH, q, nbits);
            code0(P_IOR, q, r);
            }
        }
    base->Vrflags = saveflags;
    vrfree(base);
    q->Vrtype = uinttype;
    return gcastr(CAST_IT_IT, q, uinttype, n->Ntype, (NODE *)NULL);
}

/* GPACKEDSTORE - Store a scalar packed member that crosses a word.
** Split the value into 9-bit address-unit fragments.  A final partial
** fragment is left-justified in its 9-bit storage unit, matching the packed
** layout used by GCC and leaving the trailing padding bits zero.
*/
static VREG *
gpackedstore(VREG *reg, NODE *n)
{
    return gpackedstoreat(reg, n, (VREG *)NULL);
}

static VREG *
gpackedstoreat(VREG *reg, NODE *n, VREG *savedbase)
{
    VREG *base, *p, *q;
    INT mask;
    int bits, units, tail, i, nbits, below, savebase, savereg;

    bits = tbitsize(n->Ntype);
    if (bitptrderef(n) || bitptrmember(n) || crossbit(n->Nxoff))
        {
        int done, bflags, rflags, shift;

        if (savedbase != NULL)
            {
            base = vrget();
            base->Vrtype = savedbase->Vrtype;
            codek0(P_MOVE, base, savedbase);
            }
        else
            base = gaddress(n);
        bflags = base->Vrflags;
        rflags = reg->Vrflags;
        base->Vrflags |= VRF_LOCK;
        reg->Vrflags |= VRF_LOCK;
        for (done = 0; done < bits; ++done)
            {
            p = vrget();
            if (done == 0)
                codek0(P_MOVE, p, base);
            else
                {
                code1(P_MOVE, p, done);
                codek0(P_ADJBP, p, base);
                }
            q = vrget();
            q->Vrtype = uinttype;
            codek0(P_MOVE, q, reg);
            shift = bits - done - 1;
            if (shift != 0)
                code8(P_LSH, q, -shift);
            code1(P_AND, q, 1);
            code0(P_DPB, q, p);
            vrfree(p);
            vrfree(q);
            }
        reg->Vrflags = rflags;
        base->Vrflags = bflags;
        vrfree(base);
        return reg;
        }
    if (packedbit(n->Nxoff) || packedbitscalar(n->Nxoff))
        {
        INT bitoff = packedmembit(n->Nxoff);
        int storebits = packedbitscalar(n->Nxoff)
                      ? ((bits + TGSIZ_CHAR - 1) / TGSIZ_CHAR) * TGSIZ_CHAR
                      : bits;
        int done = 0;
        if (savedbase != NULL)
            {
            base = vrget();
            base->Vrtype = savedbase->Vrtype;
            codek0(P_MOVE, base, savedbase);
            }
        else
            base = gaddress(n);
        savebase = base->Vrflags;
        savereg = reg->Vrflags;
        base->Vrflags |= VRF_LOCK;
        reg->Vrflags |= VRF_LOCK;
        while (done < storebits)
            {
            INT apos = bitoff + done;
            int boff = (int)(apos / TGSIZ_CHAR)
                     - (int)(bitoff / TGSIZ_CHAR);
            int skip = (int)(apos % TGSIZ_CHAR);
            int take = TGSIZ_CHAR - skip;
            int bshift, srcshift;
            INT bmask;
            if (take > storebits - done) take = storebits - done;
            bshift = TGSIZ_CHAR - skip - take;
            bmask = (((INT)1 << take) - 1) << bshift;

            p = vrget();
            if (boff == 0) codek0(P_MOVE, p, base);
            else { code1(P_MOVE, p, boff); codek0(P_ADJBP, p, base); }
            q = vrget(); q->Vrtype = uinttype; code0(P_LDB, q, p);
            vrfree(p);
            code1(P_AND, q, 0777 ^ bmask);

            if (done < bits)
                {
                int vtake = take;
                VREG *v;
                if (vtake > bits - done) vtake = bits - done;
                v = vrget(); codek0(P_MOVE, v, reg);
                srcshift = bits - done - vtake;
                if (srcshift) code8(P_LSH, v, -srcshift);
                if (vtake < TGSIZ_WORD)
                    code1(P_AND, v, (((INT)1 << vtake) - 1));
                if (bshift + (take - vtake))
                    code8(P_LSH, v, bshift + (take - vtake));
                code0(P_IOR, q, v); vrfree(v);
                }
            p = vrget();
            if (boff == 0) codek0(P_MOVE, p, base);
            else { code1(P_MOVE, p, boff); codek0(P_ADJBP, p, base); }
            code0(P_DPB, q, p); vrfree(q);
            done += take;
            }
        base->Vrflags = savebase;
        reg->Vrflags = savereg;
        vrfree(base);
        return reg;
        }
    units = (bits + TGSIZ_CHAR - 1) / TGSIZ_CHAR;
    tail = bits % TGSIZ_CHAR;
    if (savedbase != NULL)
            {
            base = vrget();
            base->Vrtype = savedbase->Vrtype;
            codek0(P_MOVE, base, savedbase);
            }
        else
            base = gaddress(n);
    savebase = base->Vrflags;
    savereg = reg->Vrflags;
    base->Vrflags |= VRF_LOCK;
    reg->Vrflags |= VRF_LOCK;
    below = bits;

    for (i = 0; i < units; ++i)
        {
        nbits = (i == units-1 && tail != 0) ? tail : TGSIZ_CHAR;
        below -= nbits;
        q = vrget();
        codek0(P_MOVE, q, reg);
        if (below != 0)
            code8(P_LSH, q, -below);
        mask = ((INT)1 << nbits) - 1;
        code1(P_AND, q, mask);
        if (nbits != TGSIZ_CHAR)
            code8(P_LSH, q, TGSIZ_CHAR - nbits);

        p = vrget();
        if (i == 0)
            codek0(P_MOVE, p, base);
        else
            {
            code1(P_MOVE, p, i);
            codek0(P_ADJBP, p, base);
            }
        code0(P_DPB, q, p);
        vrfree(q);
        }
    base->Vrflags = savebase;
    reg->Vrflags = savereg;
    vrfree(base);
    return reg;
}

/* GPACKEDCOPY - Copy a packed aggregate by its exact C-byte extent.
** Whole-word SMOVE is wrong for a packed object whose Tbytes is not a
** multiple of four because it overwrites bytes belonging to the next array
** element.  Keep this deliberately simple and PDP-6 friendly: copy native
** 9-bit address units with LDB/DPB.
*/
/* GPACKEDBITBASE - Return a native 9-bit byte pointer to the object
** containing N and the exact bit offset of N within that byte stream.
** P=074,S=0 nested aggregates cannot themselves be represented as a PDP-10
** byte pointer, but their containing packed object can.
*/
static VREG *
gpackedbitbase(NODE *n, INT *bitp)
{
    if (n != NULL && (n->Nop == Q_MEMBER || n->Nop == Q_DOT)
      && packedbitagg(n->Nxoff))
        {
        *bitp = (unsigned INT)(-n->Nxoff) >> 12;
        return gaddress(n->Nleft);
        }
    *bitp = 0;
    return gaddress(n);
}

/* GPACKEDBITVALUE - Materialize a bit-offset packed aggregate in the normal
** one- or two-word aggregate value representation.  The packed object's
** first C byte occupies the high nine bits of the first word, exactly as it
** would in aligned memory.  This is sufficient for the normal KCC/GCC ABI
** argument and return paths without inventing a software pointer value.
*/
/* GPACKEDREGSCALAR - Extract an integral packed member from a one- or
** two-word aggregate value already held in registers.
*/
static VREG *
gpackedregscalar(NODE *n, VREG *agg)
{
    VREG *q, *r;
    INT bitoff;
    int bits, word, intra, first, second, shift;

    bitoff = packedoffbit(n->Nxoff);
    bits = tbitsize(n->Ntype);
    word = (int)(bitoff / TGSIZ_WORD);
    intra = (int)(bitoff % TGSIZ_WORD);
    if (bits <= 0 || bits > TGSIZ_WORD || word < 0 || word > 1)
        {
        int_error("gpackedregscalar: bad packed member %N", n);
        return agg;
        }

    q = vrget();
    q->Vrtype = uinttype;
    codek0(P_MOVE, q, word == 0 ? agg : VR2(agg));
    if (intra + bits <= TGSIZ_WORD)
        {
        shift = TGSIZ_WORD - intra - bits;
        if (shift) code8(P_LSH, q, -shift);
        if (bits < TGSIZ_WORD) code1(P_AND, q, (((INT)1 << bits) - 1));
        }
    else
        {
        first = TGSIZ_WORD - intra;
        second = bits - first;
        code1(P_AND, q, (((INT)1 << first) - 1));
        code8(P_LSH, q, second);
        r = vrget();
        r->Vrtype = uinttype;
        codek0(P_MOVE, r, VR2(agg));
        shift = TGSIZ_WORD - second;
        if (shift) code8(P_LSH, r, -shift);
        if (second < TGSIZ_WORD) code1(P_AND, r, (((INT)1 << second) - 1));
        code0(P_IOR, q, r);
        vrfree(r);
        }
    vrfree(agg);
    return gcastr(CAST_IT_IT, q, uinttype, n->Ntype, (NODE *)NULL);
}

static VREG *
gpackedbitvalue(NODE *n, TYPE *t)
{
    VREG *sa, *sp, *q, *r, *dw;
    INT sbit, bits, done, spos, sbyte, mask;
    int siz, sflags, sskip, take, dword, dbit, sshift, dshift;

    siz = sizetype(t);
    if (siz < 1 || siz > 2)
        {
        error("bit-offset GNU packed aggregate value larger than two words is not yet supported");
        return gaddress(n);
        }

    sa = gpackedbitbase(n, &sbit);
    sflags = sa->Vrflags;
    sa->Vrflags |= VRF_LOCK;
    if (siz == 2)
        {
        r = vrdget();
        code5(P_SETZ, r);
        code5(P_SETZ, VR2(r));
        }
    else
        {
        r = vrget();
        code5(P_SETZ, r);
        }
    r->Vrtype = t;
    if (siz == 2)
        VR2(r)->Vrtype = t;

    bits = (INT)t->Tbytes * TGSIZ_CHAR;
    done = 0;
    while (done < bits)
        {
        spos = sbit + done;
        sbyte = spos / TGSIZ_CHAR;
        sskip = (int)(spos % TGSIZ_CHAR);
        take = TGSIZ_CHAR - sskip;
        dbit = (int)(done % TGSIZ_WORD);
        if (take > TGSIZ_WORD - dbit)
            take = TGSIZ_WORD - dbit;
        if ((INT)take > bits - done)
            take = (int)(bits - done);

        sp = vrget();
        if (sbyte == 0)
            codek0(P_MOVE, sp, sa);
        else
            {
            code1(P_MOVE, sp, sbyte);
            codek0(P_ADJBP, sp, sa);
            }
        q = vrget();
        q->Vrtype = uinttype;
        code0(P_LDB, q, sp);
        sshift = TGSIZ_CHAR - sskip - take;
        if (sshift != 0)
            code8(P_LSH, q, -sshift);
        mask = ((INT)1 << take) - 1;
        code1(P_AND, q, mask);

        dword = (int)(done / TGSIZ_WORD);
        dshift = TGSIZ_WORD - dbit - take;
        if (dshift != 0)
            code8(P_LSH, q, dshift);
        dw = dword == 0 ? r : VR2(r);
        code0(P_IOR, dw, q);
        done += take;
        }

    sa->Vrflags = sflags;
    vrfree(sa);
    return r;
}

/* GPACKEDBITSTOREREG - Store a one- or two-word packed aggregate value into
** a nested aggregate beginning at an exact bit offset.  Destination C bytes
** may overlap unrelated outer fields, so every partial byte is updated with
** read/modify/write.
*/
static VREG *
gpackedbitstorereg(NODE *dst, VREG *src, TYPE *t)
{
    VREG *da, *dp, *dv, *q, *sw;
    INT dbit0, bits, done, dpos, dbyte, mask, keepmask;
    int siz, dflags, sflags, dskip, take, sword, sbit, sshift, dshift;

    siz = sizetype(t);
    if (siz < 1 || siz > 2)
        {
        error("bit-offset GNU packed aggregate value larger than two words is not yet supported");
        vrfree(src);
        return gaddress(dst);
        }

    da = gpackedbitbase(dst, &dbit0);
    dflags = da->Vrflags;
    sflags = src->Vrflags;
    da->Vrflags |= VRF_LOCK;
    src->Vrflags |= VRF_LOCK;

    bits = (INT)t->Tbytes * TGSIZ_CHAR;
    done = 0;
    while (done < bits)
        {
        dpos = dbit0 + done;
        dbyte = dpos / TGSIZ_CHAR;
        dskip = (int)(dpos % TGSIZ_CHAR);
        take = TGSIZ_CHAR - dskip;
        sbit = (int)(done % TGSIZ_WORD);
        if (take > TGSIZ_WORD - sbit)
            take = TGSIZ_WORD - sbit;
        if ((INT)take > bits - done)
            take = (int)(bits - done);

        sword = (int)(done / TGSIZ_WORD);
        sw = sword == 0 ? src : VR2(src);
        q = vrget();
        q->Vrtype = uinttype;
        codek0(P_MOVE, q, sw);
        sshift = TGSIZ_WORD - sbit - take;
        if (sshift != 0)
            code8(P_LSH, q, -sshift);
        mask = ((INT)1 << take) - 1;
        code1(P_AND, q, mask);

        dp = vrget();
        if (dbyte == 0)
            codek0(P_MOVE, dp, da);
        else
            {
            code1(P_MOVE, dp, dbyte);
            codek0(P_ADJBP, dp, da);
            }
        dv = vrget();
        dv->Vrtype = uinttype;
        code0(P_LDB, dv, dp);
        dshift = TGSIZ_CHAR - dskip - take;
        keepmask = 0777L ^ (mask << dshift);
        code1(P_AND, dv, keepmask);
        if (dshift != 0)
            code8(P_LSH, q, dshift);
        code0(P_IOR, dv, q);
        code0(P_DPB, dv, dp);
        vrfree(dv);
        done += take;
        }

    src->Vrflags = sflags;
    vrfree(src);
    da->Vrflags = dflags;
    da->Vrtype = t;
    return da;
}

/* GPACKEDBITCOPY - Copy one packed aggregate whose source or destination
** begins at an arbitrary bit offset.  Operate on the containing 9-bit byte
** stream and preserve unrelated bits in every destination address unit.
** This deliberately does not create a first-class software pointer value;
** it is an internal lowering for whole aggregate assignment.
*/
static VREG *
gpackedbitcopy(NODE *dst, NODE *src, TYPE *t)
{
    VREG *da, *sa, *dp, *sp, *sv, *dv;
    INT dbit, sbit, bits, done;
    int dflags, sflags;

    if (src == NULL)
        return gaddress(dst);
    if (!(src->Nflag & NF_LVALUE))
        {
        VREG *rv;

        rv = genexpr(src);
        if (rv == NULL)
            return gaddress(dst);
        if (sizetype(t) <= 2)
            return gpackedbitstorereg(dst, rv, t);
        error("bit-offset GNU packed aggregate expression larger than two words is not yet supported");
        vrfree(rv);
        return gaddress(dst);
        }

    bits = (INT)t->Tbytes * TGSIZ_CHAR;
    da = gpackedbitbase(dst, &dbit);
    sa = gpackedbitbase(src, &sbit);
    dflags = da->Vrflags;
    sflags = sa->Vrflags;
    da->Vrflags |= VRF_LOCK;
    sa->Vrflags |= VRF_LOCK;

    done = 0;
    while (done < bits)
        {
        INT spos, dpos, sbyte, dbyte, mask, keepmask;
        int sskip, dskip, take, sshift, dshift;

        spos = sbit + done;
        dpos = dbit + done;
        sbyte = spos / TGSIZ_CHAR;
        dbyte = dpos / TGSIZ_CHAR;
        sskip = (int)(spos % TGSIZ_CHAR);
        dskip = (int)(dpos % TGSIZ_CHAR);
        take = TGSIZ_CHAR - sskip;
        if (take > TGSIZ_CHAR - dskip)
            take = TGSIZ_CHAR - dskip;
        if ((INT)take > bits - done)
            take = (int)(bits - done);

        sp = vrget();
        if (sbyte == 0)
            codek0(P_MOVE, sp, sa);
        else
            {
            code1(P_MOVE, sp, sbyte);
            codek0(P_ADJBP, sp, sa);
            }
        sv = vrget();
        sv->Vrtype = uinttype;
        code0(P_LDB, sv, sp);
        sshift = TGSIZ_CHAR - sskip - take;
        if (sshift != 0)
            code8(P_LSH, sv, -sshift);
        mask = ((INT)1 << take) - 1;
        code1(P_AND, sv, mask);

        dp = vrget();
        if (dbyte == 0)
            codek0(P_MOVE, dp, da);
        else
            {
            code1(P_MOVE, dp, dbyte);
            codek0(P_ADJBP, dp, da);
            }
        dv = vrget();
        dv->Vrtype = uinttype;
        code0(P_LDB, dv, dp);
        dshift = TGSIZ_CHAR - dskip - take;
        keepmask = 0777L ^ (mask << dshift);
        code1(P_AND, dv, keepmask);
        if (dshift != 0)
            code8(P_LSH, sv, dshift);
        code0(P_IOR, dv, sv);
        code0(P_DPB, dv, dp);
        vrfree(dv);
        done += take;
        }

    sa->Vrflags = sflags;
    vrfree(sa);
    da->Vrflags = dflags;
    da->Vrtype = t;
    return da;
}

/* GPACKEDVALUE - Materialize an aligned packed aggregate into the normal
** one- or two-word aggregate value representation without reading bytes
** beyond the object.  Packed bytes occupy successive 9-bit fields from the
** high end of each word.
*/
static VREG *
gpackedvalue(NODE *src, TYPE *t)
{
    VREG *sa, *sp, *q, *r, *dw;
    int i, bytes, word, pos, sflags;

    bytes = t->Tbytes;
    r = (sizetype(t) == 2) ? vrdget() : vrget();
    r->Vrtype = t;
    if (sizetype(t) == 2)
        VR2(r)->Vrtype = t;
    code5(P_SETZ, r);
    if (sizetype(t) == 2)
        code5(P_SETZ, VR2(r));

    sa = gaddress(src);
    sflags = sa->Vrflags;
    sa->Vrflags |= VRF_LOCK;
    for (i = 0; i < bytes; ++i)
        {
        sp = vrget();
        if (i == 0) codek0(P_MOVE, sp, sa);
        else { code1(P_MOVE, sp, i); codek0(P_ADJBP, sp, sa); }
        q = vrget();
        q->Vrtype = uinttype;
        code0(P_LDB, q, sp);
        vrfree(sp);

        word = i / 4;
        pos = i % 4;
        if (pos != 3)
            code8(P_LSH, q, (3 - pos) * TGSIZ_CHAR);
        dw = (word == 0) ? r : VR2(r);
        code0(P_IOR, dw, q);
        vrfree(q);
        }
    sa->Vrflags = sflags;
    vrfree(sa);
    return r;
}

static VREG *
gpackedcopy(NODE *dst, NODE *src, TYPE *t)
{
    VREG *da, *sa, *dp, *sp, *q;
    int i, bytes, dflags, sflags;

    if (((dst->Nop == Q_MEMBER || dst->Nop == Q_DOT)
         && packedbitagg(dst->Nxoff))
      || ((src->Nop == Q_MEMBER || src->Nop == Q_DOT)
         && packedbitagg(src->Nxoff)))
        return gpackedbitcopy(dst, src, t);

    if (!(src->Nflag & NF_LVALUE))
        {
        VREG *rv;
        rv = genexpr(src);
        if (rv == NULL)
            return gaddress(dst);
        if (sizetype(t) <= 2)
            return gpackedcopyreg(dst, rv, t);
        /* Larger aggregate expressions are represented by an address. */
        bytes = t->Tbytes;
        da = gaddress(dst);
        sa = rv;
        /* 3/4-word call results and hidden-result temporaries are returned
        ** as ordinary word addresses.  Packed copying operates in C address
        ** units, so convert that temporary address to a native 9-bit byte
        ** pointer before walking its exact byte extent.
        */
        pitopc(sa, TGSIZ_CHAR, 0, 1);
        dflags = da->Vrflags;
        sflags = sa->Vrflags;
        da->Vrflags |= VRF_LOCK;
        sa->Vrflags |= VRF_LOCK;
        for (i = 0; i < bytes; ++i)
            {
            sp = vrget();
            if (i == 0) codek0(P_MOVE, sp, sa);
            else { code1(P_MOVE, sp, i); codek0(P_ADJBP, sp, sa); }
            q = vrget();
            code0(P_LDB, q, sp);
            dp = vrget();
            if (i == 0) codek0(P_MOVE, dp, da);
            else { code1(P_MOVE, dp, i); codek0(P_ADJBP, dp, da); }
            code0(P_DPB, q, dp);
            vrfree(q);
            }
        sa->Vrflags = sflags;
        vrfree(sa);
        da->Vrflags = dflags;
        da->Vrtype = t;
        return da;
        }
    bytes = t->Tbytes;
    da = gaddress(dst);
    sa = gaddress(src);
    dflags = da->Vrflags;
    sflags = sa->Vrflags;
    da->Vrflags |= VRF_LOCK;
    sa->Vrflags |= VRF_LOCK;

    for (i = 0; i < bytes; ++i)
        {
        /* Build both byte pointers before loading the value.  P_ADJBP can
        ** become a helper call on early machines and therefore must not
        ** have a live byte value across it. */
        sp = vrget();
        if (i == 0)
            codek0(P_MOVE, sp, sa);
        else
            {
            code1(P_MOVE, sp, i);
            codek0(P_ADJBP, sp, sa);
            }
        dp = vrget();
        if (i == 0)
            codek0(P_MOVE, dp, da);
        else
            {
            code1(P_MOVE, dp, i);
            codek0(P_ADJBP, dp, da);
            }
        q = vrget();
        code0(P_LDB, q, sp);
        code0(P_DPB, q, dp);
        vrfree(q);
        vrfree(dp);
        vrfree(sp);
        }

    sa->Vrflags = sflags;
    vrfree(sa);
    da->Vrflags = dflags;
    da->Vrtype = t;
    return da;
}


/* GPACKEDCOPYREG - Materialize a packed aggregate value held in one or two
** return/value registers into an exact-byte destination.  Packed aggregate
** words use the normal PDP-10 memory image: byte zero occupies the high
** nine bits of the first word.
*/
static VREG *
gpackedcopyreg(NODE *dst, VREG *src, TYPE *t)
{
    VREG *da, *dp, *q, *sw;
    int i, bytes, word, pos, saveflags, savesrc;

    bytes = t->Tbytes;
    da = gaddress(dst);
    saveflags = da->Vrflags;
    savesrc = src->Vrflags;
    da->Vrflags |= VRF_LOCK;
    src->Vrflags |= VRF_LOCK;

    for (i = 0; i < bytes; ++i)
        {
        word = i / 4;
        pos = i % 4;
        sw = (word == 0) ? src : VR2(src);
        q = vrget();
        codek0(P_MOVE, q, sw);
        if (pos != 3)
            code8(P_LSH, q, -((3 - pos) * TGSIZ_CHAR));
        code1(P_AND, q, 0777);

        dp = vrget();
        if (i == 0)
            codek0(P_MOVE, dp, da);
        else
            {
            code1(P_MOVE, dp, i);
            codek0(P_ADJBP, dp, da);
            }
        code0(P_DPB, q, dp);
        vrfree(q);
        }

    src->Vrflags = savesrc;
    da->Vrflags = saveflags;
    vrfree(da);
    src->Vrtype = t;
    if (sizetype(t) == 2)
        VR2(src)->Vrtype = t;
    return src;
}

static VREG *
gprimary(NODE *n)
{
    VREG *q, *r;
    INT siz;
    int volat, t;

    switch (n->Nop)
	{

	case N_COMPLIT:	/* C99 compound literal */
	    if (n->Nleft != NULL && n->Nleft->Nop == N_DATA)
		genadata(n->Nleft);
	    return gprimary(n->Nright);

	case N_STMTEXPR:	/* GNU statement expression */
	    genstmt(n->Nleft);
	    return genexpr(n->Nright);

	case Q_IDENT:		/* Variable name */
	    if ((t = n->Nid->Stype->Tspec) == TS_FUNCT || t == TS_ARRAY )
		{
	   /* Check for funct/array. Make sure Ntype is ptr */
		if (n->Ntype->Tspec != TS_PTR)
/* Later make this error again */
		    int_warn("gprimary: array/funct %N", n);
		return gaddress(n);	/* Yup, just return ptr to object */
		}
	/* Normal variable or structure/union */
	    if ((volat = tisanyvolat(n->Ntype)) != 0)
		flushcode();		/* If volatile, avoid optimization */

	    if (Register_Id(n)) {
        /* A register variable is a value, not a disposable temporary.
        ** For a return expression, put the copy directly in AC1 so the
        ** caller does not need a second MOVE in greturn().  Otherwise use
        ** a normal temporary.
        */
        if (tisdimode(n->Ntype)) {
            if ((n->Nid->Sflags & (SF_ABIREG|SF_ABICONSUME))
              == (SF_ABIREG|SF_ABICONSUME)) {
                r = vrdgetreg(n->Nid->Sreg);
                /* This read-once incoming pair is now an ordinary temporary.
                ** Release its ABI reservation so later pair allocation can
                ** reuse the dead parameter ACs after this value is freed.
                */
                fnargkeepmask &= ~((1 << n->Nid->Sreg)
                    | (1 << (n->Nid->Sreg + 1)));
            } else {
                r = vrdget();
                code00(P_DMOVE, r->Vrloc, n->Nid->Sreg);
            }
            r->Vrtype = n->Ntype;
            VR2(r)->Vrtype = n->Ntype;
        } else {
            r = vrget();
            r->Vrtype = n->Ntype;
            code00(P_MOVE, r->Vrloc, n->Nid->Sreg);
        }
        }
	    else
		r = getmem(gaddress(n), n->Ntype, tisbyte(n->Ntype), 0);

	    if (volat)
		flushcode();
	    return r;

	case N_SCONST:		/* Literal string - get char pointer to it */
	    n->Nsclab = newlabel();
	    n->Nscnext = litstrings;	/* link on string stack */
	    litstrings = n;			/* include this one */
	    r = vrget();
	    r->Vrtype = n->Ntype;		/* Set C type of object in reg */
	/* Get byte ptr to str, using given bytesize of type! */
	    code10(P_MOVE, r, n->Nsclab, elembsize(n->Ntype), 0);
	    return r;

	case N_ACONST:		/* GNU label address (&&label) */
	    r = vrget();
	    r->Vrtype = n->Ntype;
	    code3(P_MOVE, r, n->Nxfsym);
	    return r;

	case N_VCONST:		/* Void "constant" */
	    return NULL;		/* No register used! */
	case N_ICONST:		/* Integer constant */
	case N_PCONST:		/* Pointer constant uses same cell etc */
	    if (tisdimode(n->Ntype))
		{
		INT hi, lo;

		r = vrdget();
		r->Vrtype = n->Ntype;
		dimode_iconst_words(n, &hi, &lo);
		lo = dimode_lo_expand(hi, lo);
		flushcode();
		code1(P_MOVE, r, hi);
		code1(P_MOVE, VR2(r), lo);
		flushcode();
		return r;
		}
	    r = vrget();
	    r->Vrtype = n->Ntype;	/* Set C type of object in reg */
	    code1(P_MOVE, r, n->Niconst);
	    return r;

	case N_FCONST:		/* Floating-point constant */
	    switch (n->Ntype->Tspec)
		{
		case TS_FLOAT:
		    r = vrget();
		    r->Vrtype = n->Ntype;	/* Set C type of object in reg */
		    code9(P_MOVE, r, n->Nfconst, 0);
		    break;
		case TS_DOUBLE:
		case TS_LNGDBL:
		    r = vrdget();
		    r->Vrtype = n->Ntype;	/* Set C type of object in reg */
		    code9(P_DMOVE, r, n->Nfconst, 1);
		    break;
		}
	    return r;

	case Q_ASM:
	    gasm(n);
	    return NULL;		/* Currently never returns anything */

	case T_JFFO:
	    gjffo(n);
	    return NULL;

	case N_FNCALL:		/* Function call */
	    return gcall(n);

	case Q_DOT:			/* (). direct component selection */
	    if (!(n->Nleft->Nflag & NF_LVALUE))
		break;		/* Ugh, do hairy stuff if not lvalue! */


	    if ((debcsi == KCC_DBG_NULL) && (n->Nleft->Nop == Q_MEMBER))
		{
		_chnl = n->Nleft->sfline;
		if (n->Nleft->Nleft->Nop == N_CAST)
		    code4 (P_NULPTR, (VREG *) NULL, gaddress (n->Nleft->Nleft->Nleft));
		else
		    code4 (P_NULPTR, (VREG *) NULL, gaddress (n->Nleft->Nleft));
		}

	/* OK, fall thru to handle like Q_MEMBER */

	/* FALLTHROUGH */
	case Q_MEMBER:		/* ()-> indirect component selection */
	    if ((volat = tisanyvolat(n->Ntype)) != 0)
		flushcode();		/* Ugh, avoid optimiz of volatile */

#if 0 /* KAR-1/92, leaving this as #if 0 in case I need it later */
	    if ((debcsi == KCC_DBG_NULL) && (n->Nop == Q_MEMBER))
		{
		_chnl = n->sfline;
		code4 (P_NULPTR, (VREG *) NULL, gaddress (n->Nleft));
		}
#endif

            if ((n->Ntype->Tspec == TS_STRUCT || n->Ntype->Tspec == TS_UNION)
              && packedbitagg(n->Nxoff))
                {
                r = gpackedbitvalue(n, n->Ntype);
                if (volat)
                    flushcode();
                return r;
                }

            if (tisinteg(n->Ntype) && (packedcross(n->Nxoff) || packedbit(n->Nxoff) || packedbitscalar(n->Nxoff) || crossbit(n->Nxoff)))
                {
                r = gpackedload(n);
                if (volat)
                    flushcode();
                return r;
                }

	    if (Register_Id(n))	/* approximate getmem() for registers */
#if 0
		r = rgetmem(gaddress(n), n->Ntype,
			    (n->Nxoff < 0) || tisbyte(n->Ntype), 0);
#else
		r = rgetmem(gaddress(n), n->Ntype, 0);
#endif
	    else
		r = getmem(gaddress(n), n->Ntype,
			    (n->Nxoff < 0) || tisbyte(n->Ntype), 0);
	    if (volat)
		flushcode();
	    return r;

	case Q_MUUO:
	    return gmuuo(n);

	default:
	    int_error("gprimary: bad op %N", n);
	    return NULL;
	}

    /* Hairy stuff for Q_DOT of something that isn't an lvalue.
    ** This can only happen for a struct returned from a function call.
    ** The structure resulting from the expression will either be
    ** completely contained in the registers (if size <= 2) or the register
    ** will contain the structure address.
    */
    if ((siz = sizetype(n->Nleft->Ntype)) > 2)	/* Find # wds in it */
	/* Fake out gaddress into using genexpr instead of another gaddress
	** when evaluating the structure expression, since result will
	** be a pointer.
	*/
	{
	n->Nop = Q_MEMBER;

	if (Register_Id(n))	/* approximate getmem() for registers */
#if 0
	    return rgetmem(gaddress(n), n->Ntype,
			(n->Nxoff < 0) || tisbyte(n->Ntype), 0);
#else
	    return rgetmem(gaddress(n), n->Ntype, 0);
#endif
	else
	    return getmem(gaddress(n), n->Ntype,
			(n->Nxoff < 0) || tisbyte(n->Ntype), 0);
	}

    /* Pull component out of structure in 1- or 2-word register */
    r = genexpr(n->Nleft);	/* Get the structure */
    if (tisinteg(n->Ntype)
      && (packedcross(n->Nxoff) || packedbit(n->Nxoff)
       || packedbitscalar(n->Nxoff)))
        return gpackedregscalar(n, r);
    switch (n->Nxoff)		/* See which part of it we want */
	{
	case 0:			/* Want first word? */
	    if (siz == 2 && sizetype(n->Ntype) == 1)
		vrnarrow(r);	/* Keep 1st word of a 2-word value */
	    return r;
	case 1:			/* Want second word? */
	    vrnarrow(r = VR2(r)); /* Keep second word of a 2-word value */
	    return r;

	default:			/* Bitfield of some kind */
	/* NOTE: This generates a very uncommon use of PTA_BYTEPOINT
	** wherein the E field of the byte pointer is actually a register
	** address.  This is why vrreal() is called, to get the
	** actual register number.  As long as this is one of the return-value
	** registers as it should be, this usage is probably safe from
	** the peephole optimizer.
	*/
	    q = vrget();		/* Get another register */
	    q->Vrtype = n->Ntype;	/* Set C type of object in reg */
	    (void) vrstoreal(q, r);	/* Make sure both regs are active! */
	    codebp(P_LDB, vrreal(q), (unsigned)((- (n->Nxoff)) & 07777) << 6,
		   0, NULL, vrreal(r) + ((unsigned)(-(n->Nxoff)) >> 12));
	    vrfree(r);		/* don't need rest of struct */
	    return q;
	}
}

/* GCALL - Generate function call
*/

/* GCCABI_COLLECT_ARGS - Collect call arguments in source order. */
static void
gccabi_collect_args(NODE *n, NODE **args, int *nargs)
{
    if (n == NULL)
        return;
    if (n->Nop == N_EXPRLIST) {
        gccabi_collect_args(n->Nleft, args, nargs);
        if (*nargs < 64)
            args[(*nargs)++] = n->Nright;
        return;
    }
    if (*nargs < 64)
        args[(*nargs)++] = n;
}

/* GCCABI_LOAD_ARGS - Expose the GCC PDP-10 argument ABI at the call edge.
**
** KCC still evaluates arguments into a private temporary stack block.
** Load GCC ABI register words from that block, then compact the stack-only
** words downward so the register-word shadows can be removed before PUSHJ.
** The callee therefore sees exactly the canonical GCC stack shape.
**
** SLOTBASE is 1 when AC1 carries the hidden return pointer for aggregates
** larger than four words, otherwise 0.
**
** Return the number of register-word shadows to remove from the temporary
** argument block before making the call.
*/
static int
gccabi_load_args(NODE *list, int slotbase, TYPE *proto)
{
    NODE *args[64];
    unsigned char regword[256];
    int nargs, namedargs, total, cum, i, j, siz, slot, oldoff, oldidx;
    int dst, nreg, target, tmpreg;
    TYPE *t, *p;
    VREG *tmpvr;

    nargs = 0;
    gccabi_collect_args(list, args, &nargs);

    /* GCC passes only named arguments in AC1..AC4.  Arguments after an
    ** ellipsis are stack-only even while register argument slots remain.
    */
    namedargs = nargs;
    if (proto) {
        int n;
        n = 0;
        p = proto;
        while (p && p->Tspec == TS_PARAM) {
            ++n;
            p = p->Tproto;
        }
        if (p && p->Tspec == TS_PARINF)
            namedargs = n;
    }

    total = sizeargs(list);
    if (total > 256)
        int_error("gccabi_load_args: too many argument words");
    memset(regword, 0, sizeof(regword));
    nreg = 0;

    cum = 0;
    for (i = 0; i < nargs; ++i) {
        t = args[i]->Ntype;
        siz = sizetype(t);
        if (i < namedargs &&
            !((t->Tspec == TS_STRUCT || t->Tspec == TS_UNION) && siz > 2)) {
            if (siz == 2 && slotbase + cum == GCCABI_ARG_REGS - 1) {
                /* Match GCC's partial two-word argument ordering: word 1
                ** uses AC4 while word 0 remains in the outgoing stack area.
                */
                j = 1;
                oldoff = -(cum + siz - 1 - j);
                oldidx = -oldoff;
                codemdx(P_MOVE, 4, (SYMBOL *)NULL, oldoff, R_SP);
                if (oldidx >= 0 && oldidx < 256 && !regword[oldidx]) {
                    regword[oldidx] = 1;
                    ++nreg;
                }
            } else {
                for (j = 0; j < siz; ++j) {
                    slot = slotbase + cum + j;
                    if (slot >= GCCABI_ARG_REGS)
                        continue;
                    oldoff = -(cum + siz - 1 - j);
                    oldidx = -oldoff;
                    codemdx(P_MOVE, slot + 1, (SYMBOL *)NULL, oldoff, R_SP);
                    if (oldidx >= 0 && oldidx < 256 && !regword[oldidx]) {
                        regword[oldidx] = 1;
                        ++nreg;
                    }
                }
            }
        }
        cum += siz;
    }

    /* The stack pointer will move down by NREG before PUSHJ.  Move each
    ** stack-passed word NREG positions downward first.  Work bottom-up so
    ** overlapping moves cannot overwrite a source that is still needed.
    */
    tmpvr = NULL;
    tmpreg = 0;
    dst = total - nreg - 1;
    for (oldidx = total - 1; oldidx >= 0; --oldidx) {
        if (regword[oldidx])
            continue;
        target = nreg + dst;
        if (oldidx != target) {
            if (tmpvr == NULL) {
                tmpvr = vrget();
                tmpreg = vrtoreal(tmpvr);
            }
            codemdx(P_MOVE, tmpreg, (SYMBOL *)NULL, -oldidx, R_SP);
            flushcode();
            codemdx(P_MOVEM, tmpreg, (SYMBOL *)NULL, -target, R_SP);
            flushcode();
        }
        --dst;
    }
    if (tmpvr != NULL)
        vrfree(tmpvr);
    return nreg;
}

/* GCCABI_DIRECT_REG_ARGS - Generate simple C arguments without shadows.
**
** The compatibility call path first pushes every argument and then reloads
** AC1..AC4.  When every argument is one word, evaluate only genuinely
** stack-passed arguments onto the stack and retain register-passed values in
** virtual registers for a parallel copy into the ABI ACs.
**
** Arguments are evaluated right-to-left, matching KCC's historical gfnarg()
** traversal.  AC16 breaks register-copy cycles.  SLOTBASE reserves AC1 for a
** hidden aggregate-result pointer when needed.  Return nonzero if the direct
** path was used.
*/
static int
gccabi_direct_reg_args(NODE *list, int slotbase, TYPE *proto, int defermem)
{
    NODE *args[64];
    VREG *vals[GCCABI_ARG_REGS];
    TYPE *p;
    INT cval[GCCABI_ARG_REGS];
    int src[GCCABI_ARG_REGS];
    unsigned char isconst[GCCABI_ARG_REGS];
    unsigned char ismem[GCCABI_ARG_REGS];
    int dst[GCCABI_ARG_REGS];
    int pending[GCCABI_ARG_REGS];
    SYMBOL *memsym[GCCABI_ARG_REGS];
    int nargs, namedargs, nreg, i, j, k, progress, pick;

    nargs = 0;
    gccabi_collect_args(list, args, &nargs);

    namedargs = nargs;
    if (proto) {
        int n;
        n = 0;
        p = proto;
        while (p && p->Tspec == TS_PARAM) {
            ++n;
            p = p->Tproto;
        }
        if (p && p->Tspec == TS_PARINF)
            namedargs = n;
    }

    for (i = 0; i < nargs; ++i)
        if (sizetype(args[i]->Ntype) != 1)
            return 0;

    nreg = namedargs;
    if (nreg > GCCABI_ARG_REGS - slotbase)
        nreg = GCCABI_ARG_REGS - slotbase;
    if (nreg < 0)
        nreg = 0;

    /* ABI argument ACs are fixed physical registers.  Do not let CSE or
    ** register-retargeting reach backward across an earlier call/branch and
    ** decide that a required AC load is redundant.  This is a call-edge
    ** barrier, not a general optimizer fence.
    */
    if (!defermem)
        flushcode();

    for (i = 0; i < GCCABI_ARG_REGS; ++i) {
	isconst[i] = 0;
	ismem[i] = 0;
    }

    k = nreg;
    for (i = nargs - 1; i >= 0; --i) {
        if (i < nreg) {
            --k;
            if (Register_Id(args[i])) {
                vals[k] = NULL;
                src[k] = args[i]->Nid->Sreg;
            } else if (args[i]->Nop == N_ICONST || args[i]->Nop == N_PCONST) {
                vals[k] = NULL;
                isconst[k] = 1;
                cval[k] = args[i]->Niconst;
                src[k] = 0;
            } else if (defermem && args[i]->Nop == Q_IDENT
                       && !tisanyvolat(args[i]->Ntype)
                       && args[i]->Nid->Stype->Tspec != TS_ARRAY
                       && args[i]->Nid->Stype->Tspec != TS_FUNCT
                       && (args[i]->Nid->Sclass == SC_ISTATIC
                           || args[i]->Nid->Sclass == SC_XEXTREF
                           || args[i]->Nid->Sclass == SC_EXLINK
                           || args[i]->Nid->Sclass == SC_EXTDEF
                           || args[i]->Nid->Sclass == SC_EXTREF
                           || args[i]->Nid->Sclass == SC_INTDEF
                           || args[i]->Nid->Sclass == SC_INTREF
                           || args[i]->Nid->Sclass == SC_INLINK)) {
                vals[k] = NULL;
                ismem[k] = 1;
                memsym[k] = args[i]->Nid;
                if (memsym[k]->Sclass == SC_ISTATIC)
                    memsym[k] = memsym[k]->Ssym;
                src[k] = 0;
            } else
                vals[k] = genexpr(args[i]);
        } else
            gfnarg(args[i]);
    }

    /* A nested call while evaluating a later register argument may spill an
    ** earlier argument value.  Reload only the argument values below.
    ** Spills that were already live on entry belong to the surrounding
    ** expression and must remain on the stack across this call.
    */
    for (i = 0; i < nreg; ++i) {
        if (vals[i])
            src[i] = vrtoreal(vals[i]);
        dst[i] = slotbase + i + 1;
        pending[i] = !isconst[i] && !ismem[i] && (src[i] != dst[i]);
    }

    for (;;) {
        progress = 0;
        for (i = 0; i < nreg; ++i) {
            if (!pending[i])
                continue;
            for (j = 0; j < nreg; ++j)
                if (pending[j] && src[j] == dst[i])
                    break;
            if (j != nreg)
                continue;
            code00(P_MOVE, dst[i], src[i]);
            if (!defermem)
                flushcode();
            pending[i] = 0;
            progress = 1;
        }
        if (progress)
            continue;

        pick = -1;
        for (i = 0; i < nreg; ++i)
            if (pending[i]) {
                pick = i;
                break;
            }
        if (pick < 0)
            break;

        /* No acyclic copy remains, so the pending graph consists only of
        ** cycles among the fixed destination ACs.  AC6 is outside AC1..AC4
        ** and all non-cycle sources have already been consumed; use it as a
        ** fixed cycle breaker instead of allocating a VREG that could land
        ** in one of the destinations and destroy a still-live argument.
        */
        code00(P_MOVE, R_ABITMP, src[pick]);
        if (!defermem)
            flushcode();
        src[pick] = R_ABITMP;
    }

    for (i = 0; i < nreg; ++i) {
	if (isconst[i])
	    codr1(P_MOVE, dst[i], cval[i]);
	else if (ismem[i])
	    codemdx(P_MOVE, dst[i], memsym[i], 0, 0);
        if (!defermem && (isconst[i] || ismem[i]))
            flushcode();
    }

    for (i = 0; i < nreg; ++i)
        if (vals[i])
            vrfree(vals[i]);
    return 1;
}

/* GCCABI_DIRECT_TAIL_OK - Check for a register-only direct tail call. */
static int
gccabi_direct_tail_ok(NODE *n)
{
    NODE *args[64];
    TYPE *p;
    SYMBOL *fn;
    int nargs, i;

    if (!(n->Nflag & NF_RETEXPR) || n->Nleft->Nop != Q_IDENT)
        return 0;
    fn = n->Nleft->Nid;
    if (fn->Sflags & (TF_BLISS | TF_FORTRAN | TF_INTERRUPT))
        return 0;
    if (curfn->Sflags & TF_INTERRUPT)
        return 0;
    if (!cmptype(curfn->Stype->Tsubt, n->Ntype))
        return 0;
    if (sizetype(n->Ntype) > GCCABI_RET_REGS)
        return 0;

    p = n->Nleft->Ntype->Tproto;
    while (p && p->Tspec == TS_PARAM)
        p = p->Tproto;
    if (p && p->Tspec == TS_PARINF)
        return 0;

    nargs = 0;
    gccabi_collect_args(n->Nright, args, &nargs);
    if (nargs > GCCABI_ARG_REGS)
        return 0;
    for (i = 0; i < nargs; ++i)
        if (sizetype(args[i]->Ntype) != 1)
            return 0;
    return 1;
}

/* GCCABI_DIRECT_TAIL_REGS - Restore direct ABI parameters for a tail call.
**
** Tail-call eligibility has already proved that the arguments are this
** function's parameters in their original order.  A direct-ABI non-leaf
** keeps all of those one-word parameters in preserved registers, so copy
** them back to AC1..AC4 before restoring the preserved registers.
*/
static void
gccabi_direct_tail_regs(NODE *list)
{
    NODE *args[64];
    int nargs, i;

    nargs = 0;
    gccabi_collect_args(list, args, &nargs);
    for (i = 0; i < nargs && i < GCCABI_ARG_REGS; ++i) {
        if (args[i]->Nop != Q_IDENT || args[i]->Nid->Sclass != SC_RARG) {
            int_error("gccabi_direct_tail_regs: bad argument");
            return;
        }
        code00(P_MOVE, i + 1, args[i]->Nid->Sreg);
    }
}

/* GCCABI_TAIL_REGS - Reload private-image ABI argument registers.
**
** A compatibility-mode KCC callee keeps a private full-argument stack image.
** Before a direct tail transfer to a function with the same C type, reload
** AC1..AC4 from that image.  Stack-only arguments are already in the
** caller-provided external ABI area below the private shim.
*/
static void
gccabi_tail_regs(void)
{
    TYPE *p, *t;
    int cum, siz, slot, oldidx, i;

    cum = 0;
    if (sizetype(curfn->Stype->Tsubt) > GCCABI_RET_REGS) {
        codemdx(P_MOVE, 1, (SYMBOL *)NULL, -(stackoffset + 1), R_SP);
        flushcode();
        cum = 1;
    }

    p = curfn->Stype->Tproto ? curfn->Stype->Tproto : curfn->Shproto;
    while (p && p->Tspec == TS_PARAM) {
        t = p->Tsubt;
        siz = sizetype(t);
        if (!((t->Tspec == TS_STRUCT || t->Tspec == TS_UNION) && siz > 2)) {
            if (siz == 2 && cum == GCCABI_ARG_REGS - 1) {
                oldidx = cum;
                codemdx(P_MOVE, 4, (SYMBOL *)NULL,
                        -(stackoffset + 1 + oldidx), R_SP);
                flushcode();
            } else {
                for (i = 0; i < siz; ++i) {
                    slot = cum + i;
                    if (slot >= GCCABI_ARG_REGS)
                        break;
                    oldidx = cum + siz - 1 - i;
                    codemdx(P_MOVE, slot + 1, (SYMBOL *)NULL,
                            -(stackoffset + 1 + oldidx), R_SP);
                    flushcode();
                }
            }
        }
        if (siz == 2 && tisdimode(t) && !tisunsign(t)) {
            int lowreg;
            lowreg = 0;
            if (cum < GCCABI_ARG_REGS - 1)
                lowreg = cum + 2;
            else if (cum == GCCABI_ARG_REGS - 1)
                lowreg = GCCABI_ARG_REGS;
            if (lowreg) {
                gccabi_dimode_normalize_reg(lowreg);
                if (cum < GCCABI_ARG_REGS - 1) {
                    gccabi_dimode_encode_regs(cum + 1, lowreg);
                } else {
                    /* Split pair: the high word remains in the external
                    ** stack area, so reload it temporarily to test sign. */
                    codemdx(P_MOVE, R_ABITMP, (SYMBOL *)NULL,
                            -(stackoffset + 1 + cum + 1), R_SP);
                    gccabi_dimode_encode_regs(R_ABITMP, lowreg);
                }
                flushcode();
            }
        }
        cum += siz;
        p = p->Tproto;
    }
}

static
VREG*
gcall (NODE* n)
    {
    NODE*	l;
    INT		narg,
		siz;
    VREG*	r;
    VREG*	calladdr;
    SYMBOL*	arg;
    long	fnflags;
    int	abiregwords;
    int directtail;

    calladdr = NULL;

    if (n->Nleft->Ntype->Tspec != TS_FUNCT)
	int_error ("gcall: non-function %N", n);

    /* Check to see if OK to try for tail recursion */

    if (!optgen				/* Not optimizing? */
	|| stkgoto			/* Function contains a setjmp call? */
	|| stackrefs)			/* Fn makes addr refs to stack? */
	n->Nflag &=~ NF_RETEXPR;	/* If any of the above, forget it. */

    directtail = gccabi_direct_tail_ok(n);

    /* Check for args in same order - if ok, can tail recurse */

    l = n->Nright;
    siz = sizetype(n->Ntype);		/* calculate size of return value */

    if (n->Ntype->Tspec == TS_ARRAY)	/* Someday flush this, I hope */
	{
	int_error ("gcall: array type %N", n);
	siz = 0;
	}

    narg = directtail ? 0 : -1;

    while (!directtail && (n->Nflag & NF_RETEXPR) && l != NULL)
	{
	if (l->Nop == N_EXPRLIST)
	    {
	    arg = (l->Nright->Nop == Q_IDENT? l->Nright->Nid : NULL);
	    l = l->Nleft;
	    }
	else
	    {
	    arg = (l->Nop == Q_IDENT? l->Nid : NULL);
	    l = NULL;
	    }

	if (arg == NULL || (arg->Sclass != SC_ARG && arg->Sclass != SC_RARG))
	    n->Nflag &=~ NF_RETEXPR;
	else
	    {
	    if (narg == -1)
		narg = arg->Svalue;
	    else if (narg != arg->Svalue)
		n->Nflag &=~ NF_RETEXPR;

	    narg -= sizetype(arg->Stype);

	    if (narg < 0)
		n->Nflag &=~ NF_RETEXPR;
	    }
	}

    if (siz > GCCABI_RET_REGS)
	narg -= 1;		/* account for retval (struct *) */

    if (n->Nright == NULL)
	narg = 0;	/* no args always matches */

    fnflags = (n->Nleft->Nop == Q_IDENT) ? n->Nleft->Nid->Sflags : 0;

    /* A tail transfer can reuse the caller's external argument area only
    ** when source and destination have the same C ABI shape.  Restrict the
    ** optimization to direct normal-C calls with a compatible function
    ** type.  BLISS, FORTRAN, interrupt, and indirect linkages keep the
    ** ordinary call path.
    */
    if (!directtail && (n->Nleft->Nop != Q_IDENT
      || (fnflags & (TF_BLISS | TF_FORTRAN | TF_INTERRUPT))
      || (curfn->Sflags & TF_INTERRUPT)
      || !cmptype(curfn->Stype, n->Nleft->Ntype)))
	n->Nflag &= ~NF_RETEXPR;

    /*
     * If we still think we can tail recurse, do it.
     *
     * NOTE: profiling precludes tail recursion: MVS 09/20/89
     */

    if (!profbliss && !fnvla_v11)	/* VLA frame must be unwound normally */
	{
	if ((n->Nflag & NF_RETEXPR) && (directtail || narg == 0))
	    {
	    int j;

            if (directtail) {
                if (!gccabi_direct_reg_args(n->Nright, 0,
                        n->Nleft->Ntype->Tproto, 1))
                    int_error("gcall: direct tail argument generation failed");
                flushcode();
            }

	    /* Restore the external GCC ABI state.  Reload register arguments
	    ** from KCC's private parameter image, restore all call-preserved
	    ** registers, discard locals and private argument copies, retain the
	    ** original return PC, and jump directly to the destination.
	    */
	    if (!directtail) {
	        if (fnabidirect)
		    gccabi_direct_tail_regs(n->Nright);
	        else
		    gccabi_tail_regs();
            }
	    if (R_PRESERVE_COUNT >= _reg_count)
		for (j = 0; j < _reg_count; ++j) {
		    codemdx(P_MOVE, j + r_maxnopreserve + 1, (SYMBOL *)NULL,
			    1 + fnsavescr + (fnvla_v11 ? 1 : 0) + j - stackoffset, R_SP);
		    flushcode();
		    }
	    if (fnsavescr)
		{
		codemdx(P_MOVE, R_SCRREG, (SYMBOL *)NULL,
			1 - stackoffset, R_SP);
		flushcode();
		}
	    code8(P_ADJSP, VR_SP, -stackoffset);
	    flushcode();
	    if (fnargregs) {
		code00(P_POP, R_SP, R_ABITMP);
		flushcode();
		code8(P_ADJSP, VR_SP, -fnargregs);
		flushcode();
		code00(P_PUSH, R_SP, R_ABITMP);
		flushcode();
		}
	    code6(P_JRST, (VREG *)NULL, n->Nleft->Nid);
	    return NULL;		/* can't want a return value */
	    }
	}

    /* Arguments proven dead before the first bare call no longer need their
    ** incoming ABI ACs reserved while that call's arguments are generated.
    */
    if (fnargpredropmask) {
        fnargkeepmask &= ~fnargpredropmask;
        fnargpredropmask = 0;
    }

    if (fnflags & TF_FORTRAN)	    /* FORTRAN fn */
	XF4_call_spill = (char) ~0;		/* spill preserved regs if XF4 call */ // FW KCC-NT

    vrallspill();			/* save active non-preserved regs */

    XF4_call_spill = 0;

    /* Evaluate an ordinary indirect C call target before loading the fixed
    ** argument ACs.  gaddress() may need a scratch register; evaluating it
    ** after AC1..AC4 are populated can overwrite a live argument, especially
    ** in non-optimized code.  The C language does not sequence evaluation of
    ** the function designator relative to its arguments, so this order is
    ** valid.  Keeping CALLADDR live also prevents the argument generator from
    ** allocating its register.
    */
    if (!(fnflags & (TF_BLISS | TF_FORTRAN | TF_INTERRUPT))
      && n->Nleft->Nop != Q_IDENT) {
        calladdr = gaddress(n->Nleft);
        /* AC1..AC4 are fixed argument destinations and may overwrite the
        ** register chosen for the function designator.  Preserve the target
        ** in KCC's reserved AC16 before argument setup.  fn_needs_scrreg()
        ** makes the containing function save/restore AC16 for indirect calls.
        */
        code00(P_MOVE, R_SCRREG, vrtoreal(calladdr));
        flushcode();
        vrfree(calladdr);
        calladdr = NULL;
    }

    /*
     * Next push function arguments
     */

    l = n->Nright;
    narg = stackoffset;			/* remember argument block start */

    /*
     * Choose bliss, fortran, interrupt, or normal C function argument linkage
     */

    if (fnflags & TF_BLISS)
	emit_blissargs (l);		/* bliss linkage */
    else if (fnflags & TF_FORTRAN)
	{
	/* fortran linkage */

	code1(P_MOVS, (r = vrget()), (- sizeargs(l)) & 0777777L);
	code0(P_PUSH, VR_SP, r);	/* Start with -<# arg wds>,,0 */
	stackoffset++;
	emit_blissargs(l);		/* now push args in BLISS order */
	}
    else if (fnflags & TF_INTERRUPT)
	{
	/*
	 * FW 2A(52)
	 *
	 * This is an interrupt function.  As such, it needs a special
	 * prolog and epilog.
	 */

	}
    else				/* ...No, it's a C fn */
	{
	NODE *arglist = l;
	int directargs;
	directargs = gccabi_direct_reg_args(arglist,
		siz > GCCABI_RET_REGS ? 1 : 0, n->Nleft->Ntype->Tproto, 0);
	if (!directargs) {
	    while (l != NULL)
		{
		if (l->Nop == N_EXPRLIST)
		    {
		    gfnarg(l->Nright);
		    l = l->Nleft;
		    }
		else
		    {
		    gfnarg(l);
		    break;
		    }
		}

	    if (siz > GCCABI_RET_REGS)
		code13(P_MOVE, VR_RETVAL,
		    autooff_v11(n->Nretstruct));
	    abiregwords = gccabi_load_args(arglist,
		    siz > GCCABI_RET_REGS ? 1 : 0, n->Nleft->Ntype->Tproto);
	    if (abiregwords) {
		flushcode();
		code8(P_ADJSP, VR_SP, -abiregwords);
		flushcode();
		stackoffset -= abiregwords;
		}
	    }
	else if (siz > GCCABI_RET_REGS)
	    code13(P_MOVE, VR_RETVAL,
		autooff_v11(n->Nretstruct));
	}

    narg -= stackoffset;	/* calculate neg number of arg words */

    if (fnflags & TF_FORTRAN)	/* for a FORTRAN fn */
	{
	/*
	 * Do FORTRAN call.  Must get function address first, in case it is
	 * an expression that might possibly clobber AC16, and then point
	 * AC16 (R_FAP) to the start of our args on stack.
	 */

	r = gaddress(n->Nleft);		/* Get function addr first */
	code13(P_MOVE, VR_FAP, narg+2); /* Point to just after count */
	code4(P_PUSHJ, VR_SP, r);	/* Call function */
	}
    else if (n->Nleft->Nop == Q_IDENT)
	code6(P_PUSHJ, VR_SP, n->Nleft->Nid);	/* optimization */
    else
        codemdx(P_PUSHJ, R_SP, (SYMBOL *)NULL, 0, R_SCRREG);

    /* Arguments proven dead at the first bare call no longer need their
    ** incoming ABI ACs reserved after control returns from that call.
    */
    if (fnargdropmask) {
        fnargkeepmask &= ~fnargdropmask;
        fnargdropmask = 0;
    }

    /*
     * flush args off stack
     */

    if (narg)
	{
	code8 (P_ADJSP, VR_SP, narg);
	stackoffset += narg;
	}

    if ((n->Ntype->Tspec == TS_STRUCT || n->Ntype->Tspec == TS_UNION)
      && siz > 2 && siz <= GCCABI_RET_REGS)
	{
	INT roff;
	roff = autooff_v11(n->Nretstruct);
	for (narg = 0; narg < siz; ++narg)
	    codemdx(P_MOVEM, narg + 1, (SYMBOL *)NULL, roff + narg, R_SP);
	code13(P_MOVE, (r = vrretget()), roff);
	}
    else if (siz == 1)
	r = vrretget ();		/* one return register */
    else if (siz == 2) {
	r = vrretdget ();		/* two */
        if (tisdimode(n->Ntype) && !tisunsign(n->Ntype))
            code8(P_TLZ, VR2(r), 0400000L);
    }
    else if (siz > GCCABI_RET_REGS)
	{
	code13 (P_MOVE, (r = vrretget ()),
		autooff_v11(n->Nretstruct));
	}
    else
	return NULL;			/* Returning void */

    if (fnflags & TF_FORTRAN)	/* for a FORTRAN fn */
	{
	/* FORTRAN functions return values in regs 0+1 instead of 1+2 */

	gretmove(n->Ntype, r, VR_ZERO);
	code5(P_SETZ, VR_ZERO);	/* This may not be necessary */
	}

    r->Vrtype = n->Ntype;		/* Set C type of result obj */
    return r;
    }

/*
 * void emit_blissargs (NODE*)
 *
 * This little recursive function traverses a NODE tree
 * and generates calles to gfnarg () such as to emit pushes
 * for a function's arguments in the reverse of the usual order.
 */

static
void
emit_blissargs (NODE *l)
    {
    if (l)
	{
	if (l->Nop == N_EXPRLIST)
	    {
	    emit_blissargs(l->Nleft);
	    gfnarg(l->Nright);
	    }
	else
	    gfnarg(l);
	}
    }

/* Count # words needed by all args ahead of time, so FORTRAN linkage
** can use it without backpatching.
*/

static INT
sizeargs(NODE *l)
{
    INT size = 0;
    for (; l; l = l->Nleft)
	{
	if (l->Nop == N_EXPRLIST)
	    size += sizetype(l->Nright->Ntype);
	else
	    {
	    size += sizetype(l->Ntype);
	    break;
	    }
	}
    return size;
}

/* GFNARG - generate function argument value and push on stack
**
*/
static void
gfnarg(NODE *n)
{
    VREG *reg;
    INT siz;

    siz = sizetype(n->Ntype);
    if (n->Ntype->Tspec == TS_ARRAY)
	{
	int_error("gfnarg: array type %N", n);
	siz = 0;
	}

#if 0		/* Reg linkage */
    if (Register_Id(n))
	{
	if (siz == 1)
	    {
	    code00(P_PUSH, R_SP, n->Nid->Sreg);
	    stackoffset++;
	    }
	else
	    int_error ("gfnarg: size of reg var %s > 1", n->Nid->Sname);
	return;
	}
#endif

    switch (siz)
	{
	case 1:
	    code0(P_PUSH, VR_SP, genexpr(n));
	    stackoffset++;
	    break;
	case 2:
	    reg = genexpr(n);
            if (tisdimode(n->Ntype) && !tisunsign(n->Ntype)) {
                code8(P_TLZ, VR2(reg), 0400000L);
                code8(P_TLN+POF_ISSKIP+POS_SKPE, reg, 0400000L);
                code8(P_TLO, VR2(reg), 0400000L);
            }
	    code0(P_PUSH, VR_SP, reg);
	    code0(P_PUSH, VR_SP, VR2(reg));
            if (tisdimode(n->Ntype) && !tisunsign(n->Ntype))
                code8(P_TLZ, VR2(reg), 0400000L);
	    vrfree(reg);
	    stackoffset += 2;
	    break;

	default:
	    reg = vrget();
	    code8(P_ADJSP, VR_SP, siz);	/* Make space on the stack */
	    stackoffset += siz;		/* remember where we are on stack */
	    code13(P_MOVE, reg, -(siz-1));	/* Get pointer to the space */
	    code4s(P_SMOVE, reg, genexpr(n), 0, siz);	/* Copy, release reg */
	    vrfree(reg);
	}
}

/* PACKEDMEMBYTE - Return a packed member's 9-bit byte offset.
**
** The normal negative Ssmoff encoding records word, P and S.  Packed
** integer members that fit in one word are byte-aligned, so recover their
** exact C-byte start independently of the member's value width.
*/
static int
packedptrderef(NODE *n)
{
    return n != NULL && n->Nop == N_PTR && n->Nleft != NULL
        && tispackedptr(n->Nleft->Ntype);
}

static int
bitptrderef(NODE *n)
{
    return n != NULL && n->Nop == N_PTR && n->Nleft != NULL
        && tisbitptr(n->Nleft->Ntype);
}

static int
maybitptrderef(NODE *n)
{
    return n != NULL && n->Nop == N_PTR && n->Nleft != NULL
        && tismaybitptr(n->Nleft->Ntype);
}

static int
bitptrmember(NODE *n)
{
    if (n == NULL || n->Nleft == NULL) return 0;
    if (n->Nop == Q_MEMBER)
        return tisbitptr(n->Nleft->Ntype);
    if (n->Nop == Q_DOT && n->Nleft->Nop == N_PTR
      && n->Nleft->Nleft != NULL)
        return tisbitptr(n->Nleft->Nleft->Ntype);
    return 0;
}

/* Return a packed member encoding as an exact bit displacement. */
static INT
packedoffbit(INT off)
{
    unsigned INT code;
    INT word, bit;
    int pos, siz;

    if (off >= 0) return off * TGSIZ_WORD;
    code = (unsigned INT)(-off);
    if ((code & 07777L) == 07300L
      || (code & 07777L) == 07400L
      || (code & 07777L) == 07500L
      || (code & 07777L) == 07600L)
        return (INT)(code >> 12);
    if ((code & 07777L) == 07700L)
        return (INT)(code >> 12) * TGSIZ_CHAR;
    word = (INT)(code >> 12);
    pos = (int)((code >> 6) & 077);
    siz = (int)(code & 077);
    bit = word * TGSIZ_WORD + TGSIZ_WORD - pos - siz;
    return bit;
}

static int
packedcross(INT off)
{
    INT code;

    if (off >= 0)
        return 0;
    code = -off;
    return (code & 07777L) == 07700L;
}

static int
packedbit(INT off)
{
    INT code;
    if (off >= 0) return 0;
    code = -off;
    return (code & 07777L) == 07600L;
}

static int
crossbit(INT off)
{
    INT code;
    if (off >= 0) return 0;
    code = -off;
    return (code & 07777L) == 07300L;
}

static INT
crossbitoff(INT off)
{
    if (!crossbit(off)) return -1;
    return (unsigned INT)(-off) >> 12;
}

static int
packedbitscalar(INT off)
{
    INT code;
    if (off >= 0) return 0;
    code = -off;
    return (code & 07777L) == 07500L;
}

static int
packedbitagg(INT off)
{
    INT code;
    if (off >= 0) return 0;
    code = -off;
    return (code & 07777L) == 07400L;
}

static INT
packedmembit(INT off)
{
    if (!packedbit(off) && !packedbitscalar(off)) return -1;
    return (unsigned INT)(-off) >> 12;
}

static INT
packedmembyte(INT off)
{
    INT code, word, pos, bsiz, bit;

    if (off >= 0)
        return -1;
    code = -off;
    if ((code & 07777L) == 07700L)
        return (unsigned INT)code >> 12;
    bsiz = code & 077;
    pos = (code & 07700) >> 6;
    word = (unsigned INT)code >> 12;
    if (bsiz <= 0 || bsiz > TGSIZ_WORD)
        return -1;
    bit = word * TGSIZ_WORD + TGSIZ_WORD - pos - bsiz;
    if (bit % TGSIZ_CHAR)
        return -1;
    return bit / TGSIZ_CHAR;
}

/* GADDRESS - Generate address of object or function.
**	Will set up as byte pointer if necessary
*/
static VREG *
gaddress(NODE *n)
{
    int boff, bsiz;
    INT offset, b;
    VREG *r, *p;
    SYMBOL *s;

    switch (n->Nop)
	{
	case N_COMPLIT:	/* C99 compound literal */
	    if (n->Nleft != NULL && n->Nleft->Nop == N_DATA)
		genadata(n->Nleft);
	    return gaddress(n->Nright);

	case Q_PLUS:
	case Q_MINUS:
            /* Address-valued array subscripts retain an ARRAY result type on
            ** the lvalue node even though one operand is the pointer used to
            ** form the address.  Feeding that node back through genexpr()
            ** makes gbinary() classify e.g. 2 + a as ordinary ARRAY
            ** arithmetic and eventually call garithop(TS_ARRAY).  Generate
            ** the underlying pointer arithmetic directly instead.  This is
            ** especially visible for &vla[i], but applies to fixed arrays too.
            */
            if (n->Nleft != NULL && n->Nright != NULL)
                {
                if (n->Nleft->Ntype->Tspec == TS_PTR)
                    {
                    r = genexpr(n->Nleft);
                    p = gptraddend(n->Nleft->Ntype, n->Nright);
                    return gptrop(n->Nop, r, p,
                                  n->Nleft->Ntype, n->Nright->Ntype);
                    }
                if (n->Nop == Q_PLUS && n->Nright->Ntype->Tspec == TS_PTR)
                    {
                    p = gptraddend(n->Nright->Ntype, n->Nleft);
                    r = genexpr(n->Nright);
                    return gptrop(n->Nop, r, p,
                                  n->Nright->Ntype, n->Nleft->Ntype);
                    }
                }
            return genexpr(n);

	case Q_ASPLUS:	/* ptr += &a[] */
	case Q_ASMINUS:	/* ptr -= &a[] */
	case N_PTR:
	    return genexpr(n->Nleft);

	case Q_DOT:
	case Q_MEMBER:

	    if (n->Nop == Q_MEMBER)
		{
		if (debcsi == KCC_DBG_NULL)
		    {
		    _chnl = n->sfline;
		    switch (n->Nleft->Nop)
			{
			case N_CAST:
			case N_ADDR:
			case Q_ASGN:
			case N_FNCALL:
			    if (n->Nleft->Nleft->Nop == Q_IDENT)
				code4 (P_NULPTR, (VREG *) NULL, gaddress (n->Nleft->Nleft));
			    else
				_chnl = -1;
			    break;
			default:
			    code4 (P_NULPTR, (VREG *) NULL, gaddress (n->Nleft));
			    break;
			}
		    }
		r = genexpr (n->Nleft);
		}
	    else
		r = gaddress (n->Nleft);

	    offset = n->Nxoff;		/* calculate offset */

        /* A member selected through an S=1 logical bit pointer is already
        ** based at the exact first bit of the packed aggregate.  Advance
        ** by the member's exact bit displacement and keep the S=1 form.
        */
        if (bitptrmember(n))
            {
            b = packedoffbit(offset);
            if (b != 0)
                {
                p = vrget();
                code1(P_MOVE, p, b);
                code0(P_ADJBP, p, r);
                vrfree(r);
                r = p;
                }
            return r;
            }

        /* A normal cross-word bit-field uses an exact S=1 internal bit
        ** pointer.  Its offset is measured from the aggregate's first bit,
        ** so first advance the word address, convert it to a one-bit byte
        ** pointer, and then advance within that word.  Such pointers never
        ** escape because C forbids taking a bit-field address.
        */
        if (crossbit(offset))
            {
            INT bit = crossbitoff(offset);
            INT word = bit / TGSIZ_WORD;
            INT phase = bit % TGSIZ_WORD;
            if (word != 0)
                code1(P_ADD, r, word);
            pitopc(r, 1, 0, 0);
            if (phase != 0)
                {
                p = vrget();
                code1(P_MOVE, p, phase);
                code0(P_ADJBP, p, r);
                vrfree(r);
                r = p;
                }
            return r;
            }

        /* A packed-char aggregate is addressed by a 9-bit byte pointer.
        ** Its character members are therefore reached by adjusting that
        ** existing byte pointer, not by rebuilding one from a word address.
        */
        if (offset < 0
          && (packedbitagg(offset) || packedbit(offset)
              || packedbitscalar(offset) || packedcross(offset)
              || ((n->Nop == Q_MEMBER && n->Nleft->Ntype->Tspec == TS_PTR
                   && tispacked(n->Nleft->Ntype->Tsubt))
                  || (n->Nop == Q_DOT && tispacked(n->Nleft->Ntype)))))
            {
            if (packedbitagg(offset))
                {
                INT bit = (unsigned INT)(-offset) >> 12;
                INT byte = bit / TGSIZ_CHAR;
                INT phase = bit % TGSIZ_CHAR;

                if (phase == 0 && n->Ntype->Tspec == TS_ARRAY
                  && n->Ntype->Tsubt != NULL && tispacked(n->Ntype->Tsubt))
                    {
                    if (byte != 0)
                        {
                        p = vrget();
                        code1(P_MOVE, p, byte);
                        code0(P_ADJBP, p, r);
                        vrfree(r);
                        r = p;
                        }
                    return r;
                    }

                /* First-class internal packed pointers use an ordinary
                ** PDP-10 byte pointer with S=1.  A one-bit byte can advance
                ** continuously across word boundaries, unlike a 9/16/18-bit
                ** byte pointer, while still fitting in the normal one-word
                ** pointer ABI.
                */
                if (byte != 0)
                    {
                    p = vrget();
                    code1(P_MOVE, p, byte);
                    code0(P_ADJBP, p, r);
                    vrfree(r);
                    r = p;
                    }
                code10(P_PTRCNV, r, (SYMBOL *)NULL, 1, -TGSIZ_CHAR);
                if (phase != 0)
                    code1(P_SUB, r, phase << 30);
                return r;
                }
            if (packedbit(offset) || packedbitscalar(offset))
                {
                b = packedmembit(offset) / TGSIZ_CHAR;
                if (b != 0)
                    {
                    p = vrget();
                    code1(P_MOVE, p, b);
                    code0(P_ADJBP, p, r);
                    vrfree(r);
                    r = p;
                    }
                return r;
                }
            b = packedmembyte(offset);
            if (b >= 0)
                {
                int mbsiz = tbitsize(n->Ntype);

                if (b != 0)
                    {
                    p = vrget();
                    code1(P_MOVE, p, b);
                    code0(P_ADJBP, p, r);
                    vrfree(r);
                    r = p;
                    }
                if (mbsiz != TGSIZ_CHAR && !packedcross(offset))
                    code10(P_PTRCNV, r, (SYMBOL *)NULL, mbsiz, -TGSIZ_CHAR);
                return r;
                }
            }

	/* Check for attempt to get address of object within a word. */
	    if (offset < 0)		/* bitfield or byte? */
		{
		offset = -offset;		/* Get back original encoding */
		if (!tisbitf(n->Ntype))	/* If not bitfield, assume byte */
		    {
		    bsiz = (int) (offset & 077);	/* Get byte size of object */
		    boff = (int) (((TGSIZ_WORD	/* Get offset in bytes */
			      - ((offset & 07700) >> 6)
			    ) / bsiz) - 1);
		    offset = (unsigned INT)offset >> 12;	/* Get wd offset */
		    if (offset > 0)
			code1(P_ADD, r, offset);	/* Do word offset */
		    pitopc(r, bsiz, boff, 1);	/* Turn addr into byte ptr */
		    return r;
		    }

	    /* True bitfield.
	    ** Note that although C does not allow pointers to bitfields, we
	    ** still want to generate bitfield "addresses" for internal use
	    ** so that the code generation can avoid lots of special-casing.
	    */
		p = vrget();		/* Need another reg */
		p->Vrtype = n->Ntype;	/* Set C type of object in reg */
		(void) vrstoreal(r, p);	/* Ensure both regs active! */
		codebp(P_MOVE, vrreal(p),	/* Construct local byte pointer */
			    (unsigned INT)(offset&07777) << 6,
			    vrreal(r), NULL, (unsigned INT)offset >> 12);

	    /* Now we release the struct address, even though it is still
	    ** needed as index by the byte pointer we created!  This is
	    ** should be safe as long as the resulting address is used
	    ** immediately -- for bitfields this should always be true since
	    ** bitfield addresses cannot have an independent existence.
	    */
		vrfree(r);
		return p;
		}

	    if (offset > 0)
		code1(P_ADD, r, offset);	/* perform offset */
	    if (tisbytearray(n->Ntype))	/* If addr of byte array, */
		pitopc(r, elembsize(n->Ntype), 0, 1);	/* make BP to start */
	    else if (tisbyte(n->Ntype))	/* If addr of single byte, */
		pitopc(r, tbitsize(n->Ntype),	/* point to low byte */
			    (TGSIZ_WORD/tbitsize(n->Ntype))-1, 1);
	    return r;

	case Q_IDENT:
	/* Note type checked is that of the symbol's, not that of the
	** node's.  This ensures we do the right thing when the ident
	** is that of a function or array.
	*/
	    r = vrget();
	    r->Vrtype = n->Ntype;		/* Set C type of object in reg */
	    s = n->Nid;
	    if (tisbytearray(s->Stype))	/* If ident is byte array, */
		{
		bsiz = elembsize(s->Stype);		/* set byte params */
		offset = 0;				/* with left-justified byte */
		}
            else if (tispacked(n->Ntype))
                {
                bsiz = TGSIZ_CHAR;
                offset = 0;
                }
	    else if (tisbyte(n->Ntype))	/* If it's a single byte, */
		{
		bsiz = tbitsize(n->Ntype);		/* also set them */
		offset = (TGSIZ_WORD/bsiz)-1;	/* with right-justified byte */
		}
	    else
		bsiz = 0;

	    switch (s->Sclass)
		{
		case SC_RAUTO:		/* value already moved to reg */
		    code00(P_MOVE, r->Vrloc, s->Sreg);
		    return r;

		case SC_AUTO:		/* Local variables */
                    {
                    SYMBOL *vb = vlabase_v11(s);
                    if (vb != NULL)
                        {
                        codemdx(P_MOVE, vrtoreal(r), (SYMBOL *)NULL,
                                autooff_v11(vb), frameindex_v11());
                        if (tisbytearray(s->Stype))
                            pitopc(r, elembsize(s->Stype), 0, 1);
                        return r;
                        }
                    if (fnvla_v11)
                        codemdx(P_MOVEI, vrtoreal(r), (SYMBOL *)NULL,
                                autooff_v11(s), R_MAXREG);
                    else
                        code13(P_MOVE, r, autooff_v11(s));
                    }
		    break;


		case SC_RARG:		/* Function parameters */
		    code00(P_MOVE, r->Vrloc, s->Sreg);
		    return r;

		case SC_ARG:
                    if (fnvla_v11)
                        codemdx(P_MOVEI, vrtoreal(r), (SYMBOL *)NULL,
                                argoff_v11(s), R_MAXREG);
                    else
                        code13(P_MOVE, r, argoff_v11(s));
		    break;

		case SC_ENUM:
		    int_error("gaddress: enum tag: %S %N", s, n);
		    return r;

		case SC_ISTATIC:	/* Internal static */
		    s = s->Ssym;	/* uses internal label instead */

		/* FALLTHROUGH */
		case SC_XEXTREF:
		case SC_EXLINK:	/* Anything with linkage */
		case SC_EXTDEF:
		case SC_EXTREF:
		case SC_INTDEF:
		case SC_INTREF:
		case SC_INLINK:
		    if (bsiz)				/* If byte pointer is addr, */
			code10(P_MOVE, r, s, bsiz, offset);	/* make BP */
		    else
			code3(P_MOVE, r, s);	/* else just make addr */
		    return r;

		default:
		    int_error("gaddress: bad Sclass %d %N", s->Sclass, n);
		    return r;
		}

	    if (bsiz)			/* If addr is a byte addr, */
		pitopc(r, bsiz, offset, 1);	/* make BP */
	    return r;

	default:
	    int_error("gaddress: bad op %N", n);
	    return 0;
	}
}

/* GETMEM - Get object from memory, given address in register.
**	Releases the register unless the "keep" flag is set.
*/

VREG *
getmem(VREG *reg, TYPE *t, int byte, int keep)
{
    VREG *q;

    switch (sizetype(t))
	{
	case 1:
	    q = vrget();
	    q->Vrtype = t;		/* Set C type of object in reg */
	    if (byte)
		(keep ? codek0(P_LDB, q, reg) : code0(P_LDB, q, reg));
	    else
		(keep ? codek4(P_MOVE, q, reg) : code4(P_MOVE, q, reg));
	    return q;

	case 2:
	    q = vrdget();
	    q->Vrtype = t;		/* Set C type of object in reg */
	    if (tisdimode(t))
		gdimemload(q, reg, t, keep);
	    else
		(keep ? codek4(P_DMOVE, q, reg) : code4(P_DMOVE, q, reg));
	    return q;

	default:
	    return reg;
	}
}


/* STOMEM - Store register into memory; inverse of GETMEM.
**	Releases the address register, returns the value register
**	for possible further processing.
*/
VREG *
stomem(VREG *reg, VREG *ra, INT siz, int byteptr)		
/* Reg w/value to store (NULL if stacked struct), Reg w/address to 
 * store into, Size of object in words, True if "address" is a byte pointer 
 */
{
    switch (siz)
	{
	case 1:			/* Store single word or byte */
	    if (byteptr)
		code0(P_DPB, reg, ra);
	    else
		code4(P_MOVEM, reg, ra);
	    break;

	case 2:			/* Store doubleword */
	    if (reg && reg->Vrtype && tisdimode(reg->Vrtype))
		gdimemstore(reg, ra);
	    else
		code4(P_DMOVEM, reg, ra);
	    break;

	default:			/* Store a stacked structure */
	/* ra has dest addr, reg has source addr */
	    code4s(P_SMOVE, ra, reg, 0, siz);	/* Copy, release reg */
	    return ra;
	}
    return reg;
}

/* RGETMEM - Get object from register and do not release register,
**	     imitating GETMEM for registers.
*/

static
VREG *
#if 0
rgetmem(VREG *reg, TYPE *t, int byte, int keep)
#else
rgetmem(VREG *reg, TYPE *t, int keep)
#endif
{
    VREG *q;

    if (sizetype(t) == 1)
	{
	q = vrget();
	q->Vrtype = t;		/* Set C type of object in reg */
#if 0
	if (byte)
	    codek0(P_LDB, q, reg);
	else
	    codek0(P_MOVE, q, reg);

	if (byte)
	    (keep ? codek0(P_LDB, q, reg) : code0(P_LDB, q, reg));
	else
#else
	if (keep)
	    codek0(P_MOVE, q, reg);
	else
	    code0 (P_MOVE, q, reg);
#endif
	return q;
	}
    else
	{
	int_error("rgetmem: register var of non-arithmetic type");
	return reg;
	}
}
#if 0
/* RSTOMEM - Store register into register; inverse of RGETMEM.
**	     Do not release register, imitating STOMEM for registers.
**	
*/
VREG *
rstomem(VREG *reg, int ra, INT siz, int byteptr)
/* Reg w/value to store (NULL if stacked struct), Real reg to 
 * store into, Size of object in words, True if "address" is a byte pointer 
 */
{
    if (siz == 1)
	code00(P_MOVE, ra, reg->Vrloc);
    else	/* reg not defined for arrays or stacked structures */
	{
	int_error("rstomem: register var of non-arithmetic type");
	}
    return reg;
}
#endif

/* PITOPC - Construct byte pointer from word pointer.
**	Currently the only offsets used are either 0 (for left justified byte)
** or <# bytes-per-word>-1 (for right justified byte).  Note that the latter
** can result in unused low-order bits if the byte size does not completely
** fill the word.
**
** Turn a word pointer into a byte pointer.  So that our programs
** should run in extended addressing as well as in section 0, we
** must be able to create either local or global byte pointers,
** so we add in our P/S fields from a table instead of literally.
**
** Even if we know that the pointer will point to the same section
** that the code is in, we cannot use a local byte pointer, because
** pointers are local to where they are stored rather than to where
** the PC currently is.
**
** If the pointer is merely going to be used to load or deposit
** a byte, it will get turned into a local byte pointer later by
** the peepholer (see localbyte() in CCOPT).
*/
static void
pitopc(VREG *r, int bsiz, int offset, int safe)	
/* byte size in bits,  # bytes offset from start of word addr in R, 
 * and Set if pointer known to be non-NULL, needn't test for 0.
 */
{
    if (!safe)				/* Unless we already know not NULL */
	code0(P_SKIP+POF_ISSKIP+POS_SKPE, r, r);	/* NULL stays NULL */
    code10(P_IOR, r, (SYMBOL *)NULL, bsiz, offset);	/* Make it a pointer */
}

/* BPTRREF - sees if expression value consists of a byte pointer
**	reference.
** Returns:
**	1 if expression is a legal lvalue referenced via byte ptr.
**	0 if expression is a legal lvalue referenced via word address.
**	-1 if expression is not an lvalue operand.
*/
static int
bptrref(NODE *n)
{
    switch (n->Nop)
	{
	case Q_DOT:
	case Q_MEMBER:
	    return (n->Nxoff < 0			/* bitfield? */
		    || tisbyte(n->Ntype));	/* or char? */

	case N_PTR:
	    return tisbytepointer(n->Nleft->Ntype);	/* byte pointer deposit */

	case Q_IDENT:
	    return tisbyte(n->Ntype);

	default:
	    return -1;
	}
}

/* GASM - Generate direct assembly language constructs
**
*/
static void
gasm(NODE *n)
{
    NODE *arg;

    if ((arg = n->Nleft) == 0)
	{
	int_error("gasm: no arg %N", n);
	return;
	}
    if (arg->Nop != N_SCONST)
	{
	int_error("gasm: non-string arg %N", n);
	return;
	}
    /* Output the string, minus the terminating null char */
    codestr(arg->Nsconst, arg->Nsclen-1);
}

/* GJFFO - Generate _KCC_jffo(value,label). */
static void
gjffo(NODE *n)
{
    VREG *r;

    if (!n->Nleft || !n->Nxfsym)
	{
	int_error("gjffo: bad arg %N", n);
	return;
	}

    r = genexpr(n->Nleft);
    code6(P_JFFO, r, n->Nxfsym);
    vrfree(r);
}


/* KAR-6/91, Changed sentinal value for _chnl to -1 from 0 */
int		_chnl = -1;
#define mnem_param	temp->Nleft->Nleft->Nleft
#define ac_param	temp->Nleft->Nleft
#define ret_param	temp->Nleft->Nright
#define ea_param	temp->Nright
#define ch_sig		n->Nleft

/* --------------------------- */
/*	imuuo statement	       */
/*		-by KAR 1/91   */
/* --------------------------- */
VREG *
gmuuo(struct node * n)
{
    VREG    *ac, *ret_ac;
    int	    p3_omitted = 0, p4_omitted = 0;
    NODE    *temp = n->Nleft->Nleft;	/* else, BC++ fails!? */

    if ((ret_param->Nop == N_ICONST) && (ret_param->Niconst == 0))
	p3_omitted = 1;
    if ((ea_param->Nop == N_ICONST) && (ea_param->Niconst == 0))
	p4_omitted = 1;
	
	/* The following code is outputed:
	 *	MOVE   ac,ac_contents
         *      muuo   ac,
         *      TDZA   1,1
         *      MOVEI  1,1
	 *      MOVEM  ac,ret_val	; if a return address is specified
	 */
    if (!strcmp(mnem_param->Nright->Nsconst, "CIRC"))
	{
	/* CIRC operates on two raw adjacent AC words, not on KCC's
	** canonical DImode integer representation.  Load exactly two
	** consecutive words through the caller-supplied pointer, circulate
	** that physical pair, and optionally store the raw pair back.
	*/
	VREG *count, *ra;

	ra = genexpr(ac_param->Nright);
	ac = vrdget();
	code4(P_DMOVE, ac, ra);
	count = genexpr(ea_param);
	code4(P_CIRC, ac, count);
	if (p3_omitted != 1)
	    {
	    ra = genexpr(ret_param);
	    code4(P_DMOVEM, ac, ra);
	    }
	vrfree(ac);
	ret_ac = vrget();
	code5(P_SETZ, ret_ac);
	return ret_ac;
	}

    ac = vrget();
    if (ch_sig->Nright->Niconst == 0)
	code0(P_MOVE, ac, genexpr(ac_param->Nright));
    else
	_chnl = ac_param->Nright->Niconst;
    if (p4_omitted != 1)
	code4m(P_MUUO, ac, genexpr(ea_param),mnem_param->Nright->Nsconst);
    else
	code5m(P_MUUO, ac, mnem_param->Nright->Nsconst);
    ret_ac = vrget();
    code0(P_TDZ+POF_ISSKIP+POS_SKPA, ret_ac, ret_ac);
    code1(P_MOVE, ret_ac, 1);
    if (p3_omitted != 1)
	switch (ret_param->Nid->Sclass)
	    {
	    case SC_ARG:
	    case SC_AUTO:
		code4(P_MOVEM, ac, gaddress(ret_param));
		break;
	    case SC_RARG:
	    case SC_RAUTO:
	    case SC_REGISTER:
		code0(P_MOVEM, ac, gaddress(ret_param));
		break;
	    default:
		code6(P_MOVEM, ac, ret_param->Nid);
		break;
	    } /* switch */
    vrfree(ac);
    return(ret_ac);
}
