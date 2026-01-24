/*	CCOUT.C - Output pseudo-code to assembler file
**
**	 (c) Copyright Ken Harrenstien 1989
**		All changes after v.259, 11-Aug-1988
**	 (c) Copyright Ken Harrenstien, SRI International 1985, 1986
**		All changes after v.77, 8-Aug-1985
**
**	Original version (C) 1981  K. Chen
*/

#include "cc.h"
#include "ccgen.h"
#include "ccchar.h"
#include <string.h>
#include <stdlib.h>	/* calloc () */

/* Imported functions */
extern char *estrcpy (char *, char *),
	*fstrcpy (char *, char *, char *);	/* CCASMB */
extern SYMBOL *symfidstr (char *);		/* CCSYM */
extern int foldtrna (PCODE *);			/* CCJSKP */
extern void errfopen (char *, char *);
extern void dmpmlbuf (void);

/* Exported functions (defined here and used externally) */
void outinit (void), outdone (int),
	outlab (SYMBOL *), outscon (char *, int, int),
	outid (char *), outmidef (SYMBOL *), outmiref (SYMBOL *),
	outptr (SYMBOL *, int, INT), outstr (char *), outnum (INT),
	outsix (unsigned INT);
void realcode (PCODE *);
int codeseg (void), dataseg (void), bssseg (void), prevseg (int);
int outflt (int, INT *, int);			/* CCGEN */
INT binexp (unsigned INT);			/* CCEVAL */
int adjboffset (INT, INT *, int);
void outsix (unsigned INT), outnl (void);
void outpghdr (void); /* KAR, added for mixed listing page header output */
 /* profiler functions: 9/15/89 by MVS */
void outprolog (SYMBOL *), outepilog (SYMBOL *);
static int makprefile (void); /* KAR-2/92, create ENTRY list in side .MAC */

/* Internal Functions */
static void outrj6 (unsigned INT);
static char *ahmacs (void), *octcpy (char *, unsigned INT);
static void simptrcnv (PCODE *), simsmove (PCODE *),
	simufltr (PCODE *), simfltr (PCODE *), simfix (PCODE *),
	simdsngl (PCODE *),
	simuidiv (PCODE *), simsidiv (PCODE *), simsubbp (PCODE *), simdfix (PCODE *),
	simdmovx (PCODE *),
	simadjsp (PCODE *), simadjbp (PCODE *), simdfop (PCODE *),
	outadjsp (INT), outdmovn (int);
static void outadjbp_body (void);
static void outmpdbl (INT *, int), outpreamble (void),
	outlpnum (unsigned INT, int), outinstr (PCODE *),
	outrawnum (unsigned INT), outpnum (unsigned INT), outop (int), outreg (int),
	outpti (int, INT), outaddress (PCODE *), outasmh (void),
	outlpnum (unsigned INT, int);
static int fltpow2 (double), outdecl (void),
	   obplh (INT, INT *, int), bumpaddr (PCODE *), gasbplhval (int);
static int directop (int);
static int bigfloat (PCODE *);
#ifndef __COMPILER_KCC__
static unsigned long host_u32_from_bytes (unsigned char *);
static unsigned long host_float_bits (float *);
static void host_double_bits (double, unsigned long *, unsigned long *);
static void host_pdp10_double_words (double, unsigned INT *, unsigned INT *);
#endif
static void outfile (void); /* KAR-1/92, output of input filename (for NPD) */
static void outnpd (PCODE *); /* KAR-1/92, added for output of NPD code */

extern char *mlbptr;
extern char mainname[];
int	    _word_cnt;		/* KAR-2/91, function wd cnt stat */
static int whichseg;	/* 1 = code, -1 = data, 0 = unknown */


/*
 * Tables to handle references to built-in C RunTime symbols without
 * going through the overhead of using the symbol table.
 * Currently this is only used to determine which CRT symbols need to
 * be declared EXTERN in the assembly-language output file.
 * Note that $$CVER is always defined and declared in every KCC module,
 * as are the appropriate $$CPxx symbols.
 */

#ifdef	MULTI_SECTION /* FW 2A (51) */
#define crtsyms \
	crtsym(CRT_,"")			/* avoid zero index */\
	crtsym(CRT_CRT, "$$$CRT")	/* CRT module "entry point" */\
	crtsym(CRT_CPU, "$$$CPU")	/* CPU module "entry point" */\
	crtsym(CRT_CVER,  "$$CVER")	/* KCC code & library version # */\
	crtsym(CRT_CPUKA, "$$CPKA")	/* KCC/CPU type KA-10 */\
	crtsym(CRT_CPUKI, "$$CPKI")	/* KCC/CPU type KI-10 */\
	crtsym(CRT_CPUKS, "$$CPKS")	/* KCC/CPU type KS-10 and KL-10A */\
	crtsym(CRT_CPUKL0,"$$CPKL")	/* KCC/CPU type KL-10B, section 0 */\
	crtsym(CRT_CPUKLX,"$$CPKX")	/* KCC/CPU type KL-10B, non-0 sect */\
	crtsym(CRT_SECT, "$$SECT")	/* CPU Section # being loaded into */\
	crtsym(CRT_BPH0, "$$BPH0")	/* CPU Byte ptr LHs for 18-bit bytes*/\
	crtsym(CRT_BPH1, "$$BPH1")	/* CPU */\
	crtsym(CRT_BP90, "$$BP90")	/* CPU Byte ptr LHs for 9-bit bytes */\
	crtsym(CRT_BP91, "$$BP91")	/* CPU */\
	crtsym(CRT_BP92, "$$BP92")	/* CPU */\
	crtsym(CRT_BP93, "$$BP93")	/* CPU */\
	crtsym(CRT_BP80, "$$BP80")	/* CPU Byte ptr LHs for 8-bit bytes */\
	crtsym(CRT_BP81, "$$BP81")	/* CPU */\
	crtsym(CRT_BP82, "$$BP82")	/* CPU */\
	crtsym(CRT_BP83, "$$BP83")	/* CPU */\
	crtsym(CRT_BP70, "$$BP70")	/* CPU Byte ptr LHs for 7-bit bytes */\
	crtsym(CRT_BP71, "$$BP71")	/* CPU */\
	crtsym(CRT_BP72, "$$BP72")	/* CPU */\
	crtsym(CRT_BP73, "$$BP73")	/* CPU */\
	crtsym(CRT_BP74, "$$BP74")	/* CPU */\
	crtsym(CRT_BP60, "$$BP60")	/* CPU Byte ptr LHs for 6-bit bytes */\
	crtsym(CRT_BP61, "$$BP61")	/* CPU */\
	crtsym(CRT_BP62, "$$BP62")	/* CPU */\
	crtsym(CRT_BP63, "$$BP63")	/* CPU */\
	crtsym(CRT_BP64, "$$BP64")	/* CPU */\
	crtsym(CRT_BP65, "$$BP65")	/* CPU */\
	crtsym(CRT_BPPS, "$$BPPS")	/* CPU Mask for BP P+S field */\
	crtsym(CRT_BPSZ, "$$BPSZ")	/* CPU BP LH to get BP size field */\
	crtsym(CRT_BSHF, "$$BSHF")	/* CPU # bits to shift in P_SUBBP */\
	crtsym(CRT_BMP6, "$$BMP6")	/* CPU val for 6bit MULI in P_SUBBP */\
	crtsym(CRT_BMP7, "$$BMP7")	/* CPU  "   "  7-bit " */\
	crtsym(CRT_BMP8, "$$BMP8")	/* CPU  "   "  8-bit " */\
	crtsym(CRT_BMP9, "$$BMP9")	/* CPU  "   "  9-bit " */\
	crtsym(CRT_BMPH, "$$BMPH")	/* CPU  "   "  18-bit " */\
	crtsym(CRT_PH90, "$$PH90")	/* CPU Instr #0 for 18->9 cnv */\
	crtsym(CRT_PH91, "$$PH91")	/* CPU Instr #1 for 18->9 cnv */\
	crtsym(CRT_P9H0, "$$P9H0")	/* CPU Instr #0 for 9->18 cnv */\
	crtsym(CRT_P9H1, "$$P9H1")	/* CPU Instr #1 for 9->18 cnv */\
	crtsym(CRT_P9H2, "$$P9H2")	/* CPU Instr #2 for 9->18 cnv */\
	crtsym(CRT_START,"$START")	/* CRT start location */\
	crtsym(CRT_RET,  "$RET")	/* CRT Convenient return label */\
	crtsym(CRT_RETZ, "$RETZ")	/* CRT    ditto, returns 0 */\
	crtsym(CRT_RETF, "$RETF")	/* CRT    ditto, returns 0 */\
	crtsym(CRT_RETP, "$RETP")	/* CRT    ditto, returns 1 */\
	crtsym(CRT_RETT, "$RETT")	/* CRT    ditto, returns 1 */\
	crtsym(CRT_RETN, "$RETN")	/* CRT    ditto, returns -1 */\
	crtsym(CRT_ZERO, "$ZERO")	/* CRT Double 0.0 constant */\
	crtsym(CRT_ADJBP, "$ADJBP")	/* CRT ADJBP simulation rtn */\
	crtsym(CRT_KDFAD, "$KDFAD")	/* KA10 double floating add */\
	crtsym(CRT_KDFSB, "$KDFSB")	/* KA10 double floating subtract */\
	crtsym(CRT_KDFMP, "$KDFMP")	/* KA10 double floating multiply */\
	crtsym(CRT_KDFDV, "$KDFDV")	/* KA10 double floating divide */\
	crtsym(CRT_BPMUL, "$BPMUL")	/* CRT BP mul table (for P_SUBBP) */\
	crtsym(CRT_BPADT, "$BPADT")	/* CRT BP table of $BPADn (for ") */\
	crtsym(CRT_BPAD6, "$BPAD6")	/* CRT BP 6-bit add table (for ") */\
	crtsym(CRT_BPAD7, "$BPAD7")	/* CRT BP 7-bit add table (for ") */\
	crtsym(CRT_BPAD8, "$BPAD8")	/* CRT BP 8-bit add table (for ") */\
	crtsym(CRT_BPAD9, "$BPAD9")	/* CRT BP 9-bit add table (for ") */\
	crtsym(CRT_BPADH, "$BPADH")	/* CRT BP 18-bit add table (for ") */\
	crtsym(CRT_BPCNT, "$BPCNT")	/* CRT Byte ptr auxiliary */\
	crtsym(CRT_SUBBP, "$SUBBP")	/* CRT Byte ptr subtraction aux */\
	crtsym(CRT_DFIX,  "$DFIX")	/* CRT Aux for (int) (double) cast */\
	crtsym(CRT_DFIXS, "$DFIXS")	/* CRT Aux for (int) (double) cast */\
	crtsym(CRT_DFLTS, "$DFLTS")	/* CRT Aux for (double) (int) cast */\
	crtsym(CRT_SPUSH, "$NSPUSH")	/* CRT Aux to put struct on stack */\
	crtsym(CRT_SPOP,  "$NSPOP")	/* CRT Aux to get struct from stack */\
        /* KAR-1/92, added symbol for null pointer det. function */\
	crtsym(CRT_NPD,   "$CFNP")	/* Null pointer det. function */
#else
#define crtsyms \
	crtsym(CRT_,"")			/* avoid zero index */\
	crtsym(CRT_CRT, "$$$CRT")	/* CRT module "entry point" */\
	crtsym(CRT_CPU, "$$$CPU")	/* CPU module "entry point" */\
	crtsym(CRT_CVER,  "$$CVER")	/* KCC code & library version # */\
	crtsym(CRT_CPUKS, "$$CPKS")	/* KCC/CPU type KS-10 and KL-10A */\
	crtsym(CRT_BPH0, "$$BPH0")	/* CPU Byte ptr LHs for 18-bit bytes*/\
	crtsym(CRT_BPH1, "$$BPH1")	/* CPU */\
	crtsym(CRT_BP90, "$$BP90")	/* CPU Byte ptr LHs for 9-bit bytes */\
	crtsym(CRT_BP91, "$$BP91")	/* CPU */\
	crtsym(CRT_BP92, "$$BP92")	/* CPU */\
	crtsym(CRT_BP93, "$$BP93")	/* CPU */\
	crtsym(CRT_BP80, "$$BP80")	/* CPU Byte ptr LHs for 8-bit bytes */\
	crtsym(CRT_BP81, "$$BP81")	/* CPU */\
	crtsym(CRT_BP82, "$$BP82")	/* CPU */\
	crtsym(CRT_BP83, "$$BP83")	/* CPU */\
	crtsym(CRT_BP70, "$$BP70")	/* CPU Byte ptr LHs for 7-bit bytes */\
	crtsym(CRT_BP71, "$$BP71")	/* CPU */\
	crtsym(CRT_BP72, "$$BP72")	/* CPU */\
	crtsym(CRT_BP73, "$$BP73")	/* CPU */\
	crtsym(CRT_BP74, "$$BP74")	/* CPU */\
	crtsym(CRT_BP60, "$$BP60")	/* CPU Byte ptr LHs for 6-bit bytes */\
	crtsym(CRT_BP61, "$$BP61")	/* CPU */\
	crtsym(CRT_BP62, "$$BP62")	/* CPU */\
	crtsym(CRT_BP63, "$$BP63")	/* CPU */\
	crtsym(CRT_BP64, "$$BP64")	/* CPU */\
	crtsym(CRT_BP65, "$$BP65")	/* CPU */\
	crtsym(CRT_BPPS, "$$BPPS")	/* CPU Mask for BP P+S field */\
	crtsym(CRT_BPSZ, "$$BPSZ")	/* CPU BP LH to get BP size field */\
	crtsym(CRT_BSHF, "$$BSHF")	/* CPU # bits to shift in P_SUBBP */\
	crtsym(CRT_BMP6, "$$BMP6")	/* CPU val for 6bit MULI in P_SUBBP */\
	crtsym(CRT_BMP7, "$$BMP7")	/* CPU  "   "  7-bit " */\
	crtsym(CRT_BMP8, "$$BMP8")	/* CPU  "   "  8-bit " */\
	crtsym(CRT_BMP9, "$$BMP9")	/* CPU  "   "  9-bit " */\
	crtsym(CRT_BMPH, "$$BMPH")	/* CPU  "   "  18-bit " */\
	crtsym(CRT_PH90, "$$PH90")	/* CPU Instr #0 for 18->9 cnv */\
	crtsym(CRT_PH91, "$$PH91")	/* CPU Instr #1 for 18->9 cnv */\
	crtsym(CRT_P9H0, "$$P9H0")	/* CPU Instr #0 for 9->18 cnv */\
	crtsym(CRT_P9H1, "$$P9H1")	/* CPU Instr #1 for 9->18 cnv */\
	crtsym(CRT_P9H2, "$$P9H2")	/* CPU Instr #2 for 9->18 cnv */\
	crtsym(CRT_START,"$START")	/* CRT start location */\
	crtsym(CRT_RET,  "$RET")	/* CRT Convenient return label */\
	crtsym(CRT_RETZ, "$RETZ")	/* CRT    ditto, returns 0 */\
	crtsym(CRT_RETF, "$RETF")	/* CRT    ditto, returns 0 */\
	crtsym(CRT_RETP, "$RETP")	/* CRT    ditto, returns 1 */\
	crtsym(CRT_RETT, "$RETT")	/* CRT    ditto, returns 1 */\
	crtsym(CRT_RETN, "$RETN")	/* CRT    ditto, returns -1 */\
	crtsym(CRT_ZERO, "$ZERO")	/* CRT Double 0.0 constant */\
	crtsym(CRT_ADJBP, "$ADJBP")	/* CRT ADJBP simulation rtn */\
	crtsym(CRT_KDFAD, "$KDFAD")	/* KA10 double floating add */\
	crtsym(CRT_KDFSB, "$KDFSB")	/* KA10 double floating subtract */\
	crtsym(CRT_KDFMP, "$KDFMP")	/* KA10 double floating multiply */\
	crtsym(CRT_KDFDV, "$KDFDV")	/* KA10 double floating divide */\
	crtsym(CRT_BPMUL, "$BPMUL")	/* CRT BP mul table (for P_SUBBP) */\
	crtsym(CRT_BPADT, "$BPADT")	/* CRT BP table of $BPADn (for ") */\
	crtsym(CRT_BPAD6, "$BPAD6")	/* CRT BP 6-bit add table (for ") */\
	crtsym(CRT_BPAD7, "$BPAD7")	/* CRT BP 7-bit add table (for ") */\
	crtsym(CRT_BPAD8, "$BPAD8")	/* CRT BP 8-bit add table (for ") */\
	crtsym(CRT_BPAD9, "$BPAD9")	/* CRT BP 9-bit add table (for ") */\
	crtsym(CRT_BPADH, "$BPADH")	/* CRT BP 18-bit add table (for ") */\
	crtsym(CRT_BPCNT, "$BPCNT")	/* CRT Byte ptr auxiliary */\
	crtsym(CRT_SUBBP, "$SUBBP")	/* CRT Byte ptr subtraction aux */\
	crtsym(CRT_DFIX,  "$DFIX")	/* CRT Aux for (int) (double) cast */\
	crtsym(CRT_DFIXS, "$DFIXS")	/* CRT Aux for (int) (double) cast */\
	crtsym(CRT_DFLTS, "$DFLTS")	/* CRT Aux for (double) (int) cast */\
	crtsym(CRT_SPUSH, "$NSPUSH")	/* CRT Aux to put struct on stack */\
	crtsym(CRT_SPOP,  "$NSPOP")	/* CRT Aux to get struct from stack */\
        /* KAR-1/92, added symbol for null pointer det. function */\
	crtsym(CRT_NPD,   "$CFNP")	/* Null pointer det. function */
#endif
			
