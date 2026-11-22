# '$rD <- <expression> <binary operator> $rA' with no binary operator,
# or no expression before it: an error, not an internal error.
	$r1 <- int $r2
	$r1 <- float $r2
	$r1 <- 5 $r2
	$r1 <- short sum $r2
	$r1 <- * $r2
	$r1 <- short & $r2
