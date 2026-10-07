/*	CCTYPE.C - Type checking and conversion during parsing
**
**	(c) Copyright Ken Harrenstien 1989
**		All changes after v.123, 4-Jun-1988
**	(c) Copyright Ken Harrenstien, SRI International 1985, 1986
**		All changes after v.41, 8-Aug-1985 (complete re-write)
*/

#include "cc.h"

/* Imported functions */
extern NODE *evalexpr(NODE *);				/* CCEVAL */
extern NODE *ndef(int op, TYPE *t, int f, NODE *l, NODE *r);	/* CCNODE */
extern NODE *ndeftl(int op, TYPE *t, NODE *l);		/* CCNODE */
extern NODE *ndeficonst(INT val);			/* CCNODE */
extern TYPE *findtype(int, TYPE *), *findctype(int, INT, unsigned INT, TYPE *),
	 *findutype(TYPE *), *findqtype(TYPE *, INT);			/* CCSYM */
extern TYPE *tcomposite(TYPE *, TYPE *);		/* CCSYM */
extern int cmputype(TYPE *, TYPE *);			/* CCSYM */

/* Exported functions */
NODE *convcast(TYPE *, NODE *), *convbinary(NODE *), 
	    *convunary(NODE *), *convarrfn(NODE *),
	    *convasgn(TYPE *, NODE *), *convfunarg(NODE *), 
	    *convnullcomb(NODE *), *convvoidptr(NODE *);
TYPE *convfparam(TYPE *), *convternaryt(NODE *);

/* Internal functions and vars */
static int cast_op(TYPE *, TYPE *);
static int nisnull(NODE **);
static int convisnull(NODE *);
static NODE *ndefcast(int, TYPE *, NODE *);
static void convboth(NODE *, TYPE *);
static NODE *convxboth(NODE *, TYPE *);
#if 0
static void convboth();
static NODE *convxboth();
static int cast_op();
static int nisnull();
static int convisnull();
static NODE *ndefcast();
#endif

#if 0

	How cast conversions and definitions are set up:

CCSYM.H: defines (as enums) all of the cast conversions supported.
	These CAST_ operation values are entered in convtab[]
	and used in N_CAST parse-tree nodes.

Next page: Maps from all possible C conversions into the ones actually
	supported.

Cast conversions must be supported in 3 places:
	(1) here in CCTYPE, which controls parsing semantics of cast exprs.
	(2) in CCEVAL which does compile-time evaluation of constants.
	(3) in CCGEN2 which generates code for run-time conversions.
There is also CCDUMP (to examine parse trees) which is rarely invoked.

The CONVTAB table, plus cast_op(), does two things.  In addition to
specifying whether a given cast is legal, it also specifies the general
nature of the cast.  The evaluator and generator must decide for themselves
how to best derive explicit actions from both this general hint and the
exact type values provided in the N_CAST node.  This allows the parser
to remain relatively independent of the evaluator/generator.  Note that
whenever an exact and unique value is needed for a specific cast, the
"castidx" macro can be used (e.g. in switch statements) to derive this.

/* The following definitions list all of the possible cast conversions
** which are legal in C.  They are grouped by target type, just as for
** the type conversion table in CCTYPE.
*/

	/* All conversions to TS_VOID are allowed. */
	CAST_VOID	/* Value is discarded in all cases. */

	/* No conversions to TS_FUNCT are allowed. */
	/* No conversions to TS_ARRAY are allowed. */

	/* The only conversion to TS_STRUCT is the trivial one. */
	/* The only conversion to TS_UNION  is the trivial one. */
	CAST_TRIV	/* Permit "conversion" only if types identical */

	/* Conversions to TS_PTR.  Lots of checking involved. */
	/* Pointer subtype conversions.  A pointer may have
	** any subtype, including "void" in ANSI C.
	** This means there are N different
	** pointer subtypes which may be converted into any of the other
	** N subtypes.  We'll let the code generation worry about it
	** and punt with "pointer to pointer".
	** Conversions from void, struct/union, enum or floats are illegal.
	*/
	CAST_FN_PF	/* Function -> Pointer to Function */
	CAST_AR_PA	/* Array -> Pointer to 1st Array element */
	CAST_PT_PT	/* Pointer to a Type -> Pointer to another Type */
	CAST_IT_PT	/* Integer type -> Pointer type */

	/* Conversions to TS_ENUM.  All others are illegal. */
	CAST_EN_EN	/* Enum to Enum.  (may wish to check tags) */
	CAST_IT_EN	/* Integer to Enum */

	/* Conversions to TS_FLOAT, TS_DOUBLE, TS_LNGDBL */
	CAST_FP_FP	/* Floating to Floating */
	CAST_IT_FP	/* Integer to Floating */

	/* Conversions to Integer types */
	CAST_IT_IT	/* from Integer Type */
	CAST_FP_IT	/* from Floating-Point type */
	CAST_EN_IT	/* from ENumeration type */
	CAST_PT_IT	/* from Pointer Type */

#endif

