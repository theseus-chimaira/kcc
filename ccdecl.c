/*      CCDECL.C - Declaration Parser
**
**      (c) Copyright Ken Harrenstien 1989
**              All changes after v.371, 28-May-1988
**      (c) Copyright Ken Harrenstien, SRI International 1985, 1986
**              All changes after v.154, 8-Aug-1985
**
**      Original version (C) 1981  K. Chen
*/

#define _DEF_CHAR8      0       /* define this to default char to char8 */

#include <limits.h>             /* for INT_MAX */
#include <stdlib.h>             /* for realloc, free */
#include "cc.h"
#include "cclex.h"
int strcmp (const char *, const char *);

extern SYMBOL *lsymhead;        /* CCSYM - var indicating loc sym blk */

/* Imported functions */
extern SYMBOL *symfidstr(char *), *creatsym(char *), *uniqsym(SYMBOL *); /* CCSYM */
extern SYMBOL *findgsym(SYMBOL *);      /* CCSYM */
extern SYMBOL *newlabel(void);  /* CCSYM */
extern SYMBOL *symfxext(SYMBOL *);      /* CCSYM */
extern SYMBOL *symftag(SYMBOL *);       /* CCSYM */
extern SYMBOL *symfmember(SYMBOL *, SYMBOL *);  /* CCSYM */
extern SYMBOL *symqcreat(SYMBOL *);     /* CCSYM */
extern SYMBOL *vla_declscope_v12(void); /* CCSTMT */
extern void vla_declregion_v12(void); /* CCSTMT */
extern SYMBOL *isdupsym(SYMBOL *);      /* CCSYM */
extern SYMBOL *beglsym(void);   /* CCSYM */
extern TYPE *findctype(int, INT, unsigned INT, TYPE *),
        *newvlatype_v11(INT, TYPE *),
        *findtype(int, TYPE *), /* CCSYM */
        *findftype(TYPE *, TYPE *),                     /* CCSYM */
        *findptype(int, TYPE *, TYPE *);                        /* CCSYM */
extern TYPE *tcomposite(TYPE *, TYPE *);                /* CCSYM */
extern int  cmptype(TYPE *, TYPE *), cmputype(TYPE *, TYPE *);  /* CCSYM */
extern INT  sizetype(TYPE *), alignoftype_v1(TYPE *); /* CCSYM */
extern void copysym(SYMBOL *, SYMBOL *), ridlsym(SYMBOL *);     /* CCSYM */
extern void mapintsym(SYMBOL *);                /* CCSYM */
extern int  mapextsym(SYMBOL *);                /* CCSYM */
extern void freesym(SYMBOL *);  /* CCSYM */
extern NODE *evalexpr(NODE *);          /* CCEVAL */
extern NODE *funstmt(void), *asgnexpr(void);    /* CCSTMT */
extern NODE *exprconst(void);           /* CCSTMT */
extern INT pconst(void);                       /* CCSTMT */
extern NODE *convasgn(TYPE *, NODE *);          /* CCTYPE */
extern NODE *convarrfn(NODE *);                    /* CCTYPE */
extern TYPE *convfparam(TYPE *);                /* CCTYPE */
extern int nextoken(void);                      /* CCLEX */
extern long lex_take_gnuattrs(void);              /* CCLEX */
extern int expect (int);                        /* CCERR */

/* Exported functions */
void initpar(void);             /* Called by CC mainline */
NODE *extdef(void);             /* Called by CC mainline */
NODE *tntdef(void);     /* Called by CC mainline */
NODE *ldecllist(void);  /* CCSTMT for inner block declarations */
TYPE *typename(void);   /* CCSTMT for cast and sizeof exprs */
SYMBOL *defauto(char *, TYPE *); /* CCSTMT for invisible struct return obj */
SYMBOL *funchk(int, int, SYMBOL *, SYMBOL *);   /* CCSTMT for calls to undeclared functions */

struct protostate               /* State block for prototype parse */
    {
    int nparams;
    SYMBOL *shead, *stail;      /* Parameter symbol list */
    SYMBOL decl;
    };

/* Internal functions */
static NODE *funcdef(SYMBOL *, SYMBOL *, SYMBOL *),
            *datadef(SYMBOL *, SYMBOL *, SYMBOL *);
static void pdecllist(void), sdeclenum(SYMBOL *), packstruct(TYPE *),
            decllist(SYMBOL *,SYMBOL *,SYMBOL *,SYMBOL *,NODE **,NODE **);
static INT  sdeclstruct(SYMBOL *, int), fldsize(int, INT *, int *);
static TYPE *pbase(SYMBOL *);
static void palignas_v5(SYMBOL *);
static INT symalign_v5(long);
static TYPE *qualarray (TYPE *type, int flags, int *oldflags);
static void staticassertdecl(void);
static TYPE *typeofspec(void);
int  isdecl(void);
static TYPE *fundecl(SYMBOL *, int),
            *paramlist(struct protostate
*),
*mkproto(SYMBOL *);
static void pidentlist(struct protostate
*);
static void plcmpare(TYPE *, TYPE *, int),
            plcmpold(SYMBOL *, TYPE *, TYPE *), plcheck(TYPE *);
static NODE *dodecl(int, SYMBOL *, SYMBOL *);
static SYMBOL *declarator(SYMBOL *);
static TYPE *addpp(TYPE *, TYPE *), *pushsztype(int, INT, INT, TYPE *),
            *vlamake_v11(NODE *, TYPE *), *vlaclone_v11(TYPE *, TYPE *),
            *fnmaybitptrtype(TYPE *), *storagemaybitptrtype_v18(TYPE *),
            *tagspec(int);
static int fnmaybitptrtarget(TYPE *);
static void vlabindtype_v11(TYPE *);
static void vlamarkfn_v11(void);
static int fnregcap_v11(void);
static struct vlaobj_v11 *vlaobjadd_v11(SYMBOL *);
void vlaobjmark_v12(SYMBOL *, SYMBOL *);
static SYMBOL *sdeclaration(SYMBOL *, SYMBOL *, INT *, int *, int *);
static SYMBOL *sdeclanon_v7(SYMBOL *, SYMBOL *, TYPE *, INT *, int *, int *);
static void anonpromote_v7(SYMBOL *, SYMBOL *, INT);
static INT anonoffadd_v7(INT, INT);
static int anoncontains_v7(TYPE *, SYMBOL *);
static SYMBOL *anonroot_v7(TYPE *, SYMBOL *);
static NODE *pizanon_v7(TYPE *, SYMBOL *);
NODE *pizer(SYMBOL *);
static void errtwotyp(SYMBOL *, SYMBOL *), errdupsym(SYMBOL *);
static TYPE *mkprox(SYMBOL *);
static int nsetjmps(void);
static NODE *piztype(TYPE *, int), *chkarith(TYPE *, NODE *, int),
            *pizstruct(TYPE *, int, int), *pizarray(TYPE *, int),
            *pizdesigvalue(TYPE *), *pizanon_v7(TYPE *, SYMBOL *),
            *pizmergeanon_v7(NODE *, NODE *, TYPE *),
            *pexizer(int), *pizlist(void);
static void pizflush(int);
static int isauto(SYMBOL *), nisconst(NODE *);
static char *declputd(char *, int);
static void Set_Register(SYMBOL *, int, int);

/* VLA metadata is deliberately kept out of TYPE and SYMBOL so ordinary
** compilation pays no structure-size penalty.  VLA TYPE objects themselves
** are non-interned (CCSYM newvlatype_v11), which makes a TYPE * a unique key
** for its runtime bound expression.
*/
struct vlainfo_v11 {
    TYPE *type;
    NODE *bound;
    SYMBOL *boundsym;
    int captured;
    struct vlainfo_v11 *next;
};

struct vlaobj_v11 {
    SYMBOL *object;
    SYMBOL *base;
    SYMBOL *mark;
    struct vlaobj_v11 *next;
};

static struct vlainfo_v11 *vlainfos_v11;
static struct vlaobj_v11 *vlaobjs_v11;
static unsigned int vlacounter_v11;

static struct vlainfo_v11 *
vlafind_v11(TYPE *t)
{
    struct vlainfo_v11 *v;
    for (v = vlainfos_v11; v != NULL; v = v->next)
        if (v->type == t)
            return v;
    return NULL;
}

static TYPE *
vlamake_v11(NODE *bound, TYPE *subt)
{
    struct vlainfo_v11 *v;
    TYPE *t;

    t = newvlatype_v11(typbsiztab[TS_ARRAY], subt);
    v = (struct vlainfo_v11 *)calloc(1, sizeof(*v));
    if (v == NULL)
        jerr("Out of memory for VLA metadata\n");
    v->type = t;
    v->bound = bound;
    v->next = vlainfos_v11;
    vlainfos_v11 = v;
    return t;
}

static TYPE *
vlaclone_v11(TYPE *old, TYPE *subt)
{
    struct vlainfo_v11 *v;
    v = vlafind_v11(old);
    if (v == NULL) {
        int_error("vlaclone_v11: missing metadata");
        return findctype(TS_ARRAY, 0, 0, subt);
    }
    return vlamake_v11(v->bound, subt);
}

static int
vlacontains_v11(TYPE *t)
{
    for (; t != NULL; t = t->Tsubt)
        if (tisvla(t))
            return 1;
        else if (t->Tspec != TS_ARRAY && t->Tspec != TS_PTR)
            break;
    return 0;
}

static char *
declputd(char * cp, int val)
{
    char buf[20];
    int i;
    unsigned int u;

    if (val < 0)
        {
        *cp++ = '-';
        u = (unsigned int)(-(val + 1)) + 1;
        }
    else
        u = (unsigned int)val;

    i = 0;
    do {
        buf[i++] = (char)('0' + (u % 10));
        u /= 10;
    } while (u != 0);

    while (--i >= 0)
        *cp++ = buf[i];
    *cp = '\0';
    return cp;
}

static int
fnregcap_v11(void)
{
    return R_PRESERVE_COUNT - (fnvla_v11 ? 1 : 0);
}

static void
vlamarkfn_v11(void)
{
    SYMBOL *s;

    if (fnvla_v11)
        return;
    fnvla_v11 = 1;

    /* AC14 is the VLA frame pointer.  If register allocation already used
    ** all five preserved ACs before the first VLA declaration, demote only
    ** the AC14 occupant.  Earlier preserved locals stay in AC10..AC13.
    */
    if (_reg_count < R_PRESERVE_COUNT)
        return;
    s = Reg_Id[_reg_count - 1];
    if (s == NULL || s->Sreg != R_MAXREG) {
        int_error("vlamarkfn_v11: AC14 register owner missing");
        return;
    }
    if (s->Sclass == SC_RAUTO) {
        s->Sclass = SC_AUTO;
        s->Sreg = 0;
        s->Svalue = maxauto;
        maxauto += sizetype(s->Stype);
    } else if (s->Sclass == SC_RARG) {
        s->Sclass = SC_ARG;
        s->Sreg = 0;
    } else {
        int_error("vlamarkfn_v11: bad AC14 symbol class %d", s->Sclass);
        return;
    }
    Reg_Id[--_reg_count] = NULL;
}

static SYMBOL *
vlahidden_v11(char *stem, TYPE *typ)
{
    char name[40];
    char *cp = name;
    char *sp = stem;

    *cp++ = SPC_IAUTO;
    while (*sp != '\0')
        *cp++ = *sp++;
    cp = declputd(cp, (int)++vlacounter_v11);
    *cp = '\0';
    return defauto(name, typ);
}

static void
vlabindtype_v11(TYPE *t)
{
    struct vlainfo_v11 *v;

    if (t == NULL)
        return;
    if (t->Tspec == TS_ARRAY) {
        if (tisvla(t)) {
            v = vlafind_v11(t);
            if (v == NULL) {
                int_error("vlabindtype_v11: missing metadata");
                return;
            }
            if (v->boundsym == NULL)
                v->boundsym = vlahidden_v11("vlab", v->bound->Ntype);
        }
        vlabindtype_v11(t->Tsubt);
    } else if (t->Tspec == TS_PTR)
        vlabindtype_v11(t->Tsubt);
}

static struct vlaobj_v11 *
vlaobjadd_v11(SYMBOL *s)
{
    struct vlaobj_v11 *v;
    TYPE *pt;

    v = (struct vlaobj_v11 *)calloc(1, sizeof(*v));
    if (v == NULL)
        jerr("Out of memory for VLA object metadata\n");
    pt = findtype(TS_PTR, s->Stype->Tsubt);
    v->object = s;
    v->base = vlahidden_v11("vlap", pt);
    v->next = vlaobjs_v11;
    vlaobjs_v11 = v;
    vlabindtype_v11(s->Stype);
    return v;
}

static NODE *
vlaparamdata_v11(SYMBOL *args)
{
    NODE *head, *tail, *d, *iz, *vz;
    SYMBOL *s;

    head = tail = NULL;
    for (s = args; s != NULL; s = s->Spmnext) {
        if (!vlacontains_v11(s->Stype))
            continue;
        vlamarkfn_v11();
        vlabindtype_v11(s->Stype);
        vz = ndeft(N_VLA, s->Stype);
        vz->Nid = s;
        iz = ndeflr(N_IZ, ndefident(s), vz);
        d = ndefl(N_DATA, iz);
        if (tail != NULL)
            tail->Nright = d;
        else
            head = d;
        tail = d;
    }
    return head;
}

SYMBOL *
vlabase_v11(SYMBOL *s)
{
    struct vlaobj_v11 *v;
    for (v = vlaobjs_v11; v != NULL; v = v->next)
        if (v->object == s)
            return v->base;
    return NULL;
}

void
vlaobjmark_v12(SYMBOL *s, SYMBOL *mark)
{
    struct vlaobj_v11 *v;
    for (v = vlaobjs_v11; v != NULL; v = v->next)
        if (v->object == s) {
            v->mark = mark;
            return;
        }
    int_error("vlaobjmark_v12: missing VLA object");
}

SYMBOL *
vlaobjmarkget_v12(SYMBOL *s)
{
    struct vlaobj_v11 *v;
    for (v = vlaobjs_v11; v != NULL; v = v->next)
        if (v->object == s) return v->mark;
    return NULL;
}

void
vlaclear_v12(void)
{
    struct vlainfo_v11 *vi, *vin;
    struct vlaobj_v11 *vo, *von;

    for (vi = vlainfos_v11; vi != NULL; vi = vin) {
        vin = vi->next;
        free(vi);
    }
    for (vo = vlaobjs_v11; vo != NULL; vo = von) {
        von = vo->next;
        free(vo);
    }
    vlainfos_v11 = NULL;
    vlaobjs_v11 = NULL;
}

NODE *
vlaboundexpr_v11(TYPE *t)
{
    struct vlainfo_v11 *v = vlafind_v11(t);
    return v ? v->bound : NULL;
}

int
vlaboundcaptured_v12(TYPE *t)
{
    struct vlainfo_v11 *v = vlafind_v11(t);
    return v ? v->captured : 0;
}

void
vlaboundsetcaptured_v12(TYPE *t)
{
    struct vlainfo_v11 *v = vlafind_v11(t);
    if (v != NULL) v->captured = 1;
}

SYMBOL *
vlaboundsym_v11(TYPE *t)
{
    struct vlainfo_v11 *v = vlafind_v11(t);
    return v ? v->boundsym : NULL;
}


/* Internal data */

static int paramok;     /* True if OK to parse parameter list for a function
                        ** definition.  Only true when at top
                        ** level and parsing the first declarator.
                        */
static int inproto;     /* True when parsing a function prototype list.
                        ** This is bumped to indicate level.
                        */

static NODE *statdecls, /* Pointer to list of static decls within current fn */
        *stattail;      /* Pointer to tail of statdecls list */
static int nsdefs;      /* # of enum/struct/union side effect definitions seen.
                        ** The exact number doesn't matter; this is only used
                        ** to tell when a type-specifier has had the side
                        ** effect of defining a tag or enum constant.
                        */
static int itags;       /* # of internal tags defined.  This is used only
                        ** to create unique names for internal tags. 
                        */
static int ianonmem_v7; /* # of hidden C11 anonymous aggregate members. */
static int tntcnt;      /* # of tentative defs output.  Only used to initialize
                        ** tntdef(), by setting to -1.
                        */
#if 0
/* Flags returned from pbase(). */
/* These aren't really used yet, maybe should be flushed. */
#define BASEF_SC 01     /* Storage class parsed */
#define BASEF_TS 02     /* Type-specifier parsed */
#define BASEF_TAG 04    /* enum/struct/union tag was defined as side effect */

/* Also not really used yet */
typedef struct
    {
    int Dflags;
    SYMBOL *Dpnext, *Dpsblk;
    SYMBOL s;
    }
declsym;
#define DSname s.Sname
#define DSvalue s.Svalue
#define DSclass s.Sclass
#define DStype s.Stype
#endif

/* INITPAR - Initialize parser
**      Called once for each file compiled.
*/
void
initpar(void)
{
    nodeinit();                 /* No parse-tree nodes used */
    curfn = NULL;               /* Not in any function yet */
    itags = 0;                  /* Reset internal tag count for gensyms */
    ianonmem_v7 = 0;             /* Reset anonymous-member gensym count */
    nsdefs = 0;                 /* Reset side-effect def count for neatness */
    tntcnt = -1;                /* Reset tntdef() */
}


/* EXTDEF() - Parse top-level declaration or definition
**      [dpANS 3.7, 3.5]
**
**      <external-decl> ::= <function-definition> | <decl>
**
**      <function-definition> ::=
**                      {decl-specs} <declor> {decl-list} <compound-stmt>
**
**      <decl> ::=      <decl-specs> {init-decl-list} ';'
**
**      <init-decl-list> ::= <init-declor> {',' init-decl-list}
**      <init-declor>   ::= <declor> {'=' izer}
**
** This is the main entry to input parser.  Current token is the
** first one of the <external-decl>.  Since both possibilities (function
** def or a decl) can start with <decl-specs>, we parse for that first.
** If there was nothing but <decl-specs>, we can just return.  Otherwise
** the initially parsed "base" needs to be passed on for further parsing.
*/

NODE*
extdef (void)
{
    SYMBOL*     s;
    SYMBOL      tempsym;
    SYMBOL      base;


    /* Do top level initializations */

    paramok = 1;                /* OK to parse a function parameter list */
    inproto = 0;                /* Not in prototype scope */
    curfnnew = fline;           /* Remember line in file where funct started*/
    _reg_count = 0;             /* Count of "preserved" registers used */
                                /* for register variables */

    if ((token == T_STATIC_ASSERT || token == T_STATIC_ASSERT2))
        {
        staticassertdecl();
        return NULL;
        }

    pbase(&base);               /* Parse base (storage class & type) */

    if (token == T_SCOLON)      /* Only got a type (struct)? */
        {
        if ((base.Sflags&SF_SIDEFF) == 0)       /* If no side effs, */
            error("Null declaration");

        switch (base.Sclass)
            {
            case SC_UNDEF:
                break;                  /* None given */
            case SC_AUTO:
            case SC_RAUTO:
                error("Illegal storage class");
                break;
            default:
                note("Useless storage class");
                break;
            }
        nextoken();             /* Skip over the semicolon */
        return NULL;            /* and stop doing this def */
        }

    /* Parsed <decl-specs>, now look for <declor> to determine whether
    ** we're doing a function def or not.
    */
    copysym(&tempsym, &base);           /* Copy base storage class & type */
    if (tempsym.Sclass == SC_UNDEF)     /* Set up for defaults */
        tempsym.Sclass = SC_EXLINK;     /* Default stg class: extern */
    if (tempsym.Stype == NULL)
        tempsym.Stype = deftype;        /* Default type: int */
    while ((s = declarator(&tempsym)) == NULL)/* Until we find declarator */
        /* No declarator found, may want to loop. */
        {
        error("Null declarator, expecting ident");
        if (tempsym.Spmnext)            /* Flush param syms if any (in case)*/
            ridlsym((SYMBOL *)NULL);
        if (token == T_COMMA)           /* Only continue if hit comma */
            {
            nextoken();                 /* in which case skip over it */
            continue;                   /* and try again */
            }

        if (token != T_SCOLON)          /* Else give up on it */
            error("Bad syntax for top-level declaration");

        /* 9/91 infinite loop if test for T_RBRACE also (SPR 9579) */
        while (token != T_EOF && token != T_SCOLON)
            nextoken();         /* Flush to probable end of stmt */
        return NULL;
        }

    paramok = 0;                /* No longer OK to parse function param list */

    /* If function type, need to examine more closely to see whether this
    ** is a definition or just a forward-reference declaration.
    ** It is a definition only if the current token is one of:
    **          Left brace (start of function body)
    **          A type-specifier keyword (a function parameter declaration).
    **          The "register" keyword (only permissible storage class for
    **                  function parameter declarations).
    ** We permit any storage class here, for better error diagnostics later.
    */

    if (tempsym.Stype->Tspec == TS_FUNCT)
        {
        if (token == T_LBRACE || isdecl())
            return funcdef(&base, &tempsym, s);  /* Parse funct def */
        }

    /* Not a function definition, so is either a function reference, or a
    ** data definition/reference.
    */
    return datadef(&base, &tempsym, s); /* data def/ref or function ref */
}

/* TNTDEF - Return next remaining tentative definition, if any.
**      This is only invoked by the CC mainline after all input parsing
**      is completed for a file.  It scans for all tentative definitions
**      with internal or external linkage, and for each one returns a
**      node that defines it with a zero initializer.
*/
NODE *
tntdef(void)
{
    static SYMBOL *s;

    if (++tntcnt == 0)          /* If first time, */
        s = symbol;             /* start scanning global sym list */
    while ((s = s->Snext) != NULL)
        {
        switch (s->Sclass)
            {
            default:
                continue;
            case SC_INTREF:             /* Should only exist for functions */
                if (s->Srefs)
                    error("Static function %S not defined", s);
                else
                    note("Static function %S not defined or used", s);
                continue;
            case SC_INTDEF:             /* May be function or object */
                if (s->Srefs == 0)
                    note("Static %s %S never used",
                            (s->Stype->Tspec != TS_FUNCT ? "object" : "function"),
                            s);
                continue;
            case SC_INLINK:
                s->Sclass = SC_INTDEF;
                if (s->Srefs)   /* Ensure internal object was used */
                    break;              /* Yup, go define it */
                note("Static object %S never used, not emitted", s);
                continue;
            case SC_EXLINK:
                s->Sclass = SC_EXTDEF;  /* Always do it if external linkage */
                break;
            }
        /* Set up a Q_IDENT and a null izer definition for it */
        return ndefl(N_DATA, ndefl(N_IZ, ndefident(s)));
        }
    return NULL;
}

/* FUNCDEF() - Parse function definition
**      [dpANS 3.7.1]
**      <function-definition> ::=
**                      {decl-specs} <declor> {decl-list} <compound-stmt>
**
** Only called from extdef() when a function declaration turns out
** to be the start of a definition.  At this point, the current token
** is the first one past the <declor>.
**
** The first two pointer args always point to temporary symbol
** structures not in the symbol table.  However, the parameter list
** symbols ARE in the table, chained as local symbols.
*/

/* PROMOTE_ABI_PARAMS - Keep a simple register-only parameter list off stack.
**
** KCC's expression allocator uses AC1..AC7, so incoming ABI arguments cannot
** remain indefinitely in AC1..AC4.  For functions whose complete named
** parameter list consists of at most four one-word arguments, move those
** arguments directly into KCC's existing call-preserved register-variable
** ACs.  This avoids constructing the historical private parameter image.
**
** Address-taken parameters remain stack based.  Track that property while
** parsing the address operator instead of walking KCC's non-uniform parse
** tree after the fact.  Addresses of unrelated local variables do not need
** to disable register-resident parameters.
*/
/* FN_ABI_LEAF - Conservatively recognize a call-free function body.
**
** This is intentionally bounded: no CFG and no recursive walk.  Unknown
** nodes, inline assembly, calls, an overfull work stack, or more than 4096
** visited nodes all disable the optimization.
*/
static int
fn_abi_leaf(NODE *root, int *has_query)
{
    NODE *work[128], *n;
    NODE *left, *right;
    int sp, visits, op;

    if (has_query)
        *has_query = 0;
    if (root == NULL)
        return 1;
    sp = 0;
    visits = 0;
    work[sp++] = root;
    while (sp > 0) {
        n = work[--sp];
        if (n == NULL)
            continue;
        if (++visits > 4096)
            return 0;
        op = n->Nop;
        left = right = NULL;

        if (op == N_FNCALL || op == Q_ASM || op == Q_MUUO)
            return 0;

        switch (op) {
        /* Terminals and statements without expression children. */
        case Q_IDENT:
        case N_ICONST:
        case N_PCONST:
        case N_ECONST:
        case N_FCONST:
        case N_SCONST:
        case N_VCONST:
        case Q_BREAK:
        case Q_CONTINUE:
            break;

        /* Statement nodes with one specifically defined child. */
        case Q_RETURN:
            right = n->Nright;
            break;
        case Q_GOTO:
        case Q_CASE:
        case Q_DEFAULT:
        case N_LABEL:
            left = n->Nleft;
            break;

        /* Unary expression nodes. */
        case N_CAST:
        case N_ADDR:
        case N_PTR:
        case N_NEG:
        case N_PREINC:
        case N_PREDEC:
        case N_POSTINC:
        case N_POSTDEC:
        case Q_COMPL:
        case Q_NOT:
            left = n->Nleft;
            break;

        /* Nodes whose normal representation has two node children. */
        case N_NODE:
        case N_STATEMENT:
        case N_EXPRLIST:
        case Q_IF:
        case Q_FOR:
        case Q_DO:
        case Q_WHILE:
        case Q_SWITCH:
        case Q_DOT:
        case Q_MEMBER:
        case Q_PLUS:
        case Q_MINUS:
        case Q_MPLY:
        case Q_DIV:
        case Q_MOD:
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
            left = n->Nleft;
            right = n->Nright;
            break;
        case Q_QUERY:
            if (has_query)
                *has_query = 1;
            left = n->Nleft;
            right = n->Nright;
            break;
        case Q_ASGN:
        case Q_ASPLUS:
        case Q_ASMINUS:
        case Q_ASMPLY:
        case Q_ASDIV:
        case Q_ASMOD:
        case Q_ASLSH:
        case Q_ASRSH:
        case Q_ASAND:
        case Q_ASXOR:
        case Q_ASOR:
            left = n->Nleft;
            right = n->Nright;
            break;

        default:
            return 0;
        }

        if (left) {
            if (sp >= (int)(sizeof(work) / sizeof(work[0])))
                return 0;
            work[sp++] = left;
        }
        if (right) {
            if (sp >= (int)(sizeof(work) / sizeof(work[0])))
                return 0;
            work[sp++] = right;
        }
    }
    return 1;
}

