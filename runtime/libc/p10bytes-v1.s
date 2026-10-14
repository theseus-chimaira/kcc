	.text
; OS-independent PDP-10 byte-pointer primitives. These operate on the
; size encoded in the PDP-10 pointer, not the C 'char' type. AC1/AC2
; are byte pointers, AC3 is an element count. AC0..AC7 call-clobbered.
; They require matching pointer sizes for copy/compare. The caller owns
; object bounds and nonoverlap, just as for memcpy.
	.globl	p10bcopy_v1
p10bcopy_v1:
	move	6,1
	jumpe	3,p10bcopy_done
p10bcopy_loop:
	ldb	4,2
	dpb	4,6
	ibp	2
	ibp	6
	sojn	3,p10bcopy_loop
p10bcopy_done:
	popj	17,

; AC1 destination byte pointer, AC2 fill value, AC3 element count.
	.globl	p10bfill_v1
p10bfill_v1:
	move	6,1
	jumpe	3,p10bfill_done
p10bfill_loop:
	dpb	2,6
	ibp	6
	sojn	3,p10bfill_loop
p10bfill_done:
	popj	17,

; Return -1,0,+1 from unsigned byte values; no C char-width assumptions.
	.globl	p10bcmp_v1
p10bcmp_v1:
	jumpe	3,p10bcmp_equal
	move	4,1
	move	5,2
p10bcmp_loop:
	ldb	6,4
	ldb	7,5
	camn	6,7
	jrst	p10bcmp_next
	caml	6,7
	jrst	p10bcmp_greater
	seto	1,
	popj	17,
p10bcmp_greater:
	movei	1,1
	popj	17,
p10bcmp_next:
	ibp	4
	ibp	5
	sojn	3,p10bcmp_loop
p10bcmp_equal:
	setz	1,
	popj	17,
