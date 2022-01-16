  .text
  .p2align 1

  ######################################################
  # conditional branches with bit-test
  ######################################################
  .irp A,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp BIT,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  if $r\A[\BIT] == 0 $pc <- .Ltarget
  if $r\A[\BIT] == 1 $pc <- .Ltarget

  .endr
  .endr


  if $r1[12] == 0 $pc <- .Ltarget

  ######################################################
  # conditional branches with comparison to reg
  ######################################################
  .irp A,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
  .irp B,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

  if $r\A == $r\B $pc <- .Ltarget
  if $r\A != $r\B $pc <- .Ltarget
  if $r\A <  $r\B $pc <- .Ltarget
  if $r\A >  $r\B $pc <- .Ltarget
  if $r\A <= $r\B $pc <- .Ltarget
  if $r\A >= $r\B $pc <- .Ltarget

  if $sr\A == $sr\B $pc <- .Ltarget
  if $sr\A != $sr\B $pc <- .Ltarget
  if $sr\A <  $sr\B $pc <- .Ltarget
  if $sr\A >  $sr\B $pc <- .Ltarget
  if $sr\A <= $sr\B $pc <- .Ltarget
  if $sr\A >= $sr\B $pc <- .Ltarget

  if $fr\A == $fr\B $pc <- .Ltarget
  if $fr\A != $fr\B $pc <- .Ltarget
  if $fr\A <  $fr\B $pc <- .Ltarget
  if $fr\A >  $fr\B $pc <- .Ltarget
  if $fr\A <= $fr\B $pc <- .Ltarget
  if $fr\A >= $fr\B $pc <- .Ltarget

  .endr
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

  if $sr\A == 0 $pc <- .Ltarget
  if $sr\A != 0 $pc <- .Ltarget
  if $sr\A <  0 $pc <- .Ltarget
  if $sr\A >  0 $pc <- .Ltarget
  if $sr\A <= 0 $pc <- .Ltarget
  if $sr\A >= 0 $pc <- .Ltarget

  if $fr\A == 0 $pc <- .Ltarget
  if $fr\A != 0 $pc <- .Ltarget
  if $fr\A <  0 $pc <- .Ltarget
  if $fr\A >  0 $pc <- .Ltarget
  if $fr\A <= 0 $pc <- .Ltarget
  if $fr\A >= 0 $pc <- .Ltarget

  .endr

.Ltarget:
  nop