/* Return nonzero when a function body contains a two-word integer expression.
** Directly reserving three or four incoming ABI ACs leaves too little room for
** DImode pair reloads, so such functions should use the preserved-argument
** path instead of the leaf fast path. */
static int
fn_abi_has_dimode(NODE *root)
{
    NODE *work[128], *n, *left, *right;
    int sp, visits, op;

    if (root == NULL)
        return 0;
    sp = visits = 0;
    work[sp++] = root;
    while (sp > 0) {
        n = work[--sp];
        if (n == NULL)
            continue;
        if (++visits > 4096)
            return 1;
        if (n->Ntype != NULL && tisdimode(n->Ntype))
            return 1;
        op = n->Nop;
        left = right = NULL;
        switch (op) {
        case Q_IDENT: case N_ICONST: case N_PCONST: case N_ECONST:
        case N_FCONST: case N_SCONST: case N_VCONST:
        case Q_BREAK: case Q_CONTINUE:
            break;
        case Q_RETURN:
            right = n->Nright;
            break;
        case Q_GOTO: case Q_CASE: case Q_DEFAULT: case N_LABEL:
        case N_CAST: case N_ADDR: case N_PTR: case N_NEG:
        case N_PREINC: case N_PREDEC: case N_POSTINC: case N_POSTDEC:
        case Q_COMPL: case Q_NOT:
            left = n->Nleft;
            break;
        case N_FNCALL: case N_NODE: case N_STATEMENT: case N_EXPRLIST:
        case Q_IF: case Q_FOR: case Q_DO: case Q_WHILE: case Q_SWITCH:
        case Q_DOT: case Q_MEMBER: case Q_PLUS: case Q_MINUS: case Q_MPLY:
        case Q_DIV: case Q_MOD: case Q_LSHFT: case Q_RSHFT:
        case Q_LESS: case Q_GREAT: case Q_LEQ: case Q_GEQ:
        case Q_EQUAL: case Q_NEQ: case Q_ANDT: case Q_XORT: case Q_OR:
        case Q_LAND: case Q_LOR: case Q_QUERY: case Q_ASGN:
        case Q_ASPLUS: case Q_ASMINUS: case Q_ASMPLY: case Q_ASDIV:
        case Q_ASMOD: case Q_ASLSH: case Q_ASRSH: case Q_ASAND:
        case Q_ASXOR: case Q_ASOR:
            left = n->Nleft;
            right = n->Nright;
            break;
        default:
            return 1;
        }
        if (left) {
            if (sp >= (int)(sizeof(work) / sizeof(work[0]))) return 1;
            work[sp++] = left;
        }
        if (right) {
            if (sp >= (int)(sizeof(work) / sizeof(work[0]))) return 1;
            work[sp++] = right;
        }
    }
    return 0;
}

/* FN_ABI_QUERY_NORMAL - Is this scalar ?: safe to merge in a normal AC?
**
** CCGEN2 can keep these simple, call-free conditionals out of AC1.  Keep the
** shape test conservative and in sync with gternary_normal_ok().  Nested
** conditionals, calls, assignment, division, and other special-register
** operators stay on the traditional ABI return-register path.
*/
static int
fn_abi_query_normal_expr(NODE *n, int depth)
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
        return fn_abi_query_normal_expr(n->Nleft, depth + 1);
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
        return fn_abi_query_normal_expr(n->Nleft, depth + 1)
            && fn_abi_query_normal_expr(n->Nright, depth + 1);
    default:
        return 0;
    }
}

static int
fn_abi_query_normal(NODE *n)
{
    if (!n || n->Nop != Q_QUERY || sizetype(n->Ntype) != 1 || !n->Nright)
        return 0;
    return fn_abi_query_normal_expr(n->Nleft, 0)
        && fn_abi_query_normal_expr(n->Nright->Nleft, 0)
        && fn_abi_query_normal_expr(n->Nright->Nright, 0);
}

/* Mark all locally-safe scalar conditionals in a function.  This is only
** called when ABI liveness analysis has found a return-AC argument that is
** actually read after a conditional.  Direct-return min/max expressions are
** therefore left on the cheaper traditional AC1 merge path.
*/
static void
fn_abi_mark_normal_queries(NODE *root)
{
    NODE *work[256], *n;
    int sp;

    if (!root)
        return;
    sp = 0;
    work[sp++] = root;
    while (sp) {
        n = work[--sp];
        if (!n)
            continue;
        if (n->Nop == Q_QUERY && fn_abi_query_normal(n))
            n->Nflag |= NF_QUERYNORMAL;
        if (n->Nleft && sp < 255)
            work[sp++] = n->Nleft;
        if (n->Nright && sp < 255)
            work[sp++] = n->Nright;
    }
}

/* FN_ABI_PARAM_AFTER_QUERY - Does SYM need its ABI AC after ?: ?
**
** gternary() merges a one-word result through AC1 and a two-word result
** through AC1/AC2.  Track those clobbers as an AC mask in KCC's evaluation
** order.  An incoming argument used before a query is harmless because normal
** expression generation has already copied the needed value.  Only a later
** read whose original ABI AC is in the accumulated clobber mask requires
** promotion to a preserved register.
**
** Q_QUERY is special: its condition executes first, followed by exactly one
** arm.  The outer query's own return-AC clobber happens while finishing that
** arm, so it is exposed to the parent but not made to precede its own arms.
** A nested query in the condition does precede both arms and is propagated.
**
** Assignment is also special because gassign() evaluates the RHS before the
** destination lvalue.  Other expression/statement nodes use the conservative
** left-then-right order used by the normal generator for call-free trees.
*/
static int
fn_abi_param_after_query_1(NODE *n, SYMBOL *sym, int seen, int depth,
                           int *clobbers)
{
    NODE *left, *right;
    int op, lm, rm, cm, own;

    if (clobbers)
        *clobbers = 0;
    if (n == NULL)
        return 0;
    if (depth > 256)
        return 1;

    op = n->Nop;
    left = right = NULL;
    lm = rm = cm = 0;

    if (op == Q_IDENT)
        return (seen & (1 << sym->Svalue)) && n->Nid == sym;

    switch (op) {
    case N_ICONST:
    case N_PCONST:
    case N_ECONST:
    case N_FCONST:
    case N_SCONST:
    case N_VCONST:
    case Q_BREAK:
    case Q_CONTINUE:
        return 0;

    case Q_RETURN:
        right = n->Nright;
        break;
    case Q_GOTO:
    case Q_CASE:
    case Q_DEFAULT:
    case N_LABEL:
        left = n->Nleft;
        break;

    case N_CAST:
    case N_ADDR:
    case N_PTR:
    case N_NEG:
    case N_PREINC:
    case N_PREDEC:
    case N_POSTINC:
    case N_POSTDEC:
    case Q_COMPL:
    case Q_NOT:
        left = n->Nleft;
        break;

    case Q_QUERY:
        if (fn_abi_param_after_query_1(n->Nleft, sym, seen, depth + 1, &cm))
            return 1;
        if (n->Nright) {
            if (fn_abi_param_after_query_1(n->Nright->Nleft, sym,
                                           seen | cm, depth + 1, &lm))
                return 1;
            if (fn_abi_param_after_query_1(n->Nright->Nright, sym,
                                           seen | cm, depth + 1, &rm))
                return 1;
        }
        own = 0;
        if (!(n->Nflag & NF_QUERYNORMAL)) {
            if (sizetype(n->Ntype) >= 1)
                own |= (1 << 1);
            if (sizetype(n->Ntype) >= 2)
                own |= (1 << 2);
        }
        if (clobbers)
            *clobbers = cm | lm | rm | own;
        return 0;

    case Q_ASGN:
    case Q_ASPLUS:
    case Q_ASMINUS:
    case Q_ASMPLY:
    case Q_ASDIV:
    case Q_ASMOD:
    case Q_ASLSH:
    case Q_ASRSH:
    case Q_ASAND:
    case Q_ASXOR:
    case Q_ASOR:
        if (fn_abi_param_after_query_1(n->Nright, sym, seen, depth + 1, &rm))
            return 1;
        if (fn_abi_param_after_query_1(n->Nleft, sym, seen | rm,
                                       depth + 1, &lm))
            return 1;
        if (clobbers)
            *clobbers = lm | rm;
        return 0;

    case N_NODE:
    case N_STATEMENT:
    case N_EXPRLIST:
    case Q_IF:
    case Q_FOR:
    case Q_DO:
    case Q_WHILE:
    case Q_SWITCH:
    case Q_DOT:
    case Q_MEMBER:
    case Q_PLUS:
    case Q_MINUS:
    case Q_MPLY:
    case Q_DIV:
    case Q_MOD:
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
        left = n->Nleft;
        right = n->Nright;
        break;

    case N_FNCALL:
    case Q_ASM:
    case Q_MUUO:
    default:
        return 1;
    }

    if (fn_abi_param_after_query_1(left, sym, seen, depth + 1, &lm))
        return 1;
    if (fn_abi_param_after_query_1(right, sym, seen | lm, depth + 1, &rm))
        return 1;
    if (clobbers)
        *clobbers = lm | rm;
    return 0;
}

static int
fn_abi_param_after_query(NODE *root, SYMBOL *sym)
{
    int clobbers;

    return fn_abi_param_after_query_1(root, sym, 0, 0, &clobbers);
}

/* FN_ABI_TAILONLY - Recognize functions whose only calls are returned calls.
**
** Such calls consume the incoming ABI arguments before control leaves the
** function, so AC1..AC4 do not need to be copied to preserved registers.
** Nested calls in the returned call arguments are deliberately rejected.
*/
static int
fn_abi_tailonly(NODE *root)
{
    struct workent { NODE *n; int tail; } work[128];
    NODE *n, *left, *right;
    int sp, visits, op, tail;

    if (root == NULL)
        return 1;
    sp = 0;
    visits = 0;
    work[sp].n = root;
    work[sp++].tail = 0;
    while (sp > 0) {
        --sp;
        n = work[sp].n;
        tail = work[sp].tail;
        if (n == NULL)
            continue;
        if (++visits > 4096)
            return 0;
        op = n->Nop;
        left = right = NULL;

        if (op == Q_ASM || op == Q_MUUO)
            return 0;
        if (op == N_FNCALL) {
            if (!tail)
                return 0;
            left = n->Nleft;
            right = n->Nright;
            tail = 0;
        } else switch (op) {
        case Q_IDENT:
        case N_ICONST:
        case N_PCONST:
        case N_ECONST:
        case N_FCONST:
        case N_SCONST:
        case N_VCONST:
        case Q_BREAK:
        case Q_CONTINUE:
            break;

        case Q_RETURN:
            if (n->Nright && n->Nright->Nop == N_FNCALL) {
                if (sp >= (int)(sizeof(work) / sizeof(work[0])))
                    return 0;
                work[sp].n = n->Nright;
                work[sp++].tail = 1;
                continue;
            }
            right = n->Nright;
            break;
        case Q_GOTO:
        case Q_CASE:
        case Q_DEFAULT:
        case N_LABEL:
            left = n->Nleft;
            break;

        case N_CAST:
        case N_ADDR:
        case N_PTR:
        case N_NEG:
        case N_PREINC:
        case N_PREDEC:
        case N_POSTINC:
        case N_POSTDEC:
        case Q_COMPL:
        case Q_NOT:
            left = n->Nleft;
            break;

        case N_NODE:
        case N_STATEMENT:
        case N_EXPRLIST:
        case Q_IF:
        case Q_FOR:
        case Q_DO:
        case Q_WHILE:
        case Q_SWITCH:
        case Q_DOT:
        case Q_MEMBER:
        case Q_PLUS:
        case Q_MINUS:
        case Q_MPLY:
        case Q_DIV:
        case Q_MOD:
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
        case Q_QUERY:
        case Q_ASGN:
        case Q_ASPLUS:
        case Q_ASMINUS:
        case Q_ASMPLY:
        case Q_ASDIV:
        case Q_ASMOD:
        case Q_ASLSH:
        case Q_ASRSH:
        case Q_ASAND:
        case Q_ASXOR:
        case Q_ASOR:
            left = n->Nleft;
            right = n->Nright;
            break;

        default:
            return 0;
        }

        if (left) {
            if (sp >= (int)(sizeof(work) / sizeof(work[0])))
                return 0;
            work[sp].n = left;
            work[sp++].tail = 0;
        }
        if (right) {
            if (sp >= (int)(sizeof(work) / sizeof(work[0])))
                return 0;
            work[sp].n = right;
            work[sp++].tail = 0;
        }
    }
    return 1;
}

/* FN_ABI_PARAM_USED - Conservatively find a parameter reference.
**
** The parser's Srefs count includes bookkeeping references and is therefore
** not accurate enough for deciding whether an ABI argument needs storage.
** Walk the completed function tree instead.  Unknown node shapes are treated
** as a use so this can only suppress promotion when absence is proven.
*/
static int
fn_abi_param_used(NODE *root, SYMBOL *sym)
{
    NODE *work[128], *n, *left, *right;
    int sp, visits, op;

    if (root == NULL)
        return 0;
    sp = 0;
    visits = 0;
    work[sp++] = root;
    while (sp > 0) {
        n = work[--sp];
        if (n == NULL)
            continue;
        if (++visits > 4096)
            return 1;
        op = n->Nop;
        left = right = NULL;

        if (op == Q_IDENT) {
            if (n->Nid == sym)
                return 1;
            continue;
        }

        switch (op) {
        case N_ICONST:
        case N_PCONST:
        case N_ECONST:
        case N_FCONST:
        case N_SCONST:
        case N_VCONST:
        case Q_BREAK:
        case Q_CONTINUE:
            break;

        case Q_RETURN:
            right = n->Nright;
            break;
        case Q_GOTO:
        case Q_CASE:
        case Q_DEFAULT:
        case N_LABEL:
            left = n->Nleft;
            break;

        case N_CAST:
        case N_ADDR:
        case N_PTR:
        case N_NEG:
        case N_PREINC:
        case N_PREDEC:
        case N_POSTINC:
        case N_POSTDEC:
        case Q_COMPL:
        case Q_NOT:
            left = n->Nleft;
            break;

        case N_FNCALL:
        case N_NODE:
        case N_STATEMENT:
        case N_EXPRLIST:
        case Q_IF:
        case Q_FOR:
        case Q_DO:
        case Q_WHILE:
        case Q_SWITCH:
        case Q_DOT:
        case Q_MEMBER:
        case Q_PLUS:
        case Q_MINUS:
        case Q_MPLY:
        case Q_DIV:
        case Q_MOD:
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
        case Q_QUERY:
        case Q_ASGN:
        case Q_ASPLUS:
        case Q_ASMINUS:
        case Q_ASMPLY:
        case Q_ASDIV:
        case Q_ASMOD:
        case Q_ASLSH:
        case Q_ASRSH:
        case Q_ASAND:
        case Q_ASXOR:
        case Q_ASOR:
            left = n->Nleft;
            right = n->Nright;
            break;

        case Q_ASM:
        case Q_MUUO:
        default:
            return 1;
        }

        if (left) {
            if (sp >= (int)(sizeof(work) / sizeof(work[0])))
                return 1;
            work[sp++] = left;
        }
        if (right) {
            if (sp >= (int)(sizeof(work) / sizeof(work[0])))
                return 1;
            work[sp++] = right;
        }
    }
    return 0;
}

/* FN_ABI_DROP_FIRST_CALL - Parameter dies at the first bare call.
**
** Walk the straight-line top-level statement chain until its first call.
** Leading statements must themselves be call-free, so the incoming ABI AC
** cannot have been clobbered.  The first call must be a bare call statement
** that uses the parameter, and the parameter must be dead in all following
** statements.  The AC can then remain reserved through that call and be
** released immediately afterward.
*/
/* FN_ABI_DROP_BEFORE_FIRST_CALL - Parameter dies before the first bare call.
**
** As with FN_ABI_DROP_FIRST_CALL, only a straight-line top-level prefix is
** accepted.  If the parameter is not referenced by the first call or by any
** following statement, its incoming ABI AC only has to remain reserved until
** that call is reached.  Releasing it before argument generation avoids a
** preserved-register promotion and lets the call reuse the volatile AC.
*/
static int
fn_abi_drop_before_first_call(NODE *body, SYMBOL *sym)
{
    NODE *stmt, *expr;

    stmt = body;
    if (stmt != NULL && stmt->Nop == N_STATEMENT
      && stmt->Nleft != NULL && stmt->Nleft->Nop == N_STATEMENT)
        stmt = stmt->Nleft;
    while (stmt != NULL) {
        if (stmt->Nop != N_STATEMENT)
            return 0;
        expr = stmt->Nleft;
        if (expr == NULL || expr->Nop == N_DATA) {
            stmt = stmt->Nright;
            continue;
        }
        if (expr->Nop == N_FNCALL)
            return !fn_abi_param_used(expr, sym)
                && !fn_abi_param_used(stmt->Nright, sym);
        if (!fn_abi_leaf(expr, (int *)NULL))
            return 0;
        stmt = stmt->Nright;
    }
    return 0;
}

static int
fn_abi_drop_first_call(NODE *body, SYMBOL *sym)
{
    NODE *stmt, *expr;

    stmt = body;
    if (stmt != NULL && stmt->Nop == N_STATEMENT
      && stmt->Nleft != NULL && stmt->Nleft->Nop == N_STATEMENT)
        stmt = stmt->Nleft;
    while (stmt != NULL) {
        if (stmt->Nop != N_STATEMENT)
            return 0;
        expr = stmt->Nleft;
        if (expr == NULL || expr->Nop == N_DATA) {
            stmt = stmt->Nright;
            continue;
        }
        if (expr->Nop == N_FNCALL) {
            if (!fn_abi_param_used(expr, sym))
                return 0;
            return !fn_abi_param_used(stmt->Nright, sym);
        }
        if (!fn_abi_leaf(expr, (int *)NULL))
            return 0;
        stmt = stmt->Nright;
    }
    return 0;
}

/* FN_ABI_PARAM_REFS - Count exact references in the completed function tree.
** Stop above one because phase 44 only consumes DImode ABI pairs in place
** when the parameter has a single read.
*/
static int
fn_abi_param_refs(NODE *root, SYMBOL *sym)
{
    NODE *work[128], *n, *left, *right;
    int sp, visits, count, op;

    if (root == NULL)
        return 0;
    sp = visits = count = 0;
    work[sp++] = root;
    while (sp > 0) {
        n = work[--sp];
        if (n == NULL)
            continue;
        if (++visits > 4096)
            return 2;
        op = n->Nop;
        left = right = NULL;
        if (op == Q_IDENT) {
            if (n->Nid == sym && ++count > 1)
                return count;
            continue;
        }
        switch (op) {
        case N_ICONST:
        case N_PCONST:
        case N_ECONST:
        case N_FCONST:
        case N_SCONST:
        case N_VCONST:
        case Q_BREAK:
        case Q_CONTINUE:
            break;
        case Q_RETURN:
            right = n->Nright;
            break;
        case Q_GOTO:
        case Q_CASE:
        case Q_DEFAULT:
        case N_LABEL:
        case N_CAST:
        case N_ADDR:
        case N_PTR:
        case N_NEG:
        case N_PREINC:
        case N_PREDEC:
        case N_POSTINC:
        case N_POSTDEC:
        case Q_COMPL:
        case Q_NOT:
            left = n->Nleft;
            break;
        case N_FNCALL:
        case N_NODE:
        case N_STATEMENT:
        case N_EXPRLIST:
        case Q_IF:
        case Q_FOR:
        case Q_DO:
        case Q_WHILE:
        case Q_SWITCH:
        case Q_DOT:
        case Q_MEMBER:
        case Q_PLUS:
        case Q_MINUS:
        case Q_MPLY:
        case Q_DIV:
        case Q_MOD:
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
        case Q_QUERY:
        case Q_ASGN:
        case Q_ASPLUS:
        case Q_ASMINUS:
        case Q_ASMPLY:
        case Q_ASDIV:
        case Q_ASMOD:
        case Q_ASLSH:
        case Q_ASRSH:
        case Q_ASAND:
        case Q_ASXOR:
        case Q_ASOR:
            left = n->Nleft;
            right = n->Nright;
            break;
        default:
            return 2;
        }
        if (left) {
            if (sp >= (int)(sizeof(work) / sizeof(work[0])))
                return 2;
            work[sp++] = left;
        }
        if (right) {
            if (sp >= (int)(sizeof(work) / sizeof(work[0])))
                return 2;
            work[sp++] = right;
        }
    }
    return count;
}

/* FN_ABI_DIRECT_PEER_SIMPLE - True when the operand opposite a direct
** DImode ABI parameter can be consumed without first materializing another
** doubleword temporary.  A nested wide expression must use the historical
** stack reconstruction path or it can reserve the only allocatable AC pair.
*/
static int
fn_abi_direct_peer_simple(NODE *n)
{
    if (n == NULL)
        return 1;
    switch (n->Nop) {
    case Q_IDENT:
    case N_ICONST:
    case N_PCONST:
    case N_ECONST:
    case N_FCONST:
    case N_SCONST:
    case N_VCONST:
    case N_CAST:
    case N_PTR:
    case Q_DOT:
    case Q_MEMBER:
        return 1;
    default:
        return sizetype(n->Ntype) != 2;
    }
}

/* FN_ABI_PARAM_DIRECT_USE - Limit in-place ABI-pair consumption to node
** shapes whose DImode generators are already overlap-safe.  Broader pair
** lifetime support belongs to a later phase.
*/
static int
fn_abi_param_direct_use(NODE *n, SYMBOL *sym)
{
    NODE *left, *right;
    int lhit, rhit;

    if (n == NULL || n->Nop == Q_IDENT)
        return 0;
    left = right = NULL;
    switch (n->Nop) {
    case N_ICONST:
    case N_PCONST:
    case N_ECONST:
    case N_FCONST:
    case N_SCONST:
    case N_VCONST:
    case Q_BREAK:
    case Q_CONTINUE:
        return 0;
    case Q_RETURN:
        right = n->Nright;
        break;
    case Q_GOTO:
    case Q_CASE:
    case Q_DEFAULT:
    case N_LABEL:
    case N_CAST:
    case N_ADDR:
    case N_PTR:
    case N_NEG:
    case N_PREINC:
    case N_PREDEC:
    case N_POSTINC:
    case N_POSTDEC:
    case Q_COMPL:
    case Q_NOT:
        left = n->Nleft;
        break;
    case N_FNCALL:
    case N_NODE:
    case N_STATEMENT:
    case N_EXPRLIST:
    case Q_IF:
    case Q_FOR:
    case Q_DO:
    case Q_WHILE:
    case Q_SWITCH:
    case Q_DOT:
    case Q_MEMBER:
    case Q_PLUS:
    case Q_MINUS:
    case Q_MPLY:
    case Q_DIV:
    case Q_MOD:
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
    case Q_QUERY:
    case Q_ASGN:
    case Q_ASPLUS:
    case Q_ASMINUS:
    case Q_ASMPLY:
    case Q_ASDIV:
    case Q_ASMOD:
    case Q_ASLSH:
    case Q_ASRSH:
    case Q_ASAND:
    case Q_ASXOR:
    case Q_ASOR:
        left = n->Nleft;
        right = n->Nright;
        break;
    default:
        return 0;
    }
    lhit = left != NULL && left->Nop == Q_IDENT && left->Nid == sym;
    rhit = right != NULL && right->Nop == Q_IDENT && right->Nid == sym;
    if (lhit || rhit) {
        if ((lhit && !fn_abi_direct_peer_simple(right))
          || (rhit && !fn_abi_direct_peer_simple(left)))
            return 0;
        switch (n->Nop) {
        case Q_RETURN:
        case Q_PLUS:
        case Q_MINUS:
        case Q_MPLY:
        case Q_LSHFT:
        case Q_RSHFT:
        case Q_ANDT:
        case Q_XORT:
        case Q_OR:
            return 1;
        default:
            return 0;
        }
    }
    return fn_abi_param_direct_use(left, sym)
        || fn_abi_param_direct_use(right, sym);
}

