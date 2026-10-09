/*
 * Minimal native DAIMOS runtime used only by the split KCC bootstrap phases.
 *
 * DAIMOS regular files are 36-bit word streams.  Binary stdio exposes each
 * file word as four 9-bit C characters, matching KCC's native raw-word phase
 * encodings.  Text output uses five packed 7-bit ASCII characters per word;
 * native DAS accepts that representation.  Text input accepts that packed
 * ASCII form and the ordinary DAIMOS S6REC representation.
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "u.h"

#define KIO_STREAMS 16
#define KIO_BINARY  01U
#define KIO_READ    02U
#define KIO_WRITE   04U
#define KIO_TEXT_UNKNOWN 0U
#define KIO_TEXT_ASCII7  1U
#define KIO_TEXT_S6REC   2U
#define KIO_S6_TYPE_SHIFT 30U
#define KIO_S6_TEXT        1UL
#define KIO_S6_LEN_MASK    077777777UL
#define KIO_WRITE_WORDS     0200U
#define KIO_READ_WORDS      0200U

struct kio_stream {
        int fd;
        unsigned int flags;
        unsigned int field;
        unsigned int eof;
        unsigned int error;
        unsigned int pushed;
        int pushch;
        unsigned int text_kind;
        unsigned int record_left;
        unsigned int record_newline;
        kword_t word;
        kword_t *read_words;
        unsigned int read_next;
        unsigned int read_count;
        kword_t *write_words;
        unsigned int write_count;
};

static FILE kstdin_handle = 0;
static FILE kstdout_handle = 1;
static FILE kstderr_handle = 2;
FILE *stdin = &kstdin_handle;
FILE *stdout = &kstdout_handle;
FILE *stderr = &kstderr_handle;
static FILE handles[KIO_STREAMS];
static struct kio_stream streams[KIO_STREAMS];

/* DAIMOS enters native programs with argv pointing at counted SIXBIT records,
 * while the historical KCC sources expect conventional C strings.  Keep this
 * conversion in the bootstrap runtime so compiler sources and the kernel RUN
 * ABI remain independent. */
static char kcc_argv_text[SYS_RUN_ARG_MAX][SYS_RUN_ARG_MAX_CHARS + 1U];
static char *kcc_argv[SYS_RUN_ARG_MAX + 1U];

extern int main(int, char **);

int
kcc_native_start(int argc, kword_t **argv, kword_t **envp)
{
        unsigned int count;
        unsigned int i;
        unsigned int j;
        unsigned int word;
        unsigned int shift;

        (void)envp;
        if (argc < 0 || (unsigned int)argc > SYS_RUN_ARG_MAX ||
            (argc != 0 && argv == 0))
                return 126;
        for (i = 0U; i < (unsigned int)argc; ++i) {
                if (argv[i] == 0)
                        return 126;
                count = (unsigned int)(argv[i][0] & 0777777UL);
                if (count > SYS_RUN_ARG_MAX_CHARS)
                        return 126;
                for (j = 0U; j < count; ++j) {
                        word = 1U + j / 6U;
                        shift = 30U - (j % 6U) * 6U;
                        kcc_argv_text[i][j] = (char)
                            (((argv[i][word] >> shift) & 077UL) + 040UL);
                }
                kcc_argv_text[i][count] = '\0';
                kcc_argv[i] = kcc_argv_text[i];
        }
        kcc_argv[argc] = 0;
        return main(argc, kcc_argv);
}

static struct kio_stream *
kio_stream(FILE *fp)
{
        int id;

        if (fp == 0)
                return 0;
        id = *fp;
        if (id < 3 || id >= KIO_STREAMS || streams[id].flags == 0U)
                return 0;
        return &streams[id];
}

static int
kio_flush_words(struct kio_stream *s)
{
        unsigned int done;

        done = 0U;
        while (done < s->write_count) {
                int rc;

                rc = dsys_write_words(s->fd, s->write_words + done,
                    s->write_count - done);
                if (rc <= 0) {
                        s->error = 1U;
                        return EOF;
                }
                done += (unsigned int)rc;
        }
        s->write_count = 0U;
        return 0;
}