/* CONVTAB - Conversion table.
**	This table attempts to be relatively machine independent by only
** indicating the general nature of the conversion required for a particular
** cast.  It is indexed by use of the "castidx" macro.
**
** The value of convtab[castidx(from,to)] will specify the general kind
** of conversion that the code generator will have to apply.
** There are certain special values
** which indicate errors or a need for further checking.
** WARNING!!! If the ordering of the TS_ values defined in CCSYM.H is
** changed, this table needs to be changed too!!
**	Note that most self-to-self integer conversions, such as long to
** long, have CAST_NONE.  However, both bitfields and chars have the general
** CAST_IT_IT type instead, because the size of bitfields can vary, and
** because KCC has a special feature which allows the size of chars to
** also vary.
*/
static char
convtab[TS_MAX*TS_MAX] = {
/* 18 basic types, plus 2 internal (bitfields), plus long long pair */
/* Vo Fn Ar St Un Pt En Fl Db LD SB SC SS SI SL UB UC US UI UL LL ULL */

/* Conversions to TS_VOID.  All such conversions are allowed. */
	CAST_VOID,CAST_VOID,CAST_VOID,
	CAST_VOID,CAST_VOID,CAST_VOID,CAST_VOID,
	CAST_VOID,CAST_VOID,CAST_VOID,
	CAST_VOID,CAST_VOID,CAST_VOID,CAST_VOID,CAST_VOID,
	CAST_VOID,CAST_VOID,CAST_VOID,CAST_VOID,CAST_VOID,
	CAST_VOID,CAST_VOID,

/* Conversions to TS_FUNCT.  No such conversion is allowed. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,

/* Conversions to TS_ARRAY.  No such conversion is allowed. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,

/* Conversions to TS_STRUCT.  Only the trivial conversion is allowed. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_TRIV ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,

/* Conversions to TS_UNION.  Only the trivial conversion is allowed. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_TRIV ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,

/* Conversions to TS_PTR.  Lots of checking involved. */
	CAST_ILL  ,CAST_FN_PF,CAST_AR_PA,
	CAST_ILL  ,CAST_ILL  ,CAST_PT_PT,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_IT_PT,CAST_IT_PT,CAST_IT_PT,CAST_IT_PT,CAST_IT_PT,
	CAST_IT_PT,CAST_IT_PT,CAST_IT_PT,CAST_IT_PT,CAST_IT_PT,
	CAST_IT_PT,CAST_IT_PT,

/* Conversions to TS_ENUM.  Only enum and integer source allowed. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_EN_EN,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_IT_EN,CAST_IT_EN,CAST_IT_EN,CAST_IT_EN,CAST_IT_EN,
	CAST_IT_EN,CAST_IT_EN,CAST_IT_EN,CAST_IT_EN,CAST_IT_EN,
	CAST_IT_EN,CAST_IT_EN,

/* Conversions to TS_FLOAT. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_NONE ,CAST_FP_FP,CAST_FP_FP,
	CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,
	CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,
	CAST_IT_FP,CAST_IT_FP,

/* Conversions to TS_DOUBLE. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_FP_FP,CAST_NONE ,CAST_FP_FP,
	CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,
	CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,
	CAST_IT_FP,CAST_IT_FP,

/* Conversions to TS_LNGDBL. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_FP_FP,CAST_FP_FP,CAST_NONE ,
	CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,
	CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,CAST_IT_FP,
	CAST_IT_FP,CAST_IT_FP,

/* Conversions to TS_BITF. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_PT_IT,CAST_EN_IT,
	CAST_FP_IT,CAST_FP_IT,CAST_FP_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,

/* Conversions to TS_CHAR. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_PT_IT,CAST_EN_IT,
	CAST_FP_IT,CAST_FP_IT,CAST_FP_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,

/* Conversions to TS_SHORT. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_PT_IT,CAST_EN_IT,
	CAST_FP_IT,CAST_FP_IT,CAST_FP_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_NONE ,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,

/* Conversions to TS_INT. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_PT_IT,CAST_EN_IT,
	CAST_FP_IT,CAST_FP_IT,CAST_FP_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_NONE ,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,

/* Conversions to TS_LONG. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_PT_IT,CAST_EN_IT,
	CAST_FP_IT,CAST_FP_IT,CAST_FP_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_NONE ,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,

/* Conversions to TS_UBITF. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_PT_IT,CAST_EN_IT,
	CAST_FP_IT,CAST_FP_IT,CAST_FP_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,

/* Conversions to TS_UCHAR. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_PT_IT,CAST_EN_IT,
	CAST_FP_IT,CAST_FP_IT,CAST_FP_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,

/* Conversions to TS_USHORT. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_PT_IT,CAST_EN_IT,
	CAST_FP_IT,CAST_FP_IT,CAST_FP_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_NONE ,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,

/* Conversions to TS_UINT. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_PT_IT,CAST_EN_IT,
	CAST_FP_IT,CAST_FP_IT,CAST_FP_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_NONE ,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,

/* Conversions to TS_ULONG. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_PT_IT,CAST_EN_IT,
	CAST_FP_IT,CAST_FP_IT,CAST_FP_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_NONE ,
	CAST_IT_IT,CAST_IT_IT,

/* Conversions to TS_LONGLONG. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_PT_IT,CAST_EN_IT,
	CAST_FP_IT,CAST_FP_IT,CAST_FP_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_NONE ,CAST_IT_IT,

/* Conversions to TS_ULONGLONG. */
	CAST_ILL  ,CAST_ILL  ,CAST_ILL  ,
	CAST_ILL  ,CAST_ILL  ,CAST_PT_IT,CAST_EN_IT,
	CAST_FP_IT,CAST_FP_IT,CAST_FP_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,CAST_IT_IT,
	CAST_IT_IT,CAST_NONE ,
};

/* Pointer representation conversion table
**	A C pointer can have several different representations or formats
** depending on what it is pointing to.  We handle this by defining a
** PTRREP_ value for each possible format, and assuming that
** all possible formats are encompassed by pointers to each of the
** possible TS_ values.  This allows us to quickly determine pointer
** format and identify any conversions that are necessary.
**
** PCONVTAB is indexed by the first subtype T (as in "pointer to T").
**	The exception is "pointer to array of T" which becomes
**	"pointer to T" iteratively.
*/

#if 0	/* Commented out for now */

enum ptrspec {
	PTRREP_WD,	/* Pointer to word */
	PTRREP_CH,	/* Pointer to normal char */
	PTRREP_MAX,	/* Max # of pointer rep types */
	PTRREP_CH7,	/* Pointer to 7-bit char */
	PTRREP_ILL,	/* Illegal */
};

