#ifndef KCC_SELF_TIME_H
#define KCC_SELF_TIME_H

#define time_t long
#define clock_t long
#define CLOCKS_PER_SEC 100

char *ctime(time_t *);
time_t time(time_t *);
clock_t clock(void);

#endif
