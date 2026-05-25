# Control unit test program for 2-bit RISC-V processor
# Tests the functionality of the control unit by exercising different instruction types

# The control unit manages:
# - ALU operations (ALUOp)
# - Register write enable
# - Memory access (MemWrite, MemRead)
# - ALU source selection (ALUSrc)
# - PC source selection

# Test 1: R-type instructions (register-register operations)
add x1, x2, x3     # R-type: should set ALUOp to perform addition
sw x1, 0(x0)       # Memory: should set MemWrite

# Test 2: More R-type operations to test ALU control
sub x1, x2, x3     # R-type: should set ALUOp to perform subtraction
and x1, x2, x3     # R-type: should set ALUOp to perform AND
or x1, x2, x3      # R-type: should set ALUOp to perform OR
xor x1, x2, x3     # R-type: should set ALUOp to perform XOR

# Test 3: Memory operations to test memory control signals
sw x1, 1(x0)       # Store word: MemWrite=1, RegWrite=0
lw x2, 1(x0)       # Load word: MemRead=1, RegWrite=1

# Test 4: Complex sequence to test control flow
add x1, x2, x3     # R-type operation
sw x1, 0(x0)       # Store result
lw x2, 0(x0)       # Load result back
sub x3, x1, x2     # Another R-type operation

# Test 5: Verify control signals work together correctly
# Perform a sequence that tests all main control paths
add x1, x2, x3     # R-type (ALU operation)
sw x1, 2(x0)       # Memory write
lw x1, 2(x0)       # Memory read back to register
and x2, x1, x3     # Another R-type operation
or x3, x1, x2      # Another R-type operation
sw x3, 3(x0)       # Final memory write

# End test
control_test_end:
    j control_test_end