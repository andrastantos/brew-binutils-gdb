	.text
back:
	$r0 <- tiny 0
	.space 65536
	if $r0 == $r0 $pc <- back	# -65538
	if $r1 == $r0 $pc <- fwd	# 65536
	.space 65532
fwd:
	$r0 <- tiny 0
