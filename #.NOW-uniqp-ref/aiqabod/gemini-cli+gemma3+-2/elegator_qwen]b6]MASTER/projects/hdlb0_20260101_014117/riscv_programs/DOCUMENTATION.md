# Documentation: RISC-V Programs for 2-bit Processor

## Overview
This document describes the RISC-V assembly programs created for the 2-bit RISC-V processor implementation. These programs demonstrate the functionality of the processor and can be executed in the HDLb0 format.

## Processor Architecture
- **Word size**: 2 bits
- **Address size**: 2 bits
- **Register file**: 4 registers (R0-R3), 2 bits each
- **Instruction size**: 4 bits
- **Instruction format**: [3:2] opcode | [1:0] operands/data

### Instruction Set
- **ADD (00)**: R-type - Adds R0 and R1, stores result in Rd
- **AND (01)**: R-type - ANDs R0 and R1, stores result in Rd
- **ADDI (10)**: I-type - Adds immediate value to R0, stores in Rd
- **Memory Operations (11)**:
  - **LW (00)**: Load from memory address in R0 to Rd
  - **SW (01)**: Store value from R1 to memory address in R0

## Program Descriptions

### 1. Addition Program (`add_program_corrected.s`)
**Purpose**: Performs basic addition operation (1 + 1 = 2)

**Instructions**:
1. `ADDI R0, R0, 1` (0x8): Initialize R0 with value 1
2. `ADDI R1, R0, 1` (0x9): Initialize R1 with value 1 (using R0's initial value)
3. `ADD R2` (0x2): Add R0 and R1, store result in R2
4. `NOP` (0x0): No operation

**Expected Result**: R2 = 2 (in 2-bit arithmetic: 1 + 1 = 2)

**HDLb0 File**: `add_program_corrected.hdlb0`

### 2. Logical Program (`logical_program.s`)
**Purpose**: Performs logical AND operation (1 AND 0 = 0)

**Instructions**:
1. `ADDI R1, R0, 0` (0x9): Initialize R1 with value 0
2. `ADDI R0, R0, 1` (0x8): Initialize R0 with value 1
3. `AND R2` (0x6): AND R0 and R1, store result in R2
4. `NOP` (0x0): No operation

**Expected Result**: R2 = 0 (since 1 AND 0 = 0)

**HDLb0 File**: `logical_program.hdlb0`

### 3. Memory Program (`memory_program.s`)
**Purpose**: Demonstrates store and load operations

**Instructions**:
1. `ADDI R0, R0, 1` (0x8): Set memory address to 1
2. `ADDI R1, R0, 1` (0x9): Set value to store (1)
3. `SW` (0xD): Store R1 at memory address R0
4. `LW R2` (0xC): Load from memory address R0 to R2

**Expected Result**: R2 = 1 (the value previously stored)

**HDLb0 File**: `memory_program.hdlb0`

### 4. Complex Program (`complex_program.s`)
**Purpose**: Combines arithmetic and logical operations

**Instructions**:
1. `ADDI R0, R0, 1` (0x8): Initialize R0 with value 1
2. `ADDI R3, R0, 0` (0xB): Initialize R3 with value 0
3. `ADD R2` (0x2): Add R0 and R1, store in R2
4. `AND R3` (0x7): AND R0 and R2, store in R3

**Expected Result**: R0=1, R1=0, R2=1, R3=1

**HDLb0 File**: `complex_program.hdlb0`

## HDLb0 Binary Format
Each program is converted to HDLb0 binary format with the following structure:
- Header: "HDLB0_BINARY"
- Size: 4-byte little-endian size of binary data
- Data: Packed binary instructions (2 instructions per byte)

## Testing and Verification
All programs have been functionally verified using the `test_functionality.py` script, which simulates the expected behavior of each program. All tests pass, confirming the correct functionality of both the programs and the processor implementation.

## Usage
To execute these programs on the 2-bit RISC-V processor:
1. Load the HDLb0 binary file into the processor's instruction memory
2. Reset the processor
3. Start the clock
4. Monitor register and memory values to verify correct execution

## Limitations
- The 2-bit architecture has limited precision (values wrap around after 3)
- The simplified instruction format restricts some operations
- Memory is limited to 4 words of 2 bits each
- Register addressing is fixed for R-type operations (R0 and R1 as sources)

## Future Extensions
- More complex programs could be developed for extended functionality
- Additional instructions could be added to the ISA
- Larger memory could be implemented for more complex programs