	.text
	.globl	memcpy
memcpy:
	move 6,1
	move 7,3
	jumpe 7,memcpy_done

	move 4,6
	xor 4,2
	tlne 4,0777700
	jrst memcpy_byte_loop

memcpy_peel:
	cail 7,4
	trna
	jrst memcpy_byte_loop
	hlrz 4,6
	cain 4,0331100
	jrst memcpy_words
	ldb 4,2
	dpb 4,6
	ibp 2
	ibp 6
	soja 7,memcpy_peel

memcpy_words:
	move 5,7
	ash 5,-2
	andi 7,3
	hrrz 6,6
	hrrz 2,2
	; On large aligned transfers BLT copies all complete words with one
	; instruction. The byte tail is handled by the original DPB path.
	caige 5,4
	jrst memcpy_word_loop
	hrrz 4,6
	hrl 4,2
	move 3,6
	add 3,5
	subi 3,1
	blt 4,(3)
	add 6,5
	add 2,5
	jumpe 7,memcpy_done
	hrli 6,0331100
	hrli 2,0331100
	jrst memcpy_byte_loop
memcpy_word_loop:
	move 4,(2)
	movem 4,(6)
	addi 2,1
	addi 6,1
	sojn 5,memcpy_word_loop
	jumpe 7,memcpy_done
	hrli 6,0331100
	hrli 2,0331100

memcpy_byte_loop:
	ldb 4,2
	dpb 4,6
	ibp 2
	ibp 6
	sojn 7,memcpy_byte_loop

memcpy_done:
	popj 17,
