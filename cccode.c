/*	CCCODE.C - Emit pseudo-code into peephole buffer
**
**	(c) Copyright Ken Harrenstien 1989
**		All changes after v.315, 26-Apr-1988
**	(c) Copyright Ken Harrenstien, SRI International 1985, 1986
**		All changes after v.146, 8-Aug-1985
**
**	Original version (c) 1981 K. Chen
*/

#include "cc.h"
#include "ccgen.h"
#include <string.h>		/* For memcpy, memcmp */
#include <stdlib.h>		/* for calloc() */

/* Imported functions */
extern PCODE *findrset(PCODE *, int);				/* CCOPT */
extern int findconst(PCODE *), localbyte(PCODE *, PCODE *);	/* CCOPT */
extern void foldbp(PCODE *), foldbyte(PCODE *),
	foldplus(PCODE *),
	foldboth(void), foldadjbp(PCODE *), optlsh(PCODE *);	/* CCOPT */
extern INT foldstack(INT), hackstack(SYMBOL *);			/* CCOPT */
extern void realcode(PCODE *), outlab(SYMBOL *);	/* CCOUT */

extern void reflabel(SYMBOL *, int), cleanlabs(void),
	freelabel(SYMBOL *);					/* CCSYM */
extern int changereg(int, int, PCODE *), ufcreg(int),
	pnegreg(int, PCODE *), pushneg(int, PCODE *);	/* CCCREG */
extern void foldmove(PCODE *);				/* CCCREG */
extern int sameaddr(PCODE *, PCODE *, INT),
	alias(PCODE *, PCODE *, INT);			/* CCCSE */
extern void foldidx(PCODE *);				/* CCCSE */
extern void foldskip(PCODE *, int),
	foldjump(PCODE *, SYMBOL *), optlab(SYMBOL *),
	inskip(PCODE *);				/* CCJSKP */
extern VREG *vrget(void);
extern void vrfree (VREG *);
extern int vrreal (VREG *);
extern int vrstoreal (VREG *, VREG *);
extern int vrtoreal(VREG *);
extern int rbinreg (PCODE *);
extern int rbinaddr (PCODE *);

/* Exported functions */
PCODE *before(PCODE *), *after(PCODE *);
void fixprev(void), flushcode(void), codefnreset(void), dropinstr(PCODE *),
    swappseudo(PCODE *, PCODE *);
int codcreg(VREG *, VREG *), immedop(int op);
void code0(int, VREG *, VREG *);
void coderetmove(VREG *);
void codek0(int, VREG *, VREG *);
void code00(int, int, int);		/* Reg linkage, was static */
void code1(int, VREG *, INT);
void code3(int, VREG *, SYMBOL *);
void code4(int, VREG *, VREG *);
void code40(int op, int r, int s, INT bsiz);/* Reg linkage, was static */
void code4s(int, VREG *, VREG *, int, INT);
void code5(int, VREG *);
void code6(int, VREG *, SYMBOL *);
void code8(int, VREG *, INT);
void code9(int, VREG *, double, int);
void code10(int, VREG *, SYMBOL *, INT, INT);
void code12(int, VREG *, INT);
void code13(int, VREG *, INT);
void code14(int, VREG *, int);
void code15(int, SYMBOL *, INT, VREG *);
void code16(int, VREG *, SYMBOL *, VREG *);
void code17(INT);
void codebp(int, int, INT, int, SYMBOL *, INT);

void codek4(int, VREG *, VREG *);
void codemdx(int, int, SYMBOL *, INT, int);
void codestr(char *, int);
void codgolab(SYMBOL *);
void codlabel(SYMBOL *);
void codr1(int, int, INT);
void codr1_force(int, int, INT);
void codr10(int, int, SYMBOL *, INT, INT);

#if SYS_CSI
void code4m(int op, VREG *reg, VREG *idx, char *mnem),
	code5m(int, VREG *, char *);
#endif

PCODE *chkmref(PCODE *begp, PCODE *endp, INT *aoff);

/* Internal functions */
static void codrmdx(int, int, int, SYMBOL *, INT, int);
static void codr8(int, int, INT);
static void flsprev(void);
static PCODE *newcode(int, int, int);
static void codrrx(PCODE *, PCODE *);
static int rrpre1(PCODE *), rrpre2(PCODE *, PCODE *);
static void rrpre3(PCODE *, PCODE *, int), rrpopt(PCODE *), rrpop2(PCODE *);
static PCODE *chkref(PCODE *, PCODE *, int);
static void foldxref(PCODE *);
static void optjrst(PCODE *p);
static void foldshiftmask(void);

#if DEBUG_KCC		/* 5/91 KCC size */
/* PEEPHOLER DEBUGGING ROUTINES */

/*	SHOALL and SHOCURCOD are intended to be called directly by
**	IDDT during debugging, e.g. as in PUSHJ 17,SHOALL$X
*/
#if __STDC__
static void shoall(void), shocurcod(void), shopcod(FILE *, PCODE *, INT),
	shoop(FILE *, unsigned), shohdr(char *, int, int), shocum(void),
	shocmp(FILE *);
#else
static void shoall(), shocurcod(), shopcod(), shoop(),
		shohdr(), shocum(), shocmp();
#endif

static void
shoall()
{
    INT i;
    for (i = mincode; i < maxcode; i++)
	shopcod(outmsgs, &codes[i&(MAXCODE-1)], i&(MAXCODE-1));
}

static void
shocurcod()
{
    shopcod(outmsgs, previous, codes-previous);
}


/* SHOHDR - Called at start of each CODEnn function.
**	Ensures consistent trace of all calls from CCGEN2 to CCCODE.
*/
static void
shohdr(loc, op, r)
char *loc;
int op, r;
{
    fprintf(fpho, "%s: %o=", loc, op);
    shoop(fpho, op);
    if (r)
	fprintf(fpho, " %o,", r);
    else
	fputc(' ', fpho);
}

/* SHOPCOD - Show one pseudo-code entry
**	Used as auxiliary for other debug printing routines, never called
**	directly by IDDT.
*/
static void
shopcod(f,p,idx)
FILE *f;
PCODE *p;
INT idx;
{
    unsigned int i;

    fprintf(f,"codes[%d] %6lo/ %4o %5o %2o %6o %6lo %2o %lo+%lo\n\t",
		idx, p, p->Ptype, p->Pop, p->Preg, p->Pptr,
		p->Poffset, p->Pindex, p->Pdouble1, p->Pdouble2);

    /* Output value of Pop field */
    shoop(f, p->Pop);

    /* Output value of Ptype field */
    fputs(" <", f);
    if (p->Ptype&PTF_IMM)
	fputs("Imm,", f);
    if (p->Ptype&PTF_SKIPPED)
	fputs("Skipped,", f);
    if (p->Ptype&PTF_IND)
	fputs("Indirect", f);
    fputs("> ", f);

    /* Now interpret the rest of the instruction according to mode */
    switch (i = (p->Ptype&PTF_ADRMODE))
	{
	case PTA_ONEREG:		/* no address, just register */
	    fprintf(f, "ONEREG R=%o,", p->Preg);
	    break;
	case PTA_REGIS:			/* register to register */
	    fprintf(f, "REGIS R=%o, R=%o", p->Preg, p->Pr2);
	    break;
	case PTA_MINDEXED:		/* addr+offset(index) */
	    fprintf(f, "MINDEXED R=%o, Addr=%s+%o(%o) siz %o", p->Preg,
		(p->Pptr ? p->Pptr->Sname : ""),
		p->Poffset, p->Pindex, p->Pbsize);
	    break;
	case PTA_BYTEPOINT:		/* [bsize,,addr+offset(index)] */
	    fprintf(f, "BYTEPOINT R=%o, BP=[%o,,%s+%o(%o)]", p->Preg,
		p->Pbsize,
		(p->Pptr ? p->Pptr->Sname : ""),
		p->Poffset,
		p->Pindex);
	    break;
	case PTA_PCONST:		/* [<pointer of addr+offset+bsize>] */
	    if (p->Pbsize)
		fprintf(f, "PCONST R=%o, bptr%d=[%s+%o]", p->Preg,
			p->Pbsize,
			(p->Pptr ? p->Pptr->Sname : ""),
			p->Poffset);
	    else
		fprintf(f, "PCONST R=%o, Wptr=[%s+%o]", p->Preg,
			(p->Pptr ? p->Pptr->Sname : ""),
			p->Poffset);
	    break;
	case PTA_RCONST:		/* Simple integer in pvalue */
	    fprintf(f, "RCONST R=%o, Constant=%o", p->Preg, p->Pvalue);
	    break;
	case PTA_FCONST:		/* [single-prec float] */
	    fprintf(f, "FCONST R=%o, Fltcon=%g", p->Preg, p->Pfloat);
	    break;
	case PTA_DCONST:		/* [double-prec float] */
	    fprintf(f, "DCONST R=%o, Dblcon=%g", p->Preg, p->Pdouble);
	    break;
	case PTA_DCONST1:		/* [1st wd of double float] */
	    fprintf(f, "DCONST1 R=%o, 1st wd of dblcon=%g", p->Preg, p->Pdouble);
	    break;
	case PTA_DCONST2:		/* [2nd wd of double float] */
	    fprintf(f, "DCONST2 R=%o, 2nd wd of dblcon=%g", p->Preg, p->Pdouble);
	    break;
	default:
	    fprintf(f, "Illegal ADRMODE value = %o", i);
	}
    fputc('\n', f);
}

/* SHOOP - Auxiliary for above two routines, just outputs string for opcode.
*/
static void
shoop(f, op)
FILE *f;
unsigned op;
{
    /* Test conditions, indexed by POF_OPSKIP field */
    static char *shoskt[] = { "", "A", "E", "N", "L", "GE", "G", "LE" };

    fputs(popostr[op&POF_OPCODE], f);
    if (op&(~POF_OPCODE))
	{
	if (op&POF_ISSKIP)
	    fputs("+skp", f);
	if (op&POF_OPSKIP)
	    {
	    fputc('+', f);
	    fputs(shoskt[(op&POF_OPSKIP)>>POF_OPSKIP_SHF], f);
	    }
	if (op&POF_BOTH)
	    fputs("+B", f);
	}
}

/* More debug stuff.  Pcode buffer compare, to show what's changed. */

static INT cmpcnt = 0;		/* Count of times compare done */
static PCODE *oldcodes = NULL;
static INT omaxcode = 0, omincode = 0;

static void
shocum()
{
    fprintf(fpho, "----------- Update %3d -----------------\n", cmpcnt++);
    shocmp(fpho);
    fprintf(fpho, "----------------------------------------\n");
}

/* SHOCMP - Output a buffer comparison */
static void
shocmp(f)
FILE *f;
{
    PCODE *p, *q;
    INT i, lim;

    if (oldcodes == NULL)	/* Ensure old buffer copy exists */
	if ((oldcodes = (PCODE *)calloc(sizeof(codes), 1)) == NULL)
	    {
	    error("No memory for pcode buffer");
	    return;
	    }

    /* First check for new code prior to last stuff */
    if (mincode < omincode)
	{
	fprintf(f,"    NEW---- stuff prior to start of last check:\n");
	for (i = mincode; i < omincode; ++i)
	    {
	    fprintf(f, "    NEW ");
	    shopcod(f, &codes[i&(MAXCODE-1)], i&(MAXCODE-1));
	    }
	}
    /* Then check for old code prior to current new start */
    else if (omincode < mincode)
	{
	fprintf(f,"    OLD---- stuff flushed from start of current code:\n");
	for (i = omincode; i < mincode; ++i)
	    {
	    fprintf(f, "    OLD ");
	    shopcod(f, &oldcodes[i&(MAXCODE-1)], i&(MAXCODE-1));
	    }
	}

    /* Now compare stuff that's in both buffers */
    lim = (maxcode < omaxcode) ? maxcode : omaxcode;	/* Find smallest */
    for (i = mincode; i < lim; ++i)
	{
	p = &codes[i&(MAXCODE-1)];
	q = &oldcodes[i&(MAXCODE-1)];
	if (memcmp((char *)p, (char *)q, sizeof(PCODE)) == 0)
	    continue;			/* Compared OK, keep going */
	fprintf(f, "    ----Changed\n    OLD ");
	shopcod(f, q, i&(MAXCODE-1));
	fprintf(f, "    ----to\n    NEW ");
	shopcod(f, p, i&(MAXCODE-1));
	fprintf(f, "    -----------\n");
	}

    /* Now check for new code added after last stuff */
    if (maxcode > omaxcode)
	{
	fprintf(f,"    ADD---- New stuff:\n");
	for (i = omaxcode; i < maxcode; ++i)
	    {
	    fprintf(f, "    ADD ");
	    shopcod(f, &codes[i&(MAXCODE-1)], i&(MAXCODE-1));
	    }
	}
    /* Then check for old code still existing at end */
    else if (maxcode < omaxcode)
	{
	fprintf(f,"    OLD---- stuff flushed from end:\n");
	for (i = maxcode; i < omaxcode; ++i)
	    {
	    fprintf(f, "    OLD ");
	    shopcod(f, &oldcodes[i&(MAXCODE-1)], i&(MAXCODE-1));
	    }
	}

    /* Now update our copy in preparation for next check */
#if __STDC__
    memcpy((void *)oldcodes, (void *)codes, sizeof(codes));
#else
    memcpy((char *)oldcodes, (char *)codes, sizeof(codes));
#endif
    omincode = mincode;
    omaxcode = maxcode;
}
#endif

/* BASIC PSEUDO-CODE AUXILIARIES */

/* NEWCODE - Get a new pseudo-code record.
**	Sets "previous" to the returned pointer, for global access.
*/
static int prvskip = 0;	/* Used by newcode/flushcode to remember if skipping */

/* CODEFNRESET - Reset peephole state which must not cross functions. */
void
codefnreset(void)
{
    prvskip = 0;
}

#define OVERINC	20		/* Number of ops to force out on overflow */

static PCODE *
newcode(int type, int op, int reg)
{
    PCODE *p;
    int i;
    int skipped;

    /* fixprev(); */		/* Don't need if all other code is careful */
    if (previous)
	skipped = isskip(previous->Pop)		/* If prev was skip, */
			? PTF_SKIPPED : 0;	/* say new instr is skipped! */
    else
	skipped = prvskip;			/* No prev, ask flushcode */

    p = &codes[maxcode++ & (MAXCODE - 1)];	/* Get ptr to new instr */
    if (maxcode >= mincode + MAXCODE - 1)	/* Overflow?  If so, */
	for (i = 0; i < OVERINC; i++)		/* force some instrs out */
	    realcode(&codes[(mincode++)&(MAXCODE-1)]);

    p->Ptype = type | skipped;
    p->Pop = op;
    p->Preg = reg;
    return previous = p;		/* Set "previous" to new instr */
}

/* BEFORE(p) - Return live instruction preceding this one.
** AFTER(p)  - Return live instruction succeeding this one.
*/
PCODE *
before(struct pcode * p)
{
    PCODE *b;

    if (p == NULL)
	return NULL;		/* make sure we have a real pseudo */
    b = &codes[mincode&(MAXCODE-1)];
    while (1)
	{
	if (p == b)
	    break;	/* start of buffer, can't back up */
	--p;				/* back before here */
	if (p < &codes[0])
	    p = &codes[MAXCODE-1]; /* wrap in circular buffer */
	if (p->Pop != P_NOP)
	    return p;	/* got a real op, return with it */
	}
    return NULL;
}

PCODE *
after(struct pcode * p)
{
    PCODE *b = &codes[maxcode&(MAXCODE-1)];

    if (p == NULL)
	return NULL;		/* make sure we have a real pseudo */
    while (1)
	{
	if (++p > &codes[MAXCODE-1])
	    p = &codes[0]; /* wrap */
	if (p == b)
	    break;	/* end of buffer, no more code */
	if (p->Pop != P_NOP)
	    return p;	/* got a real op, return with it */
	}
    return NULL;
}

/* SWAPPSEUDO(a,b) - swap two pseudo code locations
*/

void
swappseudo(struct pcode * a, struct pcode * b)
{
    PCODE temp;

    temp = *b;		/* Copy the structure to temp place */
    *b = *a;
    *a = temp;
}

/* FIXPREV() - Make sure "previous" points to something.
**
** This should be called after a pseudo-code which might be the last
** in the buffer is P_NOPed out.  It sets previous to the new last
** instruction in the peephole buffer.
**
** Someday this might also change maxcode to save buffer space.
*/
void
fixprev(void)
{
    if (previous && previous->Pop == P_NOP)
	{
	previous = before(previous);
	--maxcode;	/* We know at least one flushed, but */
			/* not sure how far back.  1 is better than nothing. */
	}
}

/* FOLDSHIFTMASK - Drop an ANDI made redundant by a logical right shift.
**
** After LSH R,-N, only the low 36-N bits can be nonzero.  If a following
** ANDI keeps all of those bits, it cannot change the value.  This helper is
** called after register CSE because that is where an AND against a constant
** register becomes the final immediate form.
*/
static void
foldshiftmask(void)
{
    PCODE *a, *sh;
    INT nbits, lowmask;

    a = previous;
    if (!optobj || a == NULL || a->Pop != P_AND
      || a->Ptype != PTV_IMMED || prevskips(a))
	return;
    sh = before(a);
    if (sh == NULL || sh->Pop != P_LSH || sh->Preg != a->Preg
      || (sh->Ptype & PTF_ADRMODE) != PTA_RCONST
      || sh->Pvalue > -18 || sh->Pvalue < -35 || prevskips(sh))
	return;

    nbits = 36 + sh->Pvalue;
    lowmask = ((INT)1 << nbits) - 1;
    if ((a->Pvalue & lowmask) == lowmask) {
	dropinstr(a);
	fixprev();
    }
}

