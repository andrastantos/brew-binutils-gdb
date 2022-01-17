# check unary operators
# mach: brew

.include "framework/macros.s"
.include "framework/syscall.s"
.include "framework/fileno.s"
.include "framework/framework.s"

  # Create a set of initial values for all registers (these are all primes)
  .set NUM_1,  1
  .set NUM_2,  0x12345678
  .set NUM_3,  0
  .set NUM_4,  -7
  .set NUM_5,  9
  .set NUM_6,  11
  .set NUM_7,  13
  .set NUM_8,  15
  .set NUM_9,  17
  .set NUM_10, 131
  .set NUM_11, 37000
  .set NUM_12, 31
  .set NUM_13, 32
  .set NUM_14, 33

  # Create a set of initial values for all registers
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .set IN_R$r\R, NUM_\R
  .set IN_R$sr\R, NUM_\R
  .endr

  .macro test_inc RD, RS
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- \RS + 1
  .set EXP_VAL, (IN_R\RS + 1) & 0xffffffff
  TEST \RD, EXP_VAL
  .endm

  .macro test_dec RD, RS
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- \RS - 1
  .set EXP_VAL, (IN_R\RS - 1) & 0xffffffff
  TEST \RD, EXP_VAL
  .endm

  .macro test_neg RD, RS
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- - \RS
  .set EXP_VAL, (-IN_R\RS) & 0xffffffff
  TEST \RD, EXP_VAL
  .endm

  .macro test_not RD, RS
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- ~\RS
  .set EXP_VAL, (~(IN_R\RS)) & 0xffffffff
  TEST \RD, EXP_VAL
  .endm

  .macro test_bswap RD, RS
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- bswap \RS
  .set EXP_VAL, ((((IN_R\RS) >> 0) & 0xff) << 24) | ((((IN_R\RS) >> 8) & 0xff) << 16) | ((((IN_R\RS) >> 16) & 0xff) << 8) | ((((IN_R\RS) >> 24) & 0xff) << 0)
  TEST \RD, EXP_VAL
  .endm

  .macro test_wswap RD, RS
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- wswap \RS
  .set EXP_VAL, ((((IN_R\RS) >> 0) & 0xffff) << 16) | ((((IN_R\RS) >> 16) & 0xffff) << 0)
  TEST \RD, EXP_VAL
  .endm

  .macro test_bsi RD, RS
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- bsi \RS
  .set EXP_VAL, (((-((IN_R\RS >> 7) & 0x1)) & 0xffffff00) | (IN_R\RS & 0xff))
  TEST \RD, EXP_VAL
  .endm

  .macro test_wsi RD, RS
  # Set up registers to unique values
  .irp R,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\R <- IN_R$r\R
  .endr
  \RD <- wsi \RS
  .set EXP_VAL, (((-((IN_R\RS >> 15) & 0x1)) & 0xffff0000) | (IN_R\RS & 0xffff))
  TEST \RD, EXP_VAL
  .endm

####################################################
  start
  .irp D,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp S,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  test_inc $r\D, $r\S
  test_dec $sr\D, $sr\S
  test_neg $sr\D, $sr\S
  test_not $r\D, $r\S
  test_bswap $r\D, $r\S
  test_wswap $r\D, $r\S
  test_bsi $sr\D, $sr\S
  test_wsi $sr\D, $sr\S
  .endr
  .endr
  pass
