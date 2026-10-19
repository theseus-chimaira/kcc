; PDP-6-compatible strspn, 9-bit unsigned C characters.
; AC1 = string, AC2 = accepted character set; return count in AC1.
; Count checked characters without subtracting PDP-10 byte pointers.
; This works across word boundaries and avoids pointer arithmetic helpers.
; No stack or writable data; clobbers AC1, AC3-AC7.
	.text
	.globl	strspn
strspn:
	move	3,1
	move	4,2
	setz	1,
strspn_v1_scan:
	ldb	5,3
	jumpe	5,strspn_v1_done
	move	6,4
strspn_v1_set:
	ldb	7,6
	jumpe	7,strspn_v1_done
	camn	5,7
	jrst	strspn_v1_next
	ibp	6
	jrst	strspn_v1_set
strspn_v1_next:
	ibp	3
	aoja	1,strspn_v1_scan
strspn_v1_done:
	popj	17,
