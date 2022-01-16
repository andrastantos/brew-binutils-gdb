# check subtraction
# mach: brew

.include "framework/macros.s"
.include "framework/syscall.s"
.include "framework/fileno.s"
.include "framework/framework.s"

  # Create a set of initial values for all registers (these are all primes)
  .set PRIME_1,  19  << 20
  .set PRIME_2,  -23 << 20
  .set PRIME_3,  29  << 20
  .set PRIME_4,  -31 << 20
  .set PRIME_5,  37  << 20
  .set PRIME_6,  -41 << 20
  .set PRIME_7,  43  << 20
  .set PRIME_8,  -47 << 20
  .set PRIME_9,  53  << 20
  .set PRIME_10, -59 << 20
  .set PRIME_11, 61  << 20
  .set PRIME_12, -67 << 20
  .set PRIME_13, 71  << 20
  .set PRIME_14, -73 << 20

  # Create a set of initial values for all registers
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .set IN_R$r\R, PRIME_\R
  .set IN_R$sr\R, PRIME_\R
  .endr

  # Macro for a single reg-reg test
  .macro test_rr RD, RA, RB
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- upper \RB * \RA
  .set EXP_VAL, (((IN_R\RB & 0xffffffff) * (IN_R\RA & 0xffffffff)) >> 32) & 0xffffffff
  TEST \RD, EXP_VAL
  .endm

  .macro test_ri RD, RB, IMM
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- upper \RB * (\IMM)
  .set EXP_VAL, (((IN_R\RB & 0xffffffff) * (\IMM & 0xffffffff)) >> 32) & 0xffffffff
  TEST \RD, EXP_VAL
  .endm

  .macro test_ir RD, IMM, RA
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- upper \IMM * \RA
  .set EXP_VAL, (((\IMM & 0xffffffff) * (IN_R\RA & 0xffffffff)) >> 32) & 0xffffffff
  TEST \RD, EXP_VAL
  .endm

####################################################
  start
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .set IMM, IN_R$r\R | (1 << 28)
  .irp A,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  test_ir $r\D, IMM, $r\A
  .endr
  .endr
  .endr

  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .set IMM, IN_R$r\R | (1 << 28)
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
