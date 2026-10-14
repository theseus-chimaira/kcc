	.text
	.globl	memcmp
; PDP-6 freestanding memcmp: unsigned 9-bit characters.
; AC1/AC2 byte pointers, AC3 length. Return -1, 0, or +1 in AC1.
; Word comparisons are legal only at equal 36-bit word boundaries and
; when at least four bytes remain. A mismatching word is resolved using
; byte pointers, preserving the ordering of its first differing byte.
; No stack use. All scratch registers are call-clobbered (AC0..AC7).
memcmp:
	jumpe	3,memcmp_v2_equal
	move	4,1
	move	5,2
memcmp_v2_check:
	caige	3,4
	jrst	memcmp_v2_byte
	hlrz	6,4
	cain	6,0331100
	jrst	memcmp_v2_second
	jrst	memcmp_v2_byte
memcmp_v2_second:
	hlrz	6,5
	cain	6,0331100
	jrst	memcmp_v2_word
	jrst	memcmp_v2_byte
memcmp_v2_word:
	hrrz	6,4
	move	6,(6)
	hrrz	7,5
	came	6,(7)
	jrst	memcmp_v2_byte
	hrrz	4,4
	addi	4,1
	hrli	4,0331100
	addi	7,1
	move	5,7
	hrli	5,0331100
	subi	3,4
	jumpn	3,memcmp_v2_check
	jrst	memcmp_v2_equal
memcmp_v2_byte:
	ldb	6,4
	ldb	7,5
	camn	6,7
	jrst	memcmp_v2_next
	caml	6,7
	jrst	memcmp_v2_greater
	seto	1,
	popj	17,
memcmp_v2_greater:
	movei	1,1
	popj	17,
memcmp_v2_next:
	ibp	4
	ibp	5
	sojn	3,memcmp_v2_check
memcmp_v2_equal:
	setz	1,
	popj	17,
