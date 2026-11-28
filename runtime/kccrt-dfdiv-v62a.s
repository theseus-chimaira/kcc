; Canonical D-format restoring division. 62 quotient iterations;
; exact final remainder supplies the guard and sticky bits.
.text
.globl __kcc_dfdiv_rt_v62a
.extern __kcc_dfenter_v62a
.extern __kcc_dfround_v62a
.extern __kcc_dfzero_v62a
__kcc_dfdiv_rt_v62a:
    movei 12,df62_div
    jrst __kcc_dfenter_v62a
df62_div:
    move 10,1
    ior 10,2
    jumpe 10,__kcc_dfzero_v62a
    move 10,3
    ior 10,4
    jumpe 10,__kcc_dfzero_v62a
    move 12,7
    lsh 12,-1
    xor 7,12
    andi 7,1
    sub 5,6
    addi 5,0201
    ; AC6 is the sign-biased unsigned low divisor.
    move 6,4
    tlc 6,0400000
    camge 1,3
     jrst df62_div_normalize
    came 1,3
     jrst df62_div_begin
    move 13,2
    tlc 13,0400000
    caml 13,6
     jrst df62_div_begin
df62_div_normalize:
    lshc 1,1
    subi 5,1
df62_div_begin:
    setz 10,
    setz 11,
    movei 12,076
df62_div_loop:
    lshc 10,1
    camge 1,3
     jrst df62_div_next
    came 1,3
     jrst df62_div_sub
    move 13,2
    tlc 13,0400000
    camge 13,6
     jrst df62_div_next
df62_div_sub:
    move 13,2
    sub 2,4
    sub 1,3
    tlc 13,0400000
    camge 13,6
     subi 1,1
    tro 11,1
df62_div_next:
    lshc 1,1
    sojg 12,df62_div_loop
    ; Doubled remainder vs divisor: <half, half, >half.
    setz 12,
    camge 1,3
     jrst df62_div_round
    came 1,3
     jrst df62_div_above
    move 13,2
    tlc 13,0400000
    camge 13,6
     jrst df62_div_round
    came 13,6
     jrst df62_div_above
    movei 12,4
    jrst df62_div_round
df62_div_above:
    movei 12,5
df62_div_round:
    move 1,10
    move 2,11
    lshc 1,3
    ior 2,12
    jrst __kcc_dfround_v62a
