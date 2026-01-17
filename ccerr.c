/*	CCERR.C - Error Handling
**
**	(c) Copyright Ken Harrenstien 1989
**		All changes after v.57, 15-Apr-1988
**	(c) Copyright Ken Harrenstien, SRI International 1985, 1986
**		All changes after v.23, 8-Aug-1985
**
**	Original version (C) 1981  K. Chen
*/

#include "cc.h"
#include "cclex.h"	/* For access to error line buffer */
#include "ccchar.h"	/* For isprint() */
#include <stdarg.h>	/* For var args */
#include <stdlib.h>	/* calloc(), EXIT_SUCCESS, EXIT_FAILURE */
#include <string.h>

#ifndef va_copy
# ifdef __va_copy
#  define va_copy(dst, src) __va_copy((dst), (src))
# else
#  define va_copy(dst, src) memcpy(&(dst), &(src), sizeof(va_list))
# endif
#endif

/* Exported functions declared in ccerr.h */
void errfopen (char *desc, char *fnam);
int expect (int t);
void note (char *fmt, ...);
void advise (char *fmt, ...);
void warn (char *fmt, ...);
void int_warn (char *fmt, ...);
void error (char *fmt, ...);
void int_error (char *fmt, ...);
void efatal (char *fmt, ...);

/* Imported functions */
extern char *estrcpy(char *, char *);	/* CCASMB for string hacking */
extern int nextoken(void);		/* CCLEX */

#define MAX_ERRORS 50

/* Internal functions */
static char *errmak(char *fmt, va_list ap);
static void buf_errmsg(char *);
static void context(char *etype, char *fmt, va_list ap);
static void ectran(char *to, char *from, int cnt);
static int evsprintf(char *cp, char *fmt, va_list *aap);
static char *tokname(int tok);
static int edefarg(char *cp, char **afmt, va_list *aap);
static void recover(int n);
static char *errputc(char *, int);
static char *errputs(char *, char *);
static char *errputul(char *, unsigned long, int, int);
static char *errputsl(char *, long);
static char *errpad(char *, int, int);
static int errfmtstr(char *, char *, int, int, int);
static int errfmtnum(char *, unsigned long, int, int, int, int, int, int,
    int, int, int, int);

#if 0
  static char *errmak();
  static void buf_errmsg();
  static void context();
  static void ectran();
  static int evsprintf();
  static char *tokname();
  static int edefarg();
  static void recover();

  #if __STDC__
    #define PRFUN(f) f(char *fmt, ...) { va_list ap; va_start(ap, fmt);
  #else
    #define PRFUN(f) f(fmt) char *fmt; { va_list ap; va_start(ap, fmt);
  #endif
#endif


static char *
errputc(char * cp, int c)
{
    *cp++ = (char)c;
    *cp = '\0';
    return cp;
}

static char *
errputs(char * cp, char * s)
{
    if (s == NULL)
	s = "(null)";
    return estrcpy(cp, s);
}

static char *
errpad(char * cp, int c, int n)
{
    while (n-- > 0)
	*cp++ = (char)c;
    *cp = '\0';
    return cp;
}

static char *
errputul(char * cp, unsigned long val, int base, int upper)
{
    char buf[32];
    char *dig;
    int i;

    dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    i = 0;
    do {
	buf[i++] = dig[val % (unsigned long)base];
	val /= (unsigned long)base;
    } while (val != 0);

    while (--i >= 0)
	*cp++ = buf[i];
    *cp = '\0';
    return cp;
}

static char *
errputsl(char * cp, long val)
{
    unsigned long u;

    if (val < 0)
	{
	*cp++ = '-';
	u = (unsigned long)(-(val + 1)) + 1;
	}
    else
	u = (unsigned long)val;

    return errputul(cp, u, 10, 0);
}

static int
errfmtstr(char * cp, char * s, int width, int prec, int left)
{
    char *start;
    int len;
    int pad;

    start = cp;
    if (s == NULL)
	s = "(null)";
    len = strlen(s);
    if (prec >= 0 && len > prec)
	len = prec;
    pad = width - len;
    if (!left)
	cp = errpad(cp, ' ', pad);
    while (len-- > 0)
	*cp++ = *s++;
    if (left)
	cp = errpad(cp, ' ', pad);
    *cp = '\0';
    return cp - start;
}