/* FN_ABI_PARAM_MODIFIED - Reject direct register residence when a parameter
** is used as an lvalue.  Phase 44 keeps DImode pairs direct only for
** read-only, call-free functions; all other cases retain the stack shim.
*/
static int
fn_abi_param_modified(NODE *n, SYMBOL *sym)
{
    NODE *l, *r;

    if (n == NULL)
        return 0;
    l = r = NULL;
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
    case Q_ASXOR:
    case Q_ASOR:
        if (fn_abi_param_used(n->Nleft, sym))
            return 1;
        r = n->Nright;
        break;
    case N_PREINC:
    case N_PREDEC:
    case N_POSTINC:
    case N_POSTDEC:
    case N_ADDR:
        if (fn_abi_param_used(n->Nleft, sym))
            return 1;
        break;
    case Q_IDENT:
    case N_ICONST:
    case N_PCONST:
    case N_ECONST:
    case N_FCONST:
    case N_SCONST:
    case N_VCONST:
    case Q_BREAK:
    case Q_CONTINUE:
        break;
    case Q_RETURN:
        r = n->Nright;
        break;
    case Q_GOTO:
    case Q_CASE:
    case Q_DEFAULT:
    case N_LABEL:
    case N_CAST:
    case N_PTR:
    case N_NEG:
    case Q_COMPL:
    case Q_NOT:
        l = n->Nleft;
        break;
    case N_FNCALL:
    case N_NODE:
    case N_STATEMENT:
    case N_EXPRLIST:
    case Q_IF:
    case Q_FOR:
    case Q_DO:
    case Q_WHILE:
    case Q_SWITCH:
    case Q_DOT:
    case Q_MEMBER:
    case Q_PLUS:
    case Q_MINUS:
    case Q_MPLY:
    case Q_DIV:
    case Q_MOD:
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
    case Q_QUERY:
        l = n->Nleft;
        r = n->Nright;
        break;
    default:
        return 1;
    }
    return fn_abi_param_modified(l, sym)
        || fn_abi_param_modified(r, sym);
}

static void
promote_abi_params(SYMBOL *args, SYMBOL *fn, NODE *body)
{
    SYMBOL *s;
    TYPE *p;
    int need, nargs;

    fnabidirect = 0;
    fnargkeepmask = 0;
    fnargdropmask = 0;
    fnargpredropmask = 0;
    if (!args || sizetype(fn->Stype->Tsubt) > GCCABI_RET_REGS)
        return;

    /* Keep fully register-passed, read-only DImode parameters in their
    ** incoming ABI pairs for call-free functions.  Split AC/stack pairs,
    ** address-taken or modified parameters, and functions containing calls
    ** continue to use the historical stack reconstruction.
    */
    {
        int haspair, leaf, has_query, siz, reg;

        haspair = 0;
        for (s = args; s; s = s->Spmnext)
            if (sizetype(s->Stype) == 2)
                haspair = 1;
        if (haspair) {
            leaf = fn_abi_leaf(body, &has_query);
            if (!leaf)
                return;
            for (s = args; s; s = s->Spmnext) {
                siz = sizetype(s->Stype);
                if ((siz != 1 && siz != 2)
                  || (s->Sflags & SF_ADDRTAKEN)
                  || s->Svalue - siz + 1 < 1
                  || s->Svalue > GCCABI_ARG_REGS
                  || fn_abi_param_modified(body, s)
                  || (siz == 2 && (fn_abi_param_refs(body, s) != 1
                    || !fn_abi_param_direct_use(body, s)))
                  || (s->Sclass != SC_ARG && s->Sclass != SC_RARG))
                    return;
            }
            for (s = args; s; s = s->Spmnext) {
                siz = sizetype(s->Stype);
                s->Sclass = SC_RARG;
                s->Sreg = s->Svalue - siz + 1;
                s->Sflags |= SF_ABIREG;
                if (siz == 2)
                    s->Sflags |= SF_ABICONSUME;
                for (reg = 0; reg < siz; ++reg)
                    fnargkeepmask |= (1 << (s->Sreg + reg));
            }
            fnabidirect = 1;
            return;
        }
    }

    need = 0;
    nargs = 0;
    for (s = args; s; s = s->Spmnext) {
        ++nargs;
        if (sizetype(s->Stype) != 1 || tispacked(s->Stype)
          || s->Stype->Tspec == TS_STRUCT || s->Stype->Tspec == TS_UNION
          || (s->Sflags & SF_ADDRTAKEN)
          || s->Svalue < 1 || s->Svalue > GCCABI_ARG_REGS)
            return;
        if (s->Sclass == SC_ARG) {
            if (!fn_abi_drop_before_first_call(body, s)
              && !fn_abi_drop_first_call(body, s))
                ++need;
        } else if (s->Sclass != SC_RARG)
            return;
    }

    p = fn->Stype->Tproto ? fn->Stype->Tproto : fn->Shproto;
    while (p && p->Tspec == TS_PARAM)
        p = p->Tproto;
    if (p && p->Tspec == TS_PARINF)
        return;

    /* A call-free function can leave one-word ABI parameters in AC1..AC4.
    ** Reserve those ACs from temporary allocation and from destructive
    ** peephole coalescing.  No prologue copies or preserved-register saves
    ** are then required for the parameters.
    */
    if (need <= 4 && !(nargs >= 3 && fn_abi_has_dimode(body))) {
        int leaf, has_query, nsave;

        leaf = fn_abi_leaf(body, &has_query);
        if (!(leaf || fn_abi_tailonly(body)))
            goto preserve_params;
        nsave = 0;

        /* If a simple scalar ?: is the only reason a live ABI argument
        ** would have to leave AC1/AC2, mark such queries for normal-AC
        ** merging before computing the final preservation set.
        */
        if (optgen && leaf && has_query) {
            for (s = args; s; s = s->Spmnext)
                if (s->Sclass == SC_ARG && s->Svalue <= GCCABI_RET_REGS
                  && fn_abi_param_after_query(body, s)) {
                    fn_abi_mark_normal_queries(body);
                    break;
                }
        }

        /* AC1/AC2 are also the mandatory scalar/double return registers.
        ** Expression generation may need them before the final return (for
        ** example to merge a ?: value), so an incoming parameter cannot
        ** safely remain live there for the whole function.  Keep AC3/AC4
        ** direct, but move ABI parameters from return ACs to preserved ACs.
        */
        for (s = args; s; s = s->Spmnext)
            if (leaf && has_query && s->Sclass == SC_ARG
              && s->Svalue <= GCCABI_RET_REGS
              && fn_abi_param_after_query(body, s))
                ++nsave;
        if (_reg_count + nsave > fnregcap_v11())
            return;

        for (s = args; s; s = s->Spmnext) {
            if (s->Sclass == SC_ARG) {
                s->Sclass = SC_RARG;
                if (leaf && has_query && s->Svalue <= GCCABI_RET_REGS
                  && fn_abi_param_after_query(body, s)) {
                    s->Sreg = _reg_count + r_maxnopreserve + 1;
                    Reg_Id[_reg_count++] = s;
                } else {
                    s->Sreg = s->Svalue;
                    fnargkeepmask |= (1 << s->Sreg);
                }
            } else {
                s->Sreg = s->Svalue;
                fnargkeepmask |= (1 << s->Sreg);
            }
            s->Sflags |= SF_ABIREG;
        }
        fnabidirect = 1;
        return;
    }

preserve_params:
    if (_reg_count + need > fnregcap_v11())
        return;

    for (s = args; s; s = s->Spmnext) {
        if (fn_abi_drop_before_first_call(body, s)) {
            if (s->Sclass == SC_ARG)
                s->Sclass = SC_RARG;
            s->Sreg = s->Svalue;
            s->Sflags |= SF_ABIREG;
            fnargkeepmask |= (1 << s->Sreg);
            fnargpredropmask |= (1 << s->Sreg);
            continue;
        }
        if (fn_abi_drop_first_call(body, s)) {
            if (s->Sclass == SC_ARG)
                s->Sclass = SC_RARG;
            s->Sreg = s->Svalue;
            s->Sflags |= SF_ABIREG;
            fnargkeepmask |= (1 << s->Sreg);
            fnargdropmask |= (1 << s->Sreg);
            continue;
        }
        if (s->Sclass == SC_ARG) {
            s->Sclass = SC_RARG;
            s->Sreg = _reg_count + r_maxnopreserve + 1;
            Reg_Id[_reg_count++] = s;
        }
        s->Sflags |= SF_ABIREG;
    }
    fnabidirect = 1;
}

static NODE *
funcdef(SYMBOL *b, SYMBOL *d, SYMBOL *syment)
/* Base, contains parsed <decl-specs>, 
 * Defaulted <decl-specs> plus <declor> type
 * Identifier's symtab entry 
 */
{
    INT n, siz;
    int nsjmps;
    NODE *nnode, *header, *vlaparamdata;
    SYMBOL *s1;
    SYMBOL *args = d->Spmnext;  /*  List of parameter syms */
    SYMBOL *arghead = args;
    int npartypes = (int) d->Svalue;    /* # of params if new-style proto */
    int nparidents = 0;
    NODE *nreg;

    /* "syment" points to the function name's symtab entry.
    ** Lexer will have created the symtab entry with
    ** class SC_UNDEF if it didn't already exist.
    */

    if (strcmp(syment->Sname, "main") == 0)
        fn_main = (char) ~0;                            // FW KCC-NT
    else
        fn_main = 0;

    if (!args)                  /*If no arglist, need to open local sym blk */
        (void) beglsym();       /* (needn't remember start) */

    curfnloc = curfnnew;
                /* Remember context for error messages */
    curfn = syment = funchk(1, b->Sclass, d, syment);   /* Do funct decl */
                                /* which may update symbol and may also */
                                /* do old-style <decl-list> parse! */

    /* Now that types are set, add up sizes and determine offsets for
    ** each parameter.  Also, for new-style function prototype defs, make
    ** sure that every parameter had an identifier associated with it.
    ** We do this by comparing the # of identifiers on the paramlist with
    ** the # of parameters that paramlist() actually parsed while building
    ** the prototype.
    */
    n = 0;                              /* set up for first arg */
    siz = sizetype(syment->Stype->Tsubt);       /* get size of return val */
    if (siz > 4)                        /* If returning too-large object, */
        n = 1;                          /* just use struct-return pointer */
    while (args != NULL)
        {
        nparidents++;                   /* Count # of param idents */
        s1 = args;                      /* get arg symbol */
        args = args->Spmnext;           /* Move on before zapping it! */
        n += sizetype(s1->Stype);       /* count off by size */
        s1->Svalue = n;                 /* set offset */
        }
    if (npartypes && (npartypes-1 != nparidents))
        error("Missing identifier for parameter in function def");

    maxauto = 0;                        /* no local variables yet */
    fnvla_v11 = 0;                      /* establish per-function VLA state */
    vlaparamdata = vlaparamdata_v11(arghead);
    stackrefs = 0;                      /* and therefore no refs to them */
    nsjmps = nsetjmps();                /* Remember # of setjmp refs */
    statdecls = stattail = NULL;        /* No static declarations yet */

    /* The big call.  Parse function statement, having already set up a
    ** local symbol block with the parameters in it.  On return, this
    ** block will have been ended, and the current token will be the
    ** T_RBRACE ('}') terminating the function body.  See CCSTMT's
    ** compound() for further discussion.
    */
    nnode = funstmt();                  /* Parse function statement */
    if (vlaparamdata != NULL)
        nnode = ndeflr(N_STATEMENT, vlaparamdata, nnode);

    promote_abi_params(arghead, syment, nnode);

    expect(T_RBRACE);   /* Now safe to flush the right brace
                        ** and set up new current token.
                        ** See CCSTMT's compound() for discussion of this.
                        */
    stkgoto = (nsjmps != nsetjmps());   /* Say whether any setjmps in funct */
    header = ndefop(N_NODE);            /* Put together the function header */
    header->Nright = statdecls;         /* Point to any static decls found */
    header->Nleft = ndefident(syment);  /* Point to Q_IDENT function name */

    /* Return completed parse tree */
    nreg = ndeflr(N_FUNCTION, header, nnode);
    nreg->Nreg = _reg_count;
    if (fnvla_v11)
        nreg->Nflag |= NF_VLA;

    return nreg;
}

/* NSETJMPS - Auxiliary to find current # of references to the "setjmp"
**      function.  Any functions which contain calls to setjmp have to
**      avoid using tail recursion.
*/
static int
nsetjmps(void)
{
    SYMBOL *s;
    if ((s = symfidstr("setjmp")) != NULL)
        {
        --(s->Srefs);                   /* Not a real reference */
        switch (s->Sclass)
            {
            case SC_EXTREF:
            case SC_XEXTREF:
            case SC_EXTDEF:
                if (s->Stype->Tspec == TS_FUNCT)
                    return s->Srefs;
            }
        }
    return 0;
}

/* FUNCHK - Check out a function definition or reference for
**      proper use of storage class and type specifier.
** Called from funcdef() for a definition and dodecl() for a reference.
** Also called from CCSTMT's primary() to handle the pretend-declaration
** done for a call to an undeclared function.
**
** The parsed declarator symbol struct will have the following special
** members set (by fundecl()) if called from funcdef():
**      Svalue - 0 if an old-style function identlist, N+1 if new-style proto,
**              where N is the # of real parameters in the prototype.
**      Spmnext - if non-NULL, points to a list of parameter identifiers
**              and there is still a local symbol block active for the
**              prototype scope.  This block needs to be closed and the
**              symbols flushed if this was only a reference.
**      Spmhead - what beglsym() returned for the prototype scope block.
** These syms should be NULL if called from dodecl(), which flushes the
** prototype scope block itself.
**
** May parse the function's <decl-list> if doing an old-style definition.
** Returns with the symbol table entry completely set up, and returns
** pointer to the symbol since it may have changed.
*/
SYMBOL *
funchk(int def, int baseclass, SYMBOL *d, SYMBOL *s)
/* True if a definition
 * Parsed base storage class
 * Parsed declarator (Sclass and Stype)
 * Symtab entry for parsed identifier
 */
{
    
    /* Check out storage class.
    ** Parsing should have resulted in only one of the following three
    ** things: SC_UNDEF (none), SC_EXTREF (extern), SC_INTREF (static).
    ** For existing function def/refs, there are 5
    ** possible symbol classes:
    ** SC_EXTREF, SC_EXTDEF, SC_INTREF, SC_INTDEF, plus special case of
    ** normally invisible SC_XEXTREF.
    */
    s = symfxext(s);            /* Make SC_XEXTREF visible if any */

    if ((d->Sflags & (SF_INLINE | SF_NOINLINE)) == (SF_INLINE | SF_NOINLINE))
        warn("inline function given attribute noinline");

    switch (baseclass)  /* Not all storage classes are allowed */
        {
        default:
            error("Illegal storage class for function, assuming \"extern\"");
            return funchk(def, SC_EXTREF, d, s);        /* Just call again */

        case SC_INTREF:         /* Explicit "static" */
        /* dpANS: identifier always has internal linkage.
        ** Note this must not appear within block scope!
        */
            if (!lsymhead || def)
                {
            /* At file-scope level, so "s" will be global */
                d->Sclass = (def ? SC_INTDEF : SC_INTREF);
                if (s->Sclass == SC_UNDEF               /* No old sym */
                  || s->Sclass == SC_INTREF             /* Or not defined */
                  || s->Sclass == SC_INTDEF)    /* Or defd, reffing */
                    break;                              /* Linkage matches OK */
                if (s->Sclass != SC_EXTREF && s->Sclass != SC_EXTDEF
                  && s->Sclass != SC_XEXTREF)
                    {
                    error("Storage class conflict for \"%s\"", s->Sname);

                    break;                      /* Keep internal linkage */
                    }
                warn("Linkage conflict for %S, is external", s);
            /* Else fall thru as if SC_EXTREF */
                }
            else if (!def)
                warn("Storage class for function decl in block must be \"extern\"");
        /* Drop thru to assume extern and carry on */

        /* FALLTHROUGH */
        case SC_UNDEF:          /* dpANS: default same as explicit "extern" */
        case SC_EXTREF:         /* Explicit "extern" */
        /* dpANS: linkage is same as any visible decl of this identifier
        ** with file scope (ie global).  If none, linkage is external.
        ** If there is already an expired external reference (SC_XEXTREF)
        ** then we use that in order to emit advisory warnings if the type
        ** isn't the same.
        */
        /* If doing a reference in block scope and function ident already
        ** belongs to a block-scope symbol, we:
        ** (1) see if a file-scope symbol exists (if so, use that one)
        ** (2) see if existing sym is external ref (if so, use that)
        **      (this includes SC_XEXTREF)
        ** (3) generate new file-scope symbol with SC_EXTREF.
        */
            if (!def && (s->Sflags&SF_LOCAL))
                {
                SYMBOL *ns;
                if ((ns = findgsym(s)) != NULL)/* If file-scope def/ref exists, */
                    s = ns;                     /* use that! */
                else if (s->Sclass == SC_EXTREF)
                    ;           /* OK if a block-scope extern ref */
                else
                    {
                    if (isdupsym(s))    /* If was defined in this block, */
                        errdupsym(s);   /* barf! */
                    s = uniqsym(s);             /* Create/re-use sym, now SC_UNDEF */
                    }
                }
        /* "s" now points to the sym linked to.  If it has any linkage, we
        ** copy that, otherwise we use external linkage.
        */
            switch (s->Sclass)
                {
                case SC_INTDEF:
                case SC_INTREF:
                    d->Sclass = (def ? SC_INTDEF : SC_INTREF);
                    break;

                default:
                    errdupsym(s);
                    s->Sclass = SC_UNDEF;               /* Sigh!  Smash it. */
            /* Fall thru to handle as if extern */

                /* FALLTHROUGH */
                case SC_XEXTREF:
                case SC_EXTDEF:
                case SC_EXTREF:
                case SC_UNDEF:
                    d->Sclass = (def ? SC_EXTDEF : SC_EXTREF);
                    break;
                }
            break;
        }

    /* Check for duplicate function def */
    if (def && (s->Sclass == SC_EXTDEF || s->Sclass == SC_INTDEF))
        {
        error("Duplicate function definition");
        s->Sclass = SC_UNDEF;   /* Wipe out previous definition! */
        }

    /* Handle simple case where sym hasn't yet been defined. */
    if (s->Sclass == SC_UNDEF)  /* Symbol not defined yet? */
        {
        if (!def)
            s = uniqsym(s);     /* No, ensure local if needed */
        s->Sclass = d->Sclass;          /* Copy the parsed class */
        s->Stype = d->Stype;            /* and the type specification */
        s->Sflags |= d->Sflags & (SF_NORETURN | SF_NOINLINE | SF_INLINE);
        s->Srefs = 0;                   /* and reset usage cnt in case a ref */
        if (s->Sclass == SC_INTDEF || s->Sclass == SC_INTREF)
            mapintsym(s);               /* Set Smaplab (internal unique) */
        else if (!mapextsym(s))         /* Else check external map */
            error("External link name duplicated: %S", s);
        if (def && !s->Stype->Tproto)   /* If an old-style definition, */
            {
            pdecllist();                /* parse parameter declarations! */
            s->Shproto = mkproto(d->Spmnext);   /* Build invisible prototype */
            }
        else
            s->Shproto = NULL;          /* Say no hidden prototype */
        return s;                       /* All done! */
        }

    /* There is an existing symbol for this identifier, with a matching
    ** storage class.  Must now do lots of hairy type checking.
    ** First we see if old sym is a function returning a
    ** compatible type, then maybe check parameter list compatibility.
    **
    ** If both are old-style, no param checking needed.
    ** If both are new-style, check their param lists.
    ** If one sym is old-style ref and other is new-style ref/def,
    **   just do simple check of the proto list.
    ** If one sym is old-style DEF, then:
    **          If this is more recent sym, parse decl-list into prototype
    **          and then compare param lists as if both were new-style.
    **          Else is previous sym -- compare with "hidden" prototype
    **          that was generated during the old-style function def.
    */

    /* First make sure that basic type is also "function returning ..."
    ** and that return types are compatible.
    */
    if (s->Stype->Tspec != TS_FUNCT
      || !cmptype(d->Stype->Tsubt, s->Stype->Tsubt))
        {
        /* Earlier ref/def conflicts with current one, smash to current. */
        errtwotyp(d, s);                /* Complain */
        s->Stype = d->Stype;            /* Skip prototype testing */
        }

    /* If doing an old-style definition, OK to parse decl-list now.
    ** The main reason for not doing this earlier is so error messages
    ** from the previous checks will show the most helpful context,
    ** i.e. errors are announced as soon as it is possible to detect them.
    */
    if (def && !d->Stype->Tproto)               /* If no proto was declared, */
        {
        pdecllist();                            /* parse old-style decl-list */
        s->Shproto = mkproto(d->Spmnext);       /* Build invisible prototype */
        }

    /* Now see if the parameter lists need to be checked.
    ** Test for both old-style, or both with identical prototypes.
    */
    if (d->Stype->Tproto != s->Stype->Tproto)
        {
        TYPE *tc;
        if (d->Stype->Tproto && s->Stype->Tproto)       /* Compare 2 protos? */
            {
            tc = tcomposite(d->Stype, s->Stype);        /* Get composite */
            if (tc)
                d->Stype = tc;                  /* If won, use it! */
            else
                plcmpare(d->Stype->Tproto, s->Stype->Tproto, 0); /* Give err */
            }
        else if (d->Stype->Tproto)
            {
            /* Compare existing non-proto ref/def with new prototype */
            if (s->Sclass == SC_EXTDEF || s->Sclass == SC_INTDEF)
                {
                /* Old-style DEF followed by new-style REF, use hidden proto */
                if (!tcomposite(d->Stype, s->Stype))    /* If not compatible */
                    plcmpare(d->Stype->Tproto, s->Shproto, 1);  /* Give err */
                /* Don't use composite type, just the new prototype, so
                ** leave d->Stype alone.
                */
                }
            else /* Old-style REF followed by new-style proto */
                plcheck(d->Stype->Tproto);              /* Check new proto */
            }
        else
            {
            /* Compare existing proto with new non-proto ref/def */
            if (def)
                {
                /* New-style REF followed by old-style DEF, compare params */
                plcmpold(d->Spmnext, s->Shproto, s->Stype->Tproto);
                }
            else /* New-style ref/def followed by old-style REF */
                plcheck(s->Stype->Tproto);      /* Check previous proto */
            d->Stype = s->Stype;                /* Ensure proto retained */
            }
        }

    /* Force the symbol table entry to match current declaration.
    ** Note that Srefs was incremented by the symbol lookup, hence needs
    ** to be set back if this was only a ref-type declaration.
    ** Also, if the existing symbol was a definition, don't change its
    ** class.
    */
    s->Stype = d->Stype;        /* Force the type specification */
    s->Sflags |= d->Sflags & (SF_NORETURN | SF_NOINLINE | SF_INLINE);
    if (!def)                   /* If this decl was just a ref, */
        --(s->Srefs);           /* then usage count to undo lookup bump */
    if (s->Sclass != SC_EXTDEF && s->Sclass != SC_INTDEF)
        s->Sclass = d->Sclass;  /* Force the storage class */
    return s;
}

/* PLCMPARE - Compare two prototype parameter lists.
*/
static void
plcmpare(TYPE *t1, TYPE *t2, int flag) 
/* flag is 0 if both protos, 1 if t2 is fake proto */
{
    int n;
    if (t1 == t2)
        return;

    /* If pointers don't match, prototypes aren't compatible.  Examine
    ** more closely so can give helpful error message (or barf at ourselves
    ** if necessary)
    */
    if (t1 && t1->Tspec != TS_PARVOID && t2 && t2->Tspec != TS_PARVOID)
        for (n = 1; t1 && t2; t1 = t1->Tproto, t2 = t2->Tproto, ++n)
            if (t1->Tspec != t2->Tspec
              || (t1->Tspec == TS_PARAM && !cmputype(t1->Tsubt, t2->Tsubt)))
                {
                if (flag)
                    warn("Type of param #%d conflicts with prior def", n);
                else
                    error("Type of parameter #%d conflicts with prior prototype", n);
                return;
                }
    if (t1 != t2)
        {
        if (flag)
            warn("Number of params conflicts with prior def");
        else
            error("Number of params conflicts with prior prototype");
        }
    else
        int_warn("plcmpare: proto mismatch not found");
}

/* PLCMPOLD - Compare old-style parameter sym types against a
**      prototype parameter list.
*/
static void
plcmpold(SYMBOL *s, TYPE *nt, TYPE *ot)
{
    if (nt == ot)
        return;

    /* Prototypes don't match, try to give clever error msg */
    for (; s; s = s->Spmnext, nt = nt->Tproto, ot = ot->Tproto)
        if (!nt || !ot || nt->Tspec != ot->Tspec
          || (nt->Tspec == TS_PARAM && !cmputype(nt->Tsubt, ot->Tsubt)))
            {
            error("Type of parameter \"%s\" conflicts with prior prototype",
                     s->Sname);
            return;
            }
    if (nt || ot)
        error("Number of params conflicts with prior prototype");
}

/* PLCHECK - Check a prototype parameter list to ensure that all types
**      are compatible with default argument promotions.  Note that use
**      of an ellipsis param (TS_PARINF) is illegal in this context.
*/
static void
plcheck(TYPE *t)
{
    if (t->Tspec == TS_PARVOID)
        {
        if (t->Tproto)
            int_error("plcheck: void param");
        }
    else
        for (; t; t = t->Tproto)
            if (t->Tspec != TS_PARAM || !cmputype(t->Tsubt,convfparam(t->Tsubt)))
                {
                error("Conflict between prototype and default type reference");
                break;
                }
}

