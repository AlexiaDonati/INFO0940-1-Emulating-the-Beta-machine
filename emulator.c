#include "emulator.h"



void init_computer(Computer* c, long program_memory_size, 
                                long video_memory_size, long kernel_memory_size){}


int get_word(Computer* c, long addr){return 0;}

int get_register(Computer* c, int reg){return 0;}

void free_computer(Computer* c){}

void load(Computer* c, FILE* binary){}

void load_interrupt_handler(Computer* c, FILE* binary){}

void execute_step(Computer* c){}

void raise_interrupt(Computer* c, char type, char keyval){}

int disassemble(int instruction, char* buf){return 0;}