#define crtsym(idx, sym) idx,
enum { crtsyms CRT_N };		/* Define the CRT_ indices plus count */
#undef crtsym

static int crtref[CRT_N];		/* Table of CRT sym reference counts */
static int simadjbp_ref;		/* Need one local GAS PDP-6 ADJBP helper */
#define crtsym(idx, sym) sym,
static char *crtsnam[CRT_N] = { crtsyms };	/* Table of CRT symbol names */
#undef crtsym

/*
 * outinit
 *
 * Per-module initialization of MACRO output.
 */

void
outinit (void)
{
    int		i;


    for (i = CRT_N; --i >= 0;)	/* Reset reference counts */
	crtref[i] = 0;
    simadjbp_ref = 0;

    /*
     * These two runtime symbols are ALWAYS implicitly referenced;
     * they refer to the CRT and CPU runtime modules.
     * See outdone () for a special reset check, though.
     */

    ++crtref[CRT_CRT];
    ++crtref[CRT_CPU];

    outpreamble ();		/* Output assembler preamble */
    whichseg = 1;		/* Now starting in code section */
    }

/*
 * outfile
 *
 * Output filename as data for null pointer detection
 */

static
void
outfile (void)
{
    int		i = 0;
    int		slen = strlen (inpfname);
    char	*tfname = inpfname;


    outstr ("\n$NPDFN:");

    do
	{
	if (! (i % 4))
	    outstr ("\n\tBYTE (9) ");
	else
	    outstr (", ");

	outnum ((unsigned char)*tfname);
	tfname++;
	i++;
	}
    while (i < slen);
    }

/*
 * outnpd
 *
 * Output code for Null Pointer Detection; added by KAR-1/92
 */

static
void
outnpd (PCODE *p)
    {
    int	label;
    static int	lbl_num = 0;


    label = lbl_num++;
    outstr ("SKIPE\t");
    outaddress (p);
    outnl ();
    outstr ("\t JRST\t$N");
    outrawnum ((unsigned INT)label);
    outnl ();
    outstr ("\tPUSH\t17,[");
    outnum (p->p_im.p_chnl);	/* used chnl to store line # */
    outstr ("]\n");

#ifdef	MULTI_SECTION /* FW 2A (51) */
    outstr ("\tPUSH\t17,[$$BP90+$$SECT,,$NPDFN]\n");
#else
    outstr ("\tPUSH\t17,[$$BP90,,$NPDFN]\n");
#endif

    outstr ("\tPUSHJ\t17,$CFNP\n");
    outadjsp (-2);
    outstr ("$N");
    outrawnum ((unsigned INT)label);
    outstr ("==.");

    if (mlist)
	oline += 7;
    }

/*
 * outpreamble
 *
 * Emit the MACRO "preamble".
 */

static
void
outpreamble (void)
{
    if (asmdialect == ASM_GAS)
	{
	outstr ("\t.text\n");
	return;
	}

    outstr ("\tTITLE\t");		/* Make TITLE pseudo-op */

    if (module_pragma)
	outstr (title);
    else
	outstr (inpfmodule);		/* with name of input file as title */

    outnl ();

    /*
     * Output the assembler header, either by generating it
     * ourself or copying it from a user-specified file.
     */

    outasmh ();
    }

/*
 * outdone
 *
 * Per-module completion
 */

void
outdone (int mainf)
    {
    register int i;
    register SYMBOL *s;
    static char old_mainf = 0;


    if (old_mainf)
	mainf = 0;
    else
	old_mainf = mainf;

    
    /* KAR-1/92, output string for filename */
    /* Output filename string ($NPDFN) if null ptr detection active */

    if (debcsi == KCC_DBG_NULL)
	outfile ();

    if (asmdialect == ASM_GAS)
	{
	outnl ();
	codeseg ();
	if (simadjbp_ref) {
	    outstr ("%ADJBPH:\n");
	    outadjbp_body ();
	    outstr ("\tPOPJ\t17,\n");
	}
	makprefile ();
	outdecl ();
	outnl ();
	return;
	}

    outnl ();
    codeseg ();				/* make sure in code segment */

    outstr ("\n\tLIT\n");

    if (mlist)
	oline += 2;

    /* "main" module needs entry vector set.  This crock is necessary
    ** because FAIL requires that the arg to END be defined in the current
    ** module!
    */

    if (mainf)
	{
	if (mlist)
	    oline += 4;

	outstr ("$$STRT: JRST $START##\n");
	outstr ("\tJRST $START+1\n");
	outstr ("\t0\n");		/* 3rd wd for T20 ver info */
	++crtref[CRT_START];
	}

    switch (abs (debcsi))
	{
	void	    outsymtab (void);


	case KCC_DBG_SDBG:
	case KCC_DBG_FDBG:		    /* FW 2A (42) PPS4575 */
	    outsymtab ();
	    break;


	default:
	    break;
	}

    /* Output EXTERN declarations for any C runtime symbols we used */

    /* Normally the CPU and CRT modules are always reffed, but just
    ** to make life easier for us KCC implementors, we check here
    ** for compiling those modules themselves, and avoid sending out
    ** an EXTERN if so!
    */

    /* EXTERN for $$$CRT */

    if ((!mainf) ||
	 ((s = symfidstr ("`$$$CRT")) != NULL && s->Sclass == SC_EXTDEF))
	crtref[CRT_CRT] = 0;		/* Is defined, pretend not reffed */

    if ((s = symfidstr ("`$$$CPU")) != NULL && s->Sclass == SC_EXTDEF)
	crtref[CRT_CPU] = 0;		/* Is defined, pretend not reffed */

    if (debcsi == KCC_DBG_NULL)
	{
	crtref[CRT_BP90]++;
#ifdef	MULTI_SECTION /* FW 2A (51) */
	crtref[CRT_SECT]++;
#endif
	crtref[CRT_NPD]++;
	}

    /* Emit symbol requests for all referenced C runtime support stuff */

    for (i = CRT_N; --i >= 0; )
	{
	if (crtref[i] && i != CRT_START)
	    {
	    outstr ("\tEXTERN\t");
	    outstr (crtsnam[i]);
	    outnl ();
	    }
	}

    outdecl ();			/* Output user-defined INTERN and EXTERNs */

    /* Finally, output entry vector if any */

    makprefile ();	/* KAR-2/92, added to output entry pts in .MAC file */

    if (module_pragma)
	outstr ("\tPRGEND");
    else
	outstr ("\tEND");

    if (mainf)
	outstr ("\t<3,,$$STRT>");	/* 3 wds in entry vector */

    outnl ();
    outnl ();
}

/* OUTDECL - Output assembler external & internal declarations at end
**	of compiling a module (translation unit).
*/
static int
outdecl (void)
{
    SYMBOL *s = symbol;		/* Start of global sym list */
    int num = 0;

    while ((s = s->Snext) != NULL)
	{
	switch (s->Sclass)
	    {
	    default:
		continue;

	    case SC_EXTDEF:		/* External linkage sym defined */
		continue;

	    case SC_EXTREF:		/* External linkage sym referenced */
	    case SC_XEXTREF:
	    /* Symbol not defined.  Ask for it if needed */
		if (s->Srefs == 0)	/* If never used, don't ask. */
		    {
		    if (!delete)		/* If keeping asm file, */
			outc (';');		/* output it as a comment. */
		    else
			continue;		/* Otherwise just ignore */
		    }
		outstr (asmdialect == ASM_GAS ? "\t.extern\t" : "\tEXTERN\t");
		break;
	    }
	outmiref (s);
	outnl ();
	}
    return num;
}

/* MAKPREFILE - determines whether a prefix assembler file is needed
**	and creates it if so.  Returns non-zero if one was created.
*/
int
makprefile (void)
{
    SYMBOL *sym;

    int nexfs = 0;

    /* Scan symbol table to see if there are any names
    ** we should export as library entry points.
    */

    for (sym = symbol; (sym = sym->Snext) != NULL;)
	if (sym->Sclass == SC_EXTDEF)	/* An external definition? */
	    break;				/* Yup */

    if (sym == NULL)
	return 0;	/* If none, just return */

    /* OK, now output entry statements for the exportable symbols */
    do
	{
	if (sym->Sclass == SC_EXTDEF)	/* For each external def */
	    {
	    if (asmdialect == ASM_GAS)
		{
		outstr ("\n\t.globl ");
		outmiref (sym);
		++nexfs;
		sym = sym->Snext;
		continue;
		}
	    if ((nexfs % 5) == 0)
		outstr ("\n\tENTRY ");
	    else
		outc (',');
	    outmiref (sym);
	    ++nexfs;
	    }
	sym = sym->Snext;
	}
    while (sym)
	;

    outnl ();

    return (nexfs);
}

/*
** KCC Assembler Header output
**
** This section contains the code required to generate the assembler header
** text that KCC puts at the beginning of all its assembler
** output files, just after any ENTRY or TITLE statements, and before
** any generated code.
**
** The header for both FAIL and MACRO is identical.
** However, MIDAS requires a somewhat different syntax.
*/

/* OUTASMH -  Output assembler header file.
**
** Copy Assembler Header File from its default or user-specified location.
** This should end up in the code segment, and supply macros
** %%CODE and %%DATA for switching between the two.
*/

static
char*	    asmhdr = NULL;

static
void
outasmh (void)
{
    FILE*	hdrf;
    register
    int		c;


    /* If user explicitly specified an assembler header file, use it. */

    if (asmhfile)
	{
	if ((hdrf = fopen (asmhfile, "r")) == NULL) /* open file */
	    errfopen ("preamble", asmhfile);	/* no luck, give up */
	else
	    {
	    while ((c = getc (hdrf)) != EOF)
		outc (c); /* copy file */

	    if (ferror (hdrf) || !feof (hdrf))
		jerr ("I/O error while reading preamble \"%s\"", asmhfile);

	    fclose (hdrf);
	    return;
	    }
	}

    /* No file specified, or couldn't open.  Use self-generated header. */
    outstr (asmhdr ? asmhdr : ahmacs ());	/* Make or use existing */
    }

/* Segmentation macros.  See codeseg () and dataseg () for more comments. */

static char segfai[] = "\
	DEFINE %%CODE <RELOC>\n\
	DEFINE %%DATA <RELOC>\n\n\
	TWOSEG	400000	\n\
	RELOC	0	\n\
	RELOC	400000	\n";

static char segkdb[] = "\
	DEFINE %%CODE <RELOC>\n\
	DEFINE %%DATA <RELOC>\n\n\
	EXTERN	KD$DLD	\n\
	TWOSEG	400000	\n\
	LOC	041	\n\
	PUSHJ	17,KD$DLD\n\
	RELOC	0	\n\
	RELOC	400000	\n";

static char segfnp[] = "\
	DEFINE %%CODE <RELOC>\n\
	DEFINE %%DATA <RELOC>\n\n\
	EXTERN	KD$FPL	\n\
	TWOSEG	400000	\n\
	LOC	041	\n\
	PUSHJ	17,KD$FPL\n\
	RELOC	0	\n\
	RELOC	400000	\n";

static char segsp[] = "\
	DEFINE %%CODE <RELOC>\n\
	DEFINE %%DATA <RELOC>\n\n\
	EXTERN	KD$SPL	\n\
	TWOSEG	400000	\n\
	LOC	041	\n\
	PUSHJ	17,KD$SPL\n\
	RELOC	0	\n\
	RELOC	400000	\n";

/* OCTCPY - Append an unsigned octal constant to an in-memory string.
** Values larger than one octal digit get a leading zero so DAS, whose
** expression grammar defaults to decimal, interprets the value as octal.
*/
static char *
octcpy (char *cp, unsigned INT n)
{
    char buf[(sizeof (unsigned INT) * 3) + 2];
    char *bp = buf + sizeof (buf);

    *--bp = '\0';
    do
	{
	*--bp = (char)('0' + (n & 07));
	n >>= 3;
	}
    while (n != 0);

    if ((buf + sizeof (buf) - bp) > 2)
	*--bp = '0';

    return estrcpy (cp, bp);
}

static char *
ahmacs (void)
{
    register char *cp;
    char *beg;
    size_t size;

    /* To construct the possibly large header string, we steal space
    ** by re-using the pcode buffer temporarily.  Check when done to ensure
    ** it wasn't overflowed!
    */
    beg = cp = (char *)codes;

    if (profbliss)			/* profiling switch for CSI */
	cp = estrcpy (cp, "\t.REQUIRE BLI:PROFIL.REL\n");
    /*
     * KAR-1/92, advent of library version of KCCDBG.REL caused this code to
     * be changed so that any value of debcsi requests KCCDBG.REL.
     * I removed the #if __MSDOS__ because the code needs to always execute.
     */
    else
	switch (abs (debcsi))		/* debugging switch for CSI */
	    {
	    case 0 :
		break;

	    case KCC_DBG_SDBG :
	    case KCC_DBG_FDBG :		    /* FW 2A (42) PPS4575 */
	    case KCC_DBG_SPRF :
	    case KCC_DBG_FPRF :
	    case KCC_DBG_NULL :
		cp = estrcpy (cp, "\t.REQUEST SYS:KCCDBG.REL\n");
		break;


	    default:
		int_error ("ahmacs: invalid debcsi %d", debcsi);
	    }

    /*
     * Make request for standard C library.
     * Eventually flush this request and rely on KCC linker invocation.
     */

    cp = estrcpy (fstrcpy (estrcpy (cp, "\t.REQUEST "), libpath, "c"), "\n");

    cp = estrcpy (cp, "\t$$CVER==:<");
    cp = octcpy (cp, (unsigned INT)cvercode);
    cp = estrcpy (cp, ",,");
    cp = octcpy (cp, (unsigned INT)cverlib);
    cp = estrcpy (cp, ">\n");

    /* Add machine-dependent macro definitions */

    if (profbliss)			    /* added 09/15/89 by MVS */
	cp = estrcpy (cp, "\tOPDEF\tPROF. [37B8]\n");

    if (debcsi)				    /* source debugger UUO's */
	{
	cp = estrcpy (cp, "\tOPDEF\tDEBUGP [37B8]\n");
	cp = estrcpy (cp, "\tOPDEF\tDEBUGS [37B8]\n");
	cp = estrcpy (cp, "\tOPDEF\tDEBUGE [37B8]\n");
	}

    /*
     * Try to purge MACRO pseudo-ops that we'll never need and that
     * may conflict with user-defined syms.
     */

    cp = estrcpy (cp, "\
PURGE IFE,IFN,IFG,IFGE,IFL,IFLE,IFDEF,IFNDEF,IFIDN,IFDIF\n\n");

    /* Code and data segmentation setup and %%DATA/%%CODE macro definition */

    switch (debcsi)
	{
	case 0 :			/* no debugger or profiler */
	default :
	    cp = estrcpy (cp, segfai);
	    break;


	case KCC_DBG_SDBG :		/* source-level debugger */
	case KCC_DBG_FDBG :		    /* FW 2A (42) PPS4575 */
	    cp = estrcpy (cp, segkdb);
	    break;


	case KCC_DBG_SPRF :		/* statement profiler */
	    cp = estrcpy (cp, segsp);
	    break;


	case KCC_DBG_FPRF :		/* function profiler */
	    cp = estrcpy (cp, segfnp);
	    break;
	}

    if (mlist)
	cp = estrcpy (cp, "\n\tSP=17");

    if ((!delete) || mlist)
	cp = estrcpy (cp, "\n;\t*** User code begins here ***\n");

    size = strlen (beg) + 1;		/* Find size of entire header */

    if (size > sizeof (codes))		/* Paranoia check */
	fatal ("Assembler header table overflow");

    if ((asmhdr = calloc (1, size)) != NULL)  /* Now try to relocate it */
	{
	memcpy (asmhdr, beg, size);	/* Copy complete NUL-terminated header. */
	return asmhdr;
	}

    return beg;			/* No luck, just re-generate each time */
}

/*
** Data segmentation.
**
** KCC emits code in two sections: code and data.  The header sets things up
** so that code and data are in the high and low segments, which typically
** correspond to the high and low halves of their section but which can be
** rearranged by the appropriate LINK directives.
**
** Code and data are separated in the assembly output by the %%CODE and %%DATA
** macros.  %%CODE will never be called from any context other than the data
** segment, and vice versa.  KCC expects the header to initially be in code.
**
** During code generation, codeseg () and dataseg () are used to switch
** between the two segments.
*/

/* CODESEG - Start using code (pure) segment
*/
int
codeseg (void)
{
    int oseg;
    if ((oseg = whichseg) < 0)	/* if in data */
	{
	outstr (asmdialect == ASM_GAS ? "\n\t.text\n" : "\n\t%%CODE\n");
#if SYS_CSI
	if (mlist)
	    oline += 2;
#endif
	whichseg = 1;			/* and remember in code */
	}
    return oseg;
}

/* DATASEG - Start using data (impure) segment
*/
int
dataseg (void)
{
    int oseg;
    if ((oseg = whichseg) != -1)	/* if not in initialized data */
	{
	outstr (asmdialect == ASM_GAS ? "\n\t.data\n" : "\n\t%%DATA\n");
#if SYS_CSI
	if (mlist)
	    oline += 2;
#endif
	whichseg = -1;			/* and remember in data */
	}
    return oseg;
}

/* BSSSEG - Start using zero-filled data segment.
*/
int
bssseg (void)
{
    int oseg;
    if (asmdialect != ASM_GAS)
	return dataseg ();
    if ((oseg = whichseg) != -2)
	{
	outstr ("\n\t.bss\n");
#if SYS_CSI
	if (mlist)
	    oline += 2;
#endif
	whichseg = -2;
	}
    return oseg;
}

/* PREVSEG - restore to previous segment
*/
int
prevseg (int seg)
{
    return (seg == -2) ? bssseg () : ((seg < 0) ? dataseg () : codeseg ());
}

