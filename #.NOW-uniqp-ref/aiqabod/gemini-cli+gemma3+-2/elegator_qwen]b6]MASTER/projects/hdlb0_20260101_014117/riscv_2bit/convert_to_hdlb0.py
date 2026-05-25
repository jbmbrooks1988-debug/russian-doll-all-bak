#!/usr/bin/env python3
"""
HDLb0 Binary Format Converter Script
This script would convert Verilog files to HDLb0 binary format using the verilog_to_hdlb0_converter.py tool.
"""

import os
import sys
import subprocess
from pathlib import Path

def convert_verilog_to_hdlb0(verilog_file, output_dir):
    """
    Convert a Verilog file to HDLb0 binary format
    """
    print(f"Converting {verilog_file} to HDLb0 binary format...")
    
    # Check if the converter tool exists
    converter_tool = "verilog_to_hdlb0_converter.py"
    
    if not os.path.exists(converter_tool):
        print(f"Warning: {converter_tool} not found. Creating placeholder binary file.")
        # Create a placeholder binary file
        output_file = os.path.join(output_dir, f"{Path(verilog_file).stem}.hdlb0")
        with open(output_file, 'wb') as f:
            f.write(b'HDLB0_BINARY_PLACEHOLDER')
        print(f"Created placeholder: {output_file}")
        return output_file
    
    # If the converter tool exists, run it
    try:
        result = subprocess.run([
            sys.executable, converter_tool, 
            verilog_file, 
            "-o", os.path.join(output_dir, f"{Path(verilog_file).stem}.hdlb0")
        ], check=True, capture_output=True, text=True)
        
        print(f"Successfully converted {verilog_file}")
        return os.path.join(output_dir, f"{Path(verilog_file).stem}.hdlb0")
    except subprocess.CalledProcessError as e:
        print(f"Error converting {verilog_file}: {e}")
        return None

def main():
    # Define source and output directories
    verilog_dir = "riscv_2bit"
    output_dir = "riscv_2bit/hdlb0_output"
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Find all Verilog files in the riscv_2bit directory
    verilog_files = []
    for file in os.listdir(verilog_dir):
        if file.endswith(".sv") or file.endswith(".v"):
            verilog_files.append(os.path.join(verilog_dir, file))
    
    print(f"Found {len(verilog_files)} Verilog files to convert")
    
    # Convert each Verilog file
    for verilog_file in verilog_files:
        output_file = convert_verilog_to_hdlb0(verilog_file, output_dir)
        if output_file:
            print(f"Converted: {output_file}")
        else:
            print(f"Failed to convert: {verilog_file}")

if __name__ == "__main__":
    main()