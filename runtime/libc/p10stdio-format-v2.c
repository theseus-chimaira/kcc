/*
 * PDP-10 freestanding formatted output, shared engine v2.
 *
 * C89, no static writable data, no heap, no operating system and no
 * reliance on 8-bit bytes or 32-bit integers.  GCC 3.2 uses its own
 * native PDP-10 stdarg ABI.  The one formatting engine supports
 * both caller-owned buffers and optional OS-provided character sinks.
 * The core has NO unresolved OS I/O symbols.
 *
 * Supported: d, i, u, o, x, X, c, s, p, %% and h/l integer qualifiers.
 * Flags: -, +, space, #, 0; decimal width and precision, including *.
 * Unsupported format conversions return -1 instead of emitting an
 * incorrect result.  Floating-point and file streams are not provided
 * in this initial freestanding member.
 */

#include <stdarg.h>
#include <stddef.h>
#include <limits.h>

struct p10stdio_sink_v2 {
    char *buffer;
    size_t capacity;
    size_t count;
    int (*emit)(void *, int);
    void *emit_context;
    int failed;
};

static void
p10stdio_emit_v2(struct p10stdio_sink_v2 *out, int ch)
{
    /* Numerical formatting may issue several emissions in one call.
     * Once an I/O sink fails, never invoke it a second time. */
    if (out->failed)
        return;
    if (out->count == (size_t)-1) {
        out->failed = 1;
        return;
    }
    if (out->emit != (int (*)(void *, int))0) {
        if ((*out->emit)(out->emit_context, ch) != 0) {
            out->failed = 1;
            return;
        }
    } else if (out->capacity != 0 && out->count < out->capacity - 1)
        out->buffer[out->count] = (char)ch;
    ++out->count;
}

static void
p10stdio_repeat_v2(struct p10stdio_sink_v2 *out, int ch, int count)
{
    while (count-- > 0 && !out->failed)
        p10stdio_emit_v2(out, ch);
}

static int
p10stdio_digits_v2(unsigned long value, unsigned int base,
                   int upper, char *digits)
{
    static const char lower[] = "0123456789abcdef";
    static const char capital[] = "0123456789ABCDEF";
    const char *table = upper ? capital : lower;
    int length = 0;

    do {
        digits[length++] = table[(unsigned int)(value % base)];
        value /= base;
    } while (value != 0UL);
    return length;
}

static void
p10stdio_number_v2(struct p10stdio_sink_v2 *out, unsigned long value,
                   int negative, int base, int upper,
                   int left, int zero, int plus, int space, int alternate,
                   int width, int precision)
{
    char digits[40];
    int len;
    int sign;
    int prefix;
    int zeros;
    int padding;
    int needed;
    int i;

    len = p10stdio_digits_v2(value, (unsigned int)base, upper, digits);
    if (precision == 0 && value == 0UL)
        len = 0;
    sign = negative ? '-' : plus ? '+' : space ? ' ' : 0;
    prefix = alternate && base == 16 && value != 0UL ? 2 : 0;
    if (alternate && base == 8 &&
        (len == 0 || digits[len - 1] != '0') &&
        precision <= len)
        precision = len + 1;
    zeros = precision > len ? precision - len : 0;
    /* A user-supplied precision may be INT_MAX.  Reject an output
       count beyond int's public return range without signed overflow. */
    if (zeros > INT_MAX - len - prefix - (sign != 0)) {
        out->failed = 1;
        return;
    }
    needed = len + zeros + prefix + (sign != 0);
    padding = width > needed ? width - needed : 0;

    if (!left && !(zero && precision < 0))
        p10stdio_repeat_v2(out, ' ', padding);
    if (sign != 0)
        p10stdio_emit_v2(out, sign);
    if (prefix != 0) {
        p10stdio_emit_v2(out, '0');
        p10stdio_emit_v2(out, upper ? 'X' : 'x');
    }
    if (!left && zero && precision < 0)
        p10stdio_repeat_v2(out, '0', padding);
    p10stdio_repeat_v2(out, '0', zeros);
    for (i = len - 1; i >= 0; --i)
        p10stdio_emit_v2(out, digits[i]);
    if (left)
        p10stdio_repeat_v2(out, ' ', padding);
}

static int
p10stdio_decimal_v2(const char **format, int *result)
{
    unsigned int value = 0;
    int saw_digit = 0;
    while (**format >= '0' && **format <= '9') {
        unsigned int digit = (unsigned int)(**format - '0');
        if (value > ((unsigned int)INT_MAX - digit) / 10U)
            return -1;
        value = value * 10U + digit;
        ++*format;
        saw_digit = 1;
    }
    if (saw_digit)
        *result = (int)value;
    return saw_digit;
}

