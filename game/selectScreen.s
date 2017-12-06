	.arch msp430g2553
	;; Assembly for selecting screen
	.global selectScreen
	.global notzero

selectScreen:
	cmp #0, &screen		;screen - 0
	jnz notzero		;if screen != 0
	call #mainScreen	;call mainScreen function
notzero:
	cmp #2, &screen		;screen - 2
	jnz end			;if screen != 2
	call #gameOver		;call gameOver function
end:	
