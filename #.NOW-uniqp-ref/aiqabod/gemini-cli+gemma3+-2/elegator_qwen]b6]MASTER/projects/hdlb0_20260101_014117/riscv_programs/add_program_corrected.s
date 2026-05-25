# Addition program for 2-bit RISC-V processor
# Instruction format: [3:2] opcode | [1:0] operands/data
# Opcodes: 00=ADD, 01=AND, 10=ADDI, 11=Memory ops

# This program adds two values and stores the result
# Memory layout (4 instructions max):
# Address 0: ADDI R0, R0, 1  -> Load value 1 into R0 -> 1000 (0x8)
# Address 1: ADDI R1, R0, 1  -> Load value 1 into R1 -> 1001 (0x9) 
# Address 2: ADD R2          -> R2 = R0 + R1 -> 0010 (0x2)
# Address 3: NOP             -> No operation -> 0000 (0x0)

# Note: In our implementation:
# - ADDI Rd, R0, imm -> opcode 10, Rd in [1:0] -> 10RR where RR is register number
# - ADD Rd -> opcode 00, Rd in [1:0] -> 00RR where RR is register number
# - R0=00, R1=01, R2=10, R3=11

# Program to compute 1 + 1 = 2:
# Instruction 0: 1000 (0x8) - ADDI R0, R0, 0  (Actually adds immediate 0 to R0, so R0=0)
# Wait, let me re-read the implementation...

# In instruction_decoder.sv:
# For ADDI: rs1 = 2'b00 (R0), rd = instruction[1:0], alu_op = 2'b00 (ADD)
# So ADDI R[rd], R0, immediate means it adds R0's value + immediate and stores in R[rd]
# The immediate is instruction[1:0] which is the lower 2 bits of the 4-bit instruction

# So for instruction 1001 (0x9):
# opcode = 10 (ADDI), immediate = 01 (1)
# This means: Rd = R0 + 1, where Rd is specified by lower 2 bits = 01 (R1)
# So: R1 = R0 + 1

# Program to compute 1 + 1 = 2:
# Address 0: 1001 (0x9) - ADDI R1, R0, 1  -> R1 = R0 + 1 = 0 + 1 = 1 (assuming R0 starts at 0)
# Address 1: 1010 (0xA) - ADDI R2, R0, 1  -> R2 = R0 + 1 = 0 + 1 = 1
# Address 2: 0010 (0x2) - ADD R2          -> R2 = R0 + R1 = 1 + 1 = 2
# Address 3: 0000 (0x0) - NOP

# Actually, looking more carefully:
# In the ALU: operand_b = (opcode == 2'b10) ? {immediate} : read_data_2
# So for ADDI, operand_b is the immediate value, not R0+immediate
# For ADD, operand_b is read_data_2 (which is R1)

# So ADDI Rd, R0, imm actually means Rd = R0 + imm
# And ADD Rd means Rd = R0 + R1

# Corrected program to compute 1 + 1 = 2:
# Address 0: 1001 (0x9) - ADDI R1, R0, 1  -> R1 = R0 + 1 = 0 + 1 = 1
# Address 1: 1010 (0xA) - ADDI R2, R0, 1  -> R2 = R0 + 1 = 0 + 1 = 1  
# Address 2: 0010 (0x2) - ADD R2          -> R2 = R0 + R1 = 0 + 1 = 1 (Wait, this is wrong)

# Actually, looking at the ALU again:
# operand_a = read_data_1 (which is R0)
# operand_b = (opcode == 2'b10) ? {immediate} : read_data_2 (R1 for ADD, immediate for ADDI)
# So ADDI Rd, R0, imm means Rd = R0 + immediate
# And ADD Rd means Rd = R0 + R1

