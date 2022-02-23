  .text
  .p2align 1

  ######################################################
  # special cases
  ######################################################
  .irp IMM 0,42,-42,-1,0x7fff,-0x8000
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  $r\D <- short \IMM
  #$sr\D <- short \IMM

  .endr
  .endr

  ######################################################
  # register-to-register binary ALU operations
  ######################################################
  .irp IMM 0,42,-42,-1,0x7fff,-0x8000
  .irp B,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  $r\D <- short $r\B ^ \IMM
  $r\D <- short (\IMM) ^ $r\B

  $r\D <- short $r\B | \IMM
  $r\D <- short (\IMM) | $r\B

  $r\D <- short $r\B & \IMM
  $r\D <- short (\IMM) & $r\B

  #$r\D <- short $r\B - \IMM - This should be an alias to $r\B + (-\IMM), but I don't think it works at the moment
  $r\D <- short (\IMM) - $r\B

  $r\D <- short $r\B + \IMM
  $r\D <- short (\IMM) + $r\B

  $r\D <- short (\IMM) << $r\B
  $r\D <- short (\IMM) >> $r\B
  $sr\D <- short (\IMM) >> $r\B

  $r\D <- short $r\B * \IMM
  $r\D <- short (\IMM) * $r\B

  $sr\D <- short $sr\B * \IMM
  $sr\D <- short (\IMM) * $sr\B

  $r\D <- short upper $r\B * \IMM
  $r\D <- short upper (\IMM) * $r\B

  $sr\D <- short upper $sr\B * \IMM
  $sr\D <- short upper (\IMM) * $sr\B

  .endr
  .endr
  .endr
