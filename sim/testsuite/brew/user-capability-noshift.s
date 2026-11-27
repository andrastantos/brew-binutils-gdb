# With --no-shift the multiply still works and csr_capability has only
# MUL_SUPPORT (bit 1) set.
# mach: brew
# sim: --no-shift
# status: 2
# output:

.text
    .global _start
    _start:
        $r4 <- 6
        $r5 <- 7
        $r4 <- $r4 * $r5
        $r4 <- $r4 - 42
        $r5 <- csr[1]   # csr_capability (0x8001 from TASK mode)
        $r4 <- $r4 + $r5
        syscall
        .hword 1        # SYS_exit
        .section .note.GNU-stack,"",@progbits