/* PDECLLIST - Parse the <decl-list> for an old-style function definition.
**      [dpANS 3.7.1, 3.6.2]
**
**      <decl-list> ::= *[<declaration>]
**
** The only storage-class spec allowed is "register".  No initializations
** are permitted.
*/
static void
pdecllist(void)
{
    TYPE *t;
    SYMBOL *s1, argbase, stemp;

    while (token != T_LBRACE && token != T_EOF) /* type-decl-list */
        {
        pbase(&argbase);        /* Parse storage class and type specifier */
        copysym(&stemp, &argbase);
        switch (stemp.Sclass)           /* Check storage class */
            {
            case SC_AUTO:
#if REGISTER_VARIABLES
                if (!use_registers)
                    {
#endif
                    stemp.Sclass = SC_ARG;
                    break;
#if REGISTER_VARIABLES
                    }
                /* else drop thru to error */
#endif

            default:
                error("Illegal storage class for function parameter");
            /* FALLTHROUGH */
            case SC_UNDEF:              /* Default becomes this. */
                stemp.Sclass = SC_ARG;
                break;
            case SC_RAUTO:              /* If "register" seen, */
                stemp.Sclass = SC_RARG; /* use right symbol class */
                break;
            }
        if ((t = stemp.Stype) == NULL)  /* Check type-specifier */
            {
            if (argbase.Sclass == SC_UNDEF)
                error("No type-specifier for parameter decl, assuming int");
            t = deftype;
            }

        /* Have <decl-specs> base, now parse each <declor> */
        while (1)
            {
            stemp.Sname[0] = '\0';      /* no symbol given yet */
            stemp.Stype = t;            /* Reset base type */
            for(;;)
                {
                if ((s1 = declarator(&stemp)) != NULL)  /* Get sym and rest of type */
                    break;
                if (token == T_COMMA)
                    {
                    error("Null parameter declarator");
                    nextoken();
                    continue;
                    }
                if (token == T_SCOLON)
                    {
                    nextoken();
                    break;
                    }
                error("Bad parameter declaration token");
                while (token != T_EOF && token != T_SCOLON && token != T_RBRACE)
                    nextoken();         /* Flush to probable end of stmt */
                break;          /* Will get two err msgs, but so what */
                }

            if (s1 == NULL)
                error("Null parameter declaration, expecting ident");
            else if (s1->Sclass != SC_ARG && s1->Sclass != SC_RARG)
                {
                error("Identifier \"%s\" not in function parameter list",
                        s1->Sname);             /* not an arg to this fn */
                if (s1->Sclass == SC_UNDEF)
                    freesym(s1);                /* Clean up if boo-boo */
                }
            else if (s1->Sflags & SF_PARAMDECL)
                {
                error("Duplicate parameter declaration: \"%s\"",s1->Sname);
                }
            else                /* Is arg, set type to what we parsed */
                {
                s1->Sclass = stemp.Sclass;      /* Maybe indicate register */
                s1->Sflags |= SF_PARAMDECL;     /* Say decl seen for arg */
                s1->Stype = convfparam(stemp.Stype);    /* Get right type */
                Set_Register(s1, SC_RARG, SC_ARG);
                }
            if (token != T_COMMA)
                break;  /* repeat through list */
            nextoken();                 /* skipping over commas */
            }
        expect(T_SCOLON);               /* decl line ends with a semicolon */
        }
}


/* MKPROTO - Make a prototype param type list out of a parameter symbol list.
**      Must be recursive for same reason that paramlist() is.
**      Note special handling for case of no symbols on list.
*/

static TYPE *
mkproto(SYMBOL *s)
{
    return s ? mkprox(s) : findptype(TS_PARVOID, (TYPE *)NULL, (TYPE *)NULL);
}
static TYPE *
mkprox(SYMBOL *s)
{
    return s ? findptype(TS_PARAM, mkprox(s->Spmnext), s->Stype) : NULL;
}

/* DATADEF - Parse top-level declaration that isn't a function definition.
**      [dpANS 3.5, 3.7.2]
**
**      <decl>          ::= <decl-specs> {init-decl-list} ';'
**
**      <init-decl-list> ::= {init-decl-list ','} <init-declor>
**      <init-declor>   ::= <declor> {'=' izer}
**
** Invoked by extdef() after determining that a declaration is not
** a function definition, to parse the rest of a top-level data declaration
** or function reference.
**      Current token is the first one after the first <declor>.
*/
static NODE *
datadef(SYMBOL *base, SYMBOL *s, SYMBOL *syment)
/* Parsed <decl-specs>
 * Default <decl-specs> and 1st <declor>
 * Symtab entry of identifier for 1st <declor>
 */
{
    SYMBOL defbase;
    NODE *root = NULL, *tail = NULL;

    /* Check out the storage class and type
    ** specifications.  At top level, some storage class or type specifier
    ** must have been given.
    */
    defbase.Scontents = base->Scontents;        /* Copy contents of base */
    switch (defbase.Sclass)
        {
        case SC_UNDEF:
            defbase.Sclass = s->Sclass;         /* Copy whatever default was */
            if (defbase.Stype == NULL)
                {
                error("Declaration without storage class or type-spec");
                }
            break;
        case SC_EXTREF:         /* "extern" */
        case SC_INTREF:         /* "static" */
        case SC_TYPEDEF:        /* "typedef" */
            break;
        default:
            error("Illegal top-level storage class");
            s->Sclass = defbase.Sclass = SC_EXLINK;     /* Use default */
            break;
        }
    if (defbase.Stype == NULL)
        defbase.Stype = s->Stype;       /* Copy whatever default was */

    decllist(base, &defbase, s, syment, &root, &tail);

    if (!expect(T_SCOLON) && token == T_RBRACE)
        nextoken();
    return (root);
}

/* LDECLLIST() - Parse local block declaration list
**      [dpANS 3.6.2, 3.5]
**
**      <decl-list> ::= <decl> {decl-list}
**
** This is only called by CCSTMT's compound() to parse the
** optional declaration-list at the start of a compound statement.
** Makes entries in the symbol and type tables, and returns a node
** pointer to a list of initializations that must be done.
**      Current token is the first of a possible <decl>.  If it is not
** a valid first token for a declaration, nothing is done and NULL is returned.
*/

NODE *
ldecllist(void)
{
    SYMBOL base, defbase;
    NODE *autodecls, *autotail; /* Pointers to list of inits for decls
                                ** within a block */

    if (!isdecl())
        return NULL;            /* Most common case -- no decls */

    autodecls = autotail = NULL;
    do
        {
        if ((token == T_STATIC_ASSERT || token == T_STATIC_ASSERT2))
            {
            staticassertdecl();
            continue;
            }

        /* If current token is start of a declaration, handle it. */
        pbase(&base);           /* Parse base storage-class and type */
                                /* Note all classes are OK */
        copysym(&defbase, &base);
        if (defbase.Sclass == SC_UNDEF)
            defbase.Sclass = SC_AUTO;   /* Default class is AUTO */
        if (defbase.Stype == NULL)
            defbase.Stype = deftype;

        /* Handle the local declaration, adding defs to the right list. */
        if (defbase.Sclass == SC_INTREF)        /* "static" */
            decllist(&base, &defbase, (SYMBOL *)NULL, (SYMBOL *)NULL,
                       &statdecls, &stattail);
        else
            decllist(&base, &defbase, (SYMBOL *)NULL, (SYMBOL *)NULL,
                            &autodecls, &autotail);

        expect(T_SCOLON);
        }
    while (isdecl())
        ;

    return(autodecls);
}

/* DECLLIST - Parse declarator list for a declaration, with possible izers.
**      [dpANS 3.5]
**
**      <init-decl-list> ::= <init-declor> {',' init-decl-list}
**      <init-declor>   ::= <declor> {'=' izer}
**
** This is called by datadef() for top-level declarations and
** by ldecllist() for block-level declarations.
** Note the provision for having already parsed the first declarator.
*/
static void
decllist(SYMBOL *base, SYMBOL *defbase, SYMBOL *d, SYMBOL *s,
        NODE **root, NODE **tail)
/* SYMBOL *base,         Base storage class & type-spec furnished 
 *      *defbase,        Same but completely defaulted as necessary 
 *      *d,              First parsed declarator (NULL if none) 
 *      *s;              Symbol table entry for d's ident 
 * NODE **root,          Addr of Root of declaration parse tree, if any 
 *      **tail;          Addr of Tail of parse tree, if any 
 */
{
    SYMBOL tempsym;
    NODE *n, *z;

    if (d == NULL)              /* Already parsed first declarator? */
        {
        d = &tempsym;           /* No, so do first one here. */
        copysym(d, defbase);
        s = declarator(d);
        if (s == NULL && token == T_SCOLON && (base->Sflags&SF_SIDEFF))
            return;             /* No declarators but have side-effect */
        }
    while (1)
        {
        if (s == NULL)
            error("Null declarator, expecting ident");
        else if ((n = dodecl(base->Sclass, d, s)) != NULL)
            {
            z = ndefl(N_DATA, n);
            if (*tail == NULL)                  /* Add parse result to tree */
                *root = *tail = z;              /* Either as 1st node */
            else
                *tail = (*tail)->Nright = z;    /* or at end of current tree */
            }
        if (token != T_COMMA)
            break;
        nextoken();
        copysym(d, defbase);
        s = declarator(d);
        }
    return;
}

/* ISDECL() - returns TRUE if current token is the start of a declaration,
**      i.e. the start of {declaration-specifiers}, one of:
**              {storage-class-specifier}
**              {type-specifier}
**              {type-qualifier}
*/
int
isdecl(void)
{
    return ((token == T_STATIC_ASSERT || token == T_STATIC_ASSERT2
        || token == T_ALIGNAS)
        || (csymbol != NULL && (
        (tok[token].tktype == TKTY_RWSC || tok[token].tktype == TKTY_RWTYPE)
        || (csymbol->Sclass == SC_TYPEDEF)
        )));
}

static void
staticassertdecl(void)
{
    INT v;
    char *msg = NULL;

    nextoken();
    expect(T_LPAREN);
    v = pconst();
    if (token == T_COMMA)
        {
        nextoken();
        if (token == T_SCONST)
            {
            msg = constant.csptr;
            nextoken();
            }
        else
            error("String literal expected in _Static_assert");
        }
    if (!v)
        {
        if (msg)
            error("Static assertion failed: %s", msg);
        else
            error("Static assertion failed");
        }
    expect(T_RPAREN);
    expect(T_SCOLON);
}

static TYPE *
typeofspec(void)
{
    TYPE *t;
    NODE *n;

    nextoken();
    expect(T_LPAREN);

    if (csymbol && (tok[token].tktype == TKTY_RWTYPE
        || csymbol->Sclass == SC_TYPEDEF))
        {
        t = typename();
        expect(T_RPAREN);
        return t;
        }

    n = asgnexpr();
    if (n == NULL || n->Ntype == NULL)
        t = deftype;
    else
        t = n->Ntype;
    expect(T_RPAREN);
    return t;
}

/* SYMALIGN_V5 - Decode an explicit alignment request from symbol flags. */
static INT
symalign_v5(long flags)
{
    if (flags & SF_ALIGN4)
        return 4;
    if (flags & SF_ALIGN2)
        return 2;
    if (flags & SF_ALIGN1)
        return 1;
    return 0;
}

/* PALIGNAS_V5 - Parse one C11 _Alignas alignment-specifier.
**
** KCC's PDP-10 object ABI has useful C-address-unit alignments 1, 2 and 4
** (9, 18 and 36 bits).  _Alignas(0) has no effect.  The type-name form uses
** the same canonical alignment calculation as _Alignof so declaration and
** query semantics cannot drift apart.
*/
static void
palignas_v5(SYMBOL *symp)
{
    TYPE *t;
    INT a, old;

    nextoken();
    if (token != T_LPAREN)
        {
        error("Expected '(' after _Alignas");
        return;
        }
    nextoken();

    if (csymbol && (tok[token].tktype == TKTY_RWTYPE
        || csymbol->Sclass == SC_TYPEDEF))
        {
        t = typename();
        a = alignoftype_v1(t);
        if (a == 0)
            error("Invalid type in _Alignas");
        }
    else
        a = pconst();

    expect(T_RPAREN);
    if (!symp)
        {
        error("_Alignas not allowed in type-name");
        return;
        }
    if (a == 0)
        return;
    if (a != 1 && a != 2 && a != 4)
        {
        error("Unsupported _Alignas value %ld", (INT)a);
        return;
        }

    old = symalign_v5(symp->Sflags);
    if (a <= old)
        return;
    symp->Sflags &= ~(SF_ALIGN1 | SF_ALIGN2 | SF_ALIGN4);
    if (a == 1)
        symp->Sflags |= SF_ALIGN1;
    else if (a == 2)
        symp->Sflags |= SF_ALIGN2;
    else
        symp->Sflags |= SF_ALIGN4;
}

/* PBASE(&sym) - Parse base of declaration (stg class & type)
**      Handles either <decl-specs>, if a symbol pointer is given,
**   or <spec-qual-list>, if no pointer is given.
**
**      [dpANS 3.5, 3.5.1, 3.5.2]
**
**      <decl-specs> ::=  *[<stg-class-spec> | <type-spec> | <type-qual>]
**
**      <spec-qual-list> ::= *[<type-spec> | <type-qual>]
**
**      <type-spec> ::= "void" | "char" | etc...
**      <type-qual> ::= "const" | "volatile"
**      <stg-class-spec> ::=
**                      "typedef" | "extern" | "static" | "auto" | "register"
*/

/* Flags to remember what we've already seen, for tokens that can combine
** with others in random order.
*/
#define PF_SIGNED       01
#define PF_UNSIGNED     02
#define PF_CHAR         04
#define PF_SHORT        010
#define PF_INT          020
#define PF_LONG         040
#define PF_DOUBLE       0100

static TYPE *
pbase(SYMBOL *symp)
{
    static char *errmsg = "Illegal combination of type-specifiers";
    int savnsdefs = nsdefs;             /* Remember # side-eff defs so far */
    TYPE *t = NULL, *nt = NULL;
    INT nflag, qflags = 0, tflags = 0;
    int chrsiz = 0, intsiz = 0, longcnt = 0;

    if (symp)
        {
        symp->Sname[0] = '\0';          /* init symbol */
        symp->Svalue = 0;               /* no val yet */
        symp->Sclass = SC_UNDEF;        /* no storage class */
        symp->Stype = NULL;             /* no type yet */
        symp->Sflags = 0;
        }

    /* First find all type-specs, type-quals, or stg-class-specs.
    ** Stop only when a token is seen that can't be one of these.
    */
    for ( ; ; )
        {
        if (token == T_ALIGNAS)
            {
            palignas_v5(symp);
            continue;
            }
        if (tok[token].tktype == TKTY_RWTYPE)
            {
            /* Look for reserved word type-qualifiers or type-specifiers */

            nt = NULL;

            switch (token)
                {
                case T_FORTRAN:

                    if( symp->Sflags & (TF_FORTRAN | TF_BLISS) )
                        error("Duplicate foreign language qualifier!");

                    symp->Sflags |= TF_FORTRAN;
                    nextoken();

                    continue;

                case T_BLISS:

                    if( symp->Sflags & (TF_FORTRAN | TF_BLISS) )
                        error("Duplicate foreign language qualifier!");

                    symp->Sflags |= TF_BLISS;
                    nextoken();

                    continue;

                case T_INTERRUPT:       /* FW 2A(52) */

                    if (symp->Sflags & TF_INTERRUPT)
                        error ("Duplicate interrupt qualifier");

                    symp->Sflags |= TF_INTERRUPT;
                    nextoken ();

                    continue;

                case T_CONST:           /* "const" type-qualifier */

                    if (qflags & TF_CONST)
                        error("Duplicate \"const\"");

                    qflags |= TF_CONST;
                    nextoken();

                    continue;

                case T_VOLATILE:                /* "volatile" type-qualifier */

                    if (qflags & TF_VOLATILE)
                        error("Duplicate \"volatile\"");

                    qflags |= TF_VOLATILE;
                    nextoken();

                    continue;

                case T_RESTRICT:
                case T_RESTRICT2:
                case T_RESTRICT3:
                    nextoken();             /* Accepted but not represented. */
                    continue;

                case T_INLINE:
                case T_INLINE2:
                case T_INLINE3:
                    symp->Sflags |= SF_INLINE;
                    nextoken();
                    continue;

                case T_EXTENSION:
                    nextoken();             /* Accepted and ignored. */
                    continue;

                case T_NORETURN:
                    symp->Sflags |= SF_NORETURN;
                    nextoken();
                    continue;

                case T_TYPEOF:
                case T_TYPEOF2:
                case T_TYPEOF3:
                    nt = typeofspec();
                    nflag = 1;              /* typeofspec consumed token. */

                    break;

                case T_BOOL:
                    nt = booltype;
                    nflag = 0;

                    break;

                case T_VOID:
                    nt = voidtype;
                    nflag = 0;

                    break;

                case T_FLOAT:
                    nt = flttype;
                    nflag = 0;

                    break;

                case T_STRUCT:
                    nt = tagspec(TS_STRUCT);
                    nflag = 1;

                    break;

                case T_UNION:
                    nt = tagspec(TS_UNION);
                    nflag = 1;

                    break;

                case T_ENUM:
                    (void) tagspec(TS_ENUM);
                    nt = inttype;
                    nflag = 1;

                    break;

                /* Enums always type "int", but have tags and syms defined. */

                case T_SIGNED:
                    nflag = PF_SIGNED;

                    break;

                case T_UNSIGNED:
                    nflag = PF_UNSIGNED;

                    break;

                case T_CHAR:
#if _DEF_CHAR8  
/* 6/91, chars default to size8
 * Note that if(!chrsiz){} below becomes dead code.
 */
                    chrsiz = 8;
#endif
                    nflag = PF_CHAR;

                    break;

                case T_SHORT:
                    nflag = PF_SHORT;

                    break;

                case T_INT:
                    nflag = PF_INT;

                    break;

                case T_LONG:
                    nflag = PF_LONG;
                    ++longcnt;

                    break;

                case T_DOUBLE:
                    nflag = PF_DOUBLE;

                    break;

                case T_CHAR6:
                    nflag = PF_CHAR;
                    chrsiz = 6;

                    break;

                case T_CHAR7:
                    nflag = PF_CHAR;
                    chrsiz = 7;

                    break;

                case T_CHAR8:
                    nflag = PF_CHAR;
                    chrsiz = 8;

                    break;

                case T_CHAR9:
                    nflag = PF_CHAR;
                    chrsiz = 9;

                    break;

                case T_CHAR18:
                    nflag = PF_CHAR;
                    chrsiz = 18;

                    break;

                case T_CHAR16:
                    nflag = PF_CHAR;
                    chrsiz = 16;

                    break;

                case T_INT32:
                    nflag = PF_INT;
                    intsiz = 32;

                    break;

                default:
                    int_error("pbase: unknown RWTYPE %Q", token);
                    nextoken();         /* Skip over, get next */

                    continue;
                }

            if (nt)                     /* Was type completely set? */
                {
                if (t || tflags)
                    error(errmsg);      /* Say bad typespec combo */
                t = nt;
                tflags = 0;
                /* Set flag if any side effects (tag or enum defined) */
                if (symp && savnsdefs != nsdefs)
                    symp->Sflags |= SF_SIDEFF;
                if (nflag)              /* If tagspec called, */
                    continue;           /* don't do another nextoken! */
                }
            else                        /* Nope, nflag must be set */
                {
                if (t || (tflags&nflag))
                    {
                    if (nflag == PF_LONG && (tflags&PF_LONG) && longcnt <= 2)
                        ;               /* long long: second long */
                    else
                        error(errmsg);  /* Say bad typespec combo */
                    }
                else
                    tflags |= nflag;
                }
            nextoken();                 /* On to next token */
            continue;
            }
        else if (tok[token].tktype == TKTY_RWSC)
            {
            /* Check storage class */
            if (!symp)
                error("Storage class specifier not allowed");
            else if (symp->Sclass != SC_UNDEF)  /* Already have one? */
                error("Only one storage class specifier allowed");
            else
                {
                if (t || tflags || qflags)
                    note("Storage class should come first in declaration");
                switch (token)
                    {
                    case T_AUTO:
                        symp->Sclass = SC_AUTO;
                        break;
                    case T_STATIC:
                        symp->Sclass = SC_INTREF;
                        break;
                    case T_EXTERN:
                        symp->Sclass = SC_EXTREF;
                        break;
                    case T_REGISTER:
                        symp->Sflags |= SF_REGSPEC;

        /* Later, have debugger routines in ccsym.c not depend on reg
         * values being on the stack. Then remove the debcsi < 1 in if().
         *
         * HAZARD: This looks screwy in light of the negative values of
         * debcsi.
         */
#if REGISTER_VARIABLES                      
                        if (use_registers && debcsi < 1) 
                    /* if not using  -r  or  -g=[debug,sprof,fnprof] */
                            {
                            symp->Sclass = SC_RAUTO;
                            symp->Sreg = -1;
                            }
                        else
#endif                          
                            symp->Sclass = SC_AUTO;
                        break;
                    case T_TYPEDEF:
                        symp->Sclass = SC_TYPEDEF;
                        break;
                    default:
                        int_error("pbase: unknown RWSC %Q", token);
                    }
                }
            nextoken();
            continue;           /* Skip over and get next */
            }

        /* Not a type-specifier keyword or storage-class, see if typedef,
        ** but ONLY if we haven't already seen any other type specifiers.
        ** Note that type qualifiers can be added in!  This requires a
        ** special check for function and array types, which can't have
        ** their types qualified except by typedefs.  But the qualifiers
        ** may follow the typedef, so we have to check after all's done.
        */
        if (!t && !tflags && csymbol && csymbol->Sclass == SC_TYPEDEF)
            {
            t = csymbol->Stype; /* Get the type */
            nextoken();
            continue;           /* Keep going, sigh */
            }
        break;                  /* No matches, stop loop! */
        }       /* End of moby loop */

    /* All specifiers gobbled, now see whether a type resulted.  If none,
    ** but tflags has stuff, we try to decipher the flag combos.
    */
    if (!t)
        {
        switch ((int) tflags)
            {
            case 0:
                if (!qflags && (!symp || symp->Sclass == SC_UNDEF))
                    return 0;           /* Nothing read at all */
                t = deftype;            /* OK to default to "int" */
                break;

        /* Chars have special KCC extension case */
            case PF_CHAR:
                if (!chrsiz)
                    {
                    t = chartype;
                    break;
                    }
            /* FALLTHROUGH */
            case PF_SIGNED|PF_CHAR:
                if (!chrsiz)
                    {
                    t = schartype;
                    break;
                    }
            /* FALLTHROUGH */
            case PF_UNSIGNED|PF_CHAR:
                if (!chrsiz)
                    {
                    t = uchartype;
                    break;
                    }
            /* Special extension, drop thru to handle */

                t = findctype((tflags&PF_SIGNED) ? TS_CHAR : TS_UCHAR,
                                    qflags | chrsiz, 1, (TYPE *)NULL);
                qflags = 0;             /* Don't re-do these */
                break;

            case PF_SHORT:
            case PF_SIGNED|PF_SHORT:
            case PF_SHORT|PF_INT:
            case PF_SIGNED|PF_SHORT|PF_INT:
                t = shrttype;
                break;
            case PF_UNSIGNED|PF_SHORT:
            case PF_UNSIGNED|PF_SHORT|PF_INT:
                t = ushrttype;
                break;
            case PF_INT:
            case PF_SIGNED:
            case PF_SIGNED|PF_INT:
                if (intsiz) {
                    t = findctype(TS_INT, qflags | intsiz, 1, (TYPE *)NULL);
                    qflags = 0;
                } else
                    t = inttype;
                break;
            case PF_UNSIGNED:
            case PF_UNSIGNED|PF_INT:
                if (intsiz) {
                    t = findctype(TS_UINT, qflags | intsiz, 1, (TYPE *)NULL);
                    qflags = 0;
                } else
                    t = uinttype;
                break;
            case PF_LONG:
            case PF_SIGNED|PF_LONG:
            case PF_LONG|PF_INT:
            case PF_SIGNED|PF_LONG|PF_INT:
                if (longcnt >= 2)
                    t = longlongtype;
                else
                    t = longtype;
                break;
            case PF_UNSIGNED|PF_LONG:
            case PF_UNSIGNED|PF_LONG|PF_INT:
                if (longcnt >= 2)
                    t = ulonglongtype;
                else
                    t = ulongtype;
                break;
            case PF_DOUBLE:
                t = dbltype;
                break;
            case PF_LONG|PF_DOUBLE:
                t = lngdbltype;
                break;

            default:
                error(errmsg);  /* Say bad typespec combo */
                if (nt == flttype || (tflags&PF_DOUBLE))
                    t = dbltype;
                else
                    t = inttype;
            }
        }

    if (qflags)                         /* Add qualifiers to the basic type */
        {
        int oflag = 0;


        if (t->Tspec == TS_FUNCT)       /* See typedef comments */
            advise ("Ignoring type-qualifiers for function type");
        else if (t->Tspec == TS_ARRAY)
            {
            /*
             * FEW, 2A(40), 5-August-92
             *
             * There was a problem with assigning qualifiers to typedef'ed
             * arrays.  The old code inadvertently attached the qualifier
             * to the typedef itself as well as to the instance of it.
             * The cure is qualarray (), a new recursive function that
             * creates a new chain of types as deep as necessary.
             */

            t = qualarray (t, qflags, &oflag);
            }
        else
            t = findctype (t->Tspec, qflags | (oflag = t->Tflag),
                                                t->Tsize, t->Tsubt);
        if ((oflag &= qflags) != 0)
            {
            if (clevel >= CLEV_STRICT)
                error("Redundant type qualifier \"%s\"",
                      (oflag&TF_CONST) ? "const" : "volatile");
            else
                warn("Redundant type qualifier \"%s\"",
                     (oflag&TF_CONST) ? "const" : "volatile");
            }
        }

    if (symp)
        {
        INT aflags;

        symp->Stype = t;
        if (symalign_v5(symp->Sflags) != 0 && t != NULL
          && alignoftype_v1(t) > symalign_v5(symp->Sflags))
            error("_Alignas specifies alignment weaker than natural alignment");
        aflags = lex_take_gnuattrs();
        symp->Sflags |= aflags;
        if ((aflags & SF_PACKED) && t != NULL
          && (t->Tspec == TS_STRUCT || t->Tspec == TS_UNION))
            packstruct(t);
        }

    return t;
    }

