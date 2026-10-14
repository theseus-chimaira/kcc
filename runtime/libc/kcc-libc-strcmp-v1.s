	.text
	.globl	strcmp
; Compare unsigned 9-bit characters. Result in AC1, no stack use.
strcmp:
	move	3,1
	move	4,2
strcmp_v1_loop:
	ldb	5,3
	ldb	6,4
	move	1,5
	sub	1,6
	jumpn	1,strcmp_v1_done
	jumpe	5,strcmp_v1_done
	ibp	3
	ibp	4
	jrst	strcmp_v1_loop
strcmp_v1_done:
	popj	17,
