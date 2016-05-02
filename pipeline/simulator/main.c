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
int cycle;
int halterror;
int branchPC;

typedef struct _forward{ //forwarding
	int happen;
	int rs;
	int rt;
}forward;

typedef struct _IFtoID{
	unsigned instruction;
	unsigned PC;
	unsigned stall;
	unsigned flush;
}IFtoID;

typedef struct _IDtoEX{
	unsigned PC;
	unsigned opcode;
	unsigned rs;
	unsigned regrs;
	unsigned rt;
	unsigned regrt;
	unsigned rd;
	unsigned C; //shamt, immediate, address
	unsigned funct;
	unsigned isNOP;
	unsigned stall;
	forward fwd;
	unsigned flush;
}IDtoEX;

typedef struct _EXtoDM{
	unsigned PC;
	unsigned opcode;
	unsigned ALUout;
	unsigned rd;
	unsigned rt;
	unsigned regrt;
	unsigned funct;
	unsigned isNOP;
	unsigned predict;
	forward fwd;
}EXtoDM;

typedef struct _DMtoWB{
	unsigned PC;
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

void setInstructions(){ // fast transfer
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

void IF(){ // seems to be ok
	IFID.instruction = i_memory[(PC-i_memory[0])/4 + 2];
	IFID.PC = PC;
	IFID.flush = IDEX.flush;
	PC = (IDEX.stall) ? PC : (IDEX.flush) ? branchPC : PC + 4; // decide next PC
}

void ID(){
	if(IDEX.stall){ // only need to update reg
		IDEX.regrs = reg[IDEX.rs];
		IDEX.regrt = reg[IDEX.rt];
	} else if (IFID.flush){ // flush
		memset(&IDEX, 0, sizeof(IDEX));
		IDEX.isNOP = 1;
	} else { // normal
		IDEX.PC = IFID.PC;
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
			if(IDEX.opcode == andi || IDEX.opcode == ori || IDEX.opcode == nori){
				IDEX.C = (unsigned short)tempC;
			} else { 
				IDEX.C = tempC;
			}
		} else if (IDEX.opcode == j || IDEX.opcode == jal){
			IDEX.C	   = IFID.instruction <<  6 >>  6;
		} else {
			// halt
		}
		IDEX.isNOP = ((IFID.instruction & 0xFC1FFFFF) == 0 ); // check NOP
	}
	
	// stall and forwarding     
	// may changed reg : rd -> R, rt -> I, $31 -> jal
	// may forwarding stage : EXDM(now in DMWB) -> ID, EXDM(now in EXDM) -> EX(now in here)
	// may forwarding instruction: R type, I type except load, jal
	IDEX.fwd.happen = 0;
	int rtInEXDM = (((EXDM.opcode == R && EXDM.funct != 8 && IDEX.rt == EXDM.rd) || (EXDM.opcode >= 8 && EXDM.opcode <= 37 && EXDM.rt == IDEX.rt) || (EXDM.opcode == jal && IDEX.rt == 31)) && (IDEX.rt != 0));
	int rtInDMWB = (((DMWB.opcode == R && DMWB.funct != 8 && IDEX.rt == DMWB.rd) || (DMWB.opcode >= 8 && DMWB.opcode <= 37 && DMWB.rt == IDEX.rt) || (DMWB.opcode == jal && IDEX.rt == 31)) && (IDEX.rt != 0));
	int rsInEXDM = (((EXDM.opcode == R && EXDM.funct != 8 && IDEX.rs == EXDM.rd) || (EXDM.opcode >= 8 && EXDM.opcode <= 37 && EXDM.rt == IDEX.rs) || (EXDM.opcode == jal && IDEX.rt == 31)) && (IDEX.rs != 0));
	int rsInDMWB = (((DMWB.opcode == R && DMWB.funct != 8 && IDEX.rs == DMWB.rd) || (DMWB.opcode >= 8 && DMWB.opcode <= 37 && DMWB.rt == IDEX.rs) || (DMWB.opcode == jal && IDEX.rs == 31)) && (IDEX.rs != 0));
	int EXDMforwarding = ((EXDM.opcode == R && EXDM.funct != 8) || (EXDM.opcode >= 8 && EXDM.opcode <= 15) || (EXDM.opcode >= 40 && EXDM.opcode <= 43) || (EXDM.opcode == jal));
	int DMWBforwarding = ((DMWB.opcode == R && DMWB.funct != 8) || (DMWB.opcode >= 8 && DMWB.opcode <= 15) || (DMWB.opcode >= 40 && DMWB.opcode <= 43) || (DMWB.opcode == jal));

	if(IDEX.opcode == R && (IDEX.funct == sll || IDEX.funct == srl || IDEX.funct == sra)){ // use only rt
		if(rtInEXDM){
			if(EXDMforwarding){
				EXDM.predict = 2; // predict rt forwarding
				IDEX.stall = 0; 
			} else { 
				IDEX.stall = 1;
			}
		} else if (rtInDMWB){
			IDEX.stall = 1;
		} else {
			IDEX.stall = 0;
		}
	} else if((IDEX.opcode >= 7 && IDEX.opcode <= 37 && IDEX.opcode != lui) || (IDEX.opcode == R && IDEX.funct == jr)){ // use only rs
		if(rsInEXDM){
			if(EXDMforwarding && IDEX.opcode != bgtz && IDEX.opcode != R){
				EXDM.predict = 1; // predict rs forwarding
				IDEX.stall = 0; 
			} else { 
				IDEX.stall = 1;
			}
		} else if (rsInDMWB){
			if(DMWBforwarding && (IDEX.opcode == bgtz || (IDEX.opcode == R && IDEX.funct == jr))){
				IDEX.fwd.happen = 1;
				IDEX.fwd.rs = 1;
				IDEX.fwd.rt = 0;
				IDEX.regrs = DMWB.ALUout;
				IDEX.stall = 0;
			} else {
				IDEX.stall = 1;
			}
		} else{
			IDEX.stall = 0;
		}
	} else if (IDEX.opcode != j && IDEX.opcode != jal && IDEX.opcode != halt){ // use both rs and rt
		if(IDEX.rs == IDEX.rt){
			if(rsInEXDM){
				if(EXDMforwarding && IDEX.opcode != bne && IDEX.opcode != beq){
					EXDM.predict = 1; // predict rs forwarding
					IDEX.stall = 0;
				} else {
					IDEX.stall = 1;
				}
			} else if (rsInDMWB){
				if(DMWBforwarding && (IDEX.opcode == bne || IDEX.opcode == beq)){ // bne beq forwarding
					IDEX.fwd.happen = 1;
					IDEX.fwd.rs = 1;
					IDEX.fwd.rt = 0;
					IDEX.regrs = DMWB.ALUout;
					IDEX.regrt = DMWB.ALUout;
					IDEX.stall = 0;
				} else {
					IDEX.stall = 1;
				}
			} else {
				IDEX.stall = 0;
			}
		} else {
			if((rsInEXDM || rtInEXDM) && (rsInDMWB || rtInDMWB)){ // only stall
				IDEX.stall = 1;
			} else if (rsInEXDM || rtInEXDM){ // both not in DMWB
				if(EXDMforwarding && IDEX.opcode != bne && IDEX.opcode != beq){
					if(rsInEXDM){ // rs forwarding
						EXDM.predict = 1;
					} else { // rt forwarding
						EXDM.predict = 2;
					}
					IDEX.stall = 0;
				} else {
					IDEX.stall = 1;
				}
			} else if (rsInDMWB || rtInDMWB){ // both not in EXDM
				if(DMWBforwarding && (IDEX.opcode == bne || IDEX.opcode == beq)){ // bne beq forwarding
					IDEX.fwd.happen = 1;
					IDEX.fwd.rs = rsInDMWB;
					IDEX.fwd.rt = rtInDMWB;
					if(rsInDMWB) {
						IDEX.regrs = DMWB.ALUout;
					} else {
						IDEX.regrt = DMWB.ALUout;
					}
					IDEX.stall = 0;
				} else {
					IDEX.stall = 1;
				}
			} else {
				IDEX.stall = 0; //no stall
			}
		}
	} else {
		IDEX.stall = 0;
	}
	
	//branch check if no stall

	if(IDEX.stall == 0){
		if(IDEX.opcode == beq){
			branchPC = IFID.PC + 4 + ((IDEX.regrs == IDEX.regrt) ? IDEX.C << 2 : 0);
			IDEX.flush = (IDEX.regrs == IDEX.regrt) ? 1 : 0;
		} else if (IDEX.opcode == bne){
			branchPC = IFID.PC + 4 + ((IDEX.regrs != IDEX.regrt) ? IDEX.C << 2 : 0);
			IDEX.flush = (IDEX.regrs != IDEX.regrt) ? 1 : 0;
		} else if (IDEX.opcode == bgtz){
			branchPC = IFID.PC + 4 + (((int)IDEX.regrs > 0) ? IDEX.C << 2 : 0);
			IDEX.flush = ((int)IDEX.regrs > 0) ? 1 : 0;
		} else if(IDEX.opcode == R && IDEX.funct == jr){
			branchPC = IDEX.regrs;
			IDEX.flush = 1;
		} else if(IDEX.opcode == j){
			branchPC = (IFID.PC + 4) >> 28 << 28 | (unsigned int)IDEX.C << 2;
			IDEX.flush = 1;
		} else if(IDEX.opcode == jal){
			branchPC = (IFID.PC + 4) >> 28 << 28 | (unsigned int)IDEX.C << 2;
			IDEX.flush = 1;
		} else {
			IDEX.flush = 0;
		}
	}
}

void EX(){	
	// check stall and forwarding
	if(IDEX.stall == 1){
		memset(&EXDM, 0, sizeof(EXDM));
		EXDM.isNOP = 1;
		return;
	} else if (EXDM.predict > 0){
		EXDM.fwd.happen = 1;
		EXDM.fwd.rs = (EXDM.predict == 1) ? IDEX.rs : 0;
		EXDM.fwd.rt = (EXDM.predict == 2) ? IDEX.rt : 0;
		if(EXDM.fwd.rs){
			IDEX.regrs = DMWB.ALUout;
			if(IDEX.rs == IDEX.rt)
				IDEX.regrt = DMWB.ALUout;
		} else {
			IDEX.regrt = DMWB.ALUout;
		}
		EXDM.predict = 0;
	} else {
		EXDM.fwd.happen = 0;
	}
	
	// ALU
	if(IDEX.opcode == R){	
		switch(IDEX.funct){
			int s_sign, t_sign;
			case add:
				s_sign = IDEX.regrs >> 31;
				t_sign = IDEX.regrt >> 31;
				EXDM.ALUout = IDEX.regrs + IDEX.regrt;
				if(s_sign == t_sign && s_sign != EXDM.ALUout >> 31)
					fprintf(error, "In cycle %d: Number Overflow\n", cycle+1);
				break;
			case addu:
				EXDM.ALUout = IDEX.regrs + IDEX.regrt;
				break;
			case sub:
				s_sign = IDEX.regrs >> 31;
				t_sign = (~IDEX.regrt + 1) >> 31;
				EXDM.ALUout = IDEX.regrs + (~IDEX.regrt + 1);
				if(s_sign == t_sign && s_sign != EXDM.ALUout >> 31)
					fprintf(error, "In cycle %d: Number Overflow\n", cycle+1);
				break;
			case and:
				EXDM.ALUout = IDEX.regrs & IDEX.regrt;
				break;
			case or:
				EXDM.ALUout = IDEX.regrs | IDEX.regrt;
				break;
			case xor:
				EXDM.ALUout = IDEX.regrs ^ IDEX.regrt;
				break;
			case nor:
				EXDM.ALUout = ~(IDEX.regrs | IDEX.regrt);
				break;
			case nand:
				EXDM.ALUout = ~(IDEX.regrs & IDEX.regrt);
				break;
			case slt:
				EXDM.ALUout = ((int)IDEX.regrs < (int)IDEX.regrt);
				break;
			case sll:
				EXDM.ALUout = IDEX.regrt << IDEX.C;
				break;
			case srl:
				EXDM.ALUout = IDEX.regrt >> IDEX.C;
				break;
			case sra:
				EXDM.ALUout = (int)IDEX.regrt >> IDEX.C;
				break;
			case jr:
				// no calculate
				break;
		}		
	} else if (IDEX.opcode >= 4 && IDEX.opcode <= 43){ // I instruction
		// IDEX.C as unsigned, signedC as signed
		int signedC = (int)IDEX.C << 16 >> 16;
		int addr = (int)IDEX.regrs + (int)signedC;
		// number overflow
		if(IDEX.opcode >= 32 || IDEX.opcode == 8){
			if((IDEX.regrs >> 31) == ((unsigned)signedC >> 31) && (IDEX.regrs >> 31) != ((unsigned)addr >> 31)){
				fprintf(error, "In cycle %d: Number Overflow\n", cycle+1);
			}
		}

		// don't know where should we do save word's rt mask
		if(IDEX.opcode == addi || IDEX.opcode == addiu || (IDEX.opcode <= 43 && IDEX.opcode >= 32)){
			EXDM.ALUout = addr;
		} else if (IDEX.opcode == lui){
			EXDM.ALUout = IDEX.C << 16;
		} else if (IDEX.opcode == andi){
			EXDM.ALUout = IDEX.regrs & IDEX.C;
		} else if (IDEX.opcode == ori){
			EXDM.ALUout = IDEX.regrs | IDEX.C;
		} else if (IDEX.opcode == nori){
			EXDM.ALUout = ~(IDEX.regrs | IDEX.C);
		} else if (IDEX.opcode == slti){
			EXDM.ALUout = ((int)IDEX.regrs < signedC);
		}
	} else if (IDEX.opcode == jal){
		EXDM.ALUout = IDEX.PC + 4;
	} else {
	 // halt
	}
	
	EXDM.opcode = IDEX.opcode;
	EXDM.rd = IDEX.rd;
	EXDM.rt = IDEX.rt;
	EXDM.regrt = IDEX.regrt;
	EXDM.funct = IDEX.funct;
	EXDM.PC = IDEX.PC;
	EXDM.isNOP = IDEX.isNOP;
}

void DM(){

	int addr = EXDM.ALUout;
	
	halterror = 0;
	// address overflow
	if(EXDM.opcode >= 32){
		if( ((EXDM.opcode == lw || EXDM.opcode == sw) && (addr > 1020 || addr < 0)) || 
			((EXDM.opcode == lh || EXDM.opcode == lhu || EXDM.opcode == sh) && (addr > 1022 || addr < 0)) || 
		  	((EXDM.opcode == lb || EXDM.opcode == lbu || EXDM.opcode == sb) && (addr > 1023 || addr < 0))){
			fprintf(error, "In cycle %d: Address Overflow\n", cycle+1);
			halterror = 1;
		}
	}
	// misalignment error
	if(EXDM.opcode >= 32){
		if( ((EXDM.opcode == lw || EXDM.opcode == sw) && (addr % 4) != 0) || 
			((EXDM.opcode == lh || EXDM.opcode == lhu || EXDM.opcode == sh) && (addr % 2) != 0)){
			fprintf(error, "In cycle %d: Misalignment Error\n", cycle+1);
			halterror = 1;	
		}
	}
	
	if(halterror == 1){
		// prepare to halt
	} else if (EXDM.opcode == lw){
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
	}
	
	
	DMWB.ALUout = EXDM.ALUout;
	DMWB.opcode = EXDM.opcode;
	DMWB.funct = EXDM.funct;
	DMWB.rd = EXDM.rd;
	DMWB.rt = EXDM.rt;
	DMWB.PC = EXDM.PC;
	DMWB.isNOP = EXDM.isNOP;
}

void WB(){
	
	if(DMWB.isNOP){
		
	} else if(DMWB.opcode == R && DMWB.funct != jr){
		if(DMWB.rd == 0){
			fprintf(error, "In cycle %d: Write $0 Error\n", cycle+1);
		} else {
			reg[DMWB.rd] = DMWB.ALUout;
		}
	} else if (DMWB.opcode >= 32 && DMWB.opcode <= 37){
		if(DMWB.rt == 0){
			fprintf(error, "In cycle %d: Write $0 Error\n", cycle+1);
		} else {
			reg[DMWB.rt] = DMWB.MDR;
		}
	} else if (DMWB.opcode >= 8  && DMWB.opcode <= 15){
		if(DMWB.rt == 0){
			fprintf(error, "In cycle %d: Write $0 Error\n", cycle+1);
		} else {
			reg[DMWB.rt] = DMWB.ALUout;
		}
	} else if (DMWB.opcode == jal){
		reg[31] = DMWB.PC + 4;
	}

	prev.opcode = DMWB.opcode;
	prev.funct  = DMWB.funct;
	prev.MDR = DMWB.MDR;
	prev.isNOP = DMWB.isNOP;
	prev.rd = DMWB.rd;
	prev.rt = DMWB.rt;
	prev.ALUout = DMWB.ALUout;
}

void printReg(){
	int reg_n;
	fprintf(snap, "cycle %d\n", cycle);
	for(reg_n = 0; reg_n < 32; reg_n++){
		fprintf(snap, "$%02d: 0x%08X\n", reg_n, reg[reg_n]);
	}
}

void print(int PC, int cycle){
	fprintf(snap, "PC: 0x%08X\n", IFID.PC);
	fprintf(snap, "IF: 0x%08X%s\n", IFID.instruction, (IDEX.stall) ? " to_be_stalled" : (IDEX.flush) ? " to_be_flushed" : "");
	
	fprintf(snap, "ID: %s%s", (IDEX.isNOP) ? "NOP" : (IDEX.opcode == R) ? rIns[IDEX.funct] : ins[IDEX.opcode], (IDEX.stall) ? " to_be_stalled" : "");
	if(IDEX.fwd.happen)fprintf(snap, " fwd_EX-DM_%s_$%d", (IDEX.fwd.rs) ? "rs" : "rt", (IDEX.fwd.rs) ? IDEX.rs : IDEX.rt);
	fprintf(snap, "\n");

	fprintf(snap, "EX: %s", (EXDM.isNOP) ? "NOP" : (EXDM.opcode == R) ? rIns[EXDM.funct] : ins[EXDM.opcode]);
	if(EXDM.fwd.happen)fprintf(snap, " fwd_EX-DM_%s_$%d", (EXDM.fwd.rs) ? "rs" : "rt", (EXDM.fwd.rs) ? EXDM.fwd.rs : EXDM.rt);
	fprintf(snap, "\n");

	fprintf(snap, "DM: %s\n", (DMWB.isNOP) ? "NOP" : (DMWB.opcode == R) ? rIns[DMWB.funct] : ins[DMWB.opcode]);
	fprintf(snap, "WB: %s\n", (prev.isNOP) ? "NOP" : (prev.opcode == R) ? rIns[prev.funct] : ins[prev.opcode]);
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

	PC = i_memory[0];
	cycle = 0;
	int doHalt = 0;

	while((PC - i_memory[0])/4 < i_memory[1]){
		if(doHalt == 0) printReg();
		
		WB();
		DM();
		EX();
		ID();
		IF();
		if(cycle >= 100)return;
		if(doHalt == 1) return;
		print(PC, cycle++);
		if(halterror == 1) doHalt = 1;
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
