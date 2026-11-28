; Portable canonical D-format arithmetic, common entry/exit.
; Existing KCC private ABI: AC10:11 left/result, AC13:14 right.
; AC10..14 are saved by the emitted caller, AC1..7 are saved here.
; AC15, AC16 and writable globals are untouched.
;
; On entry to a body, AC1:2 and AC3:4 hold unsigned normalized
; 62-bit fractions (high26:low36), AC5/6 biased exponents, and
; AC7 bit0/bit1 the two signs. AC12 carries the body address.
.text
.globl __kcc_dfenter_v62a
.globl __kcc_dfround_v62a
.globl __kcc_dfpack_v62a
.globl __kcc_dfzero_v62a

__kcc_dfenter_v62a:
    push 17,1
    push 17,2
    push 17,3
    push 17,4
    push 17,5
    push 17,6
    push 17,7
    move 1,10
    move 2,11
    move 3,13
    move 4,14
    pushj 17,df62_unpack
    move 10,1
    move 11,2
    move 13,5
    move 7,6
    move 1,3
    move 2,4
    pushj 17,df62_unpack
    move 3,1
    move 4,2
    move 1,10
    move 2,11
    lsh 6,1
    ior 7,6
    move 6,5
    move 5,13
    jrst 0(12)

; Decode a canonical pair. Preserve AC3/4, AC7 and AC12.
df62_unpack:
    setz 6,
    tlne 1,0400000
     movei 6,1
    jumpe 6,df62_abs
    movns 1
    movns 2
    tlz 2,0400000
    skipe 2
     subi 1,1
df62_abs:
    move 5,1
    lsh 5,-033
    andi 5,0377
    and 1,[0777777777]
    tlz 2,0400000
    lsh 2,1
    lshc 1,-1
    popj 17,

; Three discarded fraction bits in AC2: guard, next, sticky.
; AC5 biased exponent; AC7 sign (zero or one).
__kcc_dfround_v62a:
    move 12,2
    andi 12,7
    lshc 1,-3
    caige 12,4
     jrst __kcc_dfpack_v62a
    caie 12,4
     jrst df62_increment
    trnn 2,1
     jrst __kcc_dfpack_v62a
df62_increment:
    aosn 2
     addi 1,1

; Pack a rounded low-aligned 62-bit fraction. Handle rounding carry.
__kcc_dfpack_v62a:
    move 10,1
    ior 10,2
    jumpe 10,__kcc_dfzero_v62a
    tlne 1,0400
     jrst df62_carry
    jrst df62_pack
df62_carry:
    lshc 1,-1
    addi 5,1
df62_pack:
    lshc 1,1
    lsh 2,-1
    andi 5,0377
    lsh 5,033
    ior 1,5
    jumpe 7,df62_return
    movns 1
    movns 2
    tlz 2,0400000
    skipe 2
     subi 1,1
    tlo 2,0400000
    jrst df62_return
__kcc_dfzero_v62a:
    setz 1,
    setz 2,
df62_return:
    move 10,1
    move 11,2
    pop 17,7
    pop 17,6
    pop 17,5
    pop 17,4
    pop 17,3
    pop 17,2
    pop 17,1
    popj 17,
