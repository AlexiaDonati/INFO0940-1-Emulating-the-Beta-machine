.include beta.uasm  |; Include beta.uasm file for macro definition

main:  
    PUSH(R0)
    BNE(R31, main, R0)   |; Store the current value of PC in R0
    SUBC(R0, 3*4, R0)    |; Store the start adress of the interrupt handler in R0
    SUBC(R0, 400, R0)  |; Store the start adress of the kernel memory in R0
    
    |; Interrupt nb
    PUSH(R1)
    LD(R0, 13, R1)
    ANDC(R1, 0xFF, R1)
    XORC(R1, 0x1, R1)

    |; Char
    PUSH(R2)
    LD(R0, 13 +1, R2)
    ANDC(R2, 0xFF, R2)

    ADDC(R0, 13 +1 +1 +1, R0) |; Store the start address of the buffer in R0

    BEQ(R1, exit)    

key_pressed:

    |; Buf index
    PUSH(R3)
    LD(R0, -1, R3) 
    ANDC(R3, 0xFF, R3)
    
    |; Store the char in the buffer
    ADD(R0, R3, R0)
    ST(R2, 0, R0)              
    SUB(R0, R3, R0)

    |; Increment the buf index
    SHLC(R1, 24, R1)

    LD(R0, -4, R3)
    ADD(R3, R1, R3)
    ST(R3, -4, R0)
    
    SHRC(R1, 24, R1)

    POP(R3)

exit:
    ADDC(R0, 256, R0) |; Store the start address of the pressed array in R1

    |; Update the pressed array
    ADD(R0, R2, R0)
    ST(R1, 0, R0)
    
    POP(R2)
    POP(R1)
    POP(R0)

    JMP(XP, R31) |; Return from interrupt  