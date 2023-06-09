#include "emulator.h"
#include <assert.h>
#include <string.h>

void init_computer(Computer* c, long program_memory_size, 
                                long video_memory_size, long kernel_memory_size){
    
    assert(c);

    c->memory_size = program_memory_size + video_memory_size + kernel_memory_size;
    c->cpu.memory = (char *) malloc(c->memory_size * sizeof(char));
    if(c->cpu.memory == NULL){
        exit(-1);
    }

    c->program_memory_size = program_memory_size;
    c->cpu.program_memory = &c->cpu.memory[0];

    c->video_memory_size = video_memory_size;
    c->cpu.video_memory = &c->cpu.memory[program_memory_size];

    c->kernel_memory_size = kernel_memory_size;
    c->cpu.kernel_memory = &c->cpu.memory[program_memory_size + video_memory_size];

    c->program_size = 0; // It's currently empty

    c->latest_accessed = 0;

    c->cpu.interrupt_line = false;

    c->halted = false;
}

static int get_bits(int instruction, int i, int n){
    int mask = (1 << n) - 1;
    return (mask & (instruction >> i));
}

int get_word(Computer* c, long addr){
    if(addr > c->memory_size){
        return 0;
    }

    if(addr+3 < c->memory_size){
        return((unsigned char) (c->cpu.memory[addr]) |
               (unsigned char) (c->cpu.memory[addr + 1]) << 8 |
               (unsigned char) (c->cpu.memory[addr + 2]) << 16 |
               (unsigned char) (c->cpu.memory[addr + 3]) << 24);
    }

    // If addr is found at the boundary of the computer, return only the valid bytes  
    else if(addr+2 < c->memory_size){
        return((unsigned char) (c->cpu.memory[addr]) |
               (unsigned char) (c->cpu.memory[addr + 1]) << 8 |
               (unsigned char) (c->cpu.memory[addr + 2]) << 16);
    }
    else if(addr+1 < c->memory_size){
        return((unsigned char) (c->cpu.memory[addr]) |
               (unsigned char) (c->cpu.memory[addr + 1]) << 8);
    }
    else if(addr < c->memory_size){
        return (unsigned char) (c->cpu.memory[addr]);
    }

    c->latest_accessed = addr;

    return 0;
}

static void store_word(Computer* c, long addr, int word){
    if(addr < c->memory_size){ c->cpu.memory[addr] = (word >> 0) & 0xFF; }
    if(addr+1 < c->memory_size){ c->cpu.memory[addr+1] = (word >> 8) & 0xFF; }
    if(addr+2 < c->memory_size){ c->cpu.memory[addr+2] = (word >> 16) & 0xFF; }
    if(addr+3 < c->memory_size){ c->cpu.memory[addr+3] = (word >> 24) & 0xFF; }

    c->latest_accessed = addr;
}

int get_register(Computer* c, int reg){
    assert(reg >= 0 && reg <= 31); // The register's number is between 0 and 31
    if(reg == 31){
        return 0; // R31 is hardwired to 0 and cannot be modified
    }
    return c->cpu.registers[reg];
}

void free_computer(Computer* c){
    assert(c);
    free(c->cpu.memory);
}

void load(Computer* c, FILE* binary){
    assert(c && binary);

    fseek(binary, 0, SEEK_END);
    c->program_size = ftell(binary);                    
    if(c->program_size > c->program_memory_size){
        printf("There is not enough space in the computer's program memory to store this program.");
        exit(-2);
    }
    rewind(binary); 
    fread(c->cpu.program_memory, c->program_size, 1, binary); // Loads the binary at the beginning of the computer's memory
}

void load_interrupt_handler(Computer* c, FILE* binary){
    assert(c); 

    if(binary == NULL){
        return; // If binary is NULL the function does nothing
    }

    fseek(binary, 0, SEEK_END);
    int handler_size = ftell(binary);                   
    if(handler_size > c->kernel_memory_size - 400){
        printf("There is not enough space in the computer's kernel memory to store this interrupt handler.");
        exit(-2);
    }
    rewind(binary); 

    char *addr = c->cpu.kernel_memory + 400;
    fread(addr, handler_size, 1, binary); // Loads the binary at its place in kernel memory
}