static int
errfmtnum(char * cp, unsigned long val, int neg, int base, int upper, int width, int prec, int left, int plus, int space, int alt, int zero)
{
    char num[64];
    char *start, *np;
    int nlen, zeros, pad, sign, pfx1, pfx2, total;

    start = cp;
    np = errputul(num, val, base, upper);
    nlen = np - num;
    if (prec == 0 && val == 0)
	nlen = 0;

    sign = 0;
    if (neg)
	sign = '-';
    else if (plus)
	sign = '+';
    else if (space)
	sign = ' ';

    pfx1 = pfx2 = 0;
    if (alt && base == 8 && (nlen == 0 || num[0] != '0'))
	pfx1 = '0';
    else if (alt && base == 16 && val != 0)
	{
	pfx1 = '0';
	pfx2 = upper ? 'X' : 'x';
	}

    zeros = 0;
    if (prec > nlen)
	zeros = prec - nlen;

    total = nlen + zeros + (sign != 0) + (pfx1 != 0) + (pfx2 != 0);
    if (zero && !left && prec < 0 && width > total)
	{
	zeros += width - total;
	total = width;
	}

    pad = width - total;
    if (!left)
	cp = errpad(cp, ' ', pad);

    if (sign)
	*cp++ = (char)sign;
    if (pfx1)
	*cp++ = (char)pfx1;
    if (pfx2)
	*cp++ = (char)pfx2;

    cp = errpad(cp, '0', zeros);
    np = num;
    while (nlen-- > 0)
	*cp++ = *np++;
    if (left)
	cp = errpad(cp, ' ', pad);
    *cp = '\0';
    return cp - start;
}

/* NOTE - print notification message
*/
void 
note (char *fmt, ...)
{ 
    va_list ap; 

    va_start(ap, fmt);
    if (wrnlev < WLEV_NOTE)
	{
	++nwarns;
	context("[Note] ", fmt, ap);
	}
    va_end(ap);
}

/* ADVISE - print advisory message
*/
void 
advise (char *fmt, ...)
{
    va_list ap; 
    
    va_start(ap, fmt);
    if (wrnlev < WLEV_ADVISE)
	{
	++nwarns;
	context("[Advisory] ", fmt, ap);
	}
    va_end(ap);
}

/* WARN - print warning message
*/
void 
warn (char *fmt, ...)
{
    va_list ap; 

    va_start(ap, fmt);
    if (wrnlev < WLEV_WARN)
	{
	++nwarns;
	context("[Warning] ", fmt, ap);
	}
    va_end(ap);
}

void 
int_warn (char *fmt, ...)
{
    va_list ap; 
    
    va_start(ap, fmt);
    ++nwarns;
    context("[Warning][Internal error] ", fmt, ap);
    va_end(ap);
}

/* ERROR - print error message
** INT_ERROR - same, but prefixes with "Internal error - "
*/
void 
error (char *fmt, ...)
{
    va_list ap; 

    va_start(ap, fmt);
    ++nerrors;				/* Mark this as an error */
    context("", fmt, ap);		/* Show context */
    va_end(ap);
    if (nerrors >= MAX_ERRORS)
	fatal("Too many errors");
}

void 
int_error (char *fmt, ...)
{
    va_list ap; 

    va_start(ap, fmt);
    ++nerrors;					/* Mark this as an error */
    context("[Internal error] ", fmt, ap);	/* Show context */
    va_end(ap);
    if (nerrors >= MAX_ERRORS)
	fatal("Too many errors");
}

/* EFATAL - print fatal error message, with context.
*/
void 
efatal (char *fmt, ...)
{
    va_list ap; 

    va_start(ap, fmt);
    context("[FATAL] ", fmt, ap);		/* Show context */
    va_end(ap);
    exit(EXIT_FAILURE);
}

/* JMSG - print job error message (no context).
** JWARN - print job warning message ("% ...").  FW 2A(47)
** JERR - print job error message and bump error count for current module.
** FATAL - print fatal job error message and die.
*/

static char jerrhdr[] = "? KCC";
static char jwarnhdr[] = "% KCC";	/* FW 2A(47) */