/* DROPINSTR(p) - Flush pseudo-code instruction (make it a NOP)
** FLSPREV() - Flush instruction that "previous" points to.
*/
void
dropinstr(struct pcode * p)
{
    if (p)
	{
	p->Pop = P_NOP;
	fixprev();	/* Fix up in case "previous" is now (or was) a NOP */
	}
}
static void
flsprev(void)
{
    dropinstr(previous);
}

/* REG_LIVE_BEFORE_CHANGE - Check whether REG is read before it is replaced.
** The scan is bounded by the current peephole buffer and carries no state
** between calls.
*/
static int
reg_live_before_change(int start, int reg)
{
    int j;
    PCODE *p;

    for (j = start; j < maxcode; ++j) {
        p = &codes[j & (MAXCODE-1)];
        if (p->Pop == P_NOP)
            continue;
        if (rruse(p, reg))
            return 1;
        if (rrchg(p, reg))
            break;
    }
    return 0;
}

/* FOLD_INDEX_DISPLACEMENT - Keep a constant index adjustment in the final EA.
** Returns 1 for the three-instruction spelling, 2 for the spelling with an
** inserted register copy, and 0 when no fold applies.
*/
static int
fold_index_displacement(int i, PCODE *p, PCODE *q)
{
    PCODE *r3, *r4;

    if (i + 2 < maxcode) {
        r3 = &codes[(i+2) & (MAXCODE-1)];
        if (p->Pop == P_ADD && p->Ptype == PTV_IMMED
          && q->Pop == P_ADD && q->Preg == p->Preg
          && !prevskips(p) && !prevskips(q)
          && !rinaddr(q, p->Preg)
          && r3->Pop == P_MOVE && !prevskips(r3)
          && (r3->Ptype & ~PTF_SKIPPED) == PTA_MINDEXED
          && r3->Pptr == NULL && r3->Pindex == p->Preg) {
            r3->Poffset += p->Pvalue;
            p->Pop = P_NOP;
            return 1;
        }
    }

    if (i + 3 < maxcode) {
        r3 = &codes[(i+2) & (MAXCODE-1)];
        r4 = &codes[(i+3) & (MAXCODE-1)];
        if (p->Pop == P_ADD && p->Ptype == PTV_IMMED
          && q->Pop == P_MOVE && !prevskips(p) && !prevskips(q)
          && !rincode(q, p->Preg)
          && r3->Pop == P_ADD && r3->Preg == p->Preg
          && (r3->Ptype & ~PTF_SKIPPED) == PTA_REGIS
          && r3->Pr2 == q->Preg && !prevskips(r3)
          && r4->Pop == P_MOVE && !prevskips(r4)
          && (r4->Ptype & ~PTF_SKIPPED) == PTA_MINDEXED
          && r4->Pptr == NULL && r4->Pindex == p->Preg) {
            r4->Poffset += p->Pvalue;
            p->Pop = P_NOP;
            return 2;
        }
    }
    return 0;
}

/* FOLD_MASK_ZERO_TEST - Replace MOVE/ANDI/CAI-zero with a direct TRN/TLN. */
static int
fold_mask_zero_test(int i, PCODE *p, PCODE *q)
{
    PCODE *r3;

    if (i + 2 >= maxcode)
        return 0;
    r3 = &codes[(i+2) & (MAXCODE-1)];
    if (p->Pop != P_MOVE || p->Ptype != PTA_REGIS
      || q->Pop != P_AND || q->Ptype != PTV_IMMED
      || q->Preg != p->Preg || prevskips(p) || prevskips(q)
      || (r3->Pop & POF_OPCODE) != P_CAI
      || (r3->Pop & POF_OPSKIP) == 0
      || (r3->Ptype & PTF_ADRMODE) != PTA_RCONST
      || r3->Pvalue != 0 || r3->Preg != p->Preg || prevskips(r3)
      || ((r3->Pop & POF_OPSKIP) != POS_SKPE
       && (r3->Pop & POF_OPSKIP) != POS_SKPN))
        return 0;

    q->Pop = r3->Pop ^ (P_CAI ^ P_TRN);
    q->Preg = p->Pr2;
    q->Ptype = PTA_RCONST;
    p->Pop = P_NOP;
    r3->Pop = P_NOP;
    return 1;
}

/* FLUSHCODE() - Flush peephole buffer (emit everything)
*/
void
flushcode(void)
{
    int i;
    PCODE *p, *q;

    if (mincode < maxcode)
	{
	/* First discard exact local no-ops which may have been created by
	** earlier peephole rewrites.  CODE00 rejects MOVE R,R when it is
	** first emitted, but register retargeting can create one later.
	** Keep an instruction which is the target of a preceding skip: the
	** physical instruction slot is then part of the control flow.
	*/
	if (optobj)
	    for (i = mincode; i < maxcode; ++i)
		{
		p = &codes[i & (MAXCODE-1)];
		if ((p->Pop == P_MOVE || p->Pop == P_DMOVE)
		  && (p->Ptype & ~PTF_SKIPPED) == PTA_REGIS
		  && p->Preg == p->Pr2 && !prevskips(p))
		    p->Pop = P_NOP;
		else if ((p->Pop == P_ADD || p->Pop == P_SUB
		       || p->Pop == P_IOR || p->Pop == P_XOR)
		  && (p->Ptype & ~PTF_SKIPPED) == PTV_IMMED
		  && p->Pvalue == 0 && !prevskips(p))
		    p->Pop = P_NOP;
		}

	/* Late halfword fold.  A full-word AND which keeps only the left
	** half is emitted as TRZ by CCOUT, too late for the ordinary
	** peephole passes to combine it with the preceding MOVE.  Do the
	** exact pair fold here while the final pseudo-code stream is still
	** available.
	*/
	if (optobj)
	    for (i = mincode; i + 1 < maxcode; ++i)
		{
		p = &codes[i & (MAXCODE-1)];
		q = &codes[(i+1) & (MAXCODE-1)];

		/* Idempotent bit clearing is sometimes requested independently by
		** two DImode normalization paths.  An immediately repeated TLZ on
		** the same AC with the same mask cannot change the value again.
		*/
		if (p->Pop == P_TLZ && q->Pop == P_TLZ
		  && (p->Ptype & ~PTF_SKIPPED) == PTA_RCONST
		  && (q->Ptype & ~PTF_SKIPPED) == PTA_RCONST
		  && p->Preg == q->Preg && p->Pvalue == q->Pvalue
		  && !prevskips(p) && !prevskips(q))
		    {
		    q->Pop = P_NOP;
		    ++i;
		    continue;
		    }

		/* Applying the exact signed-narrow extension idiom twice is also
		** idempotent.  This is intentionally an exact six-instruction
		** match: it needs no value tracking and cannot cross control flow.
		**
		**   TRNE R,sign       TRNE R,sign
		**    TDOA R,[-width]   TDOA R,[-width]
		**    ANDI R,width-1    ANDI R,width-1
		*/
		if (i + 5 < maxcode && !prevskips(p))
		    {
		    PCODE *a2, *a3, *b1, *b2, *b3;

		    a2 = q;
		    a3 = &codes[(i+2) & (MAXCODE-1)];
		    b1 = &codes[(i+3) & (MAXCODE-1)];
		    b2 = &codes[(i+4) & (MAXCODE-1)];
		    b3 = &codes[(i+5) & (MAXCODE-1)];
		    if (p->Pop == P_TRN+POF_ISSKIP+POS_SKPE
		      && a2->Pop == P_TRO+POF_ISSKIP+POS_SKPA
		      && a3->Pop == P_AND
		      && b1->Pop == p->Pop && b2->Pop == a2->Pop
		      && b3->Pop == a3->Pop
		      && (p->Ptype & ~PTF_SKIPPED) == PTA_RCONST
		      && (a2->Ptype & ~PTF_SKIPPED) == PTA_RCONST
		      && (a3->Ptype & ~PTF_SKIPPED) == PTV_IMMED
		      && (b1->Ptype & ~PTF_SKIPPED) == PTA_RCONST
		      && (b2->Ptype & ~PTF_SKIPPED) == PTA_RCONST
		      && (b3->Ptype & ~PTF_SKIPPED) == PTV_IMMED
		      && p->Preg == a2->Preg && p->Preg == a3->Preg
		      && p->Preg == b1->Preg && p->Preg == b2->Preg
		      && p->Preg == b3->Preg
		      && p->Pvalue == b1->Pvalue
		      && a2->Pvalue == b2->Pvalue
		      && a3->Pvalue == b3->Pvalue)
			{
			b1->Pop = P_NOP;
			b2->Pop = P_NOP;
			b3->Pop = P_NOP;
			}
		    }
	if (q->Pop == P_JRST)
	    optjrst(q);
		if (p->Pop == P_MOVE && q->Pop == P_AND
		  && p->Preg == q->Preg && !prevskips(p) && !prevskips(q)
		  && q->Ptype == (PTA_RCONST+PTF_IMM)
		  && q->Pvalue == ((INT)0777777000000))
		    {
		    p->Pop = P_HLLZ;
		    q->Pop = P_NOP;
		    ++i;
		    continue;
		    }


        /* Eliminate a dead register-copy temporary used only as the source
        ** operand of the immediately following arithmetic/logical compare.
        **
        **     MOVE T,S
        **     OP   R,T
        **
        ** becomes
        **
        **     OP   R,S
        **
        ** Do not do this when T is also R: in that case the MOVE replaces
        ** the old destination value which OP may need.  This late fold is
        ** deliberately adjacency-only and therefore needs no liveness state.
        */
        if (p->Pop == P_MOVE && p->Ptype == PTA_REGIS
          && q->Ptype == PTA_REGIS && q->Pr2 == p->Preg
          && q->Preg != p->Preg && !prevskips(p) && !prevskips(q))
            {
            switch (q->Pop & POF_OPCODE) {
            case P_ADD:
            case P_SUB:
            case P_IMUL:
            case P_AND:
            case P_IOR:
            case P_XOR:
            case P_CAM:
                q->Pr2 = p->Pr2;
                p->Pop = P_NOP;
                ++i;
                continue;
            default:
                ;
            }
            }

        /* A copied narrow signed value is commonly widened as
        **
        **     MOVE  T,S
        **     TRNE  T,sign
        **      TDOA T,[-width]
        **      ANDI T,width-1
        **
        ** T remains live after the test, so the test-only copy fold below
        ** must not discard the MOVE.  Replace the whole idiom instead with
        ** an equally short, branchless sign extension.
        */
        if (i + 3 < maxcode && p->Pop == P_MOVE && p->Ptype == PTA_REGIS
          && q->Pop == P_TRN+POF_ISSKIP+POS_SKPE
          && q->Ptype == PTA_RCONST && q->Preg == p->Preg
          && q->Pvalue > 0 && (q->Pvalue & (q->Pvalue - 1)) == 0
          && !prevskips(p) && !prevskips(q)) {
            PCODE *r3, *r4;
            INT sign;
            int shift;

            r3 = &codes[(i+2) & (MAXCODE-1)];
            r4 = &codes[(i+3) & (MAXCODE-1)];
            sign = q->Pvalue;
            if (r3->Pop == P_TRO+POF_ISSKIP+POS_SKPA
              && (r3->Ptype & ~PTF_SKIPPED) == PTA_RCONST
              && r3->Preg == p->Preg && r3->Pvalue == -(sign << 1)
              && r4->Pop == P_AND
              && (r4->Ptype & ~PTF_SKIPPED) == PTV_IMMED
              && r4->Preg == p->Preg && r4->Pvalue == (sign << 1) - 1) {
                shift = 35;
                while (sign > 1) {
                    sign >>= 1;
                    --shift;
                }
                q->Pop = P_LSH;
                q->Ptype = PTA_RCONST;
                q->Pvalue = shift;
                r3->Pop = P_ASH;
                r3->Ptype = PTA_RCONST;
                r3->Pvalue = -shift;
                r4->Pop = P_NOP;
                i += 2;
                continue;
            }
        }

        /* Test instructions do not alter their AC operand.  A temporary
        ** copy used only to feed TRN/TLN can therefore be removed outright.
        ** Keep the copy when its AC is read again before being replaced.
        **
        **     MOVE T,S
        **     TRNx T,M       -> TRNx S,M
        */
        if (p->Pop == P_MOVE && p->Ptype == PTA_REGIS
          && !prevskips(p) && !prevskips(q)
          && ((q->Pop & POF_OPCODE) == P_TRN
           || (q->Pop & POF_OPCODE) == P_TLN)
          && q->Preg == p->Preg
          && !reg_live_before_change(i + 2, p->Preg)) {
            q->Preg = p->Pr2;
            p->Pop = P_NOP;
            ++i;
            continue;
        }

        /* Eliminate a constant temporary used only by the immediately
        ** following operation.  KCC represents the resulting immediate
        ** arithmetic/logical form by keeping the opcode and setting PTF_IMM.
        **
        **     MOVEI T,C
        **     OP    R,T
        **
        ** becomes OP+I R,C.
        */
        if (p->Pop == P_MOVE && p->Ptype == PTV_IMMED
          && q->Ptype == PTA_REGIS && q->Pr2 == p->Preg
          && q->Preg != p->Preg && !prevskips(p) && !prevskips(q))
            {
            switch (q->Pop & POF_OPCODE) {
            case P_ADD:
            case P_SUB:
            case P_IMUL:
            case P_AND:
            case P_IOR:
            case P_XOR:
                q->Ptype = PTV_IMMED;
                q->Pvalue = p->Pvalue;
                p->Pop = P_NOP;
                ++i;
                continue;
            default:
                ;
            }
            }

        /* Keep an index displacement in the final effective address. */
        {
            int folded;

            folded = fold_index_displacement(i, p, q);
            if (folded) {
                if (folded == 1)
                    ++i;
                continue;
            }
        }

        /* A copied, masked value tested only for zero can use TRN/TLN. */
        if (fold_mask_zero_test(i, p, q)) {
            ++i;
            continue;
        }

        /* SETZB/SETOB already write the generated value to both the AC and
        ** memory.  If the next instruction merely returns/copies that same
        ** known value, put the BOTH result directly in the final AC.
        */
        if ((p->Pop == (P_SETZ + POF_BOTH)
          || p->Pop == (P_SETO + POF_BOTH))
          && !prevskips(p) && !prevskips(q))
            {
            if (q->Pop == P_MOVE && q->Ptype == PTA_REGIS
              && q->Pr2 == p->Preg) {
                p->Preg = q->Preg;
                q->Pop = P_NOP;
                ++i;
                continue;
            }
            if (((p->Pop & POF_OPCODE) == P_SETZ && q->Pop == P_SETZ)
              || ((p->Pop & POF_OPCODE) == P_SETO && q->Pop == P_SETO)) {
                if (q->Ptype == PTA_ONEREG) {
                    p->Preg = q->Preg;
                    q->Pop = P_NOP;
                    ++i;
                    continue;
                }
            }
            }

        /* Fold a memory fetch consumed only by a following complement.
        ** SETCM accepts the same ordinary effective-address forms as MOVE.
        **
        **     MOVE  T,M
        **     SETCM R,T
        **
        ** becomes SETCM R,M.
        */
        if (p->Pop == P_MOVE && !prevskips(p) && !prevskips(q)
          && q->Pop == P_SETCM && q->Ptype == PTA_REGIS
          && q->Pr2 == p->Preg
          && ((p->Ptype & PTF_ADRMODE) == PTA_MINDEXED
           || (p->Ptype & PTF_ADRMODE) == PTA_RCONST))
            {
            q->Ptype = p->Ptype;
            q->Pptr = p->Pptr;
            q->Poffset = p->Poffset;
            q->Pindex = p->Pindex;
            q->Pbsize = p->Pbsize;
            p->Pop = P_NOP;
            ++i;
            continue;
            }

        /* A copied address used only for the first half of a two-word
        ** stack spill is redundant.
        **
        **     SETM  B,A
        **     PUSH  SP,0(B)
        **     PUSH  SP,1(A)
        **
        ** becomes two PUSHes indexed by A.  Keep the copy if B remains
        ** live after the pair.
        */
        if (i + 2 < maxcode && p->Pop == P_SETM
          && p->Ptype == PTA_REGIS && !prevskips(p) && !prevskips(q)) {
            PCODE *r3;

            r3 = &codes[(i+2) & (MAXCODE-1)];
            if (q->Pop == P_PUSH && q->Ptype == PTA_MINDEXED
              && q->Preg == R_SP && q->Pptr == NULL
              && q->Pindex == p->Preg
              && r3->Pop == P_PUSH && r3->Ptype == PTA_MINDEXED
              && r3->Preg == R_SP && r3->Pptr == NULL
              && r3->Pindex == p->Pr2
              && r3->Poffset == q->Poffset + 1
              && !prevskips(r3)
              && !reg_live_before_change(i + 3, p->Preg)) {
                q->Pindex = p->Pr2;
                p->Pop = P_NOP;
                i += 2;
                continue;
            }
        }

        /* A copied address used only to load the two halves of one
        ** double word does not need a second index AC.
        **
        **     SETM  B,A
        **     MOVE  R,0(B)
        **     MOVE  R+1,1(A)
        **
        ** becomes DMOVE R,0(A).  Keep the copy if B remains live.
        */
        if (i + 2 < maxcode && p->Pop == P_SETM
          && p->Ptype == PTA_REGIS && !prevskips(p) && !prevskips(q)) {
            PCODE *r3;

            r3 = &codes[(i+2) & (MAXCODE-1)];
            if (q->Pop == P_MOVE && q->Ptype == PTA_MINDEXED
              && q->Pptr == NULL && q->Pindex == p->Preg
              && r3->Pop == P_MOVE && r3->Ptype == PTA_MINDEXED
              && r3->Pptr == NULL && r3->Pindex == p->Pr2
              && r3->Preg == q->Preg + 1
              && r3->Poffset == q->Poffset + 1
              && !prevskips(r3)
              && !reg_live_before_change(i + 3, p->Preg)) {
                q->Pop = P_DMOVE;
                q->Pindex = p->Pr2;
                p->Pop = P_NOP;
                r3->Pop = P_NOP;
                i += 2;
                continue;
            }
        }

        /* Adjacent register copies form one double-word copy.  The target
        ** expansion already handles overlapping pairs on machines without
        ** native DMOVE, so this does not require extra allocation state.
        */
        if (tgmachuse.dmovx
          && p->Pop == P_MOVE && q->Pop == P_MOVE
          && !prevskips(p) && !prevskips(q)
          && p->Ptype == PTA_REGIS && q->Ptype == PTA_REGIS
          && q->Preg == p->Preg + 1 && q->Pr2 == p->Pr2 + 1) {
            p->Pop = P_DMOVE;
            q->Pop = P_NOP;
            ++i;
            continue;
        }

        /* A reverse-ordered adjacent load can also use native DMOVE.
        ** Keep this target-specific: on PDP-6/KA10, preserving the scalar
        ** order gives the older peephole passes more useful opportunities.
        */
        if (tgmachuse.dmovx
          && p->Pop == P_MOVE && q->Pop == P_MOVE
          && !prevskips(p) && !prevskips(q)
          && p->Ptype == PTA_MINDEXED && q->Ptype == PTA_MINDEXED
          && p->Preg == q->Preg + 1 && p->Pptr == q->Pptr
          && p->Pindex == q->Pindex && p->Poffset == q->Poffset + 1
          && q->Pindex != q->Preg && q->Pindex != p->Preg) {
            q->Pop = P_DMOVE;
            p->Pop = P_NOP;
            ++i;
            continue;
        }

		/* Adjacent full-word pairs are native double-word transfers.
		** On PDP-6/KA10 CCOUT expands these back into two ordered MOVEs;
		** KI10 and later can use DMOVE/DMOVEM directly.
		*/
		if ((p->Pop == P_MOVE || p->Pop == P_MOVEM)
		  && q->Pop == p->Pop && !prevskips(p) && !prevskips(q)
		  && p->Ptype == PTA_MINDEXED && q->Ptype == PTA_MINDEXED
		  && q->Preg == p->Preg + 1 && p->Pptr == q->Pptr
		  && p->Pindex == q->Pindex && q->Poffset == p->Poffset + 1)
		    {
		    p->Pop = (p->Pop == P_MOVE) ? P_DMOVE : P_DMOVEM;
		    q->Pop = P_NOP;
		    ++i;
		    continue;
		    }

		/* Reuse an immediately preceding full-word load of the same
		** address.  Volatile accesses flush the peephole buffer, so they
		** cannot reach this fold.
		*/
		if (p->Pop == P_MOVE && p->Ptype == PTA_MINDEXED
		  && q->Ptype == PTA_MINDEXED && sameaddr(p, q, 0)
		  && !prevskips(p) && !prevskips(q))
		    {
		    switch (q->Pop) {
		    case P_MOVE:
		    case P_ADD:
		    case P_SUB:
		    case P_IMUL:
		    case P_AND:
		    case P_IOR:
		    case P_XOR:
			q->Ptype = PTA_REGIS;
			q->Pr2 = p->Preg;
			break;
		    default:
			;
		    }
		    }

		/* MOVE T,R followed by MOVEM T,M can store R directly.
		** Effective-address evaluation precedes the store, so this remains
		** valid even when R participates in M's address.
		*/
		if (p->Pop == P_MOVE && p->Ptype == PTA_REGIS
		  && q->Pop == P_MOVEM && q->Preg == p->Preg
		  && !prevskips(p) && !prevskips(q))
		    {
            /* Keep a copy whose value is still read before replacement. */
            if (!reg_live_before_change(i + 2, p->Preg)) {
                q->Preg = p->Pr2;
                p->Pop = P_NOP;
                ++i;
                continue;
            }
		    }

		/* AOS/SOS T,M followed by MOVE R,T can write R directly.
		** AOS/SOS do not consume the old value of their AC field, so this
		** is safe even when R participates in the effective address.
		*/
		if ((p->Pop == P_AOS || p->Pop == P_SOS)
		  && q->Pop == P_MOVE && q->Ptype == PTA_REGIS
		  && q->Pr2 == p->Preg && !prevskips(p) && !prevskips(q))
		    {
		    p->Preg = q->Preg;
		    q->Pop = P_NOP;
		    ++i;
		    continue;
		    }

		/* MOVE/HRRZ R,X followed by LSH R,18 is exactly HRLZ R,X.
		** Do this only here, after the ordinary halfword-store peepholes
		** have finished, so it cannot hide an HRLM opportunity.
		*/
		if ((p->Pop == P_MOVE || p->Pop == P_HRRZ)
		  && q->Pop == P_LSH && p->Preg == q->Preg
		  && !prevskips(p) && !prevskips(q)
		  && q->Ptype == PTA_RCONST && q->Pvalue == 022)
		    {
		    p->Pop = P_HRLZ;
		    q->Pop = P_NOP;
		    ++i;
		    }
		}

	/* Some of the final folds above can themselves create exact no-ops.
	** Sweep once more immediately before output.  This remains bounded by
	** the existing peephole buffer and carries no state between flushes.
	*/
	if (optobj)
	    for (i = mincode; i < maxcode; ++i)
		{
		p = &codes[i & (MAXCODE-1)];
		if ((p->Pop == P_MOVE || p->Pop == P_DMOVE)
		  && (p->Ptype & ~PTF_SKIPPED) == PTA_REGIS
		  && p->Preg == p->Pr2 && !prevskips(p))
		    p->Pop = P_NOP;
		else if ((p->Pop == P_ADD || p->Pop == P_SUB
		       || p->Pop == P_IOR || p->Pop == P_XOR)
		  && (p->Ptype & ~PTF_SKIPPED) == PTV_IMMED
		  && p->Pvalue == 0 && !prevskips(p))
		    p->Pop = P_NOP;
		}

	prvskip = (previous && isskip(previous->Pop))	/* If prev was skip, */
			? PTF_SKIPPED : 0;	/* remember fact for newcode */

	do
	    realcode(&codes[(mincode++)&(MAXCODE-1)]);
	while (mincode < maxcode)
	    ;

#if DEBUG_KCC		/* 5/91 KCC size */
	if (debpho)
	    {
	    fprintf(fpho,"FLUSHCODE:\n");
	    shocum();		/* Show stuff forced out. */
	    }
#endif
	}
    previous = NULL;
}

