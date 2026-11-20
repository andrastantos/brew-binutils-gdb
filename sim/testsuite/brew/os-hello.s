# mach: brew
# sim: --environment operating
# ld: -Ttext=0
# status: 0
# Console output and exit status 0 through the simulator control device.

.include "os-utils.inc"

.text
    .global _start
    _start:
        os_puts .Lstr_pass, .Lstr_pass_end
        os_exit 0

.section .rodata
    .Lstr_pass:
        .ascii "pass\n"
    .Lstr_pass_end:
