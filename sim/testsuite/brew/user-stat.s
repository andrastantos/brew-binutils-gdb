# mach: brew
# stat() and fstat() fill in newlib's struct stat (st_mode at offset 4,
# 72 bytes in all) and fail with newlib's errno values.

.text
    .global _start
    _start:
        # stat("/", buf) -> 0, S_ISDIR (st_mode & 0170000 == 0040000)
        $r4 <- root
        $r5 <- buf
        $lr <- 0
        syscall
        .hword 15       # SYS_stat
        if $r4 != 0 $pc <- fail
        $r0 <- mem[buf + 4]
        $r0 <- $r0 & 0170000
        $r1 <- 0040000
        if $r0 != $r1 $pc <- fail
        # Nothing is written past the 72 bytes of struct stat
        $r0 <- mem[buf + 72]
        $r1 <- 0x5a5a5a5a
        if $r0 != $r1 $pc <- fail
        # stat("/nonexistent...", buf) -> -1, ENOENT (2)
        $r4 <- missing
        $r5 <- buf
        syscall
        .hword 15       # SYS_stat
        $r0 <- -1
        if $r4 != $r0 $pc <- fail
        $r0 <- 2
        if $lr != $r0 $pc <- fail
        # fstat(1000, buf) -> -1, EBADF (9)
        $r4 <- 1000
        $r5 <- buf
        syscall
        .hword 10       # SYS_fstat
        $r0 <- -1
        if $r4 != $r0 $pc <- fail
        $r0 <- 9
        if $lr != $r0 $pc <- fail
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
    root:
        .string "/"
    missing:
        .string "/nonexistent-brew-sim-test/x"
    .p2align 2
    buf:
        .fill 76, 1, 0x5a
