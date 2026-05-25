// Memory operations test program for 16-bit RISC-V processor
// This program tests load and store operations

// Initialize registers
li x1, 0x1000      // Base address for memory operations
li x2, 0xABCD      // Value to store in memory
li x3, 0x5555      // Another value to store

// Store operations
sw x2, 0(x1)       // Store 0xABCD at address 0x1000
sw x3, 2(x1)       // Store 0x5555 at address 0x1002

// Load operations
lw x4, 0(x1)       // Load value from address 0x1000 to x4
lw x5, 2(x1)       // Load value from address 0x1002 to x5

// Perform operations on loaded data
add x6, x4, x5     // Add the two loaded values
sub x7, x4, x5     // Subtract the second from the first

// Store results
sw x6, 4(x1)       // Store sum at address 0x1004
sw x7, 6(x1)       // Store difference at address 0x1006

// Test with immediate offsets
li x8, 0x2000      // New base address
li x9, 0x1234      // Value to store
sw x9, 0(x8)       // Store at base address
sw x9, 4(x8)       // Store at base + 4
sw x9, 8(x8)       // Store at base + 8

// Load with immediate offsets
lw x10, 0(x8)      // Load from base address
lw x11, 4(x8)      // Load from base + 4
lw x12, 8(x8)      // Load from base + 8

// Verify loaded values are the same
sub x13, x10, x11  // Should be 0 if same
sub x14, x11, x12  // Should be 0 if same

// Store verification results
sw x13, 10(x1)     // Store verification result 1
sw x14, 12(x1)     // Store verification result 2

// End program
li x17, 0          // Load exit syscall number
ecall              // Make system call to exit