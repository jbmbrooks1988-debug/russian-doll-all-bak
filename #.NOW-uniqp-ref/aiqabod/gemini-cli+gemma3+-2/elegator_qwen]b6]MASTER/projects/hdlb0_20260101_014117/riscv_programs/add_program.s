# Simple addition program for 2-bit RISC-V processor
# This program adds values in R0 and R1, stores result in R2

# Initialize R0 with value 1
addi r0, r0, 1    # Instruction: 10 00 (0x8) - ADDI R0, R0, 1
                  # In 4-bit format: 1000 (0x8)

# Initialize R1 with value 2 (using ADDI to R1)
# Since our ADD instruction always uses R0 and R1 as sources, 
# we need to pre-load values into registers

# Actually, our instruction format is limited. Let me implement using the actual instruction format:
# ADD R2, R0, R1  -> opcode 00, Rd = 10 (R2) -> 0010 (0x2)

# Load immediate value 1 into R0
# This would be ADDI R0, R0, 1 -> opcode 10, Rd = 00 -> 1000 (0x8)
# But this would add 1 to current value of R0

# Actually, our implementation is more limited. Let me create a program based on the actual implementation:
# Instruction format: [3:2] opcode | [1:0] operands/data
# ADD R2, R0, R1 -> opcode 00, destination R2 (10) -> 0010 (0x2)
# ADDI R2, R0, 1 -> opcode 10, destination R2 (10) -> 1010 (0xA)

# Program: Add 1 + 2 = 3
# Step 1: Load 1 into R0 (ADDI R0, R0, 1) -> 1000 (0x8)
# Step 2: Load 2 into R1 (ADDI R1, R0, 2) -> 1001 (0x9) - Wait, this doesn't work as intended
# Actually, in our implementation, ADDI R[rd], R0, immediate
# So ADDI R1, R0, 1 would be opcode 10, Rd=01 -> 1001 (0x9)
# This would add immediate 1 to R0's value and store in R1

# Let's create a program that adds 1 + 1 = 2
# Step 1: Load 1 into R0 (ADDI R0, R0, 1) -> 1000 (0x8)
# Step 2: Load 1 into R1 (ADDI R1, R0, 1) -> 1001 (0x9) 
# Step 3: Add R0 and R1, store in R2 (ADD R2) -> 0010 (0x2)

# Instruction memory content (4 instructions max):
# Address 0: ADDI R0, R0, 1  -> 1000 (0x8)
# Address 1: ADDI R1, R0, 1  -> 1001 (0x9) 
# Address 2: ADD R2          -> 0010 (0x2)
# Address 3: NOP (or another operation) -> 0000 (0x0)

# Note: In our implementation, ADD always uses R0 and R1 as sources and Rd as destination
# where Rd is specified in the lower 2 bits of the instruction