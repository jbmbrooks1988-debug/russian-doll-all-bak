# Logic operation program for 2-bit RISC-V processor
# Performs AND operation on registers x2 and x3, stores result in x1

# For 2-bit processor, values are limited to 0, 1, 2, 3

# AND operation: x1 = x2 AND x3
and x1, x2, x3

# Store result to memory at address 1
sw x1, 1(x0)

# Perform OR operation: x1 = x2 OR x3
or x1, x2, x3

# Store result to memory at address 2
sw x1, 2(x0)

# Perform XOR operation: x1 = x2 XOR x3
xor x1, x2, x3

# Store result to memory at address 3
sw x1, 3(x0)

# Load AND result back from memory to x2
lw x2, 1(x0)

# Load OR result back from memory to x3
lw x3, 2(x0)

# End program (halt)
loop:
    j loop