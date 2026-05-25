# ALU test program for 2-bit RISC-V processor
# Tests the functionality of the ALU

# Test 1: Addition operations
# Assume x2 and x3 have initial values
add x1, x2, x3     # x1 = x2 + x3
sw x1, 0(x0)       # Store result

# Test 2: Subtraction operations
sub x1, x2, x3     # x1 = x2 - x3
sw x1, 1(x0)       # Store result

# Test 3: AND operations
and x1, x2, x3     # x1 = x2 AND x3
sw x1, 2(x0)       # Store result

# Test 4: OR operations
or x1, x2, x3      # x1 = x2 OR x3
sw x1, 3(x0)       # Store result

# Test 5: XOR operations
xor x1, x2, x3     # x1 = x2 XOR x3
sw x1, 0(x1)       # Store result (using x1 as offset)

# Test 6: Various combinations
# Load previous results and perform more operations
lw x2, 0(x0)       # Load addition result
lw x3, 2(x0)       # Load AND result
add x1, x2, x3     # Add the results
sw x1, 4(x0)       # Store result

# Test 7: Edge cases with 2-bit values
# In 2-bit system, values are 0, 1, 2, 3
# Test wraparound behavior if applicable
add x1, x2, x2     # x1 = x2 + x2 (may cause overflow in 2-bit)

# End test
alu_test_end:
    j alu_test_end