# With --no-multiply a multiply is an unknown instruction, as on an Espresso
# built without its multiplier, and the user environment stops with SIGILL.
# mach: brew
# sim: --no-multiply
# status: 4
# output: *signal 4*

.text
    .global _start
    _start:
        $r4 <- 6
        $r5 <- 7
        $r4 <- $r4 << $r5      # shifts still work
        $r4 <- $r4 * $r5
        $r4 <- 0               # not reached
        syscall
        .hword 1        # SYS_exit
        .section .note.GNU-stack,"",@progbits