static int
p10stdio_render_v2(struct p10stdio_sink_v2 *out,
                   const char *format, va_list args)
{
    int c;

    if (format == (const char *)0)
        return -1;
    while ((c = (unsigned char)*format++) != 0 && !out->failed) {
        int left;
        int zero;
        int plus;
        int space;
        int alternate;
        int width;
        int precision;
        int length;
        int n;
        int base;
        int upper;
        int negative;
        unsigned long value;
        long signed_value;
        const char *s;

        if (c != '%') {
            p10stdio_emit_v2(out, c);
            continue;
        }
        left = zero = plus = space = alternate = 0;
        for (;;) {
            c = (unsigned char)*format;
            if (c == '-')
                left = 1;
            else if (c == '+')
                plus = 1;
            else if (c == ' ')
                space = 1;
            else if (c == '0')
                zero = 1;
            else if (c == '#')
                alternate = 1;
            else
                break;
            ++format;
        }
        width = 0;
        if (*format == '*') {
            width = va_arg(args, int);
            ++format;
            if (width < 0) {
                if (width == INT_MIN)
                    return -1;
                width = -width;
                left = 1;
            }
        } else if (p10stdio_decimal_v2(&format, &width) < 0)
            return -1;
        precision = -1;
        if (*format == '.') {
            ++format;
            precision = 0;
            if (*format == '*') {
                precision = va_arg(args, int);
                ++format;
            } else if (p10stdio_decimal_v2(&format, &precision) < 0)
                return -1;
        }
        length = 0;
        if (*format == 'h') {
            length = -1;
            ++format;
        } else if (*format == 'l') {
            length = 1;
            ++format;
        }
        c = (unsigned char)*format++;
        if (c == 0)
            return -1;
        if (c == '%') {
            p10stdio_emit_v2(out, '%');
            continue;
        }
        if (c == 's') {
            if (length != 0)
                return -1;
            s = va_arg(args, const char *);
            if (s == (const char *)0)
                s = "(null)";
            n = 0;
            while (s[n] && (precision < 0 || n < precision) &&
                   n < INT_MAX)
                ++n;
            if (!left)
                p10stdio_repeat_v2(out, ' ', width > n ? width - n : 0);
            length = n;
            while (n-- > 0)
                p10stdio_emit_v2(out, (unsigned char)*s++);
            if (left)
                p10stdio_repeat_v2(out, ' ',
                                   width > length ? width - length : 0);
            continue;
        }
        if (c == 'c') {
            if (length != 0)
                return -1;
            n = va_arg(args, int);
            if (!left)
                p10stdio_repeat_v2(out, ' ', width > 1 ? width - 1 : 0);
            p10stdio_emit_v2(out, n);
            if (left)
                p10stdio_repeat_v2(out, ' ', width > 1 ? width - 1 : 0);
            continue;
        }
        if (c == 'p') {
            if (length != 0)
                return -1;
            value = (unsigned long)va_arg(args, void *);
            p10stdio_number_v2(out, value, 0, 16, 0, left, zero,
                               0, 0, 1, width, precision);
            continue;
        }
        if (c != 'd' && c != 'i' && c != 'u' && c != 'o' &&
            c != 'x' && c != 'X')
            return -1;
        base = c == 'o' ? 8 : (c == 'x' || c == 'X') ? 16 : 10;
        upper = c == 'X';
        negative = 0;
        if (c == 'd' || c == 'i') {
            signed_value = length > 0 ? va_arg(args, long) :
                           length < 0 ? (short)va_arg(args, int) :
                           va_arg(args, int);
            if (signed_value < 0) {
                negative = 1;
                value = (unsigned long)(-(signed_value + 1L)) + 1UL;
            } else
                value = (unsigned long)signed_value;
        } else {
            value = length > 0 ? va_arg(args, unsigned long) :
                    length < 0 ? (unsigned short)va_arg(args, int) :
                    va_arg(args, unsigned int);
            plus = space = 0;
        }
        p10stdio_number_v2(out, value, negative, base, upper, left,
                           zero, plus, space, alternate, width, precision);
    }
    return out->failed ? -1 : 0;
}

int
vsnprintf(char *destination, size_t size, const char *format, va_list args)
{
    struct p10stdio_sink_v2 out;
    int rc;

    if (size != 0 && destination == (char *)0)
        return -1;
    out.buffer = destination;
    out.capacity = size;
    out.count = 0;
    out.emit = (int (*)(void *, int))0;
    out.emit_context = (void *)0;
    out.failed = 0;
    rc = p10stdio_render_v2(&out, format, args);
    if (size != 0) {
        size_t end = out.count < size ? out.count : size - 1;
        destination[end] = 0;
    }
    if (rc != 0 || out.count > (size_t)INT_MAX)
        return -1;
    return (int)out.count;
}

/*
 * Backend extension: one character is written immediately per callback.
 * Only the OS-specific archive provides a callback.  A failed write
 * terminates formatting with -1; no output heap or global sink exists.
 */
int
__p10stdio_vformat_v2(const char *format, va_list args,
                      int (*emit)(void *, int), void *context)
{
    struct p10stdio_sink_v2 out;

    if (emit == (int (*)(void *, int))0)
        return -1;
    out.buffer = (char *)0;
    out.capacity = 0;
    out.count = 0;
    out.emit = emit;
    out.emit_context = context;
    out.failed = 0;
    if (p10stdio_render_v2(&out, format, args) != 0 ||
        out.count > (size_t)INT_MAX)
        return -1;
    return (int)out.count;
}

int
snprintf(char *destination, size_t size, const char *format, ...)
{
    va_list args;
    int result;
    va_start(args, format);
    result = vsnprintf(destination, size, format, args);
    va_end(args);
    return result;
}

int
vsprintf(char *destination, const char *format, va_list args)
{
    return vsnprintf(destination, (size_t)-1, format, args);
}

int
sprintf(char *destination, const char *format, ...)
{
    va_list args;
    int result;
    va_start(args, format);
    result = vsprintf(destination, format, args);
    va_end(args);
    return result;
}
