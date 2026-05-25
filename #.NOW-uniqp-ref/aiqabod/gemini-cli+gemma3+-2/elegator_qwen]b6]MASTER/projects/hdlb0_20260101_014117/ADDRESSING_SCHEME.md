# HDLb0 Addressing Scheme Documentation

## Overview
This document explains the addressing scheme implemented in the RISC-V to HDLb0 assembler to ensure compatibility with the HDLb0 emulator.

## Memory Addressing Requirements
- The HDLb0 emulator reserves addresses 0-15 for special purposes
- General-purpose RAM operations must use addresses 16 and above
- All generated HDLb0 files must comply with this addressing scheme

## Implementation Details
The `riscv_to_hdlb0_assembler.py` has been updated to automatically shift memory addresses:

1. **Load/Store Instructions (`lw`, `sw`)**: Memory offsets are automatically increased by 16
   - Example: `sw x1, 0(x0)` becomes an operation at address 16 instead of 0
   - Example: `lw x1, 1(x0)` becomes an operation at address 17 instead of 1

2. **Load Address Instruction (`la`)**: Addresses are automatically increased by 16
   - Example: `la x1, 4` becomes `la x1, 20` (4 + 16)

3. **Load Immediate Instruction (`li`)**: Addresses in the range 0-15 are automatically increased by 16
   - Example: `li x1, 4` becomes `li x1, 20` (4 + 16)
   - Addresses outside the 0-15 range remain unchanged

4. **Add Immediate Instruction (`addi`)**: Memory addresses in the range 0-15 are automatically increased by 16
   - Example: `addi x1, x0, 2` becomes `addi x1, x0, 18` (2 + 16)
   - Values outside the 0-15 range remain unchanged

## Benefits
- Ensures all generated HDLb0 files are compatible with the emulator
- Maintains the same functionality while fixing addressing
- Provides backward compatibility with existing designs
- Automatically handles address translation without user intervention

## File Locations
- Updated assembler: `riscv_to_hdlb0_assembler.py`
- Corrected HDLb0 files: `riscv_2bit/hdlb0_output_new/`
- Test results: Verified emulator compatibility