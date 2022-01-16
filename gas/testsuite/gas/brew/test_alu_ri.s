  .text
  .p2align 1

  ######################################################
  # special cases
  ######################################################
  .irp IMM 0,42,-42,0xffffffff
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  # 0x0... group
  $r\D <- \IMM

  # 0x8... group
  .if \D == 0
  $tpc <- \IMM
  .endif

  .endr
  .endr





  ######################################################
  # register-to-register binary ALU operations
  ######################################################
  .irp IMM 0,42,-42,0xffffffff
  .irp B,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  # 0x0... group
  $r\D <- $r\B ^ \IMM

  # 0x1... group
  $r\D <- $r\B | \IMM

  # 0x2... group
  $r\D <- $r\B & \IMM

  # 0x3... group
  $r\D <- $r\B - \IMM

  # 0x4... group
  $r\D <- $r\B + \IMM

  # 0x5... group
  $r\D <- $r\B << \IMM

  # 0x6... group
  $r\D <- $r\B >> \IMM

  # 0x7... group
  $sr\D <- $sr\B >> \IMM

  # 0x8... group
  .if \B != 0 && \D != 0
  $r\D <- $r\B * \IMM
  .endif

  # 0x9... group
  .if \B != 0 && \D != 0
  $sr\D <- $sr\B * \IMM
  .endif

  # 0xa... group
  .if \B != 0 && \D != 0
  $r\D <- upper $r\B * \IMM
  .endif

  # 0xb... group
  .if \B != 0 && \D != 0
  $sr\D <- upper $sr\B * \IMM
  .endif

  # 0xc... group
  .if \B != 0 && \D != 0
  $fr\D <- $fr\B + \IMM
  .endif

  # 0xd... group
  .if \B != 0 && \D != 0
  $fr\D <- $fr\B - \IMM
  .endif

  # 0xe... group
  .if \B != 0 && \D != 0
  $fr\D <- $fr\B * \IMM
  .endif

  .endr
  .endr
  .endr