/* REALCODE (p) - Generate real code for given pseudo-instruction.
**	This is the primary assembler output function.
*/
void
realcode (PCODE *p)
{
    int i, typ;

    typ = (p->Ptype &~ PTF_SKIPPED);	/* Get sanitized addressing type */

    switch (p->Pop & POF_OPCODE)
	{

	case P_NOP:			/* Ignore deleted pseudo-code */
	    return;

    /* Handle the various simulated ops, which do not correspond
    ** directly to a real PDP-10 instruction.
    */
	case P_PTRCNV:
	    simptrcnv (p);
	    return;	/* "Pointer Conversion"  */
	case P_SMOVE:
	    simsmove (p);
	    return;	/* "Structure Move" */
	case P_UFLTR:
	    simufltr (p);
	    return;	/* "Unsigned Float & Round" */
	case P_UIDIV:
	    simuidiv (p);
	    return;	/* "Unsigned Integer Divide" */
	case P_IDIV:
	    if (tgcpu == TGCPU_PDP6 || tgarch == TGARCH_BASE || tgarch == TGARCH_PDP10)
		{
		simsidiv (p);
		return;	/* Safe signed divide for PDP-6/common targets */
		}
	    break;
	case P_SUBBP:
	    simsubbp (p);
	    return;	/* "Subtract Byte Pointer" */
	case P_DFIX:
	    simdfix (p);
	    return;	/* "Double Fix" */
	case P_FLTR:
	    if (tgcpu == TGCPU_PDP6 || tgcpu == TGCPU_KA)
		{
		simfltr (p);
		return;	/* Simulated signed Float for PDP-6/KA10 */
		}
	    break;
	case P_FIX:
	    if (tgcpu == TGCPU_PDP6 || tgcpu == TGCPU_KA)
		{
		simfix (p);
		return;	/* Simulated single Fix for PDP-6/KA10 */
		}
	    break;
	case P_DSNGL:
	    simdsngl (p);
	    return;	/* "Double to Single" */

	case P_ADJSP:
	    if (!tgmachuse.adjsp) {
		simadjsp(p);
		return;
	    }
	    break;

	case P_ADJBP:
	    if (!tgmachuse.adjbp) {
		simadjbp(p);
		return;
	    }
	    break;

	case P_DMOVE:
	case P_DMOVEM:
	case P_DMOVN:
	    if (!tgmachuse.dmovx) {
		simdmovx(p);
		return;
	    }
	    break;

	case P_DFAD:
	case P_DFSB:
	case P_DFMP:
	case P_DFDV:
	    if (tgarch == TGARCH_PDP6) {
		PCODE q;
		int scr1, scr2;
		char *mn;

		switch (p->Pop & POF_OPCODE) {
		case P_DFAD: mn = "FADL"; break;
		case P_DFSB: mn = "FSBL"; break;
		case P_DFMP: mn = "FMPL"; break;
		case P_DFDV: mn = "FDVL"; break;
		default: mn = "FADL"; break;
		}

		if (p->Ptype == PTA_DCONST) {
		    scr1 = 013;
		    scr2 = 014;
		    if (p->Preg == scr1 || p->Preg + 1 == scr1
		      || p->Preg == scr2 || p->Preg + 1 == scr2) {
			scr1 = 010;
			scr2 = 011;
		    }
		    fprintf (out, "\tPUSH\t17,0%o\n", scr1);
		    fprintf (out, "\tPUSH\t17,0%o\n", scr2);
		    q = *p;
		    q.Pop = P_MOVE;
		    q.Preg = scr1;
		    q.Ptype = PTA_DCONST1 | (p->Ptype & PTF_SKIPPED);
		    outinstr (&q);
		    q.Preg = scr2;
		    q.Ptype = PTA_DCONST2 | (p->Ptype & PTF_SKIPPED);
		    outinstr (&q);
		    fprintf (out, "\t%s\t0%o,0%o\n", mn, p->Preg, scr1);
		    fprintf (out, "\tPOP\t17,0%o\n", scr2);
		    fprintf (out, "\tPOP\t17,0%o\n", scr1);
		    return;
		}

		if ((p->Ptype & PTF_ADRMODE) == PTA_REGIS) {
		    fprintf (out, "\t%s\t0%o,0%o\n", mn, p->Preg, p->Pr2);
		    return;
		}

		outc ('\t');
		outstr (mn);
		outc ('\t');
		outnum (p->Preg);
		outc (',');
		outaddress (p);
		outnl ();
		return;
	    }
	    if (tgcpu == TGCPU_PDP6 || tgcpu == TGCPU_KA) {
		simdfop(p);
		return;
	    }
	    break;

	case P_SETZ:
	case P_SETO:
	    if (!(p->Pop & POF_BOTH)
	      && (p->Ptype & PTF_ADRMODE) != PTA_ONEREG
	      && (p->Ptype & PTF_ADRMODE) != PTA_REGIS)
		{
		outop (p->Pop);
		outreg (p->Preg);
		outnl ();
		return;
		}
	    break;

    /* End of simulated ops; switch continued on next page! */
	    
    /* Switch continued from previous page!  Start of "real" ops. */

    /*
    ** If we are doing a test or comparison with a number, and the number
    ** is larger than 18 bits, we have to use a literal.
    ** We also have to perform a similar transformation if we want
    ** to compare the register with an immediate address.
    ** This entails changing the opcode from TRN to TDN, CAI to CAM, etc.
    ** Note this is NOT an optimization -- it is needed to avoid generating
    ** impossible instructions.
    */
	case P_TRN:
	    if (foldtrna (p))
		return;
	/* FALLTHROUGH */
	case P_TRC:
	case P_TRZ:
	case P_TRO:
	case P_CAI:
	    if (typ != PTA_RCONST || (p->Pvalue &~ 0777777L) == 0)
		break;
	    if ((p->Pvalue & 0777777L) == 0)
		{
		switch (p->Pop & POF_OPCODE)
		    {
		    case P_TRN:
			i = P_TRN ^ P_TLN;
			break;
		    case P_TRO:
			i = P_TRO ^ P_TLO;
			break;
		    case P_TRZ:
			i = P_TRZ ^ P_TLZ;
			break;
		    case P_TRC:
			i = P_TRC ^ P_TLC;
			break;
		    default:
			i = 0;
			break;
		    }
		if (i)
		    {
		    p->Pop ^= i;			/* Flip from RH to LH */
		    p->Pvalue = ((unsigned INT) p->Pvalue) >> 18;
		    break;
		    }
		}
	    if (! (p->Pvalue &~ 0777777L))
		break; /* still too big? */
	    p->Pop = directop (p->Pop);	/* Turn into memory version */
	    typ |= PTF_IMM;		/* Make PTV_IMMED so outinstr hacks bigness */
	    break;

    /* Optimization for non-test bitwise operations (IOR, AND, XOR).
    ** We try turning these into tests to avoid referencing memory
    ** for a literal constant.
    */
	case P_IOR:
	    if (!optobj || typ != PTV_IMMED || (p->Pvalue &~ 0777777L) == 0)
		break;			/* Can't or don't optimize */
	    if ((p->Pvalue & 0777777L) == 0) /* x,,0 */
		{
		p->Pop = P_TLO;
		typ = PTA_RCONST;
		p->Pvalue = ((unsigned INT) p->Pvalue) >> 18;
		}
	    else if (( (unsigned INT) p->Pvalue >> 18) == 0777777L) /* 777777,,x */
		{
		p->Pop = P_ORCM;
		p->Pvalue = (~p->Pvalue) & 0777777L;
		}
	    break;

	case P_XOR:
	    if (!optobj || typ != PTV_IMMED || (p->Pvalue &~ 0777777L) == 0)
		break;			/* Can't or don't optimize */
	    if ((p->Pvalue & 0777777L) == 0) /* x,,0 */
		{
		p->Pop = P_TLC;
		typ = PTA_RCONST;
		p->Pvalue = ((unsigned INT) p->Pvalue) >> 18;
		}
	    else if (( (unsigned INT) p->Pvalue >> 18) == 0777777L) /* 777777,,x */
		{
		p->Pop = P_EQV;
		p->Pvalue = (~p->Pvalue) & 0777777L;
		}
	    break;

	case P_AND:
	    if (!optobj || typ != PTV_IMMED || (p->Pvalue &~ 0777777L) == 0)
		break;			/* Can't or don't optimize */
	    if ((p->Pvalue & 0777777L) == 0777777L) /* x,,777777 */
		{
		p->Pop = P_TLZ;
		typ = PTA_RCONST;
		p->Pvalue = (unsigned INT) (~p->Pvalue) >> 18;
		}
	    else if (( (unsigned INT)p->Pvalue >> 18) == 0777777L) /* 777777,,x */
		{
		p->Pop = P_TRZ;
		typ = PTA_RCONST;
		p->Pvalue = (~p->Pvalue) & 0777777L;
		}
	    break;
	    
    /*
    ** Signed integer multiplication by a power of two is better done as an ASH
    ** (arithmetic shift - not the same as LSH).  Don't try this
    ** for division, as the result is incorrect for negative numbers!
    ** Note that unsigned multiplication will use P_MUL, not P_IMUL.
    ** Note the subtle test for power-of-two-ness which relies on
    ** twos-complement arithmetic.
    **
    ** Similarly, floating multiply by two becomes FSC (floating scale).
    ** Division is OK, unlike the integer case.
    ** This can only work for single-precision floats (not doubles).
    ** Otherwise we fall through to the other floating code.
    */
	case P_IMUL:
	    if (optobj && typ == PTV_IMMED && (p->Pvalue & (p->Pvalue - 1)) == 0)
		{
		p->Pvalue = binexp (p->Pvalue);	/* get # of bits to shift */
		p->Pop = P_ASH;			/* and opcode */
		typ = PTA_RCONST;			/* and code type */
		}
	    break;

	case P_FMPR:		/* If doing single-precision mult or div */
	case P_FDVR:
	    if (optobj
	      && typ == PTA_FCONST		/* If operand is a float constant */
	      && (i = fltpow2 (p->Pfloat)) != 0)	/* and is a power of 2 */
		{
		p->Pvalue = (p->Pop == P_FDVR) ? -i : i;
		p->Pop = P_FSC;
		typ = PTA_RCONST;
		break;
		}

    /*
    ** Optimize moves of constants to use immediate form if possible.
    ** A check for "float" constants, which can be
    ** MOVSI'd into the LH if their RH is zero, is made in outinstr ().
    ** "double" constants
    ** normally do not come here as they use DMOVx, but it is possible
    ** as part of a code sequence that zeros the second AC separately.
    */
	/* FALLTHROUGH */
	case P_MOVN:
	    if (!optobj || typ != PTV_IMMED || (p->Pvalue &~ 0777777L) == 0)
		break;
	    if ((p->Pvalue & ((INT)0777777777777)) == ((INT)0400000000000))
		break;			/* avoid bad MOVSI form for INT_MIN */
	    p->Pop = P_MOVE;		/* re-invert P_MOVN */
	    p->Pvalue = - p->Pvalue;	/* for fixup into P_MOVSI */
	/* FALLTHROUGH */
	case P_MOVE:
	    if (!optobj || typ != PTV_IMMED)
		break;
	    if (p->Pvalue && (p->Pvalue & 0777777L) == 0)
		{
		p->Pop = P_MOVS;	/* MOVEI of left half quantity */
		p->Pvalue = (((unsigned INT) p->Pvalue) & 0777777777777L) >> 18;	/* becomes MOVSI */
		}
	    break;
	default:
	    ;	/* do nothing */
	}
    /* End of moby switch on instruction opcode! */

    typ |= p->Ptype & PTF_SKIPPED;
    p->Ptype = typ;	/* Put back type in case it was changed. */
    outinstr (p);	/* Output the instruction! */
}