/* CODFALLTHROUGH - Drop only a trailing direct jump to the label that the
** caller is about to emit immediately.  Unlike OPTLAB this performs no label
** or fan-in restructuring; it is used for shared return epilogues whose
** incoming jumps have historically made the general label optimizer unsafe.
*/
void
codfallthrough(struct symbol * lab)
{
    PCODE *p;

    p = previous;
    if (!optobj || p == NULL || p->Pop != P_JRST
      || (p->Ptype & (PTF_SKIPPED | PTF_IND)) != 0
      || (p->Ptype & PTF_ADRMODE) != PTA_MINDEXED
      || p->Pptr != lab || p->Pindex != 0 || p->Poffset != 0)
        return;
    reflabel(lab, -1);
    dropinstr(p);
}

/* CODCREG - Change register.
**	This is a CODxxx routine both for consistency and so that
** PHO debugging information can be output if desired.
** Returns 1 if all relevant instances of the "from" register in the
** peephole buffer were successfully changed to the "to" register.
** Otherwise returns 0 and nothing was changed.
*/
int
codcreg(struct vreg * to, struct vreg * from)
{
    int ret;

    (void) vrstoreal(from, to);
    ret = changereg(vrreal(to), vrreal(from), previous);
#if DEBUG_KCC		/* 5/91 KCC size */
    if (ret && debpho)
	{
	fprintf(fpho, "CODCREG: to %o from %o\n", vrreal(to), vrreal(from));
	shocum();
	}
#endif
    return ret;
}

/* CODE0 - Generates instruction with register-only operands.
**	OP r1,r2
**
** CODE0 releases register r2 for future reassignment (although later
** peephole optimizations might notice that it hasn't been changed yet
** and re-use whatever value it contained).
**
** CODEK0 does NOT release register r2.  The assignment operators need to
** keep around two registers sometimes.
*/


void
codek0(int op, struct vreg * r1, struct vreg * r2)
{
    VREG *r3 = vrget();		/* Get another temp reg */
    int s = vrstoreal(r3, r2);		/* Ensure it and both regs real */
    int r = vrstoreal(r1, r2);

    vrfree(r3);
    code00(P_SETM, s, vrreal(r2));	/* Use this to avoid losing r2 */
    code00(op, r, s);
}
void
code0(int op, struct vreg * r1, struct vreg * r2)
{
    int s = vrstoreal(r2, r1);		/* Make regs real ones */
    if (r1 != r2)
	vrfree(r2);		/* flush operand for later */
    code00(op, vrreal(r1), s);
}

/* CODERETMOVE - Move a one-word result into AC1 at a return boundary.
** If vrstoreal() has just materialized the result temporary with
** MOVE temp,AC1, that temporary copy and the copy back to AC1 are both
** dead.  Drop the temporary MOVE instead of emitting MOVE AC1,temp.
** This deliberately applies only at the return boundary.
*/
void
coderetmove(struct vreg * r)
{
    int s;
    PCODE *p;

    s = vrstoreal(r, VR_RETVAL);
    if (r != VR_RETVAL)
        vrfree(r);
    p = previous;
    if (p != NULL
      && (p->Ptype & (PTF_ADRMODE | PTF_SKIPPED)) == PTA_REGIS
      && p->Pop == P_MOVE && p->Preg == s && p->Pr2 == R_RETVAL) {
        dropinstr(p);
        return;
    }
    if (s != R_RETVAL && changereg(R_RETVAL, s, p))
        return;
    code00(P_MOVE, vrreal(VR_RETVAL), s);
}

/*
** FOLDHALFSTORE - Fold a discarded packed-half update into a native
** halfword store.  This is deliberately called only when the C assignment
** result is discarded, so the full recombined word does not need to remain
** available in a register after the store.
**
** Recognized right-half form:
**     MOVE  D,M
**     ANDI/AND D,777777000000
**     ... produce S & 0777777 ...
**     IOR   D,S
**     MOVEM D,M
** becomes:
**     ... produce S & 0777777 ...
**     HRRM  S,M
**
** Recognized left-half form:
**     HRRZ  D,M
**     ... produce S & 0777777 ...
**     LSH   S,18
**     IOR   D,S
**     MOVEM D,M
** becomes HRLM using the unshifted halfword source when the source
** extraction is directly adjacent.
*/
void
foldhalfstore(void)
{
    PCODE *st, *ior, *srcop, *srcget, *keep, *load, *mask;
    int d, s;

    st = previous;
    if (!optobj || st == NULL || st->Pop != P_MOVEM
      || (st->Ptype & PTF_ADRMODE) != PTA_MINDEXED)
	return;

    ior = before(st);
    if (ior == NULL || ior->Pop != P_IOR
      || (ior->Ptype & PTF_ADRMODE) != PTA_REGIS)
	return;
    d = st->Preg;
    if (ior->Preg != d)
	return;
    s = ior->Pr2;

    /* Right-half update. */
    srcop = before(ior);

    /* The source may already have been extracted as a clean right half:
    **
    **     MOVE D,M          MOVE D,M
    **     TRZ  D,777777     AND  D,777777000000
    **     HRRZ S,N          HRRZ S,N
    **     IOR  D,S          IOR  D,S
    **     MOVEM D,M         MOVEM D,M
    **
    ** In either case HRRM can consume S directly.  This is a strict local
    ** fold: all participating instructions must be unskipped and the load
    ** and store must address the same word.
    */
    if (srcop != NULL && srcop->Pop == P_HRRZ && srcop->Preg == s
      && !prevskips(srcop)) {
        keep = before(srcop);
        mask = NULL;
        load = (keep != NULL) ? before(keep) : NULL;
        if (keep != NULL && keep->Preg == d && !prevskips(keep)) {
            if (keep->Pop == P_AND && keep->Ptype == PTA_REGIS) {
                mask = load;
                load = (mask != NULL) ? before(mask) : NULL;
                if (mask == NULL || mask->Pop != P_MOVE
                  || mask->Ptype != PTV_IMMED || mask->Preg != keep->Pr2
                  || mask->Pvalue != ((INT)0777777000000) || prevskips(mask))
                    load = NULL;
            } else if (!((keep->Pop == P_TRZ && keep->Ptype == PTA_RCONST
                         && keep->Pvalue == 0777777L)
                      || (keep->Pop == P_AND
                         && keep->Ptype == (PTA_RCONST+PTF_IMM)
                         && keep->Pvalue == ((INT)0777777000000))))
                load = NULL;
        } else
            load = NULL;
        if (load != NULL && load->Pop == P_MOVE && load->Preg == d
          && !prevskips(load) && sameaddr(load, st, 0)) {
            st->Pop = P_HRRM;
            st->Preg = s;
            dropinstr(ior);
            dropinstr(keep);
            dropinstr(load);
            return;
        }
    }
    if (srcop != NULL && srcop->Pop == P_HRRZ
      && (srcop->Ptype & PTF_ADRMODE) == PTA_REGIS
      && srcop->Preg == s && srcop->Pr2 == s)
	{
	srcget = before(srcop);
	keep = (srcget != NULL) ? before(srcget) : NULL;
	load = (keep != NULL) ? before(keep) : NULL;
	if (srcget != NULL && srcget->Pop == P_MOVE
	  && srcget->Preg == s
	  && (srcget->Ptype & PTF_ADRMODE) == PTA_REGIS
	  && keep != NULL && keep->Pop == P_AND && keep->Preg == d
	  && keep->Ptype == (PTA_RCONST+PTF_IMM)
	  && keep->Pvalue == ((INT)0777777000000)
	  && load != NULL && load->Pop == P_MOVE && load->Preg == d
	  && sameaddr(load, st, 0) && !prevskips(load))
	    {
	    st->Pop = P_HRRM;
	    st->Preg = s;
	    dropinstr(ior);
	    dropinstr(keep);
	    dropinstr(load);
	    return;
	    }
	}

    /* Left-half update with an independently produced source value:
    **
    **     HRRZ  D,M
    **     <produce S>
    **     LSH   S,18
    **     IOR   D,S
    **     MOVEM D,M
    **
    ** HRLM stores the right half of S into the left half of M, exactly
    ** the bits that LSH S,18 would contribute to D.  When the one source
    ** instruction between the old-half load and the shift does not use D,
    ** the old-half load, shift, and IOR are all dead.
    */
    if (srcop != NULL && srcop->Pop == P_LSH && srcop->Preg == s
      && (srcop->Ptype & PTF_ADRMODE) == PTA_RCONST
      && srcop->Pvalue == 18 && !prevskips(srcop)) {
        srcget = before(srcop);
        load = (srcget != NULL) ? before(srcget) : NULL;
        if (srcget != NULL && !prevskips(srcget)
          && load != NULL && load->Pop == P_HRRZ && load->Preg == d
          && !prevskips(load) && d != s && !rincode(srcget, d)
          && sameaddr(load, st, 0)) {
            st->Pop = P_HRLM;
            st->Preg = s;
            dropinstr(ior);
            dropinstr(srcop);
            dropinstr(load);
            return;
        }
    }

    /* Left-half update.  Require the canonical source sequence so the
    ** shift can be removed and HRLM can consume the original low half.
    */
    if (srcop != NULL && srcop->Pop == P_LSH && srcop->Preg == s
      && (srcop->Ptype & PTF_ADRMODE) == PTA_RCONST
      && srcop->Pvalue == 18)
	{
	srcget = before(srcop);
	if (srcget != NULL && srcget->Pop == P_HRRZ
	  && (srcget->Ptype & PTF_ADRMODE) == PTA_REGIS
	  && srcget->Preg == s && srcget->Pr2 == s)
	    {
	    PCODE *smove, *rload;
	    smove = before(srcget);
	    rload = (smove != NULL) ? before(smove) : NULL;
	    if (smove != NULL && smove->Pop == P_MOVE && smove->Preg == s
	      && (smove->Ptype & PTF_ADRMODE) == PTA_REGIS
	      && rload != NULL && rload->Pop == P_HRRZ && rload->Preg == d
	      && sameaddr(rload, st, 0) && !prevskips(rload))
		{
		st->Pop = P_HRLM;
		st->Preg = s;
		dropinstr(ior);
		dropinstr(srcop);
		dropinstr(rload);
		return;
		}
	    }
	}
}

void
code00(int op, int r, int s)
{
    PCODE *p, *prev;

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	shohdr("CODE0", op, r);
	fprintf(fpho, "%o\n", s);
	}
