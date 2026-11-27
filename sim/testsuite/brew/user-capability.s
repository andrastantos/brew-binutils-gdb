# csr_capability reports the multiplier (MUL_SUPPORT, bit 1) and the shifter
# (SHIFT_SUPPORT, bit 2), and no floating-point unit. The exit status is the
# register's value, after a multiply that must work.
# mach: brew
# status: 6
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
