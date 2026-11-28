; Canonical D-format addition/subtraction. No writable data.
.text
.globl __kcc_dfadd_rt_v62a
.globl __kcc_dfsub_rt_v62a
.extern __kcc_dfenter_v62a
.extern __kcc_dfpack_v62a
.extern __kcc_dfround_v62a
.extern __kcc_dfzero_v62a
__kcc_dfsub_rt_v62a:
    ; Subtract by exact full-pair negation of the right operand.
    movns 13
    movns 14
    tlz 14,0400000
    skipe 14
     subi 13,1
    tlne 13,0400000
     tlo 14,0400000
__kcc_dfadd_rt_v62a:
    movei 12,df62_add
    jrst __kcc_dfenter_v62a
df62_add:
    move 10,1
    ior 10,2
    jumpe 10,df62_add_right
    move 10,3
    ior 10,4
    jumpe 10,df62_add_left
    ; AC12 records opposite signs. Choose the larger magnitude.
    move 12,7
    lsh 12,-1
    xor 12,7
    andi 12,1
    camge 5,6
     jrst df62_swap
    came 5,6
     jrst df62_align
    camge 1,3
     jrst df62_swap
    came 1,3
     jrst df62_align
    move 10,2
    move 11,4
    tlc 10,0400000
    tlc 11,0400000
    caml 10,11
     jrst df62_align
df62_swap:
    exch 1,3
    exch 2,4
    exch 5,6
    lsh 7,-1
df62_align:
    andi 7,1
    lshc 1,3
    lshc 3,3
    movn 6,6
    add 6,5
    jumpe 6,df62_combine
    ; Align smaller fraction with sticky. For shifts >=65 only
    ; the sticky bit remains. Otherwise the discarded bits are
    ; exactly those left by a complementary shift.
    caige 6,0101
     jrst df62_align_small
    setz 3,
    movei 4,1
    jrst df62_combine
df62_align_small:
    move 10,3
    move 11,4
    movn 13,6
    lshc 3,0(13)
    movei 13,0110
    sub 13,6
    lshc 10,0(13)
    ior 10,11
    skipn 10
     jrst df62_combine
    tro 4,1
df62_combine:
    ; Unsigned low-word carry/borrow via sign-biased comparisons.
    move 10,2
    tlc 10,0400000
    jumpn 12,df62_sub
    add 2,4
    add 1,3
    move 11,2
    tlc 11,0400000
    camge 11,10
     addi 1,1
    tlne 1,04000
     jrst df62_sum_carry
    jrst __kcc_dfround_v62a
df62_sum_carry:
    move 10,2
    lshc 1,-1
    trne 10,1
     tro 2,1
    addi 5,1
    jrst __kcc_dfround_v62a
df62_sub:
    sub 2,4
    sub 1,3
    tlc 4,0400000
    camge 10,4
     subi 1,1
    move 10,1
    ior 10,2
    jumpe 10,__kcc_dfzero_v62a
df62_sub_normalize:
    tlne 1,02000
     jrst __kcc_dfround_v62a
    lshc 1,1
    soja 5,df62_sub_normalize
df62_add_right:
    move 1,3
    move 2,4
    move 5,6
    lsh 7,-1
df62_add_left:
    andi 7,1
    jrst __kcc_dfpack_v62a
