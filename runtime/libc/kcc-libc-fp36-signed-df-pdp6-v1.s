; PDP-6/KA10 signed 36-bit integer <-> D-format double, version 1.
; Real KCC compiler runtime entry, no heap, static memory, or OS calls.
;
; D-format is a sign/exponent/fraction27 high word and a 35-bit
; continuation low word, with bit35 reserved for the copied sign.
; Negative values are encoded by 71-bit two's complement across the
; 36+35 fraction pair, *not* by negating each word independently.
; Convert a negative pair back into the positive representation with
; the carry from the low 35 bits, convert its unsigned magnitude with
; the existing exact full-range compiler helper, then restore sign.
;
; The reverse conversion uses an *exact* 36-bit unsigned magnitude
; to D-format, not single precision plus a zero continuation word.
; The sign is applied to the full 36+35-bit D-format pair.  Both
; routines preserve AC5 across calls and rely on the existing exact
; unsigned ABI helpers to preserve their own scratch accumulators.
	.text
	.globl	__kcc_fixdfsi
	.globl	__kcc_floatsidf
	.extern	__kcc_fixunsdfsi
	.extern	__kcc_floatunsidf

; Signed 36-bit integer in AC1 to two-word D-format in AC1:AC2.
; For -2^35, MOVNS keeps the unsigned bit pattern of +2^35 and
; the unsigned converter then produces the exact positive fraction.
__kcc_floatsidf:
	push	17,5
	move	5,1
	jumpge	5,kfloatsidf_v1_magnitude
	movns	1
kfloatsidf_v1_magnitude:
	pushj	17,__kcc_floatunsidf
	jumpge	5,kfloatsidf_v1_done
	setca	1,
	setca	2,
	and	2,[0377777777777]
	addi	2,1
	tlne	2,0400000
	 addi	1,1
	and	2,[0377777777777]
	tlo	2,0400000
kfloatsidf_v1_done:
	pop	17,5
	popj	17,

__kcc_fixdfsi:
	push	17,5
	move	5,1
	jumpge	5,kfixdfsi_v1_absdone
	setca	1,
	setca	2,
	and	2,[0377777777777]
	addi	2,1
	tlne	2,0400000
	 addi	1,1
	and	2,[0377777777777]
kfixdfsi_v1_absdone:
	pushj	17,__kcc_fixunsdfsi
	jumpge	5,kfixdfsi_v1_done
	movns	1
kfixdfsi_v1_done:
	pop	17,5
	popj	17,
