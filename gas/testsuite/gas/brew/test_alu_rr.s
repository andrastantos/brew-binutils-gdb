  .text
  .p2align 1

  ######################################################
  # /very/ special cases
  ######################################################
  fill
  break
  syscall
  stm
  sii
  fence_rw_rw
  fence_r__rw
  fence__w_rw
  fence____rw
  fence_rw__w
  fence_r___w
  fence__w__w
  fence_____w
  fence_rw_r_
  fence_r__r_
  fence__w_r_
  fence____r_
  fence_rw___
  fence_r____
  fence__w___
  woi
  nop

  ######################################################
  # PC-related
  ######################################################
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  $tpc <- $r\D
  $r\D <- $tpc
  $pc <- $r\D
  $r\D <- $pc

  .endr

  .irp A,0,2,4,6,8,10,12,14,16,18,20,22,24,26,28
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  $r\D <- $pc + \A

  .endr
  .endr

  ######################################################
  # unary ALU operations
  ######################################################
  .irp A,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  $r\D <- $r\A + 1
  $r\D <- $r\A - 1
  $r\D <- -$r\A
  $r\D <- ~$r\A
  $r\D <- wse $r\A
  $r\D <- bse $r\A
  $r\D <- int $r\A
  $r\D <- float $r\A
  $r\D <- rsqrt $r\A
  $r\D <- sum $r\A
  $r\D <- 1 / $r\A

  .endr
  .endr


  ######################################################
  # register-to-register binary ALU operations
  ######################################################
  .irp A,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp B,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  $r\D <- $r\A ^ $r\B
  $r\D <- $r\A | $r\B
  $r\D <- $r\A & $r\B
  $r\D <- $r\A - $r\B
  $r\D <- $r\A + $r\B
  $r\D <- $r\A << $r\B
  $r\D <- $r\A >> $r\B
  $r\D <- $r\A >>> $r\B
  $r\D <- $r\A * $r\B

  .endr
  .endr
  .endr
