# Simple addition program for 2-bit RISC-V processor
# Adds contents of registers x2 and x3, stores result in x1

# Initialize registers with values
# For 2-bit processor, values are limited to 0, 1, 2, 3

# Load immediate value 2 into register x2
# Since we don't have immediate load instructions in our 2-bit implementation,
# we'll simulate by setting x2 to 2 using an operation
# For this example, we'll assume x2 and x3 are pre-loaded with values

# Add contents of x2 and x3, store in x1
add x1, x2, x3

# Store result to memory at address 0
sw x1, 0(x0)

# Load result back from memory to x2
lw x2, 0(x0)

# End program (halt)
# In our implementation, we'll just have a loop to stop execution
loop:
    j loop