#endif


    if (op == P_MOVE && r == s)
	return;		/* Don't create useless "MOVE R,R" */

    if (Register_Nopreserve(s) && (op & POF_OPCODE) != P_IBP)
    /* Simple pre-optimization.  IBP modifies its second AC in place, so
    ** substituting a CSE register for that operand would move the side
    ** effect to the wrong physical register.
    */
	s = ufcreg(s);			/* Flush failed changereg for 2nd AC */
    if (Register_Nopreserve(r))
	if ((op & POF_OPCODE) == P_CAM)	/* and other if comparison */
	    r = ufcreg(r);

    /* A plain register copy followed immediately by the reverse copy leaves
    ** the second destination unchanged.  Do not fold across a skip, because
    ** the first MOVE might not execute.
    **
    **     MOVE S,R
    **     MOVE R,S     -> MOVE S,R
    */
    prev = previous;
    if (optobj && op == P_MOVE && prev != NULL
      && !prevskips(prev) && prev->Pop == P_MOVE
      && (prev->Ptype & ~PTF_SKIPPED) == PTA_REGIS
      && prev->Preg == s && prev->Pr2 == r)
	return;

    /* A register copy immediately overwritten by another register copy
    ** has no observable effect.  Keep skipped instructions intact because
    ** removing one would change the control-flow meaning of the skip.
    **
    **     MOVE R,S
    **     MOVE R,T     -> MOVE R,T
    */
    prev = previous;
    if (optobj && op == P_MOVE && prev != NULL
      && !prevskips(prev)
      && (prev->Ptype & ~PTF_SKIPPED) == PTA_REGIS
      && prev->Pop == P_MOVE && prev->Preg == r)
	dropinstr(prev);

    /* Now just add the instruction. */
    prev = previous;			/* Remember previous, if any */
    p = newcode(PTA_REGIS, op, r);
    p->Pr2 = s;

    /* Fold a value move followed by self negation.
    **
    **     MOVE  R,X
    **     MOVN  R,R     -> MOVN R,X
    **
    ** This also covers MOVEI/MOVNI.  MOVE and MOVN accept the same
    ** effective-address forms, so the transformation is exact.
    */
    if (optobj && op == P_MOVN && r == s && prev != NULL
      && !prevskips(prev) && prev->Pop == P_MOVE && prev->Preg == r)
        {
        prev->Pop = P_MOVN;
        dropinstr(p);
        return;
        }

    /* Fold a fetched word followed by a self halfword extraction.
    **
    **     MOVE  R,m
    **     HRRZ  R,R
    **
    ** becomes HRRZ R,m (and likewise for HLRZ).  Keep this deliberately
    ** local and restricted to ordinary memory addressing; it needs no
    ** persistent analysis state and is exact on every PDP-10 target.
    */
    if (optobj && r == s && (op == P_HRRZ || op == P_HLRZ)
      && prev != NULL && !prevskips(prev)
      && prev->Pop == P_MOVE && prev->Preg == r
      && ((prev->Ptype & PTF_ADRMODE) == PTA_MINDEXED
       || (prev->Ptype & PTF_ADRMODE) == PTA_REGIS))
	{
	prev->Pop = op;
	dropinstr(p);
	return;
	}

    /* Everything else is optimization hacks */

    /* Try to avoid storing a MOVE-type op by going back through
    ** the buffer and changing code so that the operand register (s)
    ** is already the destination (r).
    ** Neither pushneg nor pnegreg creates or kills any instructions,
    ** so pointers remain safe.
    */
    if (prev != NULL &&
	    Register_Nopreserve(p->Preg) && Register_Nopreserve(p->Pr2) &&
	    Register_Nopreserve(prev->Preg) && Register_Nopreserve(prev->Pr2))
	if (optobj)
	    switch (op)
		{
		case P_SETM:			/* Special code to avoid optimizer! */
		    break;			/* Otherwise S will get re-used. */

		case P_MOVM:			/* If case hash, also do nothing */
		    break;

		case P_MOVN:			/* fold: out P_MOVN */
		    if (r == s && pnegreg(s, prev))	/* Try to use other instrs */
			{
			flsprev();		/* Win, don't need MOVN at all! */
			break;
			}
		    if (!pushneg(s, prev))	/* Try to negate value */
			{
			codrrx(prev, p);	/* Didn't work, try other opts */
			break;
			}
		    p->Pop = P_MOVE;		/* Value negated! Change op to MOVE */
					/* and drop thru to handle MOVE */
		/* FALLTHROUGH */
		case P_MOVE:
		    if (changereg(r, s, prev))
			flsprev();		/* Won, can flush instr we added */
		    else
			codrrx(prev, p);	/* Nope, just apply other opts. */
		    break;

		case P_DMOVE:
		    if (r == s)			/* Changereg doesn't like doubles, */
			flsprev();		/* so this is the best we can do. */
		    else
			codrrx(prev, p);	/* Nope, try other optimizations. */
		    break;

		default:			/* None of above, just try */
		    codrrx(prev, p);		/* normal post-optimizations */
		    break;
		}

    foldshiftmask();

#if DEBUG_KCC		/* 5/91 KCC size */
    /* Done, show updated buffer on debugging output if required */
    if (debpho)
	shocum();
#endif
}

/* CODE1 - Generates instruction with an immediate integer constant operand
**	OP+I r,val
** This becomes type PTA_RCONST+PTF_IMM. 
*/

void
code1(int op, struct vreg * vr, INT s)
{
    codr1(op, vrtoreal(vr), s);
}

void
codr1(int op, int r, INT s)
{
    PCODE *p;

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	shohdr("CODE1", op, r);
	fprintf(fpho,"<imm> %o\n", s);
	}
#endif
    p = newcode(PTA_RCONST+PTF_IMM, op, r);
    p->Poffset = s;
    /* Reuse an unsigned-ordering bias across the fall-through arm of a
    ** short-circuit range test.  GBOOLOP emits unsigned comparisons as
    ** MOVE/TLC/CAM.  In
    **
    **     MOVE A,S
    **     TLC  A,400000
    **     CAMx A,[C1]
    **      JRST L
    **     MOVE B,S
    **     TLC  B,400000
    **     CAMy B,[C2]
    **
    ** A is unchanged on the fall-through path, so the second MOVE/TLC is
    ** redundant.  Keep this exact and local: both comparisons must be CAM
    ** skips, the intervening instruction must be their skipped JRST, and
    ** both source copies must be ordinary unskipped register moves.
    */
    if (optobj && (op & POF_OPCODE) == P_CAM)
	{
	PCODE *t2, *m2, *j, *c1, *t1, *m1;

	t2 = before(p);
	m2 = (t2 != NULL) ? before(t2) : NULL;
	j = (m2 != NULL) ? before(m2) : NULL;
	c1 = (j != NULL) ? before(j) : NULL;
	t1 = (c1 != NULL) ? before(c1) : NULL;
	m1 = (t1 != NULL) ? before(t1) : NULL;
	if (t2 != NULL && t2->Pop == P_TLC && t2->Preg == r
	  && t2->Ptype == PTA_RCONST && t2->Pvalue == 0400000L
	  && !prevskips(t2)
	  && m2 != NULL && m2->Pop == P_MOVE && m2->Preg == r
	  && (m2->Ptype & ~PTF_SKIPPED) == PTA_REGIS && !prevskips(m2)
	  && j != NULL && j->Pop == P_JRST
	  && (j->Ptype & PTF_SKIPPED) != 0
	  && c1 != NULL && (c1->Pop & POF_OPCODE) == P_CAM
	  && isskip(c1->Pop)
	  && t1 != NULL && t1->Pop == P_TLC
	  && t1->Preg == c1->Preg && t1->Ptype == PTA_RCONST
	  && t1->Pvalue == 0400000L && !prevskips(t1)
	  && m1 != NULL && m1->Pop == P_MOVE && m1->Preg == t1->Preg
	  && (m1->Ptype & ~PTF_SKIPPED) == PTA_REGIS && !prevskips(m1)
	  && m1->Pr2 == m2->Pr2 && m1->Preg != m1->Pr2)
	    {
	    p->Preg = m1->Preg;
	    dropinstr(t2);
	    dropinstr(m2);
	    }
	}

    if (optobj)
	{
	foldplus(p);		/* now do post-optimizations */
	foldmove(previous);	/* "previous" instead of "p", maybe changed */

	foldshiftmask();
	}
#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}

/* CODR1_FORCE - Emit a fixed-register immediate instruction without CSE.
**
** This is for ABI boundary setup, where an instruction such as MOVEI AC1,1
** is required on every incoming control-flow edge.  The ordinary codr1()
** deliberately runs foldmove(), which may reuse an older value of the same
** constant.  That is correct inside a basic block but is not safe for fixed
** argument ACs at a call edge: the matching value may only exist on another
** branch reaching the call.  Keep the instruction in PCODE so normal output
** and later passes still see it, but do not perform local CSE here.
*/
void
codr1_force(int op, int r, INT s)
{
    PCODE *p;

    p = newcode(PTA_RCONST+PTF_IMM, op, r);
    p->Poffset = s;
}

/* CODEBP - Generate instr with PTA_BYTEPOINT local byte pointer operand.
**	OP r,[bbbbii,,sym+o]
**
** Used in optimization of string ops, and in generating struct bit fields.
** NOTE!!! The reg and index arguments are REAL register #s, not pointers
** to virtual regs.
*/

void
codebp(int op, int r, INT b, int i, struct symbol * sym, INT o)
{
    PCODE *p;

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	shohdr("CODEBP", op, r);
	fprintf(fpho,"[%o,,%s+%o(%o)]\n",
			b, (sym ? sym->Sname : ""), o, i);
	}
#endif
    p = newcode(PTA_BYTEPOINT, op, r);		/* Add the instruction */
    p->Pindex = i;
    p->Pptr = sym;
    p->Poffset = o;
    p->Pbsize = b;		/* Store P+S field here */

    /* Apply local-format byte-pointer optimizations */
    if (optobj)
	{
	foldbp(p);	/* Try to optimize operand (doesn't change opcode) */
	foldbyte(p);	/* Attempt byte op optimizations */
	}
#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}

/* CODE3 - Generates address for symbol.
**	OPI R,sym
**
** Only used with P_MOVE, so this becomes MOVEI R,SYM.
*/

void
code3(int op, struct vreg * vr, struct symbol * s)
{
    codrmdx(PTA_MINDEXED+PTF_IMM, op, vrtoreal(vr), s, 0, 0);
}

/* CODE4 - Generates instruction using indexed register operand.
**	OP r,(idx)
**
** This is used after a gaddress() to get the contents of a variable,
** and also for assignment ops (like Q_ASGN = P_MOVEM).  The index is released.
**
** CODEK4 is the same but doesn't release the index register.
**
** CODE4S takes an extra argument which it stuffs into the Pbsize field
** of the stored op.  This is used as a special hack for P_SMOVE only,
** and the size is associated with the op rather than the operand.
*/

void
code4(int op, struct vreg * reg, struct vreg * idx)
{
    int r, s;

    r = reg ? vrstoreal(reg, idx) : 0;	/* Allow IBP/JRST 0, */
    s = ufcreg(vrtoreal(idx));
    if (reg != idx)
	vrfree(idx);	/* will no longer need register */
    code40(op, r, s, 0);
}

extern int _chnl;	/* KAR-2/91, temporary storage for channel numbers */

#if SYS_CSI
static char *tmp_mnem;  /* KAR-2/91, static storage for imuuo mnemonic */
void
code4m(int op, struct vreg * reg, struct vreg * idx, char * mnem)
{
    int r, s;

    tmp_mnem = (char *) calloc(1, strlen(mnem) + 1);
    if (tmp_mnem == NULL)
	jerr("Out of memory for imuuo mnemonic\n");
    memcpy(tmp_mnem, mnem, strlen(mnem) + 1);

    r = reg ? vrstoreal(reg, idx) : 0;	/* Allow IBP/JRST 0, */
    s = ufcreg(vrtoreal(idx));
    if (reg != idx)
	vrfree(idx);	/* will no longer need register */
    code40(op, r, s, 0);
}
#endif

void
codek4(int op, struct vreg * reg, struct vreg * idx)
{
    VREG *r3;
    int r, s;

    r = ufcreg(vrtoreal(idx));
    s = vrstoreal(r3 = vrget(), idx);
    code00(P_SETM, s, r);	/* Use this to avoid losing r2 */
    r = vrstoreal(reg, r3);
    s = vrreal(r3);
    vrfree(r3);
    code40(op, r, s, 0);
}

void
code4s(int op, struct vreg * reg, struct vreg * idx, int keep, INT bsiz)
{
    VREG *r3;
    int r, s;
    if (keep)
	{
	r = ufcreg(vrtoreal(idx));
	s = vrstoreal(r3 = vrget(), idx);
	code00(P_SETM, s, r);	/* Use this to avoid losing r2 */
	r = vrstoreal(reg, r3);
	vrfree(r3);
	}
    else
	{
	r = vrstoreal(reg, idx);
	s = ufcreg(vrreal(idx));
	if (reg != idx)
	    vrfree(idx);	/* will no longer need register */
	}
    code40(op, r, s, bsiz);
}

void
code40(int op, int r, int s, INT bsiz)
{
    PCODE *p, *addp, *movp;
    INT offset;

    /* LSHC/ASHC take a signed constant count directly.  Do not keep a
    ** one-use AC merely to hold a constant shift count.
    **
    **     MOVEI t,n       MOVEI t,n
    **     LSHC  r,0(t)    MOVN  t,t
    **                     ASHC  r,0(t)
    **
    ** become LSHC r,n and ASHC r,-n respectively.  code4() has already
    ** released the index VREG, so the immediately preceding definition is
    ** the complete lifetime of t. */
    if (optobj && bsiz == 0 && (op == P_LSHC || op == P_ASHC)
      && previous != NULL && !prevskips(previous)) {
        PCODE *q;

        if (previous->Pop == P_MOVE && previous->Preg == s
          && (previous->Ptype & ~PTF_SKIPPED) == PTV_IMMED) {
            offset = previous->Pvalue;
            dropinstr(previous);
            codr8(op, r, offset);
            return;
        }
        if (previous->Pop == P_MOVN && previous->Preg == s
          && (previous->Ptype & ~PTF_SKIPPED) == PTA_REGIS
          && previous->Pr2 == s && (q = before(previous)) != NULL
          && !prevskips(q) && q->Pop == P_MOVE && q->Preg == s
          && (q->Ptype & ~PTF_SKIPPED) == PTV_IMMED) {
            offset = -q->Pvalue;
            dropinstr(previous);
            dropinstr(q);
            codr8(op, r, offset);
            return;
        }
    }

    offset = 0;
    /* Fold a one-use address temporary into the PDP-10 indexed operand.
    **
    **     MOVE  t,base
    **     ADDI  t,n
    **     MOVE  r,0(t)     -> MOVE  r,n(base)
    **
    ** The same fold is safe for MOVEM when the stored value is not the
    ** address temporary itself.  Restricting the transformation to these
    ** two common operations keeps it local and avoids any liveness table.
    */
    if (optobj && previous != NULL
      && (r != s || op == P_MOVE)) {
        /* If the address was just copied out of another register, the
        ** indexed operand can use that register directly.  UFCREG cannot
        ** do this for preserved registers because it must conservatively
        ** protect their values in general; here the temporary is consumed
        ** immediately as an address, so the fold is exact.
        */
        movp = previous;
        if ((movp->Ptype & ~PTF_SKIPPED) == PTA_REGIS
          && movp->Pop == P_MOVE && movp->Preg == s
          && !prevskips(movp)) {
            s = movp->Pr2;
            dropinstr(movp);
        }

        addp = previous;
        movp = addp ? before(addp) : NULL;
        if (movp != NULL
          && (addp->Ptype & ~PTF_SKIPPED) == PTV_IMMED
          && addp->Pop == P_ADD && addp->Preg == s
          && !prevskips(addp)
          && (movp->Ptype & ~PTF_SKIPPED) == PTA_REGIS
          && movp->Pop == P_MOVE && movp->Preg == s
          && !prevskips(movp)) {
            offset = addp->Pvalue;
            s = movp->Pr2;
            dropinstr(addp);
            dropinstr(movp);
        }
    }

    /* Forward a recent store through a pure register-indexed address.
    ** codemdx() handles the symbolic/indexed form; code40() is the path
    ** for 0(AC) and folded displacement operands.  Look through a few
    ** register-only operations, but stop at memory, control flow, skips,
    ** or a change to the address index.
    */
    if (optobj && op == P_MOVE && previous != NULL) {
        PCODE *q, *t;
        int nscan;

        for (q = previous, nscan = 0; q != NULL && nscan < 4;
             q = before(q), ++nscan) {
            int amode, changed, regmem;

            amode = q->Ptype & PTF_ADRMODE;
            regmem = (amode == PTA_MINDEXED && q->Pptr == NULL
                      && q->Pindex == 0 && q->Poffset > 0
                      && q->Poffset < NREGS);
            if (prevskips(q) || isskip(q->Pop) || rrchg(q, s)
              || (regmem && q->Poffset == s))
                break;
            if (q->Pop == P_MOVEM
              && (q->Ptype & ~PTF_SKIPPED) == PTA_MINDEXED
              && q->Pptr == NULL && q->Poffset == offset
              && q->Pindex == s) {
                changed = 0;
                for (t = after(q); t != NULL; t = after(t)) {
                    int tm = t->Ptype & PTF_ADRMODE;
                    int trm = (tm == PTA_MINDEXED && t->Pptr == NULL
                               && t->Pindex == 0 && t->Poffset > 0
                               && t->Poffset < NREGS);
                    if (rrchg(t, q->Preg) || (trm && (t->Pop & POF_BOTH)
                                              && t->Poffset == q->Preg)) {
                        changed = 1;
                        break;
                    }
                    if (t == previous)
                        break;
                }
                if (!changed) {
                    code00(P_MOVE, r, q->Preg);
                    return;
                }
                break;
            }
            if (((popflg[q->Pop & POF_OPCODE] & PF_MEMCHG) && !regmem)
              || ((amode == PTA_MINDEXED || amode == PTA_BYTEPOINT)
                  && !regmem))
                break;
            switch (q->Pop & POF_OPCODE) {
            case P_JRST:
            case P_JUMP:
            case P_PUSHJ:
            case P_POPJ:
            case P_AOJ:
            case P_SOJ:
                q = NULL;
                break;
            default:
                break;
            }
            if (q == NULL)
                break;
        }
    }

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	shohdr("CODE4", op, r);
	fprintf(fpho,"(%o) siz %lo\n", s, (INT) bsiz);
	}
