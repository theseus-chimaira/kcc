; PDP-6-compatible strpbrk, 9-bit unsigned C characters.
; AC1 = string, AC2 = accepted character set; return matching
; character's actual byte pointer in AC1, or zero.
; Single-pass scanning, no stack and no writable data.
; Clobbers AC1, AC3-AC7; the empty set never matches NUL.
	.text
	.globl	strpbrk
strpbrk:
	move	3,1
	move	4,2
strpbrk_v1_scan:
	ldb	5,3
	jumpe	5,strpbrk_v1_miss
	move	6,4
strpbrk_v1_set:
	ldb	7,6
	jumpe	7,strpbrk_v1_next
	camn	5,7
	jrst	strpbrk_v1_hit
	ibp	6
	jrst	strpbrk_v1_set
strpbrk_v1_next:
	ibp	3
	jrst	strpbrk_v1_scan
strpbrk_v1_hit:
	move	1,3
	popj	17,
strpbrk_v1_miss:
	setz	1,
	popj	17,
