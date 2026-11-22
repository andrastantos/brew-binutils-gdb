	.section losec,"ax",@progbits
	.globl lo
	.byte 0
lo:
	.byte 0
	.section hisec,"ax",@progbits
	.globl hi
hi:
	$r0 <- tiny 1
	.section .note.GNU-stack,"",@progbits
