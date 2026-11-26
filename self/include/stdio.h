#ifndef KCC_SELF_STDIO_H
#define KCC_SELF_STDIO_H

#include <stddef.h>
#include <stdarg.h>

#define FILE int

#define EOF (-1)
#define BUFSIZ 512
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int fclose(FILE *);
void clearerr(FILE *);
int feof(FILE *);
int ferror(FILE *);
int fflush(FILE *);
int fscanf(FILE *, char *, ...);
int fprintf(FILE *, char *, ...);
int fputc(int, FILE *);
int fputs(char *, FILE *);
size_t fread(void *, size_t, size_t, FILE *);
int fseek(FILE *, long, int);
int getc(FILE *);
int printf(char *, ...);
int vprintf(char *, va_list);
int vsprintf(char *, const char *, va_list);
int vsnprintf(char *, size_t, const char *, va_list);
int putc(int, FILE *);
int puts(char *);
void rewind(FILE *);
int sprintf(char *, const char *, ...);
int snprintf(char *, size_t, const char *, ...);
FILE *tmpfile(void);
int ungetc(int, FILE *);
FILE *fopen(char *, char *);

#endif