void 
jmsg (char *fmt, ...)
{
    va_list ap; 
    char *msg;


    va_start(ap, fmt);
    msg = errmak(fmt, ap);
    va_end(ap);

    if (outmsgs == stdout)
        fprintf(outmsgs, "%s - %s\n", jerrhdr, msg);

    fprintf(stderr, "%s - %s\n", jerrhdr, msg);
}

/* FW 2A(47) */
void 
jwarn (char *fmt, ...)
    {
    va_list ap; 

    
    va_start (ap, fmt);
    fprintf (outmsgs, "%s - %s\n", jwarnhdr, errmak (fmt, ap));
    va_end(ap);
    }

void 
jerr (char *fmt, ...)
{
    va_list ap; 

    
    va_start(ap, fmt);
    ++nerrors;
    fprintf(outmsgs, "%s - %s\n", jerrhdr, errmak(fmt, ap));
    va_end(ap);
}

void fatal (char *fmt, ...)
{
    va_list ap; 

    va_start(ap, fmt);
    fprintf(outmsgs, "%s - Fatal error: %s\n", jerrhdr, errmak(fmt, ap));
    va_end(ap);
    exit(EXIT_FAILURE);				/* stop program */
}

/* ERRFOPEN - Auxiliary for CC and CCOUT, invoked after failing fopen()s
*/
void
errfopen(char *desc, char *fnam)
{
    jerr("Could not open %s file \"%s\"", desc, fnam);
}

#if 0	/* 5/91 KCC size */
/* ERRNOMEM - Auxiliary invoked when a calloc fails.
*/
void
errnomem(char *s)
{
    efatal("Out of memory %s", s);
}
#endif

/* ERRMAK - Return pointer to a static buffer containing error msg text.
**	This does not contain a newline.
*/
static char *
errmak(char *fmt, va_list ap)
{
    va_list aq;
#if SYS_CSI		/* 9/91 shrink KCC */
    static char *emsgbuf = NULL;
    if (emsgbuf == NULL)
	{
	emsgbuf = (char *) calloc (1, 1024);
        if (emsgbuf == NULL)
            jerr("Out of memory for huge error message buffer\n");
	}
#else
    static char emsgbuf[2000];	/* Want lots of room to be real safe */
#endif

    va_copy(aq, ap);
    evsprintf(emsgbuf, fmt, &aq);
    va_end(aq);
    return emsgbuf;
}

#if SYS_CSI 
/* BUF_ERRMSG - buffer current error message for printing later
**		in the mixed-listing file.
**
*/
static void 
buf_errmsg (char *errmsg)
{
	if (errbuf == NULL) {
		errbuf = (char *) calloc (1, strlen (errmsg) + 1);
                if (errbuf == NULL)
                    jerr("Out of memory for error message buffer\n");
		errputs (errbuf, errmsg);
		err_waiting = 1;
		} /* if-part */
	else {
		{
		int oldlen = strlen(errbuf);
		errbuf = (char *) realloc((void *) errbuf,
			   oldlen + strlen(errmsg) + 1);
                if (errbuf == NULL)
                    jerr("Out of memory for error message realloc\n");
		errputs(errbuf + oldlen, errmsg);
		}
		err_waiting++;
	} /* else-part */
}
#endif

/* CONTEXT - print context of error
**	If "fline" is set 0 (normally never, since it starts at 1)
**	the input buffer context will not be shown.
**	This feature is used when emitting warnings after a file has been
**	completely compiled.
*/

