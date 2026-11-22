; KCC shared signed byte-pointer adjustment for CPUs without ADJBP.
; Entry: AC1 = pointer, AC16 = signed element count.
; Exit:  AC1 = result; preserve AC13-AC16.
; The PDP-6 does not expose its accumulators as low-core memory operands.
; Keep every divisor and register transfer in real stack memory.
; A 36-bit word holds floor(36/S) bytes; normalize signed count to a
; word displacement and fewer than floor(36/S) forward IBP operations.
.text
	.globl $ADJBP

$ADJBP:
	jumpe	01,$ADJX
	jumpe	016,$ADJX
	add	017,[6,,6]
	movem	013,-5(017)
	movem	014,-4(017)
	movem	015,-3(017)
	movem	01,-2(017)
	movem	016,-1(017)
	hlrz	013,-2(017)
	lsh	013,-6
	andi	013,077
	jumpe	013,$ADJR
	movem	013,0(017)
	movei	014,044
	idiv	014,0(017)
	jumpe	014,$ADJR
	movem	014,0(017)
	move	013,-1(017)
	idiv	013,0(017)
	jumpge	014,$ADJA
	subi	013,1
	add	014,0(017)
$ADJA:
	movem	013,0(017)
	hrrz	015,-2(017)
	add	015,0(017)
	hrrm	015,-2(017)
	jumpe	014,$ADJR
$ADJP:
	ibp	-2(017)
	sojg	014,$ADJP
$ADJR:
	move	01,-2(017)
	move	015,-3(017)
	move	014,-4(017)
	move	013,-5(017)
	sub	017,[6,,6]
$ADJX:
	popj	017,