static ptyptab[20] = {
 /* 18 basic types */
/* Vo Fn Ar St Un Pt En Fl Db SB SC SS SI SL UB UC US UI UL */
	PTRREP_ILL,	/* (void *) illegal at moment, until ANSI comes */
	PTRREP_WD,	/* function addr */
	PTRREP_ILL,	/* No ptrs to array - should be caught earlier */
	PTRREP_WD,	/* (struct foo *) */
	PTRREP_WD,	/* (union foo *) */
	PTRREP_WD,	/* (type **) */
	PTRREP_WD,	/* (enum foo *) */
	PTRREP_WD,	/* (float *) */
	PTRREP_WD,	/* (double *) */
	PTRREP_WD,	/* (long double *) */
	PTRREP_CH,	/* (bitfield *) */
	PTRREP_CH,	/* (char *) */
	PTRREP_WD,	/* (short *) */
	PTRREP_WD,	/* (int *) */
	PTRREP_WD,	/* (long *) */
	PTRREP_CH,	/* (unsigned bitfield *) */
	PTRREP_CH,	/* (unsigned char *) */
	PTRREP_WD,	/* (unsigned short *) */
	PTRREP_WD,	/* (unsigned int *) */
	PTRREP_WD,	/* (unsigned long *) */
};

/* Pointer conversion table, [to][from] */
static pconvtab[PTRREP_MAX][PTRREP_MAX] = {
	/* To word ptr */
	CAST_NONE,		/* From word ptr */
	CAST_PC_PI,		/* From char ptr */

	/* To char ptr */
	CAST_PI_PC,		/* From word ptr */
	CAST_NONE,		/* From char ptr */
};
#endif

/* CAST_OP	Find CAST_ conversion type to use.	[H&S 6.2 - 6.9 incl]
**	This is the only routine that references the conversion table.
**	The returned operation is based purely on the top type of
**	each type, ignoring qualifiers.  Minimal checking is done.
*/
static int
cast_op(struct type * to, struct type * frm)
{
    int cop;

    /* _Bool is represented as a 9-bit unsigned byte type with a semantic
    ** flag.  Converting any scalar to it is a truth-value conversion and
    ** must not collapse to an ordinary same-width integer no-op.
    */
    if (tisbool(to) && !tisbool(frm)) {
        if (!tisscalar(frm)) {
            error("Illegal conversion - %s to _Bool", tsnames[frm->Tspec]);
            return CAST_ILL;
        }
        return CAST_BOOL;
    }

    switch (cop = convtab[castidx(frm->Tspec,to->Tspec)]) {
	case CAST_TRIV:		/* Verify that types are identical */
	    if (to == frm	/* This should have been caught before call */
	      || cmputype(to, frm))	/* Ignore type-qualifiers */
		return CAST_NONE;
	    cop = CAST_ILL;	/* Otherwise drop through to complain */
	/* FALLTHROUGH */
	case CAST_ILL:
	    error("Illegal conversion - %s to %s",
		tsnames[frm->Tspec], tsnames[to->Tspec]);
	    break;
	case CAST_VOID:		/* Discard -- converting to void */
	    break;
	case CAST_NONE:		/* Usually no representation change needed */
	    /* Exact-width KCC integer types can share a TS_ code with the
	    ** native type while differing in value width.  Promotion from
	    ** e.g. signed _KCCtype_int32 to 36-bit int still requires real
	    ** sign extension, so do not collapse it to a no-op.
	    */
	    if (tisinteg(to) && tisinteg(frm)
	      && tbitsize(to) != tbitsize(frm))
		return CAST_IT_IT;
	    break;

	/* These two conversions should have been caught before this.
	*/
	case CAST_AR_PA:	/* Array to T should be ptr to T */
	case CAST_FN_PF:	/* Function should be ptr to function */
	    int_error("cast_op: bad cast - %s to %s",
		tsnames[frm->Tspec], tsnames[to->Tspec]);
	    cop = CAST_ILL;
	    break;

	case CAST_PT_PT:	/* OK as is.  Someday convert into more */
	    break;		/* specific type of conversion? */

	/* Integer to integer conversion.
	**	May be able to detect that no actual conversion is needed.
	**	This code assumes that signed and unsigned types occupy the
	**	same amount of space, and that there is no representation
	**	change when converting between the two!  This will be true
	**	for just about all computers.
	*/
	case CAST_IT_IT:
	    if ((tbitsize(frm) == tbitsize(to))		/* If same size */
		/* && tisunsigned(frm) == tisunsigned(to) *//* & signedness? */
		)
		return CAST_NONE;
	    break;
    default:
	;	/* do nothing */
    }
    return cop;
}

