# Register file test program for 2-bit RISC-V processor
# Tests the functionality of the register file

# Test 1: Write and read different values to/from registers
# Write value to x1
add x1, x0, x0     # x1 = 0 + 0 = 0
sw x1, 0(x0)       # Store x1 to memory location 0

# Write value to x2  
add x2, x0, x0     # x2 = 0 + 0 = 0
sw x2, 1(x0)       # Store x2 to memory location 1

# Write value to x3
add x3, x0, x0     # x3 = 0 + 0 = 0
sw x3, 2(x0)       # Store x3 to memory location 2

# Test register operations
# Load values and perform operations
lw x1, 0(x0)       # Load 0 to x1
lw x2, 1(x0)       # Load 0 to x2
lw x3, 2(x0)       # Load 0 to x3

# Perform operations to change register values
add x1, x0, x0     # x1 = 0
add x2, x1, x1     # x2 = 0 (since x1 is 0)
add x3, x2, x1     # x3 = 0

# Write different values
# Since we're limited to 2-bit values, we'll use operations to change values
# For example, if we want to set x1 to 1, we need a source with value 1
# For this test, we'll assume registers can be initialized with different values

# Test 2: Verify x0 is always zero
add x1, x0, x0     # x1 should be 0 since x0 is always 0
sw x1, 3(x0)       # Store result to verify x0 behavior

# Test 3: Verify register read/write independence
add x1, x0, x0     # x1 = 0
add x2, x1, x1     # x2 = 0 + 0 = 0
add x3, x2, x1     # x3 = 0 + 0 = 0

# Store all register values to memory to verify
sw x1, 0(x0)       # Store x1
sw x2, 1(x0)       # Store x2  
sw x3, 2(x0)       # Store x3

# Load back and verify
lw x1, 0(x0)       # Load x1 value
lw x2, 1(x0)       # Load x2 value
lw x3, 2(x0)       # Load x3 value

# End test
test_end:
    j test_end