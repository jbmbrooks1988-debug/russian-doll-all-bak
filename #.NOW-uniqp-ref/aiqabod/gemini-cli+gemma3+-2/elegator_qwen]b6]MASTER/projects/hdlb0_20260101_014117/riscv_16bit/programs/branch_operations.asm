// Branch operations test program for 16-bit RISC-V processor
// This program tests conditional branching

// Initialize registers
li x1, 10          // Value 1
li x2, 5           // Value 2
li x3, 0           // Counter
li x4, 0           // Result accumulator

// Loop that runs 5 times
loop:
    add x4, x4, x1   // Add x1 to accumulator
    addi x3, x3, 1   // Increment counter
    li x5, 5         // Load loop limit
    blt x3, x5, loop // Branch if counter < 5

// Conditional operations
li x6, 15          // Another value
li x7, 20          // Yet another value

// Compare and branch
bne x1, x2, not_equal  // Branch if x1 != x2
    // If we reach here, x1 == x2 (this shouldn't happen in our case)
    li x8, 1
    j end_compare
not_equal:
    // x1 != x2, so do something else
    li x8, 0

end_compare:
    // Test greater than
    blt x6, x7, x6_less_than_x7  // Branch if x6 < x7
    // x6 >= x7
    li x9, 0
    j after_comparison
x6_less_than_x7:
    // x6 < x7
    li x9, 1

after_comparison:
    // Test equality
    beq x8, x9, equal_result
    // Results are different
    li x10, 0
    j finish
equal_result:
    // Results are the same
    li x10, 1

finish:
    // Store final results
    sw x4, 0(x0)   // Store loop result
    sw x8, 2(x0)   // Store comparison result
    sw x9, 4(x0)   // Store second comparison result
    sw x10, 6(x0)  // Store final comparison

    // End program
    li x17, 0      // Load exit syscall number
    ecall          // Make system call to exit