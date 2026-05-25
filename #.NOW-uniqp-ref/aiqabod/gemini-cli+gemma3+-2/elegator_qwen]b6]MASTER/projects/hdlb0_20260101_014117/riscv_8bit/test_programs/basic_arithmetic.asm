# Test Program 1: Basic Arithmetic Operations
# Tests ADD, SUB, AND, OR, XOR operations

# Initialize registers
mov r1, #10      # Load value 10 into r1
mov r2, #5       # Load value 5 into r2

# Test ADD operation
add r3, r1, r2   # r3 = r1 + r2 = 15
nop

# Test SUB operation  
sub r4, r1, r2   # r4 = r1 - r2 = 5
nop

# Test AND operation
and r5, r1, r2   # r5 = r1 AND r2
nop

# Test OR operation
or r6, r1, r2    # r6 = r1 OR r2
nop

# Test XOR operation
xor r7, r1, r2   # r7 = r1 XOR r2
nop

# End of program
loop:
    jmp loop      # Infinite loop to stop