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
#define KPCODE_WORD_MASK ((unsigned INT)0777777777777)

enum kpcode_record_kind {
    KPCODE_REC_MODULE_BEGIN = 1,
    KPCODE_REC_SYMBOL,
    KPCODE_REC_PCODE,
    KPCODE_REC_LABEL,
    KPCODE_REC_SEGMENT,
    KPCODE_REC_TEXT,
    KPCODE_REC_MODULE_END
};

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
    KPCODE_PCODE_OFFSET,
    KPCODE_PCODE_AUX0,
    KPCODE_PCODE_AUX1,
    KPCODE_PCODE_WORDS
};

int kpcode_write_header(FILE *, const struct kpcode_header *);
int kpcode_read_header(FILE *, struct kpcode_header *);
int kpcode_write_record(FILE *, unsigned int, const INT *, unsigned int);
int kpcode_write_symbol(FILE *, unsigned int, const SYMBOL *);
int kpcode_write_pcode(FILE *, const PCODE *, unsigned int);
int kpcode_read_record(FILE *, unsigned int *, INT *, unsigned int,
    unsigned int *);

#endif