/* OUTINSTR (p) - Output instruction.
**
** We give a skipped-over op an extra space of indentation, to make it look
** more like human code (big deal) and to make debugging KCC easier.
*/
static void
outinstr (PCODE *p)
{

    int big, i, opr;

    outtab ();				/* Start instruction output w/ tab */
    if (p->Ptype & PTF_SKIPPED)		/* Indent skipped-over instr */
	outc (' ');

    switch (p->Ptype & PTF_ADRMODE)
	{

	case PTA_ONEREG:
	    if (p->Pop == P_MUUO)
		{
		outstr (p->p_im.mnemonic);
		if (p->p_im.p_chnl != -1)	/* KAR-6/91, chgd sentinal to -1 */
		    {
		    outtab ();	/* KAR-2/92, chgd outreg () to outnum () call */
		    outnum (p->p_im.p_chnl);
		    outc (',');
		    }
		else
		    outreg (p->Preg);
		}
	    else
		{
		outop (p->Pop);
		outreg (p->Preg);
		}

	    break;

	case PTA_REGIS:
	    outop (p->Pop);
	    outreg (p->Preg);

	    if (mlist && Register_Preserve (p->Pr2))
		outid (Reg_Id[p->Pr2 -
		      (r_maxnopreserve + 1)]->Sname); /* FW 2A (47) */
	    else if (mlist && p->Pr2 == R_SP)
		outid ("SP");
	    else
		outnum (p->Pr2);
	    break;

    /* PTA_MINDEXED operands require special hackery when the operand is
    ** being used in an immediate fashion, i.e. the address value itself
    ** is needed.  This can happen either if the operand is marked
    ** immediate (PTF_IMM set) or if the opcode is one that treats E as
    ** an immediate operand (only CAIx and TRx are checked).
    */
	case PTA_MINDEXED:
	    if (p->Pop == P_NULPTR)
		{
		outnpd (p);
		break;
		}

	    if (! (p->Ptype & PTF_IMM))
		{
	    /* Not using immediate address, only need to check for
	    ** instrs that are always immediate.
	    */
		if ((i = directop (p->Pop)) != 0) /* Convert CAIx to CAMx? */
		    {
		    p->Pop = i;		/* Ugh, convert to direct form */
		/* Fall past to do MOVEI */
		    }
		else
		    {
		/* Normal operand, easy! */

		    if (p->Pop == P_MUUO)
			{
			outstr (p->p_im.mnemonic);
		    /* KAR-6/91, changed sentinal value to -1 from 0 */
			if (p->p_im.p_chnl != -1)
			    outreg (p->p_im.p_chnl);
			else
			    outreg (p->Preg);
			}
		    else
			{
			outop (p->Pop);
			outreg (p->Preg);
			}

		    outaddress (p);		/* and address */
		    break;			/* Done! */
		    }
		}

	/* Instruction is using an immediate address.
	*/
	    if (p->Pop == P_MOVE || p->Pop == P_MOVEI)
		{
		outop (P_MOVEI);
		outreg (p->Preg);	/* Use given reg */
		outaddress (p);	/* and address */
		break;
		}
	/* Instruction needs an MOVEI preceding it.  Barf if we
	** generated a skip over this instr in the mistaken belief it
	** only occupied 1 word, which means that oneinstr () was either
	** wrong or wasn't called.  In particular, this should never happen
	** for the second of a cascaded skip.
	*/
	    if (p->Ptype & PTF_SKIPPED)
		int_error ("outinstr: MOVEI skipped");
	    outop (P_MOVEI);
	    outreg (R_SCRREG);	/* simulate by MOVEI into scratch */
	    outaddress (p);		/* of desired address */
	    outnl ();
	    outtab ();
	    outop (p->Pop);		/* followed by real instruction */
	    outreg (p->Preg);	/* into desired reg */
	    outnum (R_SCRREG);	/* from scratch reg */
	    break;

    /* PTA_PCONST requires some special checks for immediate pointer
    ** operands, which are distinguished by the lack of a word address
    ** symbol (Pptr is NULL).
    ** These should only be seen for TDZ, TDO, and IOR.
    ** TDZ uses them to mask out the P+S fields of a ptr (converting to wd ptr)
    ** IOR uses them to generate a byte ptr from a word ptr.
    ** TDOx is sometimes seen due to conversion from an IOR.
    ** Not clear if anything else can ever have a null word address.
    */
	case PTA_PCONST:
	    i = 0;
	    if (p->Pptr == NULL)
		switch (p->Pop & POF_OPCODE)
		    {
		    case P_TDZ:
			i = P_TDZ ^ P_TLZ;
			break;	/* Change TDZ to TLZ */
		    case P_TDO:
			i = P_TDO ^ P_TLO;
			break;	/* Change TDO to TLO */
#if 0
		    case P_IOR:
			i = P_IOR ^ P_TLO;
			break;	/* Change IOR to TLO */
#else
		    case P_IOR:
			i = P_IOR ^ P_HRLI;
			break;	/* Change IOR to HRLI */
#endif
		    default:
			;	/* do nothing */
		    }
	    if (i)			/* If can use immediate form, */
		{
		outop (p->Pop ^ i);		/* change opcode and output it! */
		outreg (p->Preg);
		outpti ((int) p->Pbsize, p->Poffset); /* Output immediate P+S sym */
		}
	    else
		{
		outop (p->Pop);		/* Normal full-word pointer constant */
		outreg (p->Preg);
		outc ('[');
		outptr (p->Pptr, (int) p->Pbsize, p->Poffset);
		outc (']');
		}
	    break;

	case PTA_BYTEPOINT:
	    outop (p->Pop);
	    outreg (p->Preg);
	    outc ('[');
	    if (asmdialect == ASM_GAS)
		{
		int bppos;
		int bpsiz;
		bppos = (p->Pbsize >> 12) & 077;
		bpsiz = (p->Pbsize >> 6) & 077;
		fprintf (out, "POINT %d,", bpsiz);
		outaddress (p);
		fprintf (out, ",%d", 35 - bppos);
		}
	    else
		{
		outnum (p->Pbsize);	/* Output P+S field */
		outstr (",,");
		outaddress (p);		/* Now add right half addr+offset (index) */
		}
	    outc (']');
	    break;

	case PTA_RCONST:
	    outop (opr = p->Pop);
	    big = 1;		/* Assume full-word operand */
	    if ((popflg[opr&POF_OPCODE]&PF_EIMM)	/* If op takes E as immed op */
	      || ! (p->Ptype & PTF_IMM))		/* or optype not marked "imm"*/
		big = 0;				/* then just output OP R,val */
	    else
		{
	    /* Op is not an E-immediate type, but is marked as "immediate".
	    ** So either we generate an opI form,
	    ** or make the operand into a memory literal as in OP R,[val].
	    */
		if ((popflg[opr&POF_OPCODE] & PF_OPI)	/* If op can be opI,*/
		  && (p->Pvalue &~ 0777777L) == 0)	/* and operand has zero LH, */
		    {
		    outc (asmdialect == ASM_GAS ? 'i' : 'I'); /* make immediate op */
		    big = 0;			/* and say small operand */
		    }
		}
	    outreg (p->Preg);		/* Output reg and comma */
	    if (big)
		{
		outc ('[');
		outnum (p->Pvalue);
		outc (']');
		}
	    else
		outnum (p->Pvalue);
	    break;


	case PTA_FCONST:
	    switch (p->Pop & POF_OPCODE)
		{
		case P_FADR:
		case P_FSBR:	/* Single-precision operations */
		case P_FMPR:
		case P_FDVR:	/* can be optimized sometimes */
		case P_MOVS:
		    big = bigfloat (p);	/* Set flag 0 if small */
		    if (!tgmachuse.fpimm && (p->Pop & POF_OPCODE) != P_MOVS)
			big = 1;	/* PDP-6 has no FP immediate mode */
		    break;
		case P_MOVE:		/* Check for quick setup of float */
		    if ((big = bigfloat (p)) == 0)
			p->Pop = P_MOVS;	/* MOVE R,[flt] into MOVSI R, (flt) */
		    break;
		default:			/* Always big otherwise */
		    big = 1;
		    break;
		}
	    if (asmdialect == ASM_GAS && !big)
		{
		/* DEC MACRO accepts forms such as MOVSI R,(floatword)
		 * and FADRI R,(floatword).  The DAIMON/GAS-like
		 * dialect deliberately treats parentheses as normal
		 * expression grouping, so those forms either assemble
		 * as the wrong halfword or become dialect-specific.
		 * Emit an explicit literal instead.
		 */
		if ((p->Pop & POF_OPCODE) == P_MOVS)
		    outop (P_MOVE);
		else
		    outop (p->Pop);
		outreg (p->Preg);
		outc ('[');
#ifdef  __COMPILER_KCC__		/* native mode */
		outnum (* (INT *) (&p->Pfloat));
#else					/* host may not be PDP-10! */
		outmpdbl ((INT *) &p->Pfloat, 1);
#endif
		outc (']');
		break;
		}

	    outop (p->Pop);
	    if (!big)
		outc (asmdialect == ASM_GAS ? 'i' : 'I');
	    outreg (p->Preg);
	    if (big)
		outc ('[');
	    else
		{
		outc (' ');
		outc ('(');
		}
#ifdef  __COMPILER_KCC__		/* native mode */
	    outnum (* (INT *) (&p->Pfloat));	/* Pass float as INT val */
#else					/* host may not be PDP-10! */
	    /* PTA_FCONST always holds a single-precision value in Pfloat.
	    ** Even when the operand must be emitted as a full literal, emit
	    ** one PDP-10 single-float word.  Using Pdouble here reads the
	    ** same union storage as a host double and creates a bogus two-word
	    ** literal, which breaks FIX/FADR/FMPR/etc. on host-built KCC.
	    */
	    outmpdbl ((INT *) &p->Pfloat, 1);
#endif
	    outc (big ? ']' : ')');

	/* Add a readable comment so humans can understand what the floating
	 * constant is.  However, because this is slow, only do it when
	 * we know the assembler output is going to stay around.
	 */
	    if (!delete)			/* If keeping asm file around, */
		fprintf (out,"\t; %.20g",p->Pfloat);	/* add comment for humans */
	    break;

	case PTA_DCONST1:		/* Double-word floating constant */
	case PTA_DCONST2:
	    i = ((p->Ptype)&PTF_ADRMODE) == PTA_DCONST1 ? 1 : 2;
	    outop (p->Pop);
	    outreg (p->Preg);
	    outc ('[');
	    if (tgmachuse.mapdbl)		/* If target mach fmt is different */
		{
#ifdef __COMPILER_KCC__
		outmpdbl ((INT *)&p->Pdouble, i);/* Output part of mapped double */
#else
		unsigned INT dword1, dword2;
		host_pdp10_double_words (p->Pdouble, &dword1, &dword2);
		outpnum (i == 1 ? dword1 : dword2);
#endif
		}
	    else
		outnum (i == 1 ? p->Pdouble1 : p->Pdouble2);
	    outc (']');

	    if (!delete)			/* If keeping asm file around, */
		fprintf (out,"\t; %.20g", p->Pdouble); /* add comment for humans */
	    break;

	case PTA_DCONST:		/* Double-word floating constant */
	    outop (p->Pop);
	    outreg (p->Preg);
	    if (p->Pdouble == 0)		/* Hack: if a double zero, */
		{
		outstr (crtsnam[CRT_ZERO]);	/* Use universal constant location. */
		crtref[CRT_ZERO]++;
		}
	    else if (tgmachuse.mapdbl)	/* If target mach fmt is different */
		{
		outc ('[');
#ifdef __COMPILER_KCC__
		outmpdbl ((INT *) &p->Pdouble, 3);	/* Output mapped double */
#else
		unsigned INT dword1, dword2;
		host_pdp10_double_words (p->Pdouble, &dword1, &dword2);
		outpnum (dword1);
		outnl ();
		outtab ();
		outtab ();
		outpnum (dword2);
#endif
		outc (']');
		}
	    else
		{
		outc ('[');
		outnum (p->Pdouble1);
		outnl ();
		outtab ();
		outtab ();
		outnum (p->Pdouble2);
		outc (']');
		}
	    if (!delete)			/* If keeping asm file around, */
		fprintf (out,"\t; %.20g",p->Pdouble); /* add comment for humans */
	    break;

	default:
	    int_error ("outinstr: bad adrmode %d", (p->Ptype & PTF_ADRMODE));
	}
    outnl ();			/* Done with instruction, finish off! */

    if (vrbfun)
	_word_cnt++;
}

/* ONEINSTR (p) - See if an instruction is safe to skip over.
**
** Takes an instruction as argument and returns true if that
** instruction will expand to one machine code word.
** This routine is in CCOUT because it must accurately reflect what
** CCOUT will actually do for a given pseudo-instruction.
*/
int
oneinstr (PCODE *p)
{
    switch (p->Pop & POF_OPCODE)
	{

    /* Simulated ops always expand out. */
	case P_PTRCNV:
	case P_SMOVE:
	case P_UIDIV:
	case P_UFLTR:
	case P_SUBBP:
	case P_DFIX:
	case P_DSNGL:
	    return 0;

    /* These instructions may or may not expand out, depending on the
    ** target machine.  If unsupported, they are actually macros defined
    ** by the assembler header.
    ** None of them have an "immediate" form, so PTV_IINDEXED can never happen.
    */
	case P_DMOVE:
	case P_DMOVN:
	case P_DMOVEM:
	    return tgmachuse.dmovx;	/* TRUE if machine has DMOVx */
	case P_ADJSP:
	    return tgmachuse.adjsp;	/* FALSE if simulated as ADD */
	case P_ADJBP:
	    return tgmachuse.adjbp;	/* TRUE if machine has ADJBP */
	case P_DFAD:
	case P_DFSB:
	case P_DFMP:
	case P_DFDV:
	    return (tgarch == TGARCH_PDP6 || (tgcpu != TGCPU_PDP6 && tgcpu != TGCPU_KA));
	case P_IDIV:
	    if (tgcpu == TGCPU_PDP6 || tgarch == TGARCH_BASE || tgarch == TGARCH_PDP10)
		return 0;
	    return 1;
	case P_FLTR:
	    return tgmachuse.fltr;	/* TRUE if machine has hardware FLTR */

	case P_TRN:
	case P_TRO:
	case P_TRC:
	case P_TRZ:	/* RH tests */
	case P_TLN:
	case P_TLO:
	case P_TLC:
	case P_TLZ:	/* LH tests */
	case P_CAI:
	case P_LSH:
	case P_ASH:
	    return (p->Ptype != PTA_MINDEXED);	/* PTV_IINDEXED for implicit immed op */

    /* All other (standard) instructions.
    */
	default:			/* OPI R,addr  expands to */
	    return (p->Ptype != PTV_IINDEXED);	/* MOVEI 16,addr ? OP r,16 */

    /* Special case of above, which always wins */
	case P_MOVE:
	case P_MOVEI:
	    return 1;			/* OK even for PTV_IINDEXED */
	}
}

static int
directop (int op)
{
    switch (op & POF_OPCODE)
	{
	case P_CAI:
	    return op ^ (P_CAI ^ P_CAM);
	case P_TRN:
	    return op ^ (P_TRN ^ P_TDN);
	case P_TRO:
	    return op ^ (P_TRO ^ P_TDO);
	case P_TRC:
	    return op ^ (P_TRC ^ P_TDC);
	case P_TRZ:
	    return op ^ (P_TRZ ^ P_TDZ);
	default:
	    return 0;
	}
}

/* SIMUFLTR - Output expansion of P_UFLTR unsigned float "instruction".
**	UFLTR R,M
**		R = register to leave single-precision float in.
**		M = any memory reference to single-word integer operand
**
** Note that for machines without the FLTR instruction, we don't even
** bother to get things right since the usual substitute (FSC R,233) won't
** work.  Need a fancy KA-10 float routine.
**
** Expands into:
**	if R == M	if R != M
**	SKIPGE 16,M	SKIPGE R,M	; Fetch or copy the float
**	 LSH R,-1	 LSH R,-1	; If sign bit set, shift down
**	FLTR R,R	FLTR R,R	; Float the positive integer!
**	CAIGE 16,	SKIPGE M	; If number was shifted,
**	 FSC R,1	 FSC R,1	; adjust exponent accordingly.
*/
static void
simufltr (PCODE *p)
{
    static int ufltr_label = 0;
    int lab = ++ufltr_label;
    int r = p->Preg;
    int mr = ((p->Ptype&PTF_ADRMODE) == PTA_REGIS) ? p->Pr2 : -1;

    if (mr != r)
	{
	p->Pop = P_MOVE;
	outinstr (p);
	}

    if (tgcpu == TGCPU_PDP6 || tgcpu == TGCPU_KA)
	{
	fprintf (out,"\tTLNN\t%o,0777000\n", r);
	fprintf (out,"\t JRST\t%%UFL%o\n", lab);
	fprintf (out,"\tLSH\t%o,-011\n", r);
	fprintf (out,"\tFSC\t%o,0233\n", r);
	fprintf (out,"\tFSC\t%o,011\n", r);
	fprintf (out,"\tJRST\t%%UFX%o\n", lab);
	fprintf (out,"%%UFL%o:\tFSC\t%o,0233\n", lab, r);
	fprintf (out,"%%UFX%o:\n", lab);
	return;
	}

    fprintf (out,"\tMOVE\t%o,0%o\n", R_SCRREG, r);
    fprintf (out,"\tJUMPGE\t%o,.+2\n", R_SCRREG);
    fprintf (out,"\t LSH\t%o,-1\n", r);
    fprintf (out,"\tFLTR\t%o,0%o\n", r, r);
    fprintf (out,"\tJUMPGE\t%o,.+2\n", R_SCRREG);
    fprintf (out,"\t FSC\t%o,1\n", r);
}

/* SIMFLTR - Output expansion of P_FLTR for PDP-6/KA10.
**	FLTR is not a PDP-6/KA10 hardware instruction.  For signed C
**	integers, use FSC with the standard 233 scale factor after making
**	the magnitude positive, then restore the sign in floating format.
*/
static void
simfltr (PCODE *p)
{
    int r = p->Preg;

    if ((p->Ptype&PTF_ADRMODE) != PTA_REGIS || r != p->Pr2)
	{
	p->Pop = P_MOVE;
	outinstr (p);
	}
    fprintf (out, "\tMOVE\t16,0%o\n", r);
    outstr ("\tJUMPGE\t16,.+2\n");
    fprintf (out, "\tMOVNS\t%o\n", r);
    fprintf (out, "\tFSC\t%o,0233\n", r);
    outstr ("\tCAIGE\t16,\n");
    fprintf (out, "\t MOVNS\t%o\n", r);
}

/* SIMFIX - Output expansion of P_FIX for PDP-6/KA10.
**	FIX is not a PDP-6/KA10 hardware instruction.  Convert a single
**	floating operand to a signed integer using the same exponent/fraction
**	shape as the double-fix expansion, but with a single ASH.
*/
static void
simfix (PCODE *p)
{
    int r = p->Preg;

    if ((p->Ptype&PTF_ADRMODE) != PTA_REGIS || r != p->Pr2)
	{
	p->Pop = P_MOVE;
	outinstr (p);
	}
    fprintf (out, "\tHLRE\t16,0%o\n\tASH\t16,-011\n", r);
    fprintf (out, "\tJUMPGE\t16,.+3\n");
    fprintf (out, "\tMOVNS\t%o\n", r);
    outstr ("\tTRC\t16,-01\n");
    fprintf (out, "\tTLZ\t%o,0777000\n", r);
    fprintf (out, "\tASH\t%o,-0233(16)\n", r);
    outstr ("\tCAIGE\t16,\n");
    fprintf (out, "\t MOVNS\t%o\n", r);
}

static void
outadjsp(INT n)
{
    if (tgmachuse.adjsp) {
	outstr ("\tADJSP\t17,");
	outnum (n);
	outnl ();
    }
    else if (n < 0) {
	/*
	** Do not emit ADD 17,[-N,,-N] here.  On real PDP-6/KA10
	** two's-complement ADD, carry out of the right half feeds the
	** left half, so ADD with a negative double-half literal only
	** accidentally behaved like stack decrement under old buggy SIMH
	** ADD semantics.  Use SUB with a positive [N,,N], matching the
	** GCC PDP-10 backend and the hardware pushdown-list intent.
	*/
	outstr ("\tSUB\t17,[");
	outnum (-n);
	outstr (",,");
	outnum (-n);
	outstr ("]\n");
    }
    else {
	outstr ("\tADD\t17,[");
	outnum (n);
	outstr (",,");
	outnum (n);
	outstr ("]\n");
    }
}

static void
simadjsp (PCODE *p)
{
    if (p->Preg != R_SP) {
	int_error ("simadjsp: bad form");
	outinstr (p);
	return;
    }
    outadjsp (p->Pvalue);
}

static void
outadjbp_body (void)
{
    static int adjbp_label = 0;
    int n = adjbp_label++;

    /* PDP-6 has no ADJBP instruction and its accumulators must not be
    ** treated as low-core memory operands.  Keep the pointer and negative
    ** adjustment scratch values in real stack memory while simulating it.
    **
    ** Entry:  AC1  = byte pointer
    **         AC16 = signed byte count
    ** Exit:   AC1  = adjusted byte pointer
    ** Clobbers AC13, AC14, AC15, AC16.
    */
    fprintf(out, "\tJUMPE\t1,%%ADJX%o\n", n);
    fprintf(out, "\tJUMPE\t16,%%ADJX%o\n", n);
    outstr("\tPUSH\t17,1\n");
    fprintf(out, "\tJUMPL\t16,%%ADJN%o\n", n);
    fprintf(out, "%%ADJP%o:\tIBP\t0(17)\n", n);
    fprintf(out, "\tSOJG\t16,%%ADJP%o\n", n);
    fprintf(out, "\tJRST\t%%ADJR%o\n", n);

    /* Negative adjustment.  Layout after ADJSP +2 is:
    **   -2(17) pointer, -1(17) byte size S, 0(17) scratch.
    */
    fprintf(out, "%%ADJN%o:\tADD\t17,[2,,2]\n", n);
    outstr("\tHLRZ\t13,-2(17)\n");
    outstr("\tLSH\t13,-6\n");
    outstr("\tANDI\t13,077\n");
    outstr("\tMOVEM\t13,-1(17)\n");
    fprintf(out, "%%ADNI%o:\tHLRZ\t14,-2(17)\n", n);
    outstr("\tLSH\t14,-014\n");
    outstr("\tANDI\t14,077\n");
    outstr("\tMOVEI\t15,044\n");
    outstr("\tSUB\t15,-1(17)\n");
    outstr("\tMOVEM\t15,0(17)\n");
    fprintf(out, "\tCAMN\t14,0(17)\n\t JRST\t%%ADJW%o\n", n);
    outstr("\tADD\t14,-1(17)\n");
    fprintf(out, "\tJRST\t%%ADJS%o\n", n);
    fprintf(out, "%%ADJW%o:\tHRRZ\t15,-2(17)\n", n);
    outstr("\tSUBI\t15,1\n");
    outstr("\tHRRM\t15,-2(17)\n");
    outstr("\tMOVEI\t15,044\n");
    fprintf(out, "%%ADJL%o:\tSUB\t15,-1(17)\n", n);
    fprintf(out, "\tJUMPGE\t15,%%ADJL%o\n", n);
    outstr("\tADD\t15,-1(17)\n");
    outstr("\tMOVEM\t15,0(17)\n");
    outstr("\tMOVE\t14,0(17)\n");
    fprintf(out, "%%ADJS%o:\tHLRZ\t15,-2(17)\n", n);
    outstr("\tANDI\t15,07777\n");
    outstr("\tLSH\t14,014\n");
    outstr("\tMOVEM\t14,0(17)\n");
    outstr("\tIOR\t15,0(17)\n");
    outstr("\tHRLM\t15,-2(17)\n");
    fprintf(out, "\tAOJL\t16,%%ADNI%o\n", n);
    outstr("\tSUB\t17,[2,,2]\n");
    fprintf(out, "%%ADJR%o:\tPOP\t17,1\n", n);
    fprintf(out, "%%ADJX%o:\n", n);
}

