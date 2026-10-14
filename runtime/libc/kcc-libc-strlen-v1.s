	.text
	.globl	strlen
; PDP-6 9-bit character strlen. Constant space, no stack or divide.
; Input AC1 byte pointer; return length in AC1. AC2-AC3 clobbered.
strlen:
	move	2,1
	setz	1,
strlen_v1_loop:
	ldb	3,2
	jumpe	3,strlen_v1_done
	ibp	2
	aoja	1,strlen_v1_loop
strlen_v1_done:
	popj	17,
