/*	CCLEX.C - New KCC Lexer - Token input
**
**	(c) Copyright Ken Harrenstien 1989
**		All changes after v.150, 8-Apr-1988
**	(c) Copyright Ken Harrenstien, SRI International 1985, 1986
**		All changes after v.43, 8-Aug-1985
**
**	Original version (C) 1981  K. Chen
*/

#include "cc.h"
#include "ccchar.h"
#include "cclex.h"	/* Get stuff shared with CCINP */
#include <stddef.h>	/* ptrdiff_t */
int strcmp (const char *, const char *);
int strncmp (const char *, const char *, size_t);
size_t strlen (const char *);

/* Imported functions */
extern SYMBOL *symfind(char *, int);	/* CCSYM */
extern int nextpp(void);		/* CCPP */
extern void pushpp(void);		/* CCPP */

/* Exported functions defined in CCLEX: */
void lexinit(void);		/* Initializes the lexer (CC) */
int nextoken(void);	/* Reads and parses next token(CCDECL,CCERR,CCSTMT) */
void tokpush(int t, SYMBOL *s);	/*Pushes back a token (like ungetc) (",",") */
long lex_take_gnuattrs(void);

/* Internal functions */
static int trident(void), trintcon(void), trfltcon(void),
	trstrcon(void), trchrcon(void);
static int spcident(char *, char *, int), cchar(char **);
static int zerotok(void), dzerotok(void), szerotok(void);
static int strctok(char *), skipgnuattr(void), skipcxxattr(void),
	attralignval(char *), gnuattrname(char *, char *);
static long gnuattr_pending;
static char *stripintseps(char *, char *, int);
char ra_expr;

/* Globals used */
extern int savelits;	/* Set 0 by CC main parsing loop for each toplevel
			** declaration parse, to indicate that string literal
			** space is free and can be re-used again.
			*/
#if SYS_CSI /* KAR-11/91, usage before initializtion, used to signal if
	     * parsing the right hand side of an assignment expr.
	     */
extern char ra_expr;
#endif

/* See also stuff in "cclex.h" */

/* Globals set:
 *	int token	Current token code.
 *   If token==T_ICONST, T_CCONST, T_FCONST, T_SCONST
 *	struct {} constant	contains type+value of constant (CCINP,CCSTMT)
 *   If token==Q_IDENT or a reserved-word token,
 *	SYMBOL *csymbol		contains pointer to SYMBOL for this identifier.
**				If it hasn't yet been defined, it will be a
**				global symbol with class SC_UNDEF.
 *					(CCDECL,CCERR,CCSTMT)
 *
 * Note: the "constant" structure is not correct after nextoken() returns
 * a token which was pushed back by tokpush().
 *
 * Note that most routines operate, or begin to operate, on the current
 * token in "token", rather than immediately reading the next token.  When
 * a token is completely processed and is not needed any more, nextoken()
 * must be called in order to get rid of it and set up a new token for
 * whatever will be next looking at the input.  Occasionally "token" is
 * set directly for proper "priming".
 */	

/* Token stack - entries added by tokpush(), removed by nextok() */
static int tokstack;
static struct
    {
    int      ttoken;
    SYMBOL  *tsym;
    }
tstack[MAXTSTACK];

/* String literal char pool */
static char *slcptr = NULL;	/* Pointer into slcpool */
static int slcleft;		/* Countdown of # free chars left */
static int slcocnt;		/* Saved slcleft for deriving string len */

/* Macros to handle deposit of chars into string literal char pool (slcpool)*/
#if 0	/* 5/91 Dynamic tables */
static void slcresize();
static unsigned char slcsize = 0;
 /* String literal character pool, non-static since CCSTMT needs csptr as
  * an offset, thus base (slcpool) needed.
  */
static char *slcpool = NULL;
 #define slcreset() ((!slcpool? slcresize():0), slcleft =	\
	 slcsize*DYN_SIZE - 1, slcptr=slcpool)
 #define slcput(c) ((--slcleft > 0 ? 0:slcresize()), *++slcptr = (c))
 #define slcend() ((--slcleft > 0 ? 0:slcresize()), *++slcptr = 0, slclen())
#else
static char slcpool[CPOOLSIZE];	/* String literal character pool */
 #define slcreset() (slcleft=CPOOLSIZE-1, slcptr=slcpool)
 #define slcput(c) (--slcleft > 0 ? *++slcptr = (c) : (c))
 #define slcend() (--slcleft > 0 ? (*++slcptr = 0, slclen()) : -1)
#endif
 #define slcbeg() (slcocnt=slcleft, slcptr+1)
 #define slclen() (slcocnt - slcleft)

/* LEXINIT() - Initialize the lexer
**	The symbol table must have already been set up (by initsym)
**	and the preprocessor initialized (by initinp)
**	otherwise the initial nextoken() will not work properly.
*/
void
lexinit(void)
{
    gnuattr_pending = 0;
    tokstack = 0;
    ra_expr = 0;
    savelits = 0;		/* OK to reset string literal char pool */
    if (!prepf)
	nextoken();	/* Prime with 1st token */
}

/* TOKPUSH(tok, sym) - Push a token
**	Note that the "constant" structure is not pushed or changed.
** It is OK for the current token to be a constant, if the token pushed
** (arg to tokpush) is not a constant.  In fact, no constants can be
** pushed.  The code for unary() in CCSTMT is the only place where this
** sort of thing has to be taken into account.
*/
void
tokpush(int t, SYMBOL *s)
{
    if(++tokstack >= MAXTSTACK)		/* Token stack depth exceeded? */
	--tokstack, int_error("tokpush: tokstack overflow");
    else
	{
	tstack[tokstack].ttoken = token;
	tstack[tokstack].tsym = csymbol;
	token = t;
	csymbol = s;
	}
}

