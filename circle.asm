.include beta.uasm  |; Include beta.uasm file for macro definition

CMOVE(stack, SP)    |; Initialize stack pointer (SP) 
MOVE(SP, BP)        |; Initialize base of frame pointer (BP)
BR(main)

video_memory_start:
    LONG(33554432)

radius:
    LONG(100)
   
centerx:
    LONG(300)

centery:
    LONG(200)
    
orange:
    LONG(0x0080FF)

sqrt:
    PUSH(LP)
    PUSH(BP)
    MOVE(SP, BP)
    PUSH(R1)          |; Save R1
    PUSH(R2)          |; Save R2
    LD(BP, -12, R1)   |; R1 <- x
    MOVE(R31, R2)     |; R2 <- l = 0

sqrt_loop:
    ADDC(R2, 1, R0)   |; R0 <- l + 1
    MUL(R0, R0, R0)   |; R0 <- (l + 1) * (l + 1)
    CMPLE(R0, R1, R0) |; R0 <- R0 <= R1
    BF(R0, sqrt_end)  |; if false goto sqrt_end
    ADDC(R2, 1, R2)   |; R2 <- l + 1
    BR(sqrt_loop)

sqrt_end:
    MOVE(R2, R0)
    POP(R2)
    POP(R1)
    POP(BP)
    POP(LP)
    RTN()    

distance:
    PUSH(LP)
    PUSH(BP)
    MOVE(SP, BP)
    PUSH(R1)         
    PUSH(R2)         
    PUSH(R3)         
    PUSH(R4)         
    LD(BP, -12, R1)  |; R1 <- i
    LD(BP, -16, R2)  |; R2 <- j
    LD(BP, -20, R3)  |; R3 <- centerX
    LD(BP, -24, R4)  |; R4 <- centerY
    SUB(R1, R3, R1)  |; R1 <- i - centerX
    SUB(R2, R4, R2)  |; R2 <- j - centerY
    MUL(R1, R1, R1)  |; R1 <- R1 * R1
    MUL(R2, R2, R2)  |; R2 <- R2 * R2
    ADD(R1, R2, R0)  |; R0 <- R1 + R2
    PUSH(R0)
    CALL(sqrt)       |; R0 <- sqrt(R0)
    DEALLOCATE(1)
    POP(R4)
    POP(R3)
    POP(R2)
    POP(R1)
    POP(BP)          |; Restore BP
    POP(LP)          |; Restore return address
    RTN()            |; Return

main:
    LD(R31, video_memory_start, R1) |; R1 <- video_memory_start
    ADDC(R1, 600*4*10, R1)          |; R1 += offset
    ADDC(R1, 600*4*10, R1)          |; R1 += offset
    ADDC(R1, 600*4*10, R1)          |; R1 += offset
    ADDC(R1, 600*4*10, R1)          |; R1 += offset
    ADDC(R1, 600*4*10, R1)          |; R1 += offset
    ADDC(R1, 600*4*10, R1)          |; R1 += offset
    ADDC(R1, 600*4*10, R1)          |; R1 += offset
    ADDC(R1, 600*4*10, R1)          |; R1 += offset
    ADDC(R1, 600*4*10, R1)          |; R1 += offset
    ADDC(R1, 600*4*10, R1)          |; R1 += offset
    ADDC(R1, 600*4*10, R1)          |; R1 += offset
    ADDC(R1, 200*4, R1)             |; R1 += offset
    LD(R31, radius, R2)             |; R2 <- radius
    LD(R31, centerx, R3)            |; R3 <- centerx
    LD(R31, centery, R4)            |; R4 <- centery
    LD(R31, orange, R5)             |; R5 <- orange
    MOVE(R3, R6)                    |; R6 <- i
    SUB(R6, R2, R6)                 |; i = centerx-radius

loop1:
    MOVE(R4, R7)                    |; R7 <- centery
    SUB(R7, R2, R7)                 |; j = centery - radius
    BR(loop2)                       |; goto loop2

loop1_inc:
    ADDC(R1, 400*4, R1)
    ADDC(R6, 1, R6)                 |; i++
    ADD(R2, R3, R0)                 |; R0 <- radius + centerx
    CMPLT(R6, R0, R0)               |; R0 <- i < radius + centerx
    BT(R0, loop1)                   |; if true goto loop

loop1_end:
    BR(end)

loop2:
    PUSH(R4) PUSH(R3) PUSH(R7) PUSH(R6)
    CALL(distance)
    DEALLOCATE(4)
    MOVE(R0, R8)
    CMPLE(R0, R2, R0)
    BF(R0, loop2_inc)
    |; compute color
    ADDC(R31, 255, R5) |; RR

    MULC(R8, -127, R0)     |; GG
    DIV(R0, R2, R0)        |; GG
    ADDC(R0, 255, R0)      |; GG
    ANDC(R0, 255, R0)      |; GG
    SHLC(R0, 8, R0)        |; GG
    ADD(R5, R0, R5)        |; GG 

    MULC(R8, -255, R0)     |; BB
    DIV(R0, R2, R0)        |; BB
    ADDC(R0, 255, R0)      |; BB
    ANDC(R0, 255, R0)      |; BB
    SHLC(R0, 16, R0)       |; BB
    ADD(R5, R0, R5)        |; BB

    ST(R5, 0, R1)                   |; Store R5 in R1

loop2_inc:
    ADDC(R1, 4, R1)                 |; Add 4 to R1
    ADDC(R7, 1, R7)                 |; j++
    ADD(R2, R4, R0)                 |; R0 <- radius + centery
    CMPLT(R7, R0, R0)               |; R0 <- j < height
    BT(R0, loop2)                   |; if j < height goto loop2
    
loop2_end:
    BR(loop1_inc)

stack: 
    STORAGE(1024)

end:
    HALT()
    
|; End of file
