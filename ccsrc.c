/*
 * ccsrc.c - DAIMOS/host raw C-source storage decoder.
 *
 * C-SIX is deliberately decoded before KCPP translation phase 1.  This
 * module therefore knows nothing about C tokens, strings, comments, or
 * trigraph semantics.
 */
#include "ccsrc.h"

#include <string.h>

#define CCSRC_WORD_MASK       0777777777777UL
#define CCSRC_S6_TYPE_SHIFT   30U
#define CCSRC_S6_TYPE_MASK    077UL
#define CCSRC_S6_TEXT         1UL
#define CCSRC_S6_LEN_MASK     077777777UL

static int raw_word_get(CCSRC *, unsigned long *);
static int raw_rewind(CCSRC *);
static int host_is_word_container(CCSRC *, unsigned long *);
static int scan_s6rec(CCSRC *);
static int physical_getc(CCSRC *);
static int csix_getc(CCSRC *);

/* Read one native 36-bit word.  Hosted tools use the canonical eight-byte
 * little-endian container.  Native KCC's C byte is 9 bits, so four binary
 * C bytes carry one 36-bit file word, most-significant 9-bit byte first. */
static int
raw_word_get(CCSRC *s, unsigned long *wp)
{
#ifdef __COMPILER_KCC__
    unsigned char b[4];
    size_t n;

    n = fread(b, 1, 4, s->fp);
    if (n == 0) {
        if (ferror(s->fp)) s->error = 1;
        return 0;
    }
    if (n != 4) {
        s->error = 1;
        return 0;
    }
    *wp = ((unsigned long)b[0] << 27)
        | ((unsigned long)b[1] << 18)
        | ((unsigned long)b[2] << 9)
        | (unsigned long)b[3];
#else
    unsigned char b[8];
    size_t n;
    unsigned long w;

    n = fread(b, 1, 8, s->fp);
    if (n == 0) {
        if (ferror(s->fp)) s->error = 1;
        return 0;
    }
    if (n != 8 || (b[4] & 0360U) != 0U || b[5] != 0U ||
        b[6] != 0U || b[7] != 0U) {
        s->error = 1;
        return 0;
    }
    w = (unsigned long)b[0]
        | ((unsigned long)b[1] << 8)
        | ((unsigned long)b[2] << 16)
        | ((unsigned long)b[3] << 24)
        | ((unsigned long)(b[4] & 017U) << 32);
    *wp = w;
#endif
    s->word_index++;
    return 1;
}

static int
raw_rewind(CCSRC *s)
{
    if (fseek(s->fp, 0L, SEEK_SET) != 0) {
        s->error = 1;
        return -1;
    }
    clearerr(s->fp);
    s->word_index = 0;
    s->field = 0;
    return 0;
}

/* Hosted byte-stream ASCII is not a PDP-10 word container.  Native DAIMOS
 * files are inherently word streams, so no container test is necessary. */
static int
host_is_word_container(CCSRC *s, unsigned long *countp)
{
#ifdef __COMPILER_KCC__
    unsigned long w, n;

    n = 0;
    while (raw_word_get(s, &w)) n++;
    if (s->error) return 0;
    *countp = n;
    (void)raw_rewind(s);
    return 1;
#else
    unsigned char b[8];
    size_t n;
    unsigned long words;

    words = 0;
    if (fseek(s->fp, 0L, SEEK_SET) != 0) return 0;
    for (;;) {
        n = fread(b, 1, 8, s->fp);
        if (n == 0) break;
        if (n != 8 || (b[4] & 0360U) != 0U || b[5] != 0U ||
            b[6] != 0U || b[7] != 0U) {
            (void)fseek(s->fp, 0L, SEEK_SET);
            clearerr(s->fp);
            return 0;
        }
        words++;
    }
    (void)fseek(s->fp, 0L, SEEK_SET);
    clearerr(s->fp);
    *countp = words;
    return words != 0;
#endif
}

/* Strictly validate the complete word stream as S6REC text. */
static int
scan_s6rec(CCSRC *s)
{
    unsigned long h, len, words, i;
    int saw;

    saw = 0;
    s->error = 0;
    if (raw_rewind(s) != 0) return 0;
    for (;;) {
        if (!raw_word_get(s, &h)) {
            if (s->error) goto no;
            break;
        }
        if (((h >> CCSRC_S6_TYPE_SHIFT) & CCSRC_S6_TYPE_MASK)
                != CCSRC_S6_TEXT)
            goto no;
        len = h & CCSRC_S6_LEN_MASK;
        if (h != ((CCSRC_S6_TEXT << CCSRC_S6_TYPE_SHIFT) | len))
            goto no;
        words = (len + 5UL) / 6UL;
        for (i = 0; i < words; ++i)
            if (!raw_word_get(s, &h)) goto no;
        saw = 1;
    }
    /* The validation pass has already counted every input word.
     * Native KCPP need not read the whole file again just to count it. */
    s->word_count = s->word_index;
    s->error = 0;
    (void)raw_rewind(s);
    return saw;
no:
    s->error = 0;
    (void)raw_rewind(s);
    return 0;
}

