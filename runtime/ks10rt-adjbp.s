.text
	.globl $ADJBP

$ADJBP:
	jumpe	01,$ADJX
	jumpe	016,$ADJX
	jumpl	016,$ADJN
$ADJP:	ibp	01
	sojg	016,$ADJP
$ADJX:	popj	017,

$ADJN:	hlrz	013,01
	lsh	013,-06
	andi	013,077
	hlrz	014,01
	lsh	014,-012
	andi	014,077
	movei	015,044
	sub	015,013
	camn	014,015
	jrst	$ADJW
	add	014,013
	jrst	$ADJS
$ADJW:	subi	01,01
	movei	015,044
$ADJL:	sub	015,013
	jumpge	015,$ADJL
	add	015,013
$ADJD:	move	014,015
$ADJS:	hlrz	015,01
	andi	015,07777
	lsh	014,012
	ior	015,014
	hrlm	015,01
	aojl	016,$ADJN
	popj	017,
