/* OS-independent less-frequent string routines.
 * Hot comparison/length primitives use separate PDP-6 assembler modules.
 */
char *
strcpy(char *dst, char *src)
{
        char *ret;

        ret = dst;
        while ((*dst++ = *src++) != 0)
                ;
        return ret;
}

char *
strncpy(char *dst, char *src, unsigned int count)
{
        char *ret;

        ret = dst;
        while (count != 0U && *src != 0) {
                *dst++ = *src++;
                --count;
        }
        while (count-- != 0U)
                *dst++ = 0;
        return ret;
}

char *
strcat(char *dst, char *src)
{
        char *ret;

        ret = dst;
        while (*dst != 0)
                ++dst;
        while ((*dst++ = *src++) != 0)
                ;
        return ret;
}

char *
strncat(char *dst, char *src, unsigned int count)
{
        char *ret;

        ret = dst;
        while (*dst != 0)
                ++dst;
        while (count-- != 0U && *src != 0)
                *dst++ = *src++;
        *dst = 0;
        return ret;
}

char *
strchr(char *s, int ch)
{
        for (;;) {
                if ((unsigned char)*s == (unsigned char)ch)
                        return s;
                if (*s++ == 0)
                        return 0;
        }
}

char *
strrchr(char *s, int ch)
{
        char *last;

        last = 0;
        for (;;) {
                if ((unsigned char)*s == (unsigned char)ch)
                        last = s;
                if (*s++ == 0)
                        return last;
        }
}

char *
strtok(char *s, char *delim)
{
        static char *next;
        char *start;

        if (s != 0)
                next = s;
        if (next == 0)
                return 0;
        while (*next != 0 && strchr(delim, *next) != 0)
                ++next;
        if (*next == 0) {
                next = 0;
                return 0;
        }
        start = next;
        while (*next != 0 && strchr(delim, *next) == 0)
                ++next;
        if (*next != 0)
                *next++ = 0;
        else
                next = 0;
        return start;
}

/* Number of initial characters not occurring in the rejection set. */
unsigned int
strcspn(const char *s, const char *reject)
{
        const char *p;
        const char *q;

        for (p = s; *p != 0; ++p) {
                for (q = reject; *q != 0; ++q) {
                        if (*p == *q)
                                return (unsigned int)(p - s);
                }
        }
        return (unsigned int)(p - s);
}
