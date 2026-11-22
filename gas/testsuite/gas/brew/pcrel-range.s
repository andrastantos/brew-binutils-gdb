# The branch offset is a 17-bit signed, even byte offset from the start
# of the instruction: -65536 to 65534.
	.text
back:
	$r0 <- tiny 0
	.space 65534
	if $r0 == $r0 $pc <- back	# -65536
	if $r1 == $r0 $pc <- fwd	# 65534
	.space 65530
fwd:
	$r0 <- tiny 0