static int
kio_write_word(struct kio_stream *s)
{
        if (s->write_words == 0) {
                s->write_words = (kword_t *)malloc(
                    KIO_WRITE_WORDS * sizeof(kword_t));
                if (s->write_words == 0) {
                        s->error = 1U;
                        return EOF;
                }
        }
        s->write_words[s->write_count++] = s->word;
        s->word = 0UL;
        s->field = 0U;
        if (s->write_count == KIO_WRITE_WORDS)
                return kio_flush_words(s);
        return 0;
}

FILE *
fopen(char *name, char *mode)
{
        kword_t path[U_PATH_WORDS];
        unsigned int flags;
        int id;
        int fd;

        if (name == 0 || mode == 0 || u_s6_pack(path, U_PATH_WORDS, name) != 0)
                return 0;
        flags = 0U;
        if (mode[0] == 'r')
                flags = SYS_O_RDONLY;
        else if (mode[0] == 'w')
                flags = SYS_O_WRONLY | SYS_O_CREAT | SYS_O_TRUNC;
        else if (mode[0] == 'a')
                flags = SYS_O_WRONLY | SYS_O_CREAT | SYS_O_APPEND;
        else
                return 0;
        fd = dsys_open(path, flags);
        if (fd < 0)
                return 0;
        for (id = 3; id < KIO_STREAMS; ++id)
                if (streams[id].flags == 0U)
                        break;
        if (id == KIO_STREAMS) {
                (void)dsys_close(fd);
                return 0;
        }
        memset(&streams[id], 0, sizeof(streams[id]));
        streams[id].fd = fd;
        streams[id].flags = mode[0] == 'r' ? KIO_READ : KIO_WRITE;
        if (strchr(mode, 'b') != 0)
                streams[id].flags |= KIO_BINARY;
        handles[id] = id;
        return &handles[id];
}

int
fflush(FILE *fp)
{
        struct kio_stream *s;

        if (fp == stdout || fp == stderr)
                return 0;
        s = kio_stream(fp);
        if (s == 0 || (s->flags & KIO_WRITE) == 0U)
                return fp == stdin ? 0 : EOF;
        if (s->field != 0U && kio_write_word(s) == EOF)
                return EOF;
        return s->write_count != 0U ? kio_flush_words(s) : 0;
}

int
fclose(FILE *fp)
{
        struct kio_stream *s;
        int rc;

        if (fp == stdin || fp == stdout || fp == stderr)
                return 0;
        s = kio_stream(fp);
        if (s == 0)
                return EOF;
        rc = (s->flags & KIO_WRITE) != 0U ? fflush(fp) : 0;
        if (dsys_close(s->fd) != 0)
                rc = EOF;
        if (s->write_words != 0)
                free(s->write_words);
        if (s->read_words != 0)
                free(s->read_words);
        memset(s, 0, sizeof(*s));
        return rc;
}

int
fputc(int ch, FILE *fp)
{
        struct kio_stream *s;
        unsigned int shift;

        if (fp == stdout)
                return dsys_writechar(1, ch) == 0 ? ch : EOF;
        if (fp == stderr)
                return dsys_writechar(2, ch) == 0 ? ch : EOF;
        s = kio_stream(fp);
        if (s == 0 || (s->flags & KIO_WRITE) == 0U)
                return EOF;
        if ((s->flags & KIO_BINARY) != 0U) {
                shift = 27U - s->field * 9U;
                s->word |= ((kword_t)((unsigned int)ch & 0777U)) << shift;
                if (++s->field == 4U && kio_write_word(s) == EOF)
                        return EOF;
        } else {
                shift = 29U - s->field * 7U;
                s->word |= ((kword_t)((unsigned int)ch & 0177U)) << shift;
                if (++s->field == 5U && kio_write_word(s) == EOF)
                        return EOF;
        }
        return ch;
}

int
putc(int ch, FILE *fp)
{
        return fputc(ch, fp);
}

int
fputs(char *text, FILE *fp)
{
        if (text == 0)
                return EOF;
        while (*text != 0)
                if (fputc((unsigned char)*text++, fp) == EOF)
                        return EOF;
        return 0;
}

int
puts(char *text)
{
        return fputs(text, stdout) == EOF || fputc('\n', stdout) == EOF ?
            EOF : 0;
}

