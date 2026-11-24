# The Espresso cycle model around indirect jumps through memory, back to back
# with other branches - an indirect jump is decided when its load returns, and
# the instructions behind it wait (the RTL's indirect_branch_lockout). The same
# program as the RTL test rig's test_branch_ind (anachron/rtl/espresso/test),
# whose cycles these are (6 later in the rig, which counts from reset). Each
# block adds a bit to $r9, so the exit status is 0 only if no wrong path ran.
# mach: brew
# sim: --model espresso --environment operating --timing=espresso --trace-insn
# ld: --section-start=.reset=0 -Ttext=0x08000000 -e 0
# status: 0
# output: *\[265\] \$spc: 0x8000070 *\[283\] \$spc: 0x800007e *\[285\] \$spc: 0x8000084 *
# output: \[302\] \$spc: 0x800008e *\[304\] \$spc: 0x8000094 *\[319\] \$spc: 0x800009e *
# output: \[330\] \$spc: 0x80000aa *\[332\] \$spc: 0x80000b0 *\[348\] \$spc: 0x8000160 *
# output: \[359\] \$spc: 0x80000b4 *\[378\] \$spc: 0x8000160 *\[390\] \$spc: 0x80000b8 *
# output: \[392\] \$spc: 0x80000be *\[400\] \$spc: 0x80000c2 *\[410\] \$spc: 0x80000cc *
# output: \[416\] \$spc: 0x80000d0 *\[445\] \$spc: 0x80000da *\[447\] \$spc: 0x80000e0 *
# output: \[455\] \$spc: 0x80000e4 *\[469\] \$spc: 0x80000e8 *\[470\] \$spc: 0x80000ee *
# output: \[481\] \$spc: 0x80000fa *\[506\] \$spc: 0x8000104 *\[509\] \$spc: 0x800010a *
# output: \[510\] \$spc: 0x800010e *\[525\] \$spc: 0x8000118 *\[528\] \$spc: 0x800011e *
# output: \[539\] \$spc: 0x800012c *\[541\] \$spc: 0x8000132 *\[569\] \$spc: 0x8000136 *
# output: \[571\] \$spc: 0x800013c *\[588\] \$spc: 0x8000140 *\[591\] \$spc: 0x8000146 *
# output: \[602\] \$spc: 0x800014a *\[621\] \$spc: 0x800014e *
.section .reset,"ax"
        $pc <- 0x08000000

.text
	$r6 <- 0x08000800
	$r9 <- 0
	$r1 <- A1
	mem[$r6 + 0] <- $r1
	$r1 <- A2
	mem[$r6 + 4] <- $r1
	$r1 <- B1
	mem[$r6 + 8] <- $r1
	$r1 <- C1
	mem[$r6 + 12] <- $r1
	$r1 <- D1
	mem[$r6 + 16] <- $r1
	$r1 <- E1
	mem[$r6 + 20] <- $r1
	$r1 <- F1
	mem[$r6 + 24] <- $r1
	$r1 <- FN
	mem[$r6 + 28] <- $r1
	$r1 <- 0x1234
	mem[$r6 + 32] <- $r1
	$r1 <- G1
	mem[$r6 + 36] <- $r1

# 1. indirect jump directly followed by another one (wrong path), target is an indirect jump
	$pc <- mem[$r6 + 0]
	$pc <- mem[$r6 + 4]          # wrong path
	$r9 <- $r9 + 1000            # wrong path
A1:
	$r9 <- $r9 + 1
	$pc <- mem[$r6 + 4]          # indirect jump at the target of an indirect jump
	$r9 <- $r9 + 2000
A2:
	$r9 <- $r9 + 2
# 2. indirect jump to a direct jump, to a return
	$pc <- mem[$r6 + 8]
	$r9 <- $r9 + 3000
B1:
	$pc <- B2                    # direct jump right at the target
	$r9 <- $r9 + 4000
B2:
	$r9 <- $r9 + 4
	call mem[$r6 + 28]           # FN returns at once
	call mem[$r6 + 28]           # back-to-back calls through memory
	$r9 <- $r9 + 8
# 3. $tpc <- mem (no branch in SCHEDULER mode) followed by branches
	$tpc <- mem[$r6 + 32]
	if $r9 == $r9 $pc <- C0      # conditional branch right after it
	$r9 <- $r9 + 5000
C0:
	$tpc <- mem[$r6 + 32]
	$pc <- mem[$r6 + 12]         # indirect jump right after it
	$r9 <- $r9 + 6000
C1:
	$r9 <- $r9 + 16
	$tpc <- mem[$r6 + 32]
	$tpc <- mem[$r6 + 32]        # two in a row
	$r9 <- $r9 + 32
# 4. direct and conditional branches whose target or fall-through is an indirect jump
	$pc <- D0
	$r9 <- $r9 + 7000
D0:
	$pc <- mem[$r6 + 16]
	$r9 <- $r9 + 8000
D1:
	$r9 <- $r9 + 64
	if $r9 != $r9 $pc <- E0      # not taken: falls into an indirect jump
	$pc <- mem[$r6 + 20]
E0:
	$r9 <- $r9 + 9000
E1:
	$r9 <- $r9 + 128
	if $r9 == $r9 $pc <- F0      # taken: the indirect jump in its shadow must not run
	$pc <- mem[$r6 + 24]
	$r9 <- $r9 + 10000
F0:
	$r9 <- $r9 + 256
	$pc <- mem[$r6 + 24]
F1:
	$r9 <- $r9 + 512
# 5. indirect jump to the next instruction, twice
	$pc <- mem[$r6 + 36]
G1:
	$r1 <- G2
	mem[$r6 + 40] <- $r1
	$pc <- mem[$r6 + 40]
G2:
	$r9 <- $r9 + 1024
	$pc <- EXIT                  # same length as the rig's terminator store
1:	$pc <- 1b
FN:
	$pc <- $lr
EXIT:
	$r9 <- $r9 - 2047
	$r1 <- 0x20200
	mem32[$r1] <- $r9
2:	$pc <- 2b