static
TYPE*
qualarray (TYPE* type, int flags, int* oldflags)
    {
    /*
     * FEW 2A(40) 5-August-92
     *
     * This is magic--not quite pure white, but a reasonably light grey.
     * The problem is that, when we are trying to apply qualifiers to the
     * base type of an n-dimensional array, we need to create n + 1 new
     * types: a new base type, plus a new parent type for each dimension
     * of the array.  Since we do not know what the base type is going to
     * be until we reach the bottom of the chain, we have to recurse.
     */

    TYPE*       subtype = type->Tsubt;
    int         findflags = flags;


    if (type->Tspec == TS_ARRAY)
        {
        subtype = qualarray (subtype, flags, oldflags);
        findflags = 0;
        }

    return findctype (type->Tspec, findflags | (*oldflags = type->Tflag),
            type->Tsize, subtype);
    }

/* TAGSPEC - Handles tag for struct/union/enum
*/
static TYPE *
tagspec(int typ)                /* TS_STRUCT, TS_UNION, or TS_ENUM */
{
    SYMBOL s, *tagsym, *osym;
    int tok;

    switch (nextoken())
        {
        case Q_IDENT:   /* Have tag ident */
            tagsym = symftag(osym = csymbol);   /* Get existing tag if any */
            if ((tok = nextoken()) == T_LBRACE)
                nsdefs++;                       /* Defining, say decl has side effs */
                                        /* (Note this clobbers csymbol) */
            if (tok == T_SCOLON)                /* reference to tag */
                {
                nsdefs++;                       /* not a null declaration */
                if (tagsym != NULL)
                    break;                      /* tag exists; use it as-is */
                /* else: no existing tag; fall through to create SC_UTAG */
                }
            break;

        case T_LBRACE:  /* No tag given, will have to make up internal one. */
            tagsym = osym = NULL;
            break;

        default:
            error("struct/union/enum not followed by tag or definition");
            while (1)
                switch (nextoken())
                    {
                    case T_EOF:
                    case T_RBRACE:
                    case T_SCOLON:
                        return NULL;
                    }
        }

    /* If a tag symbol already exists, check it to see whether we can use
    ** that one or need to create a new one.  This also checks for
    ** duplicate definitions.
    ** Note that if this is a reference (not a definition) then we always
    ** just use whatever tagsym is.
    */
    if (tagsym && token == T_LBRACE)    /* If this will be a definition */
        {
        if (tagsym->Sclass == SC_TAG)   /* Tag already defined? */
            {
            if (isdupsym(tagsym))       /* Dup def? */
                error("Duplicate tag definition: %S", tagsym);
            tagsym = NULL;              /* Force a new definition */
            }
        else if (tagsym->Sclass == SC_UTAG)     /* A ref already exists? */
            {
            if (!isdupsym(tagsym))      /* If not in same block, */
                tagsym = NULL;          /* don't satisfy the ref! */
            }
        }
    if (tagsym && tagsym->Stype->Tspec != typ)
        {
        error("Tag redefined with different type: %S", tagsym);
        tagsym = NULL;
        }

    /* If no tag (specified or internal) exists, make one and pretend it
    ** was seen as a forward reference (which it may well be).
    */
    if (tagsym == NULL)         /* Need tag symbol? */
        {
        if (osym)                       /* If already have an identifer, */
            tagsym = symqcreat(osym);   /* use that for quick sym setup! */
        else    /* No existing tag, invent one. */
                /* Note (safe) assumption that ident string is big enough */
            {
            {
            char *cp;
            cp = s.Sname;
            *cp++ = SPC_TAG;
            (void) declputd(cp, ++itags);
            }
            tagsym = creatsym(s.Sname); /* Make symbol of right scope */
            }
        tagsym->Sflags |= SF_TAG;
        tagsym->Sclass = SC_UTAG;       /* but with no defined body */
        tagsym->Ssmnext = NULL;         /* No members yet */
        tagsym->Srefs++;                /* This creation is a reference too */
        if (inproto)                    /* If created within fn prototype, */
            tagsym->Sflags |= SF_PROTOTAG;      /* flag it for ridlsym */
                                        /* Create new type for it */
        tagsym->Stype = findtype(typ, (TYPE *)tagsym);
        tagsym->Stype->Tsmtag = tagsym; /* Make sure type points to tag */
        }
    if (osym && osym->Sclass == SC_UNDEF)
        freesym(osym);                  /* Can flush orig sym now if unused */

    if (token != T_LBRACE)              /* If no definition, just return */
        return tagsym->Stype;

    /* Define the structure. */
    nextoken();                         /* Flush the left brace */
    if (typ == TS_ENUM)
        sdeclenum(tagsym);
    else
        tagsym->Stype->Tsize = sdeclstruct(tagsym, typ);
    if (eof)
        error("Unexpected EOF in declaration");
    else
        expect(T_RBRACE);               /* Now flush the right brace */

    tagsym->Sclass = SC_TAG;            /* Struct is now defined */
    return tagsym->Stype;
}


/* PACKSTRUCT - Apply the supported GNU packed aggregate subset.
**
** GNU packed lays integer members out in C address units (9-bit bytes).
** Recompute each member offset from that byte stream rather than retaining
** the normal KCC alignment.  Members whose value fits wholly inside one
** 36-bit word can still use KCC's existing negative Ssmoff byte encoding,
** so no new address representation is needed for them.
**
** Cross-word packed scalar members need split load/store generation and are
** deliberately diagnosed until that path is implemented.
*/
static void
packstruct(TYPE *t)
{
    SYMBOL *m;
    unsigned INT pbits = 0;
    int isunion;
    unsigned INT maxbits = 0;

    if (t == NULL || t->Tsmtag == NULL)
        return;
    if (t->Tspec != TS_STRUCT && t->Tspec != TS_UNION)
        return;
    isunion = (t->Tspec == TS_UNION);

    for (m = t->Tsmtag->Ssmnext; m != NULL; m = m->Ssmnext)
        {
        unsigned INT bits, storebits, bytes, startbit, wordbit, word;

        /* Packed bit-fields occupy exactly their declared number of bits.
        ** P=076,S=0 is reserved as an exact-bit-offset marker.
        */
        if (tisbitf(m->Stype))
            {
            bits = (unsigned INT)tbitsize(m->Stype);
            startbit = isunion ? 0 : pbits;
            m->Ssmoff = -(((INT)startbit << 12) | 07600L);
            if (isunion)
                {
                if (bits > maxbits) maxbits = bits;
                }
            else
                pbits += bits;
            continue;
            }

        if (tisinteg(m->Stype) && !tisdimode(m->Stype))
            {
            bits = (unsigned INT)tbitsize(m->Stype);
            if (bits == 0 || bits > TGSIZ_WORD)
                {
                error("GNU packed member has unsupported size");
                return;
                }
            bytes = (bits + TGSIZ_CHAR - 1) / TGSIZ_CHAR;
            storebits = bytes * TGSIZ_CHAR;
            startbit = isunion ? 0 : pbits;

            /* P=075,S=0 marks an ordinary packed scalar whose storage starts
            ** at an arbitrary bit offset.  Unlike a bit-field, the scalar
            ** still occupies its normal ceil(width/9) C address units; any
            ** spare low bits in the final address unit are padding.
            */
            if ((startbit % TGSIZ_CHAR) != 0)
                m->Ssmoff = -(((INT)startbit << 12) | 07500L);
            else
                {
                unsigned INT nbytes = startbit / TGSIZ_CHAR;
                word = startbit / TGSIZ_WORD;
                wordbit = startbit % TGSIZ_WORD;
                if (bits > TGSIZ_CHAR || wordbit + bits > TGSIZ_WORD)
                    m->Ssmoff = -(((INT)nbytes << 12) | 07700L);
                else
                    {
                    INT woff = (INT)word;
                    int boff = (int)wordbit;
                    m->Ssmoff = fldsize((int)bits, &woff, &boff);
                    }
                }
            if (isunion)
                {
                if (storebits > maxbits) maxbits = storebits;
                }
            else
                pbits += storebits;
            continue;
            }

        if (tispacked(m->Stype)
          && (m->Stype->Tspec == TS_STRUCT || m->Stype->Tspec == TS_UNION))
            bytes = m->Stype->Tbytes;
        else if (m->Stype->Tspec == TS_ARRAY
              && tispacked(m->Stype->Tsubt)
              && (m->Stype->Tsubt->Tspec == TS_STRUCT
                  || m->Stype->Tsubt->Tspec == TS_UNION))
            bytes = m->Stype->Tsize * m->Stype->Tsubt->Tbytes;
        else if (m->Stype->Tspec == TS_ARRAY
              && tisinteg(m->Stype->Tsubt)
              && !tisdimode(m->Stype->Tsubt))
            {
            unsigned INT ebits = (unsigned INT)tbitsize(m->Stype->Tsubt);
            if (ebits == 0 || ebits > TGSIZ_WORD)
                {
                error("GNU packed array member has unsupported element size");
                return;
                }
            bytes = m->Stype->Tsize
                  * ((ebits + TGSIZ_CHAR - 1) / TGSIZ_CHAR);
            }
        else
            {
            error("GNU packed currently supports integers, arrays, and nested packed aggregates");
            return;
            }

        storebits = bytes * TGSIZ_CHAR;
        startbit = isunion ? 0 : pbits;
        if ((startbit % TGSIZ_CHAR) != 0)
            {
            /* P=074,S=0 marks a nested aggregate at an exact bit offset.
            ** Direct child selections are flattened by CCSTMT.  Taking its
            ** address still requires a future software bit-pointer form.
            */
            m->Ssmoff = -(((INT)startbit << 12) | 07400L);
            }
        else
            m->Ssmoff = -(((INT)(startbit / TGSIZ_CHAR) << 12) | 07700L);
        if (isunion)
            {
            if (storebits > maxbits) maxbits = storebits;
            }
        else
            pbits += storebits;
        }

    if (isunion)
        pbits = maxbits;
    if (pbits == 0)
        return;

    t->Tbytes = (unsigned short)((pbits + TGSIZ_CHAR - 1) / TGSIZ_CHAR);
    t->Tsize = (t->Tbytes + (TGSIZ_WORD/TGSIZ_CHAR) - 1)
             / (TGSIZ_WORD/TGSIZ_CHAR);
    t->Tsmtag->Sflags |= SF_PACKED;
}


/* SDECLENUM - define enumeration type
*/
static void
sdeclenum(SYMBOL *tag)
{
    INT val;
    SYMBOL *s, *last;

    s = NULL;
    val = -1;                   /* start at zero (after pre-incrementing) */
    last = tag;
    for (;; nextoken())
        {
        if (token == T_RBRACE)
            {
            if (!s)
                {
                if (clevel >= CLEV_STRICT)
                    error("Empty enum definition list");
                else
                    warn("Empty enum definition list");
                }

            break;
            }
        if (token != Q_IDENT)
            {
            error("Identifier expected as enum constant");
            continue;
            }
        nsdefs++;               /* Enum constant def as "side effect" */
        s = csymbol;            /* get identifier */
        if (isdupsym(s))
            errdupsym(s);
        s = uniqsym(s);         /* Create or re-use symbol as needed */
        s->Sclass = SC_ENUM;    /* this is an enum constant */
        s->Stype = inttype;     /* acting like an int */
        s->Ssmtag = tag;        /* Remember tag defined within */
        s->Ssmnext = NULL;
        last->Ssmnext = s;      /* Link onto list of enum members */

        last = s;               /* MVS: fixed broken linked list! */

        if (nextoken() == Q_ASGN)
            {
            nextoken();         /* want specific value for this one */
            val = pconst();     /* so set it to given constant */
            s->Svalue = val;
            }
        else
            {
            if (val == INT_MAX)
                {
                if (clevel >= CLEV_STRICT)
                    error("ENUM value will exceed INT_MAX");
                else
                    warn("ENUM value will exceed INT_MAX");
                }
            s->Svalue = ++val;
            }
        if (token != T_COMMA)
            break;
        }
    /* Current token should now be T_RBRACE.  Caller will check. */
}


/* SDECLSTRUCT - Parse struct/union member list.
**      Returns size in words of the struct/union.
*/
static INT
sdeclstruct(SYMBOL *tag, int typ)
{
    SYMBOL *lastmem = tag;
    INT maxsize = 0, offset = 0;
    int boffset = 0, inbitf = 0;
    int savpok = paramok;

    paramok = 0;                        /* Not OK to collect param syms */
    while (!eof && token != T_RBRACE)
        {
        lastmem = sdeclaration(tag, lastmem, &offset, &boffset, &inbitf);
        if (typ == TS_STRUCT)
            continue;

        /* For union, each member starts over at beginning */
        if (boffset > 0)
            offset++;   /* Round out to full word */
        if (offset > maxsize)           /* Remember size of largest element */
            maxsize = offset;
        offset = boffset = inbitf = 0;  /* Start over with zero offset */
        }
    paramok = savpok;                   /* Restore saved paramok */

    /* A flexible array member requires at least one other named member.
    ** Unnamed bit-fields do not satisfy this C99 constraint.
    */
    if (typ == TS_STRUCT && tag->Ssmnext != NULL
      && tag->Ssmnext == lastmem
      && lastmem->Stype->Tspec == TS_ARRAY
      && lastmem->Stype->Tsize == 0)
        error("Flexible array member in otherwise empty struct");

    /* Return either total size (struct) or largest element size (union) */
    if (typ == TS_STRUCT)
        {
        if (boffset > 0)
            offset++;   /* Round offset up to full word */
        maxsize = offset;               /* Total size is current offset */
        }
    if (maxsize == 0
      && !(typ == TS_STRUCT && lastmem != tag
           && lastmem->Stype->Tspec == TS_ARRAY
           && lastmem->Stype->Tsize == 0))
        error("Empty %s declaration", typ == TS_STRUCT ? "struct" : "union");
    return maxsize;
}

/* ANONOFFADD_V7 - Add a physical anonymous-member offset to a child
** member offset.  Anonymous aggregates are word-aligned by the normal KCC
** layout rules, so OUTER is non-negative here.  Negative child offsets use
** KCC's encoded byte/bit-field representation, whose word component advances
** in units of 010000 (64*64).
*/
static INT
anonoffadd_v7(INT outer, INT inner)
{
    if (outer < 0) {
        int_error("anonymous aggregate has non-word physical offset");
        return inner;
    }
    return (inner >= 0) ? outer + inner : inner - outer * 010000L;
}

/* ANONPROMOTE_V7 - Install lookup-only aliases for the visible named members
** of an anonymous struct/union.  These aliases deliberately do not appear on
** the physical Ssmnext member chain, so sizeof/layout and positional
** initialization continue to see exactly one anonymous aggregate member.
** Ssmnext on an alias is instead used as a private pointer to the original
** named member for designated-initializer path reconstruction.
*/
static void
anonpromote_v7(SYMBOL *outertag, SYMBOL *sourcetag, INT baseoff)
{
    SYMBOL *m, *old, *a;

    for (m = sourcetag->Ssmnext; m != NULL; m = m->Ssmnext) {
        if (m->Sflags & SF_ANONMEM) {
            if (m->Stype != NULL && m->Stype->Tsmtag != NULL)
                anonpromote_v7(outertag, m->Stype->Tsmtag,
                               anonoffadd_v7(baseoff, m->Ssmoff));
            continue;
        }

        old = symfmember(m, outertag);
        if (old != NULL) {
            error("Duplicate struct member declaration through anonymous aggregate: %S", m);
            continue;
        }

        a = creatsym(m->Sname);
        a->Sflags |= SF_MEMBER | SF_ANONALIAS;
        a->Sclass = SC_MEMBER;
        a->Ssmoff = anonoffadd_v7(baseoff, m->Ssmoff);
        a->Stype = m->Stype;
        a->Ssmtag = outertag;
        a->Ssmnext = m;
    }
}

/* ANONCONTAINS_V7 / ANONROOT_V7 - Find the first physical anonymous member
** on the path from aggregate T to TARGETTAG.  This avoids persistent path
** objects: nested anonymous paths are recovered from the physical member
** chains only when a designated initializer needs them.
*/
static int
anoncontains_v7(TYPE *t, SYMBOL *targettag)
{
    SYMBOL *m;

    if (t == NULL || t->Tsmtag == NULL)
        return 0;
    if (t->Tsmtag == targettag)
        return 1;
    for (m = t->Tsmtag->Ssmnext; m != NULL; m = m->Ssmnext)
        if ((m->Sflags & SF_ANONMEM) && anoncontains_v7(m->Stype, targettag))
            return 1;
    return 0;
}

static SYMBOL *
anonroot_v7(TYPE *t, SYMBOL *leaf)
{
    SYMBOL *m;

    if (t == NULL || t->Tsmtag == NULL || leaf == NULL)
        return NULL;
    for (m = t->Tsmtag->Ssmnext; m != NULL; m = m->Ssmnext)
        if ((m->Sflags & SF_ANONMEM)
          && anoncontains_v7(m->Stype, leaf->Ssmtag))
            return m;
    return NULL;
}

/* SDECLANON_V7 - Add one physical anonymous aggregate member and promote its
** visible names into the containing aggregate's member namespace.
*/
static SYMBOL *
sdeclanon_v7(SYMBOL *tag, SYMBOL *prevsmem, TYPE *type, INT *offset,
             int *boffset, int *inbitf)
{
    SYMBOL *u;
    char name[IDENTSIZE];
    char *cp;
    INT offcode;

    if (type == NULL || (type->Tspec != TS_STRUCT && type->Tspec != TS_UNION)
      || type->Tsmtag == NULL || type->Tsmtag->Sclass != SC_TAG) {
        error("Anonymous member must have complete struct or union type");
        return prevsmem;
    }

    /* Anonymous members are aggregate objects and therefore enter normal
    ** word mode before placement.
    */
    if (*boffset) {
        *boffset = 0;
        (*offset)++;
    }
    *inbitf = 0;
    offcode = *offset;
    *offset += sizetype(type);

    cp = name;
    *cp++ = '%';
    *cp++ = 'A';
    (void)declputd(cp, ++ianonmem_v7);
    u = creatsym(name);
    u->Sflags |= SF_MEMBER | SF_ANONMEM;
    u->Sclass = SC_MEMBER;
    u->Ssmoff = offcode;
    u->Stype = type;
    u->Ssmtag = tag;
    u->Ssmnext = NULL;
    prevsmem->Ssmnext = u;
    prevsmem = u;

    if (type->Tflag & (TF_QUALS | TF_SIQUALS)) {
        if (type->Tflag & (TF_CONST | TF_SICONST))
            tag->Stype->Tflag |= TF_SICONST;
        if (type->Tflag & (TF_VOLATILE | TF_SIVOLAT))
            tag->Stype->Tflag |= TF_SIVOLAT;
    }

    anonpromote_v7(tag, type->Tsmtag, offcode);
    return prevsmem;
}

/* SDECLARATION - declare members of a struct or union
*/
static SYMBOL *
sdeclaration(SYMBOL *tag, SYMBOL *prevsmem, INT *offset, int *boffset,
        int *inbitf)
{
    SYMBOL base, tempsym, *u, *s;
    INT offcode, bsiz;
    int ts;

    pbase(&base);               /* Parse base storage-class and type */
    if (base.Stype == NULL)
        {
        error("No type-specifier for struct member, assuming int");
        base.Stype = deftype;
        }
    if (base.Sclass != SC_UNDEF)
        error("Storage class illegal for struct member");
    base.Sclass = SC_MEMBER;

    /* C11 anonymous struct/union member.  The type specifier itself must be
    ** an unnamed struct/union definition; a tagged declaration such as
    ** `struct tag;` remains a declaration with no member.
    */
    if (token == T_SCOLON && base.Stype != NULL
      && (base.Stype->Tspec == TS_STRUCT || base.Stype->Tspec == TS_UNION)
      && base.Stype->Tsmtag != NULL
      && base.Stype->Tsmtag->Sname[0] == SPC_TAG) {
        prevsmem = sdeclanon_v7(tag, prevsmem, base.Stype, offset, boffset, inbitf);
        nextoken();
        return prevsmem;
    }

    while (1)
        {
        if (token == T_COLON)
            {

            /*
            ** Colon without declarator before it - specifies space
            ** left for alignment.  Constant expression following
            ** colon is how much space, or zero to align to a word.
            */
            if (tag->Stype->Tspec == TS_UNION)
                {
                if (clevel < CLEV_ANSI)
                    error("Bit-field not allowed in union");
                else
                    advise("Unnamed bit-field meaningless in union");
                }
            nextoken();                 /* skip over colon */
                /* If not on word boundary and previous member was not a
                ** bitfield, force alignment.  There can be non-bitfield
                ** objects smaller than a word (eg chars).
                */
            if (*boffset && !(*inbitf))
                {
                *boffset = 0;   /* Force to word boundary */
                (*offset)++;
                }
            fldsize((int)pconst(), offset, boffset); /* parse & handle size */
            (*inbitf)++;                        /* Say in bitfield now */
            }
        else
            {

            /*
            ** Normal declarator.  Parse it, then check if there is
            ** a colon expression after it making it a bit field, or
            ** if it is a whole word expression.
            **
            ** For bitfields, the offset is encoded as follows:
            **  offcode % 07777 - high 12 bits of byte pointer to the field
            **  offcode >> 12   - word offset in struct
            ** and then the whole thing is negated.
            **
            ** Note that we let the bit offset remain at 36 rather
            ** than folding it to zero - the calculations are easier.
            */
            copysym(&tempsym, &base);
            u = declarator(&tempsym);
            if (u != NULL)
                tempsym.Stype = storagemaybitptrtype_v18(tempsym.Stype);
            if (u == NULL)              /* Check for case of no identifier */
                {
                error("Null declarator (expecting ident)");
                if (token == T_COLON)
                    continue;   /* Pretend no declarator */
                if (token == T_COMMA)
                    {
                    nextoken();
                    continue;
                    }
                break;                  /* Something bad, stop loop */
                }

            /* C99 flexible array members are represented by KCC as an
            ** unknown-sized array member.  Such a member is permitted only
            ** as the final named member of a struct; it is never permitted
            ** directly in a union.  Diagnose a previous flexible member as
            ** soon as another named member follows it.
            */
            if (prevsmem != tag
              && prevsmem->Stype->Tspec == TS_ARRAY
              && prevsmem->Stype->Tsize == 0)
                error("Flexible array member must be last in struct");
            if (tempsym.Stype->Tspec == TS_ARRAY
              && tempsym.Stype->Tsize == 0
              && tag->Stype->Tspec == TS_UNION)
                error("Flexible array member not allowed in union");

            if (token == T_COLON)
                {

                if (tempsym.Sflags & (SF_ALIGN1 | SF_ALIGN2 | SF_ALIGN4))
                    error("Alignment specifier not allowed on bit-field");

                /* Handle bitfield */
                if (tag->Stype->Tspec == TS_UNION
                  && clevel < CLEV_ANSI)
                    error("Bit-field not allowed in union");
                switch (tempsym.Stype->Tspec)
                    {
                    case TS_INT:
                        ts = TS_BITF;
                        break;
                    default:
                        error("Bit-field must be int, unsigned int, or _Bool");
                    /* FALLTHROUGH */
                    case TS_UINT:               /* Above error drops thru */
                        ts = TS_UBITF;
                        break;
                    }
                if (tisbool(tempsym.Stype))
                    ts = TS_UBITF;
                /* If not on word boundary and previous member was not a
                ** bitfield, force alignment.  There can be non-bitfield
                ** objects smaller than a word (eg chars).
                */
                if (*boffset && !(*inbitf))
                    {
                    *boffset = 0;       /* Force to word boundary */
                    (*offset)++;
                    }
                (*inbitf)++;                    /* Say now in bitfield */

                nextoken();                     /* move over colon */
                bsiz = pconst();                /* Parse size */
                offcode = fldsize((int)bsiz, offset, boffset);  /* Handle it */
                tempsym.Stype = findctype(ts,           /* Make bitfld type */
                        bsiz | (tempsym.Stype->Tflag&(TF_QUALS|TF_BOOL)),
                        1, (TYPE *)NULL);
                }
            else                        /* not bitfield */

                /* Handle normal non-bitfield member */

                /* Leave byte mode if necessary.
                ** If new member is not a byte object, or if byte mode is
                ** due to previous bitfield, always leave it, to force
                ** word alignment.
                */
                {
                if (tempsym.Stype->Tspec == TS_VOID)
                    {
                    error("Struct/union member cannot be void");
                    tempsym.Stype = deftype;
                    }

                bsiz = tisscalar(tempsym.Stype) ?       /* Get object size */
                        tbitsize(tempsym.Stype)         /* in bits if can */
                        : TGSIZ_WORD;           /* else force wd mode */

                /* GNU aligned(N) is expressed in C address units.  On the
                ** PDP-10 target one unit is 9 bits and useful alignment is
                ** capped at one 36-bit word.  Apply member alignment before
                ** the ordinary KCC byte/word placement rules.
                */
                if (tempsym.Sflags & SF_ALIGN4)
                    {
                    if (*boffset)
                        {
                        *boffset = 0;
                        (*offset)++;
                        }
                    }
                else if ((tempsym.Sflags & SF_ALIGN2) && *boffset)
                    {
                    if (*boffset <= TGSIZ_HALFWD)
                        *boffset = TGSIZ_HALFWD;
                    else
                        {
                        *boffset = 0;
                        (*offset)++;
                        }
                    }
                if (*boffset > 0                /* If in byte mode */
                  && ((bsiz >= TGSIZ_WORD)      /* and obj not byte */
                      || (*inbitf)))            /* or prev obj was bitfield */
                    {
                    *boffset = 0;       /* Then force word alignment */
                    (*offset)++;        /* and move up */
                    }
                *inbitf = 0;            /* Say no longer in bitfield */

                /* Now see if this type should be in byte mode or not.
                ** Sizes smaller than a word will either enter or remain
                ** in byte mode.  If the size is >= to a word, then the
                ** above code will have already taken us out of byte mode.
                */
                if (bsiz < TGSIZ_WORD)
                    {
                    if (*boffset % bsiz)        /* Align to byte bndry */
                        *boffset += (int) bsiz - (*boffset % (int) bsiz);
                    offcode = fldsize((int)bsiz, offset, boffset);
                    }
                else                    /* One or more words */
                    {
                    offcode = *offset;          /* starts at offset */
                    *offset += sizetype(tempsym.Stype); /* remember size */
                    }
                }

            /*
            ** Now we have parsed the declarator, and the encoded
            ** offset is in offcode.  Always make a new symbol for each
            ** structure member.
            */
            if ((s = symfmember(u, tag)) != NULL)       /* Check for dup */
                /* If a dup, just ignore current declaration. */
                {
                error("Duplicate struct member declaration: %S", s);
                if (u->Sclass == SC_UNDEF)
                    freesym(u);
                }
            else
                {
                u = symqcreat(u);               /* Get new sym w/right scope */
                u->Sflags |= SF_MEMBER;
                u->Sclass = SC_MEMBER;          /* Say it is a struct mem */
                u->Ssmoff = offcode;            /* with the given offset */
                u->Stype = tempsym.Stype;       /* Store type of member */
                u->Ssmtag = tag;                /* Point to parent structure */
                u->Ssmnext = NULL;              /* This is last mem so far */
                prevsmem->Ssmnext = u;  /* Point prev smem to this one */
                prevsmem = u;           /* This is new current smem */
                /* Pass on knowledge of any type qualifiers in struct! */
                if (u->Stype->Tflag & (TF_QUALS|TF_SIQUALS))
                    {
                    if (u->Stype->Tflag & (TF_CONST|TF_SICONST))
                        tag->Stype->Tflag |= TF_SICONST;
                    if (u->Stype->Tflag & (TF_VOLATILE|TF_SIVOLAT))
                        tag->Stype->Tflag |= TF_SIVOLAT;
                    }
                }
            }
        if (token != T_COMMA)
            break;
        nextoken();
        }
    expect(T_SCOLON);
    return prevsmem;                    /* return with latest pointer */
}