static int
kio_read_word(struct kio_stream *s)
{
        int rc;

        if (s->read_next == s->read_count) {
                if (s->read_words == 0) {
                        s->read_words = (kword_t *)malloc(
                            KIO_READ_WORDS * sizeof(kword_t));
                        if (s->read_words == 0) {
                                s->error = 1U;
                                return 0;
                        }
                }
                rc = dsys_read_words(s->fd, s->read_words, KIO_READ_WORDS);
                if (rc <= 0 || (unsigned int)rc > KIO_READ_WORDS) {
                        if (rc == 0)
                                s->eof = 1U;
                        else
                                s->error = 1U;
                        return 0;
                }
                s->read_next = 0U;
                s->read_count = (unsigned int)rc;
        }
        s->word = s->read_words[s->read_next++];
        return 1;
}

int
getc(FILE *fp)
{
        struct kio_stream *s;
        unsigned int shift;
        unsigned int ch;
        kword_t len;

        if (fp == stdin)
                return dsys_readchar(0);
        s = kio_stream(fp);
        if (s == 0 || (s->flags & KIO_READ) == 0U)
                return EOF;
        if (s->pushed) {
                s->pushed = 0U;
                return s->pushch;
        }
        if ((s->flags & KIO_BINARY) != 0U) {
                if (s->field == 0U && !kio_read_word(s))
                        return EOF;
                shift = 27U - s->field * 9U;
                ch = (unsigned int)((s->word >> shift) & 0777UL);
                if (++s->field == 4U)
                        s->field = 0U;
                return (int)ch;
        }

        if (s->text_kind == KIO_TEXT_S6REC) {
                if (s->record_newline) {
                        s->record_newline = 0U;
                        return '\n';
                }
                if (s->record_left == 0U) {
                        if (!kio_read_word(s))
                                return EOF;
                        len = s->word & KIO_S6_LEN_MASK;
                        if (((s->word >> KIO_S6_TYPE_SHIFT) & 077UL) !=
                            KIO_S6_TEXT) {
                                s->error = 1U;
                                return EOF;
                        }
                        s->record_left = (unsigned int)len;
                        s->field = 0U;
                        if (s->record_left == 0U)
                                return '\n';
                }
                if (s->field == 0U && !kio_read_word(s)) {
                        s->error = 1U;
                        return EOF;
                }
                shift = 30U - s->field * 6U;
                ch = (unsigned int)((s->word >> shift) & 077UL) + 040U;
                if (++s->field == 6U)
                        s->field = 0U;
                if (--s->record_left == 0U)
                        s->record_newline = 1U;
                return (int)ch;
        }

        if (s->field == 0U) {
                if (!kio_read_word(s))
                        return EOF;
                if (s->text_kind == KIO_TEXT_UNKNOWN &&
                    ((s->word >> KIO_S6_TYPE_SHIFT) & 077UL) == KIO_S6_TEXT &&
                    (s->word & KIO_S6_LEN_MASK) <= 07777UL) {
                        s->text_kind = KIO_TEXT_S6REC;
                        s->record_left = (unsigned int)(s->word & KIO_S6_LEN_MASK);
                        if (s->record_left == 0U)
                                return '\n';
                        if (!kio_read_word(s)) {
                                s->error = 1U;
                                return EOF;
                        }
                        shift = 30U;
                        ch = (unsigned int)((s->word >> shift) & 077UL) + 040U;
                        s->field = 1U;
                        if (--s->record_left == 0U)
                                s->record_newline = 1U;
                        return (int)ch;
                }
                s->text_kind = KIO_TEXT_ASCII7;
        }
        shift = 29U - s->field * 7U;
        ch = (unsigned int)((s->word >> shift) & 0177UL);
        if (++s->field == 5U)
                s->field = 0U;
        if (ch == 0U) {
                s->eof = 1U;
                return EOF;
        }
        return (int)ch;
}

int
ungetc(int ch, FILE *fp)
{
        struct kio_stream *s;

        if (ch == EOF || fp == stdin)
                return EOF;
        s = kio_stream(fp);
        if (s == 0 || s->pushed)
                return EOF;
        s->pushed = 1U;
        s->pushch = ch;
        s->eof = 0U;
        return ch;
}

