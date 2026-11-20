	.text
	.globl	__kcc_floatunssisf
	.globl	__kcc_floatunsidf
	.globl	__kcc_fixunssfsi
	.globl	__kcc_fixunsdfsi
	.globl	__kcc_floatsisf
	.globl	__kcc_fixsfsi
;
; PDP-6/KA10 real 36-bit integer / floating conversion helpers.
; Version 1.  Inputs in AC1; results in AC1 (SF) or AC1:AC2 (DF).
; C source conversions use the real compiler ABI, not a test stub.
;
; The single conversion rounds a full unsigned 36-bit input to the
; canonical 27-bit PDP-10 floating mantissa, nearest/ties-to-even.
; The double conversion carries all 36 bits exactly in D-format's
; 27+35 fraction.  Both are pure arithmetic with bounded stack use.
;
; AC3/4/6/7 are saved to preserve caller registers in both paths;
; AC5 is also saved for double.  AC1:AC2 are the result registers.
;
__kcc_floatunssisf:
	jumpn	1,kufsf_v1_nonzero
	popj	17,
kufsf_v1_nonzero:
	push	17,3
	push	17,4
	push	17,6
	push	17,7
	move	6,1
	movei	7,043
kufsf_v1_scan:
	tlne	6,0400000
	 jrst	kufsf_v1_found
	lsh	6,1
	soja	7,kufsf_v1_scan
kufsf_v1_found:
	movei	6,032
	sub	6,7
	jumpge	6,kufsf_v1_shift
; Compute the discarded low bits and half-ULP without overflowing
; the original 36-bit unsigned value.
	move	3,1
	movn	4,6
	movei	6,1
	lsh	6,0(4)
	move	4,6
	subi	4,1
	and	3,4
	movei	6,032
	sub	6,7
	lsh	1,0(6)
	move	4,7
	subi	4,033
	movei	6,1
	lsh	6,0(4)
	sub	3,6
	jumpl	3,kufsf_v1_exponent
	jumpg	3,kufsf_v1_round
	move	3,1
	andi	3,1
	jumpe	3,kufsf_v1_exponent
kufsf_v1_round:
	aoj	1,
	came	1,[01000000000]
	 jrst	kufsf_v1_exponent
	lsh	1,-1
	aoj	7,
	jrst	kufsf_v1_exponent
kufsf_v1_shift:
	lsh	1,0(6)
kufsf_v1_exponent:
	movei	6,0201
	add	6,7
	lsh	6,033
	ior	1,6
	pop	17,7
	pop	17,6
	pop	17,4
	pop	17,3
	popj	17,

; SF -> unsigned 36-bit integer, truncate toward zero.
; Positive SF has 27 fraction bits and exponent biased by 0201.
; A signed FIX result would overflow above 2^35-1; direct logical
; alignment retains the full unsigned range.  AC6 is preserved.
__kcc_fixunssfsi:
	jumpe	1,kufixsf_v1_done
	jumpl	1,kufixsf_v1_zero
	push	17,6
	move	6,1
	lsh	6,-033
	andi	6,0377
	subi	6,0201
	jumpl	6,kufixsf_v1_small
	and	1,[0777777777]
	subi	6,032
	lsh	1,0(6)
	pop	17,6
kufixsf_v1_done:
	popj	17,
kufixsf_v1_small:
	pop	17,6
kufixsf_v1_zero:
	setz	1,
	popj	17,

; Positive canonical D-format to unsigned 36-bit integer.
; Full 27+35 fraction prevents loss of the low integer bits at 2^35.
; In-range positive inputs, including 2^36-1, are exact in D-format.
__kcc_fixunsdfsi:
	jumpe	1,kufixdf_v1_zero
	jumpl	1,kufixdf_v1_zero
	push	17,3
	push	17,4
	push	17,6
	push	17,7
	move	6,1
	lsh	6,-033
	andi	6,0377
	subi	6,0201
	jumpl	6,kufixdf_v1_small
	caig	6,032
	 jrst	kufixdf_v1_highonly
	move	7,6
	subi	7,032
	and	1,[0777777777]
	lsh	1,0(7)
	movei	4,043
	sub	4,7
	movn	4,4
	move	3,2
	lsh	3,0(4)
	ior	1,3
	jrst	kufixdf_v1_done
kufixdf_v1_highonly:
	and	1,[0777777777]
	movei	7,032
	sub	7,6
	movn	7,7
	lsh	1,0(7)
kufixdf_v1_done:
	pop	17,7
	pop	17,6
	pop	17,4
	pop	17,3
	popj	17,
kufixdf_v1_small:
	pop	17,7
	pop	17,6
	pop	17,4
	pop	17,3
kufixdf_v1_zero:
	setz	1,
	popj	17,

; Signed SI -> SF, rounding the absolute 36-bit magnitude through the
; same exact unsigned helper.  Negation of -2^35 leaves its original
; bit pattern, which represents +2^35 to the unsigned helper.
__kcc_floatsisf:
	push	17,5
	move	5,1
	jumpge	5,ksf_v1_positive
	movns	1
ksf_v1_positive:
	pushj	17,__kcc_floatunssisf
	jumpge	5,ksf_v1_done
	movns	1
ksf_v1_done:
	pop	17,5
	popj	17,

; Canonical SF -> signed 36-bit integer.  Convert the magnitude using
; the real unsigned helper, then apply the original floating sign.
; In particular, -2^35 remains the exact signed minimum after MOVNS.
__kcc_fixsfsi:
	push	17,5
	move	5,1
	jumpge	5,ksfix_v1_positive
	movns	1
ksfix_v1_positive:
	pushj	17,__kcc_fixunssfsi
	jumpge	5,ksfix_v1_done
	movns	1
ksfix_v1_done:
	pop	17,5
	popj	17,

__kcc_floatunsidf:
	jumpn	1,kufdf_v1_nonzero
	setz	2,
	popj	17,
kufdf_v1_nonzero:
	push	17,3
	push	17,4
	push	17,5
	push	17,6
	push	17,7
	move	5,1
	movei	6,043
kufdf_v1_scan:
	tlne	5,0400000
	 jrst	kufdf_v1_found
	lsh	5,1
	soja	6,kufdf_v1_scan
kufdf_v1_found:
	caig	6,032
	 jrst	kufdf_v1_small
; Split the unsigned value into the 27-bit top fraction and
; 35-bit low continuation, without losing any integer bits.
	move	7,6
	subi	7,032
	move	3,1
	movn	4,7
	lsh	1,0(4)
	movei	4,043
	sub	4,7
	lsh	3,0(4)
	tlz	3,0400000
	move	2,3
	jrst	kufdf_v1_exponent
kufdf_v1_small:
	movei	4,032
	sub	4,6
	lsh	1,0(4)
	setz	2,
kufdf_v1_exponent:
	movei	4,0201
	add	4,6
	lsh	4,033
	ior	1,4
	pop	17,7
	pop	17,6
	pop	17,5
	pop	17,4
	pop	17,3
	popj	17,