/* NEXTOKEN() - Get next C language token, by transforming one or more
**	PP-tokens from CCPP.
*/

int
nextoken (void)
{
    if (tokstack)		/* Pop token from push-back stack */
	{
	csymbol = tstack[tokstack].tsym;
	token = tstack[tokstack--].ttoken;
	}
    else
        {
	csymbol = NULL;			/* Clear sym associated with token */

	for (;;)
	    {
	    switch (token = nextpp ())	/* Get next preproc token */
	        {
		case T_WSP:		/* Just skip whitespace */
		case T_EOL:
		    continue;		/* for */
		
		default:		/* Most returned directly! */
		    break;		/* switch */

		/* Transform things that need transforming */
		
		case T_IDENT:
		    token = trident();	/* Identifier */
		    if (token == T_ATTRIBUTE || token == T_ATTRIBUTE2)
			{
			if (!skipgnuattr())
			    error("Bad __attribute__ syntax");
			continue;
			}
		    break;		/* switch */
	

		case T_ICONST:
		    token = trintcon();	/* Integer constant */
		    break;		/* switch */
		    

		case T_FCONST:
		    token = trfltcon();	/* Floating constant */
		    break;		/* switch */
		    

		case T_CCONST:
		    token = trchrcon();	/* Char constant */
		    break;		/* switch */
		    

		case T_SCONST:
		    token = trstrcon();	/* String constant */
		    break;		/* switch */

		case T_LBRACK:
		    if (skipcxxattr())
			continue;
		    break;		/* switch */
		    
		/*
		 * Do debug checking to catch PP-only stuff.  This would be
		 * caught later on by higher levels, but most responsible
		 * to screen them here.
		 */
		
		case T_MACRO:
		case T_MACARG:
		case T_MACINS:
		case T_MACSTR:
		case T_MACCAT:
		    int_error ("nextoken: PP-only token %Q", token);
		    continue;		/* for */


		case T_SHARP:
		case T_SHARP2:
		    error ("# or ## can only appear in directives or macros");
		    continue;		/* for */
		

		case T_UNKNWN:
		    error ("Unknown token: \"%s\"", curval.cp);
		    continue;		/* for */
		}

	    break;
	    }
	}

    /*
     * A lexing pre-compilation scheme will break the input stream
     * at this point.  Tokens will be diverted to a file, along with
     * their associated string literals and constant values, if any;
     * the symbol and type tables will also be dumped.
     */

    /*
     * Usage-before-initialization tracking belongs to the lexer/parser
     * boundary, not the raw preprocessor.  This keeps the cooked-token
     * interface complete when CCPP and the compiler run in separate
     * processes.
     */
    switch (token) {
    case Q_ASGN:
	ra_expr = 1;
	break;
    case T_COMMA:
    case T_SCOLON:
    case T_LBRACE:
	ra_expr = 0;
	break;
    default:
	break;
    }
    return token;
}

/* TRIDENT() - Transform identifer token
**
** Sets "csymbol" to point to the resulting symbol, and then returns the token
** corresponding to the given identifier (i.e. reserved word or Q_IDENT).
*/

static int
trident(void)
{
    char ident[IDENTSIZE+4];	/* Identifier big enuf to trigger trunc */
    char *cp;

#if SYS_CSI /* KAR-11/91, needed temp storages for v1=v2=v3...; check */
    int t;
    SYMBOL *s;
#endif

    if ((cp = curval.cp) == 0)
	{
	int_error("trident: no string");	/* No string for T_IDENT */
	return zerotok();
	}
    if ((!strcmp(cp, "__func__") || !strcmp(cp, "__FUNCTION__")
      || !strcmp(cp, "__PRETTY_FUNCTION__")) && curfn != NULL)
	return strctok(curfn->Sname);

    if ((csymbol = cursym) != 0)
	switch (csymbol->Sclass)
	    {
	    case SC_RW:		/* Reserved word, use its token */
		return token = (int) csymbol->Stoken;
	    case SC_MACRO:		/* Paranoia check on CCPP */
		int_error("trident: Escaped macro %S", csymbol);
	    /* FALLTHROUGH */
	    default:		/* Normal symbol, just return identifier */
#if SYS_CSI /* KAR-11/91, usage bef. init. code */
	    /* KAR-11/91, added check for v1=v2=v3...; code */
		if (ra_expr == 1)	/* if parsing right side of asgn. expr */
		    switch (csymbol->Sclass)
			{
			case SC_AUTO:
			case SC_RAUTO:
			case SC_ISTATIC:
			    t = token;
			    s = csymbol;
			    if (nextoken() != Q_ASGN)	/* check for v1=v2=v3...; */
				if ((!s->Sinit) && (s->Stype->Tspec != TS_ARRAY))
				    {
				    warn("Possible usage before initialization: %s",
					    s->Sname);
				    s->Sinit = 1;
				    }
			    tokpush (t, s);	/* push back last token */
			    break;
			default:
			    break;
			}
		else			/* & address op seen before ident */
		/*
		 * KAR-11/91, changed value of ra_expr if an & is seen
		 * to 20, if ra_expr was 0 and 21 if ra_expr was 1 to be
		 * able to restore the old value of ra_expr
		 */
		    {
		    if (ra_expr == 21)
			ra_expr = 1;
		    else if (ra_expr == 20)
			ra_expr = 0;

		    csymbol->Sinit = 1;
		    }
#endif
		return token = Q_IDENT;
	    }

    if (*cp == SPC_IDQUOT && clevkcc)
	{
	if (!spcident(ident, cp, sizeof(ident)-1))
	    return zerotok();
	cp = ident;
	}
    else
	int_error("trident: cursym 0 for \"%s\"", cp);

    /* If no symbol already exists for identifier, find or get one.
    ** This will only happen when creating a symbol for a quoted identifier
    ** (which cannot be a macro), or recovering from an internal error.
    ** If a symbol is made, it will have class SC_UNDEF.
    ** symfind() will complain if the identifier was truncated.
    */
    csymbol = symfind(cp, 1);	/* Find sym or make one */
    return token = Q_IDENT;
}

