# mach: brew
# sim: --environment operating
# ld: -Ttext=0
# status: 9
# An 8-bit write to the simulator control device ends the simulation too.

.include "os-utils.inc"

.text
    .global _start
    _start:
        os_puts .Lstr_pass, .Lstr_pass_end
        os_exit8 9

.section .rodata
    .Lstr_pass:
        .ascii "pass\n"
    .Lstr_pass_end:
