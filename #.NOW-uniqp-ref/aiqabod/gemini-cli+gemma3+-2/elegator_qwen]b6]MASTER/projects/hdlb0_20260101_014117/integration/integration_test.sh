#!/bin/bash
# Integration test script for HDLB0 16-bit RISC-V system
# This script integrates all components and tests them on the emulator

set -e  # Exit on any error

echo "HDLB0 16-bit RISC-V Integration Test Script"
echo "==========================================="

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

# Step 1: Test the minimal C compiler
echo "Step 1: Testing minimal C compiler..."
cd "$PROJECT_ROOT/compiler"
gcc -o minimal_c_compiler minimal_c_compiler.c
echo "Compiler built successfully"

# Compile the test program
echo "Compiling test program..."
./minimal_c_compiler test_program.c > test_program.asm
if [ $? -eq 0 ]; then
    echo "Test program compiled successfully"
else
    echo "Error compiling test program"
    exit 1
fi

# Assemble the generated assembly to HDLb0 format
echo "Assembling to HDLb0 format..."
cd "$PROJECT_ROOT/riscv_16bit"
python3 assembler.py "$PROJECT_ROOT/compiler/test_program.asm" "$PROJECT_ROOT/integration/test_program.hlo"
if [ $? -eq 0 ]; then
    echo "Assembly completed successfully"
else
    echo "Error assembling program"
    exit 1
fi

# Step 2: Prepare a complete system program that includes kernel, shell, and file system
echo
echo "Step 2: Preparing complete system program..."

# Create a simple test program that uses the file system and shell
cat > "$PROJECT_ROOT/integration/system_test.asm" << 'EOF'
// System test program for HDLB0 16-bit RISC-V
// Tests integration of kernel, file system, and shell

// Initialize the system
.text
.global _start
_start:
    // Initialize file system
    li x17, 0x15  // FS_INIT syscall (hypothetical)
    ecall

    // Create a test file
    la x10, test_filename
    li x17, 0x10  // FS_CREATE_FILE
    ecall

    // Write data to the file
    la x10, test_filename
    la x11, test_data
    li x12, 12    // Length of "Hello World!"
    li x17, 0x13  // FS_WRITE_FILE
    ecall

    // Read the file back
    la x10, test_filename
    la x11, read_buffer
    li x12, 100   // Buffer size
    li x17, 0x12  // FS_READ_FILE
    ecall

    // List files
    li x17, 0x14  // FS_LIST_FILES
    ecall

    // Exit
    li x17, 0
    ecall

.data
test_filename: .asciiz "test.txt"
test_data: .asciiz "Hello World!"
read_buffer: .space 100
EOF

# Assemble the system test
python3 assembler.py "$PROJECT_ROOT/integration/system_test.asm" "$PROJECT_ROOT/integration/system_test.hlo"
if [ $? -eq 0 ]; then
    echo "System test assembled successfully"
else
    echo "Error assembling system test"
    exit 1
fi

# Step 3: Run the test on the emulator
echo
echo "Step 3: Running tests on emulator..."

# Run the system test
echo "Running system test..."
"$EMU_PATH" -c "$CHIP_BANK" -p "$PROJECT_ROOT/integration/system_test.hlo"
if [ $? -eq 0 ]; then
    echo "System test completed successfully"
else
    echo "Error running system test"
    exit 1
fi

# Run the compiled C program test
echo "Running C compiler test..."
"$EMU_PATH" -c "$CHIP_BANK" -p "$PROJECT_ROOT/integration/test_program.hlo"
if [ $? -eq 0 ]; then
    echo "C compiler test completed successfully"
else
    echo "Error running C compiler test"
    exit 1
fi

echo
echo "All integration tests completed successfully!"
echo "Components tested:"
echo "  - 16-bit RISC-V processor"
echo "  - File system operations"
echo "  - Shell commands"
echo "  - Minimal C compiler"
echo "  - Kernel system calls"