# Test Program 3: Branch Operations
# Tests conditional branching

# Initialize registers
mov r1, #10      # Value 1
mov r2, #5       # Value 2

# Compare and branch if equal
beq r1, r2, target1   # Branch to target1 if r1 == r2
nop

# This code runs if NOT equal
mov r3, #1       # Set r3 to 1 (not equal path)
jmp end

target1:
# This code runs if equal
mov r3, #0       # Set r3 to 0 (equal path)

end:
# Test branch if less than
slt r4, r2, r1   # Set r4 to 1 if r2 < r1, else 0
nop

# End of program
loop:
    jmp loop      # Infinite loop to stop