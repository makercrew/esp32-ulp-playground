.section .text.sp

# Assembly function to get the value of the stack pointer
.global getsp
.type getsp, @function

getsp:
    add a0, sp, zero
    ret