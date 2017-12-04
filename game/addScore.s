	.arch msp430g2553
	.text
	.global addScore
	.global resetScore

addScore:
	mov &score, r12
	add #1, r12
	ret
resetScore:
	mov #048, r12
	ret
