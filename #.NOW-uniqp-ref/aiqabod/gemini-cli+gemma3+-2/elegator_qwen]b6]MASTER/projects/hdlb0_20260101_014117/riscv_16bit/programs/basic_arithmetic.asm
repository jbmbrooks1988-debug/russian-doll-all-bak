// Basic arithmetic operations test program for 16-bit RISC-V processor
// This program performs simple arithmetic operations to test the ALU

// Initialize registers with values
li x1, 10          // Load immediate 10 into register x1
li x2, 5           // Load immediate 5 into register x2
li x3, 0           // Initialize register x3 to 0

// Addition: x3 = x1 + x2
add x3, x1, x2     // x3 = 10 + 5 = 15

// Subtraction: x4 = x1 - x2
sub x4, x1, x2     // x4 = 10 - 5 = 5

// AND operation: x5 = x1 & x2
and x5, x1, x2     // x5 = 10 & 5 (bitwise AND)

// OR operation: x6 = x1 | x2
or x6, x1, x2      // x6 = 10 | 5 (bitwise OR)

// XOR operation: x7 = x1 ^ x2
xor x7, x1, x2     // x7 = 10 ^ 5 (bitwise XOR)

// Shift operations
sll x8, x1, 1      // x8 = x1 << 1 (shift left logical)
srl x9, x1, 1      // x9 = x1 >> 1 (shift right logical)

// Set less than operations
slt x10, x2, x1    // x10 = 1 if x2 < x1, else 0
sltu x11, x2, x1   // x11 = 1 if x2 < x1 (unsigned), else 0

// Store results in memory
sw x3, 0(x0)       // Store x3 at memory address 0
sw x4, 2(x0)       // Store x4 at memory address 2
sw x5, 4(x0)       // Store x5 at memory address 4

// End program
li x17, 0          // Load exit syscall number
ecall              // Make system call to exit