/* CCOUTUTIL.C - small target/output helpers shared by KGEN and CCOUT. */

#include "cc.h"
#include "ccgen.h"

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