/* CONVCAST - Apply "cast conversions" to an expression.  [H&S 6.10]
**	This is called for explicit casts by the user, and by
**	some implicit conversions elsewhere in CCTYPE.
**	Any type qualifiers are removed from the target type.
**
**	For easier debugging, this almost always forces a cast to the indicated
**	type, even if it may not be necessary.  Optimization can clean up
**	such no-ops farther on.
*/
NODE *
convcast(struct type * t, struct node * n)
{
    int op;

    if (n->Ntype->Tspec == TS_FUNCT || n->Ntype->Tspec == TS_ARRAY)
	n = convarrfn(n);		/* Pre-convert funct/arrays here */
    if (t == n->Ntype)			/* May now be a trivial cast */
	return n;
    if (tisqualif(t))
	t = findutype(t);		/* Use unqualified type as target */

    /* Logical packed pointers carry their representation in the pointer
    ** word itself.  A cast which does not require changing that word must
    ** retain the internal representation flag instead of silently turning
    ** the value back into an ordinary pointer.
    **
    ** A known S=1 pointer may reinterpret another integral element width:
    ** the address remains the same bit address and later arithmetic uses
    ** the new target width.  A TF_MAYBITPTR value is different: its runtime
    ** value may instead be a native byte pointer, whose representation can
    ** depend on the target width.  Keep those casts representation-neutral
    ** by requiring equal element bit widths.
    */
    if (t->Tspec == TS_PTR && n->Ntype->Tspec == TS_PTR
      && (tisbitptr(n->Ntype) || tismaybitptr(n->Ntype))) {
	TYPE *st = n->Ntype->Tsubt;
	TYPE *dt = t->Tsubt;
	INT flags;
	int compatible = 0;
	int subwordsrc = 0;
	int wordtarget = 0;

	if (st != NULL && dt != NULL) {
	    subwordsrc = tisinteg(st)
	      && tbitsize(st) > 0 && tbitsize(st) < TGSIZ_WORD;
	    wordtarget = dt->Tspec != TS_VOID
	      && (!tisinteg(dt) || tbitsize(dt) >= TGSIZ_WORD);
	}

	if (st != NULL && dt != NULL) {
	    if (cmputype(st, dt))
		compatible = 1;
	    else if (tisbitptr(n->Ntype)
	      && (st->Tspec == TS_VOID || dt->Tspec == TS_VOID))
		compatible = 1;
	    else if (tisinteg(st) && tisinteg(dt)) {
		if (tisbitptr(n->Ntype))
		    compatible = 1;
		else if (tbitsize(st) == tbitsize(dt))
		    compatible = 1;
	    }
	    else if (tismaybitptr(n->Ntype) && st->Tspec == TS_VOID
	      && tisinteg(dt) && tbitsize(dt) > 0
	      && tbitsize(dt) < TGSIZ_WORD)
		compatible = 1;
	}

	if (!compatible) {
	    if (tismaybitptr(n->Ntype)
	      && st != NULL && dt != NULL
	      && ((tisinteg(st) && tisinteg(dt))
	        || (subwordsrc && wordtarget)
	        || (st->Tspec == TS_VOID && wordtarget)))
		compatible = 1;
	    else if (tismaybitptr(n->Ntype)
	      && st != NULL && dt != NULL && dt->Tspec == TS_VOID) {
		/* Canonicalize a representation-polymorphic byte pointer to
		** KCC's S=1 logical form before erasing its pointed-to type.
		** The resulting void * is an ordinary ABI pointer type; the raw
		** pointer word itself carries the representation information.
		*/
		return ndefcast(CAST_PT_PT, t, n);
	    }
	    else if (tismaybitptr(n->Ntype))
		error("function-boundary packed pointer cast requires compatible pointer target");
	    else
		error("unsupported logical packed pointer cast");
	    if (!compatible)
		return n;
	}

	/* Casting a representation-polymorphic byte pointer to a word pointer
	** must materialize an ordinary word address.  Both native byte pointers
	** and KCC's S=1 form keep the containing word address in the RH, so the
	** existing pointer cast code can discard the P+S half uniformly.
	*/
	if (tismaybitptr(n->Ntype) && wordtarget
	  && (subwordsrc || (st != NULL && st->Tspec == TS_VOID)))
	    return ndefcast(CAST_PT_PT, t, n);

	flags = t->Tflag;
	if (tisbitptr(n->Ntype))
	    flags |= TF_PACKEDPTR | TF_BITPTR;
	/* Keep representation polymorphism only for a true no-op cast.
	** An explicit cast to a different byte-pointer type must materialize
	** that target representation instead of leaking TF_MAYBITPTR into
	** an ordinary local pointer type.
	*/
	if (tismaybitptr(n->Ntype) && dt != NULL && tisinteg(dt)
	  && tbitsize(dt) > 0 && tbitsize(dt) < TGSIZ_WORD
	  && !(tischar(dt) && tbitsize(dt) == TGSIZ_CHAR))
	    flags |= TF_MAYBITPTR;
	t = findctype(TS_PTR, flags, t->Tsize, dt);

	/* A width-changing TF_MAYBITPTR cast needs code generation so native
	** byte pointers are rewritten while S=1 logical pointers remain raw.
	*/
	if (tismaybitptr(n->Ntype) && st != NULL && dt != NULL
	  && tisinteg(st) && tisinteg(dt)
	  && tbitsize(st) != tbitsize(dt))
	    return ndefcast(CAST_PT_PT, t, n);

	n->Ntype = t;
	return n;
    }

    /* A void pointer may carry KCC's canonical S=1 packed bit address even
    ** after storage in an ordinary void * object has erased the source type
    ** flag.  A cast back to a non-native subword integer pointer therefore
    ** has two runtime representations: ordinary word pointer or S=1 logical
    ** pointer.  Keep ordinary native char * on the standard byte-address ABI.
    */
    if (t->Tspec == TS_PTR && n->Ntype->Tspec == TS_PTR
      && n->Ntype->Tsubt != NULL && n->Ntype->Tsubt->Tspec == TS_VOID
      && t->Tsubt != NULL && tisinteg(t->Tsubt)
      && tbitsize(t->Tsubt) > 0 && tbitsize(t->Tsubt) < TGSIZ_WORD
      && !(tischar(t->Tsubt) && tbitsize(t->Tsubt) == TGSIZ_CHAR)) {
	TYPE *mt = findctype(TS_PTR, t->Tflag | TF_MAYBITPTR,
	                    t->Tsize, t->Tsubt);
	return ndefcast(CAST_PT_PT, mt, n);
    }

    op = cast_op(t, n->Ntype);		/* find out which cast to use */
    if (op == CAST_PT_PT && clevkcc	/* Special hack for KCC extension */
	&& n->Nop == N_SCONST		/* If casting string literal */
	&& tischar(t->Tsubt)) {		/* to another kind of char ptr */
	n->Ntype = t;			/* then just change type of const! */
	return n;			/* And code gen will win. */
    }
    return ndefcast(op, t, n);		/* Make cast using that cast op */
}