#endif
    
    /* First just add the instruction */
    p = newcode(PTA_MINDEXED, op, r);
    p->Pptr = NULL;
    p->Poffset = offset;
    p->Pindex = s;
    p->Pbsize = bsiz;

    /* Drop an earlier store to the same pure indexed address when only
    ** register/constant operations intervene.  The later MOVEM overwrites
    ** the first value before memory can observe it.  Keep the scan short and
    ** conservative: no skips, no memory operands, no memory-changing ops,
    ** no control flow, and the address index must remain unchanged.
    */
    if (optobj && op == P_MOVEM && !prevskips(p)) {
        PCODE *q;
        int nscan;

        for (q = before(p), nscan = 0; q != NULL && nscan < 4;
             q = before(q), ++nscan) {
            int amode;

            if (prevskips(q))
                break;
            if (q->Pop == P_MOVEM
              && (q->Ptype & PTF_ADRMODE) == PTA_MINDEXED
              && q->Pptr == NULL && sameaddr(q, p, 0)) {
                dropinstr(q);
                break;
            }
            if ((popflg[q->Pop & POF_OPCODE] & PF_MEMCHG)
              || isskip(q->Pop) || rrchg(q, p->Pindex))
                break;
            amode = q->Ptype & PTF_ADRMODE;
            if (amode == PTA_MINDEXED || amode == PTA_BYTEPOINT)
                break;
            switch (q->Pop & POF_OPCODE) {
            case P_JRST:
            case P_JUMP:
            case P_PUSHJ:
            case P_POPJ:
            case P_AOJ:
            case P_SOJ:
                q = NULL;
                break;
            default:
                break;
            }
            if (q == NULL)
                break;
        }
    }

/*
 * KAR-1/92, use channel field for null ptr det. to store line number also
 * need to skip mnemonic copy, so changed "if" to a "switch" below.
 */
    switch (op)
	{
#if SYS_CSI
	case P_MUUO:
	    p->p_im.mnemonic = (char *) calloc(1, strlen(tmp_mnem) + 1);
	    if (p->p_im.mnemonic == NULL)
		jerr("Out of memory for null ptr detection mnemonic\n");
	    memcpy(p->p_im.mnemonic, tmp_mnem, strlen(tmp_mnem) + 1);
	    free(tmp_mnem);
	    tmp_mnem = NULL;
	/* FALLTHROUGH */
#endif
	case P_NULPTR:
	    p->p_u.p_int = _chnl;
	    _chnl = -1;	/* KAR-6/91, Changed empty signal to -1 from 0 */
	    break;
	default:
	    if (optobj)			/* Apply post-optimization */
		foldxref(p);
	    break;
	}

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}

/* FOLDRETPOPJ - Retarget a terminal return producer into AC1.
**
**     op    T,x
**     MOVE  1,T
**     POPJ  17,
**
** becomes:
**
**     op    1,x
**     POPJ  17,
**
** Only pure value producers are accepted.  Arithmetic operations such as
** ADD/SUB are excluded because their result depends on the old destination.
*/
static void
foldretpopj(struct pcode * p)
{
    PCODE *m, *q;
    int src, op;

    if (p == NULL || p->Pop != P_POPJ
      || (m = before(p)) == NULL || m->Pop != P_MOVE
      || m->Ptype != PTA_REGIS || m->Preg != R_RETVAL
      || prevskips(m))
	return;

    src = m->Pr2;
    if (src == R_RETVAL || (q = before(m)) == NULL || q->Preg != src
      || prevskips(q) || (q->Pop & POF_BOTH))
	return;

    op = q->Pop & POF_OPCODE;
    switch (op)
	{
	case P_MOVE:
	case P_MOVN:
	case P_SETZ:
	case P_SETO:
	case P_HLRZ:
	case P_HRRZ:
	    break;
	default:
	    return;
	}

    q->Preg = R_RETVAL;
    dropinstr(m);
}

/* FOLDRETCHAIN - Retarget a short terminal value chain into AC1.
**
**     setop T,x
**     modop T,y
**     MOVE  1,T
**     POPJ  17,
**
** becomes:
**
**     setop 1,x
**     modop 1,y
**     POPJ  17,
**
** The first op must completely define T.  The modifying op may read T, but
** must not otherwise reference AC1; otherwise retargeting the first op could
** destroy a value used by the modifier.
*/
static void
foldretchain(struct pcode * p)
{
    PCODE *m, *q, *d;
    int src, op;

    if (p == NULL || p->Pop != P_POPJ
      || (m = before(p)) == NULL || m->Pop != P_MOVE
      || m->Ptype != PTA_REGIS || m->Preg != R_RETVAL
      || prevskips(m))
	return;

    src = m->Pr2;
    if (src == R_RETVAL || (q = before(m)) == NULL || q->Preg != src
      || prevskips(q) || (q->Pop & POF_BOTH) || rincode(q, R_RETVAL))
	return;

    op = q->Pop & POF_OPCODE;
    switch (op)
	{
	case P_ADD:
	case P_SUB:
	case P_AND:
	case P_IOR:
	case P_XOR:
	case P_TLC:
	case P_TLO:
	case P_TLZ:
	case P_TRC:
	case P_TRO:
	case P_TRZ:
	    break;
	default:
	    return;
	}

    if ((d = before(q)) == NULL || d->Preg != src || prevskips(d)
      || (d->Pop & POF_BOTH))
	return;

    op = d->Pop & POF_OPCODE;
    switch (op)
	{
	case P_MOVE:
	case P_MOVN:
	case P_SETZ:
	case P_SETO:
	case P_HLRZ:
	case P_HRRZ:
	    break;
	default:
	    return;
	}

    d->Preg = R_RETVAL;
    q->Preg = R_RETVAL;
    dropinstr(m);
}

/* CODE5 - Generate op using only a single register operand.
**	OP reg,
**
** Currently only used for SETZ, POPJ, TRNA.
*/

void
code5(int op, struct vreg * reg)
{
    PCODE *p, *q;
    int r;
    r = reg ? vrtoreal(reg) : 0;	/* Allow for TRNA 0, in gboolop() */

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	shohdr("CODE5", op, r);
	fputc('\n', fpho);
	}
#endif

    p = newcode(PTA_ONEREG, op, r);		/* Add the instruction */

    if (optobj && op == P_POPJ)
	{
	    foldretpopj(p);
	    foldretchain(p);
	}

    if (optobj && (op == P_SETZ || op == P_SETO))
	{
	if ((q = before(p)) != NULL	/* Check previous instr */
		&& q->Pop == op
		&& q->Ptype == PTA_ONEREG /* && !prevskips */)
	    {
	    /*
	    ** fold:  SETZ  S, / SETZ R,	(or SETO)
	    ** into:  SETZB S,R			(or SETOB)
	    */
	    p->Ptype = PTA_REGIS;
	    p->Pop |= POF_BOTH;
	    p->Pr2 = r;
	    dropinstr(p);	/* Flush instr we just added */
	    }
	else if (op == P_SETZ)
	    foldmove(p);
	}

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}

#if SYS_CSI
void
code5m(int op, struct vreg * reg, char * mnem)
{
    PCODE *p, *q;
    int r;

    r = reg ? vrtoreal(reg) : 0;	/* Allow for TRNA 0, in gboolop() */

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	shohdr("CODE5", op, r);
	fputc('\n', fpho);
	}
#endif

    p = newcode(PTA_ONEREG, op, r);		/* Add the instruction */
    
    /* KAR-2/91, Copies mnemonic into PCODE node for imuuo */
    if (op == P_MUUO)
	{
	p->p_im.mnemonic = (char *) calloc(1, strlen(mnem) + 1);
	if (p->p_im.mnemonic == NULL)
	    jerr("Out of memory for null ptr mnemonic\n");
	memcpy(p->p_im.mnemonic, mnem, strlen(mnem) + 1);
	p->p_im.p_chnl = _chnl;
	_chnl = -1;	/* KAR-6/91, Changed sentinal value to -1 from 0 */
	}

    if (optobj && (op == P_SETZ || op == P_SETO))
	{
	if ((q = before(p)) != NULL	/* Check previous instr */
		&& q->Pop == op
		&& q->Ptype == PTA_ONEREG /* && !prevskips */)
	    {
	    /*
	    ** fold:  SETZ  S, / SETZ R,	(or SETO)
	    ** into:  SETZB S,R			(or SETOB)
	    */
	    p->Ptype = PTA_REGIS;
	    p->Pop |= POF_BOTH;
	    p->Pr2 = r;
	    dropinstr(p);	/* Flush instr we just added */
	    }
	else if (op == P_SETZ)
	    foldmove(p);
	}

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}
#endif /* SYS_CSI */

/* CODE6 - Generate op with a symbolic address operand.
**	OP reg,sym
** This is mostly used for jumps and switch jump tables.
*/


void
code6(int op, struct vreg * reg, struct symbol * s)
{
    PCODE *p;
    int r = reg ? vrtoreal(reg) : 0;	/* Handle JRST 0,lab */

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	shohdr("CODE6", op, r);
	if (s)
	    fputs(s->Sname, fpho);
	fputc('\n', fpho);
	}
#endif

    /* Pre-optimization, sigh */
    if (optobj
      && (op & POF_OPCODE) == P_JUMP
      && previous && !isskip(previous->Pop))
	{

	/* Emit: JUMPx R,lab	as:	CAIx  R,0
	**				 JRST  lab
	**
	** So skip optimization can work best.  If it doesn't get folded,
	** then optjrst() will turn it back into JUMPx.
	*/
	r = ufcreg(r);		/* optimization loses w/o this? */
	p = newcode(PTA_RCONST,
		op ^ (P_CAI ^ P_JUMP ^ POF_ISSKIP ^ POSF_INVSKIP),
		r);
	p->Pvalue = 0;
	foldskip(p, 1);
	op = P_JRST;		/* make it a jump */
	r = 0;			/* JRST doesn't take a register */
	}

    /* All set up, drop values into the node */
    p = newcode(PTA_MINDEXED, op, r);
    p->Pptr = s;
    p->Pindex = 0;
    p->Poffset = 0;
    reflabel(s, 1);		/* Increment label reference count */

    /* Now do post-optimization */
    if (op == P_JRST && optobj)
	{
	foldjump(before(p), s);
	optjrst(previous);		/* p may have become nop */
	}

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}

/* CODEMDX - Generates general-purpose PTA_MINDEXED op.
**	OP rreg,pptr+poffset(rindex)
**
** NOTE!!! Because this is only used to duplicate addressing of an
** already-generated op, the register args are REAL register numbers,
** not virtual reg pointers!
*/

void
codemdx(int op, int rreg, struct symbol * pptr, INT poffset, int rindex)
{
    PCODE *p;
    int nreg;

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	shohdr("CODEMDX", op, rreg);
	fprintf(fpho,"%s+%o(%o)\n", (pptr ? pptr->Sname : ""),poffset,rindex);
	}
#endif

    /* Maybe can do more complicated optimizing code generation */
    if (pptr == NULL && poffset == 0)
	{
	code40(op, rreg, ufcreg(rindex), 0);
#if DEBUG_KCC		/* 5/91 KCC size */
	if (debpho)
	    shocum();
#endif
	return;
	}
    nreg = ufcreg(rreg);		/* undo failed changereg */

    /* Forward an immediately preceding store to the exact same address.
    ** No memory analysis is needed because there is no intervening code.
    **
    **     MOVEM S,X
    **     MOVE  R,X        -> MOVE R,S
    */
    if (optobj && op == P_MOVE && previous != NULL
      && previous->Pop == P_MOVEM
      && (previous->Ptype & ~PTF_SKIPPED) == PTA_MINDEXED
      && previous->Pptr == pptr && previous->Poffset == poffset
      && previous->Pindex == rindex && !prevskips(previous))
	{
	code00(P_MOVE, nreg, previous->Preg);
	if (nreg != rreg)
	    code00(P_MOVE, rreg, nreg);
	return;
	}

    /* too general for optimization, just add the code */
    p = newcode(PTA_MINDEXED, op, nreg);
    p->Pptr = pptr;
    p->Poffset = poffset;
    p->Pindex = rindex;

    if (nreg != rreg)		/* Put result back in right register */
	code00(P_MOVE, rreg, nreg);
#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}

/* CODE8 - Generates instruction with integer constant operand
**	OP r,const
**
** Like code1(), but op doesn't become immediate, making type PTA_RCONST.
** E.g. code8(P_ADJSP, VR_SP, n) where n is some stack offset.
**
** KLH: Note slipshod assumption that because CCGSWI calls code8 heavily
** for P_CAIx, foldskip() shouldn't try to change value of R, in case
** it is needed by later switch tests.
*/

void
code8(int op, struct vreg * reg, INT val)
{
    codr8(op, vrtoreal(reg), val);
}

static void
codr8(int op, int r, INT val)
{
    PCODE *p, *q;

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	shohdr("CODE8", op, r);
	fprintf(fpho,"%o\n", val);
	}
#endif

    /* Pre-optimization when about to add ADJSP */
    if (optobj && op == P_ADJSP)
	{
	val = foldstack(val);
	if (val == 0)			/* If completely folded, can return */
	    {
#if DEBUG_KCC		/* 5/91 KCC size */
	    if (debpho)
		shocum();
#endif
	    return;
	    }
}

    /* Fold a constant formed by clearing a register and then toggling
    ** bits in its left half.
    **
    **     SETZ  R,
    **     TLC   R,n
    **
    ** becomes a single MOVE of the corresponding 36-bit constant.
    ** Later output optimization may fold that constant into its consumer.
    */
    if (optobj && op == P_TLC && previous != NULL) {
	q = previous;
	if (q->Pop == P_SETZ && q->Ptype == PTA_ONEREG
	  && q->Preg == r && !prevskips(q)) {
	    q->Pop = P_MOVE;
	    q->Ptype = PTV_IMMED;
	    q->Pvalue = (val & 0777777L) << 18;
#if DEBUG_KCC
	    if (debpho)
		shocum();
#endif
	    return;
	}
    }

    p = newcode(PTA_RCONST, op, r);
    p->Pvalue = val;

    /* Post-optimization for ADJSP, CAIx, CAMx */
    if (optobj)
	{
	if (op == P_ADJSP)
	    {
	    if ((p = before(p)) != NULL
	       && p->Ptype == PTV_IINDEXED)	/* && !prevskips */
		{
		if (p->Pindex == R_SP)
		    p->Poffset -= val;		/* Adjust stk addr for swap */
		swappseudo(p, previous);	/* Encourage tail recursion */
		}
	    }
	else				/* See if can fold: CAM or CAI */
	    foldskip(p, 0);		/* Could be from switch, be careful */
	}
#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}

/* CODE9 - Generates instruction with floating-point constant operand
**	OP r,[float]
**
** Grave misunderstandings will result if the "twowds" flag
** does not match the op specified!
**	Either that flag or the opcode are valid ways of determining 
** whether we are dealing with a 1 or 2 word constant.
*/

void
code9(int op, VREG *vr, double value, int twowds)
/* twowds is TRUE if 2 wds (double), else 1-wd float */
{
    PCODE *p;
    int r = vrtoreal(vr);

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	shohdr("CODE9", op, r);
	fprintf(fpho,"[%.20g]\n", value);
	}
#endif

    /* Optimize a P_MOVE or P_DMOVE of 0.0 since floating point zero is
     * same as integer zero.
     */
    if (value == 0.0 && optobj)
	switch (op)
	    {
	    case P_DMOVE:	/* Make DMOVE R,[0 ? 0] be SETZB R,R+1 */
		/* Can't use code0 as that would release r+1 */
		p = newcode(PTA_REGIS, P_SETZ+POF_BOTH, r);
		p->Pr2 = r+1;
#if DEBUG_KCC		/* 5/91 KCC size */
		if (debpho)
		    shocum();
#endif
		return;
	    case P_MOVE:
		code5(P_SETZ, vr);
#if DEBUG_KCC		/* 5/91 KCC size */
		if (debpho)
		    shocum();
#endif
		return;
	    }

    p = newcode((twowds ? PTA_DCONST : PTA_FCONST), op, r);
    if (twowds)
	p->Pdouble = value;
    else
	p->Pfloat = (float) value;

    if (optobj)
	foldmove(p);	/* see if already have this one */
#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}

