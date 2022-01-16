  .text
  .p2align 1

  ######################################################
  # /very/ special cases
  ######################################################
  fill
  break
  syscall
  stu
  sii
  fence
  wfence
  woi
  nop

  ######################################################
  # register-to-register unary ALU operations
  ######################################################
  .irp A,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  # 0x8... group
  .if \A != 0
  $tpc <- $r\A
  $r\A <- $tpc
  .endif

  # 0x9... group
  .if \A != 0 && \D != 0
  $r\D <- $r\A + 1
  $r\D <- $r\A - 1
  .endif

  # 0xa... group
  .if \A != 0 && \D != 0
  $sr\D <- -$sr\A
  $r\D <- ~$r\A
  .endif

  # 0xb... group
  .if \A != 0 && \D != 0
  $r\D <- BSWAP $r\A
  $r\D <- WSWAP $r\A
  .endif

  # 0xc... group
  .if \A != 0 && \D != 0
  $sr\D <- WSI $r\A
  $sr\D <- BSI $r\A
  .endif

  # 0xd... group
  .if \A != 0 && \D != 0
  $fr\D <- $sr\A
  $sr\D <- floor $fr\A
  .endif

  # 0xe... group
  .if \A != 0 && \D != 0
  $fr\D <- rsqrt $fr\A
  $fr\D <- 1 / $fr\A
  .endif

  .endr
  .endr





  ######################################################
  # register-to-register binary ALU operations
  ######################################################
  .irp A,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp B,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  # 0x0... group
  .if \D != 0
    $r\D <- $r\A ^ $r\B
  .endif

  # 0x1... group
  .if \A != 0 || \B != 0 || \D !=0
    $r\D <- $r\A | $r\B
  .endif

  # 0x2... group
  $r\D <- $r\A & $r\B

  # 0x3... group
  $r\D <- $r\A - $r\B

  # 0x4... group
  $r\D <- $r\A + $r\B

  # 0x5... group
  $r\D <- $r\A << $r\B

  # 0x6... group
  $r\D <- $r\A >> $r\B

  # 0x7... group
  $sr\D <- $sr\A >> $r\B

  # 0x8... group
  .if \A != 0 && \B != 0 && \D != 0
  $r\D <- $r\A * $r\B
  .endif

  # 0x9... group
  .if \A != 0 && \B != 0 && \D != 0
  $sr\D <- $sr\A * $sr\B
  .endif

  # 0xa... group
  .if \A != 0 && \B != 0 && \D != 0
  $r\D <- upper $r\A * $r\B
  .endif

  # 0xb... group
  .if \A != 0 && \B != 0 && \D != 0
  $sr\D <- upper $sr\A * $sr\B
  .endif

  # 0xc... group
  .if \A != 0 && \B != 0 && \D != 0
  $fr\D <- $fr\A + $fr\B
  .endif

  # 0xd... group
  .if \A != 0 && \B != 0 && \D != 0
  $fr\D <- $fr\A - $fr\B
  .endif

  # 0xe... group
  .if \A != 0 && \B != 0 && \D != 0
  $fr\D <- $fr\A * $fr\B
  .endif

  .endr
  .endr
  .endr
