# mach: brew
.section .rodata
    .p2align 2
    .Lstr_pass:
        .string "pass\n"
    .Lstr_pass_end:
    .Lstr_fail:
        .string "fail\n"
    .Lstr_fail_end:

.text
    .global _start
    _start:
        $r3 <- $r3 ^ $r3
        $r4 <- 1
        $r5 <- .Lstr_pass
        $r6 <- (.Lstr_pass_end - 1)
        $r6 <- $r6 - $r5
        syscall
        .hword 5
        $r4 <- 0
        syscall
        .hword 1
