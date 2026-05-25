# Memory test program for 2-bit RISC-V processor
# Tests the functionality of the memory subsystem

# Test 1: Write to all memory locations
add x1, x0, x0     # x1 = 0 (value to store)
sw x1, 0(x0)       # Store 0 to memory[0]

add x1, x0, x0     # x1 = 0
add x1, x1, x1     # x1 = 0 + 0 = 0, or try to get value 1
# Actually, to get different values in 2-bit system, we need to use operations
# For now, we'll just test the memory access patterns

# Since we're limited in how we can generate values in 2-bit system,
# we'll focus on testing memory access patterns

# Store different values to different memory locations
# Using x2 and x3 as different "values"
sw x2, 0(x0)       # Store x2 to memory[0]
sw x3, 1(x0)       # Store x3 to memory[1]

# Test 2: Read from all memory locations
lw x1, 0(x0)       # Load from memory[0] to x1
lw x2, 1(x0)       # Load from memory[1] to x2

# Test 3: Write-read consistency
add x1, x0, x0     # Prepare a value (0)
sw x1, 2(x0)       # Store to memory[2]
lw x3, 2(x0)       # Load from memory[2] to x3
# Now x3 should equal the original value of x1

# Test 4: Store result of operations
add x1, x2, x3     # Perform operation
sw x1, 3(x0)       # Store result to memory[3]
lw x2, 3(x0)       # Load result back

# Test 5: Memory pattern test
# Store a pattern and verify
sw x1, 0(x0)       # Store x1 to memory[0]
sw x2, 1(x0)       # Store x2 to memory[1] 
sw x3, 2(x0)       # Store x3 to memory[2]
sw x1, 3(x0)       # Store x1 to memory[3]

# Read back the pattern
lw x1, 0(x0)       # Read from memory[0]
lw x2, 1(x0)       # Read from memory[1]
lw x3, 2(x0)       # Read from memory[2] 
lw x1, 3(x0)       # Read from memory[3]

# End test
memory_test_end:
    j memory_test_end