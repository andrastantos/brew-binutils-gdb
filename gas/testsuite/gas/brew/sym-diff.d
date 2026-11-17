#as:
#objdump:-dr -s
#source:sym-diff.s

.*sym-diff.o:     file format elf32-brew

Contents of section .text:
 0000 22122212 22122f34 06000000 2f34faff  .*
 0010 ffff2f34 00000000 2f340000 0000      .*
Contents of section .data:
 0000 04000000 06000000 10000000 02000000  .*
 0010 06000000                             .*

Disassembly of section .text:

00000000 <L1>:
   0:	22 12       	\$r1 <- \$r2
   2:	22 12       	\$r1 <- \$r2

00000004 <L2>:
   4:	22 12       	\$r1 <- \$r2

00000006 <L3>:
   6:	2f 34 06 00 	\$r3 <- 6 \(0x6\) \+ \$r2
   a:	00 00 
   c:	2f 34 fa ff 	\$r3 <- -6 \(0xfffffffa\) \+ \$r2
  10:	ff ff 
  12:	2f 34 00 00 	\$r3 <- 0 \(0x0\) \+ \$r2
  16:	00 00 
			14: R_BREW_NEG32	.text
  18:	2f 34 00 00 	\$r3 <- 0 \(0x0\) \+ \$r2
  1c:	00 00 
			1a: R_BREW_NEG32	ext
