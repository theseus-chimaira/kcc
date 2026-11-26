/* Exact portable PDP-10 D-format decimal conversion.
 * Included only by the optional v3 formatter. No FP arithmetic or heap.
 * A normalized value is M * 2^(E-128-62), M having 62 bits.
 * At most 190 factors of five: 62*log10(2)+190*log10(5) < 152.
 * Seventeen base-10^9 words therefore cover the entire finite format.
 */
#define P10DF_BASE 1000000000UL
#define P10DF_LIMBS 17
#define P10DF_DIGITS (9 * P10DF_LIMBS)

static int
p10df_muladd_v3(unsigned long *a, int n, unsigned long factor,
                unsigned long carry)
{
    int i;
    unsigned long x, q;
    /* factor <= 64: every intermediate fits an unsigned 36-bit word. */
    for (i = 0; i < n; ++i) {
        x = a[i] * factor + carry;
        q = x / P10DF_BASE;
        a[i] = x - q * P10DF_BASE;
        carry = q;
    }
    if (carry != 0) {
        if (n == P10DF_LIMBS)
            return -1;
        a[n++] = carry;
    }
    return n;
}

/* Return digits in most-significant-first order, with decimal point
 * before digit 'point'. No insignificant leading zero is stored.
 * The limb array dies before the caller starts emitting characters.
 */
static int
p10df_digits_v3(double value, char *digits, int *point, int *negative)
{
    union { double d; unsigned long w[2]; } bits;
    unsigned long a[P10DF_LIMBS], hi, lo, x, q;
    int e, scale, n, i, j, count, shift;
    char c;

    bits.d = value;
    hi = bits.w[0];
    lo = bits.w[1] & 0377777777777UL;
    *negative = (hi & 0400000000000UL) != 0;
    if (*negative) {
        lo = (0UL - lo) & 0377777777777UL;
        hi = (~hi + (lo == 0)) & 0777777777777UL;
    }
    e = (int)((hi >> 27) & 255UL) - 128 - 62;
    hi &= 0777777777UL;
    if (hi == 0 && lo == 0) {
        digits[0] = '0';
        *point = 1;
        return 1;
    }
    a[0] = hi;
    n = p10df_muladd_v3(a, 1, 32UL, lo >> 30);
    for (shift = 24; shift >= 0; shift -= 6) {
        n = p10df_muladd_v3(a, n, 64UL, (lo >> shift) & 63UL);
        if (n < 0)
            return -1;
    }
    scale = e < 0 ? -e : 0;
    while (e < 0) {
        n = p10df_muladd_v3(a, n, e <= -2 ? 25UL : 5UL, 0UL);
        e += e <= -2 ? 2 : 1;
        if (n < 0)
            return -1;
    }
    while (e > 0) {
        shift = e > 6 ? 6 : e;
        n = p10df_muladd_v3(a, n, 1UL << shift, 0UL);
        e -= shift;
        if (n < 0)
            return -1;
    }
    count = 0;
    for (i = 0; i < n; ++i) {
        x = a[i];
        for (j = 0; j < 9; ++j) {
            q = x / 10UL;
            digits[count++] = (char)('0' + (x - q * 10UL));
            x = q;
            if (i == n - 1 && x == 0)
                break;
        }
    }
    for (i = 0, j = count - 1; i < j; ++i, --j) {
        c = digits[i];
        digits[i] = digits[j];
        digits[j] = c;
    }
    *point = count - scale;
    return count;
}

/* Round to 'keep' significant digits, ties to even. For fixed format
 * keep may be <= 0. Carry across every retained digit changes point.
 */
static int
p10df_round_v3(char *d, int n, int *point, int keep)
{
    int i, up;
    if (keep >= n)
        return n;
    up = 0;
    if (keep >= 0) {
        up = d[keep] > '5';
        if (d[keep] == '5') {
            for (i = keep + 1; i < n && d[i] == '0'; ++i)
                ;
            up = i < n || (keep > 0 && ((d[keep-1] - '0') & 1));
        }
    }
    if (keep <= 0) {
        d[0] = up ? '1' : '0';
        *point = up ? *point + 1 : 1;
        return 1;
    }
    if (up) {
        i = keep;
        while (i > 0 && d[i-1] == '9')
            d[--i] = '0';
        if (i == 0) {
            d[0] = '1';
            ++*point;
            return 1;
        }
        ++d[i-1];
    }
    return keep;
}

static int
p10stdio_float_v3(struct p10stdio_sink_v2 *out, double value,
                  int conv, int width, int precision, int left,
                  int zero, int plus, int space, int alternate)
{
    char d[P10DF_DIGITS];
    int n, point, neg, sign, p, scientific, frac, whole, dot;
    int exponent, suffix, total, pad, i, pos, k;

    n = p10df_digits_v3(value, d, &point, &neg);
    if (n < 0)
        return -1;
    p = precision < 0 ? 6 : precision;
    scientific = conv == 'e' || conv == 'E';
    if (conv == 'g' || conv == 'G') {
        if (p == 0)
            p = 1;
        n = p10df_round_v3(d, n, &point, p);
        scientific = point <= -4 || point > p;
        if (!alternate) {
            while (n > 1 && d[n-1] == '0')
                --n;
            frac = scientific ? n - 1 : (n > point ? n - point : 0);
        } else {
            if (!scientific && point < 0 && p > INT_MAX + point)
                return -1;
            frac = scientific ? p - 1 : (p > point ? p - point : 0);
        }
    } else {
        /* Only the finite exact digit string can contribute to rounding.
         * Avoid overflowing p+point for an enormous requested precision. */
        if (p < P10DF_DIGITS + 190)
            n = p10df_round_v3(d, n, &point, scientific ? p + 1 : point + p);
        frac = p;
    }
    sign = neg ? '-' : plus ? '+' : space ? ' ' : 0;
    exponent = point - 1;
    if (scientific)
        point = 1;
    whole = point > 0 ? point : 1;
    dot = frac > 0 || alternate;
    suffix = scientific ? 4 : 0; /* e +/- two digits; |exponent| < 100 */
    if (frac > INT_MAX - whole - dot - suffix - (sign != 0))
        return -1;
    total = whole + dot + frac + suffix + (sign != 0);
    pad = width > total ? width - total : 0;
    if (!left && !zero)
        p10stdio_repeat_v2(out, ' ', pad);
    if (sign)
        p10stdio_emit_v2(out, sign);
    if (!left && zero)
        p10stdio_repeat_v2(out, '0', pad);
    if (point <= 0)
        p10stdio_emit_v2(out, '0');
    else
        for (i = 0; i < point; ++i)
            p10stdio_emit_v2(out, i < n ? d[i] : '0');
    if (dot)
        p10stdio_emit_v2(out, '.');
    pos = point;
    if (pos < 0) {
        k = frac < -pos ? frac : -pos;
        p10stdio_repeat_v2(out, '0', k);
        frac -= k;
        pos += k;
    }
    while (frac > 0 && pos >= 0 && pos < n) {
        p10stdio_emit_v2(out, d[pos++]);
        --frac;
    }
    p10stdio_repeat_v2(out, '0', frac);
    if (scientific) {
        p10stdio_emit_v2(out, conv == 'E' || conv == 'G' ? 'E' : 'e');
        p10stdio_emit_v2(out, exponent < 0 ? '-' : '+');
        if (exponent < 0)
            exponent = -exponent;
        p10stdio_emit_v2(out, '0' + exponent / 10);
        p10stdio_emit_v2(out, '0' + exponent % 10);
    }
    if (left)
        p10stdio_repeat_v2(out, ' ', pad);
    return out->failed ? -1 : 0;
}
