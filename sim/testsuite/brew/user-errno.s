# mach: brew
# A failing syscall returns the new errno value in $lr, as newlib numbers it.
# A path component longer than NAME_MAX fails with ENAMETOOLONG, which is 91
# in newlib (36 on Linux). A successful syscall leaves $lr unchanged.

.text
    .global _start
    _start:
        # chdir(long_name) -> -1, $lr = 91
        $r4 <- long_name
        $lr <- 0
        syscall
        .hword 14       # SYS_chdir
        $r0 <- -1
        if $r4 != $r0 $pc <- fail
        $r0 <- 91
        if $lr != $r0 $pc <- fail
        # getpid() succeeds and keeps the errno value passed in
        $lr <- 1234
        syscall
        .hword 8        # SYS_getpid
        $r0 <- 1234
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
    long_name:
        .fill 300, 1, 0x61
        .byte 0
