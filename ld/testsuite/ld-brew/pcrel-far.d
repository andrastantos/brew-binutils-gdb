#source: pcrel-1.s
#source: pcrel-2.s
#ld: --section-start=ybr=0x20000 --section-start=losec=0x10002 --section-start=hisec=0x30000
#error: \(ybr\+0x2\): relocation truncated to fit: R_BREW_16_SPCREL against symbol `hi'.*\n.*\(ybr\+0x6\): relocation truncated to fit: R_BREW_16_SPCREL against symbol `lo'