/* SPCIDENT(to, frm, cnt) - Get quoted identifier; special KCC extension.
**	First char of "frm" string is '`'.
*/
static int
spcident(char * to, char * frm, int cnt)
{
    register int c;
    register char *s = to;

    *s = SPC_IDQUOT;		/* Start sym with special char */
    for(;;)
	{
	switch (c = *++frm)	/* Loop over input chars */
	    {
	    case '`':		/* Terminator? */
		if (!*++frm)	/* Yes, string must stop now! */
		    break;	/* Won! */
		/* Drop thru to flag as error */
	    /* FALLTHROUGH */
	    case 0:
		int_error("spcident: Bad string for %s %Q", to, token);
		return 0;		/* Leave loop */

	    case '\\':
		c = cchar(&frm);	/* Get escaped char */
		--frm;			/* Back up so ++ gets next */
					/* and drop thru to default */
	    /* FALLTHROUGH */
	    default:
		if (c == UNDERSCORE_MAPCHR) /* Check symbol chars */
		    c = '_';
		if (!iscsym(c) && (c != '$') && (c != '%') && (c != '.'))
		    warn("Bad PDP10 symbol char: '%c'", c);
		if (--cnt > 0)
		    *++s = c;		/* add to ident. */
		continue;		/* and continue loop */
	    }
	break;				/* Leave loop */
	}

    *++s = '\0';			/* null terminate */
    if (!to[1])
	{
	error("Quoted identifier is null");
	return 0;		/* Say no token */
	}
    return 1;
}


static int
attralignval(char *cp)
{
    int base, c, d, v;

    if (cp == NULL || *cp == '\0')
	return 0;
    base = 10;
    if (*cp == '0')
	{
	++cp;
	if (*cp == 'x' || *cp == 'X')
	    {
	    base = 16;
	    ++cp;
	    }
	else
	    base = 8;
	}
    v = 0;
    while ((c = (unsigned char)*cp) != '\0')
	{
	if (c >= '0' && c <= '9')
	    d = c - '0';
	else if (c >= 'a' && c <= 'f')
	    d = c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
	    d = c - 'A' + 10;
	else
	    break;
	if (d >= base)
	    break;
	v = v * base + d;
	if (v > 4)
	    return v;
	++cp;
	}
    return v;
}

static int
gnuattrname(char *name, char *plain)
{
    size_t n;

    if (!strcmp(name, plain))
        return 1;
    n = strlen(plain);
    return name[0] == '_' && name[1] == '_'
        && !strncmp(name + 2, plain, n)
        && name[n + 2] == '_' && name[n + 3] == '_'
        && name[n + 4] == '\0';
}

static int
skipgnuattr(void)
{
    int t, depth, inaligned;
    int align;
    char *name;

    do
	t = nextpp();
    while (t == T_WSP || t == T_EOL);

    if (t != T_LPAREN)
	{
	pushpp();
	return 0;
	}

    depth = 1;
    inaligned = 0;
    while (depth > 0)
	{
	t = nextpp();
	if (t == T_EOF)
	    return 0;
	if (t == T_LPAREN)
	    ++depth;
	else if (t == T_RPAREN)
	    {
	    --depth;
	    if (depth < 3)
		inaligned = 0;
	    }
	else if (t == T_COMMA && depth == 2)
	    inaligned = 0;
	else if (t == T_IDENT && depth == 2 && (name = curval.cp) != NULL)
	    {
	    inaligned = 0;
	    if (gnuattrname(name, "noreturn"))
		gnuattr_pending |= SF_NORETURN;
	    else if (gnuattrname(name, "noinline"))
		gnuattr_pending |= SF_NOINLINE;
	    else if (gnuattrname(name, "packed"))
		gnuattr_pending |= SF_PACKED;
	    else if (gnuattrname(name, "aligned"))
		{
		/* GCC defaults to maximum useful target alignment when the
		** argument is omitted.  PDP-10 GCC caps object alignment at
		** one 36-bit word (four 9-bit C address units).
		*/
		gnuattr_pending &= ~SF_ALIGN2;
		gnuattr_pending |= SF_ALIGN4;
		inaligned = 1;
		}
	    else if (gnuattrname(name, "unused")
	          || gnuattrname(name, "deprecated")
	          || gnuattrname(name, "format")
	          || gnuattrname(name, "format_arg")
	          || gnuattrname(name, "warn_unused_result")
	          || gnuattrname(name, "pure")
	          || gnuattrname(name, "const")
	          || gnuattrname(name, "always_inline")
	          || gnuattrname(name, "hot")
	          || gnuattrname(name, "cold")
	          || gnuattrname(name, "malloc"))
		;                       /* Correctness-neutral metadata/optimization. */
	    else if (gnuattrname(name, "weak")
	          || gnuattrname(name, "alias")
	          || gnuattrname(name, "section")
	          || gnuattrname(name, "mode")
	          || gnuattrname(name, "visibility")
	          || gnuattrname(name, "constructor")
	          || gnuattrname(name, "destructor")
	          || gnuattrname(name, "used")
	          || gnuattrname(name, "common")
	          || gnuattrname(name, "nocommon")
	          || gnuattrname(name, "dllimport")
	          || gnuattrname(name, "dllexport"))
		error("GNU attribute %s is not supported", name);
	    else
		warn("Unknown GNU attribute %s ignored", name);
	    }
	else if (t == T_ICONST && depth == 3 && inaligned
	      && curval.cp != NULL)
	    {
	    align = attralignval(curval.cp);
	    if (align > 0)
		{
		gnuattr_pending &= ~(SF_ALIGN2 | SF_ALIGN4);
		if (align == 2)
		    gnuattr_pending |= SF_ALIGN2;
		else if (align > 2)
		    gnuattr_pending |= SF_ALIGN4;
		}
	    }
	}
    return 1;
}

