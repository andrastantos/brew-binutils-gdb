  .text
  .p2align 1

  ######################################################
  # $tpc loads and stores
  ######################################################
  .irp IMM,0,1,-1,42,-42,0xffffffff,0x7fff,0x8000
  .irp A,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  .if \IMM == 0
  $pc <- mem32[$r\A]
  $pc <- mem  [$r\A]
  .endif

  .if \A == 0
  $pc  <- mem32[\IMM]
  $pc  <- mem  [\IMM]
  .endif

  .if \IMM <= 0x7fff && \IMM >= -0x8000
  $pc  <- mem32[$r\A+ (\IMM)]
  $pc  <- mem  [$r\A+ (\IMM)]
  .endif

  .endr
  .endr

  ######################################################
  # loads and stores
  ######################################################
  .irp IMM,0,1,-1,42,-42,0xffffffff,0x7fff,0x8000
  .irp A,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  .if \IMM == 0
  $r\D  <- mem8 [$r\A]
  $r\D  <- mem16[$r\A]
  $r\D  <- mem32[$r\A]
  $r\D  <- mem  [$r\A]

  mem8 [$r\A] <- $r\D
  mem16[$r\A] <- $r\D
  mem32[$r\A] <- $r\D
  mem  [$r\A] <- $r\D
  .endif

  .if \A == 0
  $r\D  <- mem8 [\IMM]
  $r\D  <- mem16[\IMM]
  $r\D  <- mem32[\IMM]
  $r\D  <- mem  [\IMM]

  mem8 [\IMM] <- $r\D
  mem16[\IMM] <- $r\D
  mem32[\IMM] <- $r\D
  mem  [\IMM] <- $r\D
  .endif

  .if \IMM <= 0x7fff && \IMM >= -0x8000
  $r\D  <- mem8 [$r\A + (\IMM)]
  $r\D  <- mem16[$r\A + (\IMM)]
  $r\D  <- mem32[$r\A + (\IMM)]
  $r\D  <- mem  [$r\A + (\IMM)]

  mem8 [$r\A+(\IMM)] <- $r\D
  mem16[$r\A+(\IMM)] <- $r\D
  mem32[$r\A+(\IMM)] <- $r\D
  mem  [$r\A+(\IMM)] <- $r\D
  .endif

  .endr
  .endr
  .endr

  ######################################################
  # tiny loads and stores
  ######################################################
  .irp IMM,0,4,-4,44,-44,508,-512
  .irp A,12,13
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  $r\D  <- mem32[$r\A + tiny (\IMM)]
  $r\D  <- mem  [$r\A + tiny (\IMM)]

  mem32[$r\A+tiny (\IMM)] <- $r\D
  mem  [$r\A+tiny (\IMM)] <- $r\D

  .endr
  .endr
  .endr

