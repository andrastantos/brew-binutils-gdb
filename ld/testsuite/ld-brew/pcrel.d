#source: pcrel-1.s
#source: pcrel-2.s
#ld: --section-start=ybr=0x20000 --section-start=losec=0x10004 --section-start=hisec=0x2fffe
#objdump: -d -j ybr

.*:     file format elf32-brew


Disassembly of section ybr:

00020000 <_start>:
   20000:	00 f1 fe ff 	if \$r0 == \$r0 \$pc <- \$pc \+ 65534 \(0xfffe\)
   20004:	10 f1 01 00 	if \$r1 == \$r0 \$pc <- \$pc - 65536 \(0x10000\)