long
lex_take_gnuattrs(void)
{
    long flags = gnuattr_pending;
    gnuattr_pending = 0;
    return flags;
}

static int
skipcxxattr(void)
{
    int t, last;

    t = nextpp();
    if (t != T_LBRACK)
	{
	pushpp();
	return 0;
	}

    last = 0;
    for (;;)
	{
	t = nextpp();
	if (t == T_EOF)
	    {
	    error("Unterminated [[attribute]]");
	    return 1;
	    }
	if (last == T_RBRACK && t == T_RBRACK)
	    return 1;
	last = t;
	}
}

static char *
stripintseps(char * src, char * buf, int siz)
{
    char *d = buf;

    while (*src && siz > 1)
	{
	if (*src != '\'')
	    {
	    *d++ = *src;
	    --siz;
	    }
	++src;
	}
    *d = '\0';
    if (*src)
	error("Integer constant too long");
    return buf;
}

static int
zerotok(void)
{
    constant.ctype = inttype;
    constant.cvalue = 0;
    constant.cwide = 0;
    constant.chi = 0;
    return token = T_ICONST;
}

static int
strctok(char * s)
{
    if (savelits++ == 0)
	slcreset();
    constant.ctype = strcontype;
    constant.csptr = slcbeg();
    while (*s)
	(void)slcput(*s++);
    if ((constant.cslen = slcend()) < 0)
	{
	error("Too many string literal chars, internal overflow");
	return szerotok();
	}
    return token = T_SCONST;
}

#define WD36MASK ((unsigned long long)0777777777777ULL)
#define DIMODE_LO35MASK ((unsigned long long)0377777777777ULL)

#ifdef __COMPILER_KCC__
static void
widefromull(unsigned long long acc, INT *hi, INT *lo)
{
    constant.cwide = 1;
    *lo = (INT)(acc & DIMODE_LO35MASK);
    *hi = (INT)((acc >> 35) & WD36MASK);
    constant.cvalue = *lo;
    constant.chi = *hi;
}

static unsigned long long
parsewide_decimal(char *cp)
{
    unsigned long long acc = 0;

    while (isdigit(*cp))
        {
        acc = acc * 10ULL + (unsigned long long)(*cp - '0');
        cp++;
        }
    return acc;
}

static unsigned long long
parsewide_hex(char *cp)
{
    unsigned long long acc = 0;
    int c;

    while (isxdigit(c = *cp))
        {
        acc = (acc << 4) + (unsigned long long)toint((char)c);
        cp++;
        }
    return acc;
}

static unsigned long long
parsewide_binary(char *cp)
{
    unsigned long long acc = 0;

    while (*cp == '0' || *cp == '1')
        {
        acc = (acc << 1) + (unsigned long long)(*cp - '0');
        cp++;
        }
    return acc;
}
#else
/*
** Accumulate a target 71-bit integer without requiring a host integer wider
** than 64 bits.  The low target word has 35 value bits and the high word 36.
** Bases accepted by C integer tokens are at most 16, so each individual
** multiply fits comfortably in hosted unsigned INT.
*/
static int
wideaccum(INT *hip, INT *lop, int base, int digit)
{
    unsigned INT hi, lo, prod, carry;

    hi = (unsigned INT)*hip;
    lo = (unsigned INT)*lop;
    prod = lo * (unsigned INT)base + (unsigned INT)digit;
    carry = prod >> 35;
    lo = prod & (unsigned INT)DIMODE_LO35MASK;
    prod = hi * (unsigned INT)base + carry;
    if (prod > (unsigned INT)WD36MASK)
        return 0;
    *hip = (INT)prod;
    *lop = (INT)lo;
    return 1;
}

static int
parsewide_words(char *cp, int base, INT *hip, INT *lop)
{
    INT hi, lo;
    int c, digit, ok;

    hi = lo = 0;
    ok = 1;
    while ((c = *cp) != 0)
        {
        if (base == 16)
            {
            if (!isxdigit(c))
                break;
            digit = toint((char)c);
            }
        else
            {
            if (!isdigit(c))
                break;
            digit = c - '0';
            if (digit >= base)
                break;
            }
        if (!wideaccum(&hi, &lo, base, digit))
            ok = 0;
        cp++;
        }
    *hip = hi;
    *lop = lo;
    return ok;
}
#endif