/* CODE10 - Generate op with pointer constant operand (ie a pointer literal)
**	OP r,[pointer]
**
** The pointer is made from an:
**		address (pointer to a symbol)
**		offset (in bytes from address)
**		bytesize (0,6,7,8,9,18)
** Using this information, CCOUT constructs a byte or word pointer using
** symbolic references which are resolved at load time into whatever
** the proper values are, whether local or OWGBP format (non-extended or
** extended).
**	A bytesize of -1 is special and indicates that a MASK is desired
** for the P+S field of a byte-pointer.  This should never be used with
** an address or offset.
**	When used with the P_PTRCNV op (which converts a pointer in
** the register), the offset indicates the byte size of the current pointer,
** and the bsize indicates the desired byte size to convert it to.
** The address must be NULL.
*/

void
code10(int op, struct vreg * vr, struct symbol * addr, INT bsize, INT offset)
{
    codr10(op, vrtoreal(vr), addr, bsize, offset);
}

void
codr10(int op, int r, struct symbol * addr, INT bsize, INT offset)
{
    PCODE *p;
    int nreg;

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	shohdr("CODE10", op, r);
	fprintf(fpho,"[%s+%o (size %d)]\n",
		(addr ? addr->Sname : ""), offset, bsize);
	}
#endif
    nreg = ufcreg(r);	/* undo failed changereg */
			/* this goes with the code0 call below */

    p = newcode(PTA_PCONST, op, nreg);
    p->Pptr = addr;
    p->Poffset = offset;
    p->Pbsize = bsize;

    if (nreg != r)
	code00(P_MOVE, r, nreg);

    if (optobj && op == P_MOVE)
	foldmove(previous);	/* see if already have this one */
#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}

/* CODRMDX - Auxiliary for following routines, just to share
**	common code that creates a PTA_MINDEXED op and returns without
**	doing any optimization.
**	Note that the registers "reg" and "index" are real regs, not virtual.
*/
static void
codrmdx(int type, int op, int reg, struct symbol * ptr, INT offset, int index)
{
    PCODE *p;

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	shohdr("CODRMDX", op, reg);
	if (type&PTF_IMM)
	    fputs("+I ", fpho);
	if (type&PTF_IND)
	    fputc('@', fpho);
	if (ptr)
	    fputs(ptr->Sname, fpho);
	if (ptr && offset)
	    fputc('+', fpho);
	if (offset)
	    fprintf(fpho, "%o", offset);
	if (index)
	    fprintf(fpho, "(%o)", index);
	fputc('\n', fpho);
	}
#endif
    p = newcode(type, op, reg);
    p->Pptr = ptr;
    p->Poffset = offset;
    p->Pindex = index;
#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}

/* CODE12 - Generates register despill instruction.
**	OP reg,offset(17)
**
** This is used only by CCREG to despill registers with MOVE or DMOVE.
*/
void
code12(int op, struct vreg * vr, INT offset)
{
    codrmdx(PTA_MINDEXED, op, vrtoreal(vr), (SYMBOL *)NULL, offset, R_SP);
    /* codrmdx() deliberately performs no optimization.  A direct two-word
    ** stack load, however, is already in canonical address form and may
    ** immediately follow a store of the same value.  Give DMOVE the same
    ** store-forward/CSE opportunity as other generated loads. */
    if (optobj && op == P_DMOVE)
        foldmove(previous);
}

/* CODE13 - Generates MOVEI of stack location.
**	opI reg,offset(17)
**
** Used by several things, but op is always MOVE, so this always
** becomes an MOVEI of a stack location.
*/
void
code13(int op, struct vreg * vr, INT offset)
{
    codrmdx(PTA_MINDEXED+PTF_IMM, op, vrtoreal(vr),
					(SYMBOL *)NULL, offset, R_SP);
}

/* CODE14 - Generates an op whose effective address is an accumulator.
**	OP reg,ac
**
** PDP-10 accumulators are memory locations 0 through 17.  This form is
** distinct from 0(ac), which uses the accumulator as an index register.
*/
void
code14(int op, struct vreg * vr, int addr)
{
    codrmdx(PTA_MINDEXED, op, vrtoreal(vr),
					(SYMBOL *)NULL, (INT) addr, 0);
}

/* CODE15 - Generates op with indirect indexed local label
**	OP @lab+offset(idx)
**
** Only used by CCGSWI with op JRST for switch jump tables.
*/
void
code15(int op, struct symbol * lab, INT off, struct vreg * idx)
{
    codrmdx(PTA_MINDEXED+PTF_IND, op, 0, lab, off, vrtoreal(idx));
}

/* CODE16 - Generates op with indexed local label.
**	OP reg,$lab(idx)
**
** Only used by CCGSWI with op CAM for checking switch hash tables.
*/
void
code16(int op, struct vreg * vr, struct symbol * lab, struct vreg * vs)
{
    int r = vrstoreal(vr, vs);		/* Ensure both R and S in real regs */
    codrmdx(PTA_MINDEXED, op, r, lab, 0, vrreal(vs));
}

/* CODE17 - Generates a literal value.
**
** Only used by CCGSWI for generating switch hash tables.
*/

void
code17(INT value)
{
    PCODE *p;
#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	fprintf(fpho,"CODE17: literal %o\n", value);
	}
#endif
    p = newcode(PTA_RCONST, P_CVALUE, 0);
    p->Pvalue = value;
#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}

/* CODESTR - Generate "code" consisting of a direct user-specified
**	assembly language string.  This implements the asm() construction.
*/
void
codestr(char * s, int len)
{
    extern void outnstr(char *, int);

    flushcode();		/* Ensure pcode buffer flushed */
    outnstr(s, len);
}

/* CODLABEL, CODGOLAB - Generate "code" consisting of the given label symbol.
**	codlab does more optimization and may not actually emit the label.
**	codgolab always emits its label; it is used for goto labels.
*/
/* CODLABEL - Emit a (forward) label.
**
** Optimizations are performed and if the label still has references
** to it, it is emitted.  Then realfreelabel() is called on it, and
** if we emitted it and thus cleared the peephole buffer we also free
** the list of labels queued by freelabel().
*/
void
codlabel(struct symbol * lab)
{
    INT after = 0;

#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	fprintf(fpho, "CODLAB: %s\n", lab->Sname);
	}
#endif

    if (optobj)
	{
	after = hackstack(lab);		/* pull ADJSP across POPJ */
	optlab(lab);			/* call peephole optimizer */
	}
    if (lab->Svalue > 0)		/* See if still must emit */
	{
#if DEBUG_KCC		/* 5/91 KCC size */
	if (debpho)
	    shocum();		/* Yes, report changes prior to */
#endif
	flushcode();			/* clearing out previous code. */
	outlab(lab);			/* Now emit label. */
	cleanlabs();			/* Now OK to clean up queued labels */
	}
    if (after)
	code8(P_ADJSP, VR_SP, after);	/* fix up stack */
    freelabel(lab);
#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}

/* CODGOLAB - Emit a GOTO type label.
** These are not as well behaved as loop and if labels so we can't do as much.
*/
void
codgolab(struct symbol * lab)
{
#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	{
	fprintf(fpho, "CODGOLAB: %s\n", lab->Sname);
	}
#endif
    if (optobj)
	optlab(lab);	/* Optimize */
    flushcode();		/* Clear out previous code */
    outlab(lab);		/* and unconditionally emit label */
#if DEBUG_KCC		/* 5/91 KCC size */
    if (debpho)
	shocum();
#endif
}

/* This code should be flushed eventually,
** and TXO, TXZ, TXC forms substituted for this silly nonsense.
*/

/* ------------------------------------------------------ */
/*	return immediate version of boolean operator      */
/* ------------------------------------------------------ */

int
immedop(int op)
{
    switch (op & POF_OPCODE)
	{
	case P_CAM:
	    return op ^ (P_CAM ^ P_CAI);
	case P_TDN:
	    return op ^ (P_TDN ^ P_TRN);
	case P_TDO:
	    return op ^ (P_TDO ^ P_TRO);
	case P_TDC:
	    return op ^ (P_TDC ^ P_TRC);
	case P_TDZ:
	    return op ^ (P_TDZ ^ P_TRZ);
	default:
	    return 0;
	}
}

/* PEEPHOLER PSEUDO-OP OPTIMIZATION ROUTINES */

/* CODRRX - Optimization auxiliary for CODE0.
**	Basically intended to help make sense of the gigantic switch
**	statement that previously existed.
**
**	prev - instruction farther back in peephole buffer, which we are
**		looking at.
**	np - instruction we are trying to optimize (normally what was
**		just added; an OP R,S instr).
*/
static void
codrrx(struct pcode * prev, struct pcode * np)
{
    PCODE *q;

    /* Avoids faulty optimizations in the functions: codrrx, rrpre1, rrpre2,
     * rrpre3, rrpopt, rrpop2, localbyte, chkref, chkmref, rincode, rinaddr, 
     * rrchg, foldbyte, foldplus, foldadjbp, foldskip, sameaddr, alias, 
     * inskip, and findconst.
     */
    if (Register_Preserve(np->Pr2))
	return;

    /* If have an unskipped previous instruction to look at,
    ** try to fold: our newly added instr into it.
    */
    if (prev && !prevskips(prev))
	{
	if (prev->Preg != np->Pr2
	  && rrpre1(np))		/* Prev instr R != new S */
	    return;			/* If won, just return. */

	if ((q = findrset(prev, np->Pr2)) != NULL
	  && rrpre2(q, np))		/* Prev instr R == new S */
	    return;			/* If won, just return */
	}
    rrpop2(np);				/* Try standard optimizations */
}

/* RRPRE1 - Auxiliary for CODE0.
**	Does some commutative register-register optimization.
** The constraints for coming here are that:
**	A previous instr exists, which is not skipped,
**		such that its R (register field) is NOT the same as the S
**		of the new instruction we are adding.
**	P points to the newly added instruction, which is always a
**		register-register op.
**
** Returns TRUE if an optimization change was made.
*/
static int
rrpre1(struct pcode * p)
{
    PCODE *q;
    int r, s;

    /* See if opcode of new instruction is one we can do something with.
    ** If the operation is commutative or can otherwise have the
    ** ordering of its operands reversed, then see if this can result
    ** in an improvement.
    */
    switch (p->Pop & POF_OPCODE)
	{
	case P_ADD:
	case P_IMUL:		/* Commutative integer ops */
	case P_FADR:
	case P_FMPR:		/* Commutative floating ops */
	case P_IOR:
	case P_AND:
	case P_XOR:	/* Bitwise ops */
	case P_CAM:				/* Invertible test */
	    break;			/* Win, can check further! */
	default:
	    return 0;			/* Something else, so fail */
	}

    /* A recognized op, check further. */
    if (p->Pr2 == R_SP			/* Never mess with stack reg! */
      || (q = findrset(before(p), p->Preg)) == NULL)	/* Find instr setting R */
	return 0;

    switch (q->Pop)			/* Does that instr */
	{
	case P_SETZ:
	case P_SETO:	/* set R in a simple way? */
	case P_MOVE:
	case P_MOVN:
	    break;
	default:			/* No, just give up. */
	    return 0;
	}
    if (q->Ptype == PTV_IINDEXED)	/* Also ignore if imm addr op */
	return 0;

    /* OK, try swapping the operands and see if that helps. */
    r = p->Preg;
    s = p->Pr2;
    p->Preg = s;			/* Reverse order of AC,E in op */
    p->Pr2 = r;
    if ((p->Pop&POF_OPCODE) == P_CAM)
	{
	p->Pop = swapop(p->Pop);	/* Invert the skip test */
	if (rrpre2(q, p))		/* See if this helped optimize */
	    return 1;
	p->Pop = swapop(p->Pop);	/* Nope, restore original test */
	}
    else if (rrpre2(q, p))		/* Re-check optimization now */
	{
	code00(P_MOVE, r, s);		/* Won! Put back in right reg */
	return 1;
	}
    p->Preg = r;	/* Failed, restore original ordering. */
    p->Pr2 = s;
    return 0;		/* Return failure so rrpre2 called */
}

