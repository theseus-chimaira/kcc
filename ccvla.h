#ifndef KCC_CCVLA_H
#define KCC_CCVLA_H

struct type;
struct node;
struct symbol;

/* Shared VLA metadata kept outside TYPE/SYMBOL to avoid structure growth. */
int vlainfoadd_v12(struct type *, struct node *, struct symbol *, int);
void vlaboundsetsym_v12(struct type *, struct symbol *);
struct node *vlaboundexpr_v11(struct type *);
int vlaboundcaptured_v12(struct type *);
void vlaboundsetcaptured_v12(struct type *);
struct symbol *vlaboundsym_v11(struct type *);

int vlaobjaddmeta_v12(struct symbol *, struct symbol *, struct symbol *);
struct symbol *vlabase_v11(struct symbol *);
void vlaobjmark_v12(struct symbol *, struct symbol *);
struct symbol *vlaobjmarkget_v12(struct symbol *);

void vlaclear_v12(void);

#endif
