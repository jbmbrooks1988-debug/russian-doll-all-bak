# 2-bit RISC-V Processor Implementation

## Overview
This project implements a 2-bit RISC-V processor using HDLb0. The processor is designed with minimal gate count in mind while maintaining the essential features of a RISC-V processor.

## Architecture
- 2-bit data path
- 4 general-purpose registers (x0-x3, with x0 hardwired to 0)
- 4 memory locations (2-bit values each)
- Simplified RISC-V instruction set
- 4-bit instruction encoding

## Components
- Register file: 2-bit, 4-register implementation
- ALU: Supports ADD, SUB, AND, OR, XOR operations
- Control unit: Manages processor control signals
- Program counter: 2-bit implementation
- Memory subsystem: 4 locations of 2-bit values each
- Instruction decoder: Decodes 4-bit instructions

## Assembly Programs
The `programs/` directory contains various RISC-V assembly programs:
- Basic operation programs (addition, logic operations, memory access)
- Test programs for each component
- "Hello World" equivalent program

## HDLb0 Conversion
Assembly programs are converted to HDLb0 format using the custom assembler. The converted files are in the `hdlb0_output/` directory with `.hlo` extension.

## Usage
To run programs on the HDLb0 emulator:
1. Select a .hlo file from the hdlb0_output directory
2. Load into the HDLb0 emulator
3. Execute the program

## Design Constraints
- Limited to 2-bit values (0, 1, 2, 3)
- Simplified instruction format due to bit constraints
- No direct immediate value support in this implementation
- Minimal instruction set focusing on core functionality

## Files
- `programs/` - RISC-V assembly source files
- `hdlb0_output/` - HDLb0 binary format files
- `docs/` - Documentation
- `assembler.py` - Custom assembler for converting assembly to HDLb0 format