static int
dzerotok(void)
{
    constant.ctype = dbltype;
    constant.Cdouble = 0.0;
    return token = T_FCONST;
}

/* TRINTCON() - Transform PP-number integer constant
*/
#define SIGN ((unsigned INT)1<<(TGSIZ_LONG-1))
#define MAXPOSLONG ((INT)((~(unsigned INT)0)>>1))

static int
trintcon(void)
{
    register char *cp;
    register int c;
    register INT v = 0;
    int ovfl = 0;
    char sepbuf[256];
    char *numstart;

    if ((cp = curval.cp) == 0)
	{
	int_error("trintcon: no str");
	return zerotok();
	}
    cp = stripintseps(cp, sepbuf, sizeof(sepbuf));
    numstart = cp;

    if ((c = *cp) == '0')		/* Octal/Hex prefix? */
	{
	c = *++cp;
	if (c == 'x' || c == 'X')	/* Hex (base 16) */
	    {
	    if (isxdigit(c = *++cp))  /* must have at least one hex digit */
		{
		v = toint((char) c);			// FW KCC-NT
		while (isxdigit(c = *++cp))
		    {
		    if (v & ((unsigned INT)017 << (TGSIZ_LONG-4)))
			ovfl++;
		    v = ((unsigned INT)v << 4) + toint((char) c); // FW KCC-NT
		    }
		}
	    else
		error("Illegal hex const %s", curval.cp);
	    }
	else if (c == 'b' || c == 'B')	/* Binary (base 2), C23/GNU */
	    {
	    c = *++cp;
	    if (c == '0' || c == '1')
		{
		v = c - '0';
		while ((c = *++cp) == '0' || c == '1')
		    {
		    if (v & ((unsigned INT)01 << (TGSIZ_LONG-1)))
			ovfl++;
		    v = ((unsigned INT)v << 1) + c - '0';
		    }
		}
	    else
		error("Illegal binary const %s", curval.cp);
	    if (isdigit(c))
		{
		error("Binary constant cannot have digits other than 0 or 1");
		return zerotok();
		}
	    }
	else			/* Octal (base 8) */
	    {
	    while (isodigit(c))
		{
		if (v & ((unsigned INT)07 << (TGSIZ_LONG-3)))
		    ovfl++;
		v = ((unsigned INT)v << 3) + c - '0';
		c = *++cp;
		}
	    if (isdigit(c))		/* Helpful msg for common error */
		{
		error("Octal constant cannot have '8' or '9'");
		return zerotok();
		}
	    }
	constant.ctype = (v&SIGN) ? uinttype : inttype;	/* Set right type */
	}
    else				/* Decimal (base 10) */
	{
	v = c - '0';
	while (isdigit(c = *++cp))
	    {
	    if (v < ((MAXPOSLONG-9)/10))
		v = v*10 + c - '0';	/* Can't overflow, do it fast */
	    else			/* Slow unsigned multiply loop */
		{
		unsigned INT pv, uv = v;
		do
		    {
		    pv = uv;			/* Remember prev value */
		    uv = uv*10 + c - '0';
		    if (uv/10 != pv)
			++ovfl;	/* If cannot recover, ovflw */
		    }
		while (isdigit(c = *++cp))
		    ;
		v = uv;
		break;
		}
	    }
	constant.ctype = (v&SIGN) ? ulongtype:inttype;	/* Set right type */
	}

    /* Fix up result by checking suffixes and deciding type to use.
    ** Must use first of the types that can represent the value:
    ** Decimal:	int, long, ulong
    ** Oct/Hex:	int, uint, long, ulong
    **  U     :	uint, ulong
    **	L     : long, ulong
    **  UL    : ulong
    **
    ** Since for the PDP-10 int and long are the same size, this basically
    ** just amounts to deciding whether signed or unsigned is appropriate.
    **	If sign bit set, unsigned type can hold value.
    **	If overflow is set, no type can hold value, use largest.
    *
    *  And now, specifically to please the Plum Hall validation suite:
    *  Recognize and complain when an integer constant is suffixed with
    *  a floating constant suffix (as opposed to a random character).
    *				  MVS, CSI, 6/27/90
    */

    constant.cwide = 0;
    constant.chi = 0;

    if (c)
	{
	if ((c = toupper((char) c)) == 'L')	// FW KCC-NT
	    {
	    if (!*++cp)
		constant.ctype = (ovfl||(v&SIGN)) ? ulongtype:longtype;
	    else if (toupper(*cp) == 'L')
		{
		++cp;
		if (!*cp)
		    constant.ctype = (ovfl||(v&SIGN)) ? ulonglongtype:longlongtype;
		else if (toupper(*cp) == 'U' && !*++cp)
		    constant.ctype = ulonglongtype;
		else
		    c = -1;
		}
	    else if (toupper(*cp++) == 'U')
		constant.ctype = ulongtype;
	    else
		c = -1;		/* Bad */
	    }
	else if (c == 'U')
	    {
	    if (!*++cp)
		constant.ctype = (ovfl) ? ulongtype : uinttype;
	    else if (toupper(*cp) == 'L')
		{
		++cp;
		if (!*cp)
		    constant.ctype = (ovfl) ? ulongtype : longtype;
		else if (toupper(*cp) == 'L' && !*++cp)
		    constant.ctype = ulonglongtype;
		else
		    c = -1;
		}
	    else
		c = -1;		/* Bad */
	    }
	else if (c == 'F')
	    {
	    error("Invalid floating point constant");
	    constant.cvalue = v;
	    return token = T_FCONST;
	    }
	else
	    c = -1;			/* Bad */

	if (c < 0 || *cp)		/* Bad if flag set or anything left */
	    error("Bad integer constant suffix");
	}

    if (constant.ctype == longlongtype || constant.ctype == ulonglongtype)
	{
	char *start = numstart;
#ifdef __COMPILER_KCC__
	unsigned long long acc;

	if (*start == '0' && (start[1] == 'x' || start[1] == 'X'))
	    acc = parsewide_hex(start + 2);
	else if (*start == '0' && (start[1] == 'b' || start[1] == 'B'))
	    acc = parsewide_binary(start + 2);
	else if (*start == '0')
	    {
	    unsigned long long uv = 0;
	    char *dp = start;

	    while (isodigit(*dp))
		uv = (uv << 3) + (unsigned long long)(*dp++ - '0');
	    acc = uv;
	    }
	else
	    acc = parsewide_decimal(start);
	widefromull(acc, &constant.chi, &constant.cvalue);
#else
	{
	INT hi, lo;
	int base, ok;

	if (*start == '0' && (start[1] == 'x' || start[1] == 'X'))
	    { base = 16; start += 2; }
	else if (*start == '0' && (start[1] == 'b' || start[1] == 'B'))
	    { base = 2; start += 2; }
	else if (*start == '0')
	    base = 8;
	else
	    base = 10;
	ok = parsewide_words(start, base, &hi, &lo);
	if (!ok)
	    error("Integer constant overflow");
	constant.cwide = 1;
	constant.chi = hi & (INT)WD36MASK;
	constant.cvalue = lo & (INT)DIMODE_LO35MASK;
	}
#endif
	}
    else
	{
	if (ovfl)
	    {
	    error("Integer constant overflow");
	    constant.ctype = ulongtype;
	    }
	constant.cvalue = v;
	}
    return token = T_ICONST;
}

