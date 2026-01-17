#ifndef KCC_SELF_STDLIB_H
#define KCC_SELF_STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

int abs(int);
int atoi(char *);
void exit(int);
void free(void *);
void *calloc(size_t, size_t);
void *malloc(size_t);
void *realloc(void *, size_t);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
int remove(const char *);

#endif
