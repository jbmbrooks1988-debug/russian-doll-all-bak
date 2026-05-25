# "Hello World" equivalent program for 2-bit RISC-V processor
# Demonstrates basic functionality of the processor

# Initialize registers with values (simulated)
# x1 will hold our "output" value
# x2 and x3 will hold operands for operations

# Perform a sequence of operations to demonstrate functionality
# Add operation
add x1, x2, x3

# Store result
sw x1, 0(x0)

# Perform AND operation
and x1, x2, x3

# Store result
sw x1, 1(x0)

# Perform OR operation
or x1, x2, x3

# Store result
sw x1, 2(x0)

# Load and combine results
lw x2, 0(x0)  # Load addition result
lw x3, 1(x0)  # Load AND result

# Combine results
add x1, x2, x3

# Store final result
sw x1, 3(x0)

# This would be our "Hello World" equivalent:
# A program that demonstrates the basic operations of the processor
# and shows that it can perform calculations and store/retrieve data

# End program (halt)
halt:
    j halt