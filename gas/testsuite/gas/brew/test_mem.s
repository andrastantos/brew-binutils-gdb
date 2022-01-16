  .text
  .p2align 1

  ######################################################
  # $tpc loads and stores
  ######################################################
  .irp IMM,0,1,-1,42,-42,0xffffffff
  .irp A,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  .if \IMM == 0
  $ftpc <- mem8 [$r\A]
  $stpc <- mem8 [$r\A]
  $tpc  <- mem8 [$r\A]
  $ftpc <- mem16[$r\A]
  $stpc <- mem16[$r\A]
  $tpc  <- mem16[$r\A]
  $ftpc <- mem32[$r\A]
  $stpc <- mem32[$r\A]
  $tpc  <- mem32[$r\A]
  $ftpc <- mem  [$r\A]
  $stpc <- mem  [$r\A]
  $tpc  <- mem  [$r\A]

  mem8 [$r\A] <- $ftpc
  mem8 [$r\A] <- $stpc
  mem8 [$r\A] <- $tpc
  mem16[$r\A] <- $ftpc
  mem16[$r\A] <- $stpc
  mem16[$r\A] <- $tpc
  mem32[$r\A] <- $ftpc
  mem32[$r\A] <- $stpc
  mem32[$r\A] <- $tpc
  mem  [$r\A] <- $ftpc
  mem  [$r\A] <- $stpc
  mem  [$r\A] <- $tpc
  .endif

  .if \A == 0
  $ftpc <- mem8 [\IMM]
  $stpc <- mem8 [\IMM]
  $tpc  <- mem8 [\IMM]
  $ftpc <- mem16[\IMM]
  $stpc <- mem16[\IMM]
  $tpc  <- mem16[\IMM]
  $ftpc <- mem32[\IMM]
  $stpc <- mem32[\IMM]
  $tpc  <- mem32[\IMM]
  $ftpc <- mem  [\IMM]
  $stpc <- mem  [\IMM]
  $tpc  <- mem  [\IMM]

  mem8 [\IMM] <- $ftpc
  mem8 [\IMM] <- $stpc
  mem8 [\IMM] <- $tpc
  mem16[\IMM] <- $ftpc
  mem16[\IMM] <- $stpc
  mem16[\IMM] <- $tpc
  mem32[\IMM] <- $ftpc
  mem32[\IMM] <- $stpc
  mem32[\IMM] <- $tpc
  mem  [\IMM] <- $ftpc
  mem  [\IMM] <- $stpc
  mem  [\IMM] <- $tpc
  .endif

  $ftpc <- mem8 [$r\A, (\IMM)]
  $stpc <- mem8 [$r\A, (\IMM)]
  $tpc  <- mem8 [$r\A, (\IMM)]
  $ftpc <- mem16[$r\A, (\IMM)]
  $stpc <- mem16[$r\A, (\IMM)]
  $tpc  <- mem16[$r\A, (\IMM)]
  $ftpc <- mem32[$r\A, (\IMM)]
  $stpc <- mem32[$r\A, (\IMM)]
  $tpc  <- mem32[$r\A, (\IMM)]
  $ftpc <- mem  [$r\A, (\IMM)]
  $stpc <- mem  [$r\A, (\IMM)]
  $tpc  <- mem  [$r\A, (\IMM)]

  mem8 [$r\A, (\IMM)] <- $ftpc
  mem8 [$r\A, (\IMM)] <- $stpc
  mem8 [$r\A, (\IMM)] <- $tpc
  mem16[$r\A, (\IMM)] <- $ftpc
  mem16[$r\A, (\IMM)] <- $stpc
  mem16[$r\A, (\IMM)] <- $tpc
  mem32[$r\A, (\IMM)] <- $ftpc
  mem32[$r\A, (\IMM)] <- $stpc
  mem32[$r\A, (\IMM)] <- $tpc
  mem  [$r\A, (\IMM)] <- $ftpc
  mem  [$r\A, (\IMM)] <- $stpc
  mem  [$r\A, (\IMM)] <- $tpc

  .endr
  .endr

  ######################################################
  # loads and stores
  ######################################################
  .irp IMM,0,1,-1,42,-42,0xffffffff
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

  .endr
  .endr
  .endr

