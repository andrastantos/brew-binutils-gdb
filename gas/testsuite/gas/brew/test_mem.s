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
  $pc  <- mem32[$r\A, (\IMM)]
  $pc  <- mem  [$r\A, (\IMM)]
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
  $fr\D <- mem8 [$r\A]
  $sr\D <- mem8 [$r\A]
  $r\D  <- mem8 [$r\A]
  $fr\D <- mem16[$r\A]
  $sr\D <- mem16[$r\A]
  $r\D  <- mem16[$r\A]
  $fr\D <- mem32[$r\A]
  $sr\D <- mem32[$r\A]
  $r\D  <- mem32[$r\A]
  $fr\D <- mem  [$r\A]
  $sr\D <- mem  [$r\A]
  $r\D  <- mem  [$r\A]

  mem8 [$r\A] <- $fr\D
  mem8 [$r\A] <- $sr\D
  mem8 [$r\A] <- $r\D
  mem16[$r\A] <- $fr\D
  mem16[$r\A] <- $sr\D
  mem16[$r\A] <- $r\D
  mem32[$r\A] <- $fr\D
  mem32[$r\A] <- $sr\D
  mem32[$r\A] <- $r\D
  mem  [$r\A] <- $fr\D
  mem  [$r\A] <- $sr\D
  mem  [$r\A] <- $r\D
  .endif

  .if \A == 0
  $fr\D <- mem8 [\IMM]
  $sr\D <- mem8 [\IMM]
  $r\D  <- mem8 [\IMM]
  $fr\D <- mem16[\IMM]
  $sr\D <- mem16[\IMM]
  $r\D  <- mem16[\IMM]
  $fr\D <- mem32[\IMM]
  $sr\D <- mem32[\IMM]
  $r\D  <- mem32[\IMM]
  $fr\D <- mem  [\IMM]
  $sr\D <- mem  [\IMM]
  $r\D  <- mem  [\IMM]

  mem8 [\IMM] <- $fr\D
  mem8 [\IMM] <- $sr\D
  mem8 [\IMM] <- $r\D
  mem16[\IMM] <- $fr\D
  mem16[\IMM] <- $sr\D
  mem16[\IMM] <- $r\D
  mem32[\IMM] <- $fr\D
  mem32[\IMM] <- $sr\D
  mem32[\IMM] <- $r\D
  mem  [\IMM] <- $fr\D
  mem  [\IMM] <- $sr\D
  mem  [\IMM] <- $r\D
  .endif

  .if \IMM <= 0x7fff && \IMM >= -0x8000
  $fr\D <- mem8 [$r\A, (\IMM)]
  $sr\D <- mem8 [$r\A, (\IMM)]
  $r\D  <- mem8 [$r\A, (\IMM)]
  $fr\D <- mem16[$r\A, (\IMM)]
  $sr\D <- mem16[$r\A, (\IMM)]
  $r\D  <- mem16[$r\A, (\IMM)]
  $fr\D <- mem32[$r\A, (\IMM)]
  $sr\D <- mem32[$r\A, (\IMM)]
  $r\D  <- mem32[$r\A, (\IMM)]
  $fr\D <- mem  [$r\A, (\IMM)]
  $sr\D <- mem  [$r\A, (\IMM)]
  $r\D  <- mem  [$r\A, (\IMM)]

  mem8 [$r\A, (\IMM)] <- $fr\D
  mem8 [$r\A, (\IMM)] <- $sr\D
  mem8 [$r\A, (\IMM)] <- $r\D
  mem16[$r\A, (\IMM)] <- $fr\D
  mem16[$r\A, (\IMM)] <- $sr\D
  mem16[$r\A, (\IMM)] <- $r\D
  mem32[$r\A, (\IMM)] <- $fr\D
  mem32[$r\A, (\IMM)] <- $sr\D
  mem32[$r\A, (\IMM)] <- $r\D
  mem  [$r\A, (\IMM)] <- $fr\D
  mem  [$r\A, (\IMM)] <- $sr\D
  mem  [$r\A, (\IMM)] <- $r\D
  .endif

  .endr
  .endr
  .endr

