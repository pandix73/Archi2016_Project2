#include <stdio.h>
#include <stdlib.h>

#define $sp 29
// R
#define R 0
#define add 32   
#define addu 33  
#define sub 34   
#define and 36   
#define or 37    
#define xor 38   
#define nor 39   
#define nand 40  
#define slt 42   
#define sll 0    
#define srl 2    
#define sra 3    
#define jr 8     
// I
#define addi 8   
#define addiu 9  
#define lw 35    
#define lh 33    
#define lhu 37   
#define lb 32    
#define lbu 36   
#define sw 43    
#define sh 41    
#define sb 40    
#define lui 15   
#define andi 12  
#define ori 13   
#define nori 14  
#define slti 10  
#define beq 4    
#define bne 5    
#define bgtz 7   
// J
#define j 2      
#define jal 3    
// S
#define halt 63  

FILE *i_file, *d_file, *error, *snap;
int i_size, d_size;
char *i_buffer, *d_buffer;
size_t i_result, d_result;
unsigned int PC, i_memory[1024];
unsigned int sp, d_data[256];
unsigned char d_memory[1024];
unsigned int reg[32];


typedef struct _IFtoID{
	unsigned instruction;
	unsigned PC;
}IFtoID;

typedef struct _IDtoEX{
	unsigned opcode;
	unsigned rs;
	unsigned rt;
	unsigned rd;
	unsigned C; //shamt, immediate, address
	unsigned funct;
}IDtoEX;

typedef struct _EXtoDM{
	unsigned ALUout;
	unsigned rd;
	unsigned rt;
}EXtoDM;

IFtoID IFID;
IDtoEX IDEX;
EXtoDM EXDM;

void read_d_memory(int load_num){
	int i;
	for(i = 0; i < load_num; i++){
		//change 12 34 56 78  to  78 56 34 12
		d_data[i] = d_data[i] << 24 | d_data[i] >> 8 << 24 >> 8 | d_data[i] >> 16 << 24 >> 16 | d_data[i] >> 24;	
	}
	reg[29] = d_data[0];
	for(i = 0; i < d_data[1]; i++){
		d_memory[i*4] = d_data[i+2] >> 24;
		d_memory[i*4 + 1] = d_data[i+2] << 8 >> 24;
		d_memory[i*4 + 2] = d_data[i+2] << 16 >> 24;
		d_memory[i*4 + 3] = d_data[i+2] << 24 >> 24;
	}
}

void read_i_memory(int load_num){
	unsigned int i, opcode;
	int funct, rs, rt, rd, shamt, C_26;
	short C;
	for(i = 0; i < load_num; i++){
		//change 12 34 56 78  to  78 56 34 12
		i_memory[i] = i_memory[i] << 24 | i_memory[i] >> 8 << 24 >> 8 | i_memory[i] >> 16 << 24 >> 16 | i_memory[i] >> 24;
	}
}



void IF(int PC){
	IFID.instruction = i_memory[PC/4 + 2];
	IFID.PC = PC + 4;
}



void ID(){
	IDEX.opcode = IFID.instruction >> 26;
	if(IDEX.opcode == R){	
		IDEX.funct = IFID.instruction << 26 >> 26;
		IDEX.rs    = IFID.instruction <<  6 >> 27;
		IDEX.rt    = IFID.instruction << 11 >> 27;
		
		IDEX.rs    = reg[IDEX.rs];
		IDEX.rt	   = reg[IDEX.rt];

		IDEX.rd	   = IFID.instruction << 16 >> 27;
		IDEX.C	   = IFID.instruction << 21 >> 27;
	} else if (IDEX.opcode >= 4 && IDEX.opcode <= 43){
		short tempC;
		IDEX.rs	   = IFID.instruction <<  6 >> 27;
		IDEX.rt	   = IFID.instruction << 11 >> 27;
		
		IDEX.rs    = reg[IDEX.rs];
		IDEX.rt    = reg[IDEX.rt];
		
		tempC	   = IFID.instruction << 16 >> 16;
		if(IDEX.opcode == andi || IDEX.opcode == ori || IDEX.opcode == nori)
			IDEX.C = (unsigned short)tempC;
		else 
			IDEX.C = tempC;
	} else if (IDEX.opcode < 4){
		IDEX.C	   = IFID.instruction <<  6 >>  6;
	} else {
	 // halt
	}
}