static void
simadjbp (PCODE *p)
{
    PCODE q;
    INT n;
    int npush, srcreg;

    if ((p->Ptype & ~PTF_SKIPPED) != PTV_IMMED) {
        srcreg = (((p->Ptype & PTF_ADRMODE) == PTA_REGIS) ? p->Pr2 : -1);
	npush = 0;
	if (p->Preg != 13) {
	    outstr("\tPUSH\t17,13\n");
	    ++npush;
	}
	if (p->Preg != 14) {
	    outstr("\tPUSH\t17,14\n");
	    ++npush;
	}
	if (p->Preg != 15) {
	    outstr("\tPUSH\t17,15\n");
	    ++npush;
	}
	if (p->Preg != 1) {
	    outstr("\tPUSH\t17,1\n");
	    ++npush;
	}
	q = *p;
	q.Pop = P_MOVE;
	q.Preg = (p->Preg == R_SCRREG) ? 13 : R_SCRREG;
        if (srcreg >= 0)
            ;                           /* Marshal the source AC below. */
	else if ((q.Ptype & PTF_ADRMODE) == PTA_BYTEPOINT
	  && q.Pindex == R_SP
	  && q.Pptr == NULL) {
	    fprintf(out, "\tMOVEI\t%o,", q.Preg);
	    outnum(q.Poffset - npush);
	    outstr("(17)\n");
	    fprintf(out, "\tHRLI\t%o,", q.Preg);
	    outnum(q.Pbsize);
	    outnl();
	} else {
	    if (q.Pindex == R_SP
	      && ((q.Ptype & PTF_ADRMODE) != PTA_BYTEPOINT)
	      && q.Pbsize == 0)
		q.Poffset -= npush;
	    outinstr(&q);
	}
	/* PDP-6 accumulators are not readable through low-core effective
	** addresses.  Marshal count and pointer through stack memory instead
	** of attempting register-to-register MOVE/SETM/EXCH transfers.
	** The helper receives AC1 = byte pointer and AC16 = signed byte count,
	** and returns the adjusted pointer in AC1.
	*/
	outadjsp(2);
	fprintf(out, "\tMOVEM\t%o,-1(17)\n", p->Preg);
        if (srcreg >= 0)
            fprintf(out, "\tMOVEM\t%o,0(17)\n", srcreg);
        else
            fprintf(out, "\tMOVEM\t%o,0(17)\n", q.Preg);
	outstr("\tMOVE\t1,0(17)\n");
	outstr("\tMOVE\t16,-1(17)\n");
	if (asmdialect == ASM_GAS) {
	    outstr("\tPUSHJ\t17,%ADJBPH\n");
	    simadjbp_ref = 1;
	} else {
	    fprintf(out, "\tPUSHJ\t17,%s\n", crtsnam[CRT_ADJBP]);
	    ++crtref[CRT_ADJBP];
	}
	if (p->Preg != 1) {
	    outstr("\tMOVEM\t1,0(17)\n");
	    fprintf(out, "\tMOVE\t%o,0(17)\n", p->Preg);
	}
	outadjsp(-2);
	if (p->Preg != 1)
	    outstr("\tPOP\t17,1\n");

	if (p->Preg != 15)
	    outstr("\tPOP\t17,15\n");
	if (p->Preg != 14)
	    outstr("\tPOP\t17,14\n");
	if (p->Preg != 13)
	    outstr("\tPOP\t17,13\n");
	return;
    }

    n = p->Pvalue;
    if (n < 0) {
	if (p->Preg != 13)
	    outstr("\tPUSH\t17,13\n");
	if (p->Preg != 14)
	    outstr("\tPUSH\t17,14\n");
	if (p->Preg != 15)
	    outstr("\tPUSH\t17,15\n");
	if (p->Preg != 1) {
	    outstr("\tPUSH\t17,1\n");
	    fprintf(out, "\tSETM\t1,%o\n", p->Preg);
	}
	fprintf(out, "\tMOVE\t%o,[", R_SCRREG);
	outnum(n);
	outstr("]\n");
	if (asmdialect == ASM_GAS) {
	    outstr("\tPUSHJ\t17,%ADJBPH\n");
	    simadjbp_ref = 1;
	} else {
	    fprintf(out, "\tPUSHJ\t17,%s\n", crtsnam[CRT_ADJBP]);
	    ++crtref[CRT_ADJBP];
	}
	if (p->Preg != 1) {
	    fprintf(out, "\tSETM\t%o,1\n", p->Preg);
	    outstr("\tPOP\t17,1\n");
	}
	if (p->Preg != 15)
	    outstr("\tPOP\t17,15\n");
	if (p->Preg != 14)
	    outstr("\tPOP\t17,14\n");
	if (p->Preg != 13)
	    outstr("\tPOP\t17,13\n");
	return;
    }

    while (n-- > 0)
	fprintf(out, "\tIBP\t%o\n", p->Preg);
}

static int
bumpaddr (PCODE *p)
{
    switch (p->Ptype & PTF_ADRMODE) {
    case PTA_REGIS:
	++p->Pr2;
	return 1;
    case PTA_MINDEXED:
    case PTA_BYTEPOINT:
	++p->Poffset;
	return 1;
    default:
	return 0;
    }
}

static void
outdmovn (int r)
{
    if (tgmachuse.dmovx)
	fprintf(out, "\tDMOVN\t%o,%o\n", r, r);
    else if (tgcpu == TGCPU_PDP6) {
	fprintf(out, "\tMOVNS\t%o\n", r);
	fprintf(out, "\tMOVNS\t%o\n", r + 1);
    } else
	fprintf(out, "\tDFN\t%o,%o\n", r, r + 1);
}

static void
simdmovx (PCODE *p)
{
    PCODE q;
    int op = p->Pop & POF_OPCODE;
    int r = p->Preg;

    q = *p;
    if (op == P_DMOVE || op == P_DMOVN) {
	q.Pop = P_MOVE;
	if ((q.Ptype & PTF_ADRMODE) == PTA_DCONST) {
	    q.Ptype = PTA_DCONST1 | (p->Ptype & PTF_SKIPPED);
	    outinstr(&q);
	    q.Preg = r + 1;
	    q.Ptype = PTA_DCONST2 | (p->Ptype & PTF_SKIPPED);
	    outinstr(&q);
	} else if ((q.Ptype & PTF_ADRMODE) == PTA_REGIS
		&& r == q.Pr2 + 1) {
	    /*
	    ** Simulate an upward-overlapping register-pair move in reverse
	    ** order.  MOVE R+1,S after MOVE R,S would otherwise destroy the
	    ** original S+1 value when R == S+1.
	    */
	    q.Preg = r + 1;
	    q.Pr2++;
	    outinstr(&q);
	    q.Preg = r;
	    q.Pr2--;
	    outinstr(&q);
	} else {
	    outinstr(&q);
	    q.Preg = r + 1;
	    if (!bumpaddr(&q)) {
		int_error ("simdmovx: bad DMOVE adrmode");
		return;
	    }
	    outinstr(&q);
	}
	if (op == P_DMOVN)
	    outdmovn(r);
	return;
    }

    if (op == P_DMOVEM) {
	q.Pop = P_MOVEM;
	outinstr(&q);
	q.Preg = r + 1;
	if (!bumpaddr(&q)) {
	    int_error ("simdmovx: bad DMOVEM adrmode");
	    return;
	}
	outinstr(&q);
	return;
    }

    int_error ("simdmovx: bad op");
}

static void
simdfop (PCODE *p)
{
    PCODE q, q2;
    int i, op, helper, save[5];

    op = p->Pop & POF_OPCODE;
    switch (op) {
    case P_DFAD:
	helper = CRT_KDFAD;
	break;
    case P_DFSB:
	helper = CRT_KDFSB;
	break;
    case P_DFMP:
	helper = CRT_KDFMP;
	break;
    case P_DFDV:
	helper = CRT_KDFDV;
	break;
    default:
	int_error("simdfop: bad op");
	return;
    }

    for (i = 0; i < 5; ++i) {
	save[i] = 010 + i;
	fprintf(out, "\tPUSH\t17,0%o\n", save[i]);
    }

    q = *p;
    q.Pop = P_MOVE;
    if ((q.Ptype & PTF_ADRMODE) == PTA_MINDEXED && q.Pindex == R_SP)
	q.Poffset -= 5;

    if (q.Ptype == PTA_DCONST) {
	q.Preg = 013;
	q.Ptype = PTA_DCONST1 | (p->Ptype & PTF_SKIPPED);
	outinstr(&q);
	q.Preg = 014;
	q.Ptype = PTA_DCONST2 | (p->Ptype & PTF_SKIPPED);
	outinstr(&q);
    } else {
	q2 = q;
	if (!bumpaddr(&q2)) {
	    int_error("simdfop: bad adrmode");
	    return;
	}
	if (((q.Ptype & PTF_ADRMODE) == PTA_MINDEXED && q.Pindex == 013)
	  || (q.Ptype == PTA_REGIS && q.Pr2 == 013)) {
	    q2.Preg = 014;
	    outinstr(&q2);
	    q.Preg = 013;
	    outinstr(&q);
	} else {
	    q.Preg = 013;
	    outinstr(&q);
	    q2.Preg = 014;
	    outinstr(&q2);
	}
    }

    fprintf(out, "\tMOVE\t10,0%o\n", p->Preg);
    fprintf(out, "\tMOVE\t11,0%o\n", p->Preg + 1);
    fprintf(out, "\tPUSHJ\t17,%s\n", crtsnam[helper]);
    ++crtref[helper];
    fprintf(out, "\tMOVE\t0%o,010\n", p->Preg);
    fprintf(out, "\tMOVE\t0%o,011\n", p->Preg + 1);

    for (i = 0; i < 5; ++i)
	if (save[i] != p->Preg && save[i] != p->Preg + 1)
	    fprintf(out, "\tMOVE\t%o,%d(17)\n", save[i], i - 4);
    outadjsp(-5);
}

/* SIMDFIX - Output expansion of P_DFIX double-to-integer conversion "instr"
**	P_DFIX reg,M
**		reg = register pair, integer will be left in 1st reg.
**		M = any memory ref to double-word float operand.
**
** Note that reg+1 is clobbered!
** Expands into:
**	DMOVE	R,M
**	HLRE	16,R		;This mattered when shifts were bit-at-a-time
**	ASH	16,-11		;Get just exponent (9 bits)
**	JUMPGE	16,.+3		;Positive?
**	  DMOVN	R,R		;No, negate, orig sign still in 1B0[A]
**				; For KA-10 format this is DFN R,R+1.
**	  TRC	16,777777	;Watch for diff between twos and ones comp
**	TLZ	R,777000	;Bash exponent and sign ... now positive
**				; For KA-10 format, LSH R+1,10 goes here.
**	ASHC	R,-233(16)	;Make an integer (may overflow)
**	CAIGE	16,		;Original negative?  Check its sign.
**	 MOVN	R,R		;Yup, negate result.
*/
static void
simdfix (PCODE *p)
{
    int r = p->Preg;

    /* Check for DFIX R,R and skip the DMOVE if that's what we have */
    if ((p->Ptype&PTF_ADRMODE) != PTA_REGIS || r != p->Pr2)
	{
	p->Pop = P_DMOVE;		/* Make DMOVE R,M to get double */
	outinstr (p);
	}
    fprintf (out, "\tHLRE\t16,0%o\n\tASH\t16,-011\n", r);
    if (tgcpu == TGCPU_PDP6) {
	fprintf (out, "\tJUMPGE\t16,.+4\n");
	fprintf (out, "\tMOVNS\t%o\n", r);
	fprintf (out, "\tMOVNS\t%o\n", r + 1);
    } else {
	fprintf (out, "\tJUMPGE\t16,.+3\n");
	outdmovn(r);
    }
    outstr ("\tTRC\t16,-01\n");

    fprintf (out, "\tTLZ\t%o,0777000\n", r);
    fprintf (out, "\tASHC\t%o,-0233(16)\n", r);
    outstr ("\tCAIGE\t16,\n");		/* Check sign bit of original # */
    fprintf (out, "\t MOVNS\t%o\n", r);	/* Negate result */
}

/* SIMDSNGL - Output expansion of P_DSNGL double-to-float conversion "instr"
**	P_DSNGL reg,M
**		reg = register pair, float will be left in 1st reg.
**		M = any memory ref to double-word float operand.
**
** Note that reg+1 is clobbered!
** Expands into:
**	DMOVE R,M		; Get double value
**	SKIPGE 16,R		; Check sign, save indicator
**	 DMOVN R,R		; Negative, make positive temporarily.
**	TLNE R+1,200000		; If low word >= .5
**	 TRON R,1		; then must round high wd, try fast hack
**	  JRST .+4		; Won!
**	   MOVE R+1,R		; Ugh, have to do it hard way.  Copy high wd
**	   AND R+1,[777000000001]	; Zap all but exp and low-order bit
**	   FADR R,R+1		; Add low bit to effect rounding
**	CAIGE 16,		; Now if original was negative,
**	 MOVN R,R		; make it negative again.
*/
static void
simdsngl (PCODE *p)
{
    int r = p->Preg;
    /* Check for DSNGL R,R and skip the DMOVE if that's what we have */
    if ((p->Ptype&PTF_ADRMODE) != PTA_REGIS || r != p->Pr2)
	{
	p->Pop = P_DMOVE;		/* Make DMOVE R,M to get double */
	outinstr (p);
	}
    if (tgcpu == TGCPU_PDP6) {
	fprintf (out, "\tSKIPL\t16,0%o\n", r);
	fprintf (out, "\t JRST\t.+3\n");
	fprintf (out, "\tMOVNS\t%o\n", r);
	fprintf (out, "\tMOVNS\t%o\n", r + 1);
    } else {
	fprintf (out, "\tSKIPGE\t16,0%o\n", r);
	outc (' ');
	outdmovn(r);
    }
    fprintf (out, "\tTLNE\t%o,0200000\n", r+1);
    fprintf (out, "\t TRON\t%o,01\n\t  JRST\t.+04\n", r);
    fprintf (out, "\tMOVE\t%o,0%o\n\tAND\t%o,[0777000,,01]\n\tFADR\t%o,0%o\n",
		r+1, r,
		r+1,
		r, r+1);

    outstr ("\tCAIGE\t16,\n");		/* Check sign bit of original # */
    fprintf (out, "\t MOVNS\t%o\n", r);	/* Negate result */
}

