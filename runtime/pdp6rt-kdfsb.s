.text
	.globl $KDFSB

$KDFSB:
	fsbl	010,013
	popj	017,