/* FLDSIZE - handle bitfield size specification
*/
static INT
fldsize(int bsiz, INT *offset, int *boffset)
{
    if (bsiz > TGSIZ_WORD || bsiz < 0)  /* range check */
        error("Bit field longer than word (%d bits)", TGSIZ_WORD);
    if (bsiz == 0 && *boffset > 0)      /* Zero size means round to wd bdry */
        *boffset = TGSIZ_WORD+1;        /* Hack so roundup is forced */

    *boffset += bsiz;                   /* advance by that many bits */
    if (*boffset > TGSIZ_WORD)  /* If not enough room */
        {
        *boffset = bsiz;                /* move bit offset to word bdy */
        (*offset)++;                    /* in next word */
        }
    /* Return encoded offset */
    return -(((*offset * 64) + TGSIZ_WORD - *boffset) * 64 + bsiz);
}

/* DECLARATOR - Parse a declarator with or without identifier
**      [dpANS 3.5.4, 3.5.5]
**
**      <declor> ::= {pointer} <direct-declor>
**
**      <direct-declor> ::= <ident>
**                              | '(' <declor> ')'
**                              | <direct-declor> '[' {const-expr} ']'
**                              | <direct-declor> '(' <param-type-list> ')'
**                              | <direct-declor> '(' {ident-list} ')'
**
**      <abstract-declor> ::= <pointer>
**                              | {pointer} <direct-abs-declor>
**      <direct-abs-declor> ::=
**                              | '(' <abstract-declor> ')'
**                              | {direct-abs-declor} '[' {const-expr} ']'
**                              | {direct-abs-declor} '(' {param-type-list} ')'
**
**      <pointer> ::= '*' {type-qual-list} {pointer}
**      <type-qual-list> ::= *[<type-qual>]
**
** A DECLARATOR specifies the identifier being declared and may also
** supply additional type information.  The resulting type is one of:
**      * decl          - Pointer to
**      ( decl )        - parens used to establish precedence
**      ident           - Simple declarator
**      decl [...]      - Array of
**      decl (...)      - Function (definition or reference)
**
** This function parses both <declor> and <abstract-declor>.  The main
** difficulty in distinguishing them is how to interpret a left-parenthesis;
** does it indicate a function, or merely group an inner declarator?
** The decision can be made based on the following token and on whether
** the identifier has already been seen or not:
**      Ident?  Next token      Declor  Interpretation  
**
**      Yes     '(' <ident>     Direct  Function, with <ident-list>.
**      Yes     '(' ')'         Direct  Function, no prototype info.
**      Yes     '(' <decl-spec> Direct  Function, with <param-type-list>.
**      Yes     '(' other       Direct  Illegal
**      No      '(' <ident>     Direct  Start of grouping
**      No      '(' ')'         Abstract Function, no prototype info.
**      No      '(' <decl-spec> Abstract Function, with <param-type-list>.
**      No      '(' other       Either  Start of grouping
**
**
** All declaration statements eventually call this routine.
**      normal = extdef (toplevel) & decllist (toplevel & local)
**      smems = sdeclaration (decl within structure definition)
**      params = pdecllist (old-style function param decls)
**              paramlist (new-style proto param decls)
**      abstract = typename (abstract declaration for casts & sizeof)
**
** The argument symbol pointer "d" must always point to a temporary symbol
** structure which is NOT in the symbol table.  The parsed type is
** returned in this symbol.  It may be the same as the original type.
**
** The return value is NULL if nothing was parsed or the declarator was
** an abstract declarator.  Otherwise, the return value is a pointer
** to the symbol table entry of the parsed identifier, and the declarator
** was a normal non-abstract declarator.
*/
#define DF_INABSTR 01   /* In abstract declarator */
#define DF_INPROTO 02   /* In function prototype */

#if 0   /* 8/91 shrink KCC */
#define DF_INSMEM 04    /* In struct/union declaration */
#define DF_TYPEDEF 010  /* In typedef */
#endif

static SYMBOL *
declarator(SYMBOL *d)
{
    SYMBOL *idsym;      /* Symtab ptr to parsed identifier */
    TYPE *pp;           /* Holds "derivation part" of type */
    int saveidsc;       /* Temp while parsing function params */
    INT dimension;      /* for parsing array dimension */

    d->Spmnext = d->Spmhead = NULL;     /* Init paramlist in case function */
    d->Svalue = 0;                      /* Ditto param style indicator */

    /* Parse <pointer> before ident part, and mung the base type directly. */
    while (token == Q_MPLY)
        {
        INT tflag = 0;
        for (;;)
            {
            switch (nextoken())
                {
                case T_CONST:
                    if (tflag&TF_CONST)
                        error("Duplicate \"const\"");
                    tflag |= TF_CONST;
                    continue;
                case T_VOLATILE:
                    if (tflag&TF_VOLATILE)
                        error("Duplicate \"volatile\"");
                    tflag |= TF_VOLATILE;
                    continue;
                case T_RESTRICT:
                case T_RESTRICT2:
                case T_RESTRICT3:
                    continue;
                }
            break;
            }
        d->Stype = pushsztype(TS_PTR, tflag, typsiztab[TS_PTR], d->Stype);
        }

    /* Now parse main part, <direct-declor> or <direct-abs-declor>.
    ** This is normally the ident,
    ** but it can be missing for abstract declarators.
    */
    switch (token)
        {
        case T_LPAREN:                  /* Left-paren is tricky case */
            if (nextoken() == T_RPAREN  /* Must examine next token */
              || isdecl())
                {
            /* '(' ')' -- Abstract function without prototype info */
            /* '(' <decl-spec> -- Abstract function with prototype info */
                idsym = NULL;
                pp = fundecl(d, DF_INABSTR);
                }
            else                        /* None of those, assume grouping. */
                {
                TYPE *savt = d->Stype;  /* Save type thus far */
                d->Stype = NULL;                /* and pretend it's null */
                idsym = declarator(d);  /* Now parse stuff in parens */
                pp = d->Stype;          /* Make base be derived part! */
                d->Stype = savt;                /* Then can restore saved type */
                expect(T_RPAREN);
            /* pp is now NULL if no derivation was added, or will be some
            ** sequence of "pointer to", "array of", or "function returning"
            ** with an ultimate NULL base which has to be filled in later.
            */
                }
            break;

        case Q_IDENT:
            idsym = csymbol;            /* Remember sym ptr for this ident */

            if (strcmp(idsym->Sname, "main") == 0)
                fn_main = (char) ~0;   /* detect main(REGISTER int argc, */ // FW KCC-NT

            nextoken();
            pp = NULL;                  /* No derived part */
            break;

        default:
            idsym = NULL;                       /* No identifier */
            pp = NULL;                  /* No derived part */
            break;
        }

    /*
     * Look for attempt to define function improperly.  This could happen
     * if the user tried to define a typedef function type, eg:
     *     typedef int FUNC(void);
     *     FUNC f
     *     { ...
     *     }
     */

    if (token == T_LBRACE)
        error("Illegal function definition");

    /* Check for function or array specifiers */
    for (;;)
        {
        switch (token)
            {
            case T_LPAREN:              /* Function definition or reference */
                d->Sflags |= lex_take_gnuattrs();
            /* Parse param list of function.  If idsym is a new symbol,
            ** we set it temporarily so as to avoid bashing our global
            ** function-name symbol if it turns out to have a parameter
            ** name the same as the function name!
            */
                if (idsym && ((saveidsc = idsym->Sclass) == SC_UNDEF))
                    idsym->Sclass = SC_EXLINK;  /* Fake out paramlist */
                nextoken();                             /* Start paramlist parse */
                pp = addpp(pp, fundecl(d,
                    (paramok && !inproto && d->Sclass != SC_TYPEDEF)
                    ? 0 : DF_INPROTO));
                if (idsym)                      /* Restore real class after fakeout */
                    {
                    idsym->Sclass = saveidsc;
                    idsym->Sflags |= d->Sflags; /* copy base flags */
                    }
                break;

            case T_LBRACK:              /* Array of something */

                if( d->Sflags & TF_FORTRAN )
                    warn("FORTRAN arrays not supported: will be C array");

                nextoken();

                dimension = 0;
                if (token != T_RBRACK)
                    {
                    NODE *bnd = asgnexpr();
                    bnd = evalexpr(bnd);
                    if (bnd != NULL && bnd->Nop == N_ICONST)
                        {
                        dimension = bnd->Niconst;
                        if (dimension <= 0)
                            error("array dimension const must be > zero");
                        pp = addpp(pp, pushsztype(TS_ARRAY, 0, dimension,
                                                 (TYPE *)NULL));
                        }
                    else
                        {
                        if (!inproto && (lsymhead == NULL || curfn == NULL))
                            {
                            error("Variable length array requires block scope");
                            pp = addpp(pp, pushsztype(TS_ARRAY, 0, 1,
                                                     (TYPE *)NULL));
                            }
                        else if (bnd == NULL || !tisinteg(bnd->Ntype))
                            {
                            error("Variable length array bound must have integer type");
                            pp = addpp(pp, pushsztype(TS_ARRAY, 0, 1,
                                                     (TYPE *)NULL));
                            }
                        else
                            {
                            if (!inproto)
                                vlamarkfn_v11();
                            pp = addpp(pp, vlamake_v11(bnd, (TYPE *)NULL));
                            }
                        }
                    }
                else
                    pp = addpp(pp, pushsztype(TS_ARRAY, 0, 0, (TYPE *)NULL));
                expect(T_RBRACK);
                break;
                                /* fall through */
            default:            /* Not array or function, we're done */
            /* Add base type to built-up derived type, return the result */
                if (pp)
                    d->Stype = addpp(pp, d->Stype);
                d->Sflags |= lex_take_gnuattrs();
                return idsym;
            }
        }
}

/* FUNDECL - Parse function parameter list declarator
**              This may be either <param-type-list> or <ident-list>.
**      [dpANS 3.5.4]
**
**      <param-type-list> ::= <param-list> {',' "..."}
**      <param-list> ::= <param-decl> *{',' <param-decl>}
**      <param-decl> ::= <decl-specs> {<declor> | <abs-declor>}
**      <ident-list> ::= <ident> *{',' <ident>}
**
** Only called by declarator() to handle a function type declaration.
** Current token is the first one following the opening left-paren.
** On return, current token is first thing after the right paren.
** (if error, current token is whatever halted the parse).
** Returns a pointer to the resulting function type, with a NULL
**      return-value subtype which is filled in later by declarator().
**
** If any valid parameter identifiers were seen, the following values are also
** returned in the declarator symbol struct:
**      Svalue - 0 if old-style identlist, N+1 if new-style prototype,
**                      where N is the # of parameters ("void" and "..."
**                      are not counted).
**      Spmnext - pointer to 1st parameter in param list
**      Spmhead - start of local symbol block that was opened for the
**              prototype scope, as returned by beglsym().  This block will
**              still be active on return, and must be closed by the caller
**              eventually.
*/

static TYPE *
fundecl(SYMBOL *d, int dflag)
{
    TYPE *proto = NULL;

    if (token == T_RPAREN)              /* Quick check for most common case */
        nextoken();                     /* No param or ident list */
    else
        {
        struct protostate ps;
        SYMBOL *blkhead;

        blkhead = beglsym();            /* Begin a local symbol block */
        ++inproto;                      /* Now inside prototype scope! */
        ps.nparams = 0;                 /* No params yet */
        ps.shead = ps.stail = NULL;     /* Empty param sym list */
        if (!isdecl())
            {
            if (dflag)
                error("Bad context for old-style function parameters");
            pidentlist(&ps);            /* Handle old identlist form */
            }
        else
            {
            proto = paramlist(&ps);     /* Handle new prototype form */
            ps.nparams += 1;            /* Ensure new-style indicator is set */
            }
        --inproto;                      /* Out of proto scope... */
        if (dflag)              /* If wasn't OK to collect params, then */
            ridlsym(blkhead);   /* quietly flush all syms defd in prototype */
        else                    /* Top-level, so remember param idents! */
            {
            if (!ps.shead)      /* If no actual parameter idents, then */
                ridlsym(blkhead);       /* flush sym block anyway */
            if (d->Spmnext)
                {
                int_error("fundecl: already parsed funct params");
                ridlsym(blkhead);
                }
            else                /* Keep local sym block active */
                {
                d->Spmnext = ps.shead;
                d->Spmhead = blkhead;
                d->Svalue = ps.nparams; /* Remember # of new-style params */
                }
            }

        /* Done, must have ended in right-paren */
        if (token == T_RPAREN)
            nextoken();
        else
            {
            error("Bad syntax in function parameter list");
            for (;;)
                {
                switch (nextoken())
                    {
                    default:
                        continue;
                    case T_EOF:
                    case T_RBRACE:
                    case T_SCOLON:
                    case T_RPAREN:
                        break;
                    }
                break;
                }
            }
        }
    return findftype((TYPE *)NULL, proto);
}

/* PIDENTLIST - Parse old ident-list form of function params.
**      Note ps->nparams is always left 0 as "old-style" flag.
*/
static void
pidentlist(struct protostate
*ps)
{
    SYMBOL *s;

    /* Parse parameter list as a simple ident-list. */
    while (token == Q_IDENT && csymbol->Sclass != SC_TYPEDEF)
        {
        if (csymbol->Sclass == SC_ARG)  /* Already a parameter? */
            error("Duplicate parameter: \"%s\"", csymbol->Sname);
        s = uniqsym(csymbol);   /* make local sym for param */
        if (ps->shead == NULL)  /* Save pointer to this param */
            ps->shead = s;      /* either in head of list */
        else
            ps->stail->Spmnext = s;     /* or in last param so far */
        ps->stail = s;          /* Move on to end of chain */
        s->Spmnext = NULL;      /* This one is now the last */
        s->Sclass = SC_ARG;     /* Say it's a function parameter */
        s->Stype = deftype;     /* and (int) unless declared later */

        if (nextoken() != T_COMMA)
            break;
        nextoken();
        }
}

/* PARAMLIST() - Parse parameter-type-list (function prototype declaration)
**
** Recursion is needed in order to come up with a canonical
** prototype list without lots of wasteful duplication.  The problem is
** that calls to findptype() must already know what the Tproto value
** is before a pure type can be found/created.  So we have to parse the
** entire parameter list before the prototype can be constructed!
** Stack usage is minimized by using as few args and locals as possible.
*/
static TYPE *
paramlist(struct protostate
*ps)
{
    SYMBOL *s;
    TYPE *t;

    if (token == T_RPAREN)
        return NULL;
    if (token == T_ELPSIS)
        {
        if (!ps->nparams)
            error("Ellipsis must follow a parameter");
        return findptype(TS_PARINF,
                (nextoken() == T_COMMA ? (nextoken(), paramlist(ps)) : NULL),
                NULL);
        }

    pbase(&(ps->decl));                 /* First get <decl-specs> */
    if (ps->decl.Sflags & (SF_ALIGN1 | SF_ALIGN2 | SF_ALIGN4))
        error("_Alignas not allowed on parameter declaration");
    switch (ps->decl.Sclass)
        {
        case SC_AUTO:
#if REGISTER_VARIABLES
            if (!use_registers)
                {
#endif
                ps->decl.Sclass = SC_ARG;
                break;
#if REGISTER_VARIABLES          
                }
                /* else drop thru to error */
#endif

        default:
            error("Only storage class allowed for param is \"register\"");
                /* Drop thru to pretend normal arg */
        /* FALLTHROUGH */
        case SC_UNDEF:
            ps->decl.Sclass = SC_ARG;
            break;
        case SC_RAUTO:
            ps->decl.Sclass = SC_RARG;
            break;
        }
    if (ps->decl.Stype == NULL)
        {
        if (token == T_COMMA || token == T_RPAREN)
            error("Null parameter declaration; expecting <type>");
        else
            error("No type-specifier for parameter, assuming int");
        ps->decl.Stype = deftype;
        }
    s = declarator(&(ps->decl));        /* Finish type, maybe get ident */
    t = ps->decl.Stype;

    /* Now check type for conversions, and add to type list */
    switch (t->Tspec)
        {
        case TS_VOID:                   /* Check for "(void)" */
            if (!ps->nparams && !s && token == T_RPAREN)
                return findptype(TS_PARVOID, (TYPE *)NULL, (TYPE *)NULL);
            error("Function parameter cannot be void");
            t = deftype;
            break;
        case TS_FUNCT:                  /* Function of T becomes */
            t = findtype(TS_PTR, t);    /* pointer to function of T */
            break;
        case TS_ARRAY:                  /* Array of T becomes */
            t = findtype(TS_PTR, t->Tsubt);     /* pointer to T */
            break;
        default:
            ;   /* do nothing */
        }

    /* Function-boundary pointers to sub-word exact-width integers may
    ** arrive either as their ordinary PDP-10 byte-pointer representation
    ** or as KCC's S=1 logical pointer.  The raw one-word value remains
    ** self-describing through its S field.
    */
    t = fnmaybitptrtype(t);

    /* Add parameter symbol to list.  Always need to build list, even if
    ** discarding later, so we can diagnose duplicate param defs.
    */
    if (s)
        {
        if (s->Sclass == SC_ARG /* Already a parameter? */
            || s->Sclass == SC_RARG)
            {
            if (isdupsym(s))            /* See if defined in current block */
                error("Duplicate parameter: \"%s\"", s->Sname);
            }
        s = uniqsym(s);                 /* Make local sym for param */
        if (ps->shead == NULL)          /* Save pointer to this param */
            ps->shead = s;              /* either in head of list */
        else
            ps->stail->Spmnext = s;     /* or in last param so far */
        ps->stail = s;                  /* Move on to end of chain */
        s->Spmnext = NULL;              /* This one is now the last */
        s->Stype = t;                   /* Set type, and */
        s->Sclass = ps->decl.Sclass;    /* say it's a function parameter */
        Set_Register(s, SC_RARG, SC_ARG);
        }

    ps->nparams++;
    return findptype(TS_PARAM,
        ((token == T_COMMA) ? (nextoken(), paramlist(ps)) : NULL),
        t);
}

/* Exact-width sub-word integer pointers can originate inside a packed
** object at a non-byte-aligned bit position.  Function boundaries preserve
** the raw one-word pointer and mark the type as representation-polymorphic.
*/
static int
fnmaybitptrtarget(TYPE *t)
{
    INT bits;

    if (t == NULL || !tisinteg(t))
        return 0;
    bits = tbitsize(t);
    /* Ordinary C char pointers define KCC's external byte-address ABI and
    ** always use the native character-pointer representation at function
    ** boundaries.  Representation-polymorphic function values are required
    ** only for explicitly non-native exact-width sub-word pointer types.
    ** This keeps normal char * calls assignable to ordinary objects while
    ** preserving S=1 logical pointers for KCC exact-width extensions.
    */
    if (tischar(t) && bits == TGSIZ_CHAR)
        return 0;
    return bits > 0 && bits < TGSIZ_WORD;
}

static TYPE *
fnmaybitptrtype(TYPE *t)
{
    if (t != NULL && t->Tspec == TS_PTR && t->Tsubt != NULL
      && fnmaybitptrtarget(t->Tsubt))
        return findctype(TS_PTR, t->Tflag | TF_MAYBITPTR,
                         t->Tsize, t->Tsubt);
    return t;
}

/* STORAGEMAYBITPTRTYPE_V18 - Preserve exact-width packed pointer words in
** persistent pointer objects.
**
** A pointer to a non-native exact-width sub-word integer may contain either
** an ordinary PDP-10 byte pointer or KCC's S=1 logical packed pointer.  The
** raw word is self-describing through its S field, so static-storage pointer
** objects and aggregate pointer members must retain TF_MAYBITPTR in every
** translation unit that accesses them.
*/
static TYPE *
storagemaybitptrtype_v18(TYPE *t)
{
    if (t != NULL && t->Tspec == TS_PTR && t->Tsubt != NULL
      && fnmaybitptrtarget(t->Tsubt))
        return findctype(TS_PTR, t->Tflag | TF_MAYBITPTR,
                         t->Tsize, t->Tsubt);
    return t;
}

/* ADDPP - add type to inside of nesting
**      Only invoked by declarator().
*/
static TYPE *
addpp(TYPE *pp, TYPE *t)
{
    /*
    ** This takes a base-less type structure in pp, and returns the
    ** result of replacing the NULL where the base should be with t.
    ** Thus it is the inverse of pushsztype(), adding the new type
    ** at the base of the structure rather than at the top.
    **
    ** I realize the recursive definition below may look messy,
    ** but an iterative definition of this function would be worse...
    */

    if (pp == NULL)
        return t;
    if (tisvla(pp))
        return vlaclone_v11(pp, addpp(pp->Tsubt, t));
    return pushsztype(pp->Tspec, pp->Tflag, pp->Tsize, addpp(pp->Tsubt, t));
}

/* PUSHTYPE - Construct a derived declarator type, checking for validity.
**
** As per [H&S 4.5.5] checks for the following illegal type
** combinations:
**      (1) Any type with "void" except "... function returning void"
**              Note [dpANS] also permits "pointer to void".
**      (2) "Array of function ..."
**      (3) "Function returning array of ..."
**      (4) "Function returning function ..."
** There is also one other special case (5) that this list omits: as per
** [H&S 4.5.3 and 5.5.3], a N-dimensional array must always have
** all of the last N-1 sizes specified; only the first dimension's size
** can be omitted.
**
**      If the type combination is illegal, some plausible type is
** substituted, not so that something useful will be compiled but so
** that no bizarre "types" will unduly interfere with scanning the rest of the
** source file for further errors.
**      Note that "void" is OK all by itself as a type.  The checks here
** only look for illegal COMBINATIONS of types.
*/
static TYPE *
pushsztype(int typ, INT flags, INT siz, TYPE *ptr)
{
    if (ptr != NULL)
        switch (typ)    /* If making a combination, check it */
            {
            default:    /* Top type must be one of array, funct, or ptr! */
                int_error("pushsztype: bad top type %d", typ);
                ptr = NULL;
                break;

            case TS_PTR:                        /* Ptr to most anything is OK */
                if (ptr->Tspec == TS_VOID       /* But (1) allow "ptr to void" */
                  && clevel < CLEV_ANSI)        /* only if level high enough */
                    warn("Restricted type - pointer to void");
                break;

            case TS_ARRAY:              /* Check for (2) and (5) */
                switch (ptr->Tspec)
                    {
                    case TS_VOID:
                        error("Illegal type - array of void");
                        ptr = inttype;  /* Use int instead */
                        break;
                    case TS_ARRAY:              /* Array of arrays OK if size given */
                                        /* array[][x] ok, array[x][] bad */
                        if (ptr->Tsize == 0 && !tisvla(ptr))
                            {
                            error("Illegal type - array of unknown-sized array");
                            ptr = inttype;      /* Use int instead */
                            }
                        break;
                    case TS_FUNCT:              /* Array cannot have functions */
                        error("Illegal type - array of function");
                        ptr = inttype;  /* Lose, substitute "int" */
                    }
                break;

            case TS_FUNCT:              /* Check for (3) and (4) */
                if (ptr->Tspec == TS_ARRAY || ptr->Tspec == TS_FUNCT)
                    {
                    /* Function cannot return array or function */
                    error("Illegal type - function returning %s",
                        ptr->Tspec == TS_ARRAY ? "array" : "function");
                    ptr = voidtype;     /* Lose, substitute void */
                    }
                break;
            }

    if (typ == TS_FUNCT)
        ptr = fnmaybitptrtype(ptr);

    /* Now hash up the actual type and return the canonicalized version */
    return findctype(typ, flags | typbsiztab[typ], siz, ptr);
}