/* CONVASGN - Apply "assignment conversions" to an expression.  [H&S 6.11]
**	[dpANS 3.3.16.1]
**	Takes two args, a pointer to the type of the left-hand op
** (destination type) and a node pointer to the right-hand expression
** which needs to have its type checked and perhaps fixed up.
*/
NODE *
convasgn(struct type * lt, struct node * n)
{
    TYPE *rt;		/* Right (source) type */
    NODE *cn;		/* New cast node if any */

    /* If same type, OK */
    if (lt == n->Ntype) return n;
    rt = n->Ntype;

    /* Not same type.  Check to see if it's OK to silently convert the
    ** right-hand type to left-hand type.  If so, call convcast() to do it.
    */
    if (tisarith(lt) && tisarith(rt))	/* Any arith type to any arith type */
	return convcast(lt, n);		/* OK, do the specified conversion */
    if (tisbool(lt) && tisscalar(rt))
        return convcast(lt, n);          /* C99 scalar -> _Bool */

    switch (lt->Tspec) {
    case TS_PTR:
	if (nisnull(&n))		/* Integer 0 to any ptr type */
	    return convcast(lt, n);	/* OK, do the specified conversion */
	rt = n->Ntype;			/* n may have changed */
	if (rt->Tspec != TS_PTR)	/* Ensure all ptr-to-ptr after here */
	    break;
	/* Representation-polymorphic exact-width pointers preserve their raw
	** pointer word.  TF_MAYBITPTR means the runtime S field selects ordinary
	** byte-pointer versus S=1 logical semantics; no cast may rewrite that
	** field here.
	*/
	if (tismaybitptr(lt) && lt->Tsubt != NULL && rt->Tsubt != NULL
	  && cmputype(lt->Tsubt, rt->Tsubt))
	    {
	    n->Ntype = lt;
	    return n;
	    }
	if (tismaybitptr(rt) && !tismaybitptr(lt))
	    {
	    error("exact-width packed pointer requires representation-preserving destination");
	    n->Ntype = lt;
	    return n;
	    }
	if (tisbitptr(rt) && !tisbitptr(lt))
	    {
	    error("non-byte-aligned GNU packed pointer cannot yet escape its local logical pointer representation");
	    return convcast(lt, n);
	    }
	if (tisbitptr(lt) && !tisbitptr(rt))
	    {
	    error("ordinary pointer cannot replace a non-byte-aligned GNU packed logical pointer");
	    return convcast(lt, n);
	    }
	/* Ensure left subtype has all qualifiers that right subtype does */
	if (((~lt->Tsubt->Tflag) & rt->Tsubt->Tflag)&TF_QUALS)
	    break;			/* R has qual that L doesnt */
	if (cmputype(lt->Tsubt, rt->Tsubt)
	  || (lt->Tsubt->Tspec == TS_VOID && rt->Tsubt->Tspec != TS_FUNCT)
	  || (rt->Tsubt->Tspec == TS_VOID && lt->Tsubt->Tspec != TS_FUNCT) )
	    return convcast(lt, n);	/* OK, do the specified conversion */
	break;

    case TS_STRUCT:
    case TS_UNION:
	if (lt->Tspec == rt->Tspec	/* Ignore qualifiers, check tag */
	  && lt->Tsmtag == rt->Tsmtag)
	    return convcast(lt, n);	/* OK, do the specified conversion */
	break;
    default:
	;	/* do nothing */
    }

    /* Last-resort attempt */
    if (rt->Tspec == TS_FUNCT || rt->Tspec == TS_ARRAY)
	return convasgn(lt, convarrfn(n));

    /* Technically nothing else is OK.  But we try to do it anyway
    ** and print a warning message.  If the attempted conversion is illegal
    ** then cast_op will complain.
    */
    cn = convcast(lt, n);		/* Attempt to force the cast */
    if (cn != n && cn->Nop == N_CAST	/* If cast was done, print warning */
      && cn->Ncast != CAST_ILL)		/* unless cast_op already barfed. */
	{
	if (clevel >= CLEV_STRICT)
	    error("Illegal conversion - %s to %s", 
		tsnames[rt->Tspec], tsnames[lt->Tspec]);
	else
	    warn("Implicit conversion - %s to %s",
		tsnames[rt->Tspec], tsnames[lt->Tspec]);
	}
    return cn;
}

/* CONVARRFN - Apply array/function conversions to an expression node.
**	Returns pointer to a converted expression (usually the same node)
*/
NODE *
convarrfn(struct node * n)
{
    switch (n->Ntype->Tspec) {
	default: break;		/* Not array or function */

	case TS_ARRAY:		/* Convert "array of T" to "pointer to T" */
	    if (!(n->Nflag & NF_LVALUE)) {	/* Can only convert lvalues */
		error("Array ref must be lvalue");
	    }				/* Do it anyway after complaint */

	    if (!(n->Nflag & NF_GLOBAL)) { /* If obj has local extent */
		stackrefs++;		/* then count it as a */
		n->Nflag |= NF_STKREF;	/* stack reference */
	    }

	    /* For array name, or array subscripting, just set the type.
	    ** Anything else requires using & to get the address.
	    ** That can only be N_PTR, Q_DOT, or Q_MEMBER.
	    */
	    {
		TYPE *et = n->Ntype->Tsubt;
		TYPE *pt;
		int packedarr = 0;

		if (n->Nop == Q_DOT && n->Nleft != NULL
		  && tispacked(n->Nleft->Ntype))
		    packedarr = 1;
		else if (n->Nop == Q_MEMBER && n->Nleft != NULL
		  && n->Nleft->Ntype != NULL
		  && n->Nleft->Ntype->Tspec == TS_PTR
		  && tispacked(n->Nleft->Ntype->Tsubt))
		    packedarr = 1;

                {
                INT pflags = typbsiztab[TS_PTR];
                if (packedarr)
                    pflags |= TF_PACKEDPTR;
                if (packedarr && n->Nxoff < 0)
                    {
                    unsigned INT code = (unsigned INT)(-n->Nxoff);
                    if ((code & 07777) == 07400
                      && ((code >> 12) % TGSIZ_CHAR) != 0)
                        pflags |= TF_BITPTR;
                    }
                pt = packedarr
                   ? findctype(TS_PTR, pflags, typsiztab[TS_PTR], et)
                   : findtype(TS_PTR, et);
                }

		if (n->Nop == Q_IDENT || n->Nop == Q_PLUS) {
		    n->Ntype = pt;
		    n->Nflag &= ~NF_LVALUE;	/* Ensure not an lvalue */
		} else
		    return ndef(N_ADDR, pt, n->Nflag & ~NF_LVALUE,
				n, (NODE *)NULL);
	    }
	    break;

	case TS_FUNCT:	/* Convert "function of T" to "pointer to fun of T" */
	    if (n->Nop == Q_IDENT)	/* For function name, just set type */
		n->Ntype = findtype(TS_PTR, n->Ntype);
	    else
		return ndeftl(N_ADDR, findtype(TS_PTR, n->Ntype), n);
    }
    return n;
}

