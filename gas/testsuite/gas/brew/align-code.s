# Alignment in code is filled with NOPs, alignment in data with zeros.
	.text
f:
	$r1 <- 1
	.p2align 2
L1:
	$r1 <- $r1 + $r1
	.p2align 3
L2:
	$r2 <- 2
	.byte 7
	.p2align 2
L3:
	$pc <- $lr
	.balign 16
L4:
	$pc <- $lr

	.data
	.byte 1
	.p2align 2
d:
	.byte 2
