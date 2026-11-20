# mach: brew
# sim: --model espresso --environment operating
# ld: -Ttext=0
# status: 3
# The espresso model has the same devices (and ROM at address 0).

.include "os-utils.inc"

.text
    .global _start
    _start:
        os_puts .Lstr_pass, .Lstr_pass_end
        os_exit 3

.section .rodata
    .Lstr_pass:
        .ascii "pass\n"
    .Lstr_pass_end:
