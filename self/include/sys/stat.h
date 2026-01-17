#ifndef KCC_SELF_SYS_STAT_H
#define KCC_SELF_SYS_STAT_H

#include <time.h>

struct stat {
    long st_size;
    time_t st_mtime;
};

int stat(char *, struct stat *);
int fstat(int, struct stat *);

#endif