/* SIMSUBBP - Output expansion of P_SUBBP byte-pointer subtraction "instr".
**	P_SUBBP reg,M		[plus Pbsize set to bytesize if known]
**		reg = register pair, 1st reg contains minuend BP
**		M = any memory ref to subtrahend BP
**		Pbsize = bytesize of pointers (> 0 if known)
**			This is currently only used if M operand is P_CONST.
**	Leaves resulting # in 2nd reg!
**
**	This makes use of 2 special symbols from CPU.C and
** some tables in CRT.C, and expands into:
**	Known size		Unknown size
**				LDB 16,[$$BPSZ,,R]	; get PS from R
**	SUB R,M			SUB R,M
**	MULI R,$$BMPn		MUL R,$BPMUL (16)
**	ASH R+1,$$BSHF		ASH R+1,$$BSHF
**				ADD R,$BPADT (16)
**	ADD R+1,$BPADn (A)	ADD R+1, (A)
*/
static void
simsubbp (PCODE *p)
{
    int siz, typ, bytes_per_word, first_e, i;
    int oldop, oldreg;
    const char *idxsep;

    idxsep = (asmdialect == ASM_GAS) ? "" : " ";
    typ = (p->Ptype&PTF_ADRMODE);

    /* P_SUBBP carries the byte size in Pbsize when the generator knows the
    ** pointed-to element type.  Older code only trusted Pbsize for literal
    ** byte-pointer operands, which left register-vs-register pointer
    ** subtraction on the historical CRT table fallback.
    */
    siz = (int) p->Pbsize;

    /* Simple check to verify addressing mode is OK */
    switch (typ)
	{
	case PTA_PCONST:
	case PTA_REGIS:
	case PTA_MINDEXED:
	    break;
	default:
	    int_error ("simsubbp: bad adrmode: %ld", (INT) typ);
	}

    if (siz > 0 && siz <= TGSIZ_WORD)
	{
	/*
	** Keep standalone-hosted KCC output self-contained.  The historical
	** expansion used CRT constants/tables ($$BMPn, $$BSHF, $BPADn) that are
	** not available in the bare DAIMON test runtime.  For a known byte size,
	** compute:
	**     word_delta * bytes_per_word + left_byte_index - right_byte_index
	** directly from the byte-pointer word address and encoded P field.
	**
	** The encoded field extracted by HLRZ/LSH/ANDI is E = 35 - P.  For the
	** first byte in a word E is 36 - size, then it decreases by size for
	** each following byte.
	**
	** Uses AC15 and AC16 as CCOUT scratch registers, as other CCOUT
	** pseudo-op expansions already do with R_SCRREG/AC16.  P_SUBBP destroys
	** the first register of its input pair and leaves the result in R+1.
	*/
	bytes_per_word = 36 / siz;
	first_e = 36 - siz;

	oldop = p->Pop;
	oldreg = p->Preg;
	p->Pop = P_MOVE;
	p->Preg = 016;
	outinstr (p);			/* MOVE 16,M ; right byte pointer */
	p->Pop = oldop;
	p->Preg = oldreg;

	/* Current DAS does not alias low-core effective addresses to PDP-6
	** accumulators.  Keep both byte pointers and the call-preserved AC15
	** in real stack memory while this expansion decomposes them.  This
	** also makes every numeric constant below explicitly octal for DAS.
	*/
	outadjsp (4);
	fprintf (out, "\tMOVEM\t%o,-03(17)\n", p->Preg);
	outstr ("\tMOVEM\t16,-02(17)\n");
	outstr ("\tMOVEM\t15,-01(17)\n");

	fprintf (out, "\tMOVE\t%o,-03(17)\n", p->Preg + 1);
	fprintf (out, "\tANDI\t%o,0777777\n", p->Preg + 1);
	fprintf (out, "\tMOVE\t%o,-02(17)\n", p->Preg);
	fprintf (out, "\tANDI\t%o,0777777\n", p->Preg);
	fprintf (out, "\tMOVEM\t%o,00(17)\n", p->Preg);
	fprintf (out, "\tSUB\t%o,00(17)\n", p->Preg + 1);
	if (bytes_per_word != 1)
	    fprintf (out, "\tIMULI\t%o,0%o\n", p->Preg + 1, bytes_per_word);

	outstr ("\tHLRZ\t15,-03(17)\n");
	outstr ("\tLSH\t15,-014\n");
	outstr ("\tANDI\t15,077\n");
	fprintf (out, "\tMOVEI\t%o,00\n", p->Preg);
	for (i = 1; i < bytes_per_word; ++i)
	    {
	    fprintf (out, "\tCAIN\t15,0%o\n", first_e - (siz * i));
	    fprintf (out, "\t MOVEI\t%o,0%o\n", p->Preg, i);
	    }
	fprintf (out, "\tMOVEM\t%o,00(17)\n", p->Preg);
	fprintf (out, "\tADD\t%o,00(17)\n", p->Preg + 1);

	outstr ("\tHLRZ\t15,-02(17)\n");
	outstr ("\tLSH\t15,-014\n");
	outstr ("\tANDI\t15,077\n");
	fprintf (out, "\tMOVEI\t%o,00\n", p->Preg);
	for (i = 1; i < bytes_per_word; ++i)
	    {
	    fprintf (out, "\tCAIN\t15,0%o\n", first_e - (siz * i));
	    fprintf (out, "\t MOVEI\t%o,0%o\n", p->Preg, i);
	    }
	fprintf (out, "\tMOVEM\t%o,00(17)\n", p->Preg);
	fprintf (out, "\tSUB\t%o,00(17)\n", p->Preg + 1);
	outstr ("\tMOVE\t15,-01(17)\n");
	outadjsp (-4);
	return;
	}

    if (siz)
	{
	int_error ("simsubbp: bad Pbsize: %ld", (INT) siz);
	siz = 0;
	}

    /* Unknown byte-size fallback.  This still needs the historical CRT
    ** tables and is retained for non-constant/foreign byte pointers.
    */
    fprintf (out, "\tLDB\t16,[%s,,%o]\n", crtsnam[CRT_BPSZ], p->Preg);
    crtref[CRT_BPSZ]++;
    p->Pop = P_SUB;			/* Make SUB R,M */
    outinstr (p);
    fprintf (out, "\tMUL\t%o,%s%s(16)\n", p->Preg, crtsnam[CRT_BPMUL], idxsep);
    crtref[CRT_BPMUL]++;
    fprintf (out, "\tASH\t%o,-%s\n", p->Preg+1, crtsnam[CRT_BSHF]);
    crtref[CRT_BSHF]++;
    fprintf (out, "\tADD\t%o,%s%s(16)\n", p->Preg, crtsnam[CRT_BPADT], idxsep);
    crtref[CRT_BPADT]++;
    fprintf (out, "\tADD\t%o,%s(%o)\n", p->Preg+1, idxsep, p->Preg);
}

/*
** NOTE:
** Byte pointer comparison used to be simulated too but now is done
** with explicit instructions by CCGEN2.
*/
/* NOTE:
** Some PDP-10 architecture machines have no ADJBP instruction.
** We used to expand a sequence for P_ADJBP similar to that for
** P_SUBBP, but now we always just output P_ADJBP
** as ADJBP R,X and it is up to the C-HDR preamble file to redefine
** ADJBP as a macro (in a form similar to the above) if the machine
** does not support ADJBP.  This allows machine-language library routines
** to use ADJBP in their code.
*/

/* SIMPTRCNV - Output expansion of P_PTRCNV pointer conversion "instruction".
**	P_PTRCNV reg,offset	[plus Pbsize set to desired bytesize]
**		reg = register containing pointer to convert
**		offset = bytesize of old pointer
**		Pbsize = desired bytesize of new pointer
**
**	Currently this only supports 9<->18 bit conversions, and the
** code generator shouldn't give us anything else.
** The interaction between the special instruction symbols defined in the
** library $$$CPU module and the code here has been carefully arranged.
** In particular the instructions must take up the same amount of space
** whether being loaded for zero-section or multi-section operation, and
** they must NOT change the value of a NULL (zero) pointer.
**
** The conversions to and from word pointers are done by the code
** generator (with TDZ/TLZ or SKIPE+IOR/TLO) because it may be possible
** for the peephole optimizer to do something with those instructions.
*/
static void
simptrcnv (PCODE *p)
{
    int *ip;
#ifdef	MULTI_SECTION /* FW 2A (51) */
    static INT cvH_9[] = {CRT_PH90, CRT_PH91, 0};
    static INT cv9_H[] = {CRT_P9H0, CRT_P9H1, CRT_P9H2, 0};
#endif


    /* A negative source size is an internal packed-member conversion.
    ** The pointer is being dereferenced, so NULL preservation is not needed;
    ** preserve its exact starting bit while replacing the S field.
    */
    if (p->Poffset < 0)
        {
        int oldsiz = -(int)p->Poffset;
        int newsiz = (int)p->Pbsize;
        int delta = oldsiz - newsiz;

        if (oldsiz <= 0 || oldsiz > TGSIZ_WORD
          || newsiz <= 0 || newsiz > TGSIZ_WORD)
            {
            int_error ("simptrcnv: bad packed bsize: %ld", (INT)p->Pbsize);
            return;
            }
        fprintf(out, "\tTLZ\t%o,0007700\n", p->Preg);
        if (delta > 0)
            fprintf(out, "\tADD\t%o,[0%02o0000000000]\n",
                    p->Preg, delta & 077);
        else if (delta < 0)
            fprintf(out, "\tSUB\t%o,[0%02o0000000000]\n",
                    p->Preg, (-delta) & 077);
        fprintf(out, "\tTLO\t%o,0%06o\n", p->Preg,
                (newsiz & 077) << 6);
        return;
        }

    /* Switch depending on original size plus desired size */
    switch ((int) (TGSIZ_WORD* p->Poffset + p->Pbsize))
	{
	case TGSIZ_WORD* 9 + 18:	/* 9-bit to 18-bit */
		fprintf (out, "\tTLZE\t%o,0117700\n", p->Preg);
		fprintf (out, "\t TLO\t%o,0002200\n", p->Preg);
		return;

	case TGSIZ_WORD* 18 + 9:	/* 18-bit to 9-bit */
		fprintf (out, "\tTLZE\t%o,0007700\n", p->Preg);
		fprintf (out, "\t TLO\t%o,0111100\n", p->Preg);
		return;

	default:
	    int_error ("simptrcnv: bad bsize: %ld", (INT) p->Pbsize);
	    return;
	}

    /* Wrap up with special instructions the value of which depends
    ** on whether the code is being loaded for a zero-section or
    ** multi-section program.  The specified register is added into
    ** the symbols in such a way that it fits into the AC field.
    */
    for (; *ip; ++ip)
	{
	fprintf (out,"\t%s+<%" INT_OFMT ">\n", crtsnam[*ip], (unsigned INT)p->Preg<<23);
	++crtref[*ip];
	}
}

/* SIMSMOVE - Output expansion of P_SMOVE structure copy "instruction".
**	P_SMOVE reg,addr+offset (idx)  [plus Pbsize set to # words]
**		reg = register containing destination word address
**		addr+offset (idx) = source address
**		Pbsize = # words to copy
*/
static void
simsmove (PCODE *p)
{
    INT size;
    const char *idxsep;

    idxsep = (asmdialect == ASM_GAS) ? "" : " ";

    if ((p->Ptype&PTF_ADRMODE) != PTA_MINDEXED)
	{
	int_error ("simsmove: bad adrmode:\t%o", p->Ptype);
	return;
	}
    switch ((int) (size = p->Pbsize))
	{
	case 1:
	case 2:
	    fprintf (out,"\tMOVEI\t16,-1%s(%o)\n", idxsep, p->Preg); /* dest <addr-1> */
	    while (--size >= 0)
		{
		outstr ("\tPUSH\t16,");
		outaddress (p);
		outnl ();
		++ (p->Poffset);
		}
	    return;

	default:

	    if (size <= 0)
		{
		int_error ("simsmove: bad size %ld", (INT) size);
		return;
		}

	    fprintf (out,"\tMOVEI\t16,%s(%o)\n\tHRLI\t16,", idxsep, p->Preg);
	    outaddress (p);		/* for HRLI\t16,<source-addr> */
	    fprintf (out,"\n\tBLT\t16,%" INT_OFMT "%s(%o)\n", (unsigned INT)(size-1), idxsep, p->Preg);
	    return;
	}
    }

/* SIMSIDIV - Output portable signed division expansion for P_IDIV.
**
** PDP-6 and KA10/KI10 do not agree on the IDIV edge where the dividend is
** the signed 36-bit minimum value.  PDP-6 reports No Divide for that input,
** while later processors accept more cases.  For common/base code, and for
** exact PDP-6 C code, guard only that edge and otherwise retain the normal
** hardware IDIV fast path.
**
** On the edge path, divide abs(INT_MIN) as an unsigned value using the
** existing P_UIDIV expansion, then restore the C/PDP-10 signed quotient and
** remainder signs.  INT_MIN / -1 is C overflow/undefined, but this path still
** returns INT_MIN with zero remainder rather than depending on machine-specific
** No Divide behavior.
*/
static void
simsidiv (PCODE *p)
{
    PCODE q;
    int rq = p->Preg;
    int rr = rq + 1;
    static int sidiv_label = 0;
    int lab = sidiv_label++;
    int sav = 015;
    INT divisor;

    if ((p->Ptype & PTF_ADRMODE) == PTA_RCONST)
        {
        divisor = p->Pvalue;
        if (divisor > 1 && divisor <= 01000000L
          && (divisor & (divisor - 1)) == 0)
            {
            fprintf(out, "\tMOVE\t%o,%o\n", rr, rq);
            fprintf(out, "\tSKIPGE\t%o\n", rq);
            fprintf(out, "\t ADDI\t%o,%" INT_OFMT "\n",
                    rq, (unsigned INT)(divisor - 1));
            fprintf(out, "\tASH\t%o,-%" INT_OFMT "\n",
                    rq, (unsigned INT)binexp(divisor));
            fprintf(out, "\tMOVE\t%o,0%o\n", R_SCRREG, rq);
            fprintf(out, "\tLSH\t%o,%" INT_OFMT "\n",
                    R_SCRREG, (unsigned INT)binexp(divisor));
            fprintf(out, "\tSUB\t%o,0%o\n", rr, R_SCRREG);
            return;
            }
        }

    if (rq == sav || rr == sav)
	sav = 014;

    q = *p;
    q.Pop = P_MOVE;
    q.Preg = R_SCRREG;
    outinstr (&q);			/* scratch = divisor */

    fprintf (out, "\tCAMN\t%o,[0400000000000]\n", rq);
    fprintf (out, "\t JRST\t%%SIDN%o\n", lab);
    if ((p->Ptype & PTF_ADRMODE) == PTA_RCONST) {
        fprintf (out, "\tIDIVI\t%o,", rq);
        outnum (p->Pvalue);
        outnl ();
    } else
        fprintf (out, "\tIDIV\t%o,0%o\n", rq, R_SCRREG);
    fprintf (out, "\tJRST\t%%SIDD%o\n", lab);

    fprintf (out, "%%SIDN%o:\tPUSH\t17,0%o\n", lab, sav);
    fprintf (out, "\tPUSH\t17,0%o\n", R_SCRREG);
    fprintf (out, "\tMOVM\t%o,0%o\n", sav, R_SCRREG);
    fprintf (out, "%%SIDP%o:\tMOVE\t%o,[0400000000000]\n", lab, rq);
    fprintf (out, "\tMOVE\t%o,0%o\n", R_SCRREG, sav);

    q = *p;
    q.Pop = P_UIDIV;
    q.Preg = rq;
    q.Ptype = PTA_REGIS;
    q.Pr2 = R_SCRREG;
    simuidiv (&q);

    fprintf (out, "\tSKIPL\t0(17)\n");
    fprintf (out, "\t MOVN\t%o,%o\n", rq, rq);
    fprintf (out, "\tSKIPE\t%o\n", rr);
    fprintf (out, "\t MOVN\t%o,%o\n", rr, rr);
    outadjsp (-1);
    fprintf (out, "\tPOP\t17,0%o\n", sav);
    fprintf (out, "%%SIDD%o:\n", lab);
}

/* SIMUIDIV - Output expansion of P_UIDIV unsigned division "instruction".
**	P_UIDIV reg,<addr>
**		reg = register containing dividend (two-word register)
**		addr = divisor
** This sequence was derived from one suggested by Peter Samson
** at Systems Concepts.
*/
static void
simuidiv (PCODE *p)
{
    register int rq = p->Preg;		/* RQ - Quotient register */
    register int rr = rq+1;		/* RR - Remainder register, RQ+1 */
    register INT divisor;
    static int uidiv_label = 0;
    int lab = uidiv_label++;

    /* First, try to use optimized sequences if the divisor is a constant. */
    if ((p->Ptype&PTF_ADRMODE) == PTA_RCONST)
	{
	divisor = p->Pvalue;
	if (divisor == 1 || divisor == 0)	/* Div by 1 (or 0??) */
	    {
	    fprintf (out,"\tSETZ\t%o,\n", rr);	/* just clears rem */
	    return;
	    }
	if ((divisor & (divisor-1)) == 0)	/* If divisor is power of 2 */
	    {
	    fprintf (out,"\tLSHC\t%o,-%" INT_OFMT "\n", rq, (unsigned INT)binexp(divisor));
	    fprintf (out,"\tLSH\t%o,-%" INT_OFMT "\n", rr, (unsigned INT)(TGSIZ_WORD - binexp(divisor)));
	    return;
	    }
	if (divisor > 0)		/* High bit not set? */
	    {
	    fprintf (out, "\tSKIPL\t%o,%o\n", rr, rq);
	    fprintf (out, "\t TDZA\t%o,%o\n", rq, rq);
	    fprintf (out, "\t  MOVEI\t%o,1\n", rq);
	    if (! (p->Pvalue & ~0777777L))	/* Constant fits in RH? */
		fprintf (out, "\tDIVI\t%o,%" INT_OFMT "\n", rq, (unsigned INT)divisor);
	    else
		fprintf (out, "\tDIV\t%o,[%" INT_OFMT "]\n", rq, (unsigned INT)divisor);
	    return;
	    }
	/* Constant divisor has high bit set, ugh!
	** This case is pretty unlikely so don't bother optimizing it.
	** Just drop through to general-purpose division algorithm.
	*/
	p->Ptype |= PTF_IMM;		/* Ensure outinstr checks bigness */
	}

    /* Sigh, cannot avoid doing full-fledged hairy unsigned division.
    ** Output entire algorithm -- see following comments for more explanation.
    ** Note that divisor is fetched into reg 16 immediately to avoid any
    ** addressing conflicts with regs RQ or RQ+1.  We temporarily set
    ** Preg to this scratch reg so that we can take advantage of outinstr ()
    ** and have it use the right register.
    **
    ** Older versions of this routine used hand-counted .+N jumps.  That is
    ** fragile when this sequence is changed: the DAIMON KCC runtime harness
    ** caught the positive-dividend path jumping out of udivsi_var into the
    ** next generated function after the high-numerator workaround grew the
    ** instruction stream.  Use generated local labels instead.
    */
    if ((p->Ptype & PTF_ADRMODE) == PTA_REGIS && p->Pr2 == R_SCRREG)
	{
	/* The divisor is already in AC16.  SKIPGE 16,16 followed by JRST
	** only tests its sign, so use the native one-instruction branch.
	*/
	fprintf (out,"\tJUMPL\t%o,%%UIDN%o\n", R_SCRREG, lab);
	}
    else
	{
	p->Pop = P_SKIP+POF_ISSKIP+POS_SKPGE; /* Fetch and test divisor */
	p->Preg = R_SCRREG;
	outinstr (p);
	fprintf (out,"\t JRST\t%%UIDN%o\n", lab); /* Divisor negative */
	}
    fprintf (out,"\tJUMPGE\t%o,%%UIDP%o\n", rq, lab); /* Both positive */

    /* Divisor is positive, but dividend isn't.
    ** Must check for special case of 1, which leaves high-order (sign) bit
    ** still set! (All other values zero it).  Might as well include 0 here.
    */
    fprintf (out,"\tCAIG\t%o,1\n", R_SCRREG);	/* CAIG 16,1 Check divisor */
    fprintf (out,"\t JRST\t%%UIDZ%o\n", lab);	/* Divisor 0 or 1 */

    /* Dividend is neg (has high bit set), divisor doesn't.
    **
    ** Original KCC used:
    **
    **	MOVE	RR,RQ
    **	MOVEI	RQ,1
    **	DIV	RQ,16
    **
    ** to synthesize a double-word unsigned dividend.  That depends on
    ** a fragile DIV corner for high-bit one-word dividends; the PDP-6/KA10
    ** SIMH path used by the DAIMON harness can wedge there.  Avoid that
    ** hardware DIV case by halving first and using a positive signed IDIV.
    ** IDIV uses the single-word dividend in RQ and leaves the remainder in
    ** RR, so it is safe after the shift has cleared the sign bit:
    **
    **	q0 = (u >> 1) / d
    **	r0 = (u >> 1) % d
    **	q  = 2*q0
    **	r  = 2*r0 + (u & 1)
    **	if (r >= d) { r -= d; ++q; }
    **
    ** RQ still receives the quotient and RR the remainder.
    */
    fprintf (out,"\tMOVE\t%o,%o\n", rr, rq);	/* RR = original dividend */
    fprintf (out,"\tANDI\t%o,1\n", rr);	/* RR = original low bit */
    fprintf (out,"\tPUSH\t17,0%o\n", rr);	/* save odd bit */
    fprintf (out,"\tLSH\t%o,-1\n", rq);	/* make dividend positive */
    fprintf (out,"\tIDIV\t%o,0%o\n", rq, R_SCRREG); /* q0/rem0 */
    fprintf (out,"\tLSH\t%o,1\n", rq);	/* q = 2*q0 */
    fprintf (out,"\tLSH\t%o,1\n", rr);	/* r = 2*r0 */
    fprintf (out,"\tADD\t%o,0(17)\n", rr);	/* add saved odd bit */
    outadjsp (-1);
    fprintf (out,"\tCAMGE\t%o,0%o\n", rr, R_SCRREG);
    fprintf (out,"\t JRST\t%%UIDD%o\n", lab);
    fprintf (out,"\tSUB\t%o,0%o\n", rr, R_SCRREG);
    fprintf (out,"\tAOJA\t%o,%%UIDD%o\n", rq, lab);	/* ++quotient, done */

/* Label N: Divisor is negative (high bit is set) */
    /* Because divisor's high bit is set, there's no way the dividend
    ** can be more than twice the magnitude of the divisor.  So the
    ** quotient must be either 0 or 1, with the remainder being respectively
    ** either the dividend or the dividend less 1 times the divisor.
    */
    fprintf (out,"%%UIDN%o:\tMOVE\t%o,%o\n", lab, rr, rq);
    fprintf (out,"\tMOVEI\t%o,0\n", rq);
    fprintf (out,"\tJUMPGE\t%o,%%UIDD%o\n", rr, lab);
    fprintf (out,"\tCAMGE\t%o,0%o\n", rr, R_SCRREG);
    fprintf (out,"\t JRST\t%%UIDD%o\n", lab);
    fprintf (out,"\tSUB\t%o,0%o\n", rr, R_SCRREG);
    fprintf (out,"\tAOJA\t%o,%%UIDD%o\n", rq, lab);

/* Label Z: Divisor is 0 or 1, dividend is neg */
    fprintf (out,"%%UIDZ%o:\tTDZA\t%o,%o\n", lab, rr, rr);
						/* Clear rem and skip next instr */
/* Label P: Divisor and dividend both positive */
    fprintf (out,"%%UIDP%o:\tIDIV\t%o,0%o\n", lab, rq, R_SCRREG);
/* Label D: Done! */
    fprintf (out,"%%UIDD%o:\n", lab);
}


