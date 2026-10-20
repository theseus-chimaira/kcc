; PDP-6-safe signed 71-bit integer <-> PDP-10 software D-format.
; NOT PDP-6 native floating hardware.  All operations are integer only.
; This supports the established two-word C double ABI on the portable
; DAIMOS baseline; native PDP-6 FP must remain a distinct target mode.
;
; __kcc_floatdidf: AC1 high36, AC2 low35 with sign duplicated in bit35.
; __kcc_fixdfdi:   AC1/AC2 are a software D-format double.
; Results returned in AC1/AC2; other ACs may be clobbered, no stack.
; D-format: sign/exponent/fraction27, signcopy/fraction35.
; The exponent is excess-128 and fraction is 62-bit two's-complement.
; Integer->double uses round-to-nearest, ties-to-even.
; Double->integer truncates toward zero, for representable values.
	.text
	.globl	__kcc_floatdidf
	.globl	__kcc_fixdfdi

__kcc_floatdidf:
	move	3,1
	ior	3,2
	jumpe	3,kdf_v1_zero
	setz	10,
	tlne	1,0400000
	jrst	kdf_v1_abs
	jrst	kdf_v1_unsigned
kdf_v1_abs:
	movei	10,1
	setca	1,
	setca	2,
	and	2,[0377777777777]
	addi	2,1
	tlne	2,0400000
	addi	1,1
	and	2,[0377777777777]
kdf_v1_unsigned:
	; Make a 72-bit register pair from a 71-bit high36:low35.
	lsh	2,1
	movei	3,0107		; 71 bits in the signed integer.
kdf_v1_normalize:
	tlne	1,0400000
	jrst	kdf_v1_mantissa
	lshc	1,1
	soja	3,kdf_v1_normalize
kdf_v1_mantissa:
	; Fraction bits 71..10 of the normalized 72-bit pair.
	move	4,1
	lsh	4,-011
	move	5,1
	andi	5,0777
	lsh	5,032
	move	6,2
	lsh	6,-012
	ior	5,6
	; Guard bit is bit9; sticky are bits8..0.
	move	6,2
	andi	6,01777
	subi	6,01000
	jumpl	6,kdf_v1_pack
	jumpe	6,kdf_v1_tie
	jrst	kdf_v1_round
kdf_v1_tie:
	trne	5,1
	jrst	kdf_v1_round
	jrst	kdf_v1_pack
kdf_v1_round:
	addi	5,1
	tlne	5,0400000
	jrst	kdf_v1_carry
	jrst	kdf_v1_pack
kdf_v1_carry:
	and	5,[0377777777777]
	addi	4,1
	move	6,4
	lsh	6,-033
	jumpe	6,kdf_v1_pack
	jrst	kdf_v1_overflow
kdf_v1_overflow:
	move	4,[0400000000]
	aos	3
kdf_v1_pack:
	addi	3,0200
	lsh	3,033
	ior	4,3
	move	1,4
	move	2,5
	jumpe	10,kdf_v1_return
	; Form negative 71-bit two's-complement D-format.
	setca	1,
	setca	2,
	and	2,[0377777777777]
	addi	2,1
	tlne	2,0400000
	addi	1,1
	and	2,[0377777777777]
	tlo	2,0400000
kdf_v1_return:
	popj	17,
kdf_v1_zero:
	setz	1,
	setz	2,
	popj	17,

__kcc_fixdfdi:
	move	3,1
	ior	3,2
	jumpe	3,kdf_v1_zero
	setz	10,
	tlne	1,0400000
	jrst	kdf_v1_fix_abs
	jrst	kdf_v1_fix_positive
kdf_v1_fix_abs:
	movei	10,1
	setca	1,
	setca	2,
	and	2,[0377777777777]
	addi	2,1
	tlne	2,0400000
	addi	1,1
	and	2,[0377777777777]
kdf_v1_fix_positive:
	move	3,1
	lsh	3,-033
	andi	3,0377
	subi	3,0200
	jumple	3,kdf_v1_zero
	; Reconstruct 72-bit fixed point: frac62 left shifted by ten.
	move	4,1
	and	4,[0777777777]
	lsh	4,011
	move	5,2
	lsh	5,-032
	ior	4,5
	move	5,2
	lsh	5,012
	move	1,4
	move	2,5
	; Fraction is normalized to 2^71. Shift right (71-exponent)
	; to obtain the unsigned magnitude in a high36:low35 pair.
	move	6,3
	subi	6,0107
	lshc	1,0(6)
	lsh	2,-1
	jumpe	10,kdf_v1_fix_positive_return
	; Negate 71-bit magnitude; duplicate sign in low word for the ABI.
	setca	1,
	setca	2,
	and	2,[0377777777777]
	addi	2,1
	tlne	2,0400000
	addi	1,1
	and	2,[0377777777777]
	tlo	2,0400000
	popj	17,
kdf_v1_fix_positive_return:
	and	2,[0377777777777]
	popj	17,
