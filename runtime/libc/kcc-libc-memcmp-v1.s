	.text
	.globl	memcmp
; Byte-wise unsigned compare, no temporary storage; -1/0/+1 result.
memcmp:
	jumpe	3,memcmp_v1_equal
	move	4,1
	move	5,2
memcmp_v1_loop:
	ldb	6,4
	ldb	7,5
	camn	6,7
	jrst	memcmp_v1_next
	caml	6,7
	jrst	memcmp_v1_greater
	seto	1,
	popj	17,
memcmp_v1_greater:
	movei	1,1
	popj	17,
memcmp_v1_next:
	ibp	4
	ibp	5
	sojn	3,memcmp_v1_loop
memcmp_v1_equal:
	setz	1,
	popj	17,
