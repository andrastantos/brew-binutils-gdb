# mach: brew
# sim: --environment operating
# ld: -Ttext=0
# status: 0
# GPIO3 prints the bytes written to it, like the RTL test bench.

.include "os-utils.inc"

.text
    .global _start
    _start:
        os_puts_gpio3 .Lstr_pass, .Lstr_pass_end
        os_exit 0

.section .rodata
    .Lstr_pass:
        .ascii "pass\n"
    .Lstr_pass_end:
