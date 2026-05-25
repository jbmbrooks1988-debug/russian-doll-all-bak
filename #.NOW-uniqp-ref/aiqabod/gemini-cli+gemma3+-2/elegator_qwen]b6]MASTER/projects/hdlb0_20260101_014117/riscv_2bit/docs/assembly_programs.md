# RISC-V Assembly Programs for 2-bit Processor

## Overview
This directory contains RISC-V assembly programs specifically designed for the 2-bit RISC-V processor implemented in HDLb0. These programs demonstrate various functionalities of the processor including arithmetic operations, logic operations, memory access, and control flow.

## Directory Structure
- `programs/` - Contains the RISC-V assembly source files (.s)
- `hdlb0_output/` - Contains the HDLb0 binary format files (.hlo) converted from assembly
- `docs/` - Contains documentation about the programs and implementation

## Assembly Programs

### Basic Operation Programs
1. `add_program.s` - Simple addition operation (add x1, x2, x3)
2. `logic_program.s` - Logic operations (AND, OR, XOR)
3. `memory_program.s` - Memory access operations (load/store)
4. `loop_program.s` - Simple loop implementation
5. `hello_world.s` - "Hello World" equivalent demonstrating basic functionality

### Test Programs
1. `register_test.s` - Tests the register file functionality
2. `alu_test.s` - Tests the ALU functionality
3. `memory_test.s` - Tests the memory subsystem
4. `control_test.s` - Tests the control unit functionality

## Instruction Set
The 2-bit RISC-V processor supports a simplified instruction set:
- Arithmetic: add, sub
- Logic: and, or, xor
- Memory: lw (load word), sw (store word)
- Control: j (jump)

## Register File
- 4 registers: x0, x1, x2, x3
- x0 is hardwired to 0
- Each register holds 2-bit values (0, 1, 2, 3)

## Memory
- 4 memory locations
- Each location stores 2-bit values
- Addressed using 2-bit addresses (0, 1, 2, 3)

## HDLb0 Conversion
The assembly programs are converted to HDLb0 format using a custom assembler. Each instruction is encoded as a 4-bit value that can be executed by the HDLb0 emulator.

## Usage
To run these programs on the HDLb0 emulator:
1. Use the .hlo files in the hdlb0_output directory
2. Load the file into the HDLb0 emulator
3. Execute the program

## Implementation Notes
- Due to the 2-bit limitation, programs are highly constrained
- Immediate values are not directly supported in this implementation
- Programs rely on pre-loaded register values or simple operations to generate values