/* RRPRE2 - (another) Auxiliary for CODE0.
**	Checks previous instruction for optimization with the one we're
** currently adding.  The constraints for coming here are that:
**	P points to a previous instr, which is not skipped,
**		such that its R (register field) is the SAME as the S
**		of the new instruction we are adding.
**	NP points to the newly added instruction, which is an OP R,S.
**		(register-register op)
**		Note that S is NOT referenced by anything between P and NP,
**		which makes various things safe.
*/
static int
rrpre2(struct pcode * p, struct pcode * np)
{
    int op = np->Pop;	/* OP R,S of new instruction we just added */
    int r = np->Preg;
    int s = np->Pr2;
    PCODE *q;

    /* Check opcode of PREVIOUS instr.
    ** Breaking out of the switch statement simply leaves new instr alone
    ** and returns failure.
    */
    switch (p->Pop & (POF_OPCODE | POF_BOTH))
	{
	case P_IMUL:
		{
		PCODE *b, *bb;

		if (p != before(np))
		    break;	/* For now */

		if (op != P_SUB || (q = before (p)) == NULL || q->Pop != P_IDIV ||
		    q->Preg != p->Preg || !sameaddr (p, q, 0) ||
		    (b = before (q)) == NULL || b->Pop != P_MOVE ||
		    b->Preg != p->Preg)
		    break;
		if (b->Ptype == PTA_REGIS && b->Pr2 == r)
		    bb = NULL;
		else if ((bb = before (b)) == NULL || bb->Preg != r ||
			 bb->Pop != P_MOVE || prevskips (bb) ||
			 !sameaddr (b, bb, 0))
		    break;

	/*
	** fold:  MOVE R,x
	**	  MOVE S,x
	**	  IDIV S,y
	**	  IMUL S,y
	**	  SUB R,S
	**
	** into:  MOVE S,x
	**	  IDIV S,y	;error if S is preserve reg.
	**	  MOVE R,S+1
	**
	** for ignorant Pascal programmers.
	*/

		if (bb != NULL)
		    bb->Pop = P_NOP; /* drop first move */
		p->Pop = P_NOP;			/* drop P_IMUL */
		dropinstr(np);			/* Drop new instr, fix up previous */
		code00(P_MOVE, r, s+1);		/* put result in right reg */
		return 1;
		}

	case P_DMOVN:			/* similar to P_MOVN below */
	    switch (op)
		{
		case P_DMOVE:			/* move over for optimization */
		    p->Pop = P_DMOVE;
		    op = P_DMOVN;
		    break;
		case P_DMOVN:			/* cancel double DMOVN */
		    p->Pop = P_DMOVE;
		    op = P_DMOVE;
		    break;
		case P_DFAD:			/* R + -X is same as R - X */
		    p->Pop = P_DMOVE;
		    op = P_DFSB;
		    break;
		case P_DFSB:			/* R - -X is same as R + X */
		    p->Pop = P_DMOVE;
		    op = P_DFAD;
		    break;
		default:
		    return 0;		/* Give up, failed to add instr */
		}
	    np->Pop = op;
	/* Then drop through to following case */

	/* FALLTHROUGH */
	case P_DMOVE:
	    switch (op)
		{
		case P_DFAD:
		case P_DFSB:
		case P_DFDV:
		case P_DFMP:
		case P_DMOVE:
		case P_DMOVN:
		/* Optimize	DMOVE S,x /.../ Dop R,S
		** into		Dop R,x
		*/
		    rrpre3(p, np, op);
		    return 1;

	/* Try to fold: DMOVE R,M / PUSH x,R / PUSH x,R+1
	** into PUSH x,M / PUSH x,M+1
	** We come here when the current instr is DMOVE and we are adding
	** a PUSH.
	*/
		case P_PUSH:
		    if (p != before(np))
			break;	/* For now */

	    /* Check out addressing mode, and forget it unless
	    ** it's a plain vanilla REGIS, MINDEXED, or DCONST.
	    ** Indirection, immediateness, or skippedness all cause failure.
	    */
		    switch (p->Ptype)
			{
			case PTA_REGIS:
			case PTA_MINDEXED:
			case PTA_DCONST:
			    break;
			default:
			    return 0;		/* Give up, no optimization done */
			}

	    /* First turn DMOVE R,M / PUSH P,R
	    ** into MOVE R+1,M+1 / PUSH P,M
	    */
		    p->Pop = P_MOVE;		/* fold: the DMOVE R,M */
		    p->Preg++;			/* into MOVE R+1,M */
		    switch (p->Ptype)		/* Now fix up memory operand */
			{
			case PTA_REGIS:
			    p->Pr2++;			/* MOVE R+1,R2+1 */
						/* PUSH P,R2 */
			    break;
			case PTA_MINDEXED:
			    dropinstr(np);			/* Flush new instr */
			    codemdx(P_PUSH, r, p->Pptr,		/* MOVE R+1,M+1 */
				    p->Poffset++, p->Pindex);	/* PUSH P,M */
			    np = previous;
			    break;
			case PTA_DCONST:
			    p->Ptype = PTA_DCONST2;		/* MOVE R+1,1+[const] */
			    np->Ptype = PTA_DCONST1;	/* PUSH R,[const] */
			    np->Pdouble = p->Pdouble;
			    break;
			}

	    /* Now try to rearrange the MOVE / PUSH into PUSH / MOVE
	    ** if we still know where things are (not always true).
	    ** np should point to the PUSH.
	    */
		    if (np->Pop != P_PUSH)
			return 1;	/* If not, forget it */
		    if ((p = before(np)) != NULL	/* See if prev instr is */
		      && (p->Pop == P_MOVE))		/* still MOVE */
		/* Yep, swap 'em */
			{
			if ((p->Ptype&PTF_ADRMODE)==PTA_MINDEXED
			  && p->Pindex == R_SP)
			    p->Poffset--;
			swappseudo(p, np);		/* into PUSH / MOVE */
			}
		    return 1;
		}
	    break;

	case P_IOR:				/* Prev instr was IOR of possible BP */
	case P_ADJBP:			/* or ADJBP of known BP */
	    if (op == P_LDB || op == P_DPB)	/* If one-time use of BP, */
		{
		if (localbyte(p, np))	/* try to make it local-fmt */
		    return 1;		/* Won, folded. */
		}
	    break;

	case P_SETZ:
	    switch (op)
		{
		case P_MOVE:		/* fold:  SETZ S, ; error if S preserve reg */
		case P_FMPR:		/*	    ...		*/
		case P_IMUL:		/*        MOVE/etc R,S  */
		case P_MOVN:		/* into:  SETZ R,   */
		case P_FLTR:
		    np->Pop = P_SETZ;	/* Replace this op with a SETZ R, */
		    np->Ptype = PTA_ONEREG;
		    dropinstr(p);		/* Flush the old SETZ */
		    return 1;

		case P_ADD:		/* fold:  SETZ S, error if S preserve reg */
		case P_FADR:		/*	   ...		*/
		case P_SUB:		/*        ADD  R,S	*/
		case P_FSBR:		/* into:  null		*/
		    dropinstr(np);		/* Flush the new op */
		    dropinstr(p);		/* and flush the SETZ */
		    return 1;
		}

	/* FALLTHROUGH */
	case P_SETO:			/* fall in from P_SETZ above */
	/* fold: SETZ/SETO S, to MOVNI S,0/1 */
	    p->Pvalue = (p->Pop == P_SETO ? 1 : 0);
	    p->Pop = P_MOVN;
	    p->Ptype = PTV_IMMED;		/* then drop through */

	/* FALLTHROUGH */
	case P_MOVN:
	/* invert MOVN(*) to MOVE(*) for following optimization */

	    switch(op & POF_OPCODE)
		{
		case P_MOVE:
		    p->Pop = P_MOVE;	/* move P_MOVN over for optimization */
		    np->Pop = P_MOVN;
		    break;
		case P_MOVN:
		    p->Pop = P_MOVE;	/* cancel double P_MOVN */
		    np->Pop = P_MOVE;
		    if (changereg (r, s, p))
			{
			dropinstr(np);	/* Flush pointless MOVE */
			return 1;
			}
		    break;
		case P_ADD:		/* R + -X is same as R - X */
		    p->Pop = P_MOVE;
		    np->Pop = P_SUB;
		    break;
		case P_SUB:		/* R - -X is same as R + X */
		    p->Pop = P_MOVE;
		    np->Pop = P_ADD;
		    break;
		case P_FADR:		/* R + -X is same as R - X */
		    p->Pop = P_MOVE;
		    np->Pop = P_FSBR;
		    break;
		case P_FSBR:		/* R - -X is same as R + X */
		    p->Pop = P_MOVE;
		    np->Pop = P_FADR;
		    break;

		case P_CAM:
		case P_IMUL:
		case P_FMPR:
		case P_FDVR:
		    if (p->Ptype != PTV_IMMED
		      && (q = before(p)) != NULL
		      && q->Preg == r
		      && !prevskips (q))
			switch (q->Pop)
			    {
			    case P_MOVE:	/* a <= -b is same as -a >= b */
				q->Pop = P_MOVN;
				p->Pop = P_MOVE;
				np->Pop = swapop(np->Pop);
				codrrx(p, np);		/* Continue looking back */
				return 1;

			    case P_MOVN:	/* -a <= -b is same as a >= b */
				q->Pop = P_MOVE;
				p->Pop = P_MOVE;
				np->Pop = swapop(np->Pop);
				codrrx(p, np);		/* Continue looking back */
				return 1;
			    }
		/* FALLTHROUGH */
		default:
		    if (p->Ptype == PTV_IMMED)
			{
			p->Pop = P_MOVE;	/* unknown const case, make P_MOVE */
			p->Pvalue = - p->Pvalue; /* with negated value */
			}
		    else
			{
			return 0;		/* Give up, no optimization done */
			}
		}	/* Drop through to following case */

	/* FALLTHROUGH */
	case P_MOVE:
	    /* IBP updates its operand in place; unlike ordinary OP R,S forms,
	    ** folding MOVE R,M / IBP R into IBP M does not leave the updated
	    ** value in R.  A later use/store of R would therefore see stale data.
	    */
	    if (np->Pop == P_IBP)
		break;
	    rrpre3(p, np, np->Pop);		/* Always takes care of everything */
	    return 1;

	case P_HRRZ:
	case P_HLRZ:
	/* Turn HLRZ/HRRZ S,X
	**	...
	**	HRRE R,S
	** into:
	**	HLRE/HRRE R,X
	*/
	    if (op == P_HRRE)
		{
		rrpre3(p, np, (p->Pop == P_HRRZ ? P_HRRE : P_HLRE));
		return 1;
		}
	    break;

	case P_ADD:
	case P_SUB:
	/*
	** fold:  ADD/SUB S,x
	**		...
	**        ADD/SUB R,S
	**
	** into:  ADD/SUB R,x
	**		...
	**        ADD/SUB R,S
	**
	** and keep looking back for further optimization.
	*/

	/* Ensure that new instr is ADD or SUB before wasting more time */
	    if (op != P_ADD && op != P_SUB)
		break;

	/* Make sure the old ADD/SUB doesn't use its own reg as index */
	    if (rinaddr(p, p->Preg))
		break;

	/* See whether new instr's reg is otherwise referenced.
	** It is OK to skip over references of the form ADD/SUB R,M as
	** the result doesn't depend on the order of the instrs, and this
	** allows us to convert things like (a - (b+c+d)) into (a-b-c-d).
	*/
	    for (q = before(np); (q = chkref(p, q, r)) != NULL; q = before(q))
		{
	    /* Found a reference, see if it's OK to pass by. */
		if ((q->Pop == P_ADD || q->Pop == P_SUB)	/* ADD or SUB? */
		  && r == q->Preg && !rinaddr(q, r))	/* R ref reg-only? */
		    continue;				/* Win, skip over */
		return 0;			/* Failed, no optimization. */
		}

	    if (op == P_SUB)		/* If new instr is SUB, */
		p->Pop ^= (P_ADD ^ P_SUB);	/* invert sense of old instr */

	    p->Preg = r;		/* Make ADD/SUB S,x be ADD/SUB R,x */
	    codrrx(before(p), np);	/* Continue optimization if can */
	    return 1;


	case P_SETZ+POF_BOTH:
	case P_SETO+POF_BOTH:
	    if (p->Ptype == PTA_REGIS
	      && (op == P_DMOVE || op == P_DMOVN)
	      && p->Preg == s && p->Pr2 == s + 1)
		{

	    /*
	    ** fold:  SETZB/SETOB S,S+1
	    **		...
	    **        DMOVE R,S
	    **
	    ** into:  SETZB R,R+1
	    */
		np->Pop = p->Pop;		/* Copy the SETZB/SETOB */
		np->Pr2 = np->Preg + 1;
		dropinstr(p);			/* Flush the SETZB/SETOB */
		return 1;
		}
	    break;
	}		/* end switch (p->Pop&(POF_OPCODE|POF_BOTH)) */

    return 0;		/* No optimization done, let caller handle it. */
}

/* RRPRE3 - (yet another) Auxiliary for RRPRE2 (auxiliary for CODE00).
**	Handles case where previous instruction was a "data fetch"
** into register and new instruction operates on that register.
**	 Turn:	p->   MOVE/MOVEI/DMOVE/HRRZ/HLRZ S,x
**		      ...
**		np->  OPN R,S
**	 into:
**		      OP    R,x
**
** If there are instructions in between (i.e. "..." contains something) then
** we need to be careful that:
**	(1) S is NOT otherwise referenced between the MOVE and the OP.
**		This rule has already been satisfied by findrset().
**	(2) OP is not a skip, and R is NOT referenced in "..."
**		If so, then OK to fold: the MOVE to OP R,X.
**	Otherwise, (3) X doesn't use anything that "..." changes.
**		If so, we can flush the MOVE and add the OP R,X.
**	Otherwise give up.
**
** Note that the OP of a munged instruction may differ from the OPN
** originally pointed to by "np".  This is so the halfword code can
** provide the correct op.  For most calls, the "op" arg will just be
** "np->Pop".
*/
static void
rrpre3(struct pcode * p, struct pcode * np, int op)
{
    PCODE *q;
    INT stkoff;


    /* don't make an XPUSHI - foldstack works better without it */
    if (p->Ptype == PTV_IINDEXED && op == P_PUSH)
	{
	rrpop2(np);		/* Just do simple post-ops */
	return;
	}

    /* Ensure the two instructions are handling the same registers.
    ** If one is a doubleword op and the other isn't then they might not.
    ** e.g. the sequence MOVE S,x / SETZ S+1, / DFAD R,S
    */
    if (rbinreg(p) != rbinaddr(np))	/* Verify p's S == np's S */
	{
	rrpop2(np);			/* Oops, just do simple post-ops */
	return;
	}

    /* If there's nothing in between, then no need to worry about rules.
    ** Otherwise, check for rule (2).
    */
    q = before(np);

    if (Register_Preserve (p->Preg) || Register_Preserve(np->Pr2))
	return;			/* avoid faulty opt */

    if ( p == q			/* Always OK if no "..." in between */
      || (!isskip(np->Pop)		/* Otherwise must not be skip */
	 && !chkref(p, q, np->Preg)))	/* and "..." must not reference R. */
	{
	p->Pop = op;			/* Win!!  fold: MOVE S,X */
	p->Preg = np->Preg;		/* to OP R,X */
	dropinstr(np);			/* Flush new instr */
	rrpopt(p);			/* Do more checks on result! */
	return;
	}

    /* Checking for R usage didn't work out.
    ** Now apply painful check of X by going forwards from
    ** the MOVE, checking each instr to make sure it cannot reference
    ** X or use a register that X does.
    */
    if (chkmref(p, np, (INT *) &stkoff) == NULL)
	{
	/* WON!  Zap the MOVE and turn new op into OP R,X */
	int r = np->Preg;	/* Save R */
	*np = *p;		/* Copy MOVE into added instr */
	p->Pop = P_NOP;		/* Then zap the MOVE */
	np->Pop = op;		/* fold: MOVE S,X */
	np->Preg = r;		/* to OP R,X */
	if (stkoff)		/* If stack indexed through and it changed, */
	    np->Poffset -= stkoff;	/* adjust offset of instruction. */
	rrpopt(np);		/* Do post-optimization checks now! */
	return;
	}
    rrpop2(np);			/* Failed, apply standard post-opts. */
}

/* CHKREF(p, q, reg) - Checks series of instrs to see if any reference
**	the specified register.
**	Searches backwards from q (INCLUSIVE) to p (EXCLUSIVE).
**	Returns non-NULL pointer to most recent reference found.
**	Returns NULL if no reference seen.
*/
static PCODE *
chkref(struct pcode * begp, struct pcode * q, int r)
{
    for (; q && begp != q; q = before(q))
	{
#if 0
	if (rincode(q, r))
#else
	    /*
	     * FW, 22-Mar-93 (SPR 41)
	     *
	     * This function needs to interpret transfers of control
	     * as "references" to the register, because we must
	     * assume that the code jumped to may use the register's
	     * contents.  Therefore, we will break on any of PUSHJ,
	     * POPJ, JUMPn, or JRST.
	     */

	int	    opcode = q->Pop & POF_OPCODE;

	if (rincode (q, r) || opcode == P_PUSHJ || opcode == P_POPJ
		|| opcode == P_JUMP || opcode == P_JRST)
#endif
	    return q;		/* Quit loop if find a reference */
	}
    return NULL;
}

/* CHKMREF - Check an instr sequence for anything affecting a mem ref.
**	BEGP points to start (INCLUSIVE), the instr here defines X.
**	ENDP points to end (EXCLUSIVE)
** Returns pointer to offending instr if any ref found.
**
**	Applies painful check of X (for OP R,X) by going forwards from
** the begp (inclusive), checking each instr to make sure it cannot reference
** X or use a register that X does, since we want to move the X reference
** from the instr at begp to the instr at endp.
**	This involves three distinct checks:
**	(1) If the original instr at begp modifies memory, then this
**		is considered a conflicting reference and we fail, since
**		the instr at endp probably depends on the new value.
**	(2) If X uses a register, that register must not be changed
**		by an intervening instruction!
**	(3) If an intervening instruction references the same place
**		as X (or even threatens to do so), then it must not change
**		memory.
 *
 *	(4) FW, 22-Mar-93: If an intervening instruction jumps, we
 *		must assume that the code jumped to depends on the
 *		value.
 * 
** Must track stack offset both in order to properly check for sameaddr(),
** and to correct the X if it uses the stack (and something in "..."
** changed stack ptr).
**
**	If X depends on stack pointer value, the returned
** offset will be nonzero and X needs it added to its Poffset if it is
** to be used in the instruction at ENDP.
**	If at any point the stack offset changes in a way that means X
** would become non-existent (not on the stack, i.e. a positive stack
** offset) then the routine fails immediately.
*/

PCODE *
chkmref (PCODE *begp, PCODE *endp, INT *aoff)
    {
    PCODE *q;
    INT ioff;
    INT stkoff = 0;
    int xreg = -1;		/* Default assumes X uses no reg */

    *aoff = 0;

    if ((begp->Pop & POF_BOTH) || (popflg[begp->Pop & POF_OPCODE] & PF_MEMCHG))
	return begp;		/* Start instr changes mem! Fail. */

    switch (begp->Ptype & PTF_ADRMODE)
	{
	case PTA_BYTEPOINT:
	case PTA_MINDEXED:
	    if (begp->Pindex)		/* If nonzero index reg, remember it */
		xreg = begp->Pindex;

	    if (xreg != R_SP)		/* Unless stack ptr, forget offset */
		aoff = NULL;
	    break;

	case PTA_REGIS:
	    xreg = begp->Pr2;		/* Then drop thru to ignore offset */

	/* FALLTHROUGH */
	default:
	    aoff = NULL;
	    break;
	}

    for (q = after (begp); q; q = after (q))
	{
	if (q == endp)
	    {
	    if (aoff)
		*aoff = stkoff;

	    return NULL;		/* Won, return 0 (no reference) */
	    }

	/* Not yet done with loop, check out this instr */

	if (sameaddr (begp, q, stkoff) || alias (begp, q, stkoff))
	    {
	    if ((q->Pop & POF_BOTH) || (popflg[q->Pop & POF_OPCODE] & PF_MEMCHG))
		break;	/* P and Q may refer to same place, and the instr */
	    }		/* changes memory, so must fail. */

	/* Account for stack changes.
	**  A PUSH, POP, or ADJSP of the stack can be understood and the
	** rrchg test skipped if we are using the stack ptr as index reg
	** (otherwise rrchg would flunk the instr).
	*/

	ioff = 0;

	switch (q->Pop & POF_OPCODE)
	    {
	    case P_PUSH:
		if (q->Preg == R_SP)
		    ioff = 1;
		break;

	    case P_POP:
		if (q->Preg == R_SP)
		    ioff = -1;
		break;

	    case P_ADJSP:
		if (q->Preg == R_SP)
		    ioff = q->Pvalue;
		break;

	    /*
	     * FW, 22-Mar-93 (SPR 41) : any transfer of control
	     * should be construed as a reference to the register.
	     */

	    case P_PUSHJ :
	    case P_POPJ :
	    case P_JUMP :
	    case P_JRST :
		return q;
	    }

	stkoff += ioff;

	if (ioff && aoff)	/* Stack change, and using ptr as index reg */
	    {
	    if ((begp->Poffset - stkoff) > 0)
		break;		/* Cell no longer protected by stack ptr */

	    continue;		/* OK, can skip rrchg test */
	    }
	else if (xreg >= 0 && rrchg (q, xreg))
	    break;		/* Modifies register that X needs */
	}

    if (aoff)
	*aoff = stkoff;

    return q;
    }

