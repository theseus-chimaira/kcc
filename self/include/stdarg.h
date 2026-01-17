#ifndef KCC_SELF_STDARG_H
#define KCC_SELF_STDARG_H

#define va_list char *

#define va_start(ap, last) ((ap) = (char *)&(last))
/* GCC ABI unnamed arguments are stack-only.  KCC reconstructs named
 * register arguments above them in its private callee stack view, so the
 * first unnamed argument is immediately below the last named parameter.
 */
#define va_arg(ap, type) (*(type *)((ap) -= sizeof(type)))
#define va_end(ap) ((void)0)
#define va_copy(dst, src) ((dst) = (src))

#endif
