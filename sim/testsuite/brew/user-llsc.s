# mach: brew
# memll places a load-lock on its address; memsc stores only while that lock
# is valid, sets $rD to 0 on success and non-zero on failure, and always
# clears the lock.

.text
    .global _start
    _start:
        $r4 <- word
        # memsc without memll fails and leaves memory alone
        $r5 <- 7
        memsc[$r4] <- $r5
        if $r5 == 0 $pc <- fail
        $r0 <- mem[$r4]
        $r1 <- 41
        if $r0 != $r1 $pc <- fail
        # Atomic increment: memll, add, memsc, retry on failure
    retry:
        $r5 <- memll[$r4]
        $r5 <- tiny $r5 + 1
        memsc[$r4] <- $r5
        if $r5 != 0 $pc <- retry
        $r0 <- mem[$r4]
        $r1 <- 42
        if $r0 != $r1 $pc <- fail
        # The lock is gone after a successful memsc
        $r5 <- 9
        memsc[$r4] <- $r5
        if $r5 == 0 $pc <- fail
        # A lock on another address doesn't allow the store
        $r5 <- memll[$r4 + 4]
        $r5 <- 9
        memsc[$r4] <- $r5
        if $r5 == 0 $pc <- fail
        $r0 <- mem[$r4]
        $r1 <- 42
        if $r0 != $r1 $pc <- fail
        # The offset form
        $r5 <- memll[$r4 + 4]
        $r5 <- 5
        memsc[$r4 + 4] <- $r5
        if $r5 != 0 $pc <- fail
        $r0 <- mem[$r4 + 4]
        $r1 <- 5
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
    word:
        .int 41
        .int 0
