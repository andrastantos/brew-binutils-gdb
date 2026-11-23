# --profile-insn counts instructions by form, with operands folded into
# placeholders (the signed offset of a tiny add or a branch becomes +/-)
# and a literal 0 kept in a compare with zero, and by class. The loop runs
# 3 times. (Options must follow this comment, the parser stops at the first
# other comment after one.)
# mach: brew
# sim: -v --profile-insn
# output: *pass\n*Instruction Statistics\n\n  Total: 17 insns\n
# output: *  arithmetic * 6 * 35.29 * 6 * 0 * 0\n
# output: *  $r <- $r + $r * 3 * 17.65\n
# output: *  $r <- tiny $r +/- N * 3 * 17.65\n
# output: *  if $r != 0 $pc <- $pc +/- N * 3 * 17.65\n
# output: *  syscall * 2 * 11.76\n*

.text
    .global _start
    _start:
        $r1 <- 3
        $r2 <- 0
    loop:
        $r2 <- $r2 + $r1
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
    pass_str:
        .ascii "pass\n"