unsigned int
fread(void *dst, unsigned int size, unsigned int count, FILE *fp)
{
        unsigned char *p;
        unsigned int total;
        unsigned int done;
        int ch;

        if (size == 0U || count == 0U)
                return 0U;
        if (count > (~0U) / size)
                return 0U;
        total = size * count;
        p = (unsigned char *)dst;
        for (done = 0U; done < total; ++done) {
                ch = getc(fp);
                if (ch == EOF)
                        break;
                p[done] = (unsigned char)ch;
        }
        return done / size;
}

int
fseek(FILE *fp, long offset, int whence)
{
        struct kio_stream *s;

        s = kio_stream(fp);
        if (s == 0 || offset != 0L || whence != SEEK_SET)
                return -1;
        /* A read stream has no pending output to flush.  Calling fflush()
         * there used to return EOF and made every scanner rewind fail after
         * a successful first pass (notably C-SIX/S6REC detection in KCPP). */
        if (((s->flags & KIO_WRITE) != 0U && fflush(fp) == EOF) ||
            dsys_seek(s->fd, 0UL, SYS_SEEK_SET) == (kword_t)-1L)
                return -1;
        s->field = s->eof = s->error = s->pushed = 0U;
        s->text_kind = KIO_TEXT_UNKNOWN;
        s->record_left = s->record_newline = 0U;
        s->word = 0UL;
        s->read_next = s->read_count = 0U;
        return 0;
}

void
rewind(FILE *fp)
{
        (void)fseek(fp, 0L, SEEK_SET);
}

int
feof(FILE *fp)
{
        struct kio_stream *s = kio_stream(fp);
        return s != 0 ? (int)s->eof : 0;
}

int
ferror(FILE *fp)
{
        struct kio_stream *s = kio_stream(fp);
        return s != 0 ? (int)s->error : 0;
}

void
clearerr(FILE *fp)
{
        struct kio_stream *s = kio_stream(fp);
        if (s != 0)
                s->eof = s->error = 0U;
}

static int
fmt_put(FILE *fp, char **dst, int ch)
{
        if (fp != 0)
                return fputc(ch, fp) == EOF ? -1 : 0;
        *(*dst)++ = (char)ch;
        return 0;
}

static int
fmt_uint(FILE *fp, char **dst, unsigned long value, unsigned int base,
    int upper, int width, int zero)
{
        char buf[32];
        char *digits;
        int n;
        int i;

        digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
        n = 0;
        do {
                buf[n++] = digits[value % base];
                value /= base;
        } while (value != 0UL);
        for (i = n; i < width; ++i)
                if (fmt_put(fp, dst, zero ? '0' : ' ') != 0)
                        return -1;
        while (n-- != 0)
                if (fmt_put(fp, dst, buf[n]) != 0)
                        return -1;
        return i > width ? i : width;
}

