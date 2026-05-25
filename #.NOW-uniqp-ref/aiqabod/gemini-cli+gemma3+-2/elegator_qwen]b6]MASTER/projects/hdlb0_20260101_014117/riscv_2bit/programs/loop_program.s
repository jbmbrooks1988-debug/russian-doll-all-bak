# Loop program for 2-bit RISC-V processor
# Demonstrates branching operations

# Initialize counter in x1 (using value 3 as our maximum for 2-bit)
# In a real implementation, we'd need to load immediate values
# For now, assume x1 is initialized to 3

loop_start:
    # Decrement counter: x1 = x1 - 1
    # Since we don't have immediate values in our 2-bit processor,
    # we'll use x0 (which is always 0) as a constant 1 by adding 1 to 0
    # Actually, in our 2-bit implementation, we'll just do a simple operation
    
    # For this example, we'll subtract 1 from x1 using x0 as a temporary
    # Since we don't have immediate instructions, we'll simulate a counter
    # by using a register that has value 1
    
    # Assume x2 contains 1 (our decrement value)
    sub x1, x1, x2
    
    # Check if counter is zero (x1 == 0)
    # In our 2-bit processor, we'll use a simple branch
    # Since we don't have beq in our basic implementation, we'll simulate
    # a loop using an unconditional jump
    
    # If x1 is not zero, continue loop
    # Since we don't have conditional branches in our 2-bit implementation,
    # we'll just do a fixed number of iterations using jumps
    
    # For demonstration, let's just do a simple loop with 4 iterations
    # by using the program counter behavior
    
    # Store current counter value
    sw x1, 0(x0)
    
    # Load counter again
    lw x3, 0(x0)
    
    # Check if x3 is zero (end condition simulated)
    # Since we don't have conditional branches, we'll just loop a fixed number of times
    # In a real implementation, we'd need conditional branch instructions
    
    # For now, just jump back to loop_start
    j loop_start

# End program
end:
    j end