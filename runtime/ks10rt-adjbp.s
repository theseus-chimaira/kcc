; KS10 hardware ADJBP variant of the KCC helper ABI.
; AC1 is the byte pointer, AC16 the signed byte displacement.
; The hardware computes the adjusted pointer in its count AC.
; Unlike PDP-6, KS10 accumulators are addressable through low core.
.text
	.globl $ADJBP

$ADJBP:
	jumpe	01,$ADJX
	jumpe	016,$ADJX
	push	017,016
	adjbp	016,01
	move	01,016
	pop	017,016
$ADJX:
	popj	017,
