# pc-relative branches: resolved by the assembler only for a target in
# the same section that the link can't replace; otherwise left to the
# linker as R_BREW_16_SPCREL with a zero field.
	.text
	.globl glob
	.weak wk
f:
	if $r0 == $r0 $pc <- loc	# same section, local: no reloc
	if $r1 == $r0 $pc <- other	# local in another section
	if $r2 == $r0 $pc <- ext	# undefined
	if $r3 == $r0 $pc <- glob	# global in this section
	if $r4 == $r0 $pc <- wk	# weak in this section
	if $r5 == $r0 $pc <- f	# same section, backwards: no reloc
loc:
glob:
wk:
	$r0 <- tiny 0

	.section .text.b,"ax",@progbits
	$r0 <- tiny 1
other:
	$r0 <- tiny 2
