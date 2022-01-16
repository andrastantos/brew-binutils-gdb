  .text
  .p2align 1
  # register-to-register ALU operations
  .irp A,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp B,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp D,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  #.irp A,1
  #.irp B,1
  #.irp D,1
  $r\D <- $r\A ^ $r\B
  $r\D <- $r\A | $r\B
  $r\D <- $r\A & $r\B
  $r\D <- $r\A - $r\B
  $r\D <- $r\A + $r\B
  $r\D <- $r\A << $r\B
  $r\D <- $r\A >> $r\B
  $sr\D <- $sr\A >> $r\B
  $r\D <- $r\A * $r\B
  $sr\D <- $sr\A * $sr\B
  $r\D <- upper $r\A * $r\B
  $sr\D <- upper $sr\A * $sr\B
  .endr
  .endr
  .endr
