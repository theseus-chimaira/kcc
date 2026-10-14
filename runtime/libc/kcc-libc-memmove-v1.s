	.text
	.globl	memmove
memmove:
	camn 1,2
	popj 17,
	jumpe 3,memmove_done

	move 7,1
	move 6,2
	move 4,1
	tlc 4,0770000
	move 5,2
	tlc 5,0770000
	rot 4,6
	rot 5,6
	caml 4,5
	jrst memmove_backward
	jrst memcpy

memmove_backward:
	move 7,1
	move 6,2
	move 5,3
	andi 5,3
	move 4,3
	ash 4,-2
	add 7,4
	add 6,4
	jumpe 5,memmove_end_ready
memmove_dst_end:
	ibp 7
	sojn 5,memmove_dst_end
	move 5,3
	andi 5,3
memmove_src_end:
	ibp 6
	sojn 5,memmove_src_end

memmove_end_ready:
	move 4,7
	xor 4,6
	tlne 4,0777700
	jrst memmove_reverse_bytes

memmove_reverse_peel:
	cail 3,4
	trna
	jrst memmove_reverse_bytes
	hlrz 4,7
	cain 4,0331100
	jrst memmove_reverse_words
	move 4,7
	subi 4,1
	hrr 7,4
	ibp 7
	ibp 7
	move 4,6
	subi 4,1
	hrr 6,4
	ibp 6
	ibp 6
	ildb 4,6
	idpb 4,7
	soja 3,memmove_reverse_peel

memmove_reverse_words:
	move 5,3
	ash 5,-2
	andi 3,3
	hrrz 7,7
	hrrz 6,6
memmove_reverse_word_loop:
	subi 6,1
	move 4,(6)
	subi 7,1
	movem 4,(7)
	sojn 5,memmove_reverse_word_loop
	jumpe 3,memmove_done
	hrli 7,0331100
	hrli 6,0331100

memmove_reverse_bytes:
	move 4,7
	subi 4,1
	hrr 7,4
	ibp 7
	ibp 7
	move 4,6
	subi 4,1
	hrr 6,4
	ibp 6
	ibp 6
	ildb 4,6
	idpb 4,7
	sojn 3,memmove_reverse_bytes

memmove_done:
	popj 17,
