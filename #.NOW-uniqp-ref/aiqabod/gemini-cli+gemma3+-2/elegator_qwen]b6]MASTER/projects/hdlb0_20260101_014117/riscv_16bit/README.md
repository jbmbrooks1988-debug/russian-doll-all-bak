# 16-bit RISC-V Processor Implementation

## Overview
This project implements a 16-bit RISC-V processor that maintains backward compatibility with the 2-bit and 8-bit implementations while providing enhanced functionality and kernel compatibility.

## Key Features
- 16-bit data path with 32 general-purpose registers
- RISC-V instruction set architecture compliance (RV32I subset adapted for 16-bit)
- Memory management unit compatible with kernel
- Interrupt handling system
- Backward compatibility with 2-bit and 8-bit programs
- Kernel system call interface

## Architecture
See `docs/architecture.md` for detailed architectural information.

## Files Created

### Core Components
- `register_file_16bit.sv`: 32-register, 16-bit register file
- `alu_16bit.sv`: 16-bit ALU with extended operations
- `instruction_decoder.sv`: Instruction decoder for 16-bit instructions
- `control_unit.sv`: Control logic generation
- `program_counter.sv`: Program counter and instruction fetch
- `memory_management_unit.sv`: Memory management and protection
- `interrupt_handling_unit.sv`: Interrupt handling
- `riscv_16bit_processor.sv`: Top-level integration
- `compatibility_module.sv`: 2-bit and 8-bit compatibility support

### Test Programs
- `programs/basic_arithmetic.asm`: Basic ALU operations
- `programs/memory_operations.asm`: Load/store operations
- `programs/branch_operations.asm`: Conditional branching
- `programs/kernel_interaction.asm`: System calls

### Testbenches
- `tests/riscv_16bit/riscv_16bit_processor_tb.sv`: Processor testbench
- `tests/riscv_16bit/alu_16bit_tb.sv`: ALU testbench

### Documentation
- `docs/architecture.md`: Architecture overview

## Compatibility
The implementation maintains backward compatibility with the 2-bit and 8-bit processors through:
- Similar architectural principles
- Compatible system call interface
- Compatibility mode support
- Similar instruction encoding concepts

## Kernel Integration
The processor is designed to work with the kernel through:
- Proper system call interface (ECALL instruction)
- Memory management that respects kernel/user boundaries
- Interrupt system compatible with kernel requirements
- Process control block format compatibility

## Verification
The implementation includes:
- Component-level testbenches
- Integration testbenches
- Functional test programs
- Documentation of all components