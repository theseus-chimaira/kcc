#ifndef KCC_SELF_STRING_H
#define KCC_SELF_STRING_H

#include <stddef.h>

int memcmp(void *, void *, size_t);
void *memcpy(void *, void *, size_t);
void *memmove(void *, void *, size_t);
void *memset(void *, int, size_t);
char *strcat(char *, char *);
char *strchr(char *, int);
char *strstr(const char *, const char *);
char *strpbrk(const char *, const char *);
size_t strspn(const char *, const char *);
char *strrchr(char *, int);
int strcmp(char *, char *);
char *strcpy(char *, char *);
size_t strcspn(const char *, const char *);
char *strerror(int);
size_t strlen(char *);
char *strncat(char *, char *, size_t);
int strncmp(char *, char *, size_t);
char *strncpy(char *, char *, size_t);
char *strtok(char *, char *);

#endif