/* TRFLTCON() - Transform floating-point PP-number constant
*/
#ifdef __COMPILER_KCC__
static long maxdbl[2] = {MAXPOSLONG, MAXPOSLONG};
#define MAXPOSDOUBLE (*(double *)maxdbl)	/* Native 72-bit double layout. */
#else
#include <float.h>
#define MAXPOSDOUBLE DBL_MAX
#endif

static int
trfltcon(void)
{
    register char *cp;
    register int c;
    INT exponent;
    double divisor, value = 0;		/* accumulated value */
    int expsign, ovfl = 0;

    /* Internal checks to verify token is correct */
    if ((cp = curval.cp) == 0 || (!isdigit(c = *cp) && c != '.'))
	{
	int_error("trfltcon: bad str");
	return dzerotok();
	}
	
    /* C99 hexadecimal floating constants use a binary exponent. */
    if (c == '0' && (cp[1] == 'x' || cp[1] == 'X'))
	{
	if (!CSTD_HAS(CSTD_C99)) {
	    error("Hexadecimal floating constants require C99");
	    return dzerotok();
	}
	int ndig = 0;

	cp += 2;
	c = *cp;
	while (isxdigit(c))
	    {
	    value = value * 16.0 + toint((char)c);
	    ++ndig;
	    c = *++cp;
	    }
	if (c == '.')
	    {
	    divisor = 1.0;
	    while (isxdigit(c = *++cp))
		{
		value += toint((char)c) / (divisor *= 16.0);
		++ndig;
		}
	    }
	if (!ndig)
	    {
	    error("Bad hexadecimal floating constant");
	    return dzerotok();
	    }
	if (c != 'p' && c != 'P')
	    {
	    error("Hexadecimal floating constant requires binary exponent");
	    return dzerotok();
	    }
	expsign = (c = *++cp);
	if (c == '-' || c == '+')
	    c = *++cp;
	if (!isdigit(c))
	    {
	    error("Bad floating constant exponent");
	    return dzerotok();
	    }
	exponent = c - '0';
	while (isdigit(c = *++cp))
	    {
	    if (exponent > 10000)
		ovfl = 1;
	    else
		exponent = exponent * 10 + (c - '0');
	    }
	if (value == 0.0)
	    {
	    ovfl = 0;
	    exponent = 0;
	    }
	if (!ovfl)
	    {
	    double pv;
	    if (expsign == '-')
		while (--exponent >= 0)
		    {
		    pv = value;
		    value /= 2.0;
		    if (pv != 0.0 && value == 0.0)
			{ ovfl = 1; break; }
		    }
	    else
		while (--exponent >= 0)
		    {
		    pv = value;
		    if (value > MAXPOSDOUBLE / 2.0)
			{ ovfl = 1; value = 1.0; break; }
		    value *= 2.0;
		    if (value != 0.0 && value < pv)
			{ ovfl = 1; value = 1.0; break; }
		    }
	    }
	else
	    value = (expsign == '-') ? 0.0 : 1.0;
	goto fltdone;
	}

    /* First do whole-number part.  We use floating arithmetic to avoid
    ** the real possibility of integer overflow.  Slower, but safer.
    */
    for (; isdigit(c); c = *++cp)
	{
	value = (value*10.0) + (c-'0');
	if (value && value < 1.0)	/* If exponent wrapped around, */
	    ovfl++;			/* we overflowed. */
	}

    /* Now do fractional part if one was specified */
    if (c == '.')
	{
	divisor = 1.0;		/* Place-value for post-. digits */
	while (isdigit(*++cp))
	    value += (*cp - '0') / (divisor *= 10.0);
	c = *cp;
	}
    
    /* Now exponent, if any */
    if (c == 'E' || c == 'e')
	{
	expsign = (c = *++cp);	/* Get possible exponent sign */
	if (c == '-' || c == '+')
	    c = *++cp;
	if (!isdigit(c))
	    {
	    error("Bad floating constant exponent");
	    return dzerotok();
	    }
	exponent = c - '0';
	while (isdigit(c = *++cp))
	    {
	    exponent = exponent*10 + (c-'0');
	    if (exponent >= ((MAXPOSLONG-9)/10))
		ovfl++, value = (expsign=='-' ? 0.0 : 1.0);
	    }

	/* EXTREMELY dumb method of scaling value by exponent */
	/* Fix this up later!! */
	if (!ovfl)
	    {
	    double pv;
	    if (expsign == '-')
		while (--exponent >= 0)
		    {
		    pv = value;			/* Remember val so can */
		    if ((value /= 10.0) > pv)	/* check for underflow */
			{
			ovfl++;
			value = 0;
			break;
			}
		    }
	    else
		while (--exponent >= 0)
		    {
		    pv = value;
		    if ((value *= 10.0) < pv)	/* Check for overflow */
			{
			ovfl++;
			value = 1.0;
			break;
			}
		    }
	    }
	}

fltdone:
    /* See whether we overflowed or not, and fix up. */
    if (ovfl)
	{
	if (value)
	    value = MAXPOSDOUBLE;
	error("Floating constant %sflow", (int) value ? "over" : "under");
	}

    /* Now check for suffix type specifier */
    if (c && toupper((char) c) == 'F')	// FW KCC-NT
	{
	constant.ctype = flttype;
	constant.Cfloat = (float) value;	// FW KCC-NT
	c = *++cp;
	}
    else if (c && toupper((char) c) == 'L') // FW KCC-NT
	{
	constant.ctype = lngdbltype;
	constant.Clngdbl = value;
	c = *++cp;
	}
    else if (c)
	{
	error("Bad floating constant suffix");
	return dzerotok();
	}
    else
	{
	constant.ctype = dbltype;	/* Set constant type to double */
	constant.Cdouble = value;	/* and set constant value */
	}
    return token = T_FCONST;
}

