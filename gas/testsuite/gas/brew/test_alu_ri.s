  .text
  .p2align 1

  ######################################################
  # special cases
  ######################################################
  .irp IMM 0,42,-42,0xffffffff
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  .if \D == 0
  $pc <- \IMM
  .endif
  $r\D <- \IMM
  #$sr\D <- \IMM
  #$fr\D <- (\IMM)

  .endr
  .endr

  ######################################################
  # register-to-register binary ALU operations
  ######################################################
  .irp IMM 0,42,-42,0xffffffff
  .irp B,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  $r\D <- $r\B ^ \IMM
  $r\D <- (\IMM) ^ $r\B

  $r\D <- $r\B | \IMM
  $r\D <- (\IMM) | $r\B

  $r\D <- $r\B & \IMM
  $r\D <- (\IMM) & $r\B

  #$r\D <- $r\B - \IMM - This should be an alias to $r\B + (-\IMM), but I don't think it works at the moment
  $r\D <- (\IMM) - $r\B

  $r\D <- $r\B + \IMM
  $r\D <- (\IMM) + $r\B

  $r\D <- (\IMM) << $r\B
  $r\D <- (\IMM) >> $r\B
  $sr\D <- (\IMM) >> $r\B

  $r\D <- $r\B * \IMM
  $r\D <- (\IMM) * $r\B

  $sr\D <- $sr\B * \IMM
  $sr\D <- (\IMM) * $sr\B

  $r\D <- upper $r\B * \IMM
  $r\D <- upper (\IMM) * $r\B

  $sr\D <- upper $sr\B * \IMM
  $sr\D <- upper (\IMM) * $sr\B

  $fr\D <- $fr\B + \IMM
  $fr\D <- \IMM + $fr\B

  $fr\D <- \IMM - $fr\B

  $fr\D <- $fr\B * \IMM
  $fr\D <- \IMM * $fr\B

  .endr
  .endr
  .endr
