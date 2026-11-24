# The Espresso cycle model with Espresso's memory map, on the RTL test rig's
# program layout - a jump at 0 in ROM, code in DRAM. This is the RTL
# microbenchmark t_ldind (in anachron/rtl/espresso's test rig) - the trace
# gives the cycle each instruction entered execute, as the rig's 'Exec' lines,
# which are 6 cycles later (the rig counts from reset).
# mach: brew
# sim: --model espresso --environment operating --timing=espresso --trace-insn
# ld: --section-start=.reset=0 -Ttext=0x08000000 -e 0
# status: 0
# output: *\[117\] $spc: 0x0 *\[161\] $spc: 0x8000000 *\[164\] $spc: 0x8000006 *\[167\] $spc: 0x800000c *
# output: \[172\] $spc: 0x8000012 *\[175\] $spc: 0x8000018 *\[178\] $spc: 0x800001e *\[181\] $spc: 0x8000024 *
# output: \[186\] $spc: 0x800002a *\[188\] $spc: 0x8000030 *\[190\] $spc: 0x8000034 *\[195\] $spc: 0x8000036 *
# output: \[199\] $spc: 0x800003a *\[212\] $spc: 0x800003c *

.include "os-utils.inc"

.section .reset,"ax"
        $pc <- 0x08000000

.text
    .global _start
    _start:
        $r1 <- 0x08000000
        $r2 <- 3
        $r6 <- 0x08000800
        $r7 <- 5
        $r3 <- 1
        $r4 <- 2
        $r5 <- 0
        $sp <- 0x08000000
        .rept 6
        $r3 <- mem[$r1 + 4]
        $r4 <- mem[tiny $sp + 4]
        .endr
        os_exit 0
