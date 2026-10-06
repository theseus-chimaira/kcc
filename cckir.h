#ifndef KCC_CCKIR_H
#define KCC_CCKIR_H

#include "cc.h"

/* KIR1 - pointer-free typed parse-tree stream between KPARSE and KGEN. */
#define KIR_MAGIC "KIR1"
#define KIR_MAGIC_BYTES 4
#define KIR_VERSION 1
#define KIR_INT_CHUNKS 4
#define KIR_MAX_RECORD_WORDS 512
#define KIR_LOCAL_ID_FLAG ((unsigned INT)0200000000000)
#define KIR_LOCAL_ID_MASK ((unsigned INT)0177777777777)

enum kir_record_kind {
    KIR_REC_EXTDEF = 1,
    KIR_REC_TYPE,
    KIR_REC_SYMBOL,
    KIR_REC_NODE,
    KIR_REC_STRING,
    KIR_REC_EXTEND,
    KIR_REC_MODULE_END,
    KIR_REC_GLOBAL
};

int kir_write_header(FILE *);
int kir_write_extdef(FILE *, NODE *);
int kir_write_tentative(FILE *, NODE *);
int kir_write_globals(FILE *, SYMBOL *);
int kir_write_module_end(FILE *, int);

int kir_read_header(FILE *);
int kir_read_next(FILE *, int *, NODE **);
int kir_read_mainflag(void);
void kir_free_graph(NODE *);
void kir_free_module(void);

#endif
