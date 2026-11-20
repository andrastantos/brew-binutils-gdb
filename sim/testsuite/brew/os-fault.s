# mach: brew
# sim: --environment operating
# ld: -Ttext=0
# status: 11
# output: *unmapped address 0x90000000*
# A fault in SCHEDULER mode has no handler, so the simulation stops with SIGSEGV
# (it used to repeat the faulting instruction forever).

.text
    .global _start
    _start:
        $r4 <- 0x90000000
        $r5 <- mem[$r4]
        $pc <- _start
