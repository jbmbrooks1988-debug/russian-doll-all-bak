# Memory access program for 2-bit RISC-V processor
# Demonstrates load and store operations

# Store value from x1 to memory address 0
sw x1, 0(x0)

# Store value from x2 to memory address 1
sw x2, 1(x0)

# Store value from x3 to memory address 2
sw x3, 2(x0)

# Load value from memory address 0 to x1
lw x1, 0(x0)

# Load value from memory address 1 to x2
lw x2, 1(x0)

# Load value from memory address 2 to x3
lw x3, 2(x0)

# Perform operations using loaded values
add x1, x2, x3

# Store the result to memory address 3
sw x1, 3(x0)

# Load the result back to x1
lw x1, 3(x0)

# End program (halt)
loop:
    j loop