void execute_step(Computer* c){
    assert(c);

    c->halted = false;
    
    bool kernel_mode = c->cpu.program_counter >= c->program_memory_size + c->video_memory_size;

    // If an interrupt line is raised (and the computer is not already executing the interrupt handler),
    if(c->cpu.interrupt_line && !kernel_mode){

        // Before handing control to the interrupt handler,

        // The CPU places the interrupt number and associated character at the adequate place in kernel memory
        c->cpu.kernel_memory[13] = c->cpu.interrupt_nb;
        c->cpu.kernel_memory[13+1] = c->cpu.interrupt_char;

        // The CPU stores PC into XP (30) so that the interrupt handler is able to return.
        c->cpu.registers[30] = c->cpu.program_counter;
                  
        // The program counter becomes the start address of the interrupt handler.
        c->cpu.program_counter = c->program_memory_size + c->video_memory_size + 400;

        c->cpu.interrupt_line = false;
    }

    // Fetch instruction
    int instruction = get_word(c, c->cpu.program_counter);

    // Decode
    int opcode = get_bits(instruction, 26, 6);
    
    int ra = get_register(c, get_bits(instruction, 16, 5));
    int rb = get_register(c, get_bits(instruction, 11, 5));

    int rc_addr = get_bits(instruction, 21, 5);

    int lit = get_bits(instruction, 0, 16);
        lit |= ((lit & 0x8000) ? 0xFFFF0000 : 0); // SEXT(literal)

    // Execute
    c->cpu.program_counter += 4;
    
    switch (opcode){
        case 0x0: 
            if(instruction != 0){ // An instruction with opcode 0 but not equal to 0 is not a valid instruction.
                return;
            }
            c->halted = true;
            break;

        case 0x18: // LD
            if(!kernel_mode && (ra + lit >= c->program_memory_size + c->video_memory_size)){
                return; // Cannot access kernel memory from user program memory
            }

            c->cpu.registers[rc_addr] = get_word(c, ra + lit); 
            break;  

        case 0x19: // ST
            if(!kernel_mode && (ra + lit >= c->program_memory_size + c->video_memory_size)){
                return; // Cannot access kernel memory from user program memory
            }

            int rc = get_register(c, rc_addr);
            store_word(c, ra + lit, rc);
            break; 

        case 0x1B: // JMP
            if(!kernel_mode && (ra & 0xFFFFFFFC >= c->program_memory_size + c->video_memory_size)){
                return; // Cannot access kernel memory from user program memory
            }

            c->cpu.registers[rc_addr] = c->cpu.program_counter; 
            c->cpu.program_counter = ra & 0xFFFFFFFC; 
            break; 

        case 0x1D: // BEQ
            if(!kernel_mode && (c->cpu.program_counter + 4 * lit >= c->program_memory_size + c->video_memory_size)){
                return; // Cannot access kernel memory from user program memory
            }

            c->cpu.registers[rc_addr] = c->cpu.program_counter; 
            if(ra == 0){
                c->cpu.program_counter = c->cpu.program_counter + 4 * lit;
            }
            break;       
        case 0x1E: // BNE
            if(!kernel_mode && (c->cpu.program_counter + 4 * lit >= c->program_memory_size + c->video_memory_size)){
                return; // Cannot access kernel memory from user program memory
            }

            c->cpu.registers[rc_addr] = c->cpu.program_counter; 
            if(ra != 0){
                c->cpu.program_counter = c->cpu.program_counter + 4 * lit;
            }
            break;  

        case 0x1F:
            // LDR is to be interpreted as STR if the address in question is part of kernel memory.
            if((c->cpu.program_counter + 4 * lit) >= c->program_memory_size + c->video_memory_size){
                int rc = get_register(c, rc_addr);
                store_word(c, c->cpu.program_counter + 4 * lit, rc); // STR
            }
            else{
                c->cpu.registers[rc_addr] = get_word(c, c->cpu.program_counter + 4 * lit); // LDR
            }
            break;

        case 0x20: c->cpu.registers[rc_addr] = ra + rb; break; // ADD   
        case 0x21: c->cpu.registers[rc_addr] = ra - rb; break; // SUB   
        case 0x22: c->cpu.registers[rc_addr] = ra * rb; break; // MUL   
        case 0x23: c->cpu.registers[rc_addr] = ra / rb; break; // DIV   

        case 0x24: c->cpu.registers[rc_addr] = (ra == rb); break; // CMPEQ   
        case 0x25: c->cpu.registers[rc_addr] = (ra < rb); break; // CMPLT      
        case 0x26: c->cpu.registers[rc_addr] = (ra <= rb); break; // CMPLE   

        case 0x28: c->cpu.registers[rc_addr] = ra & rb; break; // AND       
        case 0x29: c->cpu.registers[rc_addr] = ra | rb; break; // OR       
        case 0x2A: c->cpu.registers[rc_addr] = ra ^ rb; break; // XOR 

        // Only the 5 least-significant bits of the second operand (representing the shift) are considered.
        case 0x2C: c->cpu.registers[rc_addr] = ra << get_bits(rb, 0, 5); break; // SHL
        case 0x2D: c->cpu.registers[rc_addr] = (int) ((unsigned int) ra >> get_bits(rb, 0, 5)); break; // SHR
        case 0x2E: c->cpu.registers[rc_addr] = ra >> get_bits(rb, 0, 5); break; // SRA

        case 0x30: c->cpu.registers[rc_addr] = ra + lit; break; // ADDC
        case 0x31: c->cpu.registers[rc_addr] = ra - lit; break; // SUBC
        case 0x32: c->cpu.registers[rc_addr] = ra * lit; break; // MULC
        case 0x33: c->cpu.registers[rc_addr] = ra / lit; break; // DIVC

        case 0x34: c->cpu.registers[rc_addr] = (ra == lit); break; // CMPEQC       
        case 0x35: c->cpu.registers[rc_addr] = (ra < lit); break; // CMPLTC       
        case 0x36: c->cpu.registers[rc_addr] = (ra <= lit); break; // CMPLEC

        case 0x38: c->cpu.registers[rc_addr] = ra & lit; break; // ANDC     
        case 0x39: c->cpu.registers[rc_addr] = ra | lit; break; // ORC    
        case 0x3A: c->cpu.registers[rc_addr] = ra ^ lit; break; // XORC    

        case 0x3C: c->cpu.registers[rc_addr] = ra << get_bits(lit, 0, 5); break; // SHLC
        case 0x3D: c->cpu.registers[rc_addr] = (int) ((unsigned int) ra >> get_bits(lit, 0, 5)); break; // SHRC
        case 0x3E: c->cpu.registers[rc_addr] = ra >> get_bits(lit, 0, 5); break; // SRAC

        default: 
            break;
    }
}

