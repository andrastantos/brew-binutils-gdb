#as:
#objdump:-d -s -j .text -j .data
#source:align-code.s

.*align-code.o:     file format elf32-brew

Contents of section .text:
#...
Contents of section .data:
 0000 01000000 02  +\.\.\.\.\. *

Disassembly of section .text:

00000000 <f>:
   0:	0f 10 01 00 	\$r1 <- 1 \(0x1\)
   4:	00 00 
   6:	22 22       	nop

00000008 <L1>:
   8:	11 14       	\$r1 <- \$r1 \+ \$r1
   a:	22 22       	nop
   c:	22 22       	nop
   e:	22 22       	nop

00000010 <L2>:
  10:	0f 20 02 00 	\$r2 <- 2 \(0x2\)
  14:	00 00 
  16:	07 00       	unknown instruction 0x0007

00000018 <L3>:
  18:	02 e0       	\$pc <- \$lr
  1a:	22 22       	nop
  1c:	22 22       	nop
  1e:	22 22       	nop

00000020 <L4>:
  20:	02 e0       	\$pc <- \$lr
#pass