# So to get R2 = 1 + 1:
# First, we need to get a 1 into R0 or R1
# Address 0: 1001 (0x9) - ADDI R1, R0, 1  -> R1 = R0 + 1 = 0 + 1 = 1
# Address 1: 1001 (0x9) - ADDI R1, R0, 1  -> R1 = R0 + 1 = 0 + 1 = 1 (this would overwrite R1)
# Address 1: 1010 (0xA) - ADDI R2, R0, 1  -> R2 = R0 + 1 = 0 + 1 = 1
# Address 2: 0010 (0x2) - ADD R2          -> R2 = R0 + R1 = 0 + 1 = 1

# This won't work as expected. Let me think differently:
# Address 0: 1001 (0x9) - ADDI R1, R0, 1  -> R1 = R0 + 1 = 0 + 1 = 1
# Address 1: 1011 (0xB) - ADDI R3, R0, 1  -> R3 = R0 + 1 = 0 + 1 = 1
# Address 2: 0001 (0x1) - ADD R1          -> R1 = R0 + R1 = 0 + 1 = 1 (still not right)

# Actually, I think I misunderstood. Let me look at the ALU again:
# alu_result = case(alu_op)
#   2'b00: operand_a + operand_b;  // ADD
#   ...
# So ADD R2 means: R2 = R0 + R1 (where R0 is operand_a and R1 is operand_b for R-type)
# And ADDI R2, R0, imm means: R2 = R0 + immediate

# So to compute 1 + 1:
# Address 0: 1001 (0x9) - ADDI R1, R0, 1  -> R1 = R0 + 1 = 0 + 1 = 1
# Address 1: 1010 (0xA) - ADDI R2, R0, 1  -> R2 = R0 + 1 = 0 + 1 = 1
# Address 2: 0010 (0x2) - ADD R2          -> R2 = R0 + R1 = 0 + 1 = 1 (nope, still wrong)

# I think I need to re-read the register file addressing...
# In instruction_decoder.sv for ADD:
# rs1 = 2'b00;  // Source 1 is always R0
# rs2 = 2'b01;  // Source 2 is always R1
# rd = instruction[1:0];  // Destination register

# So ADD R2 means R2 = R0 + R1, where R0 and R1 are fixed sources!
# This means I need to set up R0 and R1 with the values I want to add.

# Final program to compute 1 + 1 = 2:
# Address 0: 1000 (0x8) - ADDI R0, R0, 1  -> R0 = R0 + 1 = 0 + 1 = 1
# Address 1: 1001 (0x9) - ADDI R1, R0, 1  -> R1 = R0 + 1 = 1 + 1 = 2 (Wait, this adds R0's NEW value + 1)
# No, this adds R0's CURRENT value + immediate, so R1 = 0 + 1 = 1

# Actually, looking at the processor again:
# operand_b = (opcode == 2'b10) ? {immediate} : read_data_2
# So for ADDI, operand_b is the immediate value, not R0+immediate
# The ALU computes operand_a + operand_b, where operand_a is R0
# So ADDI Rd, R0, imm means Rd = R0(current) + immediate

# So:
# Address 0: 1000 (0x8) - ADDI R0, R0, 1  -> R0 = R0(current) + 1 = 0 + 1 = 1
# Address 1: 1001 (0x9) - ADDI R1, R0, 1  -> R1 = R0(current) + 1 = 0 + 1 = 1 (R0's value at execution time)
# Address 2: 0010 (0x2) - ADD R2          -> R2 = R0 + R1 = 1 + 1 = 2

# Yes, this should work! The ADDI instruction uses the current value of R0, not its new value.

# Complete program:
# Address 0: 1000 (0x8) - ADDI R0, R0, 1  -> R0 = 0 + 1 = 1
# Address 1: 1001 (0x9) - ADDI R1, R0, 1  -> R1 = 0 + 1 = 1 (R0 starts at 0)
# Address 2: 0010 (0x2) - ADD R2          -> R2 = R0 + R1 = 1 + 1 = 2
# Address 3: 0000 (0x0) - NOP

# Binary: 1000 1001 0010 0000
# Hex:    8    9    2    0