/* DODECL - Do processing for a parsed declarator
**      Called by decllist() to process the results of parsing a declarator.
** This IS called for:
**      toplevel declarations
**      local (at head of block) declarations
** This is NOT called for:
**      abstract declarators - handled by typename().
**      function parameter declarations - handled by funcdef().
**      structure/union declarations - handled by sdeclaration().
** This also checks for an initializer, and handles it if one
** exists.  Note the first two args refer to temporary symbol structures
** which are NOT in the symbol table.
*/

static NODE *
dodecl(int baseclass, SYMBOL *d, SYMBOL *s)
/* Whatever was actually parsed as storage class, Declarator identifier, with 
 * sc and type all set up, and Symbol table entry for the identifier 
 */
{
    SYMBOL *ns;
    TYPE   *nt;
    NODE   *z;

    /* Static-storage pointer objects form a translation-unit boundary just
    ** like function arguments do.  Mark exact-width sub-word pointers here
    ** before linkage/composite-type processing so every declaration of an
    ** externally visible object agrees on the representation-polymorphic
    ** type.  Automatic objects keep the existing local specialization.
    */
    if (d->Sclass == SC_EXLINK || d->Sclass == SC_EXTREF
      || d->Sclass == SC_INTREF)
        d->Stype = storagemaybitptrtype_v18(d->Stype);

    if (d->Sflags & (SF_ALIGN1 | SF_ALIGN2 | SF_ALIGN4))
        {
        if (d->Stype->Tspec == TS_FUNCT)
            error("_Alignas not allowed on function declaration");
        if (d->Sclass == SC_TYPEDEF)
            error("_Alignas not allowed on typedef declaration");
        if (d->Sflags & SF_REGSPEC)
            error("_Alignas not allowed on register object");
        }

    /* Symbol table entry will always exist, because the lexer will 
    ** have created it if necessary as a global symbol wit symbol class 
    ** SC_UNDEF. If the new symbol actually should be a local one then 
    ** it needs to be flushed and then created again in the right place.
    */

    /* Check for a function reference, which needs special handling */
    if (d->Stype->Tspec == TS_VOID)     /* Declaring a void object? */
        {
        if (d->Sclass != SC_TYPEDEF)
            {
            error("Cannot declare %S (or any object) as void", s);
            d->Stype = deftype;
            }
        }
    else
        {
        if ((d->Stype->Tspec == TS_FUNCT)
                || (d->Stype->Tspec == TS_PTR
                    && d->Stype->Tsubt->Tspec == TS_FUNCT))
            {
            /* Take care of open local sym block if necessary */

            if (d->Spmnext)             /* Param sym blk active? */
                {
                if (d->Svalue == 0)     /* Old-style function identlist? */
                    error ("Bad syntax - function parameters without body");

                ridlsym (d->Spmhead);   /* Yes, flush syms from paramlist */
                d->Spmnext = d->Spmhead = NULL; /* Be neat just in case */
                }

            /*
             * FW 2A(43) 01-Mar-93
             *
             * The pointer-to-function case must be screened out
             * here, because we may be declaring a pointer-to-function
             * and initializing it, but this code assumes it will only
             * be reached for function declarations, which cannot be
             * initialized.
             */
             
            if ((d->Stype->Tspec == TS_FUNCT) && (d->Sclass != SC_TYPEDEF))
                {
                funchk(0, baseclass, d, s);     /* Yep, go handle */
                return NULL;            /* function decl doesn't use storage*/
                }
            }
        }

    /* Real variable or typedef, do things depending on class */
    switch (d->Sclass)
        {
        case SC_TYPEDEF:                /* Type definition */
            if (isdupsym(s))    /* This symbol already def'd in same block? */
                errdupsym(s);
            s = uniqsym(s);             /* Ensure sym is fresh new one */
            s->Sclass = d->Sclass;      /* Fill in necessary parts of sym */
            s->Stype = d->Stype;
            if (vlacontains_v11(s->Stype)) {
                NODE *vz;
                vlamarkfn_v11();
                vlabindtype_v11(s->Stype);
                vla_declregion_v12(); /* declaration region, no storage */
                vz = ndeft(N_VLA, s->Stype);
                vz->Nid = s;
                return ndeflr(N_IZ, ndefident(s), vz);
            }
            return NULL;                /* no initialization or storage */

        case SC_AUTO:           /* local extent variable, in function */
        case SC_RAUTO:
            if ((ns = isdupsym(s)) != NULL)
                {
                if (ns->Sclass == SC_TYPEDEF)
                    {
                    error("typedef type cannot be modified");
                    if (token == Q_IDENT)
                        {
                        s = csymbol;
                        nextoken();
                        }
                    }
                else
                    error("Duplicate definition: \"%s\"", s->Sname);
                }

            s = uniqsym(s);             /* Always make local cell */
            s->Sclass = d->Sclass;      /* Fill in necessary parts of sym */
            s->Stype = d->Stype;
            s->Sflags |= d->Sflags & (SF_ALIGN1 | SF_ALIGN2 | SF_ALIGN4);
            Set_Register(s, SC_RAUTO, SC_AUTO);

            break;                      /* Go check for izer */

    /* External linkage, use existing sym if any.
    ** No storage class given, treat basically as if "extern" with
    ** possibility of being a tentative definition.
    ** Not sure what to do about re-using Sflags.
    */
        case SC_EXLINK:         /* No storage class given, file-scope */
            if (lsymhead)               /* Should always be at file scope */
                {
                int_error("dodecl: scope mismatch");
                return NULL;
                }
            s = symfxext(s);    /* Ensure an SC_XEXTREF is made visible */
            if (s->Sclass != SC_UNDEF)  /* Check out type if any */
                {
                --(s->Srefs);           /* Linkage isn't a real ref */
                if ((nt = tcomposite(d->Stype, s->Stype)) != NULL)
                    d->Stype = nt;              /* Types compatible, use composite */
                else
                    errtwotyp(d, s);    /* Types not compatible, barf */
                }
            else if (!mapextsym(s))     /* New, so set Smaplab now */
                error("External link name duplicated: \"%s\"", s->Sname);
            s->Stype = d->Stype;                /* Clobber sym type to new one */

            if (token != Q_ASGN)
                switch (s->Sclass) /* If just a ref, simple */
                    {
                    case SC_INTDEF:
                    case SC_INTREF:
                    case SC_INLINK:
                        error("Linkage conflict (was internal)");
                        s->Sclass = (s->Sclass==SC_INTDEF ? SC_EXTDEF : SC_EXLINK);
                        return NULL;

                    default:
                        errdupsym(s);   /* Barf & fall thru */
                    /* FALLTHROUGH */
                    case SC_UNDEF:
                    case SC_XEXTREF:
                    case SC_EXTREF:
                        s->Sclass = SC_EXLINK;
                    /* FALLTHROUGH */
                    case SC_EXTDEF:
                    case SC_EXLINK:
                        return NULL;
                    }
        /* Handle initializing definition */
            switch (s->Sclass)
                {
                case SC_INTDEF:
                case SC_EXTDEF:
                    error("Multiple initialization of %s", s->Sname);
                    break;              /* Do it anyway... */
                case SC_INTREF:
                case SC_INLINK:
                    error("Linkage conflict (was internal)");
            /* Fall thru to handle as if extern */
                /* FALLTHROUGH */
                case SC_UNDEF:
                case SC_XEXTREF:
                case SC_EXTREF:
                case SC_EXLINK:
                    s->Sclass = SC_EXTDEF;
                    break;
                default:
                    errdupsym(s);
                    s->Sclass = SC_EXTDEF;      /* Sigh!  Smash it. */
                    break;
                }
            break;                              /* Go parse izer now */

    /* "extern" - existing or external linkage, use existing sym if any.
    ** If in block scope and object ident already belongs to a
    ** block-scope symbol, we:
    ** (1) see if a file-scope symbol exists (if so, use that one)
    ** (2) see if existing sym is external ref (if so, use that)
    ** (3) generate new file-scope symbol with SC_EXTREF.
    ** Again, not sure how to re-use Sflags.
    */
        case SC_EXTREF:         /* "extern" given, file or block scope */
            s = symfxext(s);    /* Ensure an SC_XEXTREF is made visible */
            if (lsymhead && (s->Sflags&SF_LOCAL))
                {
                if ((ns = findgsym(s)) != NULL)/* If file-scope def/ref exists, */
                    s = ns;                     /* use that! */
                else if (s->Sclass == SC_EXTREF)
                    ;           /* OK if a block-scope extern ref */
                else
                    {
                    if (isdupsym(s))    /* If was defined in this block, */
                        errdupsym(s);   /* barf! */
                    s = uniqsym(s);             /* Create/re-use sym, now SC_UNDEF */
                    }
                }
        /* "s" now points to the sym linked to.  If it has any linkage, we
        ** copy that, otherwise we use external linkage.
        */
            if (s->Sclass != SC_UNDEF)  /* Check out type if any */
                {
                --(s->Srefs);           /* Linkage isn't a real ref */
                if ((nt = tcomposite(d->Stype, s->Stype)) != NULL)
                    d->Stype = nt;              /* Types compatible, use composite */
                else
                    errtwotyp(d, s);    /* Types not compatible, barf */
                }
            else if (!mapextsym(s))     /* New, so set Smaplab now */
                error("External link name duplicated: \"%s\"", s->Sname);
            s->Stype = d->Stype;                /* Clobber sym type to new one */

            if (token != Q_ASGN)
                switch (s->Sclass) /* If just a ref, simple */
                    {
                    default:
                        errdupsym(s);
                    /* FALLTHROUGH */
                    case SC_UNDEF:
                        s->Stype = d->Stype;
                    /* FALLTHROUGH */
                    case SC_XEXTREF:
                        s->Sclass = SC_EXTREF;
                    /* FALLTHROUGH */
                    case SC_INTDEF:
                    case SC_INTREF:
                    case SC_INLINK:
                    case SC_EXTDEF:
                    case SC_EXTREF:
                    case SC_EXLINK:
                        return NULL;
                    }
        /* Handle initializing definition */
            if (lsymhead)
                error("Illegal to initialize block-scope extern");
            switch (s->Sclass)
                {
                case SC_INTDEF:
                case SC_EXTDEF:
                    error("Multiple initialization of %s", s->Sname);
                    break;                      /* Do it anyway... */
                case SC_INTREF:
                case SC_INLINK:
                    s->Sclass = SC_INTDEF;
                    break;
                default:
                    errdupsym(s);               /* Barf and fall thru */
                                        /* to handle as if undef */
                /* FALLTHROUGH */
                case SC_UNDEF:
                case SC_XEXTREF:
                case SC_EXTREF:
                case SC_EXLINK:
                    s->Sclass = SC_EXTDEF;
                    break;
                }
            break;                              /* Go handle izer now */

    /* "static" - internal or no linkage 
    ** In block scope always defines a unique object with no linkage;
    **          this becomes SC_ISTATIC.
    ** In file scope, it declares an object with internal linkage:
    **          with izer, it becomes SC_INTDEF (a def w/internal linkage)
    **          otherwise, SC_INLINK (a tentative def, w/internal linkage)
    **          It is an error if a SC_INTREF (function ref) already exists.
    ** Again, not sure how to re-use Sflags.
    */
        case SC_INTREF:         /* "static" given - internal linkage */
            if (!lsymhead)              /* Check for existing global ref */
                {
                d->Sclass = (token == Q_ASGN) ? SC_INTDEF : SC_INLINK;
                if (s->Sclass == SC_XEXTREF || s->Sclass == SC_EXTDEF
                  || s->Sclass == SC_EXTREF || s->Sclass == SC_EXLINK)
                    {
                    error("Linkage conflict (was external)");
                    }
                else if (s->Sclass == SC_INTDEF || s->Sclass == SC_INLINK
                    || s->Sclass == SC_UNDEF)
                    {
                    if (s->Sclass != SC_UNDEF) /* Check out type if any */
                        {
                        --(s->Srefs);   /* Not really a usage */
                        if ((nt = tcomposite(d->Stype, s->Stype)) != NULL)
                            d->Stype = nt;      /* Types compatible, use composite */
                        else
                            errtwotyp(d, s); /* Types not compatible, barf */
                        }
                    else
                        mapintsym(s);   /* New, set Smaplab to unique name */
                    s->Stype = d->Stype;        /* Clobber sym type to new one */

                    if (token != Q_ASGN)        /* Just a reference? */
                        {
                        s->Sclass = d->Sclass;
                        return NULL;    /* Yep, checking all done! */
                        }
                    if (s->Sclass != SC_INTDEF)
                        {
                        s->Sclass = SC_INTDEF;  /* Def of a prior tent def */
                        break;                  /* Go parse izer */
                        }
                    error("Duplicate initialization of %s", s->Sname);
                /* Fall thru to create duplicate symbol and initialize it */
                    }
                }
            else
                d->Sclass = SC_ISTATIC; /* Internal static */

        /* Block scope, or handling duplicate top-level sym */
            if (isdupsym(s))            /* Check for duplicate def */
                errdupsym(s);
            s = uniqsym(s);                     /* Then always make unique cell */
            if (lsymhead)                       /* If in a local block (SC_ISTATIC) */
                s->Ssym = newlabel();   /* create internal handle on object */
            s->Sclass = d->Sclass;
            s->Stype = d->Stype;
            s->Sflags |= d->Sflags & (SF_ALIGN1 | SF_ALIGN2 | SF_ALIGN4);
            break;

        default:
            int_error("dodecl: illegal Sclass %d for %s", d->Sclass, s->Sname);
            return NULL;
        }

    /* C99 6.7.5.2: an object with static storage duration may not have a
    ** variable length array type.  Pointer-to-VLA objects are not themselves
    ** VLAs, so reject only array objects whose derived array chain is variable.
    */
    if (s->Stype->Tspec == TS_ARRAY && vlacontains_v11(s->Stype)
      && s->Sclass != SC_AUTO && s->Sclass != SC_RAUTO)
        error("Variable length array must have automatic storage duration");

    /* Parse initializer.
    ** At this point the symbol is guaranteed to have one of these classes:
    **  SC_EXTDEF, SC_EXLINK, SC_EXTREF,
    **  SC_INTDEF, SC_INLINK, SC_ISTATIC, SC_AUTO, SC_RAUTO.
    */
    if (token == Q_ASGN)
        {
        nextoken();             /* skip equal sign */
        z = pizer(s);           /* New initializer parsing */
        if (z == NULL)
            error("Null initializer for \"%s\"", s->Sname);

        s->Sinit = 1;
        }
    else
        {
        z = NULL;

        /* No initializer.  Check to make sure this is okay. */
        if (s->Stype->Tspec == TS_ARRAY         /* If type is array, check */
          && s->Stype->Tsize == 0               /* to make sure size 0 okay */
          && !tisvla(s->Stype)
          && s->Sclass != SC_EXTREF             /* Only ext refs allowed */
          && s->Sclass != SC_EXLINK)
            error("Missing size for def of array \"%s\"", s->Sname);

        /* If doing a file-scope tentative definition, we defer
        ** creation of a node (and thus defer emitting code to reserve
        ** storage).  This will be done either at end of file or
        ** when an initializing definition is seen.
        */
        if (s->Sclass == SC_INLINK || s->Sclass == SC_EXLINK)
            return NULL;
        }

    /* For auto variables, assign stack offset and update total stack size.
    ** This must be done after parsing initializer in case the size of an
    ** array was specified by the izer.
    */

    if (s->Sclass == SC_RAUTO)
        s->Svalue = -1; /* temporary flag, detects when reg var uses stack */

    if (s->Sclass == SC_AUTO)
        {
        if (s->Stype->Tspec == TS_ARRAY && vlacontains_v11(s->Stype))
            {
            NODE *vz;
            s->Svalue = -1;
            (void)vlaobjadd_v11(s);
            vlaobjmark_v12(s, vla_declscope_v12());
            if (z != NULL)
                error("Variable length array may not be initialized");
            vz = ndeft(N_VLA, s->Stype);
            vz->Nid = s;
            return ndeflr(N_IZ, ndefident(s), vz);
            }
        s->Svalue = maxauto;            /* Remember its stack offset */
        maxauto += sizetype(s->Stype);  /* and count it in to frame size */
        }

    /* Make a izer node combining a Q_IDENT for the sym with its izer */
    return ndeflr(N_IZ, ndefident(s), z);
}

/* DEFAUTO - Define an automatic variable of the given type.
**      This routine is for the benefit of CCSTMT's function call
**      parsing, which sometimes needs to create temporary internal
**      variables to hold return values.
**      The id is assumed to be unique.
*/
SYMBOL *
defauto(char *id, TYPE *typ)
{
    SYMBOL *s;
    s = creatsym(id);           /* Make symbol in current scope */
    s->Sclass = SC_AUTO;
    s->Srefs++;                 /* This is always a reference */
    s->Stype = typ;
    s->Svalue = maxauto;        /* Remember stack offset for it */
    maxauto += sizetype(typ);
    return s;
}

/* DEFSTATIC - Define an unnamed internal-static object.
**      Used for file-scope compound literals.  The object is emitted lazily
**      by the static initializer generator when its address is required.
**      Keeping only the ordinary symbol and internal label avoids adding any
**      persistent compiler data structure for this C99 feature.
*/
SYMBOL *
defstatic(char *id, TYPE *typ)
{
    SYMBOL *s;

    s = creatsym(id);
    s->Sclass = SC_ISTATIC;
    s->Srefs++;
    s->Stype = typ;
    s->Sinit = 0;
    s->Sused = 0;
    s->Ssym = newlabel();
    return s;
}

/* PIZER - Parse initializer.
**      [dpANS 3.5.7]
**
**      <izer>  ::= <assignment-expr>
**                      | '{' <izer-list> {','} '}'
**
**      <izer-list> ::= <izer> {',' <izer-list>}
**
** Argument is pointer to symbol identifier being initialized.
** Current token should be first thing after the '='.
**
** All expressions for a static extent object, or within an <izer-list> for
** an array, struct, or union type, must be constant expressions.  This
** can be checked for as (!izautof) which will be TRUE if the
** expression must be a constant.
*/

                        /* These are set by pizer and refed by its subrs */
static int izautof;     /* True if symbol being initialized is automatic */
static SYMBOL *izsym;   /* Symbol for var being initialized */

NODE *
pizer(SYMBOL *s)
{
    izautof = isauto(izsym = s);        /* Set "globals" for subroutines */
    return piztype(s->Stype, 0);        /* Do outermost-level parse for type */
}

/* PIZTYPE - Parse initializer for a given type.
**      Recursive; makes use of izautof/izsym globals.
**      Returns what was parsed even if encountered error, mainly so
**      debugging can dump out the parse tree.
*/
static
NODE *
piztype (TYPE *t, int lev)       /* Level being parsed.  0 - outermost */
	{
    NODE		*e,
    			*n;

    
    switch (t->Tspec)
        {
        case TS_BITF:
        case TS_UBITF:
        case TS_CHAR:
        case TS_UCHAR:
        case TS_SHORT:
        case TS_USHORT:
        case TS_INT:
        case TS_UINT:
        case TS_LONG:
        case TS_ULONG:
        case TS_LONGLONG:
        case TS_ULONGLONG:
        case TS_ENUM:                   /* Enums treated like ints */
    
            if ((e = pexizer (lev)) != NULL) /* Parse a single expression */
                return chkarith (t, e, N_ICONST);
    
            break;

    
        case TS_FLOAT:
        case TS_DOUBLE:
        case TS_LNGDBL:
    
            if ((e = pexizer (lev)) != NULL) /* Parse a single expression */
                return chkarith (t, e, N_FCONST);
    
            break;

        
        case TS_PTR: 
           	/* Parse a single expression */
        
            if ((e = pexizer (lev)) == NULL)
                break;
            if (e->Ntype->Tspec == TS_ARRAY)
                e = convarrfn(e);

            /* A function result whose exact-width pointer may be either
            ** native or S=1 keeps that representation-polymorphic type in
            ** an automatic local.  The raw pointer word is unchanged.
            */
            if (izautof && tismaybitptr(e->Ntype) && !tismaybitptr(t)
              && t->Tsubt != NULL && e->Ntype->Tsubt != NULL
              && cmputype(t->Tsubt, e->Ntype->Tsubt))
                {
                t = findctype(TS_PTR, t->Tflag | TF_MAYBITPTR,
                              t->Tsize, t->Tsubt);
                izsym->Stype = t;
                }

            /* A non-byte-aligned packed array decays to KCC's internal
            ** S=1 logical bit pointer.  An automatic pointer initialized
            ** from such an expression can retain that representation
            ** locally without changing the external pointer ABI.
            */
            if (tisbitptr(e->Ntype) && !tisbitptr(t)
              && t->Tsubt != NULL && e->Ntype->Tsubt != NULL
              && cmputype(t->Tsubt, e->Ntype->Tsubt)
              && (izautof || (izsym != NULL
                  && (izsym->Sclass == SC_ISTATIC
                      || izsym->Sclass == SC_INTDEF))))
                {
                /* A file/function-static pointer can retain the exact S=1
                ** representation too.  Unlike an externally visible object,
                ** its KCC-only representation tag cannot disagree with a
                ** declaration in another translation unit.
                */
                t = findctype(TS_PTR, t->Tflag | TF_PACKEDPTR | TF_BITPTR,
                              t->Tsize, t->Tsubt);
                izsym->Stype = t;
                }
            
            if ((n = convasgn (t, e)) != e)      /* Apply assignment convs */
                e = (optpar || !izautof) /* and optimize result if */
                        ? evalexpr (n) : n;      /* want or need to. */
            
            if (!cmputype (t, e->Ntype)) /* Types must match */
                {
                error ("Pointer initializer has wrong type");
                break;
                }

            /* A constant expression for a pointer requires hairy checks. */
            
            if ((!izautof)       /* If must be a constant expr */
    	        && !nisconst (e))  /* then check it out */
                {
                error("Pointer initializer not constant");
                break;
                }

            break;              /* OK, return e */

        
        case TS_ARRAY:
            return pizarray (t, lev);

        
        case TS_STRUCT:
            return pizstruct (t, lev, 0);

        case TS_UNION:
        
            if (clevel >= CLEV_ANSI)
                return pizstruct (t, lev, 1);
        
            error ("Cannot initialize union type");
            
            return pizlist ();   /* Parse it anyway for debugging */

        
        case TS_VOID:
            error ("Cannot initialize void type");
        
            return pizlist ();   /* Parse it anyway for debugging */

        
        case TS_FUNCT:
            error ("Cannot initialize function type");
        
            return pizlist ();   /* Parse it anyway for debugging */

        
        default:
            int_error("piztype: unknown Tspec = %d", t->Tspec);
        
            return NULL;
        }

    return e;
	}

/* CHKARITH - Check initializer for arithmetic type.
**      This is a subroutine for code sharing purposes.
*/
static NODE *
chkarith(TYPE *t, NODE *e, int noptyp)
{
    NODE *n;

    if (!tisarith(e->Ntype))
        {
        error("Initializer must be of arithmetic type");
        return e;
        }
    if ((n = convasgn(t, e)) != e)      /* Apply assignment convs */
        e = (optpar || !izautof) /* and optimize result if */
                ? evalexpr(n) : n;      /* want or need to. */
    if ((!izautof) && e->Nop != noptyp)
        error("%s constant required as initializer",
                (noptyp == N_ICONST ? "Integer" : "Floating-point"));
    return e;
}

/* IZLISTSLOT - Return an initializer-list slot at zero-based index.
**
** Designated initializers are normalized into the historical sequential
** N_IZLIST representation.  Missing slots have a NULL Nleft and are emitted
** as zero-fill by the existing generators.  This avoids a second initializer
** representation and keeps the parser memory cost to one node per object
** element/member, exactly as for an ordinary positional initializer.
*/
static NODE *
izlistslot(NODE **rootp, TYPE *t, INT index)
{
    NODE *n;
    INT i;

    if (index < 0)
        index = 0;
    if (*rootp == NULL)
        *rootp = ndeftl(N_IZLIST, t, NULL);
    n = *rootp;
    for (i = 0; i < index; ++i) {
        if (n->Nright == NULL)
            n->Nright = ndefl(N_IZLIST, NULL);
        n = n->Nright;
    }
    return n;
}

/* IZMEMBER - Find a member and its zero-based position in an aggregate.
** Returns NULL for an unknown member.
*/
static SYMBOL *
izmember(TYPE *t, SYMBOL *name, INT *indexp, SYMBOL **promotedp)
{
    SYMBOL *m, *alias;
    INT i;

    alias = m = symfmember(name, t->Tsmtag);
    if (m == NULL)
        return NULL;
    if (m->Sflags & SF_ANONALIAS) {
        if (m->Ssmnext == NULL) {
            int_error("anonymous member alias has no source member");
            return NULL;
        }
        m = anonroot_v7(t, m->Ssmnext);
        if (m == NULL) {
            int_error("anonymous member alias has no physical root");
            return NULL;
        }
        if (promotedp != NULL)
            *promotedp = alias;
    } else if (promotedp != NULL)
        *promotedp = NULL;

    i = 0;
    for (name = t->Tsmtag->Ssmnext; name != NULL;
         name = name->Ssmnext, ++i) {
        if (name->Sclass != SC_MEMBER)
            int_error("bad smem class");
        if (name == m) {
            *indexp = i;
            return m;
        }
    }
    int_error("izmember: member not on tag chain");
    return NULL;
}

