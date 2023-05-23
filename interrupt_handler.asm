.include beta.uasm  |; Include beta.uasm file for macro definition

BR(main)            |; Go to 'main' code segment

main:
    JMP(XP,R31)     |; Return from interrupt