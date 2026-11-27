# With --no-shift every shift form is an unknown instruction (here the short
# immediate one), as on an Espresso built without its barrel shifter.
# mach: brew
# sim: --no-shift
# status: 4
# output: *signal 4*

.text
    .global _start
    _start:
        $r4 <- 6
        $r5 <- 7
        $r4 <- $r4 * $r5       # multiplies still work
        $r4 <- short $r4 >> 3
        $r4 <- 0               # not reached
        syscall
        .hword 1        # SYS_exit
        .section .note.GNU-stack,"",@progbits
