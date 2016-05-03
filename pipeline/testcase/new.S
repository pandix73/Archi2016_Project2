addi $4, $zero, 2	#10
addi $1, $1, 1	
beq $1, $4, 1
j 0x08	
addi $2, $zero, 1	#20
addi $3, $zero, 1
add $1, $2, $3	
add $2, $1, $3	
add $3, $2, $1		#30
add $1, $3, $3	
add $1, $2, $2	
add $2, $1, $1	
j 0x44				#40
jal 0x48 		
add $31 $31 $31 
jal 0x50
beq $31 $0 0		#50
sw $31 0($0)
add $31 $31 $31
add $31 $31 $31
beq $31 $0 0		#60
addi $1, $zero, 0x88
sll $1, $1, 1
srl $1, $1, 1
jr $1				#70
halt
halt
halt
halt				#80
halt
j 0x74