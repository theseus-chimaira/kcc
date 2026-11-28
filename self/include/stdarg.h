#ifndef KCC_SELF_STDARG_H
#define KCC_SELF_STDARG_H

#if defined(__GNUC__) && !defined(__COMPILER_KCC__)
/* The native KCC varargs stack convention below is not GCC's PDP-10
 * variadic calling convention.  Let GCC use its own target builtins and
 * va_list representation; mixing these silently reverses/corrupts
 * fprintf arguments even when every translation unit is GCC-built. */
#include_next <stdarg.h>
#else

#define va_list char *

#define va_start(ap, last) ((ap) = (char *)&(last))
/* GCC ABI unnamed arguments are stack-only.  KCC reconstructs named
 * register arguments above them in its private callee stack view, so the
 * first unnamed argument is immediately below the last named parameter.
 */
/* Stack slots occupy complete native words, even for packed aggregates
 * whose C sizeof is not a multiple of the word size. */
#define va_arg(ap, type) (*(type *)((ap) -= \
    ((sizeof(type) + sizeof(int) - 1) / sizeof(int)) * sizeof(int)))
/*
 * This ABI has no va_list teardown operation.  The macro still must
 * evaluate its argument exactly once: callers may supply an lvalue
 * expression with side effects (for example va_end(**slot++)).
 * Discarding the argument incorrectly omits those side effects.
 */
#define va_end(ap) ((void)(ap))
#define va_copy(dst, src) ((dst) = (src))

#endif
#endif
