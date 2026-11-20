# mach: brew
# status: 6
# output: *signal 6*
# kill(getpid(), SIGABRT), as abort() does, stops the simulation with SIGABRT
# instead of sending the signal to the simulator process.

.text
    .global _start
    _start:
        syscall
        .hword 8        # SYS_getpid
        $r5 <- 6        # SIGABRT
        syscall
        .hword 9        # SYS_kill
        $r4 <- 1        # not reached
        syscall
        .hword 1        # SYS_exit
