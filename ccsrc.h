#ifndef KCC_CCSRC_H
#define KCC_CCSRC_H

#include <stdio.h>

/*
 * Raw C source reader.  This layer is below C translation phase 1.
 * It accepts host byte ASCII, native/host-container packed 7-bit ASCII,
 * and S6REC C-SIX without exposing storage encoding to the preprocessor.
 */
enum ccsrc_mode {
    CCSRC_ASCII_BYTES = 0,
    CCSRC_ASCII_WORDS,
    CCSRC_CSIX_S6REC
};

typedef struct ccsrc {
    FILE *fp;
    int mode;
    int eof;
    int error;
    int pushed;
    int pushch;
    unsigned long word;
    unsigned long word_count;
    unsigned long word_index;
    unsigned long record_left;
    unsigned int field;
    int record_newline;
} CCSRC;

int ccsrc_init(CCSRC *, FILE *);
int ccsrc_getc(CCSRC *);
int ccsrc_ungetc(int, CCSRC *);
int ccsrc_eof(const CCSRC *);
int ccsrc_error(const CCSRC *);

#endif
