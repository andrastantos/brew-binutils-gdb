# check right-logical-shift
# mach: brew

.include "framework/macros.s"
.include "framework/syscall.s"
.include "framework/fileno.s"
.include "framework/framework.s"

  # Create a set of initial values for all registers (these are all primes)
  .set NUM_1,  1
  .set NUM_2,  3
  .set NUM_3,  0
  .set NUM_4,  -7
  .set NUM_5,  9
  .set NUM_6,  11
  .set NUM_7,  13
  .set NUM_8,  15
  .set NUM_9,  17
  .set NUM_10, 19
  .set NUM_11, 30
  .set NUM_12, 31
  .set NUM_13, 32
  .set NUM_14, 33

  # Create a set of initial values for all registers
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .set IN_R$r\R, NUM_\R
  .set IN_R$sr\R, NUM_\R
  .endr

  # Macro for a single reg-reg test
  .macro test_rr RD, RA, RB
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- \RB >> \RA
  .set EXP_VAL, ((IN_R\RB & 0xffffffff) >> (IN_R\RA & 31)) & 0xffffffff
  TEST \RD, EXP_VAL
  .endm

  .macro test_ri RD, RB, IMM
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- \RB >> (\IMM)
  .set EXP_VAL, ((IN_R\RB & 0xffffffff) >> (\IMM & 31)) & 0xffffffff
  TEST \RD, EXP_VAL
  .endm

  .macro test_ir RD, IMM, RA
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- (\IMM) >> \RA
  .set EXP_VAL, ((\IMM & 0xffffffff) >> (IN_R\RA & 31)) & 0xffffffff
  TEST \RD, EXP_VAL
  .endm

####################################################
  start
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .set IMM, IN_R$r\R
  .irp A,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  test_ir $r\D, IMM, $r\A
  .endr
  .endr
  .endr

  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .set IMM, IN_R$r\R
  .irp B,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  test_ri $r\D, $r\B, IMM
  .endr
  .endr
  .endr

  .irp A,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp B,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  test_rr $r\D, $r\A, $r\B
  .endr
  .endr
  .endr
  pass
