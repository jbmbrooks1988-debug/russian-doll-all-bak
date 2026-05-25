#!/bin/bash
# Script to convert all RISC-V assembly programs to HDLb0 format

ASSEMBLER="./riscv_2bit/assembler.py"
PROGRAMS_DIR="./riscv_2bit/programs"
OUTPUT_DIR="./riscv_2bit/hdlb0_output"

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Find all .s files in the programs directory
for asm_file in "$PROGRAMS_DIR"/*.s; do
    if [ -f "$asm_file" ]; then
        # Get the base name without extension
        base_name=$(basename "$asm_file" .s)
        output_file="$OUTPUT_DIR/${base_name}.hlo"
        
        echo "Converting $asm_file to $output_file"
        python3 "$ASSEMBLER" "$asm_file" "$output_file"
    fi
done

echo "All assembly programs converted to HDLb0 format!"