static void
context(char *etype, char *fmt, va_list ap)
{
    char *estr;
    char *cp, *ep;
    char conbuf[ERRLSIZE*6];	/* Allow for lots of "big" chars */
#if SYS_CSI
    char errstor[ERRLSIZE*6];   /* Buffer to keep entire error msg */
    char *esp = errstor;
#endif
    int cnt, colcnt;
    int here = line;		/* Line # on page */

    estr = errmak(fmt, ap);	/* Build error message */

    if (erptr != errlin && erptr[-1] == '\n')
	--here;			/* Find right line # on current page */
				/* (KLH: but probably not worth the trouble) */

#if 1	/* New version */
    fprintf(outmsgs, "\"%s\", line %d: %s%s\n",
			inpfname, fline, etype, estr);

#if SYS_CSI
    if (mlist)
	{
	esp = errputs(esp, "; \"");
	esp = errputs(esp, inpfname);
	esp = errputs(esp, "\", line ");
	esp = errputsl(esp, (long)fline);
	esp = errputs(esp, ": ");
	esp = errputs(esp, etype);
	esp = errputs(esp, estr);
	esp = errputs(esp, "\n; ");
	esp = errstor;
	}
#endif
		
    /* Someday may wish to make further context optional (runtime switch) */
    if (!fline || 0) return;	/* Omit buffer context if at EOF */

    cp = estrcpy(conbuf, "       (");	/* Indented by 6 */
    if (curfn != NULL) {		/* are we in some function? */
	cp = estrcpy(cp, curfn->Sname);	/* yes, give its name */
	if (fline > curfnloc) {
	    cp = errputc(cp, '+');
	    cp = errputsl(cp, (long)(fline - curfnloc));
	}
	cp = estrcpy(cp, ", ");		/* separate from page/line info */
    }

    cp = errputs(cp, "p.");
    cp = errputsl(cp, (long)page);
    cp = errputs(cp, " l.");
    cp = errputsl(cp, (long)here);
    cp = errputs(cp, "): ");
    colcnt = strlen(conbuf);	/* # cols so far */
    fputs(conbuf, outmsgs);
#if SYS_CSI
    if (mlist)
	esp = errputs(errstor + strlen(errstor), conbuf);
#endif

    /* Show current input context */
#if 1
    /* Unroll circular buffer */
    if (!ercsiz) ercsiz = 79;	/* Set default if needed (# cols of context) */
    colcnt = ercsiz - colcnt;	/* Get # columns available for input context */
    cp = conbuf;
    ep = erptr;
    cnt = erpleft;
    while (*ep == 0 && --cnt > 0) ++ep;	/* Scan to find first non-null */
    if (cnt > 0) {
	ectran(conbuf, ep, cnt);	/* Translate cnt chars from ep to buf*/
	ectran(conbuf+strlen(conbuf),	/* then initial part */
		errlin, ERRLSIZE - erpleft);
    } else {
	ep = errlin;
	cnt = ERRLSIZE - erpleft;
	while (*ep == 0 && --cnt > 0) ++ep;
	ectran(conbuf, ep, cnt);
    }
    if ((cnt = strlen(cp = conbuf)) > colcnt)
	cp += cnt - colcnt;	/* If too long, show only last N chars */

    fputs(cp, outmsgs);		/* Output the context string! */
    fputc('\n', outmsgs);
    fputc('\n', outmsgs);	/* Extra newline between msgs for clarity */
#if SYS_CSI
    if (mlist) {
	esp = errputs(errstor + strlen(errstor), cp);
	esp = errputs(errstor + strlen(errstor), "\n");
	buf_errmsg(esp);
    } /* if mlist */
#endif /* SYS_CSI */

#else
    if (erptr != errlin) *erptr = 0;	/* terminate line for printf */
    fprintf(outmsgs, "%s\n", errlin);	/* print where we were */
#endif

#else	/* Old version */
    fprintf(outmsgs, "\n%s at ", etype);	/* start error message */
    if (curfn != NULL) {		/* are we in some function? */
	fputs(curfn->Sname, outmsgs);	/* yes, give its name */
	if (fline > curfnloc) fprintf(outmsgs, "+%d", fline - curfnloc);
	fputs(", ", outmsgs);		/* separate from absolute loc */
    }
    if (page > 1) fprintf(outmsgs, "page %d ", page); /* page number */
    fprintf(outmsgs,"line %d of %s:\n", here, inpfname);
    if (erptr != errlin) *erptr = 0;	/* terminate line for printf */
    fputs(errlin, outmsgs);		/* print where we were */
#endif
}

