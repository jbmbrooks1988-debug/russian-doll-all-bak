#!/bin/bash
# Demonstration script for HDLB0 16-bit RISC-V OS
# Shows how to compile and run the OS with input/output

set -e  # Exit on any error

echo "HDLB0 16-bit RISC-V OS Demonstration"
echo "====================================="

# Define paths
PROJECT_ROOT="/home/debilu/Desktop/SERVICE.MOD_DEBILU/elegator-local/@.elegator]dec31]2HIP2+/$.debele_projects/$.halo.push]a0/elegator_qwen]b6]MASTER/projects/hdlb0_20260101_014117"
EMU_PATH="/home/debilu/Desktop/SERVICE.MOD_DEBILU/elegator-local/@.elegator]dec31]2HIP2+/$.debele_projects/$.halo.push]a0/elegator_qwen]b6]MASTER/projects/0.HDLb0.PIECE+3.9-/+x/0.hdlb0]22.+x"
CHIP_BANK="$PROJECT_ROOT/chip_bank.txt"

# Check if required files exist
if [ ! -f "$EMU_PATH" ]; then
    echo "Error: Emulator not found at $EMU_PATH"
    exit 1
fi

if [ ! -f "$CHIP_BANK" ]; then
    echo "Error: Chip bank file not found at $CHIP_BANK"
    exit 1
fi

echo "Emulator found: $EMU_PATH"
echo "Chip bank: $CHIP_BANK"
echo

# Step 1: Assemble the demo OS program
echo "Step 1: Assembling the demo OS program..."
cd "$PROJECT_ROOT/riscv_16bit"
python3 assembler.py "$PROJECT_ROOT/demo_os.asm" "$PROJECT_ROOT/demo_os.hlo"
if [ $? -eq 0 ]; then
    echo "Demo OS assembled successfully"
else
    echo "Error assembling demo OS"
    exit 1
fi

# Step 2: Run the demo OS on the emulator
echo
echo "Step 2: Running the demo OS on emulator..."
echo "The OS will demonstrate:"
echo "  - Welcome message output"
echo "  - File system operations (create, write, read, list)"
echo "  - System call interface"
echo

# Run the demo
if [ -f "$PROJECT_ROOT/demo_os.hlo" ]; then
    echo "Executing demo OS..."
    "$EMU_PATH" -c "$CHIP_BANK" -p "$PROJECT_ROOT/demo_os.hlo"
    if [ $? -eq 0 ]; then
        echo "Demo OS executed successfully"
    else
        echo "Error running demo OS"
        exit 1
    fi
else
    echo "Demo OS binary not found"
    exit 1
fi

echo
echo "Demo completed! The OS demonstrated:"
echo "  - Kernel initialization and system startup"
echo "  - Shell interface with prompt"
echo "  - File system operations through system calls"
echo "  - Input/Output operations"
echo "  - Proper integration of all components"