/* TRSTRCON() - Transform string constant
**	The only chars not allowed in a string constant are
**	newline, double-quote, and backslash.  They must be
**	entered as a character escape code.
**
**	If using ANSI parsing, two successive string constants are
**	merged into one!
*/

static
int
trstrcon (void)
{
    char*	cp;
    int		wideflg, escval;


    if (savelits++ == 0)	/* Can char pool be reset? */
	slcreset();		/* Yes, do so */

    constant.ctype = strcontype;  /* Set constant type to string const */
    constant.csptr = slcbeg ();		/* Set constant string ptr */
			    
    /* Internal checks to verify token is correct */

    if ((cp = curval.cp) == 0)
	{
	int_error("trstrcon: no str");
	return szerotok ();
	}

    if ((wideflg = *cp) == 'L')
	++cp;	/* Get wchar_t indicator if any */

    if (*cp != '"')
	{
	int_error ("trstrcon: no \"");
	return szerotok ();
	}

    for ( ; ; )
	{
	switch (*++cp)
	    {
	    case '\\':			/* Escape char */
		slcput (escval = cchar (&cp));	/* Handle and map it */
		--cp;			/* Need to ensure ++ gets next */
		if (escval & ~tgcmask)	/* Did we truncate any bits? */
		    {
		    printf ("escval = %o, tgcmask = %o\n", escval, tgcmask);
		    error ("Escape-seq value too large for char");
		    }
		continue;

	    case '"':			/* End of string? */
		if (*++cp)
		    int_error("trstrcon: trailing junk");
		if (clevel < CLEV_ANSI)	/* Check for string concatenation? */
		    break;			/* Nope, just return what we got */

	    /* Hairy stuff... must look at next token! */
		for (;;)		/* Dumb loop to flush wsp */
		    {
		    switch (nextpp())
			{
			case T_WSP:
			case T_EOL:
			    continue;
			case T_SCONST:
			    if ((cp = curval.cp) != 0	/* Paranoia token check */
				&& (*cp == wideflg)	/* Wideness must match */
				&& (*cp == '"'		/* Paranoia format check */
				 || (*cp == 'L' && *++cp == '"')))
				break;	/* Hurray, resume main loop! */
				/* Everything's been set up... */
		    /* Can't concatenate next literal, drop thru */
			/* FALLTHROUGH */
			default:
			    pushpp();		/* Push current token back */
			    cp = NULL;		/* Say we're done */
			    break;		/* Quit loop and return */
			}
		    break;		/* Stop inner loop */
		    }
		if (cp)
		    continue;	/* Resume outer loop if concating */
		break;		/* else just leave switch */

	    case '\0':
		int_error("trstrcon: no delim");
		break;
	    default:
#if SYS_CSI	/* 5/91 KCC size */
		(void) slcput(*cp);
#else
		slcput(*cp);
#endif
		continue;

	    }
	break;			/* Break from main switch is break from loop */
	}

    /* OK, now finalize string literal in pool. */
#if 0	/* 5/91 Dynamic tables */
    {
	constant.cslen = slcend();	/* slcend() flags error in slcresize() */
	constant.csptr = (char *) ((ptrdiff_t) slcptr - constant.cslen);
	printf("%d:%s\t", (int) (constant.csptr), constant.csptr);
	}
#else
    if ((constant.cslen = slcend()) < 0)
	{
	error("Too many string literal chars, internal overflow");
	return szerotok();
	}
#endif
    return token = T_SCONST;
}