/* ECTRAN - translate file input string to something nice for
**	error message output.  Always adds a NUL after "cnt" chars.
*/
static void
ectran(char *to, char *from, int cnt)
{
    int c;
    char *exp;
    char expbuf[8];

    while (--cnt >= 0) {
	if (isprint(c = *from++)) {
	    *to++ = c;
	    continue;
	} else switch (c) {
	case (char)-1:	exp = "<EOF>"; break;
	case '\b':	exp = "<\\b>"; break;	/* Show unusual whitespace */
	case '\f':	exp = "<\\f>"; break;
	case '\v':	exp = "<\\v>"; break;
	case '\r':	exp = "<\\r>"; break;
#if 1
	case '\t':
	case '\n':	exp = " "; break;	/* Just use whitespace */
#else
	case '\t':	exp = "<\\t>"; break;
	case '\n':	exp = "<\\n>"; break;
#endif
	default:
	    exp = expbuf;
	    exp = errputs(exp, "<\\");
	    exp = errputul(exp, (unsigned long)(unsigned char)c, 8, 0);
	    exp = errputc(exp, '>');
	    exp = expbuf;
	    break;
	}
	to = estrcpy(to, exp);
    }
    *to = '\0';		/* Ensure string ends with null. */
}

static int
evsprintf(char *cp, char *fmt, va_list *aap)
{
    int i, max;
    char *str;
    NODE *n;
    SYMBOL *s;
    int cnt = 0;
    
    for (*cp = *fmt; *cp; *++cp = *++fmt) {
	if (*cp != '%') { ++cnt; continue; }
	switch (*++fmt) {
	case '%': continue;
	case 'E':		/* Substitute new format string */
	    fmt = va_arg(*aap, char *);
	    --fmt;
	    i = 0;
	    break;
	case 'N':		/* Node op printout */
	    {
	    char *tcp = cp;
	    n = va_arg(*aap, NODE *);
	    tcp = errputs(tcp, "(node ");
	    tcp = errputsl(tcp, (long)nodeidx(n));
	    tcp = errputs(tcp, ": ");
	    tcp = errputsl(tcp, (long)n->Nop);
	    tcp = errputc(tcp, '=');
	    tcp = errputs(tcp, tokname(n->Nop));
	    tcp = errputc(tcp, ')');
	    i = tcp - cp;
	    }
	    break;
	case 'Q':		/* Token printout */
	    {
	    char *tcp = cp;
	    i = va_arg(*aap, int);
	    tcp = errputs(tcp, "(token ");
	    tcp = errputsl(tcp, (long)i);
	    tcp = errputc(tcp, '=');
	    tcp = errputs(tcp, tokname(i));
	    tcp = errputc(tcp, ')');
	    i = tcp - cp;
	    }
	    break;
	case 'S':		/* Symbol printout */
	    s = va_arg(*aap, SYMBOL *);
	    str = s->Sname;
	    i = '"';			/* Default quoting char */
	    max = IDENTSIZE-1;		/* Max ident length */
	    switch (str[0]) {
	    case SPC_IDQUOT:
		i = '`';	/* Different quote char, then drop thru */
	    /* FALLTHROUGH */
	    case SPC_SMEM:
	    case SPC_TAG:
	    case SPC_LABEL:	/* Don't show prefix char */
		--max;
		++str;
	    default:
	        ;	/* do nothing */
	    }
	    {
	    char *tcp = cp;
	    tcp = errputc(tcp, i);
	    while (max-- > 0 && *str)
		*tcp++ = *str++;
	    tcp = errputc(tcp, i);
	    i = tcp - cp;
	    }
	    break;
	default:
	    i = edefarg(cp, &fmt, aap);
	    break;
	}
	cnt += i;
	cp += i-1;
    }
    return cnt;
}

static char *
tokname(int tok)
{
    return (0 < tok && tok < NTOKDEFS) ? nopname[tok] : "??";
}

