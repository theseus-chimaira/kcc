.text
	.globl $KDFSB

$KDFSB:
	fsbl	010,013
	setz	011,
	popj	017,
