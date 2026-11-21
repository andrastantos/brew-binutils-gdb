# mach: brew
# BREW_getentropy (0x8002) fills a buffer of any length with random bytes
# (here 300, more than the host call's 256-byte limit) and nothing past it.

.text
    .global _start
    _start:
        $r4 <- buf
        $r5 <- 300
        syscall
        .hword 0x8002   # BREW_getentropy
        if $r4 != 0 $pc <- fail
        # The last word, beyond the first 256-byte chunk, was overwritten
        # (a false failure needs all 32 random bits to match the fill)
        $r0 <- mem[buf + 296]
        $r1 <- 0x5a5a5a5a
        if $r0 == $r1 $pc <- fail
        # ... and the word after the buffer was not
        $r0 <- mem[buf + 300]
        if $r0 != $r1 $pc <- fail
        $r4 <- 1
        $r5 <- pass_str
        $r6 <- 5
        syscall
        .hword 5        # SYS_write
        $r4 <- 0
        syscall
        .hword 1        # SYS_exit
    fail:
        $r4 <- 1
        syscall
        .hword 1        # SYS_exit

.data
    pass_str:
        .ascii "pass\n"
    .p2align 2
    buf:
        .fill 304, 1, 0x5a
