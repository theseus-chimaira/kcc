#ifndef KCC_CCKPCODE_H
#define KCC_CCKPCODE_H

#include "cc.h"
#include "ccgen.h"

/*
 * KPCODE1 - versioned machine-oriented stream between KGEN and KOPT.
 *
 * The logical unit is one 36-bit PDP-10 word.  The hosted transport stores
 * each word in five octets (high four bits first); native word-stream I/O can
 * later replace only these transport helpers without changing record layout.
 */
#define KPCODE_MAGIC "KPCODE1"
#define KPCODE_MAGIC_BYTES 7
#define KPCODE_VERSION 1
#define KPCODE_INT_CHUNKS 4
#define KPCODE_WORD_MASK ((unsigned INT)0777777777777)

enum kpcode_record_kind {
    KPCODE_REC_MODULE_BEGIN = 1,
    KPCODE_REC_SYMBOL,
    KPCODE_REC_SYMBOL_LINK,
    KPCODE_REC_GLOBAL_SYMBOL,
    KPCODE_REC_FLUSH_BEGIN,
    KPCODE_REC_PCODE,
    KPCODE_REC_FLUSH_END,
    KPCODE_REC_OUTLAB,
    KPCODE_REC_OUTMIDEF,
    KPCODE_REC_OUTMIREF,
    KPCODE_REC_OUTPTR,
    KPCODE_REC_OUTID,
    KPCODE_REC_OUTSCON,
    KPCODE_REC_OUTNUM,
    KPCODE_REC_OUTFLT,
    KPCODE_REC_OUTNL,
    KPCODE_REC_OUTPGHDR,
    KPCODE_REC_SEGMENT,
    KPCODE_REC_TEXT,
    KPCODE_REC_MODULE_END
};

#define KPCODE_MAX_RECORD_WORDS 2048

/* KPCODE_REC_SEGMENT operation codes. */
#define KPCODE_SEG_CODE  1
#define KPCODE_SEG_DATA  2
#define KPCODE_SEG_BSS   3
#define KPCODE_SEG_PREV  4

/* Header target_flags packing. */
#define KPCODE_TF_MAPDBL_MASK 03
#define KPCODE_TF_DMOVX       04
#define KPCODE_TF_ADJSP       010
#define KPCODE_TF_ADJBP       020
#define KPCODE_TF_FLTR        040
#define KPCODE_TF_FPIMM       0100
#define KPCODE_TF_ITS         0200

/* Header compile_flags packing. */
#define KPCODE_CF_OPTOBJ      01
#define KPCODE_CF_DELETE      02
#define KPCODE_CF_LONGIDENTS  04

struct kpcode_header {
    INT target_cpu;
    INT target_arch;
    INT target_flags;
    INT asm_dialect;
    INT compile_flags;
};

enum kpcode_pcode_field {
    KPCODE_PCODE_PTYPE = 0,
    KPCODE_PCODE_POP,
    KPCODE_PCODE_PREG,
    KPCODE_PCODE_REG2,
    KPCODE_PCODE_SYMID,
    KPCODE_PCODE_OFFSET0,
    KPCODE_PCODE_OFFSET1,
    KPCODE_PCODE_OFFSET2,
    KPCODE_PCODE_OFFSET3,
    KPCODE_PCODE_AUX00,
    KPCODE_PCODE_AUX01,
    KPCODE_PCODE_AUX02,
    KPCODE_PCODE_AUX03,
    KPCODE_PCODE_AUX10,
    KPCODE_PCODE_AUX11,
    KPCODE_PCODE_AUX12,
    KPCODE_PCODE_AUX13,
    KPCODE_PCODE_WORDS
};

int kpcode_write_header(FILE *, const struct kpcode_header *);
int kpcode_read_header(FILE *, struct kpcode_header *);
int kpcode_write_record(FILE *, unsigned int, const INT *, unsigned int);
int kpcode_write_symbol(FILE *, unsigned int, const SYMBOL *);
int kpcode_write_pcode(FILE *, const PCODE *, unsigned int);
void kpcode_pack_int(INT *, INT);
int kpcode_read_record(FILE *, unsigned int *, INT *, unsigned int,
    unsigned int *);
int kpcode_decode_symbol(const INT *, unsigned int, SYMBOL *);
int kpcode_decode_pcode(const INT *, unsigned int, PCODE *, SYMBOL *);
INT kpcode_unpack_int(const INT *);

#endif
