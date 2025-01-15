.section .text.sp

# Assembly function to get the value of the stack pointer
.global getsp
.type getsp, @function

getsp:
    add a0, sp, zero
    ret

# A contrived function to simulate massive stack growth. This is a lot easier than writing code to 
# grow the stack to a certain point hoping it will overwrite certain memory regions. However, 
# nested calls in your program have the exact same effect which is to increase the space used by 
# the stack. This just does it in a single call and can be controlled from your calling C code. It 
# expands the stack, writes a value to the stack as if it were a local or other function argument 
# and then restores the stack before returning. 

# Usage stackbuster(int size)
# Expands the stack by "size" bytes and stores a value there just like a function call would.
# "size" must be a multiple of 16
# Restores the stack and returns.

.global stackbuster
.type stackbuster, @function

stackbuster:
    neg t1, a0          # Store the negative of a0 in t1. t1 = -size
    add sp, sp, t1      # Subtract "size" bytes from the stack pointer

    li t6, 0x4058ff5c   # Put random value into t6 register
    sw t6, 4(sp)        # Save t6 register to stack. Will overwrite whatever is there

    add sp, sp, a0      # Restore the stack pointer
    ret                 # Return