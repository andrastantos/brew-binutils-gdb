# Helpers for operating-environment tests (--environment operating): the
# program starts at address 0 in SCHEDULER mode, with no syscalls. They use
# the devices of the Anachron FPGA system that the brew and espresso models
# attach there: the console (default 0x20000: DATA at +0, STATUS at +1,
# bit 1 = TX ready), GPIO3 at 0x20100 (prints every byte written to it), and
# the simulator control device (default 0x20200: a write ends the simulation
# with the written value as exit status). The bases can be overridden to match
# --console-base and --simctl-base.

    .macro os_puts str, str_end, console=0x20000
        $r5 <- \str
        $r6 <- \str_end
        $r1 <- (\console + 1)
        $r2 <- \console
    1:
        if $r5 == $r6 $pc <- 3f
    2:
        $r0 <- mem8[$r1]
        $r0 <- short 2 & $r0
        if $r0 == 0 $pc <- 2b
        $r3 <- mem8[$r5]
        mem8[$r2] <- $r3
        $r5 <- tiny $r5 + 1
        $pc <- 1b
    3:
    .endm

    # GPIO3 needs no polling
    .macro os_puts_gpio3 str, str_end
        $r5 <- \str
        $r6 <- \str_end
        $r2 <- 0x20100
    1:
        if $r5 == $r6 $pc <- 2f
        $r3 <- mem8[$r5]
        mem8[$r2] <- $r3
        $r5 <- tiny $r5 + 1
        $pc <- 1b
    2:
    .endm

    .macro os_exit status, simctl=0x20200
        $r1 <- \simctl
        $r0 <- \status
        mem32[$r1] <- $r0
    4:
        $pc <- 4b
    .endm

    # An 8-bit write, as the Anachron ROM code does to end an RTL simulation
    .macro os_exit8 status, simctl=0x20200
        $r1 <- \simctl
        $r0 <- \status
        mem8[$r1] <- $r0
    5:
        $pc <- 5b
    .endm
