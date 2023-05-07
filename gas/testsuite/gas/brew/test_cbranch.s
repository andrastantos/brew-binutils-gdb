  .text
  .p2align 1

  ######################################################
  # conditional branches with bit-test
  ######################################################
  .irp A,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp BIT,0,1,2,3,4,5,6,7,8,9,14,15,16,30,31

  if $r\A[\BIT] == 0 $pc <- .LtargetX
  if $r\A[\BIT] == 1 $pc <- .LtargetX

  .endr
  .endr

.LtargetX:

  ######################################################
  # conditional branches with comparison to reg
  ######################################################
  .irp A,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp B,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  if $r\A == $r\B $pc <- .Ltarget\A
  if $r\A != $r\B $pc <- .Ltarget\A
  if $r\A <  $r\B $pc <- .Ltarget\A
  if $r\A >  $r\B $pc <- .Ltarget\A
  if $r\A <= $r\B $pc <- .Ltarget\A
  if $r\A >= $r\B $pc <- .Ltarget\A

  if signed $r\A == $r\B $pc <- .Ltarget\A
  if signed $r\A != $r\B $pc <- .Ltarget\A
  if signed $r\A <  $r\B $pc <- .Ltarget\A
  if signed $r\A >  $r\B $pc <- .Ltarget\A
  if signed $r\A <= $r\B $pc <- .Ltarget\A
  if signed $r\A >= $r\B $pc <- .Ltarget\A

  if any $r\A == $r\B $pc <- .Ltarget\A
  if any $r\A != $r\B $pc <- .Ltarget\A
  if any $r\A <  $r\B $pc <- .Ltarget\A
  if any $r\A >  $r\B $pc <- .Ltarget\A
  if any $r\A <= $r\B $pc <- .Ltarget\A
  if any $r\A >= $r\B $pc <- .Ltarget\A

  if any signed $r\A == $r\B $pc <- .Ltarget\A
  if any signed $r\A != $r\B $pc <- .Ltarget\A
  if any signed $r\A <  $r\B $pc <- .Ltarget\A
  if any signed $r\A >  $r\B $pc <- .Ltarget\A
  if any signed $r\A <= $r\B $pc <- .Ltarget\A
  if any signed $r\A >= $r\B $pc <- .Ltarget\A

  if all $r\A == $r\B $pc <- .Ltarget\A
  if all $r\A != $r\B $pc <- .Ltarget\A
  if all $r\A <  $r\B $pc <- .Ltarget\A
  if all $r\A >  $r\B $pc <- .Ltarget\A
  if all $r\A <= $r\B $pc <- .Ltarget\A
  if all $r\A >= $r\B $pc <- .Ltarget\A

  if all signed $r\A == $r\B $pc <- .Ltarget\A
  if all signed $r\A != $r\B $pc <- .Ltarget\A
  if all signed $r\A <  $r\B $pc <- .Ltarget\A
  if all signed $r\A >  $r\B $pc <- .Ltarget\A
  if all signed $r\A <= $r\B $pc <- .Ltarget\A
  if all signed $r\A >= $r\B $pc <- .Ltarget\A

  .endr
.Ltarget\A:
  .endr

  ######################################################
  # conditional branches with comparison to 0
  ######################################################
  .irp A,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  if $r\A == 0 $pc <- .Ltarget
  if $r\A != 0 $pc <- .Ltarget
  if $r\A <  0 $pc <- .Ltarget
  if $r\A >  0 $pc <- .Ltarget
  if $r\A <= 0 $pc <- .Ltarget
  if $r\A >= 0 $pc <- .Ltarget

  if signed $r\A == 0 $pc <- .Ltarget
  if signed $r\A != 0 $pc <- .Ltarget
  if signed $r\A <  0 $pc <- .Ltarget
  if signed $r\A >  0 $pc <- .Ltarget
  if signed $r\A <= 0 $pc <- .Ltarget
  if signed $r\A >= 0 $pc <- .Ltarget


  .endr

.Ltarget:
  nop
