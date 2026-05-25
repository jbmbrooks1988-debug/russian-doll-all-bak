# RISC-V Programs for 2-bit Processor

This directory contains simple RISC-V assembly programs designed for the 2-bit RISC-V processor implementation.

## Instruction Format
- 4-bit instructions: [3:2] opcode | [1:0] operands/data
- Opcodes:
  - 00: ADD (R-type) - Adds R0 and R1, stores in Rd
  - 01: AND (R-type) - ANDs R0 and R1, stores in Rd
  - 10: ADDI (I-type) - Adds immediate to R0, stores in Rd
  - 11: Memory operations
    - 00: LW Rd - Load from memory address in R0 to Rd
    - 01: SW - Store value from R1 to memory address in R0

## Programs

### 1. add_program_corrected.s
Performs addition operation: 1 + 1 = 2 (in 2-bit arithmetic)
- Initializes R0 and R1 with value 1
- Adds them together, storing result in R2
- Instruction sequence: ADDI R0, ADDI R1, ADD R2, NOP

### 2. logical_program.s
Performs logical AND operation: 1 AND 0 = 0
- Initializes registers with values 1 and 0
- Performs AND operation, storing result in R2
- Instruction sequence: ADDI R1, ADDI R0, AND R2, NOP

### 3. memory_program.s
Demonstrates memory operations: store and load
- Sets up memory address in R0 and value in R1
- Stores value to memory, then loads it back
- Instruction sequence: ADDI R0, ADDI R1, SW, LW R2

### 4. complex_program.s
Combines arithmetic, logical, and demonstrates processor capabilities
- Performs multiple operations in sequence
- Shows how to chain operations together
- Instruction sequence: ADDI, ADDI, ADD, AND

## Binary Representation
Each program is represented as a sequence of 4-bit instructions that can be loaded into the processor's instruction memory.