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
        return tgcpu != TGCPU_PDP6 && tgcpu != TGCPU_KA;
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
	  /* Nonfinite host intermediates cannot be encoded in PDP-10 F format;
	  ** this is a source-range diagnostic, not an internal error. */
	  error ("Floating constant overflow (no PDP-10 nonfinite value)");
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
	  error ("Floating constant exponent exceeds PDP-10 F format");
	  return 0;
	}
    word = ((unsigned long long) exponent << 27) | fraction;
    if (negative)
	word = (-word) & ((1ULL << 36) - 1);
    return (unsigned INT) word;
}

void
host_pdp10_double_words (double d, unsigned INT * w1, unsigned INT * w2)
{
    unsigned INT fraction, high, low, mask35, mask36;
    unsigned long lo32, hi32;
    int exponent, negative;

    host_double_bits (d, &lo32, &hi32);
    exponent = (int)((hi32 >> 20) & 03777UL);
    negative = (hi32 & (1UL << 31)) != 0;
    /* Host subnormal doubles are below the minimum PDP-10 exponent. */
    if (exponent == 0)
	{
	*w1 = 0;
	*w2 = 0;
	return;
	}
    if (exponent == 03777) {
	/* PDP-10 D format has no infinity or NaN encoding.  Source
	** constants and folded expressions can overflow the host format;
	** diagnose them as user errors, not compiler internal failures. */
	error ("Floating constant overflow (no PDP-10 nonfinite value)");
	*w1 = *w2 = 0;
	return;
    }
    exponent -= 894; /* IEEE excess-1023 to PDP-10 excess-128. */
    if (exponent <= 0) {
	*w1 = *w2 = 0;
	return;
    }
    if (exponent > 0377) {
	error ("Floating constant exponent exceeds PDP-10 D format");
	*w1 = *w2 = 0;
	return;
    }

    /* The IEEE double's 53-bit significand fits exactly in the
    ** PDP-10 two-word 62-bit fraction.  Preserve the entire
    ** significand rather than reconstructing it in 18-bit pieces:
    ** the old emitter spilled bits into the sign-copy slot and
    ** encoded -2^35+1 with a false high-word fraction.
    */
    fraction = ((unsigned INT)1 << 52)
	| ((unsigned INT)(hi32 & 0xfffffUL) << 32)
	| (unsigned INT)lo32;
    fraction <<= 9;
    mask35 = ((unsigned INT)1 << 35) - 1;
    mask36 = ((unsigned INT)1 << 36) - 1;
    high = ((unsigned INT)exponent << 27) | (fraction >> 35);
    low = fraction & mask35;
    if (negative) {
	/* The D-format negative is a 71-bit two's complement:
	** carry from the low 35 bits propagates into the high word.
	** Bit 35 in the low storage word is the copied sign.
	*/
	low = (~low + 1) & mask35;
	high = (~high + (low == 0)) & mask36;
	low |= (unsigned INT)1 << 35;
    }
    *w1 = high;
    *w2 = low;
}

#endif /* !__COMPILER_KCC__ */
