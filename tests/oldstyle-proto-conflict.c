/* Control: a genuinely incompatible old-style definition must still fail. */
static int probe(int *);

static int
probe(p)
char *p;
{
    return p != 0;
}

int
main(void)
{
    return 0;
}