/* OUTOP - Emit opcode and condition-test fields
**
*/
static void
outop (int opr)
{
    char *mnemonic;

    if (opr == 0)
	int_error ("outop: null op");

    mnemonic = popostr[opr & POF_OPCODE];
    if (asmdialect == ASM_GAS)
	{
	while (*mnemonic)
	    {
	    char ch = *mnemonic++;
	    outc (ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
	    }
	}
    else
	outstr (mnemonic);	/* Output assembler opcode mnemonic */

    switch (opr & POF_OPSKIP)
	{
	case POS_SKPA:
	    outc (asmdialect == ASM_GAS ? 'a' : 'A');
	    break;
	case POS_SKPE:
	    outc (asmdialect == ASM_GAS ? 'e' : 'E');
	    break;
	case POS_SKPN:
	    outc (asmdialect == ASM_GAS ? 'n' : 'N');
	    break;
	case POS_SKPL:
	    outc (asmdialect == ASM_GAS ? 'l' : 'L');
	    break;
	case POS_SKPG:
	    outc (asmdialect == ASM_GAS ? 'g' : 'G');
	    break;
	case POS_SKPLE:
	    outstr (asmdialect == ASM_GAS ? "le" : "LE");
	    break;
	case POS_SKPGE:
	    outstr (asmdialect == ASM_GAS ? "ge" : "GE");
	    break;
	default:
	    ;	/* do nothing */
	}

    if (opr & POF_BOTH)
	switch (opr)
	    {
	    case P_MOVN+POF_BOTH:
	    case P_MOVM+POF_BOTH:
		outc (asmdialect == ASM_GAS ? 's' : 'S');
		break;
	    default:
		outc (asmdialect == ASM_GAS ? 'b' : 'B');
		break;
	    }
}

/* OUTREG - Output register field
*/
static void
outreg (int n)
{
    outtab ();

    if (n > 0)
	{
	if (mlist && Register_Preserve (n))
	    outid (Reg_Id[n - (r_maxnopreserve + 1)]->Sname); /* FW 2A (47) */
	else if (mlist && n == R_SP)
	    outid ("SP");
	else
	    outrawnum ((unsigned INT)n);
	outc (',');
	}
}

/* OUTADDRESS - Output address (E) field
*/
static void
outaddress (PCODE *p)
{
    if (p->Ptype & PTF_IND)
	outc ('@');	/* if indirect, say so with atsign */

    if (p->Pptr != NULL)		/* now right half: */
	{
	outmiref (p->Pptr);		/* symbol */
	if (p->Poffset)		/* with offset */
	    /* Do trick here of multiplying by 1.  This is only needed for
	    ** FAIL to force it into making Polish, to avoid wrong-seg
	    ** relocation bug (see FAIL manual doc for the TWOSEG pseudo).
	    ** MACRO and MIDAS aren't bothered by it.
	    */
	    {
	    if (p->Poffset > 0)
		outc ('+');
	    outnum (p->Poffset);
	    }
	}
    else
	outnum (p->Poffset);		/* no sym, just give offset */

    if (p->Pindex)			/* now output index register */
	{
	if (p->Poffset > 01000000L)	/* ensure valid 18 bit address */
	    int_error ("outaddress: bad stk offset 0%o", p->Poffset);
	if (asmdialect != ASM_GAS)
	    outc (' ');
	outc ('(');

	if (mlist && Register_Preserve (p->Pindex))
	    outid (Reg_Id[p->Pindex -
		   (r_maxnopreserve + 1)]->Sname); /* FW 2A (47) */
	else if (mlist && p->Pindex == R_SP)
	    outid ("SP");
	else
	    outrawnum ((unsigned INT)p->Pindex);
	outc (')');
	}
}


/* GASBPLHVAL - Return a numeric PDP-10 byte-pointer left half.
**	KCC's traditional MACRO/FAIL output names constants such as $$BP90.
**	The GNU-as-style bare assembler used by the DAIMON/KCC harness does not
**	provide those KCC runtime CPU symbols, so emit the actual local byte
**	pointer P+S value when possible.
*/
static int
gasbplhval (int i)
{
    int size = 0;
    int slot = 0;
    int pos;

    if (i == CRT_BPPS)
	return 0777700;

    if (i >= CRT_BPH0 && i <= CRT_BPH1) {
	size = 18;
	slot = i - CRT_BPH0;
    } else if (i >= CRT_BP90 && i <= CRT_BP93) {
	size = 9;
	slot = i - CRT_BP90;
    } else if (i >= CRT_BP80 && i <= CRT_BP83) {
	size = 8;
	slot = i - CRT_BP80;
    } else if (i >= CRT_BP70 && i <= CRT_BP74) {
	size = 7;
	slot = i - CRT_BP70;
    } else if (i >= CRT_BP60 && i <= CRT_BP65) {
	size = 6;
	slot = i - CRT_BP60;
    } else {
	return -1;
    }

    pos = slot * size + size - 1;
    return ((35 - pos) << 12) | (size << 6);
}

/* OUTPTI - output immediate pointer operand.
**	This is used for TLZ and TLO.
**	We already know that the instruction's operand is a PTA_PCONST
** with no symbol, so the RH is zero and only the LH is needed.
** Note $$SECT should not be used as we are masking BP position bits rather
** than address bits.  Error if byte offset is so large that a word offset
** becomes necessary.
** Bytesize of pointer (0 for illegal word ptr) and # bytes offset from 
** start of word.
*/
static void
outpti (int bsize, INT offset)
{
    int i;
    INT woff = 0;

    /* DAS/GAS can express arbitrary PDP-10 byte sizes directly.  Avoid
    ** routing those through KCC's historical $$BPxx table, which only
    ** contains the traditional 6/7/8/9/18-bit byte sizes.
    */
    if (asmdialect == ASM_GAS && bsize > 0 && bsize < TGSIZ_WORD) {
	int bpw = TGSIZ_WORD / bsize;
	int boff = adjboffset(offset, &woff, bpw);
	int pos;

	if (woff) {
	    int_error("outpti: byte offset crosses word");
	    outnum(0777700);
	    return;
	}
	pos = boff * bsize + bsize - 1;
	outnum(((35 - pos) << 12) | (bsize << 6));
	return;
    }

    if ((i = obplh (offset, &woff, bsize)) == 0
      || woff)
	{
	int_error ("outpti: bad args");
	i = CRT_BPPS;
	}
    if (asmdialect == ASM_GAS) {
	int n = gasbplhval (i);
	if (n >= 0) {
	    outnum (n);
	    return;
	}
    }

    outstr (crtsnam[i]);		/* Output byte pointer bits */
    ++crtref[i];
}

/* OUTPTR - output pointer value, for word or byte pointer.
**	sym - address identifier (if non-NULL)
**	offset - offset in bytes from given address
**	bsize - size of bytes in bits.
**		0 = word pointer, no P+S in left half.
** If the address identifier is NULL, no $$SECT is output for the LH.
** This could be used to create absolute pointers.
** Identifier to point at and Bytesize of pointer (0 = word) and  # units 
** offset from identifier
*/

void
outptr (SYMBOL *sym, int bsize, INT offset)
    {
    register int i = 0;
    INT woff = 0;


    if (asmdialect == ASM_GAS && bsize)
	{
	int boff = adjboffset(offset, &woff, TGSIZ_WORD / bsize);
	fprintf (out, "POINT %d,", bsize);
	if (sym)
	    outmiref (sym);
	else
	    outstr ("0");
	if (woff)
	    {
	    if (woff > 0)
		outc ('+');
	    outnum (woff);
	    }
	fprintf (out, ",%d", boff * bsize + bsize - 1);
	return;
	}

    if (asmdialect == ASM_GAS && !bsize)
	{
	if (sym)
	    outmiref (sym);
	else
	    outstr ("0");
	if (offset)
	    {
	    if (offset > 0)
		outc ('+');
	    outnum (offset);
	    }
	return;
	}

    if (bsize && (i = obplh (offset, &offset, bsize)) != 0)
	{
	outstr (crtsnam[i]);		/* Output byte pointer bits */
	++crtref[i];
	}

    /* Now do rest of pointer (the word address) */

#ifdef	MULTI_SECTION /* FW 2A (51) */
    if (sym)
	{
	if (i)
	    outstr ("+");		/* Combine with P+S if any */
	outstr (crtsnam[CRT_SECT]);	/* Make global word address */
	++crtref[CRT_SECT];
	}
#endif
 
    outstr (",,");

    if (sym)
	outmiref (sym);		/* Either internal static, or normal extern */
    else
	outstr ("0");

    if (offset)		/* If non-zero word offset, */
	{
	outstr ("*1");		/* See outaddress () for crock explanation */

	if (offset > 0)
	    outc ('+');	/* Add or subtract it */

	outnum (offset);		/* note outnum prefixes '-' if negative */
	}
    }

/* OBPLH - Find BP left-half value
**	If bsize = -1, use P+S mask.  This is meaningless for most all
** ops except P_TDZ or P_TLZ, however.
*/
static int
obplh (INT boff, INT *awoff, int bsize)
/* Byte offset, Addr of place to return word-offset to, Byte size (0 if word)*/
{
    switch (bsize)
	{
	default:
	    int_error ("obplh: bad bsize: %d", bsize);
	/* FALLTHROUGH */
	case -1:		/* Return P+S mask */
	    return CRT_BPPS;
	case 0:			/* Word pointer */
	    return 0;
	case 6:
	    return CRT_BP60 + adjboffset (boff, awoff, 6);
	case 7:
	    return CRT_BP70 + adjboffset (boff, awoff, 5);
	case 8:
	    return CRT_BP80 + adjboffset (boff, awoff, 4);
	case 9:
	    return CRT_BP90 + adjboffset (boff, awoff, 4);
	case 18:
	    return CRT_BPH0 + adjboffset (boff, awoff, 2);
	}
}


/* ADJBOFFSET - Adjust byte offset.  Auxiliary for obplh () and foldadjbp ().
*/
int
adjboffset (INT boff, INT *awoff, int bpw)
/* Byte offset to adjust, Place to deposit word offset, # bytes per word */
{
    if (boff < 0)
	{
	/* Negative increment, need to go back far enough
	** that boff can become a positive offset within a word.
	*/
	*awoff = - ((-boff) / bpw);	/* Find -# words */
	if ((boff = (-boff) % bpw) != 0)	/* If any bytes, */
	    {
	    (*awoff)--;			/* must bump back 1 more word */
	    return bpw - (int) boff;	/* and return positive offset. */
	    }
	else
	    return 0;
	}
    else		/* Positive increment, simple. */
	{
	*awoff = boff / bpw;		/* Find # words */
	return (int) (boff % bpw);		/* and remaining # bytes */
	}
}

/* Floating-point auxiliary functions */

/* OUTFLT - Output floating-point constant value.
**	Handles float, double, long double.
**	If no flags given, assumes outputting all of value as a static data
**	constant (used by CCGEN), else a code literal constant.
**	Returns the number of words emitted.
*/
#define OF_CONST 0100	/* Code literal constant */
#define OF_WD1 01	/* Emit word 1 only */
#define OF_WD2 02	/* Emit word 2 only */

int
outflt (int typ, INT *ptr, int flags)
/* Tspec of value, Generic pointer to object, and flags */
{
    if (!flags)
	flags = OF_WD1|OF_WD2;
    if (flags & OF_CONST)
	outc ('[');		/* Set up as literal constant ] */
    else
	{
	outtab ();
	if (asmdialect == ASM_GAS)
	    outstr (".word ");
	}
    if (typ == TS_FLOAT)
	{
#ifdef __COMPILER_KCC__
	outpnum (*ptr);
#else
	{
	float f = (float) * (double *)ptr;
	outmpdbl ((INT *)&f, 1);
	}
#endif
	typ = 1;
	}
    else if (tgmachuse.mapdbl)	/* If target mach fmt is different */
	{
	outmpdbl (ptr, 3);		/* output mapped double */
	typ = 2;
	}
    else
	{
	outpnum (ptr[0]);
	outnl ();
	outtab ();
	if (asmdialect == ASM_GAS)
	    outstr (".word ");
	if (flags & OF_CONST)
	    outtab ();
	outpnum (ptr[1]);
	typ = 2;
	}
    if (flags & OF_CONST)
	outc (']');

    /* Add a readable comment so humans can understand what the floating
    ** constant is.  However, because this is slow, only do it when
    ** we know the assembler output is going to stay around.
    */
    if (!delete)		/* If keeping asm file around, add comment */
	{
	fprintf (out, "\t; %.20g", * (double *)ptr);
	}
    if (! (flags & OF_CONST))
	outnl ();
    return typ;		/* Return # wds emitted */
}

static void		/* output exactly n digits octal unsigned int */
outlpnum (unsigned INT n, int dig)
{
    if (dig > 1)
	outlpnum (n >> 3, dig - 1);
    putc ((n & 7) + '0', out);
}

#ifndef __COMPILER_KCC__
static unsigned long
host_u32_from_bytes (unsigned char * b)
{
    union { unsigned int i; unsigned char c[sizeof(unsigned int)]; } u;

    u.i = 1;
    if (u.c[0] == 1)
	return (((unsigned long)b[3] << 24)
	      | ((unsigned long)b[2] << 16)
	      | ((unsigned long)b[1] << 8)
	      |  (unsigned long)b[0]);
    return (((unsigned long)b[0] << 24)
	  | ((unsigned long)b[1] << 16)
	  | ((unsigned long)b[2] << 8)
	  |  (unsigned long)b[3]);
}

static unsigned long
host_float_bits (float * fp)
{
    unsigned char b[4];

    if (sizeof (*fp) != 4)
	int_error ("host_float_bits: unsupported host float size");
    memcpy ((char *)b, (char *)fp, 4);
    return host_u32_from_bytes (b);
}

static void
host_double_bits (double d, unsigned long * lo, unsigned long * hi)
{
    unsigned char b[8];
    union { unsigned int i; unsigned char c[sizeof(unsigned int)]; } u;

    if (sizeof (double) != 8)
	int_error ("host_double_bits: unsupported host double size");
    memcpy ((char *)b, (char *)&d, 8);
    u.i = 1;
    if (u.c[0] == 1)
	{
	*lo = host_u32_from_bytes (b);
	*hi = host_u32_from_bytes (b + 4);
	}
    else
	{
	*hi = host_u32_from_bytes (b);
	*lo = host_u32_from_bytes (b + 4);
	}
}

static void
host_pdp10_double_words (double d, unsigned INT * w1, unsigned INT * w2)
{
    unsigned INT word1, word2;
    unsigned INT hi18, lo18;
    unsigned INT value, second, exp, sign;
    unsigned long lo32, hi32;

    host_double_bits (d, &lo32, &hi32);
    if (lo32 == 0 && hi32 == 0)
	{
	*w1 = 0;
	*w2 = 0;
	return;
	}

    second = (unsigned INT)lo32;
    sign   = (unsigned INT)(hi32 & (1UL << 31));
    exp    = (unsigned INT)((hi32 >> 20) & 03777UL);
    exp    = exp + 2;
    exp    = exp + (128 - 1024);
    value  = (unsigned INT)(hi32 & 03777777UL);
    value  = value + 04000000UL;

    if (sign)
	{
	second = ~second + 1;
	value  = ~value + (second == 0);
	value &= 07777777UL;
	exp    = ~exp & 0377UL;
	}

    hi18 = ((sign | (exp << 23) | (value << 2)) >> 14) & 0777777UL;
    lo18 = ((value << 6) | (second >> 26)) & 0777777UL;
    word1 = (hi18 << 18) | lo18;

    hi18 = (second >> 9) & 0777777UL;
    lo18 = (second << 9) & 0777777UL;
    word2 = (hi18 << 18) | lo18;

    *w1 = word1;
    *w2 = word2;
}
#endif

/* OUTMPDBL - Output mapped double-format constant
**	This is also used by CCGEN for data.
*/
static void
outmpdbl (INT *ip, int which)  /* 1 = 1st wd, 2 = 2nd wd, 3 = both wds (dbl) */
{
#ifdef  __COMPILER_KCC__		/* native floating point format */

    INT second;		/* 2nd word is the different one */
    INT exp;		/* Gotta derive new exponent */

    if ((second = ip[1]) != 0)	/* Only if low order word is non-zero */
	switch (tgmachuse.mapdbl)
	    {
	    case 1:	/* Internal format is hardware, output in software format */
		exp = (*ip < 0 ? -*ip : *ip) & ~ ((1<<27)-1);	/* Mask off pos exp */
		second = (( (unsigned INT)second) >> 8) | (exp - (27<<27));
		break;
	    case -1:	/* Internal format is software, output in hardware format */
		second = (second << 8) & ~ (1<<35);	/* Just flush 2nd exp! */
		break;
	    default:
		int_error ("outmpdbl: bad map");
	    }
    if (which&01)
	outpnum (*ip);		/* 1st wd always output as is */
    if (which==03)
	outstr ("\n\t\t");
    if (which&02)
	outpnum (second);	/* 2nd word has been mapped */
#else			/* not native - assume IEEE as used by Turbo C */
    /*
     *	MSDOS version:
     *		Convert float/double from MSDOS internal format
     *				     to PDP-10 internal (hardware) format
     */
    unsigned INT value   = 0,
		 second  = 0,
		 exp     = 0,
		 sign    = 0,
		 outword = 0;

    unsigned long lo32 = 0, hi32 = 0, f32 = 0;

    if (which == 3)			/* double precision input */
	{
	host_double_bits (*((double *)ip), &lo32, &hi32);
	if (lo32 == 0 && hi32 == 0)
	    {
	    outstr ("0\n\t\t0");			/* special case: input zero */
	    return;
	    }
	/*
	 *	input is in two longs; ip[1] is most significant.
	 *	First bit is sign.  Next 11 bits are exponent, XS 1024.
	 *	Remaining 20 bits, plus ip[0], are a 52 bit fraction
	 *	with a leading 1 implied.
	 */
	second = lo32;
	sign   = (hi32 & (1UL << 31));
	exp    = (hi32 >> 20) & 03777;	    /* normalized exponent */
	exp    = exp + 2;		    /* adjust for pdp-10 usage */
	exp    = exp + (128 - 1024);	    /* adjust to 8 bits XS 128 */
	value  = (hi32 & 03777777L);	    /* fractional part */
	value  = value  + 04000000L;	    /* add implied leading 1 */

	if (sign)				/* if negative */
	    {
	    second = ~second + 1;		/*   2's complement of */
	    value  = ~value + (second == 0);	/*   a 53 bit number!  */
	    value &=  07777777L;
	    exp    = ~exp & 0377;               /*   1's comp of exp */
	    }
	/*
	 *	Have now transformed components of input;
	 *	Must output them 18 bits at a time,
	 *	with trailing zeros for where we lack precision in 32 bits.
	 */
	outword = (sign | (exp << 23) | (value << 2)) >> 14;
	outpnum (outword);			/* first 18 bits of result */
	outword = ((value << 6) | (second >> 26));
	outlpnum (outword, 6);			/* next 18 bits */
	outstr ("\n\t\t");
	outword = (second >> 9) & 0377777L;
	outpnum (outword);			/* 3rd 18 bits */
	outword = (second << 9);
	outlpnum (outword, 6);			/* last 8 bits plus 0's */
	}
    else				/* floating point */
	{
	if (which == 2)
	    ip++;			/* use 2nd wd instead of 1st */

	f32 = host_float_bits ((float *)ip);
	if (f32 == 0)
	    {
	    outpnum (0L);		/* special case: input = zero; */
	    return;
	    }
	/*
	 *	input is one long integer;
	 *	First bit is sign; next 8 bits are exponent in XS 128.
	 *	Remaining 23 bits are fraction with leading 1 implied.
	 */
	sign   = (f32 & (1UL << 31));
	exp    = (f32 >> 23) & 0377;	    /* normalize exponent */
	exp    = exp + 2;                   /* adjust to PDP-10 world  */
	value  = (f32 & 037777777L);	    /* isolate fractional part */
	value  = value  + 040000000L;	    /* add implied leading 1   */

	if (sign)			/* if negative, */
	    {
	    value = ((unsigned int) -((int) value)) & 077777777L;	/*    take 2's complement of value */ // FW KCC-NT
	    exp   = ~exp   & 0377;	/*    take 1's complement of exp   */
	    }
	/*
	 *	Now output combined transformed parts in 2 18-bit chunks.
	 */
	outword = (sign | (exp << 23) | (value >> 1)) >> 14;
	outpnum (outword);		/* high 18 bits of result */
	outword = (value << 3);		/* remaining 15 bits, 0 terminated */
	outlpnum (outword, 6);
	}
#endif
}

/* BINEXP - count zero bits to right of rightmost 1 in word.
**	Used for converting a power-of-2 value into a shift count.
**	Also called by CCEVAL for constant folding.
*/
INT
binexp (unsigned INT n)
{
    INT e;

    e = -1;				/* init count of bits to shift */
    do
	{
	n >>= 1;			/* logical shift over one */
	e++;				/* and count a zero */
	}
    while (n != 0)
	;			/* until that was the last bit */
    return e;				/* return number of bits */
}

/* FLTPOW2 (d) - See if arg is positive and a power of 2
 *	Returns zero if not, else non-zero integer exponent.
 * Very machine-dependent, only works for standard single-precision
 * PDP-10 floating point format, and assumes a normalized number.
 *	If the number is a power of 2, only bit 9 will be set in
 * the mantissa (the rest will be 0) and the exponent field (bits 1-8)
 * can tell us what we want to know - it is in excess 128 code, so
 * that an exponent X for the fractional mantissa is represented by X+128.  
 * Subtracting 129 then gives us the power of 2 for the integer.
 *	Minor screw: 1.0 (2 to the 0th) returns 0 which is the same as
 * an error return.  However, the two cases of using 0.0 or 1.0 as operand
 * should have been caught earlier in optimization.
 */
static int
fltpow2 (double d)
{
#ifdef __COMPILER_KCC__
    unsigned INT u = (unsigned INT) (* (INT *) (&d));

    if ((d > 0.0 && u & 03777777777L) == 0)
	return (int) (u >> 27) - 129;
#else  /* Not KCC */
    unsigned long lo32, hi32;
    int exp;

    if (d > 0.0)
	{
	host_double_bits (d, &lo32, &hi32);
	exp = (int)((hi32 >> 20) & 03777);
	if (exp != 0 && (hi32 & 03777777L) == 0 && lo32 == 0)
	    return exp - 1023;
	}
#endif
    return 0;
}

/* BIGFLOAT - See whether 1st word of a floating-point number is too big
**	to fit into a halfword (i.e. RH has some non-zero bits).  
**	Note this only checks floats, not doubles!
*/
static int
bigfloat (PCODE *p)
{
    return (( (* (unsigned INT *) (&p->Pfloat))
		 & 0777777L) != 0);	/* If 1st wd RH has bits, big. */
}

/* OUTSCON - Output string constant.
**	Used to always produce an ASCIZ string, but now uses
** whatever bytesize is specified.
** The major hack to be noted here is that 6-bit characters are
** converted to PDP-10 SIXBIT!
*/
void
outscon (char *s, int l, int bsiz)
/* Char string,  Length (may include nulls!), and Byte size to use. */
{
    int i, sepchar = ',';
    char *opstr = asmdialect == ASM_GAS ? ".byte\t%d," : "BYTE\t (%d) ";

    --s;			/* Set up for preincrement */
    while (l > 0)			/* For each word */
	{
	fprintf (out, opstr, bsiz);	/* Start bytes of given size */
	for (i = (TGSIZ_WORD/bsiz);;)
	    {
	    if (bsiz == 6)
		outnum (*++s ? (toupper (*s)-040) : 0);
	    else
		outnum (*++s);		/* output char value */
	    if (--l <= 0)
		break;
	    if (--i <= 0)
		break;
	    outc (sepchar);
	    }
	outstr ("\n\t");

	if (mlist)
	    oline++;
	}

    outnl ();
}

/*
 * outlab
 *
 * Output label.  Note that the identifier has already been mangled,
 * if we are not using long identifiers.
 */

void
outlab (SYMBOL *s)
    {
    outid (s->Sname);		/* Output the actual label name */

    if (asmdialect == ASM_GAS)
	outstr (":\n");
    else if (s->Sname[0] == '$')	/* Local label? */
	outstr ("==.\n");	/* Yes, define it as half-killed.  See note. */
    else
	outstr (":\n");		/* No, normal label. */

    if (mlist)
	oline++;
    }

/*
 * outid
 *
 * Output ASCII identifier to assembler file.  Identifiers are
 * truncated here, and undergo some character-substitution
 * mangling: '_' becomes '%' .  "longidents" circumvents
 * truncation.
 */

void
outid (char *s)
    {
    int		n,
		ch;

    if (asmdialect == ASM_GAS && s[0] == '$' && s[1] >= '0' && s[1] <= '9')
	{
	putc ('%', out);
	putc ('L', out);
	++s;
	}

    if (!longidents)			/* FW 2A(51) */
	n = 6;				/* Max # chars to output */

    if ((ch = *s) == SPC_IDQUOT)	/* Handle quoted identifier syms */
	ch = *++s;

    while (ch)
	{
	if (ch == '_' && asmdialect != ASM_GAS)
	    ch = UNDERSCORE_MAPCHR;

	putc (ch, out);

	if (!longidents && (--n <= 0))	/* FW 2A(51) */
	    break;

	ch = *++s;
	}
    }

/*
 * OUTPROLOG - Output Profiler call from Function Prolog
 *	       added 09/15/89 by MVS
 */

void
outprolog (SYMBOL *s)
    {
    fputs ("\tPROF.\t0,", out);
    outmiref (s);
    putc ('\n', out);	/* 10/91 BEN, fix -p */
    }

/*
 * OUTEPILOG - Output Profiler call from Function Epilog
 *	       added 09/15/89 by MVS
 */

void
outepilog (SYMBOL *s)
    {
    fputs ("\tPROF.\t1,", out);
    outmiref (s);
    putc ('\n', out);

    if (mlist)
	oline++;
    }

/*
 * outmidef
 *
 * Output Mapped Identifier Definition
 */

void
outmidef (SYMBOL *s)
    {
    putc ('\n', out);

    if (mlist)
	oline++;

    outmiref (s);

    if (asmdialect != ASM_GAS && s->Sclass == SC_EXTDEF)
	putc (':', out);

    putc (':', out);
    if (asmdialect == ASM_GAS)
	putc ('\n', out);
    }

/*
 * outmiref
 *
 * Output Mapped Identifier Reference
 */

void
outmiref (SYMBOL *s)
    {
    switch (s->Sclass)
	{
	case SC_ISTATIC:
	    outid (s->Ssym->Sname);
	    break;

	case SC_LABEL:
	case SC_ULABEL:
	case SC_ILABEL:
	    outid (s->Sname);
	    break;

	default:

	    if (longidents)		/* FW 2A(51) */
		outid (s->Sname);	/* FW 2A(51) */
	    else if (s->Smaplab)
		outsix (s->Smaplab);	/* Output SIXBIT */
	    else
		int_error ("outmiref: no map for \"%s\"", s->Sname);
	}
}

/* Low-level assembler file output functions */

/* OUTSTR - Output string
*/
void
outstr (char *s)
{
    if (*s)
	{
	do
	    putc (*s, out);
	while (*++s)
	    ;
	}
}

void outpghdr (void)
{
    opage++;
    if (opage > 1)
	putc ('\f', out);
    fprintf (out, "; %-26s\t\tCompuServe Incorporated\t\t%s\t  Page %d\n",
	dspfname, creatime, opage);

    /* KAR-3/91, took out one tab to re-align page header */
    fprintf (out, "; KCC: %-20s\t\t\t\t\t\t%s\t\n\n", ver_str, comptime);
    oline = 3;
}

void outnl (void)
{
    putc ('\n', out);

    if (mlist)
	{
	oline++;
	if (oline > MAX_OLINE)
	    outpghdr ();
	} /* if mlist */
}

/* OUTNUM - Output value as a signed octal number (with minus sign if negative)
*/
void
outnum (INT n)
{
#if __MSDOS__
    if (!unsign_int)
#endif
	if (n < 0)
	    {
	    n = -n;
	    putc ('-', out);
	    }
    /* DAS follows C integer syntax: an unprefixed number is decimal.
    ** KCC's assembler output is traditionally octal, so make that radix
    ** explicit for every numeric operand, including accumulator numbers.
    ** DAS interprets an unprefixed token as decimal, whereas KCC's emitted
    ** assembler syntax is traditionally octal.
    */
    outpnum ((unsigned INT)n);
}

/* OUTRAWNUM - Output octal digits without a radix prefix.  Use only where
** DAS parses the field as octal by definition, such as an AC or index field.
*/
static void
outrawnum (unsigned INT n)
{
    if (n &~ 07)
	outrawnum (n >> 3);
    putc ((n & 07) + '0', out);
}

/* OUTPNUM - Output a positive unsigned octal constant with an explicit
** leading zero for DAS's decimal-default expression grammar.
*/
static void
outpnum (unsigned INT n)
{
    if (n &~ 07)
	putc ('0', out);
    outrawnum (n);
}

/* OUTSIX - Output SIXBIT word, ignoring trailing blanks
*/
void
outsix (unsigned INT wd)
{
#ifdef __COMPILER_KCC__			/* Got 36 bits?  then use sixbit */
    while (wd && ! (wd & 077))
	wd >>= 6;	/* Right-justify the sixbit */
#else					/* Got only 32?  then use rad 50 */
    while (wd && ! (wd % 050))
	wd /= 050;
#endif
    outrj6 (wd);
}

static void
outrj6 (unsigned INT ms)
{
    INT ch6;

#ifdef __COMPILER_KCC__			/* Got 36 bits: actually use sixbit */
    ch6 = (ms&077) + ' ';
    if ((ms >>= 6) != 0)
	outrj6 (ms);
    putc (ch6, out);
#else					/* Got 32 bits: use rad 50 */
    ch6 = ms %  050;
    if ((ms /= 050) != 0)
	outrj6 (ms);
    putc ((char) (fromrad50 ((char) ch6)), out);
#endif    
}

/*
 * outiprolog ()
 *
 * Emit prolog for an interrupt function.
 */

void
outiprolog (void)
{
    int nsave = r_maxnopreserve + 3;

    /* Save AC0..AC7 plus AC15/AC16.  Normal C functions preserve the
    ** latter pair at their boundary; an interrupt handler must preserve
    ** them itself because it can interrupt arbitrary code.
    */
    if (tgmachuse.adjsp)
	fprintf (out, "\tADJSP\t17,%o\n", nsave);
    else {
	outstr ("\tADD\t17,[");
	outnum (nsave);
	outstr (",,");
	outnum (nsave);
	outstr ("]\n");
    }
    fprintf (out, "\tMOVEM\t%o,(17)\n", R_SCRREG);
    fprintf (out, "\tMOVEM\t%o,-1(17)\n", R_SCRREG - 1);
    fprintf (out, "\tMOVEM\t%o,-2(17)\n", r_maxnopreserve);
    fprintf (out, "\tMOVEI\t%o,-%o(17)\n", r_maxnopreserve,
	     r_maxnopreserve + 2);
    fprintf (out, "\tBLT\t%o,-3(17)\n", r_maxnopreserve);
    }


/*
 * outiepilog ()
 *
 * Emit epilog for an interrupt function.
 */

void
outiepilog (void)
{
    int nsave = r_maxnopreserve + 3;

    fprintf (out, "\tMOVE\t%o,(17)\n", R_SCRREG);
    fprintf (out, "\tMOVE\t%o,-1(17)\n", R_SCRREG - 1);
    fprintf (out, "\tMOVSI\t%o,-%o(17)\n", r_maxnopreserve,
	     r_maxnopreserve + 2);
    fprintf (out, "\tBLT\t%o,%o\n", r_maxnopreserve, r_maxnopreserve);
    if (tgmachuse.adjsp)
	fprintf (out, "\tADJSP\t17,-%o\n", nsave);
    else {
	outstr ("\tSUB\t17,[");
	outnum (nsave);
	outstr (",,");
	outnum (nsave);
	outstr ("]\n");
    }
    fputs ("\tDEBRK$\n", out);
    }
