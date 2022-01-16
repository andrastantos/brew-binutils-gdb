  .text
  .p2align 1

  ######################################################
  # register-to-register binary ALU operations
  ######################################################
  .irp IMM 0,42,-42,0xffffffff
  .irp A,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  # 0x3... group
  $r\D <- \IMM - $r\A

  # 0x5... group
  $r\D <- \IMM << $r\A

  # 0x6... group
  $r\D <- \IMM >> $r\A

  # 0x7... group
  $sr\D <- (\IMM) >> $r\A

  # 0xd... group
  .if \A != 0 && \D != 0
  $fr\D <- \IMM - $fr\A
  .endif

  .endr
  .endr
  .endr