/* PIZMERGEANON_V7 - Merge a later anonymous-member designated initializer
** into an existing positional initializer subtree.  Repeated promoted
** designators commonly select different leaves of the same physical
** anonymous aggregate; replacing Nleft would discard the earlier leaf.
*/
static NODE *
pizmergeanon_v7(NODE *dst, NODE *src, TYPE *t)
{
    NODE *d, *sp, *prev;
    SYMBOL *m;

    if (dst == NULL)
        return src;
    if (src == NULL)
        return dst;
    if (t == NULL || (t->Tspec != TS_STRUCT && t->Tspec != TS_UNION))
        return src;

    if (t->Tspec == TS_UNION) {
        m = src->Nizmem;
        if (m != NULL && dst->Nizmem == m
          && m->Stype != NULL
          && (m->Stype->Tspec == TS_STRUCT || m->Stype->Tspec == TS_UNION)
          && dst->Nleft != NULL && src->Nleft != NULL
          && dst->Nleft->Nop == N_IZLIST && src->Nleft->Nop == N_IZLIST)
            dst->Nleft = pizmergeanon_v7(dst->Nleft, src->Nleft, m->Stype);
        else {
            dst->Nizmem = src->Nizmem;
            dst->Nleft = src->Nleft;
        }
        return dst;
    }

    d = dst;
    prev = NULL;
    sp = src;
    m = t->Tsmtag->Ssmnext;
    while (sp != NULL && m != NULL) {
        if (d == NULL) {
            d = ndefl(N_IZLIST, NULL);
            if (prev != NULL)
                prev->Nright = d;
        }
        if (sp->Nleft != NULL) {
            if (d->Nleft != NULL
              && m->Stype != NULL
              && (m->Stype->Tspec == TS_STRUCT || m->Stype->Tspec == TS_UNION)
              && d->Nleft->Nop == N_IZLIST && sp->Nleft->Nop == N_IZLIST)
                d->Nleft = pizmergeanon_v7(d->Nleft, sp->Nleft, m->Stype);
            else
                d->Nleft = sp->Nleft;
        }
        prev = d;
        d = d->Nright;
        sp = sp->Nright;
        m = m->Ssmnext;
    }
    return dst;
}

/* PIZANON_V7 - Build the positional initializer subtree needed to reach a
** named member promoted through one or more anonymous aggregates.  TOKEN is
** already positioned after the promoted member name.
*/
static NODE *
pizanon_v7(TYPE *t, SYMBOL *leaf)
{
    SYMBOL *m;
    NODE *root, *slot, *v;
    INT idx;

    if (t == NULL || leaf == NULL)
        return NULL;

    if (leaf->Ssmtag == t->Tsmtag) {
        idx = 0;
        for (m = t->Tsmtag->Ssmnext; m != NULL; m = m->Ssmnext, ++idx)
            if (m == leaf)
                break;
        if (m == NULL) {
            int_error("anonymous initializer leaf not on member chain");
            return NULL;
        }
        v = pizdesigvalue(leaf->Stype);
        if (t->Tspec == TS_UNION) {
            root = ndeftl(N_IZLIST, t, v);
            root->Nizmem = leaf;
            return root;
        }
        root = NULL;
        slot = izlistslot(&root, t, idx);
        slot->Nleft = v;
        return root;
    }

    m = anonroot_v7(t, leaf);
    if (m == NULL) {
        int_error("anonymous initializer path not found");
        return NULL;
    }
    v = pizanon_v7(m->Stype, leaf);
    if (t->Tspec == TS_UNION) {
        root = ndeftl(N_IZLIST, t, v);
        root->Nizmem = m;
        return root;
    }
    idx = 0;
    for (slot = NULL, m = t->Tsmtag->Ssmnext; m != NULL; m = m->Ssmnext, ++idx)
        if ((m->Sflags & SF_ANONMEM) && anoncontains_v7(m->Stype, leaf->Ssmtag))
            break;
    if (m == NULL) {
        int_error("anonymous initializer root not on member chain");
        return NULL;
    }
    root = NULL;
    slot = izlistslot(&root, t, idx);
    slot->Nleft = v;
    return root;
}

/* PIZDESIGVALUE - Parse the tail of a C99 designator chain.
**
** The caller has already selected one aggregate element/member and token is
** the first token following that selector.  Further .member or [index]
** selectors are recursively normalized into the existing positional
** N_IZLIST representation.  This keeps memory use proportional only to the
** initializer path and adds no persistent compiler data structure.
*/
static NODE *
pizdesigvalue(TYPE *t)
{
    SYMBOL *m;
    NODE *root, *slot, *v;
    INT idx;

    if (token == Q_ASGN) {
        nextoken();
        return piztype(t, 1);
    }

    if (token == Q_DOT) {
        if (t->Tspec != TS_STRUCT && t->Tspec != TS_UNION) {
            error("Member designator applied to non-aggregate type");
            pizflush(1);
            return NULL;
        }
        if (nextoken() != Q_IDENT) {
            error("Member name required after . in initializer");
            pizflush(1);
            return NULL;
        }
        {
        SYMBOL *promoted = NULL;
        idx = 0;
        m = izmember(t, csymbol, &idx, &promoted);
        if (csymbol->Sclass == SC_UNDEF)
            freesym(csymbol);
        if (m == NULL) {
            error("Unknown struct/union member in initializer");
            pizflush(1);
            return NULL;
        }
        nextoken();
        if (promoted != NULL)
            v = pizanon_v7(m->Stype, promoted->Ssmnext);
        else
            v = pizdesigvalue(m->Stype);
        if (t->Tspec == TS_UNION) {
            root = ndeftl(N_IZLIST, t, v);
            root->Nizmem = m;
        } else {
            root = NULL;
            slot = izlistslot(&root, t, idx);
            slot->Nleft = v;
        }
        return root;
        }
    }

    if (token == T_LBRACK) {
        if (t->Tspec != TS_ARRAY) {
            error("Array designator applied to non-array type");
            pizflush(1);
            return NULL;
        }
        nextoken();
        idx = pconst();
        if (idx < 0) {
            error("Array designator index must not be negative");
            idx = 0;
        }
        if (token != T_RBRACK) {
            error("] required after array designator");
            pizflush(1);
            return NULL;
        }
        nextoken();
        v = pizdesigvalue(t->Tsubt);
        if (t->Tsize != 0 && idx >= (INT)t->Tsize) {
            error("Array designator index exceeds array bounds");
            return NULL;
        }
        root = NULL;
        slot = izlistslot(&root, t, idx);
        slot->Nleft = v;
        return root;
    }

    error("= required after designator");
    pizflush(1);
    return NULL;
}

/* PIZSTRUCT - Parse initializer for a structure or union.
** Supports C99 member designators (.member = value).  Designators are
** flattened into the existing positional N_IZLIST form, with NULL entries
** for skipped structure members.  For a union the selected member is kept
** in Nizmem so static generation can select the same overlapping member.
*/
static NODE *
pizstruct(TYPE *t, int lev, int isunion)
{
    SYMBOL *first, *smem, *found, *promoted;
    NODE *e, *root, *slot;
    INT curidx, desidx;
    int braces, haveitem, designated;

    if (clevel < CLEV_ANSI && izautof)
        error("Initialization of auto struct/union not allowed");
    first = t->Tsmtag->Ssmnext;
    if (first == NULL) {
        error("Attempting to initialize an undefined struct/union");
        return pizlist();
    }
    if (first->Sclass != SC_MEMBER)
        int_error("bad smem class");

    braces = (token == T_LBRACE);
    if (!braces && lev == 0) {
        if (!izautof) {
            error("Static struct/union initializer must be enclosed in braces");
            return pizlist();
        }
        e = pexizer(lev);
        if (e && !cmputype(t, e->Ntype)) {
            error("Struct/union initializer has wrong type");
            pizlist();
        }
        return e;
    }

    if (!braces) {
        /* Preserve the historical inner-list positional parser. */
        smem = first;
        root = slot = ndeftl(N_IZLIST, (isunion ? smem->Stype : t),
                             piztype(smem->Stype, 1));
        while (token == T_COMMA) {
            if (isunion || (smem = smem->Ssmnext) == NULL)
                return root;
            if (nextoken() == T_RBRACE)
                return root;
            slot = slot->Nright = ndefl(N_IZLIST, piztype(smem->Stype, 1));
        }
        if (token != T_RBRACE)
            error("Bad initializer list syntax");
        return root;
    }

    nextoken();
    root = NULL;
    smem = first;
    curidx = 0;
    haveitem = 0;

    while (token != T_RBRACE && token != T_EOF) {
        designated = 0;
        if (token == Q_DOT) {
            if (!CSTD_HAS(CSTD_C99))
                error("Designated initializers require C99");
            if (nextoken() != Q_IDENT) {
                error("Member name required after . in initializer");
                pizflush(1);
                break;
            }
            desidx = 0;
            promoted = NULL;
            found = izmember(t, csymbol, &desidx, &promoted);
            if (csymbol->Sclass == SC_UNDEF)
                freesym(csymbol);
            if (found == NULL) {
                error("Unknown struct/union member in initializer");
                pizflush(1);
                break;
            }
            smem = found;
            curidx = desidx;
            nextoken();
            designated = 1;
        }

        if (smem == NULL) {
            error("Too many members in initializer list");
            pizflush(1);
            break;
        }

        if (isunion) {
            if (root == NULL)
                root = ndeftl(N_IZLIST, t, NULL);
            slot = root;
            root->Ntype = t;
            root->Nizmem = smem;
        } else
            slot = izlistslot(&root, t, curidx);
        if (designated && promoted != NULL) {
            NODE *av = pizanon_v7(smem->Stype, promoted->Ssmnext);
            if (slot->Nleft != NULL)
                slot->Nleft = pizmergeanon_v7(slot->Nleft, av, smem->Stype);
            else
                slot->Nleft = av;
        } else
            slot->Nleft = designated ? pizdesigvalue(smem->Stype)
                                     : piztype(smem->Stype, 1);
        haveitem = 1;

        if (!isunion) {
            smem = smem->Ssmnext;
            ++curidx;
        } else
            smem = NULL;

        if (token != T_COMMA)
            break;
        if (nextoken() == T_RBRACE)
            break;
        if (isunion) {
            if (token != Q_DOT) {
                error("Too many members in union initializer list");
                pizflush(1);
                break;
            }
            smem = first;       /* The designator will select the member. */
        }
    }

    if (!haveitem) {
        error("Null initializer");
        if (root == NULL)
            root = ndeftl(N_IZLIST, t, NULL);
    }
    if (token != T_RBRACE) {
        error("Bad initializer list syntax");
        pizflush(1);
        if (token == T_RBRACE)
            nextoken();
    } else
        nextoken();
    return root;
}

/* PIZARRAY - Parse initializer for an array.
** Supports C99 array designators ([constant-index] = value).  Sparse
** designators are normalized into the existing positional initializer list.
*/
static NODE *
pizarray(TYPE *t, int lev)
{
    TYPE *subt;
    NODE *n, *root, *slot, *value;
    INT cnt, idx, maxidx, limit;
    int braces, gotstr, designated;

    if (clevel < CLEV_ANSI && izautof)
        error("Initialization of auto arrays is not allowed");
    subt = t->Tsubt;
    if (subt == NULL) {
        int_error("pizarray: null subt");
        return pizlist();
    }
    if (t->Tsize == 0 && lev) {
        error("Flexible or incomplete array member cannot be initialized");
        return pizlist();
    }

    braces = (token == T_LBRACE);
    if (braces)
        nextoken();

    /* Keep the historical string-literal special case. */
    gotstr = 0;
    if (tischar(subt) && token == T_SCONST) {
        n = pexizer(1);
        if (n == NULL || n->Nop != N_SCONST) {
            int_error("pizarray: bad sconst");
            return NULL;
        }
        gotstr = 1;
        cnt = n->Nsclen;
        root = ndeftl(N_IZLIST, t, n);
        if (braces && token == T_COMMA)
            nextoken();
    } else {
        if (!braces && lev == 0) {
            error("Outer array initializer must be enclosed in braces");
            pizflush(1);
            return NULL;
        }
        root = NULL;
        idx = 0;
        maxidx = -1;
        limit = (t->Tsize == 0) ? ((unsigned)(~0) >> 1) : t->Tsize;

        while (token != T_RBRACE && token != T_EOF) {
            designated = 0;
            if (token == T_LBRACK) {
            if (!CSTD_HAS(CSTD_C99))
                error("Designated initializers require C99");
                nextoken();
                idx = pconst();
                if (idx < 0) {
                    error("Array designator index must not be negative");
                    idx = 0;
                }
                if (token != T_RBRACK) {
                    error("] required after array designator");
                    pizflush(1);
                    break;
                }
                nextoken();
                designated = 1;
            }
            value = designated ? pizdesigvalue(subt) : piztype(subt, 1);
            if (idx >= limit) {
                error("Array designator index exceeds array bounds");
            } else {
                slot = izlistslot(&root, t, idx);
                slot->Nleft = value;
                if (idx > maxidx)
                    maxidx = idx;
            }
            ++idx;
            if (token != T_COMMA)
                break;
            if (nextoken() == T_RBRACE)
                break;
        }
        cnt = maxidx + 1;
        if (root == NULL)
            root = ndeftl(N_IZLIST, t, NULL);
    }

    if (t->Tsize == 0)
        root->Ntype = izsym->Stype = findctype(TS_ARRAY, t->Tflag, cnt, subt);
    else if (gotstr && cnt-1 > (INT)t->Tsize)
        error("String exceeds char array bounds");

    if (braces) {
        if (token == T_RBRACE)
            nextoken();
        else {
            error("Bad initializer list syntax");
            pizflush(1);
            if (token == T_RBRACE)
                nextoken();
        }
    } else if ((!gotstr || lev) && token != T_COMMA && token != T_RBRACE)
        error("Bad initializer list syntax");
    return root;
}

/* PEXIZER - Parse single initializer expression.
**      Should be only one expression; outermost braces are allowed.
**      Never returns a list; complains and fixes up as necessary.
*/
static NODE *
pexizer(int lev)                /* 0 if outermost level */
{
    NODE *n;

    if (token != T_LBRACE)
        {
        if (!izautof)
            return exprconst();                 /* need constant */
        else if (optpar)
            return evalexpr(asgnexpr());        /* want constant folding */
        else
            return asgnexpr();                  /* no optimization */
        }
    
    /* We have a list of some sort.  This is only legal if at outermost
    ** level AND there is only one thing in the list.
    */
    if (lev)
        error("Inner initializer for this object cannot be a list");
    n = pizlist();              /* Parse the list, always returns N_IZLIST */
    if (n->Nright && lev == 0)  /* More than one thing in list? */
                                /* Complain unless already complained above */
        error("Initializer cannot be a list of more than one element");
    if ((n = n->Nleft) == NULL)
        {
        error("Null initializer");
        return NULL;
        }
    if (n->Nop == N_IZLIST)     /* Sublist? */
        {
        error("Initializer cannot have sublist");
        while (n->Nop == N_IZLIST && (n = n->Nleft) != NULL)
            ;
        }
    return (optpar || !izautof)  /* If want or need constant */
                ?  evalexpr(n)          /* evaluate parse result */
                : n;
}

/* PIZLIST - Parse initializer list
**      Mainly for error recovery, when contents aren't analyzed closely.
*/
static NODE *
pizlist(void)
{
    NODE *n, *root;

    if (token != T_LBRACE)      /* If not a list, return simple izer */
        return asgnexpr();      /* Don't need to waste time optimizing */

    nextoken();                         /* Skip open brace */
    root = n = ndefl(N_IZLIST, pizlist());
    while (token == T_COMMA)
        {
        nextoken();                     /* Skip comma */
        if (token == T_RBRACE)
            break;      /* allow comma at end of initializer */
        n = n->Nright = ndefl(N_IZLIST, pizlist());
        }
    expect(T_RBRACE);                   /* finish with close brace */
    return root;                        /* return whole thing */
}

/* PIZFLUSH - Flush initializer tokens
**      Used during error recovery.  Type says what situation we're in.
**      0 - at start of izer, flush entire izer.
**              (stop if top-level comma, gobble end close-brace if one seen)
**              This also works to flush only a single list, if at its start.
**      1 - inside a list, flush to higher-level close-brace and don't
**              gobble it, so caller can handle close-brace.
*/
static void
pizflush(int typ)
{
    int lev = 0;
    if (typ)
        lev = 1;                /* Pretend already in a list */
    for(;; nextoken())
        switch(token)
            {
            case T_COMMA:
                if (typ == 0 && lev <= 0)
                    return;
                break;
            case T_LBRACE:              /* Start of list */
                lev++;
                break;
            case T_RBRACE:              /* End of list */
                if (--lev <= 0)
                    {
                    if (typ == 0)
                        nextoken();     /* Maybe gobble close-brace */
                    return;
                    }
                break;

            case T_EOF:         /* Input EOF */
            case T_SCOLON:              /* End of declaration statement */
                return;
            default:
                ;       /* do nothing */
            }
}

/* NISCONST - Returns true if expression is an allowable initializer constant.
**      Return value indicates something about the type of constant:
**      0 - not a constant
**      1 - definitely a constant (arithmetic, or a cast pointer)
**      2 - address of some kind
**      3 - function address (cannot add or subtract from this)
*/
static int
nisconst(NODE *e)
{
    switch(e->Nop)
        {
        case N_CAST:            /* Assume generator will be able to handle */
            return nisconst(e->Nleft);

        case N_ICONST:
        case N_FCONST:
        case N_PCONST:
            return 1;           /* Simple constant */
        case N_SCONST:
            return 2;           /* Address */

        case N_ACONST:
            return 2;           /* Label address */

        case Q_IDENT:
                /* Identifier.  See documentation for Q_IDENT in INTERN.DOC
                ** for explanation of this method of testing.
                */
            if (e->Nid->Stype->Tspec == TS_FUNCT)
                return 3;       /* Function address */
            else if (e->Nid->Stype->Tspec == TS_ARRAY)
                return 2;       /* Array address */
            break;

        case N_ADDR:
            switch (e->Nleft->Nop)
                {
                case N_COMPLIT:
                    /* File-scope compound literals have static storage
                    ** duration, so their address is a valid static
                    ** initializer constant.  Block-scope literals use an
                    ** automatic hidden object and therefore are not.
                    */
                    if (e->Nleft->Nleft != NULL
                      && e->Nleft->Nleft->Nop != N_DATA
                      && e->Nleft->Nright != NULL
                      && e->Nleft->Nright->Nop == Q_IDENT
                      && e->Nleft->Nright->Nid->Sclass == SC_ISTATIC)
                        return 2;
                    return 0;

                case N_PTR:
                    return nisconst(e->Nleft->Nleft);

#if 0
                /* Allow for conversion of arrays generated by subscripting */
                case Q_PLUS:
                    if (e->Nleft->Ntype->Tspec == TS_ARRAY)
                        return nisconst(e->Nleft);      /* OK, continue */
                    break;                              /* Not array, fail */
#endif
                /* Structure hair.
                ** For MEMBER (->) the Nleft must be a constant address.
                **      Can just apply nisconst to this.
                ** For DOT (.) the Nleft can be anything that evaluates into
                **      a static structure.  We assume this is only possible
                **      with either Q_IDENT, or N_PTR of a struct addr.
                */
q_dot_label:
                case Q_DOT:
                    if (tisbitf(e->Nleft->Ntype))       /* No bitfield ptrs */
                        return 0;
                    switch (e->Nleft->Nleft->Nop)
                        {
                        case Q_IDENT:
                            switch (e->Nleft->Nleft->Nid->Sclass)
                                {
                                case SC_XEXTREF:
                                case SC_EXLINK:
                                case SC_EXTDEF:
                                case SC_EXTREF:
                                case SC_INTDEF:
                                case SC_INTREF:
                                case SC_INLINK:
                                case SC_ISTATIC:
                                    return 2;   /* Good address of object */
                                default:
                                    ;   /* do nothing */
                                }
                            break;
                        case N_PTR:
                            if (nisconst(e->Nleft->Nleft->Nleft)==2)
                                return 2;
                            break;

                        case Q_DOT:
                            e = e->Nleft;
                            goto q_dot_label;
                        case Q_MEMBER:
                            e = e->Nleft;
                            goto q_member_label;

                        default:
                            ;   /* do nothing */
                        }
                    break;                      /* Otherwise fail. */

q_member_label:
                case Q_MEMBER:

                    if (e->Nleft->Nleft->Nop == Q_DOT)
                        {
                        e = e->Nleft;
                        goto q_dot_label;
                        }
                    else if (e->Nleft->Nleft->Nop == Q_MEMBER)
                        {
                        e = e->Nleft;
                        goto q_member_label;
                        }
                    else if (!tisbitf(e->Nleft->Ntype)  /* No bitfield ptrs */
                      && nisconst(e->Nleft->Nleft)==2)  /* If struct addr is*/
                        return 2;               /*  OK, then we're OK */
                    break;                      /* Otherwise fail. */

                case Q_IDENT:   /* Addr OK if of external or static */
                        /* Needn't test type since parser checks it while
                        ** parsing "&" to verify not function or array.
                        */
                    switch (e->Nleft->Nid->Sclass)
                        {
                        case SC_XEXTREF:
                        case SC_EXLINK:
                        case SC_EXTDEF:
                        case SC_EXTREF:
                        case SC_INTDEF:
                        case SC_INTREF:
                        case SC_INLINK:
                        case SC_ISTATIC:
                            return 2;           /* Good address of object */
                        default:
                            ;   /* do nothing */
                        }
                    break;
                default:
                    int_error ("nisconst : invalid Q_ADDR op %d",
                            e->Nleft->Nop);
                }
            break;

        /* Non-atomic expression checks, for plus and minus. */
        case Q_PLUS:
            if (e->Nleft->Nop == N_ICONST       /* Integ constant */
                && nisconst(e->Nright) == 2)    /* plus address */
                return 2;
            /* Fall through into Q_MINUS code */
        /* FALLTHROUGH */
        case Q_MINUS:
            if (nisconst(e->Nleft) == 2         /* Address */
              && e->Nright->Nop == N_ICONST)    /* plus/minus integ constant */
                return 2;
            break;
        }
    return 0;
}

#if 0
/* CHKPTRCON - utility routine to check out a pointer initializer.
**      Allowed initializers (per CARM 4.6.3) must be an integer or
**      a static/external address plus/minus an integer.
*/
static NODE *
chkptrcon(NODE *e)
{
    NODE *n, *addr, *con;

    n = (e->Nop == N_CAST) ? e->Nleft : e;      /* Allow casts */
    if (n->Nop == N_ICONST)
        return e;       /* Integer const OK (case 1,6) */
    if (n->Nop == Q_IDENT)
        return e;       /* Funct or array name OK (case 2,3) */
                                        /* (type cked by convasgn) */

    if (n->Nop == Q_PLUS || n->Nop == Q_MINUS)
        {
        addr = n->Nleft;
        con = n->Nright;
        if (n->Nop == Q_PLUS && con->Nop != N_ICONST)
            addr = n->Nright, con = n->Nleft;
        if (con->Nop != N_ICONST)
            {
            error("Pointer initializer constant has bad expression");
            return NULL;
            }
        }
    else
        addr = n;

    /* Check out address */
    n = (addr->Nop == N_CAST) ? addr->Nleft : addr;     /* Allow casts */
    if (n->Nop == N_ICONST)
        return e;       /* Integer const OK (case 6) */
    if (n->Nop == N_SCONST)
        return e;       /* String const OK (case 7) */
    if (n->Nop == N_ADDR)               /* Addr of something (case 4) */
        {
        addr = n->Nleft;
        if (addr->Nop != Q_IDENT
          || isauto(addr->Nid))
            {
            error("Constant pointer initializer has non-constant addr");
            return NULL;
            }
        }
    return e;
}
#endif /* commented-out code */

/* ISAUTO - Returns true if symbol has automatic extent
*/
static int
isauto(SYMBOL *s)
{
    switch(s->Sclass)
        {
        case SC_ARG:
        case SC_RARG:
        case SC_AUTO:
        case SC_RAUTO:
            return 1;
        default:
            return 0;
        }
}

/* TYPENAME - Parse type name for cast and sizeof expressions.
**      [dpANS 3.5.5]
**
**      <type-name> ::= <spec-qual-list> {abstract-declor}
**      
** This parses a "type name" built from an abstract declarator.
** Storage-class specifiers and identifiers are illegal.
*/

TYPE *
typename(void)
{
    SYMBOL s, *t;

    pbase(&s);                  /* Parse base */
    if (s.Sclass != SC_UNDEF)
        {
        error("Storage class not allowed for type-name");
        s.Sclass = SC_UNDEF;
        }
    if (s.Stype == NULL)
        {
        error("No type-specifier for type-name, assuming int");
        s.Stype = deftype;
        }
    if ((t = declarator(&s)) != NULL)
        {
        error("Identifier not allowed in type-name");
        if (t->Sclass == SC_UNDEF)
            freesym(t);                 /* Clean up */
        }
    return s.Stype;
}

/* Miscellaneous auxiliaries */

static void
errtwotyp(SYMBOL *d, SYMBOL *s)         /* New decl sym and existing sym */
{
    error("Symbol %S previously declared with different type %S", s, d);
}

static void
errdupsym(struct symbol * s)
{
    error("Symbol %S previously defined", s);
}



static
void
Set_Register (SYMBOL *s, int rarg, int arg)
    {
#define tsizeone(t) (tisinteg(t) || t->Tspec == TS_FLOAT)

    if (s->Sclass == rarg)
        {
        /*
         * Registers above r_maxnopreserve up to and including R_MAXREG
         * are available for register variables.
         */

        if ((_reg_count < fnregcap_v11())
            && (tsizeone(s->Stype)
#if 0   /* next version of Reg linkage */
                || s->Stype->Tspec == TS_PTR && tsizeone(s->Stype->Tsubt)
#endif
                                            ))
            {
            s->Sreg = _reg_count + (r_maxnopreserve + 1); /* FW 2A(47) */
            Reg_Id[_reg_count] = s;
            ++_reg_count;
            }
        else            /* Max reg vars already (or wrong var type) */
            {
            s->Sclass = arg;
            s->Sreg = 0;
            }
        }
    }
