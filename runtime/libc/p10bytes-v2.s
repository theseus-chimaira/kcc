	.text
; OS-independent PDP-10 packed-byte operations, version 2.
; These pointers are native PDP-10 byte pointers, NOT C char pointers.
; AC1 destination, AC2 source/fill, AC3 number of elements.
; 6-, 9-, 18- and 36-bit *word-aligned* copies use MOVE/MOVEM.
; Other widths and tails use LDB/DPB. No stack or OS calls.
	.globl	p10bcopy_v2
p10bcopy_v2:
	move	6,1
	move	7,2
	jumpe	3,p10bcopy_v2_done
; Fast copying needs matching P/S fields and a complete word boundary.
; The count threshold and copy loop are shared by all supported widths.
	move	4,6
	xor	4,7
	tlne	4,0777700
	jrst	p10bcopy_v2_byte
	hlrz	4,6
	movei	5,6
	cain	4,0360600
	jrst	p10bcopy_v2_word
	movei	5,4
	cain	4,0331100
	jrst	p10bcopy_v2_word
	movei	5,2
	cain	4,0222200
	jrst	p10bcopy_v2_word
	movei	5,1
	cain	4,0004400
	jrst	p10bcopy_v2_word
	jrst	p10bcopy_v2_byte
p10bcopy_v2_word:
	camge	3,5
	jrst	p10bcopy_v2_byte
	; BLT setup is amortized once at least four complete words remain.
	move	2,5
	imuli	2,4
	camge	3,2
	jrst	p10bcopy_v2_one_word
	; IDIV gives complete words in AC3 and trailing bytes in AC4.
	idiv	3,5
	; BLT AC5 = source address,,destination address.
	hrrz	2,6
	add	2,3
	subi	2,1
	hrrz	5,6
	hrl	5,7
	blt	5,(2)
	add	6,3
	add	7,3
	move	3,4
	jumpn	3,p10bcopy_v2_byte
	popj	17,
p10bcopy_v2_one_word:
	hrrz	4,7
	move	4,(4)
	hrrz	2,6
	movem	4,(2)
	addi	6,1
	addi	7,1
	sub	3,5
	jumpn	3,p10bcopy_v2_word
	popj	17,
p10bcopy_v2_byte:
	ldb	4,7
	dpb	4,6
	ibp	7
	ibp	6
	sojn	3,p10bcopy_v2_byte
p10bcopy_v2_done:
	popj	17,

; Fill native packed bytes. Whole-word stores require a canonical word-start
; pointer; byte tails and widths that do not divide 36 use DPB/IBP.
; AC1=destination byte pointer, AC2=value, AC3=number of elements.
	.globl	p10bfill_v2
p10bfill_v2:
	move	6,1
	jumpe	3,p10bfill_v2_done
	hlrz	7,6
	cain	7,0360600
	jrst	p10bfill_v2_six_setup
	cain	7,0331100
	jrst	p10bfill_v2_nine_setup
	cain	7,0222200
	jrst	p10bfill_v2_eighteen_setup
	cain	7,0004400
	jrst	p10bfill_v2_thirtysix
	jrst	p10bfill_v2_bytes
p10bfill_v2_six_setup:
	andi	2,77
	move	4,2
	move	5,4
	lsh	5,6
	ior	4,5
	move	5,4
	lsh	5,14
	ior	4,5
	move	5,4
	lsh	5,14
	ior	4,5
	movei	7,6
	jrst	p10bfill_v2_word
p10bfill_v2_nine_setup:
	andi	2,777
	move	4,2
	move	5,4
	lsh	5,11
	ior	4,5
	move	5,4
	lsh	5,22
	ior	4,5
	movei	7,4
	jrst	p10bfill_v2_word
p10bfill_v2_eighteen_setup:
	andi	2,777777
	move	4,2
	move	5,4
	lsh	5,22
	ior	4,5
	movei	7,2
	jrst	p10bfill_v2_word
p10bfill_v2_thirtysix:
	move	4,2
	movei	7,1
p10bfill_v2_word:
	camge	3,7
	jrst	p10bfill_v2_bytes
	; Four or more whole words: seed the first destination word and
	; forward-BLT its contents through the remainder. Preserve the
	; original byte value (AC2) via the replicated word in AC0.
	move	5,7
	imuli	5,4
	camge	3,5
	jrst	p10bfill_v2_one_word
	move	0,4
	idiv	3,7
	move	5,4
	hrrz	4,6
	movem	0,(4)
	move	2,4
	add	2,3
	subi	2,1
	hrl	4,6
	addi	4,1
	blt	4,(2)
	add	6,3
	move	2,0
	move	3,5
	jumpn	3,p10bfill_v2_bytes
	popj	17,
p10bfill_v2_one_word:
	hrrz	5,6
	movem	4,(5)
	addi	6,1
	sub	3,7
	jumpn	3,p10bfill_v2_word
	popj	17,
p10bfill_v2_bytes:
	dpb	2,6
	ibp	6
	sojn	3,p10bfill_v2_bytes
p10bfill_v2_done:
	popj	17,

; Compare unsigned native bytes and return -1,0,+1. Equal canonical
; 6/9/18/36-bit pointers are scanned one complete word at a time.
; Mismatching words and partial tails are resolved by byte pointers.
; The sign-bit transformation orders even full 36-bit bytes UNSIGNED.
	.globl	p10bcmp_v2
p10bcmp_v2:
	jumpe	3,p10bcmp_v2_equal
	move	4,1
	move	5,2
	move	6,4
	xor	6,5
	tlne	6,0777700
	jrst	p10bcmp_v2_byte
	hlrz	6,4
	movei	7,6
	cain	6,0360600
	jrst	p10bcmp_v2_word
	movei	7,4
	cain	6,0331100
	jrst	p10bcmp_v2_word
	movei	7,2
	cain	6,0222200
	jrst	p10bcmp_v2_word
	movei	7,1
	cain	6,0004400
	jrst	p10bcmp_v2_word
	jrst	p10bcmp_v2_byte
p10bcmp_v2_word:
	camge	3,7
	jrst	p10bcmp_v2_byte
	hrrz	6,4
	move	6,(6)
	hrrz	2,5
	camn	6,(2)
	jrst	p10bcmp_v2_word_same
	jrst	p10bcmp_v2_byte
p10bcmp_v2_word_same:
	addi	4,1
	addi	5,1
	sub	3,7
	jumpn	3,p10bcmp_v2_word
	jrst	p10bcmp_v2_equal
p10bcmp_v2_byte:
	ldb	6,4
	ldb	7,5
	camn	6,7
	jrst	p10bcmp_v2_next
; PDP-10 signed CAML cannot directly compare 36-bit unsigned values.
; Flipping each operand's top bit maps unsigned order to signed order.
	tlc	6,400000
	tlc	7,400000
	caml	6,7
	jrst	p10bcmp_v2_greater
	seto	1,
	popj	17,
p10bcmp_v2_greater:
	movei	1,1
	popj	17,
p10bcmp_v2_next:
	ibp	4
	ibp	5
	sojn	3,p10bcmp_v2_byte
p10bcmp_v2_equal:
	setz	1,
	popj	17,
