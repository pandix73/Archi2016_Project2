addi $4, $zero, 2	#10
addi $1, $1, 1	
beq $1, $4, 4
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
j 0x60			
halt			
halt			
halt				#50
halt			
halt			
j 0x48				#60
j 0x48