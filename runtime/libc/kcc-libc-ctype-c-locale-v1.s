	.text
	.globl	isprint
	.globl	isspace
;
; Freestanding PDP-10 C-locale character classification, version 1.
;
; The compiler's C char holds 9 unsigned bits (0..0777).  In the C
; locale, the printable characters are ASCII 040..0176, while the
; whitespace characters are 011..015 and 040.  Other character values,
; including 0200..0777 and EOF (-1), have neither property.
;
; AC1 holds the int argument and returns 0 or 1.  No other registers,
; library globals, stack locations, or runtime functions are used.
; This is deliberately C-locale, not a Unicode/UTF-9 classifier.
;
isprint:
	caige	1,040
	 jrst	isprint_v1_no
	caile	1,0176
	 jrst	isprint_v1_no
	movei	1,1
	popj	17,
isprint_v1_no:
	setz	1,
	popj	17,

isspace:
	caie	1,040
	 jrst	isspace_v1_controls
	movei	1,1
	popj	17,
isspace_v1_controls:
	caige	1,011
	 jrst	isspace_v1_no
	caile	1,015
	 jrst	isspace_v1_no
	movei	1,1
	popj	17,
isspace_v1_no:
	setz	1,
	popj	17,
