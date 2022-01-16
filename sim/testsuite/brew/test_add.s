# check that the sim doesn't die immediately.
# mach: brew

.include "framework/macros.s"
.include "framework/syscall.s"
.include "framework/fileno.s"
.include "framework/framework.s"

  # Create a set of initial values for all registers
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .set IN_R$r\R, (1 << \R)
  .endr

  # Macro for a single reg-reg test
  .macro test_add_rr RD, RA, RB
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .set IN_R$r\R, (1 << \R)
  $r\R <- (1 << \R)
  .endr
  \RD <- \RB + \RA
  .set EXP_VAL, IN_R\RB + IN_R\RA
  TEST \RD, EXP_VAL
  .endm

  .macro test_add_ri RD, RB, IMM
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .set IN_R$r\R, (1 << \R)
  $r\R <- (1 << \R)
  .endr
  \RD <- \RB + (\IMM)
  .set EXP_VAL, (IN_R\RB + \IMM) & 0xffffffff
  TEST \RD, EXP_VAL
  .endm

  .macro test_add_ir RD, IMM, RA
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .set IN_R$r\R, (1 << \R)
  $r\R <- (1 << \R)
  .endr
  \RD <- (\IMM) + \RA
  .set EXP_VAL, (IN_R\RA + \IMM) & 0xffffffff
  TEST \RD, EXP_VAL
  .endm

####################################################
  start
  .irp IMM,0,1,-1,0x1000,0xffffffff
  .irp A,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  test_add_ir $r\D, \IMM, $r\A
  .endr
  .endr
  .endr

  .irp IMM,0,1,-1,0x1000,0xffffffff
  .irp B,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  test_add_ri $r\D, $r\B, \IMM
  .endr
  .endr
  .endr

  .irp A,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp B,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  test_add_rr $r\D, $r\A, $r\B
  .endr
  .endr
  .endr
  pass
