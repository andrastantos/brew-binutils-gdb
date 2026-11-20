# mach: brew
# sim: --environment operating
# ld: -Ttext=0
# status: 42
# The value written to the simulator control device is the exit status.

.include "os-utils.inc"

.text
    .global _start
    _start:
        os_puts .Lstr_pass, .Lstr_pass_end
        os_exit 42

.section .rodata
    .Lstr_pass:
        .ascii "pass\n"
    .Lstr_pass_end:
