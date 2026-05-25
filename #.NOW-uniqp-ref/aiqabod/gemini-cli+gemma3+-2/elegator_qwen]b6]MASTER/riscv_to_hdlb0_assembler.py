#!/usr/bin/env python3
"""
RISC-V Assembly to HDLb0 Binary Format Assembler

This script converts simple RISC-V assembly programs to HDLb0 binary format.
The 2-bit RISC-V processor has a simplified instruction set as implemented in the project.
"""

import re
import sys
from typing import List, Dict, Tuple


class RISCVToHDLB0Assembler:
    def __init__(self):
        # Define the 2-bit RISC-V instruction format based on the implementation
        # From the README: 4-bit instruction format: [3:2] opcode, [1:0] data/operands
        self.opcodes = {
            'add': 0b00,   # Addition
            'and': 0b01,   # AND operation
            'addi': 0b10,  # Add immediate
            'sw': 0b11,    # Store word
            'lw': 0b11,    # Load word (same opcode, different funct3)
            'or': 0b01,    # OR operation (using same opcode as AND, but different ALU control)
            'xor': 0b01,   # XOR operation (using same opcode as AND, but different ALU control)
            'sub': 0b00,   # Subtraction (using same opcode as ADD, but different ALU control)
        }
        
        # Register mapping (2-bit registers: x0, x1, x2, x3)
        self.registers = {
            'x0': 0b00,
            'x1': 0b01,
            'x2': 0b10,
            'x3': 0b11,
        }
        
        # Memory addresses (2-bit: 0, 1, 2, 3)
        self.memory_addresses = {i: i for i in range(4)}
        
        # HDLb0 specific constants
        self.constants = {
            '0': '0000000000000000',  # 0
            '1': '0000000000000001',  # 1
        }
        
        # HDLb0 input mapping
        self.input_map = {
            'switch_0': '0000000000000101',  # 5
            'switch_1': '0000000000000110',  # 6
            'clock': '0000000000000111',     # 7
        }

    def _get_register_value(self, reg_str: str) -> int:
        """Get register value from register string (e.g., 'x1' -> 1)."""
        reg_str = reg_str.lower().strip()
        if reg_str in self.registers:
            return self.registers[reg_str]
        elif reg_str.startswith('x') and reg_str[1:].isdigit():
            val = int(reg_str[1:])
            if 0 <= val <= 3:  # 2-bit register index
                return val
            else:
                raise ValueError(f"Invalid register index: {val} (must be 0-3)")
        elif reg_str.isdigit():
            val = int(reg_str)
            if 0 <= val <= 3:  # 2-bit register index
                return val
            else:
                raise ValueError(f"Invalid register index: {val} (must be 0-3)")
        else:
            raise ValueError(f"Invalid register format: {reg_str}")

    def parse_instruction(self, line: str) -> Dict:
        """Parse a RISC-V assembly instruction."""
        line = line.strip()
        if not line or line.startswith('#') or line.endswith(':'):  # Skip comments and labels
            return None
            
        # Remove comments
        line = line.split('#')[0].strip()
        if not line:
            return None
            
        # Parse instruction: instruction rd, rs1, rs2 or instruction rd, rs2, rs1
        parts = re.split(r'[,\s]+', line)
        if not parts or not parts[0]:
            return None
            
        instruction = parts[0].lower()
        
        # Extract operands
        operands = [op.strip() for op in parts[1:] if op.strip()]
        
        return {
            'instruction': instruction,
            'operands': operands
        }

    def encode_instruction(self, instr: Dict) -> List[str]:
        """Encode a parsed instruction to HDLb0 format."""
        instruction = instr['instruction']
        operands = instr['operands']

        if instruction == 'add':
            # Format: add rd, rs1, rs2
            if len(operands) != 3:
                raise ValueError(f"ADD instruction requires 3 operands, got {len(operands)}")

            rd = self._get_register_value(operands[0])
            rs1 = self._get_register_value(operands[1])
            rs2 = self._get_register_value(operands[2])
            
            # For HDLb0, we need to implement this using our processor components
            # This is a simplified representation - in reality, this would involve multiple HDLb0 instructions
            # to perform the actual ALU operation and register write
            
            # This is a placeholder for the actual HDLb0 instructions needed
            # to implement the ADD operation on our 2-bit processor
            hdlb0_instructions = []
            
            # Example: perform addition using ALU and store result
            # 1. Connect rs1 and rs2 to ALU
            # 2. Set ALU operation to ADD
            # 3. Write result to rd
            
            # This is a simplified representation
            opcode_bin = format(self.opcodes[instruction], '02b')
            rd_bin = format(rd, '02b')
            rs1_bin = format(rs1, '02b')
            rs2_bin = format(rs2, '02b')
            
            # Combine into a 4-bit instruction (this is conceptual)
            # In reality, each RISC-V instruction would map to multiple HDLb0 instructions
            combined = opcode_bin + rs1_bin  # Simplified encoding
            
            # For now, we'll create placeholder HDLb0 instructions
            # that would perform the ADD operation in the actual processor
            hdlb0_instructions.append(f"0000000000000001 00010000 0000{rs1:02b}00 0000{rs2:02b}00")  # Connect rs1, rs2 to ALU
            hdlb0_instructions.append(f"0000000000000001 00010001 00010000 00000000")  # ALU operation (simplified)
            hdlb0_instructions.append(f"0000000000000000 0000{rd:02b}00 00010001 00000011")  # Write result to register rd
            
            return hdlb0_instructions
            
        elif instruction == 'and':
            # Format: and rd, rs1, rs2
            if len(operands) != 3:
                raise ValueError(f"AND instruction requires 3 operands, got {len(operands)}")

            rd = self._get_register_value(operands[0])
            rs1 = self._get_register_value(operands[1])
            rs2 = self._get_register_value(operands[2])

            # Similar to ADD, but with AND operation
            hdlb0_instructions = []
            hdlb0_instructions.append(f"0000000000000001 00010000 0000{rs1:02b}00 0000{rs2:02b}00")  # Connect rs1, rs2 to ALU
            hdlb0_instructions.append(f"0000000000000001 00010001 00010000 00000001")  # ALU operation (simplified for AND)
            hdlb0_instructions.append(f"0000000000000000 0000{rd:02b}00 00010001 00000011")  # Write result to register rd

            return hdlb0_instructions
            
        elif instruction == 'sw':  # Store word
            # Format: sw rs2, offset(rs1) - store rs2 to memory at address rs1+offset
            if len(operands) != 2:
                raise ValueError(f"SW instruction requires 2 operands, got {len(operands)}")

            rs2 = self._get_register_value(operands[0])
            # Parse memory address: offset(rs1)
            mem_part = operands[1]
            match = re.match(r'(\d*)\((\w+)\)', mem_part)
            if not match:
                raise ValueError(f"Invalid memory operand format: {mem_part}")

            offset = int(match.group(1)) if match.group(1) else 0
            rs1 = self._get_register_value(match.group(2))
            address = (rs1 + offset) % 4  # 2-bit address space

            hdlb0_instructions = []
            hdlb0_instructions.append(f"0000000000000011 000000{address:02b} 0000{rs2:02b}00 00000000")  # Store operation

            return hdlb0_instructions
            
        elif instruction == 'lw':  # Load word
            # Format: lw rd, offset(rs1) - load from memory at address rs1+offset to rd
            if len(operands) != 2:
                raise ValueError(f"LW instruction requires 2 operands, got {len(operands)}")

            rd = self._get_register_value(operands[0])
            # Parse memory address: offset(rs1)
            mem_part = operands[1]
            match = re.match(r'(\d*)\((\w+)\)', mem_part)
            if not match:
                raise ValueError(f"Invalid memory operand format: {mem_part}")

            offset = int(match.group(1)) if match.group(1) else 0
            rs1 = self._get_register_value(match.group(2))
            address = (rs1 + offset) % 4  # 2-bit address space

            hdlb0_instructions = []
            hdlb0_instructions.append(f"0000000000000011 0000{rd:02b}00 000000{address:02b} 00000000")  # Load operation

            return hdlb0_instructions
            
        elif instruction == 'addi':  # Add immediate
            # Format: addi rd, rs1, immediate
            if len(operands) != 3:
                raise ValueError(f"ADDI instruction requires 3 operands, got {len(operands)}")

            rd = self._get_register_value(operands[0])
            rs1 = self._get_register_value(operands[1])
            immediate = int(operands[2])

            hdlb0_instructions = []
            hdlb0_instructions.append(f"0000000000000001 00010000 0000{rs1:02b}00 0000{immediate:02b}00")  # Connect rs1, imm to ALU
            hdlb0_instructions.append(f"0000000000000001 00010001 00010000 00000010")  # ALU operation (simplified for ADDI)
            hdlb0_instructions.append(f"0000000000000000 0000{rd:02b}00 00010001 00000011")  # Write result to register rd

            return hdlb0_instructions

        elif instruction == 'or':  # OR operation
            # Format: or rd, rs1, rs2
            if len(operands) != 3:
                raise ValueError(f"OR instruction requires 3 operands, got {len(operands)}")

            rd = self._get_register_value(operands[0])
            rs1 = self._get_register_value(operands[1])
            rs2 = self._get_register_value(operands[2])

            # OR operation using ALU
            hdlb0_instructions = []
            hdlb0_instructions.append(f"0000000000000001 00010000 0000{rs1:02b}00 0000{rs2:02b}00")  # Connect rs1, rs2 to ALU
            hdlb0_instructions.append(f"0000000000000001 00010001 00010000 00000011")  # ALU operation (simplified for OR)
            hdlb0_instructions.append(f"0000000000000000 0000{rd:02b}00 00010001 00000011")  # Write result to register rd

            return hdlb0_instructions

        elif instruction == 'xor':  # XOR operation
            # Format: xor rd, rs1, rs2
            if len(operands) != 3:
                raise ValueError(f"XOR instruction requires 3 operands, got {len(operands)}")

            rd = self._get_register_value(operands[0])
            rs1 = self._get_register_value(operands[1])
            rs2 = self._get_register_value(operands[2])

            # XOR operation using ALU
            hdlb0_instructions = []
            hdlb0_instructions.append(f"0000000000000001 00010000 0000{rs1:02b}00 0000{rs2:02b}00")  # Connect rs1, rs2 to ALU
            hdlb0_instructions.append(f"0000000000000001 00010001 00010000 00000100")  # ALU operation (simplified for XOR)
            hdlb0_instructions.append(f"0000000000000000 0000{rd:02b}00 00010001 00000011")  # Write result to register rd

            return hdlb0_instructions

        elif instruction == 'sub':  # Subtraction operation
            # Format: sub rd, rs1, rs2
            if len(operands) != 3:
                raise ValueError(f"SUB instruction requires 3 operands, got {len(operands)}")

            rd = self._get_register_value(operands[0])
            rs1 = self._get_register_value(operands[1])
            rs2 = self._get_register_value(operands[2])

            # Subtraction operation using ALU
            hdlb0_instructions = []
            hdlb0_instructions.append(f"0000000000000001 00010000 0000{rs1:02b}00 0000{rs2:02b}00")  # Connect rs1, rs2 to ALU
            hdlb0_instructions.append(f"0000000000000001 00010001 00010000 00000001")  # ALU operation (simplified for SUB)
            hdlb0_instructions.append(f"0000000000000000 0000{rd:02b}00 00010001 00000011")  # Write result to register rd

            return hdlb0_instructions

        elif instruction == 'j':  # Jump (for loops)
            # Format: j label
            # This is a simplified implementation
            hdlb0_instructions = []
            # In a real implementation, this would involve changing the PC
            # For now, we'll just add a placeholder
            hdlb0_instructions.append(f"0000000000000000 00000000 00000000 00000000")  # Placeholder for jump

            return hdlb0_instructions

        else:
            raise ValueError(f"Unsupported instruction: {instruction}")

    def assemble_file(self, input_file: str, output_file: str):
        """Assemble a RISC-V assembly file to HDLb0 format."""
        with open(input_file, 'r') as f:
            lines = f.readlines()
        
        hdlb0_instructions = []
        hdlb0_instructions.append("# Assembled from RISC-V assembly")
        hdlb0_instructions.append("#")
        
        for line_num, line in enumerate(lines, 1):
            parsed = self.parse_instruction(line)
            if parsed:
                try:
                    encoded = self.encode_instruction(parsed)
                    hdlb0_instructions.extend(encoded)
                except ValueError as e:
                    print(f"Error on line {line_num}: {e}")
                    print(f"Instruction: {line}")
                    continue
        
        with open(output_file, 'w') as f:
            f.write('\n'.join(hdlb0_instructions))
        
        print(f"Assembled {input_file} to {output_file}")
        print(f"Generated {len([l for l in hdlb0_instructions if l and not l.startswith('#')])} HDLb0 instructions")


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 riscv_to_hdlb0_assembler.py <input_asm_file> <output_hdlb0_file>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    assembler = RISCVToHDLB0Assembler()
    assembler.assemble_file(input_file, output_file)


if __name__ == "__main__":
    main()