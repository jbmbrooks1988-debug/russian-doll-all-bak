#!/usr/bin/env python3
"""
Enhanced Verilog to HDLb0 Binary Format Converter

This script converts Verilog descriptions to HDLb0 format.
It handles both gate-level Verilog and can work with behavioral Verilog
by mapping high-level constructs to equivalent gate-level implementations.

HDLb0 format consists of 4-part instructions:
1. Chip Location (16-bit binary) - specifies which chip to use (0 for pass-through, 1 for NAND, etc.)
2. Output Address (16-bit binary) - where to store the result
3. Input A (16-bit binary) - first input source
4. Input B (16-bit binary) - second input source
"""

import re
import sys
from typing import Dict, List, Tuple, Optional


class EnhancedVerilogToHDLB0Converter:
    def __init__(self):
        # Map of available chips (for now, just NAND as base)
        self.chips = {
            'nand_gate': 1,  # NAND chip
            'pass_through': 0  # Pass-through chip
        }
        
        # Memory address allocation
        self.next_address = 16  # Start from address 16 as per HDLb0 spec
        self.address_map = {}  # Map of wire names to memory addresses
        self.instructions = []  # List of HDLb0 instructions
        
        # Input mapping (as per HDLb0 API)
        self.input_map = {
            'switch_0': '0000000000000101',  # 5
            'switch_1': '0000000000000110',  # 6
            'clock': '0000000000000111',     # 7
        }
        
        # Constants
        self.constants = {
            '0': '0000000000000000',  # 0
            '1': '0000000000000001',  # 1
        }

    def allocate_address(self, wire_name: str) -> str:
        """Allocate a memory address for a wire and return its binary representation."""
        if wire_name not in self.address_map:
            self.address_map[wire_name] = self.next_address
            self.next_address += 1
        
        address = self.address_map[wire_name]
        return format(address, '016b')

    def get_input_binary(self, input_name: str) -> str:
        """Get binary representation for an input."""
        # Check if it's a constant
        if input_name in self.constants:
            return self.constants[input_name]
        
        # Check if it's a hardware input
        if input_name in self.input_map:
            return self.input_map[input_name]
        
        # Check if it's a wire in memory (RAM)
        if input_name in self.address_map:
            return format(self.address_map[input_name], '016b')
        
        # If not found, allocate a new address
        return self.allocate_address(input_name)

    def convert_nand_gate(self, inputs: List[str], output: str) -> List[str]:
        """Convert a NAND gate to HDLb0 instructions."""
        if len(inputs) != 2:
            raise ValueError("NAND gate must have exactly 2 inputs")
        
        input_a_bin = self.get_input_binary(inputs[0])
        input_b_bin = self.get_input_binary(inputs[1])
        output_bin = self.allocate_address(output)
        
        # NAND gate: chip 1 (NAND), output to specified address, inputs A and B
        instruction = [
            format(self.chips['nand_gate'], '016b'),  # NAND chip
            output_bin,  # Output address
            input_a_bin,  # Input A
            input_b_bin   # Input B
        ]
        
        return [' '.join(instruction)]

    def convert_not_gate(self, inputs: List[str], output: str) -> List[str]:
        """Convert a NOT gate to HDLb0 instructions using NAND gates."""
        if len(inputs) != 1:
            raise ValueError("NOT gate must have exactly 1 input")
        
        # NOT gate = NAND gate with both inputs tied together
        input_bin = self.get_input_binary(inputs[0])
        output_bin = self.allocate_address(output)
        
        # NAND gate: chip 1 (NAND), output to specified address, both inputs same
        instruction = [
            format(self.chips['nand_gate'], '016b'),  # NAND chip
            output_bin,  # Output address
            input_bin,   # Input A
            input_bin    # Input B (same as A)
        ]
        
        return [' '.join(instruction)]

    def convert_and_gate(self, inputs: List[str], output: str) -> List[str]:
        """Convert an AND gate to HDLb0 instructions using NAND gates."""
        if len(inputs) != 2:
            raise ValueError("AND gate must have exactly 2 inputs")
        
        # AND gate = NAND gate followed by NOT gate (another NAND with inputs tied)
        temp_wire = f"temp_and_{output}"
        
        # First, create NAND of inputs
        input_a_bin = self.get_input_binary(inputs[0])
        input_b_bin = self.get_input_binary(inputs[1])
        temp_bin = self.allocate_address(temp_wire)
        
        nand_instruction = [
            format(self.chips['nand_gate'], '016b'),  # NAND chip
            temp_bin,      # Output to temp wire
            input_a_bin,   # Input A
            input_b_bin    # Input B
        ]
        
        # Then, create NOT of the NAND result (which gives AND)
        output_bin = self.allocate_address(output)
        not_instruction = [
            format(self.chips['nand_gate'], '016b'),  # NAND chip (as NOT)
            output_bin,  # Output address
            temp_bin,    # Input A (from NAND)
            temp_bin     # Input B (same as A, for NOT)
        ]
        
        return [' '.join(nand_instruction), ' '.join(not_instruction)]

    def convert_or_gate(self, inputs: List[str], output: str) -> List[str]:
        """Convert an OR gate to HDLb0 instructions using NAND gates."""
        if len(inputs) != 2:
            raise ValueError("OR gate must have exactly 2 inputs")
        
        # OR gate using De Morgan's law: A OR B = NOT((NOT A) AND (NOT B))
        # In NAND terms: OR = NAND(NAND(A,A), NAND(B,B))
        temp_not_a = f"temp_not_a_{output}"
        temp_not_b = f"temp_not_b_{output}"
        
        # NOT A = NAND(A, A)
        input_a_bin = self.get_input_binary(inputs[0])
        not_a_bin = self.allocate_address(temp_not_a)
        nand_a_instruction = [
            format(self.chips['nand_gate'], '016b'),  # NAND chip
            not_a_bin,     # Output to temp_not_a
            input_a_bin,   # Input A
            input_a_bin    # Input B (same as A)
        ]
        
        # NOT B = NAND(B, B)
        input_b_bin = self.get_input_binary(inputs[1])
        not_b_bin = self.allocate_address(temp_not_b)
        nand_b_instruction = [
            format(self.chips['nand_gate'], '016b'),  # NAND chip
            not_b_bin,     # Output to temp_not_b
            input_b_bin,   # Input A
            input_b_bin    # Input B (same as A)
        ]
        
        # OR = NAND(NOT A, NOT B)
        output_bin = self.allocate_address(output)
        or_instruction = [
            format(self.chips['nand_gate'], '016b'),  # NAND chip
            output_bin,    # Output address
            not_a_bin,     # Input A (NOT A)
            not_b_bin      # Input B (NOT B)
        ]
        
        return [
            ' '.join(nand_a_instruction),
            ' '.join(nand_b_instruction),
            ' '.join(or_instruction)
        ]

    def convert_xor_gate(self, inputs: List[str], output: str) -> List[str]:
        """Convert an XOR gate to HDLb0 instructions using NAND gates."""
        if len(inputs) != 2:
            raise ValueError("XOR gate must have exactly 2 inputs")
        
        # XOR = (A AND NOT B) OR (NOT A AND B)
        # Using NAND gates, this requires multiple steps
        
        # Create temporary wires
        temp_not_a = f"temp_not_a_{output}"
        temp_not_b = f"temp_not_b_{output}"
        temp_a_and_not_b = f"temp_a_and_not_b_{output}"
        temp_not_a_and_b = f"temp_not_a_and_b_{output}"
        
        input_a_bin = self.get_input_binary(inputs[0])
        input_b_bin = self.get_input_binary(inputs[1])
        
        instructions = []
        
        # NOT A = NAND(A, A)
        not_a_bin = self.allocate_address(temp_not_a)
        instructions.append(' '.join([
            format(self.chips['nand_gate'], '016b'),  # NAND chip
            not_a_bin,     # Output to temp_not_a
            input_a_bin,   # Input A
            input_a_bin    # Input B (same as A)
        ]))
        
        # NOT B = NAND(B, B)
        not_b_bin = self.allocate_address(temp_not_b)
        instructions.append(' '.join([
            format(self.chips['nand_gate'], '016b'),  # NAND chip
            not_b_bin,     # Output to temp_not_b
            input_b_bin,   # Input A
            input_b_bin    # Input B (same as A)
        ]))
        
        # A AND NOT B (using NAND + NOT)
        temp_a_nand_not_b = f"temp_a_nand_not_b_{output}"
        a_nand_not_b_bin = self.allocate_address(temp_a_nand_not_b)
        instructions.append(' '.join([
            format(self.chips['nand_gate'], '016b'),  # NAND chip
            a_nand_not_b_bin,  # Output to temp_a_nand_not_b
            input_a_bin,       # Input A
            not_b_bin          # Input B (NOT B)
        ]))
        
        a_and_not_b_bin = self.allocate_address(temp_a_and_not_b)
        instructions.append(' '.join([
            format(self.chips['nand_gate'], '016b'),  # NAND chip (as NOT)
            a_and_not_b_bin,   # Output to temp_a_and_not_b
            a_nand_not_b_bin,  # Input A
            a_nand_not_b_bin   # Input B (same as A, for NOT)
        ]))
        
        # NOT A AND B (using NAND + NOT)
        temp_not_a_nand_b = f"temp_not_a_nand_b_{output}"
        not_a_nand_b_bin = self.allocate_address(temp_not_a_nand_b)
        instructions.append(' '.join([
            format(self.chips['nand_gate'], '016b'),  # NAND chip
            not_a_nand_b_bin,  # Output to temp_not_a_nand_b
            not_a_bin,         # Input A (NOT A)
            input_b_bin        # Input B
        ]))
        
        not_a_and_b_bin = self.allocate_address(temp_not_a_and_b)
        instructions.append(' '.join([
            format(self.chips['nand_gate'], '016b'),  # NAND chip (as NOT)
            not_a_and_b_bin,   # Output to temp_not_a_and_b
            not_a_nand_b_bin,  # Input A
            not_a_nand_b_bin   # Input B (same as A, for NOT)
        ]))
        
        # (A AND NOT B) OR (NOT A AND B) - using OR implementation
        # OR = NAND(NAND(X,X), NAND(Y,Y)) where X=(A AND NOT B) and Y=(NOT A AND B)
        temp_or1 = f"temp_or1_{output}"
        temp_or2 = f"temp_or2_{output}"
        
        or1_bin = self.allocate_address(temp_or1)
        instructions.append(' '.join([
            format(self.chips['nand_gate'], '016b'),  # NAND chip
            or1_bin,           # Output to temp_or1
            a_and_not_b_bin,   # Input A
            a_and_not_b_bin    # Input B (same as A, for NOT)
        ]))
        
        or2_bin = self.allocate_address(temp_or2)
        instructions.append(' '.join([
            format(self.chips['nand_gate'], '016b'),  # NAND chip
            or2_bin,           # Output to temp_or2
            not_a_and_b_bin,   # Input A
            not_a_and_b_bin    # Input B (same as A, for NOT)
        ]))
        
        output_bin = self.allocate_address(output)
        instructions.append(' '.join([
            format(self.chips['nand_gate'], '016b'),  # NAND chip (final OR)
            output_bin,        # Output address
            or1_bin,           # Input A (NOT of (A AND NOT B))
            or2_bin            # Input B (NOT of (NOT A AND B))
        ]))
        
        return instructions

    def convert_nor_gate(self, inputs: List[str], output: str) -> List[str]:
        """Convert a NOR gate to HDLb0 instructions using NAND gates."""
        if len(inputs) != 2:
            raise ValueError("NOR gate must have exactly 2 inputs")
        
        # NOR gate = NOT(OR gate) = NOT(NAND(NOT A, NOT B))
        temp_wire = f"temp_nor_{output}"
        
        # First, implement OR gate using NAND gates
        or_instructions = self.convert_or_gate(inputs, temp_wire)
        
        # Then, NOT the OR result to get NOR
        temp_bin = self.get_input_binary(temp_wire)
        output_bin = self.allocate_address(output)
        
        not_instruction = [
            format(self.chips['nand_gate'], '016b'),  # NAND chip (as NOT)
            output_bin,  # Output address
            temp_bin,    # Input A
            temp_bin     # Input B (same as A, for NOT)
        ]
        
        # Add the NOT instruction to the OR implementation
        or_instructions.append(' '.join(not_instruction))
        
        return or_instructions

    def convert_xnor_gate(self, inputs: List[str], output: str) -> List[str]:
        """Convert an XNOR gate to HDLb0 instructions using NAND gates."""
        if len(inputs) != 2:
            raise ValueError("XNOR gate must have exactly 2 inputs")
        
        # XNOR = NOT(XOR) = (A AND B) OR (NOT A AND NOT B)
        temp_wire = f"temp_xnor_{output}"
        
        # First, implement XOR gate using NAND gates
        xor_instructions = self.convert_xor_gate(inputs, temp_wire)
        
        # Then, NOT the XOR result to get XNOR
        temp_bin = self.get_input_binary(temp_wire)
        output_bin = self.allocate_address(output)
        
        not_instruction = [
            format(self.chips['nand_gate'], '016b'),  # NAND chip (as NOT)
            output_bin,  # Output address
            temp_bin,    # Input A
            temp_bin     # Input B (same as A, for NOT)
        ]
        
        # Add the NOT instruction to the XOR implementation
        xor_instructions.append(' '.join(not_instruction))
        
        return xor_instructions

    def parse_verilog_module(self, verilog_code: str) -> List[Dict]:
        """Parse a Verilog module and extract gate instances and behavioral constructs."""
        gates = []
        
        # First, try to find gate-level instantiations
        gate_types = ['nand_gate', 'not_gate', 'and_gate', 'or_gate', 'xor_gate', 'nor_gate', 'xnor_gate']
        
        for line in verilog_code.split('\n'):
            line = line.strip()
            if not line or line.startswith('//') or line.startswith('module') or line.startswith('endmodule'):
                continue
            
            # Try to match different gate types
            for gate_type in gate_types:
                # Pattern for gate with named ports: gate_type .a(input1), .b(input2), .y(output);
                named_port_pattern = rf'{gate_type}\s*\.\w+\s*\(\s*([^,\)]+)\s*\)\s*,\s*\.\w+\s*\(\s*([^,\)]+)\s*\)(?:\s*,\s*\.\w+\s*\(\s*([^,\)]+)\s*\))?.*?(\w+)\s*\(\s*\);?'
                
                match = re.search(named_port_pattern, line)
                if match:
                    groups = match.groups()
                    if gate_type == 'not_gate':
                        # NOT gate has 1 input and 1 output
                        input_val = groups[0].strip()
                        output_val = groups[2] if groups[2] else groups[1]
                        gates.append({
                            'type': gate_type,
                            'inputs': [input_val],
                            'output': output_val
                        })
                    elif gate_type in ['nand_gate']:
                        # NAND gate has 2 inputs and 1 output
                        input1 = groups[0].strip()
                        input2 = groups[1].strip()
                        output_val = groups[2] if len(groups) > 2 and groups[2] else 'unknown_output'
                        gates.append({
                            'type': gate_type,
                            'inputs': [input1, input2],
                            'output': output_val
                        })
                    else:
                        # Other gates have 2 inputs and 1 output
                        input1 = groups[0].strip()
                        input2 = groups[1].strip()
                        output_val = groups[2] if len(groups) > 2 and groups[2] else 'unknown_output'
                        gates.append({
                            'type': gate_type,
                            'inputs': [input1, input2],
                            'output': output_val
                        })
                    break  # Found a match, go to next line
        
        # Alternative pattern for simple gate instantiation: gate_type(input1, input2, output);
        simple_pattern = r'(\w+_gate)\s*\(\s*([^,)]+)\s*,\s*([^,)]+)\s*,\s*([^,)]+)\s*\)\s*;'
        for match in re.finditer(simple_pattern, verilog_code):
            gate_type, input1, input2, output = match.groups()
            if gate_type in gate_types:
                gates.append({
                    'type': gate_type,
                    'inputs': [input1.strip(), input2.strip()],
                    'output': output.strip()
                })
        
        # Handle behavioral constructs (assign statements, etc.)
        # Look for assign statements like: assign out = in1 & in2; (AND), assign out = in1 | in2; (OR), etc.
        assign_pattern = r'assign\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);'
        for match in re.finditer(assign_pattern, verilog_code):
            output = match.group(1).strip()
            expression = match.group(2).strip()
            
            # Handle simple logic operations
            if '&' in expression and expression.count('&') == 1:
                # AND operation: a & b
                parts = expression.split('&')
                input1 = parts[0].strip()
                input2 = parts[1].strip()
                gates.append({
                    'type': 'and_gate',
                    'inputs': [input1, input2],
                    'output': output
                })
            elif '|' in expression and expression.count('|') == 1:
                # OR operation: a | b
                parts = expression.split('|')
                input1 = parts[0].strip()
                input2 = parts[1].strip()
                gates.append({
                    'type': 'or_gate',
                    'inputs': [input1, input2],
                    'output': output
                })
            elif '^' in expression and expression.count('^') == 1:
                # XOR operation: a ^ b
                parts = expression.split('^')
                input1 = parts[0].strip()
                input2 = parts[1].strip()
                gates.append({
                    'type': 'xor_gate',
                    'inputs': [input1, input2],
                    'output': output
                })
            elif '~' in expression and '&(' in expression:
                # NAND operation: ~(a & b)
                inner = re.search(r'~\s*\(\s*([^&]+)\s*&\s*([^)]+)\s*\)', expression)
                if inner:
                    input1 = inner.group(1).strip()
                    input2 = inner.group(2).strip()
                    gates.append({
                        'type': 'nand_gate',
                        'inputs': [input1, input2],
                        'output': output
                    })
            elif '~' in expression and '|(' in expression:
                # NOR operation: ~(a | b)
                inner = re.search(r'~\s*\(\s*([^|]+)\s*\|\s*([^)]+)\s*\)', expression)
                if inner:
                    input1 = inner.group(1).strip()
                    input2 = inner.group(2).strip()
                    gates.append({
                        'type': 'nor_gate',
                        'inputs': [input1, input2],
                        'output': output
                    })
            elif '~' in expression and '^(' in expression:
                # XNOR operation: ~(a ^ b)
                inner = re.search(r'~\s*\(\s*([^ ^]+)\s*\^\s*([^)]+)\s*\)', expression)
                if inner:
                    input1 = inner.group(1).strip()
                    input2 = inner.group(2).strip()
                    gates.append({
                        'type': 'xnor_gate',
                        'inputs': [input1, input2],
                        'output': output
                    })
        
        return gates

    def convert_verilog_to_hdlb0(self, verilog_code: str) -> str:
        """Convert Verilog code to HDLb0 format."""
        self.instructions = []
        self.address_map = {}
        self.next_address = 16
        
        gates = self.parse_verilog_module(verilog_code)
        
        for gate in gates:
            gate_type = gate['type']
            inputs = gate['inputs']
            output = gate['output']
            
            if gate_type == 'nand_gate':
                instructions = self.convert_nand_gate(inputs, output)
            elif gate_type == 'not_gate':
                instructions = self.convert_not_gate(inputs, output)
            elif gate_type == 'and_gate':
                instructions = self.convert_and_gate(inputs, output)
            elif gate_type == 'or_gate':
                instructions = self.convert_or_gate(inputs, output)
            elif gate_type == 'xor_gate':
                instructions = self.convert_xor_gate(inputs, output)
            elif gate_type == 'nor_gate':
                instructions = self.convert_nor_gate(inputs, output)
            elif gate_type == 'xnor_gate':
                instructions = self.convert_xnor_gate(inputs, output)
            else:
                raise ValueError(f"Unsupported gate type: {gate_type}")
            
            self.instructions.extend(instructions)
        
        # Format as HDLb0 instructions
        hdlb0_output = []
        hdlb0_output.append(f"# Converted from Verilog - {len(gates)} gates")
        hdlb0_output.append("#")
        
        for i, instruction in enumerate(self.instructions):
            hdlb0_output.append(f"{instruction}")
        
        return '\n'.join(hdlb0_output)

    def convert_file(self, input_file: str, output_file: str):
        """Convert a Verilog file to HDLb0 format."""
        with open(input_file, 'r') as f:
            verilog_code = f.read()
        
        hdlb0_code = self.convert_verilog_to_hdlb0(verilog_code)
        
        with open(output_file, 'w') as f:
            f.write(hdlb0_code)
        
        print(f"Converted {input_file} to {output_file}")
        print(f"Used {len(self.address_map)} memory addresses")
        print(f"Generated {len(self.instructions)} HDLb0 instructions")


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 enhanced_verilog_to_hdlb0_converter.py <input_verilog_file> <output_hdlb0_file>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    converter = EnhancedVerilogToHDLB0Converter()
    converter.convert_file(input_file, output_file)


if __name__ == "__main__":
    main()