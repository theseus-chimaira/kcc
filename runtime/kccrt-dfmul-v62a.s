; Canonical D-format multiplication. Four native positive MULs
; form the exact 124-bit product in base-2^35 limbs.
.text
.globl __kcc_dfmul_rt_v62a
.extern __kcc_dfenter_v62a
.extern __kcc_dfround_v62a
.extern __kcc_dfzero_v62a
__kcc_dfmul_rt_v62a:
    movei 12,df62_mul
    jrst __kcc_dfenter_v62a
df62_mul:
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
    add 5,6
    subi 5,0200
    ; Convert high26:low36 to high27:low35.
    lshc 1,1
    lsh 2,-1
    lshc 3,1
    lsh 4,-1
    ; Low*low -> p1:p0.
    move 10,2
    mul 10,4
    ; High*low -> partial p2:p1, then propagate carry.
    move 13,1
    mul 13,4
    add 10,14
    tlne 10,0400000
     addi 13,1
    tlz 10,0400000
    ; Low*high -> add to p2:p1; preserve high right in AC6.
    move 6,3
    mul 2,3
    add 10,3
    add 13,2
    tlne 10,0400000
     addi 13,1
    tlz 10,0400000
    ; High*high -> p3:p2. Product limbs: 1,13,10,11.
    mul 1,6
    add 13,2
    tlne 13,0400000
     addi 1,1
    tlz 13,0400000
    ; Extract the normalized high62 and its exact guard/sticky.
    ; AC6 is the right shift of the p1/p2 limbs (26 or 27).
    movei 6,033
    tlne 1,1
     jrst df62_mul_extract
    subi 6,1
    subi 5,1
df62_mul_extract:
    movei 4,043
    sub 4,6
    movn 3,6
    move 2,13
    lsh 2,0(3)
    lsh 1,0(4)
    ior 1,2
    move 2,13
    lsh 2,0(4)
    tlz 2,0400000
    move 3,10
    movn 6,6
    lsh 3,0(6)
    ior 2,3
    ; Shift discarded portion of p1 into the low-word sign bit.
    addi 4,1
    move 3,10
    lsh 3,0(4)
    move 4,3
    lsh 4,-043
    lsh 4,2
    tlz 3,0400000
    ior 3,11
    skipn 3
     jrst df62_mul_round
    tro 4,1
df62_mul_round:
    lsh 2,1
    lshc 1,2
    ior 2,4
    jrst __kcc_dfround_v62a