void EX(){	
	if(IDEX.opcode == R){	
		switch(IDEX.funct){
			case add:
				int s_sign = IDEX.rs >> 31;
				int t_sign = IDEX.rt >> 31;
				IDEX.ALUout = IDEX.rs + IDEX.rt;
				printf("add %u = %u + %u\n", IDEX.ALUout, IDEX.rs, IDEX.rt);
				if(s_sign == t_sign && s_sign != IDEX.ALUout >> 31)
					fprintf(error, "In cycle %d: Number Overflow\n", cycle);
				break;
			case addu:
				printf("addu\n");
				IDEX.ALUout = IDEX.rs + IDEX.rt;
				break;
			case sub:
				s_sign = IDEX.rs >> 31;
				t_sign = (~IDEX.rt + 1) >> 31;
				IDEX.ALUout = IDEX.rs + (~IDEX.rt + 1);
				printf("sub %d = %d - %d\n", IDEX.ALUout, IDEX.rs, IDEX.rt);
				if(s_sign == t_sign && s_sign != IDEX.ALUout >> 31)
					fprintf(error, "In cycle %d: Number Overflow\n", cycle);
				break;
			case and:
				printf("and\n");
				IDEX.ALUout = IDEX.rs & IDEX.rt;
				break;
			case or:
				printf("or\n");
				IDEX.ALUout = IDEX.rs | IDEX.rt;
				break;
			case xor:
				printf("xor\n");
				IDEX.ALUout = IDEX.rs ^ IDEX.rt;
				break;
			case nor:
				printf("nor\n");
				IDEX.ALUout = ~(IDEX.rs | IDEX.rt);
				break;
			case nand:
				printf("nand\n");
				IDEX.ALUout = ~(IDEX.rs & IDEX.rt);
				break;
			case slt:
				IDEX.ALUout = ((int)IDEX.rs < (int)IDEX.rt);
				printf("slt %d = %d < %d\n", IDEX.ALUout, IDEX.rs, IDEX.rt);
				break;
			case sll:
				printf("sll\n");
				IDEX.ALUout = IDEX.rt << shamt;
				break;
			case srl:
				printf("srl\n");
				IDEX.ALUout = IDEX.rt >> shamt;
				break;
			case sra:
				printf("sra\n");
				IDEX.ALUout = (int)IDEX.rt >> shamt;
				break;
			case jr:
				printf("jr\n");
				PC = IDEX.rs;
				break;
		}
			
	} else if (IDEX.opcode >= 4 && IDEX.opcode <= 43){
		if(IDEX.opcode == addi || IDEX.opcode == addiu || (IDEX.opcode <= 40 && IDEX.opcode >= 32)){
			EXDM.ALUout = IDEX.rs + IDEX.C;
		} else if (IDEX.opcode == lui){
			EXDM.ALUout = IDEX.C << 16;
		} else if (IDEX.opcode == andi){
			EXDM.ALUout = IDEX.rs & IDEX.C;
		} else if (IDEX.opcode == ori){
			EXDM.ALUout = IDEX.rs | IDEX.C;
		} else if (IDEX.opcode == nori){
			EXDM.ALUout = ~(IDEX.rs | IDEX.C);
		} else if (IDEX.opcode == slti){
			EXDM.ALUout = ((int)IDEX.rs < (int)IDEX.C);
		} else if (IDEX.opcode == beq){
			//EXDM.ALUout = IDEX.rs & IDEX.C;
		} else if (IDEX.opcode == bne){
			//EXDM.ALUout = IDEX.rs & IDEX.C;
		} else if (IDEX.opcode == bgtz){
			//EXDM.ALUout = IDEX.rs & IDEX.C;
		}
	} else if (IDEX.opcode < 4){
		//branch
	} else {
	 // halt
	}



}

int main(){
	i_file = fopen("iimage.bin", "rb");
	d_file = fopen("dimage.bin", "rb");
	error = fopen("error_dump.rpt", "w");
	snap = fopen("snapshot.rpt", "w");

	if (i_file == NULL || d_file == NULL) {fputs ("File error",stderr); exit (1);}

	// obtain file size:
	fseek (i_file , 0 , SEEK_END);
	fseek (d_file , 0 , SEEK_END);
	i_size = ftell(i_file);
	d_size = ftell(d_file);
	rewind(i_file);
	rewind(d_file);

	// copy the file into the buffer:
	i_result = fread(i_memory, 4, i_size/4, i_file);
	d_result = fread(d_data  , 4, d_size/4, d_file);

	read_d_memory(d_size/4); 
	read_i_memory(i_size/4);
	
	// terminate
	fclose(i_file);
	fclose(d_file);
	fclose(error);
	free(i_buffer);
	free(d_buffer);
	return 0;
}