/* CONVUNARY - Apply "usual unary conversions" to a node. [H&S 6.12]
**	Returns pointer to a converted expression (usually the same node)
**	Note that for ANSI there are two differences:
**		(1) the float -> double conversion is suppressed!
**		(2) integer promotions preserve value, not unsignedness.
**			For the PDP-10 this means chars/shorts always
**			are converted to "int", as are bitfields less
**			than the length of an int.
**	The new type, if any, is always the unqualified type.  This may
**	cause a "trivial conversion" merely to get rid of any qualifiers.
*/
NODE *
convunary(struct node * n)
{
    TYPE *newtype;

    switch (n->Ntype->Tspec) {
	case TS_FLOAT:			/* If ANSI, leave float alone */
	    if (clevel < CLEV_ANSI) {
		newtype = dbltype;	/* Float to double */
		break;
	    }
	    /* Not converting, drop thru to default */

	/* FALLTHROUGH */
	default:	/* Usual case, no conversion needed unless qualified */
	    if (tisqualif(n->Ntype))
		n = convcast(findutype(n->Ntype), n);
	    return n;

	case TS_BITF:
	case TS_CHAR:
	case TS_SHORT:		/* Signed bitfield, char and short */
		newtype = inttype;	/* always become signed int */
		break;

	case TS_UBITF:
	case TS_UCHAR:
	case TS_USHORT:		/* But if unsigned, depends on ANSIness */
	    if ((clevel >= CLEV_ANSI)			/* If ANSI, */
		&& (TGSIZ_INT > tbitsize(n->Ntype)))	/* check size */
		newtype = inttype;	/* ANSI and int is big enough */
	    else newtype = uinttype;	/* Not ANSI, or not big enough */
	    break;


	case TS_INT:
	    /* Exact-width int extensions narrower than a PDP-10 int undergo
	    ** the ordinary integer promotions.  The canonical TS_INT object
	    ** remains 36 bits and therefore falls through unchanged.
	    */
	    if (tbitsize(n->Ntype) < TGSIZ_INT) {
		newtype = inttype;
		break;
	    }
	    if (tisqualif(n->Ntype))
		n = convcast(findutype(n->Ntype), n);
	    return n;

	case TS_UINT:
	    if (tbitsize(n->Ntype) < TGSIZ_INT) {
		newtype = (clevel >= CLEV_ANSI) ? inttype : uinttype;
		break;
	    }
	    if (tisqualif(n->Ntype))
		n = convcast(findutype(n->Ntype), n);
	    return n;

	case TS_ARRAY:	/* Convert "array of T" to "pointer to T" */
	case TS_FUNCT:	/* Convert "function of T" to "pointer to fun of T" */
	    return convarrfn(n);
    }

    /* Apply some simple cast operation and return result */
    return ndefcast(cast_op(newtype, n->Ntype), newtype, n);
}

/* CONVBINARY - Apply "usual binary conversions" to a node. [H&S 6.13]
**	Takes pointer to a binary operator node, with the operands
** pointed to by the left and right links.
** Returns pointer to a converted expression (usually the same node)
** The type will always be unqualified.
**	Note that normally a float-float rule exists ("2a" in H&S 6.13),
** as ANSI requires this.
**	Also note the check in convboth() to avoid doing a double implicit
** conversion (once by convunary and again by convboth); this makes things
** more efficient and avoids confusing the assignment-op code generation,
** which expects only a single implicit cast.
*/

NODE *
convbinary(struct node * n)
{
    TYPE  *lt, *rt;

    n->Nleft = convunary(n->Nleft);	/* First do unary convs on both */
    n->Nright = convunary(n->Nright);	/* Note this removes qualifiers! */
    n->Ntype = n->Nleft->Ntype;		/* Set type in case of quick return */

    if (n->Nleft->Ntype == n->Nright->Ntype) return n;
    lt = n->Nleft->Ntype;
    rt = n->Nright->Ntype;

    /* 1. If either operand is not arith type, do no additional conversion */
    if (!tisarith(lt) || !tisarith(rt)) return n;

    /* 2x. If one operand is "long double", make both "long double". */
    if (lt == lngdbltype || rt == lngdbltype)
	convboth(n, lngdbltype);

    /* 2. If one operand is "double", make both "double". */
    else if (lt == dbltype || rt == dbltype)
	convboth(n, dbltype);

    /* 2a. If one operand is "float", make both "float".
    ** Need not do clevel check here as convunary already did that;
    ** i.e. if clevel isn't ANSI then floats would be doubles by now.
    */
    else if (lt == flttype || rt == flttype)
	convboth(n, flttype);

    /* 3.  If one operand is "unsigned long long", make both. */
    else if (lt == ulonglongtype || rt == ulonglongtype)
	convboth(n, ulonglongtype);

    /* 3a. If one operand is "long long", make both "long long". */
    else if (lt == longlongtype || rt == longlongtype)
	convboth(n, longlongtype);

    /* 3b. If one operand is "unsigned long", make both "unsigned long" */
    else if (lt == ulongtype || rt == ulongtype)
	convboth(n, ulongtype);

    /* 4+5. CARM only: If one operand is "long" and other is "unsigned int",
    ** make both "unsigned long".
    **  ANSI: if "long" can represent all values of "unsigned int", then use
    ** "long", else use "unsigned long".
    */
    else if (lt == longtype || rt == longtype) {
	if ((lt == uinttype || rt == uinttype)
	  && (longtype->Tsize <= uinttype->Tsize	/* Check type size */
	    || (clevel < CLEV_ANSI)))
	    convboth(n, ulongtype);		/* Sigh, use ulong */
	else convboth(n, longtype);		/* Win, plain long */
    }

    /* 6.  If one operand is "unsigned", then both are made "unsigned" */
    else if (lt == uinttype || rt == uinttype)
	convboth(n, uinttype);

    /* Else should be int, and should have caught the equivalence earlier! */
    else int_error("convbinary: operand types escaped!");

    n->Ntype = n->Nleft->Ntype;		/* Result type is the common type */
    return n;
}

