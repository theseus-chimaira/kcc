/* Regression: old-style definitions must match compatible prototypes even
 * when KCC attaches an internal packed-pointer representation flag.
 */
static int probe(void *);

static int
probe(p)
void *p;
{
    return p != 0;
}

int
main(void)
{
    return probe((void *)0);
}
