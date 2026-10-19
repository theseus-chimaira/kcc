; PDP-6-compatible strstr, 9-bit C bytes, KCC/GCC AC1-AC2 ABI.
; AC1 = haystack byte pointer, AC2 = needle byte pointer.
; Return AC1 = first matching byte pointer, or zero.
; No stack or writable data; clobbers AC1, AC3-AC7 only.
; The first needle byte is retained in AC4. The candidate is checked
; with two independent advancing descriptors, so an overlap mismatch
; always advances the haystack by exactly one character.
;
; Unlike the TENEX device STRING routines, this uses ordinary LDB/IBP
; directly: no XCTBU, monitor state, or device byte dispatch.
	.text
	.globl	strstr
strstr:
	ldb	4,2
	jumpe	4,strstr_v1_empty
	move	3,1
strstr_v1_scan:
	ldb	5,3
	jumpe	5,strstr_v1_miss
	camn	5,4
	jrst	strstr_v1_candidate
	ibp	3
	jrst	strstr_v1_scan
strstr_v1_candidate:
	move	6,3
	move	7,2
	ibp	6
	ibp	7
strstr_v1_suffix:
	ldb	1,7
	jumpe	1,strstr_v1_hit
	ldb	5,6
	jumpe	5,strstr_v1_miss
	came	1,5
	jrst	strstr_v1_next
	ibp	6
	ibp	7
	jrst	strstr_v1_suffix
strstr_v1_next:
	ibp	3
	jrst	strstr_v1_scan
strstr_v1_hit:
	move	1,3
strstr_v1_empty:
	popj	17,
strstr_v1_miss:
	setz	1,
	popj	17,