int
ccsrc_init(CCSRC *s, FILE *fp)
{
    unsigned long words;

    memset(s, 0, sizeof(*s));
    s->fp = fp;
    words = 0;
#ifdef __COMPILER_KCC__
    /* DAIMOS opens native input as packed words.  A valid S6REC scan
     * both validates the format and counts the words in one pass. */
    if (scan_s6rec(s)) {
        s->mode = CCSRC_CSIX_S6REC;
        s->record_left = 0;
        s->record_newline = 0;
        return raw_rewind(s);
    }
#endif
    if (!host_is_word_container(s, &words)) {
        s->mode = CCSRC_ASCII_BYTES;
        s->error = 0;
        s->eof = 0;
        if (fseek(fp, 0L, SEEK_SET) != 0) return -1;
        clearerr(fp);
        return 0;
    }
    s->word_count = words;
    s->mode = scan_s6rec(s) ? CCSRC_CSIX_S6REC : CCSRC_ASCII_WORDS;
    s->error = 0;
    s->eof = 0;
    s->record_left = 0;
    s->record_newline = 0;
    return raw_rewind(s);
}

static int
physical_getc(CCSRC *s)
{
    unsigned long h, len;
    unsigned int shift, c;

    if (s->mode == CCSRC_ASCII_BYTES) {
        c = (unsigned int)getc(s->fp);
        if ((int)c == EOF) {
            if (ferror(s->fp)) s->error = 1;
            else s->eof = 1;
            return EOF;
        }
        return (int)(c & 0177U);
    }

    if (s->mode == CCSRC_ASCII_WORDS) {
        for (;;) {
            if (s->field == 0) {
                if (!raw_word_get(s, &s->word)) {
                    if (!s->error) s->eof = 1;
                    return EOF;
                }
            }
            shift = 29U - 7U * s->field;
            c = (unsigned int)((s->word >> shift) & 0177UL);
            if (++s->field == 5U) s->field = 0;
            if (c != 0U) return (int)c;
            /* Zero is padding only in the final packed word. */
            if (s->word_index == s->word_count) {
                s->eof = 1;
                return EOF;
            }
            s->error = 1;
            return EOF;
        }
    }

    /* S6REC: each record is one logical source line. */
    if (s->record_newline) {
        s->record_newline = 0;
        return '\n';
    }
    if (s->record_left == 0) {
        if (!raw_word_get(s, &h)) {
            if (!s->error) s->eof = 1;
            return EOF;
        }
        len = h & CCSRC_S6_LEN_MASK;
        if (((h >> CCSRC_S6_TYPE_SHIFT) & CCSRC_S6_TYPE_MASK)
                != CCSRC_S6_TEXT
            || h != ((CCSRC_S6_TEXT << CCSRC_S6_TYPE_SHIFT) | len)) {
            s->error = 1;
            return EOF;
        }
        s->record_left = len;
        s->field = 0;
        if (len == 0) {
            s->record_newline = 0;
            return '\n';
        }
    }
    if (s->field == 0 && !raw_word_get(s, &s->word)) {
        s->error = 1;
        return EOF;
    }
    shift = 30U - 6U * s->field;
    c = (unsigned int)((s->word >> shift) & 077UL) + 040U;
    if (++s->field == 6U) s->field = 0;
    if (--s->record_left == 0) s->record_newline = 1;
    return (int)c;
}

static int
csix_getc(CCSRC *s)
{
    int c, e;

    c = physical_getc(s);
    if (c == EOF) return EOF;
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    if (c != '@') return c;
    e = physical_getc(s);
    if (e == EOF || e == '\n') {
        s->error = 1;
        return EOF;
    }
    if (e >= 'A' && e <= 'Z') return e;
    switch (e) {
    case '@': return '@';
    case '\'': return '`';
    case '<': return '{';
    case '!': return '|';
    case '>': return '}';
    case '-': return '~';
    default:
        s->error = 1;
        return EOF;
    }
}

int
ccsrc_getc(CCSRC *s)
{
    int c;

    if (s->pushed) {
        s->pushed = 0;
        return s->pushch;
    }
    c = (s->mode == CCSRC_CSIX_S6REC) ? csix_getc(s) : physical_getc(s);
    return c;
}

int
ccsrc_ungetc(int c, CCSRC *s)
{
    if (c == EOF || s->pushed) return EOF;
    s->pushed = 1;
    s->pushch = c;
    s->eof = 0;
    return c;
}

int
ccsrc_eof(const CCSRC *s)
{
    return s->eof;
}

int
ccsrc_error(const CCSRC *s)
{
    return s->error;
}