/* EDEFARG - auxiliary to handle default %-specifications.
**	Simple-mindedly copies anything that looks like a sprintf format
**	spec, and invokes sprintf on it with an appropriate argument
**	plucked from the arglist.
** Return value is # chars written, and
** FMT is updated to point at last char read.
*/
static int
edefarg(char *cp, char **afmt, va_list *aap)
/* cp Points to place to deposit, afmt Points to 1st char after % */
{
    int c;
    char *fmt = *afmt;
    int left = 0;
    int plus = 0;
    int space = 0;
    int alt = 0;
    int zero = 0;
    int width = 0;
    int prec = -1;
    int typ = 0;
    int base;
    int upper;
    int neg;
    long sl;
    unsigned long ul;
    char *s;

    for (;;)
	{
	c = *fmt;
	switch (c)
	    {
	    case '-': left = 1; ++fmt; continue;
	    case '+': plus = 1; ++fmt; continue;
	    case ' ': space = 1; ++fmt; continue;
	    case '#': alt = 1; ++fmt; continue;
	    case '0': zero = 1; ++fmt; continue;
	    }
	break;
	}

    while (*fmt >= '0' && *fmt <= '9')
	{
	width = (width * 10) + (*fmt - '0');
	++fmt;
	}

    if (*fmt == '.')
	{
	++fmt;
	prec = 0;
	while (*fmt >= '0' && *fmt <= '9')
	    {
	    prec = (prec * 10) + (*fmt - '0');
	    ++fmt;
	    }
	}

    if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L')
	{
	typ = *fmt;
	++fmt;
	}

    c = *fmt;
    *afmt = fmt;

    switch (c)
	{
	case 's':
	    s = va_arg(*aap, char *);
	    return errfmtstr(cp, s, width, prec, left);

	case 'c':
	    {
	    char tmp[2];
	    tmp[0] = (char)va_arg(*aap, int);
	    tmp[1] = '\0';
	    return errfmtstr(cp, tmp, width, prec, left);
	    }

	case 'i':
	case 'd':
	    if (typ == 'l')
		sl = va_arg(*aap, long);
	    else
		sl = (long)va_arg(*aap, int);
	    if (sl < 0)
		{
		neg = 1;
		ul = (unsigned long)(-(sl + 1)) + 1;
		}
	    else
		{
		neg = 0;
		ul = (unsigned long)sl;
		}
	    return errfmtnum(cp, ul, neg, 10, 0, width, prec, left,
		plus, space, alt, zero);

	case 'u':
	case 'o':
	case 'x':
	case 'X':
	    if (typ == 'l')
		ul = va_arg(*aap, unsigned long);
	    else
		ul = (unsigned long)va_arg(*aap, unsigned int);
	    base = (c == 'o') ? 8 : ((c == 'u') ? 10 : 16);
	    upper = (c == 'X');
	    return errfmtnum(cp, ul, 0, base, upper, width, prec, left,
		0, 0, alt, zero);

	case 'f':
	case 'e':
	case 'E':
	case 'g':
	case 'G':
	    if (typ == 'L')
		(void)va_arg(*aap, long double);
	    else
		(void)va_arg(*aap, double);
	    return errfmtstr(cp, "<float>", width, prec, left);

	default:
	    cp = errputc(cp, '%');
	    cp = errputc(cp, c);
	    return 2;
	}
}

/* ---------------------- */
/*	expect token      */
/* ---------------------- */
int
expect(int t)
{
    char *s, str[32];

    if (t == token) {
	nextoken();
	return 1;
    }
    switch (t) {
    case T_LPAREN:	s = "left parenthesis"; 	break;
    case T_RPAREN:	s = "right parenthesis"; 	break;
    case T_LBRACK:	s = "left bracket"; 		break;
    case T_RBRACK:	s = "right bracket"; 		break;
    case T_SCOLON:	s = "semicolon";		break;
    case T_COMMA:	s = "comma";			break;
    case T_COLON:	s = "colon";			break;
    case Q_IDENT:	s = "identifier";		break;
    case T_LBRACE:	s = "open brace";		break;
    case T_RBRACE:	s = "close brace";		break;
    case Q_WHILE:	s = "\"while\" keyword";	break;
    default:
	s = str;
	s = errputs(s, "[token ");
	s = errputsl(s, (long)t);
	s = errputc(s, ']');
	s = str;
	break;
    }
    error("Expected token (%s) not found", s);
    recover(t);
    return 0;
}

/* ------------------------ */
/*	error recovery      */
/* ------------------------ */
/* KLH: this is pretty poor; someday work on it. */

static void
recover(int n)
{
    if (n == T_SCOLON) {
	while (!eof &&  token != T_SCOLON && 
			token != T_RBRACE &&
			token != T_LBRACE)
	    nextoken();
	if (token == T_SCOLON) nextoken();
	return;
    }
/*  tokpush(token, csymbol); */
/*  token = n;		     */
}

#if 0	/* 5/91 KCC size */
int
errflush(void)
{
    for(;;) switch (token) {
	case T_EOF: case T_SCOLON: case T_RBRACE:
	    return nextoken();
	default:
	    nextoken();
    }
}
#endif


