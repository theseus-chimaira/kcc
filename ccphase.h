#ifndef KCC_CCPHASE_H
#define KCC_CCPHASE_H

/*
 * Compact KCC preprocessor phase stream.
 *
 * The stream is byte-oriented.  Native KCC chars are 9 bits, so every
 * format byte (0..255) fits without translation while token spellings
 * remain ordinary source characters.
 *
 * Header: 'K' 'P' 'T' '4'
 *
 * Token record:
 *   token byte, 16-bit big-endian payload length, payload bytes.
 *
 * Location record:
 *   byte 255, 18-bit file line, 18-bit total line, 18-bit page,
 *   18-bit page line, 16-bit filename length, filename bytes.
 *
 * T_EOF is a normal zero-payload token and terminates the stream.
 */
#define KCC_PHASE_MAGIC_0 'K'
#define KCC_PHASE_MAGIC_1 'P'
#define KCC_PHASE_MAGIC_2 'T'
#define KCC_PHASE_MAGIC_3 '4'
#define KCC_PHASE_LOCATION 255
#define KCC_PHASE_MAX_PAYLOAD 65535U

#endif