void raise_interrupt(Computer* c, char type, char keyval){
    assert(c);

    if(c->cpu.interrupt_line){
        return; // Does nothing if an interrupt line is already raised.
    }
    
    c->cpu.interrupt_nb = type;
    c->cpu.interrupt_char = keyval;
    
    c->cpu.interrupt_line = true;
}

static char *special_reg(int r, char *reg){  
    switch (r){
        case 27: sprintf(reg, "BP"); break;
        case 28: sprintf(reg, "LP"); break;
        case 29: sprintf(reg, "SP"); break;
        case 30: sprintf(reg, "XP"); break;
        default: sprintf(reg, "R%i", r); break;
    }
}

int disassemble(int instruction, char* buf){
    assert(buf); // We assume that buf is large enough to store any disassembled instructions.

    int opcode = get_bits(instruction, 26, 6);

    char ra[4];
    special_reg(get_bits(instruction, 16, 5), ra);

    char rb[4];
    special_reg(get_bits(instruction, 11, 5), rb);

    char rc[4];
    special_reg(get_bits(instruction, 21, 5), rc);

    int lit = get_bits(instruction, 0, 16);
        lit |= ((lit & 0x8000) ? 0xFFFF0000 : 0); // Extends the sign bit

    switch (opcode){
        case 0x0: 
            if(instruction != 0){ // An instruction with opcode 0 but not equal to 0 is not a valid instruction.
                sprintf(buf, "INVALID"); 
                return -1;
            }
            sprintf(buf, "HALT()"); 
            return 0;
            break; 

        case 0x18: sprintf(buf, "LD(%s, %i, %s)", ra, lit, rc); break;       
        case 0x19: sprintf(buf, "ST(%s, %i, %s)", rc, lit, ra); break;     

        case 0x1B: sprintf(buf, "JMP(%s, %s)", ra, rc); break;    

        case 0x1D: sprintf(buf, "BEQ(%s, %i, %s)", ra, lit, rc); break;       
        case 0x1E: sprintf(buf, "BNE(%s, %i, %s)", ra, lit, rc); break;         
        case 0x1F: 
            sprintf(buf, "LDR(%i, %s)", lit, rc); 
            break;

        case 0x20: sprintf(buf, "ADD(%s, %s, %s)", ra, rb, rc); break;
        case 0x21: sprintf(buf, "SUB(%s, %s, %s)", ra, rb, rc); break;
        case 0x22: sprintf(buf, "MUL(%s, %s, %s)", ra, rb, rc); break;
        case 0x23: sprintf(buf, "DIV(%s, %s, %s)", ra, rb, rc); break;

        case 0x24: sprintf(buf, "CMPEQ(%s, %s, %s)", ra, rb, rc); break;
        case 0x25: sprintf(buf, "CMPLT(%s, %s, %s)", ra, rb, rc); break;       
        case 0x26: sprintf(buf, "CMPLE(%s, %s, %s)", ra, rb, rc); break;   

        case 0x28: sprintf(buf, "AND(%s, %s, %s)", ra, rb, rc); break;       
        case 0x29: sprintf(buf, "OR(%s, %s, %s)", ra, rb, rc); break;        
        case 0x2A: sprintf(buf, "XOR(%s, %s, %s)", ra, rb, rc); break;  

        case 0x2C: sprintf(buf, "SHL(%s, %s, %s)", ra, rb, rc); break;
        case 0x2D: sprintf(buf, "SHR(%s, %s, %s)", ra, rb, rc); break;
        case 0x2E: sprintf(buf, "SRA(%s, %s, %s)", ra, rb, rc); break;

        case 0x30: sprintf(buf, "ADDC(%s, %i, %s)", ra, lit, rc); break;
        case 0x31: sprintf(buf, "SUBC(%s, %i, %s)", ra, lit, rc); break;
        case 0x32: sprintf(buf, "MULC(%s, %i, %s)", ra, lit, rc); break;
        case 0x33: sprintf(buf, "DIVC(%s, %i, %s)", ra, lit, rc); break;  

        case 0x34: sprintf(buf, "CMPEQC(%s, %i, %s)", ra, lit, rc); break;        
        case 0x35: sprintf(buf, "CMPLTC(%s, %i, %s)", ra, lit, rc); break;       
        case 0x36: sprintf(buf, "CMPLEC(%s, %i, %s)", ra, lit, rc); break;  

        case 0x38: sprintf(buf, "ANDC(%s, %i, %s)", ra, lit, rc); break;        
        case 0x39: sprintf(buf, "ORC(%s, %i, %s)", ra, lit, rc); break;
        case 0x3A: sprintf(buf, "XORC(%s, %i, %s)", ra, lit, rc); break;

        case 0x3C: sprintf(buf, "SHLC(%s, %i, %s)", ra, lit, rc); break;
        case 0x3D: sprintf(buf, "SHRC(%s, %i, %s)", ra, lit, rc); break;
        case 0x3E: sprintf(buf, "SRAC(%s, %i, %s)", ra, lit, rc); break;

        default: 
            strcpy(buf, "INVALID"); 
            return -1;
            break;
    }

    return 0;
}

