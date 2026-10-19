; PDP-6-compatible strpbrk, 9-bit unsigned C characters.
; AC1 = string, AC2 = accepted character set; return matching
; character's actual byte pointer in AC1, or zero.
; Single-pass scanning, no stack and no writable data.
; Advances the result pointer directly in AC1, rather than copying it.
; No saved string or accept-set pointer; the empty set never matches NUL.
; Clobbers AC1, AC5-AC7.
	.text
	.globl	strpbrk
strpbrk:
strpbrk_v1_scan:
	ldb	5,1
	jumpe	5,strpbrk_v1_miss
	move	6,2
strpbrk_v1_set:
	ldb	7,6
	jumpe	7,strpbrk_v1_next
	camn	5,7
	popj	17,
	ibp	6
	jrst	strpbrk_v1_set
strpbrk_v1_next:
	ibp	1
	jrst	strpbrk_v1_scan
strpbrk_v1_miss:
	setz	1,
	popj	17,
