#source: pcrel-1.s
#source: pcrel-odd.s
#ld: --section-start=ybr=0x20000 --section-start=losec=0x20100 --section-start=hisec=0x20200
#error: \(ybr\+0x6\): dangerous relocation: branch target is not 2-byte aligned
