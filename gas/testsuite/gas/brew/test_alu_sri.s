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

  $r\D <- short $r\B - \IMM
  $r\D <- short (\IMM) - $r\B

  $r\D <- short $r\B + \IMM
  $r\D <- short (\IMM) + $r\B

  $r\D <- short $r\B << (\IMM)
  $r\D <- short $r\B >> (\IMM)
  $r\D <- short $r\B >>> (\IMM)

  $r\D <- short $r\B * \IMM
  $r\D <- short (\IMM) * $r\B

  .endr
  .endr
  .endr