/* RRPOPT - Post-optimization after adding reg-reg instruction.
**	We've just turned the sequence
**	fold:	MOVE S,x   into:  OP R,x
**		OP R,S
** and want to do further optimization on the new op/address combination
** resulting from that fold.
**	P points to the OP R,x instruction.
*/
static void
rrpopt(struct pcode * p)
{
    PCODE *q;
    int op;

    if (Register_Preserve(p->Pr2))    /* Avoids faulty optimizations  */
	return;

    /* P_CAML for an immediate type needs to become P_CAIL... */
    if ((op = immedop(p->Pop)) != 0 && (p->Ptype & PTF_IMM))
	{
	p->Pop = op;		/* fold: op to immediate type, and make */
	p->Ptype &= ~ PTF_IMM;	/* operand PTA_RCONST instead of PTV_IMMED */
	foldskip(p, 1);		/* Fix up P_CAI */
	return;
	}

    q = before(p);	/* look back before munged move.  May be NULL! */

    /* Big post-optimization switch: see what the added op was.
    ** Breaking out just returns, since instr has already been added.
    */
    switch (p->Pop & POF_OPCODE)
	{
	case P_PUSH:
	    code8(P_ADJSP, VR_SP, 0);	/* try adjustment */
	    break;			/* return */

	case P_SKIP:
	    if (p->Pop == P_SKIP+POF_ISSKIP+POS_SKPE
	      && p->Ptype == PTV_IINDEXED)
		/* Fold:	P_SKIPEI R,addr
		** Into:	MOVEI R,addr
		**	assuming that the SKIP is part of a (char *) cast
		** conversion of an immediate address value.
		*/
		p->Pop = P_MOVE;
	    break;			/* return */

	case P_FLTR:
	    if (p->Ptype != PTV_IMMED)
		break;			/* Return, not immediate operand */
	    p->Pop = P_MOVE;		/* fold: FLTRI R,x */
	    p->Ptype = PTA_FCONST;	/* into: MOVSI R,(xE0) */
	    /* This only works if target machine is same as source machine! */
	    p->Pfloat = (float) p->Pvalue;
	    break;			/* return */

	case P_IMUL:
	    if (p->Ptype != PTV_IMMED)
		return;
	    if (p->Pvalue == 1)
		{
		dropinstr(p);	/* drop IMULI R,1 */
		break;		/* then return */
		}

	    if (q && q->Ptype == PTV_IMMED /* !prevskips */
	      && q->Preg == p->Preg)
		switch(q->Pop)
		    {
		    case P_SUB:
		    case P_ADD:
			/*
			** fold:  ADDI/SUBI   R,n
			**        IMULI  R,m
			**
			** into:  IMULI  R,m
			**        ADDI/SUBI   R,m*n
			*/
			q->Pvalue *= p->Pvalue;	/* premultiply constant */
			swappseudo (p, q);	/* put add after multiply */
			p = q;		/* look at multiply for below */
			q = before(p);	/* and before in case another mult */
			if (q == NULL	 /* Check whether safe to drop in */
			  || q->Pop != P_IMUL
			  || q->Ptype != PTV_IMMED
			  || q->Preg != p->Preg)
			    break;		/* No, return now. */
			/* Drop through to next case */

		    /* FALLTHROUGH */
		    case P_IMUL:
			/*
			** fold:  IMULI  R,n
			**        IMULI  R,m
			**
			** into:  IMULI  R,n*m
			*/
			q->Pvalue *= p->Pvalue;	/* multiply both together */
			dropinstr(p);		/* Flush later one */
			break;			/* Then return */

		    case P_MOVN:
		    case P_MOVE:
			/*
			** fold:  MOVEI/MOVNI  R,n
			**        IMULI  R,m
			**
			** into:  MOVEI/MOVNI  R,m*n
			*/
			q->Pvalue *= p->Pvalue;	/* mult const by factor */
			dropinstr(p);		/* flush folded multiply */
			break;			/* then return */
		    }
	    break;

	case P_LDB:
	case P_DPB:
	    foldbyte(p);	/* Attempt some simple opts */
	    break;		/* Done, return */

	case P_ADJBP:
	    foldadjbp(p);	/* fix up ADJBP instruction */
	    break;		/* return */

	case P_SUB:
	    if (p->Ptype == PTV_IMMED
	      && q != NULL
	      && q->Preg == p->Preg
	      && (q->Ptype == PTV_IMMED || q->Ptype == PTV_IINDEXED))
		switch (q->Pop)	/* check safe then switch */
		    {
		    case P_MOVE:
		    case P_ADD:
			q->Poffset -= p->Poffset;
			dropinstr(p);		/* fold:  ADDI/MOVEI R,n */
			foldplus(q);	/*	  SUBI R,m */
			return;		/* into:  ADDI R,n-m */

		    case P_SUB:
			q->Poffset += p->Poffset;
			dropinstr(p);		/* fold:  SUBI R,n */
					/*	  SUBI R,m */
			return;		/* into:  SUBI R,n+m */
		    }
	    /* fall through to foldplus() */

	/* FALLTHROUGH */
	case P_ADD:
	    foldplus(p);		/* do general optimization on add */
	    if (p != previous)		/* Take care of possible ADDI+ADDI */
		foldplus(previous);	/* that sometimes results */
	    break;			/* return */

	case P_CAM:
	    if (q != NULL
	      && q->Ptype == PTA_REGIS /* !prevskips */
	      && q->Pop == P_MOVE
	      && q->Preg == p->Preg)
		{

		/*
		** fold:  P_MOVE  R,S
		**        P_CAMx  R,x
		**
		** into:  P_CAMx  S,x
		*/
		p->Preg = (char) (q->Pr2);	/* flatten tested register */ // FW KCC-NT
		q->Pop = P_NOP;		/* flush useless move */
		}
	    break;			/* Return */

	default:
	    rrpop2(p);		/* Apply usual opts */
	    return;
	}	/* End of huge switch on newly-added op code */
}

/* RRPOP2 - Auxiliary for CODE0, does some optimizations on OP R,S.
*/
static void
rrpop2(struct pcode * p)
{
    if (Register_Preserve(p->Pr2))    /* Avoids faulty optimizations  */
	return;

    switch (p->Pop & POF_OPCODE)
	{
	case P_PUSH:
	    code8(P_ADJSP, VR_SP, 0);	/* Hack stack */
	    break;

	case P_AND:
	case P_IOR:
	case P_XOR:
	    if (!findconst(p))
		inskip(p);		/* turn SKIPA/MOVE/IOR into TLOA/IOR */
	    break;

	case P_CAM:
	    findconst(p);		/* See if can turn CAM into CAI */
	    foldskip(p, 1);
	    break;

	case P_ADJBP:
	    foldadjbp(p);		/* fix up ADJBP instruction */
	    break;

	case P_ADD:
	    findconst(p);		/* See if can make operand immediate */
	    foldplus(p);		/* maybe addition can be fixed now */
	    break;

	case P_MOVE:
	case P_LDB:
	case P_MOVN:
	case P_SETCM:
	case P_SETO:
	case P_SETZ:
	case P_HRRZ:
	case P_HLRZ:
	case P_HRRE:
	case P_HLRE:
	case P_FIX:
	case P_FLTR:
	/* If op is one that CCCSE recognizes as simply setting register, */
	    foldmove(p);		/* try to find common sub-expression! */
	    break;
	}
}

/* FOLDXREF(p) - Attempt to optimize newly-added instr of form OP R,(S).
**	p points to just-added instruction.
*/
static void
foldxref(struct pcode * p)
{
    PCODE *q, *b, *oldprev;



    /* Avoids faulty optimizations in the functions: foldxref, optlsh,
     * foldboth, foldidx, chkmref, and rrchg.
     */
    if (Register_Preserve(p->Pr2))
	return;

    /* First attempt optimizations by looking for an instruction that sets
    ** the index register S.  If found, this will be a single-word op
    ** that is not skipped over and thus is safe to NOP out.
    */
    oldprev = before(p);	/* Find old previous */

    /* If have a single-word op setting S, check simple cases. */
    if ((q = findrset(oldprev, p->Pindex)) != NULL)
	switch (q->Ptype)
	    {

	    case PTV_IMMED:			/* Immediate RCONST */
		switch (q->Pop)
		    {

		/* fold:  MOVEI/MOVNI  S,const
		**		...
		**        OP     R,(S)
		** into:
		**	  OP     R,const (or -const)
		*/
		    case P_MOVN:
			q->Pvalue = - q->Pvalue;
		    /* FALLTHROUGH */
		    case P_MOVE:
			if (q == oldprev)	/* Can we re-use last instr? */
			    {
			    q->Pop = p->Pop;	/* Yes!  Change its op and r */
			    q->Ptype = PTA_RCONST;	/* Take immediateness out */
			    q->Preg = p->Preg;
			    q->Pbsize = p->Pbsize;
			    p = q;		/* Say this is now current instr */
			    flsprev();		/* Then flush instr that was added */
			    }
			else		/* No, zap MOVEI */
			    {
			    p->Ptype = PTA_RCONST;	/* Change our addr mode */
			    p->Pvalue = q->Pvalue;	/* to imm constant */
			    q->Pop = P_NOP;		/* Now can flush the MOVEI */
			    }
		/* Special check here for LSH (possible bitfield hacking) */
			if (p->Pop == P_LSH)
			    optlsh(p);		/* Attempt to optimize LSH */
			return;

		/* fold:  ADDI/SUBI   S,offset
		**		...
		**        OP     R,(S)
		** into:
		**	  OP     R,offset(S)
		*/
		    case P_SUB:
			q->Pvalue = - q->Pvalue;
		    /* FALLTHROUGH */
		    case P_ADD:
			p->Poffset = q->Pvalue;		/* Set offset to added val */
			q->Pop = P_NOP;			/* Then drop the ADDI/SUBI */
			q = before(q);		/* Back up to previous good instr */
					/* for the foldidx coming up. */

		/* If instruction setting the S reg was a MOVE S,I
		** then flush it, as we can use I directly.
		*/
			if ((b = findrset(q, p->Pindex)) != NULL
			  && b->Ptype == PTA_REGIS
			  && b->Pop == P_MOVE
			  && b->Preg == p->Pindex)
			    {
			    p->Pindex = b->Pr2;	/* fold: our index reg to I */
			    b->Pop = P_NOP;	/* and drop needless move */
			    q = NULL;		/* Start foldidx from oldprev */
			    }
			break;	/* Now break out to do foldidx and foldboth */
		    }
		break;

	    case PTV_IINDEXED:		/* If op S,<Immediate MINDEXED> */
		switch (q->Pop)
		    {

		    case P_MOVE:
		/*
		** fold:  P_MOVEI S,addr	(Immediate MINDEXED)
		**		...
		**        OP     R,(S)
		**
		** into:  OP     R,addr
		**
		** Note the code here is somewhat akin to that in rrpre3().
		** Perhaps someday it can be merged.
		*/
			if (q == oldprev)	/* Anything between MOVEI and op? */
		    /* Nope, safe to just re-use the MOVEI instr by
		    ** clobbering it with the op just added.
		    */
			    {
			    q->Pop = p->Pop;	/* Fix it up */
			    q->Ptype = PTA_MINDEXED;
			    q->Preg = p->Preg;
			    q->Pbsize = p->Pbsize;
			    dropinstr(p);	/* And flush original instr */
			    }
			else
			    {
		    /* Hmm, there are instrs between MOVEI and op, need
		    ** to check them out carefully.  See comments in chkmref().
		    ** If we win, zap MOVEI and modify current new instr.
		    */
			    INT stkoff;
			    if (chkmref(q, p, &stkoff) == NULL)
				{
				p->Pptr = q->Pptr;	/* Update our new address */
				p->Pindex = q->Pindex;
				p->Poffset = q->Poffset - stkoff;
				q->Pop = P_NOP;		/* Zap the MOVEI */
				}
			    }
			foldboth();		/* Further check last instr */
			return;

		    case P_ADD:			/* ADDI S,addr */
		/*
		** fold:  ADDI   S,addr(I)
		**        OP     R,(S)
		**
		** into:  MOVEI  S,addr(S)
		**	  ADD    S,I
		**        OP     R,(S)
		**
		** and optimize further...
		**
		** NOTE: only do this if ADDI is the first preceding instr
		** (oldprev), because otherwise register I also needs to
		** be checked for intermediate usage, and the "optimize
		** further" routines aren't smart enough yet to go back
		** far enough.
		*/
			if (q == oldprev	/* Ignore if ADD not last thing. */
			  && (!q->Pindex || q->Pindex != p->Preg))
			    {
			    int op = p->Pop;	/* Remember values of orig instr */
			    int r = p->Preg;
			    int s = p->Pindex;
			    INT bsiz = p->Pbsize;
			    int o = q->Pindex;

			    q->Pop = P_MOVE;		/* fold: Q to MOVEI */
			    q->Pindex = s;		/* of (S) */
			    foldidx(q);			/* Try to optimize index reg */
			    s = q->Pindex;		/* Remember it */
			    dropinstr(p);		/* Flush the OP we added */
			    if (o)
				code00(P_ADD, q->Preg, o);	/* followed by P_ADD */
			    code40(op, r, s, bsiz);	/* then try code4 again */
			    return;
			    }
			break;
		    }
		break;
	    } /* End of q->Ptype switch */

    /* No simple optimizations found... */

    foldidx(p);		/* Try folding index reg calculation. */
    foldboth();		/* Check remaining optimizations */
}

/* OPTJRST - Specialized optimization for JRST 0,sym
**	This is only used by code6 at moment.
**	This function does NOT update preserve regs.
*/
static void
optjrst(struct pcode * p)
{
    PCODE *prev, *b, *q;

    if (!p || p->Pop != P_JRST || p->Pindex || p->Poffset
      || (prev = before(p)) == NULL
      || prevskips(prev))
	return;

    switch (prev->Pop & POF_OPCODE)
	{
	case P_SKIP:
	    /* A late register rewrite can expose SKIPx R,R only after the
	    ** normal code6() optimization point.  With identical source and
	    ** destination ACs the SKIP has no value-producing side effect, so
	    ** the following JRST is exactly the inverse JUMP condition.
	    */
	    if (prev->Ptype != PTA_REGIS || prev->Preg != prev->Pr2)
		break;
	    p->Pop = prev->Pop ^
		(P_SKIP ^ P_JUMP ^ POF_ISSKIP ^ POSF_INVSKIP);
	    p->Preg = prev->Pr2;
	    clrskip(p);
	    dropinstr(prev);
	    break;

	case P_JRST:	/* See if possibly dead code precedes the JRST */
	case P_POPJ:
	case P_IFIW:
	    if ((prev->Pop & ~POF_OPCODE)==0)	/* Verify just a simple op */
		{
		reflabel(p->Pptr, -1);		/* Same as dropjump() */
		dropinstr(p);			/* Dead code, drop the JRST! */
		}
	    break;

	case P_CAI:
	    if (prev->Ptype != PTA_RCONST || prev->Pvalue != 0)
		break;

	/* fold:  CAIx  R,0	into:	--
	**	   JRST  addr		JUMPx R,addr
	*/

	/* fold: JRST to JUMP, turn off isskip flag, and invert test */
	    p->Pop = (prev->Pop ^ (P_CAI ^ P_JUMP ^ POF_ISSKIP ^ POSF_INVSKIP));
	    p->Preg = prev->Preg;
	    clrskip(p);
	    dropinstr(prev);		/* Flush the CAI */
	    break;

	case P_ADD:
	case P_SUB:
	    if (prev->Ptype != PTV_IMMED || prev->Pvalue != 1)
		break;

	/* fold:  ADDI R,1	into:  AOJA R,addr
	**         JRST addr
	**
	** and the corresponding SUBI/SOJA form.  These are exact PDP-10
	** increment/decrement-and-jump instructions; no condition is changed.
	*/
	    p->Pop = ((prev->Pop == P_ADD) ? P_AOJ : P_SOJ) + POS_SKPA;
	    p->Preg = prev->Preg;
	    clrskip(p);
	    dropinstr(prev);
	    break;

	case P_AOS:
	case P_SOS:
	    if (prev->Ptype != PTA_REGIS		/* Ensure operand is reg */
	      || (prev->Preg			/* Must be either AOS R */
		&& prev->Preg != prev->Pr2))	/* or AOS R,R */
		break;

	/* fold:  AOSx  R,R	into:	--
	**	   JRST  addr		AOJx R,addr
	*/
	    if (((p->Pop = prev->Pop)&POF_OPCODE) == P_AOS)
		p->Pop ^= (P_AOS ^ P_AOJ ^ POF_ISSKIP ^ POSF_INVSKIP);
	    else
		p->Pop ^= (P_SOS ^ P_SOJ ^ POF_ISSKIP ^ POSF_INVSKIP);
	    p->Preg = (char) (prev->Pr2);		// FW KCC-NT
	    clrskip(p);
	    dropinstr(prev);
	    break;

	case P_CAM:

	/*
	** fold:  ADDI R,1
	**	  CAMN R,x
	**	   JRST $y
	**
	** into:  SUB R,x
	**	  AOJE R,$y
	*/

	    if (prev->Pop & POSF_CMPSKIP)
		break;	/* only CAMN and CAME */
	    for (b = before(prev);
		 b && b->Pop == P_MOVE && b->Preg != prev->Preg;
		 b = before(b))
		;			/* skip over struct finding */

	    if (b == NULL || b->Preg != prev->Preg	/* look for OPI R,1 */
	      || b->Ptype != PTV_IMMED
	      || b->Pvalue != 1 || prevskips(b))
		break;
	    if (b->Pop == P_ADD)
		p->Pop = prev->Pop ^ (P_CAM ^ P_AOJ ^ POF_ISSKIP ^ POSF_INVSKIP);
	    else if (b->Pop != P_SUB)
		break;			/* must be ADDI or SUBI */
	    else				/* now SOJ */
		p->Pop = prev->Pop ^ (P_CAM ^ P_SOJ ^ POF_ISSKIP ^ POSF_INVSKIP);
	    p->Preg = b->Preg;		/* use this register */
	    b->Pop = P_NOP;			/* drop ADDI or SUBI */
	    prev->Pop = P_SUB;		/* make SUB */
	    clrskip(p);			/* following instr no longer skipped */

	    if ((b = before(b)) == NULL || b->Pop != P_JRST
	      || (q = before(b)) == NULL
	      || (q->Pop & (POF_OPCODE + POSF_CMPSKIP)) != P_CAM
	      || q->Preg != p->Preg
	      || !sameaddr(q, prev, 0) || prevskips(q))
		break;

	/*
	** fold:  CAMN R,x
	**	   JRST $y
	**	  SUB R,x
	**	  AOJE R,$z
	**
	** into:  SUB R,x
	**	  JUMPE R,$y
	**	  AOJE R,$z
	*/

	    b->Preg = p->Preg;		/* make JUMPE */
	    b->Pop = q->Pop ^ (P_CAM ^ P_JUMP ^ POF_ISSKIP ^ POSF_INVSKIP);
	    clrskip(b);			/* not skipped */
	    q->Pop = P_SUB;			/* make sub */
	    dropinstr(prev);		/* drop duplicated SUB, fix up */
	}
}
