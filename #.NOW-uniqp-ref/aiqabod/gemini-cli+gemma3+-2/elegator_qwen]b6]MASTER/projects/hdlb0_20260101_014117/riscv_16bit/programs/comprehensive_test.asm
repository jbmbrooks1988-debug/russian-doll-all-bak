// Comprehensive test program for 16-bit RISC-V processor
// This program tests multiple features: arithmetic, memory, branches, and system calls

// Initialize registers for computation
li x1, 100         // Starting value
li x2, 25          // Divisor equivalent (we'll use subtraction for division)
li x3, 0           // Counter/quotient
li x4, 0           // Remainder

// Simple division by repeated subtraction: x1 / x2
division_loop:
    blt x1, x2, division_done  // If x1 < x2, division is done
    sub x1, x1, x2             // x1 = x1 - x2
    addi x3, x3, 1             // Increment quotient
    j division_loop
division_done:
    // x3 now contains quotient, x1 contains remainder
    mv x4, x1                  // Move remainder to x4
    mv x1, x3                  // Move quotient back to x1

// Store results in memory
sw x1, 0(x0)       // Store quotient
sw x4, 2(x0)       // Store remainder

// Create a simple array in memory and process it
li x5, 0x3000      // Base address for array
li x6, 1           // Array element 0
li x7, 2           // Array element 1
li x8, 3           // Array element 2
li x9, 4           // Array element 3
li x10, 5          // Array element 4

// Store array elements
sw x6, 0(x5)       // array[0] = 1
sw x7, 2(x5)       // array[1] = 2
sw x8, 4(x5)       // array[2] = 3
sw x9, 6(x5)       // array[3] = 4
sw x10, 8(x5)      // array[4] = 5

// Calculate sum of array elements
li x11, 0          // Sum accumulator
li x12, 0          // Index
li x13, 5          // Array size

sum_loop:
    bge x12, x13, sum_done     // If index >= size, done
    // Calculate address: base + index*2 (since each element is 2 bytes)
    slli x14, x12, 1           // x14 = index * 2
    add x14, x5, x14           // x14 = base + offset
    lw x15, 0(x14)             // Load array[index]
    add x11, x11, x15          // Add to sum
    addi x12, x12, 1           // Increment index
    j sum_loop
sum_done:
    // x11 now contains the sum of array elements

// Store sum in memory
sw x11, 10(x5)     // Store sum at array+5

// Test conditional operations with the sum
li x16, 15         // Threshold value
blt x11, x16, below_threshold
    // Sum is >= 15, store 1
    li x17, 1
    j threshold_done
below_threshold:
    // Sum is < 15, store 0
    li x17, 0
threshold_done:
    sw x17, 12(x5)   // Store threshold test result

// Demonstrate system call (if supported by our kernel)
// Write a message to output (conceptual)
li x20, 0x0001     // SYS_WRITE system call number
li x21, 0          // File descriptor (stdout)
la x22, message    // Message address
li x23, 14         // Message length
ecall              // Make system call

// Exit program
li x28, 0          // Exit code
li x29, 0x0000     // SYS_EXIT system call number
ecall              // Make system call to exit

// Message data (would be stored in memory)
message: .asciiz "Test completed!"