static int
fmt_core(FILE *fp, char **dst, char *fmt, va_list *app)
{
        int count;
        int width;
        int precision;
        int zero;
        int alt;
        int longarg;
        int ch;
        long sv;
        unsigned long uv;
        char *s;

        count = 0;
        while ((ch = (unsigned char)*fmt++) != 0) {
                if (ch != '%') {
                        if (fmt_put(fp, dst, ch) != 0) return -1;
                        ++count;
                        continue;
                }
                alt = zero = longarg = 0;
                width = 0;
                precision = -1;
                if (*fmt == '#') { alt = 1; ++fmt; }
                if (*fmt == '-') ++fmt;
                if (*fmt == '0') { zero = 1; ++fmt; }
                while (*fmt >= '0' && *fmt <= '9')
                        width = width * 10 + (*fmt++ - '0');
                if (*fmt == '.') {
                        ++fmt;
                        precision = 0;
                        while (*fmt >= '0' && *fmt <= '9')
                                precision = precision * 10 + (*fmt++ - '0');
                }
                if (*fmt == 'l') { longarg = 1; ++fmt; }
                ch = (unsigned char)*fmt++;
                if (ch == '%') {
                        if (fmt_put(fp, dst, '%') != 0) return -1;
                        ++count;
                } else if (ch == 'c') {
                        if (fmt_put(fp, dst, va_arg(*app, int)) != 0) return -1;
                        ++count;
                } else if (ch == 's') {
                        s = va_arg(*app, char *);
                        if (s == 0) s = "(null)";
                        while (*s != 0 && precision != 0) {
                                if (fmt_put(fp, dst, (unsigned char)*s++) != 0) return -1;
                                ++count;
                                if (precision > 0) --precision;
                        }
                } else if (ch == 'd' || ch == 'i') {
                        sv = longarg ? va_arg(*app, long) : (long)va_arg(*app, int);
                        if (sv < 0) {
                                if (fmt_put(fp, dst, '-') != 0) return -1;
                                ++count;
                                uv = (unsigned long)(-sv);
                        } else uv = (unsigned long)sv;
                        if (fmt_uint(fp, dst, uv, 10U, 0, width, zero) < 0) return -1;
                } else if (ch == 'u' || ch == 'o' || ch == 'x' || ch == 'X') {
                        uv = longarg ? va_arg(*app, unsigned long) :
                            (unsigned long)va_arg(*app, unsigned int);
                        if (alt && ch == 'o') {
                                if (fmt_put(fp, dst, '0') != 0) return -1;
                                ++count;
                        }
                        if (fmt_uint(fp, dst, uv, ch == 'o' ? 8U :
                            (ch == 'u' ? 10U : 16U), ch == 'X', width, zero) < 0)
                                return -1;
                } else if (ch == 'p') {
                        uv = (unsigned long)va_arg(*app, void *);
                        if (fmt_uint(fp, dst, uv, 8U, 0, width, zero) < 0) return -1;
                } else if (ch == 'f' || ch == 'e' || ch == 'E' ||
                    ch == 'g' || ch == 'G') {
                        /* Native KCC only uses this path for human-readable
                         * assembler/debug comments.  Preserve valid assembly
                         * without carrying a decimal floating formatter. */
                        (void)va_arg(*app, double);
                        if (fmt_put(fp, dst, '0') != 0) return -1;
                        ++count;
                } else {
                        if (fmt_put(fp, dst, ch) != 0) return -1;
                        ++count;
                }
        }
        if (fp == 0)
                **dst = 0;
        return count;
}

int
fprintf(FILE *fp, char *fmt, ...)
{
        va_list ap;
        int rc;

        va_start(ap, fmt);
        rc = fmt_core(fp, 0, fmt, &ap);
        va_end(ap);
        return rc;
}

int
printf(char *fmt, ...)
{
        va_list ap;
        int rc;

        va_start(ap, fmt);
        rc = fmt_core(stdout, 0, fmt, &ap);
        va_end(ap);
        return rc;
}

int
sprintf(char *dst, char *fmt, ...)
{
        va_list ap;
        char *p;
        int rc;

        p = dst;
        va_start(ap, fmt);
        rc = fmt_core(0, &p, fmt, &ap);
        va_end(ap);
        return rc;
}

char *
ctime(long *tp)
{
        static char text[26];
        static char *months[12] = {
                "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };
        unsigned long t;
        unsigned int ybcd;
        unsigned int year;
        unsigned int mon;
        unsigned int day;
        unsigned int hour;
        unsigned int min;
        unsigned int sec;

        if (tp == 0)
                return 0;
        t = (unsigned long)*tp;
        ybcd = (unsigned int)((t >> 26U) & 0377UL);
        year = ((ybcd >> 4U) & 017U) * 10U + (ybcd & 017U);
        year += (t & 0200000000000UL) != 0UL ? 2100U : 2000U;
        mon = (unsigned int)((t >> 22U) & 017UL);
        day = (unsigned int)((t >> 17U) & 037UL);
        hour = (unsigned int)((t >> 12U) & 037UL);
        min = (unsigned int)((t >> 6U) & 077UL);
        sec = (unsigned int)(t & 077UL);
        if (mon == 0U || mon > 12U)
                mon = 1U;
        sprintf(text, "Sun %s %2u %02u:%02u:%02u %04u\n",
            months[mon - 1U], day, hour, min, sec, year);
        return text;
}

long
clock(void)
{
        return 0L;
}
