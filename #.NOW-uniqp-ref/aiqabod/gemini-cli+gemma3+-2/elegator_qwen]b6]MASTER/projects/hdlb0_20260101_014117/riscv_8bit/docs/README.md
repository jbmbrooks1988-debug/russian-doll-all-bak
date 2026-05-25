# 8-bit RISC-V Processor Documentation

## Overview
This document describes the 8-bit RISC-V processor implementation that maintains backward compatibility with the 2-bit implementation and is compatible with the kernel.

## Architecture

### Register File
- 8 general-purpose registers (R0-R7)
- Each register is 8 bits wide
- R0 is hardwired to 0
- Supports simultaneous read of 2 registers and write of 1 register

### ALU (Arithmetic Logic Unit)
- 8-bit wide data paths
- Supports the following operations:
  - ADD (Addition)
  - SUB (Subtraction)
  - AND (Bitwise AND)
  - OR (Bitwise OR)
  - XOR (Bitwise XOR)
  - SLL (Shift Left Logical)
  - SRL (Shift Right Logical)
  - SLT (Set Less Than - signed comparison)

### Instruction Set
The processor supports a subset of RISC-V instructions:
- R-type: ADD, SUB, AND, OR, XOR, SLL, SRL, SLT
- I-type: ADDI, ANDI, ORI, XORI, SLLI, SRLI, SLTI, LW, JALR
- S-type: SW
- B-type: BEQ, BNE, BLT, BGE, BLTU, BGEU
- J-type: JAL
- System: ECALL, EBREAK

### Memory System
- 256 bytes of addressable memory (8-bit address space)
- Separate instruction and data memory
- Memory-mapped I/O
- Kernel/user space separation

### Interrupt System
- Timer interrupt
- Keyboard interrupt
- External interrupt
- System call interrupt
- Priority-based interrupt handling

## Backward Compatibility
The 8-bit processor maintains compatibility with the 2-bit implementation through:
- Similar instruction encoding principles
- Compatible system call interface
- Similar memory layout concepts
- Compatibility mode (optional)

## Kernel Compatibility
The processor is designed to work with the kernel through:
- Proper system call interface
- Memory management unit compatible with kernel requirements
- Interrupt handling that supports kernel operations
- Process control block format compatibility

## Components

### register_file_8bit.sv
- Implements the 8-register, 8-bit wide register file
- Handles register read/write operations
- Ensures R0 is always 0

### alu_8bit.sv
- Implements the 8-bit ALU with multiple operations
- Generates zero flag for conditional operations

### instruction_decoder.sv
- Decodes 16-bit RISC-V instructions
- Generates control signals for processor execution

### control_unit.sv
- Generates control signals based on instruction opcode
- Controls data path operations

### program_counter.sv
- Manages program counter for instruction sequencing
- Includes instruction memory for fetching

### memory_management_unit.sv
- Handles memory access and management
- Implements basic virtual-to-physical address translation
- Enforces memory protection

### interrupt_handling_unit.sv
- Manages interrupt requests and responses
- Implements interrupt prioritization

### riscv_8bit_processor.sv
- Top-level integration of all components
- Implements the complete processor pipeline

## Test Programs
Several test programs are included to verify functionality:
- basic_arithmetic.asm: Tests ALU operations
- memory_operations.asm: Tests load/store operations
- branch_operations.asm: Tests conditional branching
- kernel_interaction.asm: Tests system calls

## Verification
Testbenches are provided for:
- Individual component testing
- Full processor integration testing
- Instruction execution verification

## Implementation Notes
- The processor uses a 5-stage pipeline (IF, ID, EX, MEM, WB)
- All operations are synchronized to the system clock
- Reset initializes processor to a known state
- Memory accesses take one cycle in simulation (may take more in physical implementation)