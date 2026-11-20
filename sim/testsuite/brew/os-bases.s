# mach: brew
# sim: --environment operating --console-base=0x30000 --simctl-base=0x30100
# ld: -Ttext=0
# status: 5
# The console and the simulator control device can be moved.

.include "os-utils.inc"

.text
    .global _start
    _start:
        os_puts .Lstr_pass, .Lstr_pass_end, 0x30000
        os_exit 5, 0x30100

.section .rodata
    .Lstr_pass:
        .ascii "pass\n"
    .Lstr_pass_end:
