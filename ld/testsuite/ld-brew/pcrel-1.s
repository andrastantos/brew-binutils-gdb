# Branches to the farthest targets in other sections, from the start
# of each branch instruction: 65534 bytes ahead, 65536 bytes back.
	.section ybr,"ax",@progbits
	.globl _start
_start:
	if $r0 == $r0 $pc <- hi
	if $r1 == $r0 $pc <- lo
	.section .note.GNU-stack,"",@progbits
