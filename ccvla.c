/* CCVLA.C - shared variable-length-array metadata. */

#include "cc.h"
#include "ccvla.h"
#include <stdlib.h>

struct vlainfo_v12 {
    TYPE *type;
    NODE *bound;
    SYMBOL *boundsym;
    int captured;
    struct vlainfo_v12 *next;
};

struct vlaobj_v12 {
    SYMBOL *object;
    SYMBOL *base;
    SYMBOL *mark;
    struct vlaobj_v12 *next;
};

static struct vlainfo_v12 *vlainfos_v12;
static struct vlaobj_v12 *vlaobjs_v12;

static struct vlainfo_v12 *
vlafind_v12(TYPE *t)
{
    struct vlainfo_v12 *v;
    for (v = vlainfos_v12; v != NULL; v = v->next)
        if (v->type == t)
            return v;
    return NULL;
}

static struct vlaobj_v12 *
vlaobjfind_v12(SYMBOL *s)
{
    struct vlaobj_v12 *v;
    for (v = vlaobjs_v12; v != NULL; v = v->next)
        if (v->object == s)
            return v;
    return NULL;
}

int
vlainfoadd_v12(TYPE *t, NODE *bound, SYMBOL *boundsym, int captured)
{
    struct vlainfo_v12 *v;

    if (t == NULL)
        return -1;
    v = vlafind_v12(t);
    if (v == NULL) {
        v = (struct vlainfo_v12 *)calloc(1, sizeof(*v));
        if (v == NULL)
            return -1;
        v->type = t;
        v->next = vlainfos_v12;
        vlainfos_v12 = v;
    }
    v->bound = bound;
    v->boundsym = boundsym;
    v->captured = captured != 0;
    return 0;
}

void
vlaboundsetsym_v12(TYPE *t, SYMBOL *s)
{
    struct vlainfo_v12 *v = vlafind_v12(t);
    if (v != NULL)
        v->boundsym = s;
}

NODE *
vlaboundexpr_v11(TYPE *t)
{
    struct vlainfo_v12 *v = vlafind_v12(t);
    return v != NULL ? v->bound : NULL;
}

int
vlaboundcaptured_v12(TYPE *t)
{
    struct vlainfo_v12 *v = vlafind_v12(t);
    return v != NULL ? v->captured : 0;
}

void
vlaboundsetcaptured_v12(TYPE *t)
{
    struct vlainfo_v12 *v = vlafind_v12(t);
    if (v != NULL)
        v->captured = 1;
}

SYMBOL *
vlaboundsym_v11(TYPE *t)
{
    struct vlainfo_v12 *v = vlafind_v12(t);
    return v != NULL ? v->boundsym : NULL;
}

int
vlaobjaddmeta_v12(SYMBOL *object, SYMBOL *base, SYMBOL *mark)
{
    struct vlaobj_v12 *v;

    if (object == NULL)
        return -1;
    v = vlaobjfind_v12(object);
    if (v == NULL) {
        v = (struct vlaobj_v12 *)calloc(1, sizeof(*v));
        if (v == NULL)
            return -1;
        v->object = object;
        v->next = vlaobjs_v12;
        vlaobjs_v12 = v;
    }
    v->base = base;
    v->mark = mark;
    return 0;
}

SYMBOL *
vlabase_v11(SYMBOL *s)
{
    struct vlaobj_v12 *v = vlaobjfind_v12(s);
    return v != NULL ? v->base : NULL;
}

void
vlaobjmark_v12(SYMBOL *s, SYMBOL *mark)
{
    struct vlaobj_v12 *v = vlaobjfind_v12(s);
    if (v != NULL) {
        v->mark = mark;
        return;
    }
    int_error("vlaobjmark_v12: missing VLA object");
}

SYMBOL *
vlaobjmarkget_v12(SYMBOL *s)
{
    struct vlaobj_v12 *v = vlaobjfind_v12(s);
    return v != NULL ? v->mark : NULL;
}

void
vlaclear_v12(void)
{
    struct vlainfo_v12 *vi, *vin;
    struct vlaobj_v12 *vo, *von;

    for (vi = vlainfos_v12; vi != NULL; vi = vin) {
        vin = vi->next;
        free(vi);
    }
    for (vo = vlaobjs_v12; vo != NULL; vo = von) {
        von = vo->next;
        free(vo);
    }
    vlainfos_v12 = NULL;
    vlaobjs_v12 = NULL;
}
