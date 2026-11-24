# --timing=espresso with --profile-model on the flat model, where all memory
# is DRAM - a loop of 3 iterations with a dependent add, a load used by the
# next instruction, a multiply and a taken branch back. The cycle counts are
# the model's (os-timing.s checks the model against the RTL).
# mach: brew
# sim: -v --timing=espresso --profile-model
# output: *pass\n*Cycles stalled due to branches: * 26\n*Cycles stalled due to loads: * 6\n
# output: *Total cycles (*approximate*): * 120\n*Espresso Timing (--timing=espresso)\n
# output: *Total cycles: * 120\n*Instructions entering execute: * 30\n
# output: *Refill after a taken branch: * 26 *Waiting for a register: * 33 *
# output: Waiting for memory or the bus: * 6 *Multiply lockout: * 3 *

.text
    .global _start
    _start:
        $r1 <- 3
        $r2 <- 0
        $r6 <- data
    loop:
        $r2 <- $r2 + $r1
        $r2 <- $r2 + $r1
        $r3 <- mem[$r6]
        $r3 <- $r3 + $r2
        $r5 <- $r3 * $r1
        $r1 <- tiny $r1 - 1
        if $r1 != 0 $pc <- loop
        $r4 <- 1
        $r5 <- pass_str
        $r6 <- 5
        syscall
        .hword 5        # SYS_write
        $r4 <- 0
        syscall
        .hword 1        # SYS_exit

.data
    data:
        .word 7
    pass_str:
        .ascii "pass\n"
