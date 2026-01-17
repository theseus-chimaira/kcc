#ifndef KCC_SELF_STDIO_H
#define KCC_SELF_STDIO_H

#include <stddef.h>

#define FILE int

#define EOF (-1)
#define BUFSIZ 512

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int fclose(FILE *);
int feof(FILE *);
int ferror(FILE *);
int fflush(FILE *);
int fscanf(FILE *, char *, ...);
int fprintf(FILE *, char *, ...);
int fputc(int, FILE *);
int fputs(char *, FILE *);
size_t fread(void *, size_t, size_t, FILE *);
int getc(FILE *);
int printf(char *, ...);
int putc(int, FILE *);
int puts(char *);
void rewind(FILE *);
int sprintf(char *, char *, ...);
FILE *tmpfile(void);
int ungetc(int, FILE *);
FILE *fopen(char *, char *);

#endif
