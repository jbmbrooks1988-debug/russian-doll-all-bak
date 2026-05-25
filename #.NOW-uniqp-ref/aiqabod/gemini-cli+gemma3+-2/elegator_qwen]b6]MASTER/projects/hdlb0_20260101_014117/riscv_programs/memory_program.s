# Memory operations program for 2-bit RISC-V processor
# Demonstrates store and load operations

# Program to store a value in memory and then load it back:
# Address 0: 1000 (0x8) - ADDI R0, R0, 1  -> R0 = 0 + 1 = 1 (use R0 as memory address 1)
# Address 1: 1001 (0x9) - ADDI R1, R0, 1  -> R1 = 0 + 1 = 1 (store value 1 in R1)
# Address 2: 1101 (0xD) - SW              -> Store R1 at address R0 -> Memory[1] = R1 = 1
# Address 3: 1100 (0xC) - LW R2           -> Load from address R0 to R2 -> R2 = Memory[1] = 1

# Wait, let me check the memory operation encoding again:
# In instruction_decoder.sv for opcode 2'b11:
# 2'b00: LW (Load Word) -> instruction[1:0] = 00
# 2'b01: SW (Store Word) -> instruction[1:0] = 01

# So:
# LW R2: opcode 11, lower bits 00, Rd=10 -> 1100 (0xC)
# SW: opcode 11, lower bits 01 -> 1101 (0xD)

# Complete memory program:
# Address 0: 1000 (0x8) - ADDI R0, R0, 1  -> R0 = 0 + 1 = 1 (set memory address to 1)
# Address 1: 1001 (0x9) - ADDI R1, R0, 1  -> R1 = 0 + 1 = 1 (set value to store)
# Address 2: 1101 (0xD) - SW              -> Store R1 at address R0 -> Memory[1] = 1
# Address 3: 1100 (0xC) - LW R2           -> Load from address R0 to R2 -> R2 = Memory[1] = 1

# Binary: 1000 1001 1101 1100
# Hex:    8    9    D    C