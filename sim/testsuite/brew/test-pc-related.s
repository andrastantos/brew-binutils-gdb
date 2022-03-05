# mach: brew
.section .rodata
    .p2align 2
    .Lstr_pass:
        .string "pass\n"
    .Lstr_pass_end:
    .Lstr_fail:
        .string "fail\n"
    .Lstr_fail_end:
.data
    .p2align 2
    .buffer:
        .rept 256
        .long 0
        .endr

.text
    .global _start
    _start:
        ## test_pc_load r_b=$r10 offset=None
        ###################################################
        $r0 <- (1)
        $r1 <- (4294967294)
        $r2 <- (4)
        $r3 <- (4294967288)
        $r4 <- (16)
        $r5 <- (4294967264)
        $r6 <- (64)
        $r7 <- (4294967168)
        $r8 <- (256)
        $r9 <- (4294966784)
        $r10 <- (1024)
        $r11 <- (4294965248)
        $r12 <- (4096)
        $r13 <- (4294959104)
        $r14 <- (16384)
        #>>>>>>>
        $r0 <- 3f
        $r10 <- .buffer
        mem[$r10] <- $r0
        $r0 <- 0xc001c001
        $pc <- mem[$r10]
        $r0 <- 0xdeadbeef
    3:
        $r10 <- $r0
        #<<<<<<<
        $r0 <- $r0 + (-(3221340161))
        if $r0 != 0 $pc <- 1f
        $r1 <- $r1 + (-(4294967294))
        if $r1 != 0 $pc <- 1f
        $r2 <- $r2 + (-(4))
        if $r2 != 0 $pc <- 1f
        $r3 <- $r3 + (-(4294967288))
        if $r3 != 0 $pc <- 1f
        $r4 <- $r4 + (-(16))
        if $r4 != 0 $pc <- 1f
        $r5 <- $r5 + (-(4294967264))
        if $r5 != 0 $pc <- 1f
        $r6 <- $r6 + (-(64))
        if $r6 != 0 $pc <- 1f
        $r7 <- $r7 + (-(4294967168))
        if $r7 != 0 $pc <- 1f
        $r8 <- $r8 + (-(256))
        if $r8 != 0 $pc <- 1f
        $r9 <- $r9 + (-(4294966784))
        if $r9 != 0 $pc <- 1f
        $r10 <- $r10 + (-(3221340161))
        if $r10 != 0 $pc <- 1f
        $r11 <- $r11 + (-(4294965248))
        if $r11 != 0 $pc <- 1f
        $r12 <- $r12 + (-(4096))
        if $r12 != 0 $pc <- 1f
        $r13 <- $r13 + (-(4294959104))
        if $r13 != 0 $pc <- 1f
        $r14 <- $r14 + (-(16384))
        if $r14 != 0 $pc <- 1f
        $pc <- 2f
    1:
        $r3 <- $r3 ^ $r3
        $r4 <- 1
        $r5 <- .Lstr_fail
        $r6 <- (.Lstr_fail_end - 1)
        $r6 <- $r6 - $r5
        syscall
        .hword 5
        $r4 <- 0
        syscall
        .hword 1
    2:

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
