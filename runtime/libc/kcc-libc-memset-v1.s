	.text
	.globl	memset
; Freestanding PDP-6 memset.  AC1=destination byte pointer, AC2=fill
; character, AC3=character count.  Return original destination in AC1.
; Char units are 9 bits and four chars form a 36-bit word.  The short
; path touches only requested bytes; the aligned path fills full words.
memset:
	move	6,1
	jumpe	3,memset_done
	andi	2,777
	move	4,2
	lsh	4,11	; 9 bits (DAS reads bare literals as octal)
	ior	4,2
	move	5,4
	lsh	5,22	; 18 bits
	ior	4,5

memset_peel:
	cail	3,4
	trna
	jrst	memset_bytes
	hlrz	5,6
	cain	5,0331100
	jrst	memset_words
	dpb	2,6
	ibp	6
	soja	3,memset_peel

memset_words:
	move	5,3
	ash	5,-2
	andi	3,3
	hrrz	6,6
	; Seed one word, then let forward BLT replicate it across the
	; destination. This overlap is intentional and safe for uniform fill.
	caige	5,4
	jrst	memset_word_loop
	movem	4,(6)
	hrrz	7,6
	addi	7,1
	hrl	7,6
	move	2,6
	add	2,5
	subi	2,1
	blt	7,(2)
	add	6,5
	jumpe	3,memset_done
	; Restore the 9-bit fill value after borrowing AC2 for BLT's end.
	move	2,4
	andi	2,777
	hrli	6,0331100
	jrst	memset_bytes
memset_word_loop:
	movem	4,(6)
	addi	6,1
	sojn	5,memset_word_loop
	jumpe	3,memset_done
	hrli	6,0331100

memset_bytes:
	dpb	2,6
	ibp	6
	sojn	3,memset_bytes

memset_done:
	popj	17,
