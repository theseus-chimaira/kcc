	.text
	.globl	strncmp
; Length-bounded unsigned character comparison, no stack allocation.
strncmp:
	jumpe	3,strncmp_v1_equal
	move	4,1
	move	5,2
strncmp_v1_loop:
	ldb	6,4
	ldb	7,5
	move	1,6
	sub	1,7
	jumpn	1,strncmp_v1_done
	jumpe	6,strncmp_v1_done
	ibp	4
	ibp	5
	sojn	3,strncmp_v1_loop
strncmp_v1_equal:
	setz	1,
strncmp_v1_done:
	popj	17,
