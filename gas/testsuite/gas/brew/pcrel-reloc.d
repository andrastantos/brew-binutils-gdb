#as:
#objdump:-dr
#source:pcrel-reloc.s

.*pcrel-reloc.o:     file format elf32-brew


Disassembly of section .text:

00000000 <f>:
   0:	00 f1 18 00 	if \$r0 == \$r0 \$pc <- \$pc \+ 24 \(0x18\)
   4:	10 f1 00 00 	if \$r1 == \$r0 \$pc <- \$pc \+ 0 \(0x0\)
			6: R_BREW_16_SPCREL	.text.b\+0x2
   8:	20 f1 00 00 	if \$r2 == \$r0 \$pc <- \$pc \+ 0 \(0x0\)
			a: R_BREW_16_SPCREL	ext
   c:	30 f1 00 00 	if \$r3 == \$r0 \$pc <- \$pc \+ 0 \(0x0\)
			e: R_BREW_16_SPCREL	glob
  10:	40 f1 00 00 	if \$r4 == \$r0 \$pc <- \$pc \+ 0 \(0x0\)
			12: R_BREW_16_SPCREL	wk
  14:	50 f1 ed ff 	if \$r5 == \$r0 \$pc <- \$pc - 20 \(0x14\)

00000018 <glob>:
  18:	10 00       	\$r0 <- tiny 0

Disassembly of section .text.b:

00000000 <other-0x2>:
   0:	11 00       	\$r0 <- tiny 1

00000002 <other>:
   2:	12 00       	\$r0 <- tiny 2
