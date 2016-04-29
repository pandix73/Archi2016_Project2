#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	unsigned regrs;
	unsigned rt;
	unsigned regrt;
	unsigned rd;
	unsigned C; //shamt, immediate, address
	unsigned funct;
	unsigned isNOP;
}IDtoEX;

typedef struct _EXtoDM{
	unsigned opcode;
	unsigned ALUout;
	unsigned rd;
	unsigned rt;
	unsigned regrt;
	unsigned funct;
	unsigned isNOP;
}EXtoDM;

typedef struct _DMtoWB{
	unsigned opcode;
	unsigned MDR;
	unsigned ALUout;
	unsigned rd;
	unsigned rt;
	unsigned funct;
	unsigned isNOP;
}DMtoWB;

IFtoID IFID;
IDtoEX IDEX;
EXtoDM EXDM;
DMtoWB DMWB;
DMtoWB prev;

char* ins[65];
char* rIns[45];

void setInstructions(){
	// R
	rIns[add] = "ADD";
	rIns[addu] = "ADDU";
	rIns[sub] = "SUB";
	rIns[and] = "AND";
	rIns[or] = "OR";
	rIns[xor] = "XOR";
	rIns[nor] = "NOR";
	rIns[nand] = "NAND";
	rIns[slt] = "SLT";
	rIns[sll] = "SLL";
	rIns[srl] = "SRL";
	rIns[sra] = "SRA";
	rIns[jr] = "JR";
	// I
	ins[addi] = "ADDI";
	ins[addiu] = "ADDIU";
	ins[lw] = "LW";
	ins[lh] = "LH";
	ins[lhu] = "LHU";
	ins[lb] = "LB";
	ins[lbu] = "LBU";
	ins[sw] = "SW";
	ins[sh] = "SH";
	ins[sb] = "SB";
	ins[lui] = "LUI";
	ins[andi] = "ANDI";
	ins[ori] = "ORI";
	ins[nori] = "NORI";
	ins[slti] = "SLTI";
	ins[beq] = "BEQ";
	ins[bne] = "BNE";
	ins[bgtz] = "BGTZ";
	// J
	ins[j] = "J";
	ins[jal] = "JAL";
	// S
	ins[halt] = "HALT";
}



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
	int i;
	for(i = 0; i < 2; i++){
		//change 12 34 56 78  to  78 56 34 12
		i_memory[i] = i_memory[i] << 24 | i_memory[i] >> 8 << 24 >> 8 | i_memory[i] >> 16 << 24 >> 16 | i_memory[i] >> 24;
	}
	for(i = 2; i < 2+i_memory[1]; i++){
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
		
		IDEX.regrs = reg[IDEX.rs];
		IDEX.regrt = reg[IDEX.rt];

		IDEX.rd	   = IFID.instruction << 16 >> 27;
		IDEX.C	   = IFID.instruction << 21 >> 27;
	} else if (IDEX.opcode >= 4 && IDEX.opcode <= 43){
		short tempC;
		IDEX.rs	   = IFID.instruction <<  6 >> 27;
		IDEX.rt	   = IFID.instruction << 11 >> 27;
		
		IDEX.regrs = reg[IDEX.rs];
		IDEX.regrt = reg[IDEX.rt];
		
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
	
	if((IFID.instruction & 0xFC1FFFFF) == 0){
		IDEX.isNOP = 1;
	} else {
		IDEX.isNOP = 0;
	}

}

