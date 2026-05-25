# Logical operations program for 2-bit RISC-V processor
# Demonstrates AND operation

# Program to compute 1 AND 1 = 1:
# Address 0: 1000 (0x8) - ADDI R0, R0, 1  -> R0 = 0 + 1 = 1 (initialize R0 to 1)
# Address 1: 1001 (0x9) - ADDI R1, R0, 1  -> R1 = 0 + 1 = 1 (initialize R1 to 1, using R0's initial value)
# Address 2: 0110 (0x6) - AND R2          -> R2 = R0 AND R1 = 1 AND 1 = 1
# Address 3: 0000 (0x0) - NOP

# Binary: 1000 1001 0110 0000
# Hex:    8    9    6    0

# Another example - compute 1 AND 0 = 0:
# Address 0: 1000 (0x8) - ADDI R0, R0, 1  -> R0 = 0 + 1 = 1 (initialize R0 to 1)
# Address 1: 1000 (0x8) - ADDI R0, R0, 0  -> R0 = 1 + 0 = 1 (R0 remains 1, wait no...)
# Actually, this would be ADDI R0, R0, 0 -> 1000, which sets R0 = R0(current) + 0 = R0(current)
# So this doesn't change R0.

# To get 0 in a register, since R0 is initially 0, I can use:
# Address 0: 1001 (0x9) - ADDI R1, R0, 0  -> R1 = R0(current) + 0 = 0 + 0 = 0
# Address 1: 1000 (0x8) - ADDI R0, R0, 1  -> R0 = 0 + 1 = 1
# Address 2: 0110 (0x6) - AND R2          -> R2 = R0 AND R1 = 1 AND 0 = 0
# Address 3: 0000 (0x0) - NOP

# Actually, looking again: ADDI R1, R0, 0 means R1 = R0(current) + 0
# If R0 starts at 0, then R1 = 0 + 0 = 0
# So:
# Address 0: 1001 (0x9) - ADDI R1, R0, 0  -> R1 = 0 + 0 = 0
# Address 1: 1000 (0x8) - ADDI R0, R0, 1  -> R0 = 0 + 1 = 1
# Address 2: 0110 (0x6) - AND R2          -> R2 = R0 AND R1 = 1 AND 0 = 0
# Address 3: 0000 (0x0) - NOP

# Binary: 1001 1000 0110 0000
# Hex:    9    8    6    0