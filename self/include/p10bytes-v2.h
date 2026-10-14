#ifndef KCC_P10BYTES_V2_H
#define KCC_P10BYTES_V2_H

/* OS-independent PDP-10 packed byte-pointer operations.
 *
 * p10_byte_pointer_v2 is the RAW, one-word PDP-10 pointer encoding
 * (P, S, I, X, Y). It is not an ordinary C char * and conversion to or
 * from typed pointers is implementation-specific. Callers must provide
 * valid pointer sizes, correct bounds, and nonoverlapping copy ranges.
 *
 * The C ABI passes the three operands in AC1, AC2, AC3. The copy and
 * fill operations return their original destination descriptor in AC1;
 * compare returns -1/0/+1 (unsigned byte-value ordering).
 */
typedef unsigned long p10_byte_pointer_v2;

p10_byte_pointer_v2 p10bcopy_v2(p10_byte_pointer_v2 destination,
                               p10_byte_pointer_v2 source,
                               unsigned int elements);
p10_byte_pointer_v2 p10bfill_v2(p10_byte_pointer_v2 destination,
                               unsigned long value,
                               unsigned int elements);
int p10bcmp_v2(p10_byte_pointer_v2 first,
               p10_byte_pointer_v2 second,
               unsigned int elements);

#endif
