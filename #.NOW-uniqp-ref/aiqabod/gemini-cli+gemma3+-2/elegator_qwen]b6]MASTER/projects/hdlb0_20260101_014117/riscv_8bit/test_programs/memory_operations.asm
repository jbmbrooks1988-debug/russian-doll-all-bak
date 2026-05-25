# Test Program 2: Memory Operations
# Tests load and store operations

# Initialize registers
mov r1, #100     # Address for memory storage
mov r2, #42      # Value to store

# Store value to memory
sw r2, 0(r1)     # Store r2 at address r1+0
nop

# Load value from memory
lw r3, 0(r1)     # Load from address r1+0 to r3
nop

# Store another value at different offset
mov r4, #200     # Another address
mov r5, #84      # Another value
sw r5, 0(r4)     # Store r5 at address r4+0
nop

# Load from the new address
lw r6, 0(r4)     # Load from address r4+0 to r6
nop

# End of program
loop:
    jmp loop      # Infinite loop to stop