/* CONVBOTH and CONVXBOTH - simple auxiliaries just for convbinary().
*/
static void
convboth(struct node * n, struct type * newtyp)
{
    n->Nleft = convxboth(n->Nleft, newtyp);	/* Do left side */
    n->Nright = convxboth(n->Nright, newtyp);	/* then right */
}

static NODE *
convxboth(struct node * n, struct type * newtyp)
{
    if (n->Ntype != newtyp) {
	/* Do implicit cast.  If casting a cast that's also implicit,
	** we avoid overhead by just changing the existing cast op to the
	** right thing.
	*/
	if (n->Nop == N_CAST
	  && (n->Nflag & NF_USERCAST)==0) {	/* Implicit cast? */
	    n->Ntype = newtyp;			/* Yup, fix it up */
	    n->Ncast = cast_op(newtyp, n->Nleft->Ntype);
	} else					/* Casting something else */
	    n = ndefcast(cast_op(newtyp, n->Ntype), newtyp, n);
    }
    return n;
}

/* CONVFUNARG - Apply the "function argument conversions" to a node.
**	[H&S 6.14]
**	Note that a float argument is always converted to a double,
** regardless of whether this conversion is done for unary or binary
** expressions.
** The new type is always unqualified.
*/
NODE *
convfunarg(struct node * n)
{
    if (n->Ntype->Tspec == TS_VOID) {
	error("Illegal use of void type - function arg");
	return ndeficonst(0L);
        }
    else if (n->Ntype->Tspec ==  TS_FLOAT) { 	/* Cast float to double */
	return ndefcast(cast_op(dbltype, n->Ntype), dbltype, n);
        }
    return convunary(n);
}

/* CONVFPARAM - Apply the "function parameter promotions" to a type.
**	[H&S 9.4]  No explicit casts are possible at this point; we
** simply set the variable type to what actually will be passed as an argument.
**	This routine is called by CCDECL when parsing a function declaration
** and is unlike the others in that it returns a TYPE * pointer
** rather than a NODE * pointer.
**	Note the difference between ANSI and CARM: ANSI preserves value
** whereas CARM preserves unsignedness.  This has the effect of requiring
** ANSI level to return "int" for both signed and unsigned chars/shorts,
** whereas CARM level returns "unsigned int" if the original type was unsigned.
*/
TYPE *
convfparam(struct type * t)
{
    switch (t->Tspec) {	/* Apply funct param convs */
	case TS_VOID:
	    error("Function parameter cannot have type void");
	    break;
	case TS_FUNCT:	/* "Function of T" => "pointer to function of T" */

	    if (clevel < CLEV_ANSI)	/* If not ANSI, complain */
		error("Function parameter cannot have type function");
	    return findtype(TS_PTR, t);

	case TS_ARRAY:		/* "Array of T" => "pointer to T" */
	    return findtype(TS_PTR, t->Tsubt);
	case TS_STRUCT:
	case TS_UNION:
	case TS_PTR:
	case TS_ENUM:
	    break;

	case TS_FLOAT:		/* float => double, always. */
	    return dbltype;
	case TS_DOUBLE:
	case TS_LNGDBL:
	    break;

	case TS_BITF:		/* bitfield => int */
	case TS_UBITF:		/* Ditto for unsigned bitf */
	    int_error("convfparam: BITF param");
	    break;

	case TS_CHAR:		/* char => int */
	case TS_UCHAR:		/* and unsigned char */
	    /* Parameter values are promoted before they are passed; their
	    ** in-memory packing need not divide a 36-bit word.  This matters
	    ** for the exact 16-bit PDP-10 byte extension.
	    */
	    if (clevel >= CLEV_ANSI)	/* If ANSI, */
		return inttype;		/* chars always promoted to int */
	    return (t->Tspec == TS_CHAR ? inttype : uinttype);

	case TS_SHORT:		/* short => int */
	    return inttype;
	case TS_USHORT:		/* unsigned short must check for ANSIness */
	    if (clevel >= CLEV_ANSI)
		return inttype;
	    return uinttype;

	case TS_INT:
	    if (tbitsize(t) < TGSIZ_INT)
		return inttype;
	    break;
	case TS_UINT:
	    if (tbitsize(t) < TGSIZ_INT)
		return (clevel >= CLEV_ANSI) ? inttype : uinttype;
	    break;
	case TS_LONG:
	case TS_ULONG:
	case TS_LONGLONG:
	case TS_ULONGLONG:
	    break;

	default:
	    int_error("convfparam: bad type: %d", t->Tspec);
    }
    return t;
}

