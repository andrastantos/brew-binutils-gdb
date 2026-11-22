#as:
#objdump:-dr
#source:pcrel-range.s

.*pcrel-range.o:     file format elf32-brew


Disassembly of section .text:

00000000 <back>:
       0:	10 00       	\$r0 <- tiny 0
#...
   10000:	00 f1 01 00 	if \$r0 == \$r0 \$pc <- \$pc - 65536 \(0x10000\)
   10004:	10 f1 fe ff 	if \$r1 == \$r0 \$pc <- \$pc \+ 65534 \(0xfffe\)
#...
00020002 <fwd>:
   20002:	10 00       	\$r0 <- tiny 0
