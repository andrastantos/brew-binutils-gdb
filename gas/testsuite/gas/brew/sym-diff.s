# Differences of labels in the same section must be resolved by the
# assembler, also when a variable-size (alignment) frag lies between
# the labels.  gcc emits such differences for DW_AT_high_pc and the
# .debug_aranges length (.Letext0-.Ltext0).
	.text
L1:	$r1 <- $r2
	$r1 <- $r2
L2:	.p2align 2
	$r1 <- $r2
L3:
	$r3 <- $r2 + (L3-L1)
	$r3 <- $r2 - (L3-L1)
	$r3 <- $r2 - L1
	$r3 <- $r2 - ext

	.data
	.4byte	L2-L1
	.4byte	L3-L1
	.4byte	L3-L1+10
	.2byte	L3-L2
	.2byte	0
	.4byte	L3