/* CONVTERNARYT - Do type checking and conversion for operands of
**	conditional operator.  Returns a type pointer (not a node pointer).
**	Operands must have already been run through convbinary(), thus
**	they have no type-qualifiers.
** One of the following must apply:
**	Both operands have arithmetic type
**	Both have compatible struct or union type
**	Both have void type
**	Both are pointers to {un}qualified versions of compatible types
**	One is a pointer and the other is a null ptr constant
**	One is a pointer and the other is a ptr to {un}qualified (void).
*/
TYPE *
convternaryt(struct node * n)
{
    TYPE *lt, *rt, *t;
    INT quals;

    if (convisnull(n))		/* Combo of NULL and pointer? */
	return n->Nleft->Ntype;	/* Yep, done.  Both nodes now have the */
				/* type of the pointer. */
    if (n->Nleft->Ntype->Tspec != TS_PTR || n->Nright->Ntype->Tspec != TS_PTR)
	return NULL;		/* Not both pointers, gotta give up */
    lt = n->Nleft->Ntype->Tsubt;
    rt = n->Nright->Ntype->Tsubt;
    quals = 0;
    if (tisqualif(lt)) {		/* Get unqualified subtype */
	quals |= lt->Tflag;
	lt = findutype(lt);
    }
    if (tisqualif(rt)) {		/* Get unqualified subtype */
	quals |= rt->Tflag;
	rt = findutype(rt);
    }
    if (lt->Tspec == TS_VOID) {		/* If left is (void *) */
	t = lt;				/* then convert right to that */
    } else if (rt->Tspec == TS_VOID) {	/* If right is (void *) */
	t = rt;				/* then convert left to that */
    } else if ((t = tcomposite(lt, rt)) == NULL) /* Comp of unqual subtypes */
	return NULL;
    t = findtype(TS_PTR, findqtype(t, quals));	/* Combine quals */

    /* Now have result type, ensure both left and right are that type. */
    n->Nleft = convcast(t, n->Nleft);
    n->Nright = convcast(t, n->Nright);
    return t;
}

/* Auxiliary miscellaneous routines */

/* CONVVOIDPTR - Converts combination of pointer and (void *) if present,
**	else does nothing.  Takes a binary op node with left and right
**	expressions already crunched through binary conversions.
**	Ignores qualifiers.
*/
NODE *
convvoidptr(struct node * n)
{
    if (n->Nleft->Ntype->Tspec == TS_PTR
      && n->Nleft->Ntype->Tsubt->Tspec == TS_VOID)
	n->Nright = convcast(n->Nleft->Ntype, n->Nright);
    else if (n->Nright->Ntype->Tspec == TS_PTR
      && n->Nright->Ntype->Tsubt->Tspec == TS_VOID)
	n->Nleft = convcast(n->Nright->Ntype, n->Nleft);
    return n;
}


/* CONVNULLCOMB - Converts combination of pointer and NULL if present,
**	else does nothing.  Takes a binary op node with left and right
**	expressions already crunched through binary conversions.
**	(thus they have no qualifiers on their types)
*/
NODE *
convnullcomb(struct node * n)
{
    (void)convisnull(n);	/* Invoke conversion if any, ignore result */
    return n;
}

/* CONVISNULL - worker routine for CONVNULLCOMB.
**	This is a separate routine so it can be called by CCSTMT's ternary()
**	with an indication of conversion success or failure.  It does
**	the same thing as CONVNULLCOMB but unlike the other CONV routines
**	it returns 0 (no conversion done) or 1 (conversion done) instead of
**	a node pointer, which is OK because the node pointer is never changed.
**	Type-qualifiers are flushed, but they shouldn't exist at this point
**	anyway because the operands have been run through convbinary().
*/
static int
convisnull(struct node * n)
{
    if (n->Nleft->Ntype->Tspec == TS_PTR && nisnull(&n->Nright))
	n->Nright = convcast(n->Nleft->Ntype, n->Nright);
    else if (n->Nright->Ntype->Tspec == TS_PTR && nisnull(&n->Nleft))
	n->Nleft = convcast(n->Nright->Ntype, n->Nleft);
    else return 0;
    return 1;
}

/* NISNULL - Given POINTER TO a node pointer, evaluates that node
**	to determine if it is a null pointer constant.  Updates the pointer
**	pointed to (since evaluation can change it), and returns
**	TRUE if node is a null pointer constant.
**
**	As of 7-Dec-88, the ANSI draft requires that
** a "null pointer constant" be either:
**	An integral constant expression with the value 0,
**	or such an expression cast to the type (void *).
** Someday, someday, they may come to their senses and flush the second
** possibility.  Yeech!
** Note the code here tries to avoid invoking evalexpr() by doing
** some preliminary checks.
*/
static int
nisnull(struct node ** an)
{
    NODE *n = *an;

#if KCC_PHASE_GEN
    /* KGEN only asks the conversion layer about backend-synthesized nodes.
    ** Source expressions were already converted and fully folded before KIR1.
    ** Avoid retaining the complete parser evaluator for this one predicate. */
    while (n->Nop == N_EXPRLIST)
        n = n->Nright;
    return n->Nop == N_ICONST && n->Niconst == 0;
#else

    if (clevel >= CLEV_ANSI) {
	/* Stupid ANSI mistake, but must support (void *)0 */
	if (n->Ntype == voidptrtype) {		/* If type is (void *) */
	    if (n->Nop != N_CAST		/* see if it's a cast */
	      || !tisinteg(n->Ntype))		/* of an integer */
		return 0;
	    n = n->Nleft = evalexpr(n->Nleft);	/* Sigh, must eval it */
	} else if (tisinteg(n->Ntype) && n->Nop != N_ICONST)
	    *an = n = evalexpr(n);		/* Ensure expr is evaluated */
    } else
	while (n->Nop == N_EXPRLIST)	/* Dig down to real expr */
	    n = n->Nright;

    /* Now check for integer 0 */
    return (n->Nop == N_ICONST && n->Niconst == 0);
#endif
}

/* NDEFCAST - Makes a N_CAST node and sets its cast operation value.
*/
static NODE *
ndefcast(int op, struct type * t, struct node * n)
{
    n = ndeftl(N_CAST, t, n);	/* Apply cast to given type */
    n->Ncast = op;		/* Using this specific cast operation */
    return n;
}
