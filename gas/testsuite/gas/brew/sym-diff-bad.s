# A difference of symbols in different sections can't be expressed
# with Brew relocations; it must be rejected, not truncated to its
# first operand.
	.text
L1:	$r1 <- $r2
	.data
	.4byte	ext-L1
	.4byte	L1-ext