void EX(){	
	if(IDEX.opcode == R){	
		switch(IDEX.funct){
			int s_sign, t_sign;
			case add:
				s_sign = IDEX.regrs >> 31;
				t_sign = IDEX.regrt >> 31;
				EXDM.ALUout = IDEX.regrs + IDEX.regrt;
				printf("add %u = %u + %u\n", EXDM.ALUout, IDEX.regrs, IDEX.regrt);
				//if(s_sign == t_sign && s_sign != EXDM.ALUout >> 31)
					//fprintf(error, "In cycle %d: Number Overflow\n", cycle);
				break;
			case addu:
				printf("addu\n");
				EXDM.ALUout = IDEX.regrs + IDEX.regrt;
				break;
			case sub:
				s_sign = IDEX.regrs >> 31;
				t_sign = (~IDEX.regrt + 1) >> 31;
				EXDM.ALUout = IDEX.regrs + (~IDEX.regrt + 1);
				printf("sub %d = %d - %d\n", EXDM.ALUout, IDEX.regrs, IDEX.regrt);
				//if(s_sign == t_sign && s_sign != EXDM.ALUout >> 31)
					//fprintf(error, "In cycle %d: Number Overflow\n", cycle);
				break;
			case and:
				printf("and\n");
				EXDM.ALUout = IDEX.regrs & IDEX.regrt;
				break;
			case or:
				printf("or\n");
				EXDM.ALUout = IDEX.regrs | IDEX.regrt;
				break;
			case xor:
				printf("xor\n");
				EXDM.ALUout = IDEX.regrs ^ IDEX.regrt;
				break;
			case nor:
				printf("nor\n");
				EXDM.ALUout = ~(IDEX.regrs | IDEX.regrt);
				break;
			case nand:
				printf("nand\n");
				EXDM.ALUout = ~(IDEX.regrs & IDEX.regrt);
				break;
			case slt:
				EXDM.ALUout = ((int)IDEX.regrs < (int)IDEX.regrt);
				printf("slt %d = %d < %d\n", EXDM.ALUout, IDEX.regrs, IDEX.regrt);
				break;
			case sll:
				printf("sll\n");
				EXDM.ALUout = IDEX.regrt << IDEX.C;
				break;
			case srl:
				printf("srl\n");
				EXDM.ALUout = IDEX.regrt >> IDEX.C;
				break;
			case sra:
				printf("sra\n");
				EXDM.ALUout = (int)IDEX.regrt >> IDEX.C;
				break;
			case jr:
				printf("jr\n");
				PC = IDEX.regrs;
				break;
		}
			
	} else if (IDEX.opcode >= 4 && IDEX.opcode <= 43){ // I instruction
		if(IDEX.opcode == addi || IDEX.opcode == addiu || (IDEX.opcode <= 40 && IDEX.opcode >= 32)){
			EXDM.ALUout = IDEX.regrs + IDEX.C;
		} else if (IDEX.opcode == lui){
			EXDM.ALUout = IDEX.C << 16;
		} else if (IDEX.opcode == andi){
			EXDM.ALUout = IDEX.regrs & IDEX.C;
		} else if (IDEX.opcode == ori){
			EXDM.ALUout = IDEX.regrs | IDEX.C;
		} else if (IDEX.opcode == nori){
			EXDM.ALUout = ~(IDEX.regrs | IDEX.C);
		} else if (IDEX.opcode == slti){
			EXDM.ALUout = ((int)IDEX.regrs < (int)IDEX.C);
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
	
	EXDM.opcode = IDEX.opcode;
	EXDM.rd = IDEX.rd;
	EXDM.rt = IDEX.rt;
	EXDM.regrt = IDEX.regrt;
	EXDM.funct = IDEX.funct;

	EXDM.isNOP = IDEX.isNOP;
}

void DM(){
	unsigned addr = EXDM.ALUout;
	if (EXDM.opcode == lw){
		DMWB.MDR = d_memory[addr] << 24 | d_memory[addr+1] << 16 | d_memory[addr+2] << 8 | d_memory[addr+3];	
	} else if (EXDM.opcode == lh){
		DMWB.MDR = (char)d_memory[addr] << 8 | (unsigned char)d_memory[addr+1];
	} else if (EXDM.opcode == lhu){
		DMWB.MDR = (unsigned char)d_memory[addr] << 8 | (unsigned char)d_memory[addr+1];
	} else if (EXDM.opcode == lb){
		DMWB.MDR = (char)d_memory[addr];
	} else if (EXDM.opcode == lbu){
		DMWB.MDR = (unsigned char)d_memory[addr];
	} else if (EXDM.opcode == sw){
		d_memory[addr]   = EXDM.regrt >> 24;
		d_memory[addr+1] = EXDM.regrt << 8  >> 24;
		d_memory[addr+2] = EXDM.regrt << 16 >> 24;
		d_memory[addr+3] = EXDM.regrt << 24 >> 24;
	} else if (EXDM.opcode == sh){
		d_memory[addr]   = EXDM.regrt << 16 >> 24;
		d_memory[addr+1] = EXDM.regrt << 24 >> 24;
	} else if (EXDM.opcode == sb){
		d_memory[addr]   = EXDM.regrt << 24 >> 24;
	} else {
		DMWB.ALUout = EXDM.ALUout;
	}
	
	DMWB.opcode = EXDM.opcode;
	DMWB.funct = EXDM.funct;
	DMWB.rd = EXDM.rd;
	DMWB.rt = EXDM.rt;

	DMWB.isNOP = EXDM.isNOP;
}

void WB(){
	prev.opcode = DMWB.opcode;
	prev.funct  = DMWB.funct;
	prev.isNOP = DMWB.isNOP;

	if(DMWB.opcode == R){
		reg[DMWB.rd] = DMWB.ALUout;
	} else if (DMWB.opcode >= 32 && DMWB.opcode <= 37){
		reg[DMWB.rt] = DMWB.MDR;
	} else if (DMWB.opcode >= 8  && DMWB.opcode <= 15){
		reg[DMWB.rt] = DMWB.ALUout;
	}
}

void print(int PC, int cycle){
	int reg_n;
	fprintf(snap, "cycle %d\n", cycle);
	for(reg_n = 0; reg_n < 32; reg_n++){
		fprintf(snap, "$%02d: 0x%08X\n", reg_n, reg[reg_n]);
	}
	fprintf(snap, "PC: 0x%08X\n", PC);
	fprintf(snap, "IF: 0x%08X\n", IFID.instruction);
	fprintf(snap, "ID: %s\n", (IDEX.isNOP == 1) ? "NOP" : (IDEX.opcode == R) ? rIns[IDEX.funct] : ins[IDEX.opcode]);
	fprintf(snap, "EX: %s\n", (EXDM.isNOP == 1) ? "NOP" : (EXDM.opcode == R) ? rIns[EXDM.funct] : ins[EXDM.opcode]);
	fprintf(snap, "DM: %s\n", (DMWB.isNOP == 1) ? "NOP" : (DMWB.opcode == R) ? rIns[DMWB.funct] : ins[DMWB.opcode]);
	fprintf(snap, "WB: %s\n", (prev.isNOP == 1) ? "NOP" : (prev.opcode == R) ? rIns[prev.funct] : ins[prev.opcode]);
	fprintf(snap, "\n\n");
}

void initialize(){
	memset(&IFID, 0, sizeof(IFID));
	memset(&IDEX, 0, sizeof(IDEX));
	memset(&EXDM, 0, sizeof(EXDM));
	memset(&DMWB, 0, sizeof(DMWB));
	memset(&prev, 0, sizeof(prev));
	IDEX.isNOP = 1;
	EXDM.isNOP = 1;
	DMWB.isNOP = 1;
	prev.isNOP = 1;
}

void run_pipeline(){

	int PC = i_memory[0];
	int cycle = 0;

	while((PC - i_memory[0])/4 < i_memory[1]){

		WB();
		DM();
		EX();
		ID();
		IF(PC);

		print(PC, cycle++);
		PC = IFID.PC;
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
	initialize();
	setInstructions();
	run_pipeline();

	// terminate
	fclose(i_file);
	fclose(d_file);
	fclose(error);
	free(i_buffer);
	free(d_buffer);
	return 0;
}
