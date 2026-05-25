#!/usr/bin/env python3
"""
Simple assembler for 16-bit RISC-V processor to convert assembly to HDLb0 binary format
This is a simplified implementation for demonstration purposes
"""

import sys
import re

def parse_instruction(instr):
    """Parse a simple RISC-V-like instruction and convert to 16-bit binary"""
    instr = instr.strip()
    if not instr:
        return None
    
    # Remove comments (everything after # or //)
    instr = re.split(r'[#//]', instr)[0].strip()
    if not instr:
        return None
    
    # Skip labels and directives
    if instr.endswith(':') or instr.startswith('.'):
        return None
    
    # Define simple opcodes for our 16-bit processor
    opcodes = {
        'add': 0b000,
        'sub': 0b001,
        'and': 0b010,
        'or':  0b011,
        'xor': 0b100,
        'sll': 0b101,  # shift left logical
        'srl': 0b110,  # shift right logical
        'sra': 0b111,  # shift right arithmetic
        'slt': 0b000,
        'sltu': 0b001,
        'slli': 0b101, # shift left logical immediate (same as sll but with immediate)
        'addi': 0b010,
        'lw': 0b011,
        'sw': 0b100,
        'beq': 0b101,
        'bne': 0b110,
        'blt': 0b111,
        'bge': 0b000,
        'jal': 0b001,
        'jalr': 0b010,
        'j': 0b011,     # Unconditional jump
        'ecall': 0b111100,  # System call
        'li': 0b011,        # Pseudo-instruction for loading immediate
        'mv': 0b000,        # Pseudo-instruction for move (add rd, rs, x0)
        'la': 0b011,        # Pseudo-instruction for load address (lw for our purposes)
    }
    
    # Parse instruction
    parts = instr.split()
    if not parts:
        return None
    
    opcode_str = parts[0].lower()
    
    if opcode_str not in opcodes:
        print(f"Unknown instruction: {opcode_str}")
        return None
    
    opcode = opcodes[opcode_str]
    
    # Handle different instruction formats
    if opcode_str in ['add', 'sub', 'and', 'or', 'xor', 'slt', 'sltu']:
        # R-type: opcode rs2 rs1 funct3 rd (simplified 16-bit format)
        # Format: add rd, rs1, rs2
        if len(parts) < 4:
            print(f"Invalid format for {opcode_str}: {instr}")
            return None
        
        # Extract registers, handling the comma properly
        rd_str = parts[1].strip(',').strip()
        rs1_str = parts[2].strip(',').strip()
        rs2_str = parts[3].strip(',').strip()
        
        rd_match = re.search(r'x(\d+)', rd_str)
        rs1_match = re.search(r'x(\d+)', rs1_str)
        rs2_match = re.search(r'x(\d+)', rs2_str)
        
        if not (rd_match and rs1_match and rs2_match):
            print(f"Invalid register format in {instr}")
            return None
        
        rd = int(rd_match.group(1))
        rs1 = int(rs1_match.group(1))
        rs2 = int(rs2_match.group(1))
        
        # For simplicity, use a custom 16-bit format
        # [15:13] opcode [12:10] rs1 [9:7] rs2 [6:4] rd [3:0] funct3
        funct3_map = {'add': 0, 'sub': 1, 'and': 2, 'or': 3, 'xor': 4, 'slt': 5, 'sltu': 6}
        funct3 = funct3_map.get(opcode_str, 0)
        
        # Limit registers to 3 bits (0-7) for our simplified format
        rd = rd & 0x7
        rs1 = rs1 & 0x7
        rs2 = rs2 & 0x7
        
        return ((opcode & 0x7) << 13) | ((rs1 & 0x7) << 10) | ((rs2 & 0x7) << 7) | ((rd & 0x7) << 4) | (funct3 & 0xF)
    
    elif opcode_str in ['sll', 'srl', 'sra']:
        # Shift operations: rd, rs1, rs2 (where rs2 is shift amount in RISC-V)
        # Format: sll rd, rs1, rs2  OR  sll rd, rs1, immediate
        if len(parts) < 4:
            print(f"Invalid format for {opcode_str}: {instr}")
            return None
        
        # Extract operands
        rd_str = parts[1].strip(',').strip()
        rs1_str = parts[2].strip(',').strip()
        rs2_str = parts[3].strip(',').strip()
        
        rd_match = re.search(r'x(\d+)', rd_str)
        rs1_match = re.search(r'x(\d+)', rs1_str)
        
        if not (rd_match and rs1_match):
            print(f"Invalid register format in {instr}")
            return None
        
        rd = int(rd_match.group(1))
        rs1 = int(rs1_match.group(1))
        
        # Check if rs2_str is a register or immediate
        rs2_match = re.search(r'x(\d+)', rs2_str)
        if rs2_match:
            # It's a register
            rs2 = int(rs2_match.group(1))
            # For simplicity, use a custom 16-bit format
            # [15:13] opcode [12:10] rs1 [9:7] rs2 [6:4] rd [3:0] funct3
            funct3_map = {'sll': 5, 'srl': 6, 'sra': 7}
            funct3 = funct3_map.get(opcode_str, 0)
            
            # Limit registers to 3 bits (0-7) for our simplified format
            rd = rd & 0x7
            rs1 = rs1 & 0x7
            rs2 = rs2 & 0x7  # This will be the register containing the shift amount
            
            return ((opcode & 0x7) << 13) | ((rs1 & 0x7) << 10) | ((rs2 & 0x7) << 7) | ((rd & 0x7) << 4) | (funct3 & 0xF)
        else:
            # It's an immediate value
            try:
                shift_amount = int(rs2_str)
                # For shift with immediate, we'll use the same format but treat rs2 as immediate
                # In RISC-V, the shift amount is in the lower 5 bits of rs2 field
                rs2 = shift_amount & 0x1F  # Lower 5 bits for shift amount
                
                # For simplicity, use a custom 16-bit format
                # [15:13] opcode [12:10] rs1 [9:7] rs2 [6:4] rd [3:0] funct3
                funct3_map = {'sll': 5, 'srl': 6, 'sra': 7}
                funct3 = funct3_map.get(opcode_str, 0)
                
                # Limit registers to 3 bits (0-7) for our simplified format
                rd = rd & 0x7
                rs1 = rs1 & 0x7
                rs2 = rs2 & 0x7  # This will be the shift amount
                
                return ((opcode & 0x7) << 13) | ((rs1 & 0x7) << 10) | ((rs2 & 0x7) << 7) | ((rd & 0x7) << 4) | (funct3 & 0xF)
            except ValueError:
                print(f"Invalid shift amount in {instr}")
                return None
    
    elif opcode_str in ['slli']:
        # Shift left logical immediate: rd, rs1, immediate
        # Format: slli rd, rs1, immediate
        if len(parts) < 4:
            print(f"Invalid format for {opcode_str}: {instr}")
            return None
        
        # Extract operands
        rd_str = parts[1].strip(',').strip()
        rs1_str = parts[2].strip(',').strip()
        immediate_str = parts[3].strip(',').strip()
        
        rd_match = re.search(r'x(\d+)', rd_str)
        rs1_match = re.search(r'x(\d+)', rs1_str)
        
        if not (rd_match and rs1_match):
            print(f"Invalid register format in {instr}")
            return None
        
        rd = int(rd_match.group(1))
        rs1 = int(rs1_match.group(1))
        
        # Extract immediate value
        try:
            shift_amount = int(immediate_str)
            # For shift with immediate, we'll use the same format as sll but with immediate
            # In RISC-V, the shift amount is in the lower 5 bits of rs2 field
            rs2 = shift_amount & 0x1F  # Lower 5 bits for shift amount
            
            # For simplicity, use a custom 16-bit format
            # [15:13] opcode [12:10] rs1 [9:7] rs2 [6:4] rd [3:0] funct3
            funct3 = 5  # sll
            
            # Limit registers to 3 bits (0-7) for our simplified format
            rd = rd & 0x7
            rs1 = rs1 & 0x7
            rs2 = rs2 & 0x7  # This will be the shift amount
            
            return ((opcode & 0x7) << 13) | ((rs1 & 0x7) << 10) | ((rs2 & 0x7) << 7) | ((rd & 0x7) << 4) | (funct3 & 0xF)
        except ValueError:
            print(f"Invalid shift amount in {instr}")
            return None
    
    elif opcode_str in ['addi']:
        # I-type: opcode rs1 funct3 rd immediate (simplified 16-bit format)
        # Format: addi rd, rs1, immediate
        if len(parts) < 4:
            print(f"Invalid format for {opcode_str}: {instr}")
            return None

        # Extract registers and immediate
        rd_str = parts[1].strip(',').strip()
        rs1_str = parts[2].strip(',').strip()
        immediate_str = parts[3].strip(',').strip()

        rd_match = re.search(r'x(\d+)', rd_str)
        rs1_match = re.search(r'x(\d+)', rs1_str)

        if not (rd_match and rs1_match):
            print(f"Invalid register format in {instr}")
            return None

        rd = int(rd_match.group(1))
        rs1 = int(rs1_match.group(1))

        # Extract immediate value
        immediate_str = immediate_str.strip()
        if immediate_str.startswith('0x'):
            immediate = int(immediate_str, 16)
        elif immediate_str.isdigit() or (immediate_str.startswith('-') and immediate_str[1:].isdigit()):
            immediate = int(immediate_str)
        else:
            print(f"Invalid immediate value in {instr}")
            return None

        # For addi instructions that might be used for memory addressing, shift to start from 16 and above for HDLb0 emulator compatibility
        # Only adjust if the immediate looks like a memory address (positive and small enough to be an address)
        # This is a heuristic - if immediate is 0-15, we assume it's a memory address that needs adjustment
        if 0 <= immediate <= 15:
            adjusted_immediate = immediate + 16
        else:
            adjusted_immediate = immediate  # Don't adjust other immediate values

        # For simplicity, use a custom 16-bit format
        # [15:13] opcode [12:10] rs1 [9:4] immediate [3:1] funct3 [0] unused
        funct3 = 0  # addi
        imm_part = adjusted_immediate & 0x3F  # Use lower 6 bits of adjusted immediate

        # Limit registers to 3 bits (0-7) for our simplified format
        rs1 = rs1 & 0x7
        rd = rd & 0x7

        return ((opcode & 0x7) << 13) | ((rs1 & 0x7) << 10) | ((imm_part & 0x3F) << 4) | ((funct3 & 0x7) << 1) | (0 & 0x1)
    
    elif opcode_str in ['lw', 'sw']:
        # Load/Store: rd, offset(rs1) or rs2, offset(rs1)
        # Format: lw rd, offset(rs1) or sw rs2, offset(rs1)
        if len(parts) < 3:
            print(f"Invalid format for {opcode_str}: {instr}")
            return None

        # Extract operands
        operand1_str = parts[1].strip(',').strip()
        operand2_str = parts[2].strip(',').strip()

        # For load: operand1 is rd, operand2 is offset(rs1)
        # For store: operand1 is rs2, operand2 is offset(rs1)

        # Parse the second operand to extract offset and base register
        match = re.match(r'(\d+)\s*\(\s*x(\d+)\s*\)', operand2_str)
        if not match:
            print(f"Invalid memory format in {instr}")
            return None

        offset = int(match.group(1))
        base_reg = int(match.group(2))

        # Shift memory address to start from 16 and above for HDLb0 emulator compatibility
        # Addresses 0-15 are reserved for special purposes
        adjusted_offset = offset + 16

        # Extract destination/source register
        reg_match = re.search(r'x(\d+)', operand1_str)
        if not reg_match:
            print(f"Invalid register format in {instr}")
            return None

        reg = int(reg_match.group(1))

        # For simplicity, use a custom 16-bit format
        # [15:13] opcode [12:10] rs1 [9:7] rs2 [6:4] rd [3:0] immediate[3:0]
        funct3_map = {'lw': 1, 'sw': 2}
        funct3 = funct3_map.get(opcode_str, 0)

        # For lw: rs1 is base, rd is destination
        # For sw: rs1 is base, rs2 is source
        rs1 = base_reg & 0x7  # base register
        if opcode_str == 'lw':
            rd = reg & 0x7    # destination register
            rs2 = 0           # unused for load
        else:  # sw
            rs2 = reg & 0x7   # source register
            rd = 0            # unused for store

        imm_part = adjusted_offset & 0xF  # Use lower 4 bits of adjusted offset

        return ((opcode & 0x7) << 13) | ((rs1 & 0x7) << 10) | ((rs2 & 0x7) << 7) | ((rd & 0x7) << 4) | (imm_part & 0xF)
    
    elif opcode_str in ['beq', 'bne', 'blt', 'bge']:
        # B-type: opcode rs1 rs2 funct3 immediate (simplified 16-bit format)
        # Format: beq rs1, rs2, label
        if len(parts) < 4:
            print(f"Invalid format for {opcode_str}: {instr}")
            return None
        
        # Extract registers and label
        rs1_str = parts[1].strip(',').strip()
        rs2_str = parts[2].strip(',').strip()
        label_str = parts[3].strip(',').strip()
        
        rs1_match = re.search(r'x(\d+)', rs1_str)
        rs2_match = re.search(r'x(\d+)', rs2_str)
        
        if not (rs1_match and rs2_match):
            print(f"Invalid register format in {instr}")
            return None
        
        rs1 = int(rs1_match.group(1))
        rs2 = int(rs2_match.group(1))
        
        # For simplicity, use a custom 16-bit format
        # [15:13] opcode [12:10] rs1 [9:7] rs2 [6:0] immediate
        funct3_map = {'beq': 0, 'bne': 1, 'blt': 2, 'bge': 3}
        funct3 = funct3_map.get(opcode_str, 0)
        
        # Use a placeholder immediate value (labels would be resolved in a full assembler)
        immediate = 0
        
        # Limit registers to 3 bits (0-7) for our simplified format
        rs1 = rs1 & 0x7
        rs2 = rs2 & 0x7
        
        return ((opcode & 0x7) << 13) | ((rs1 & 0x7) << 10) | ((rs2 & 0x7) << 7) | (immediate & 0x7F)
    
    elif opcode_str in ['jal', 'j']:
        # J-type: opcode rd immediate (simplified 16-bit format)
        # Format: jal rd, immediate  OR  j immediate
        if opcode_str == 'j':
            # Unconditional jump: j target
            if len(parts) < 2:
                print(f"Invalid format for {opcode_str}: {instr}")
                return None
            
            immediate_str = parts[1].strip(',').strip()
            
            # Extract immediate value or label
            immediate_str = immediate_str.strip()
            if immediate_str.startswith('0x'):
                immediate = int(immediate_str, 16)
            elif immediate_str.isdigit() or (immediate_str.startswith('-') and immediate_str[1:].isdigit()):
                immediate = int(immediate_str)
            else:
                # It's a label, use placeholder
                immediate = 0  # Labels would be resolved in a full assembler
        else:  # jal
            # Jal: jal rd, immediate
            if len(parts) < 3:
                print(f"Invalid format for {opcode_str}: {instr}")
                return None
            
            rd_str = parts[1].strip(',').strip()
            immediate_str = parts[2].strip(',').strip()
            
            rd_match = re.search(r'x(\d+)', rd_str)
            if not rd_match:
                print(f"Invalid register format in {instr}")
                return None
            
            rd = int(rd_match.group(1))
            
            # Extract immediate value or label
            immediate_str = immediate_str.strip()
            if immediate_str.startswith('0x'):
                immediate = int(immediate_str, 16)
            elif immediate_str.isdigit() or (immediate_str.startswith('-') and immediate_str[1:].isdigit()):
                immediate = int(immediate_str)
            else:
                # It's a label, use placeholder
                immediate = 0  # Labels would be resolved in a full assembler
        
        # For simplicity, use a custom 16-bit format
        # [15:13] opcode [12:7] immediate [6:4] rd [3:0] unused
        imm_part = immediate & 0x3F  # Use lower 6 bits of immediate
        
        # Limit registers to 3 bits (0-7) for our simplified format
        if opcode_str == 'j':
            rd = 0  # x0 register for j instruction
        rd = rd & 0x7
        
        return ((opcodes[opcode_str] & 0x7) << 13) | ((imm_part & 0x3F) << 7) | ((rd & 0x7) << 4) | (0 & 0xF)
    
    elif opcode_str == 'jalr':
        # Jalr: jalr rd, rs1, immediate
        if len(parts) < 4:
            print(f"Invalid format for {opcode_str}: {instr}")
            return None
        
        rd_str = parts[1].strip(',').strip()
        rs1_str = parts[2].strip(',').strip()
        immediate_str = parts[3].strip(',').strip()
        
        rd_match = re.search(r'x(\d+)', rd_str)
        rs1_match = re.search(r'x(\d+)', rs1_str)
        
        if not (rd_match and rs1_match):
            print(f"Invalid register format in {instr}")
            return None
        
        rd = int(rd_match.group(1))
        rs1 = int(rs1_match.group(1))
        
        # Extract immediate value
        immediate_str = immediate_str.strip()
        if immediate_str.startswith('0x'):
            immediate = int(immediate_str, 16)
        elif immediate_str.isdigit() or (immediate_str.startswith('-') and immediate_str[1:].isdigit()):
            immediate = int(immediate_str)
        else:
            # It's a label, use placeholder
            immediate = 0  # Labels would be resolved in a full assembler
        
        # For simplicity, use a custom 16-bit format
        # [15:13] opcode [12:10] rs1 [9:4] immediate [3:1] funct3 [0] unused
        funct3 = 0  # jalr
        imm_part = immediate & 0x3F  # Use lower 6 bits of immediate
        
        # Limit registers to 3 bits (0-7) for our simplified format
        rs1 = rs1 & 0x7
        rd = rd & 0x7
        
        return ((opcode & 0x7) << 13) | ((rs1 & 0x7) << 10) | ((imm_part & 0x3F) << 4) | ((funct3 & 0x7) << 1) | (0 & 0x1)
    
    elif opcode_str == 'li':
        # Pseudo-instruction: li rd, immediate
        # Convert to addi rd, x0, immediate
        if len(parts) < 3:
            print(f"Invalid format for {opcode_str}: {instr}")
            return None

        rd_str = parts[1].strip(',').strip()
        immediate_str = parts[2].strip(',').strip()

        rd_match = re.search(r'x(\d+)', rd_str)
        if not rd_match:
            print(f"Invalid register format in {instr}")
            return None

        rd = int(rd_match.group(1))

        # Extract immediate value
        immediate_str = immediate_str.strip()
        if immediate_str.startswith('0x'):
            immediate = int(immediate_str, 16)
        elif immediate_str.isdigit() or (immediate_str.startswith('-') and immediate_str[1:].isdigit()):
            immediate = int(immediate_str)
        else:
            print(f"Invalid immediate value in {instr}")
            return None

        # For li instructions that load addresses, shift to start from 16 and above for HDLb0 emulator compatibility
        # Only adjust if the immediate looks like a memory address (positive and small enough to be an address)
        # This is a heuristic - if immediate is 0-15, we assume it's a memory address that needs adjustment
        if 0 <= immediate <= 15:
            adjusted_immediate = immediate + 16
        else:
            adjusted_immediate = immediate  # Don't adjust other immediate values

        # Convert to addi rd, x0, immediate
        # [15:13] opcode [12:10] rs1 [9:4] immediate [3:1] funct3 [0] unused
        funct3 = 0  # addi
        imm_part = adjusted_immediate & 0x3F  # Use lower 6 bits of immediate

        # Limit registers to 3 bits (0-7) for our simplified format
        rd = rd & 0x7

        # Use addi opcode for li
        addi_opcode = 0b010  # Using addi opcode for li

        return ((addi_opcode & 0x7) << 13) | (0 << 10) | ((imm_part & 0x3F) << 4) | ((funct3 & 0x7) << 1) | (0 & 0x1)
    
    elif opcode_str == 'mv':
        # Pseudo-instruction: mv rd, rs
        # Convert to add rd, rs, x0
        if len(parts) < 3:
            print(f"Invalid format for {opcode_str}: {instr}")
            return None
        
        rd_str = parts[1].strip(',').strip()
        rs_str = parts[2].strip(',').strip()
        
        rd_match = re.search(r'x(\d+)', rd_str)
        rs_match = re.search(r'x(\d+)', rs_str)
        
        if not (rd_match and rs_match):
            print(f"Invalid register format in {instr}")
            return None
        
        rd = int(rd_match.group(1))
        rs = int(rs_match.group(1))
        
        # Convert to add rd, rs, x0
        # [15:13] opcode [12:10] rs1 [9:7] rs2 [6:4] rd [3:0] funct3
        funct3 = 0  # add
        
        # Limit registers to 3 bits (0-7) for our simplified format
        rd = rd & 0x7
        rs1 = rs & 0x7
        rs2 = 0  # x0 register
        
        # Use add opcode for mv
        add_opcode = 0b000  # Using add opcode for mv
        
        return ((add_opcode & 0x7) << 13) | ((rs1 & 0x7) << 10) | ((rs2 & 0x7) << 7) | ((rd & 0x7) << 4) | (funct3 & 0xF)
    
    elif opcode_str == 'la':
        # Pseudo-instruction: la rd, address
        # For our simplified processor, treat as load immediate
        if len(parts) < 3:
            print(f"Invalid format for {opcode_str}: {instr}")
            return None

        rd_str = parts[1].strip(',').strip()
        address_str = parts[2].strip(',').strip()

        rd_match = re.search(r'x(\d+)', rd_str)
        if not rd_match:
            print(f"Invalid register format in {instr}")
            return None

        rd = int(rd_match.group(1))

        # Extract address value (could be a label or immediate)
        if address_str.startswith('0x'):
            address = int(address_str, 16)
        elif address_str.isdigit() or (address_str.startswith('-') and address_str[1:].isdigit()):
            address = int(address_str)
        else:
            # It's a label, use placeholder
            address = 0  # Labels would be resolved in a full assembler

        # Shift memory address to start from 16 and above for HDLb0 emulator compatibility
        # Addresses 0-15 are reserved for special purposes
        if address >= 0:  # Only adjust positive addresses
            adjusted_address = address + 16
        else:
            adjusted_address = address  # Don't adjust negative addresses

        # Convert to addi rd, x0, address (load address)
        # [15:13] opcode [12:10] rs1 [9:4] immediate [3:1] funct3 [0] unused
        funct3 = 0  # addi
        imm_part = adjusted_address & 0x3F  # Use lower 6 bits of adjusted address

        # Limit registers to 3 bits (0-7) for our simplified format
        rd = rd & 0x7

        # Use addi opcode for la
        addi_opcode = 0b010  # Using addi opcode for la

        return ((addi_opcode & 0x7) << 13) | (0 << 10) | ((imm_part & 0x3F) << 4) | ((funct3 & 0x7) << 1) | (0 & 0x1)
    
    elif opcode_str == 'ecall':
        # System call instruction
        # [15:0] opcode (simplified)
        return 0x7000  # Placeholder for ecall
    
    else:
        print(f"Unsupported instruction: {opcode_str}")
        return None

def assemble_file(input_file, output_file):
    """Assemble an input file to HDLb0 binary format"""
    with open(input_file, 'r') as f:
        lines = f.readlines()
    
    binary_lines = []
    
    for line_num, line in enumerate(lines, 1):
        # Parse instruction
        binary = parse_instruction(line)
        if binary is not None:
            # Format as 16-bit hex
            binary_lines.append(f"{binary:04X}")
        # Skip lines that couldn't be parsed (comments, labels, etc.)
    
    # Write to output file
    with open(output_file, 'w') as f:
        for binary in binary_lines:
            f.write(binary + '\n')

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 assembler.py <input_asm_file> <output_bin_file>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    print(f"Assembling {input_file} to {output_file}")
    assemble_file(input_file, output_file)
    print("Assembly completed!")

if __name__ == "__main__":
    main()