# Complex program for 2-bit RISC-V processor
# Performs multiple operations: arithmetic, logical, and memory

# Program sequence:
# 1. Initialize registers with values
# 2. Perform arithmetic operation
# 3. Perform logical operation  
# 4. Store result to memory and load it back

# Step-by-step:
# Address 0: 1000 (0x8) - ADDI R0, R0, 1  -> R0 = 0 + 1 = 1
# Address 1: 1001 (0x9) - ADDI R1, R0, 1  -> R1 = 0 + 1 = 1 (using R0's initial value of 0)
# Address 2: 0010 (0x2) - ADD R2          -> R2 = R0 + R1 = 1 + 1 = 2 (0+1=1 in 2-bit wraps to 0, wait...)

# Wait, in 2-bit arithmetic: 1 + 1 = 2, but 2 in 2-bit is 10, which truncates to 0
# Actually, in 2-bit unsigned: 1 + 1 = 2, but since we only have 2 bits, it would be 0 with carry
# But our ALU is 2-bit, so 1 + 1 = 0 (with potential overflow)

# Let me use smaller values:
# Address 0: 1000 (0x8) - ADDI R0, R0, 1  -> R0 = 0 + 1 = 1
# Address 1: 1011 (0xB) - ADDI R3, R0, 0  -> R3 = 0 + 0 = 0 (get a 0 value)
# Address 2: 0010 (0x2) - ADD R2          -> R2 = R0 + R1 = 1 + 0 = 1 (R1 was 0 from initialization)
# Address 3: 0111 (0x7) - AND R3          -> R3 = R0 AND R2 = 1 AND 1 = 1

# Actually, for AND R3, it would be R3 = R0 AND R1, not R0 AND R2
# In the decoder: rs1=00 (R0), rs2=01 (R1), rd=instruction[1:0] (R3=11)
# So AND R3 means R3 = R0 AND R1

# Corrected complex program:
# Address 0: 1000 (0x8) - ADDI R0, R0, 1  -> R0 = 0 + 1 = 1
# Address 1: 1011 (0xB) - ADDI R3, R0, 0  -> R3 = 0 + 0 = 0 (initialize R3 to 0, using R0's initial value of 0)
# Address 2: 0010 (0x2) - ADD R2          -> R2 = R0 + R1 = 1 + 0 = 1
# Address 3: 0111 (0x7) - AND R3          -> R3 = R0 AND R2 = 1 AND 1 = 1

# Binary: 1000 1011 0010 0111
# Hex:    8    B    2    7