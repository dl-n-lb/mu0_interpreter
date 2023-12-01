start	
	LDA num
loop
	SUB div
	JGE notdone
	LDA res
	STP
notdone
	STO tmp
	LDA res
	ADD one
	STO res
	LDA tmp
	JMP loop


num DEFW 100
div DEFW 14
tmp DEFW 0
res DEFW 0
one DEFW 1
