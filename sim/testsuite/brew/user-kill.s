# mach: brew
# status: 143
# output: *terminated by signal 15*
# kill() only reaches the program itself. Another pid fails, ignored signals
# return 0, and a terminating signal the sim has no stop reason for (SIGTERM)
# exits with 128 + the signal.

.text
    .global _start
    _start:
        syscall
        .hword 8        # SYS_getpid
        $r8 <- $r4
        # Another process: -1 (ESRCH), nothing happens
        $r4 <- $r8 + 1
        $r5 <- 6
        syscall
        .hword 9
        $r0 <- -1
        if $r4 != $r0 $pc <- fail
        # Signal 0 and SIGCHLD (ignored by default): 0
        $r4 <- $r8
        $r5 <- 0
        syscall
        .hword 9
        $r0 <- 0
        if $r4 != $r0 $pc <- fail
        $r4 <- $r8
        $r5 <- 20
        syscall
        .hword 9
        $r0 <- 0
        if $r4 != $r0 $pc <- fail
        # SIGTERM to process group 0
        $r4 <- 0
        $r5 <- 15
        syscall
        .hword 9
    fail:
        $r4 <- 1
        syscall
        .hword 1        # SYS_exit