#if 0	/* 5/91 Dynamic tables */
static void
slcresize()
{
    ptrdiff_t offset = slcptr - slcpool;

    ++slcsize;
    slcpool = (char *) realloc (slcpool, slcsize * DYN_SIZE * sizeof(char));
    if (slcpool == NULL)
	jerr("Out of memory for string literal pool\n");
    slcleft += DYN_SIZE * sizeof(char);
    slcocnt += DYN_SIZE * sizeof(char);
    slcptr = (char *) ((ptrdiff_t) slcpool + offset);
    if (slcpool == NULL)
	error("Too many string literal chars, internal overflow");
}
#endif

static int
szerotok(void)
{
#if 0	/* 5/91 Dynamic tables */
    constant.csptr = 0;		/* Set constant string ptr */
#else
    constant.csptr = "";		/* Set constant string ptr */
#endif
    constant.cslen = 1;
    return token = T_SCONST;
}

/* TRCHRCON() - Transform character constant.
*/

static int
trchrcon (void)
{
    char *cp;
    int wideflg;
    unsigned long val;

    /* Internal checks to verify token is correct */
    if ((cp = curval.cp) == 0)
	{
	int_error("trchrcon: No str");
	return zerotok();
	}
    if ((wideflg = *cp) == 'L')
	++cp;	/* Get wchar_t indicator if any */
    if (*cp != '\'' || !*++cp || *cp == '\'')
	{
	int_error("trchrcon: Bad fmt");
	return zerotok();
	}

    val = 0;
    for (;;)
	{
	if (val & ~(((unsigned long)1 << (TGSIZ_INT-TGSIZ_CHAR)) - 1))
	    error("Character constant overflow");
	val <<= TGSIZ_CHAR;
	val |= cchar(&cp) & ((1<<TGSIZ_CHAR)-1);	/* Put into word */
	if (*cp == '\'')		/* Most common case, just one char */
	    break;
	if (!*cp)
	    {
	    int_error("trchrcon: Bad fmt");
	    return zerotok();
	    }
	}

    constant.ctype = (wideflg == 'L')
			? chartype	/* Wide char const is special type */
			: inttype;	/* Normal char const is type int */

    constant.cvalue = val;
    return T_CCONST;
}

/* CCHAR(&cp) - parse a character from a string literal, char constant,
**	or quoted identifier.
**	Handles escape sequences, and converts values into target char set.
**	Input starts at 1st char of pointer, leaves pointer at first char
**	not translated into resulting value.
*/

static
int
cchar (char **acp)
    {
    char*	cp = *acp;
    int		c = *cp;


    if (c == '\\')
	{
	switch (*++cp)	/* If escape char, handle it */
	    {
	    case 'a':
		c = 07;
		break;	/* ANSI alert - map into BEL */

	    case 'b':
		c = '\b';
		break;

	    case 'f':
		c = '\f';
		break;

	    case 'n':
		c = '\n';
		break;

	    case 'r':
		c = '\r';
		break;

	    case 't':
		c = '\t';
		break;

	    case 'v':
		c = '\v';
		break;

	    case '\'':
		c = '\'';
		break;

	    case '"':
		c = '\"';
		break;

	    case '\\':
		c = '\\';
		break;

	    case '?':
		c = '?';
		break;	/* To avoid trigraphs */

	    case 'x':	/* Hexadecimal escape sequence */

		if (isxdigit(*++cp))
		    {
		    int		ovfl = 0;


		    c = toint (*cp);

		    while (isxdigit (*++cp))
			{
			if (c & ((unsigned INT)017 << (TGSIZ_INT - 4)))
			    ovfl++;

			c = ((unsigned) c << 4) + toint (*cp);
			}

		    if (ovfl)
			error ("Hex constant overflow");
		    }
		else
		    error ("Need hex digit after \\x");

		*acp = cp;
		return c;		/* Specific hex value */

	    case '0':
	    case '1':
	    case '2':
	    case '3':		/* octal escape */
	    case '4':
	    case '5':
	    case '6':
	    case '7':
		c = *cp - '0';

		if (isodigit (*++cp))
		    {
		    c = ((unsigned) c << 3) + *cp - '0';

		    if (isodigit (*++cp))
			{
			c = ((unsigned) c << 3) + *cp - '0';
			++cp;
			}
		    }

		*acp = cp;

		if (c > 255)
		    {
		    if (!clevkcc)
			error ("Char const exceeds 8 bits");
		    else
			warn ("Non-portable: char const exceeds 8 bits");
		    }

		return c;		/* Specific octal value */

	    case '`':
		if (clevkcc)
		    {
		    c = '`';
		    break;
		    }

		/* Else not doing KCC extensions, drop through to complain. */

	    /* FALLTHROUGH */
	    default:
		error ("Unknown escape char (ignoring backslash): '\\%c'",*cp);
	    }
	}

    *acp = ++cp;
    return c;
    }
