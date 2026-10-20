/* CCOUTUTIL.C - small target/output helpers shared by KGEN and CCOUT. */

#include "cc.h"
#include "ccgen.h"
#ifndef __COMPILER_KCC__
#include <string.h>
#endif

/* ONEINSTR - true when this pseudo-op emits exactly one machine word. */
int
oneinstr(PCODE *p)
{
    switch (p->Pop & POF_OPCODE) {
    case P_PTRCNV:
    case P_SMOVE:
    case P_UIDIV:
    case P_UFLTR:
    case P_SUBBP:
    case P_DFIX:
    case P_DSNGL:
        return 0;
    case P_DMOVE:
    case P_DMOVN:
    case P_DMOVEM:
        return tgmachuse.dmovx;
    case P_ADJSP:
        return tgmachuse.adjsp;
    case P_ADJBP:
        return tgmachuse.adjbp;
    case P_DFAD:
    case P_DFSB:
    case P_DFMP:
    case P_DFDV:
        return (tgarch == TGARCH_PDP6 ||
            (tgcpu != TGCPU_PDP6 && tgcpu != TGCPU_KA));
    case P_IDIV:
        if (tgcpu == TGCPU_PDP6 || tgarch == TGARCH_BASE ||
            tgarch == TGARCH_PDP10)
            return 0;
        return 1;
    case P_FLTR:
        return tgmachuse.fltr;
    case P_TRN:
    case P_TRO:
    case P_TRC:
    case P_TRZ:
    case P_TLN:
    case P_TLO:
    case P_TLC:
    case P_TLZ:
    case P_CAI:
    case P_LSH:
    case P_ASH:
        return p->Ptype != PTA_MINDEXED;
    default:
        return p->Ptype != PTV_IINDEXED;
    case P_MOVE:
    case P_MOVEI:
        return 1;
    }
}

/* ADJBOFFSET - split a byte offset into word and in-word offsets. */
int
adjboffset(INT boff, INT *awoff, int bpw)
{
    if (boff < 0) {
        *awoff = -((-boff) / bpw);
        if ((boff = (-boff) % bpw) != 0) {
            (*awoff)--;
            return bpw - (int)boff;
        }
        return 0;
    }
    *awoff = boff / bpw;
    return (int)(boff % bpw);
}

/* BINEXP - floor(log2(n)); historical zero result is -1. */
INT
binexp(unsigned INT n)
{
    INT e;

    e = -1;
    do {
        n >>= 1;
        ++e;
    } while (n != 0);
    return e;
}

/* Hosted target-precision floating conversion shared by KPARSE, KGEN,
** KOPT and the integrated compiler.  These helpers add no PDP-6 runtime
** storage, and the native compiler does not compile this host-only code.
*/
#ifndef __COMPILER_KCC__
unsigned long
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

void
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

/* Round a host-side constant/expression to the target's 27-bit single
** precision WITHOUT narrowing through IEEE binary32.  The caller keeps
** the result as a host double (which can represent every target single
** fraction exactly).  This uses no libm, dynamic storage, or target
** runtime code.  A long-double intermediate retains the low guard/sticky
** bits of operations on 27-bit operands before their final rounding.
**
** Only normal, representable target values are folded.  A subnormal
** target result becomes zero; an out-of-range high exponent is left
** for the existing output-time overflow diagnostic.
*/
static double
pdp10_host_round_single_core_v1 (long double value, int pdp6_arithmetic)
{
    long double mag, scaled, rem, rounded;
    unsigned long fraction;
    int exponent;
    int negative = value < 0.0L;

    if (value == 0.0L)
	return 0.0;
    if (value != value) /* NaN: let the established emitter diagnose it. */
	return (double)value;
    mag = negative ? -value : value;
    if (mag < 0x1p-128L)
	return 0.0;
    if (mag >= 0x1p127L)
	return (double)value;

    exponent = 0;
    while (mag >= 1.0L)
	{
	  mag *= 0.5L;
	  ++exponent;
	}
    while (mag < 0.5L)
	{
	  mag *= 2.0L;
	  --exponent;
	}
    scaled = mag * 134217728.0L; /* 2^27, normalized fraction in [2^26,2^27). */
    fraction = (unsigned long)scaled;
    rem = scaled - (long double)fraction;
    if (rem > 0.5L ||
	(rem == 0.5L && (pdp6_arithmetic ? !negative : (fraction & 1))))
	++fraction;
    if (fraction == 134217728UL)
	{
	  fraction >>= 1;
	  ++exponent;
	}
    rounded = (long double)fraction;
    while (exponent > 27)
	{
	  rounded *= 2.0L;
	  --exponent;
	}
    while (exponent < 27)
	{
	  rounded *= 0.5L;
	  ++exponent;
	}
    return (double)(negative ? -rounded : rounded);
}

double
pdp10_host_round_single_v1 (long double value)
{
    return pdp10_host_round_single_core_v1 (value, 0);
}

/* PDP-6 rounded floating-point arithmetic (FADR/FMPR/FDVR) handles a
** halfway fraction differently from casts and literal encoding.  The
** independent AAP PDP-6 model rounds positive halfway cases upward,
** but negative halfway cases toward zero: a positive-direction tie,
** rather than magnitude-away or ties-to-even.  This is distinct from
** the existing SIMH PDP-6 emulator's negative rounding behavior, which
** must be investigated separately rather than copied into the compiler.
** Keep later PDP-10 processors separate until their FP modes are tested.
*/
double
pdp10_host_round_single_arith_pdp6_v1 (long double value)
{
    return pdp10_host_round_single_core_v1 (value, 1);
}

/* Encode a PDP-10 single word from an IEEE host DOUBLE, not a host FLOAT.
** A PDP-10 36-bit floating word has 27 fraction bits; IEEE single has
** only 24 significant bits.  Discarding those three target-only bits in
** the host lexer or output stage corrupts otherwise representable
** literals such as 134217732.0F.  Round the full host binary64 mantissa
** to the 27-bit target fraction using ties-to-even.  This routine is for
** host toolchain builds; native KCC uses its own target representation.
*/
unsigned INT
host_pdp10_float_word_v1 (double d)
{
    unsigned long lo32, hi32;
    unsigned long long fraction, low, word;
    int exponent, negative;

    host_double_bits (d, &lo32, &hi32);
    negative = (hi32 & 0x80000000UL) != 0;
    exponent = (int) ((hi32 >> 20) & 0x7ffUL);
    if (exponent == 0)
	return 0; /* Host subnormals underflow the PDP-10 exponent range. */
    if (exponent == 0x7ff)
	{
	  int_error ("host_pdp10_float_word_v1: nonfinite constant");
	  return 0;
	}

    fraction = (1ULL << 52) |
	((unsigned long long) (hi32 & 0xfffffUL) << 32) | lo32;
    low = fraction & ((1ULL << 26) - 1);
    fraction >>= 26;
    if (low > (1ULL << 25) ||
	(low == (1ULL << 25) && (fraction & 1)))
	++fraction;
    exponent = exponent - 894; /* IEEE bias 1023 to PDP bias 128. */
    if (fraction == (1ULL << 27))
	{
	  fraction >>= 1;
	  ++exponent;
	}
    if (exponent <= 0)
	return 0;
    if (exponent > 255)
	{
	  int_error ("host_pdp10_float_word_v1: exponent overflow");
	  return 0;
	}
    word = ((unsigned long long) exponent << 27) | fraction;
    if (negative)
	word = (-word) & ((1ULL << 36) - 1);
    return (unsigned INT) word;
}

#endif /* !__COMPILER_KCC__ */
