// Demonstration program for HDLB0 16-bit RISC-V OS
// This program shows the complete system: kernel, file system, and shell

.text
.global _start

_start:
    // Initialize the system by jumping to shell
    jal start_shell

// Shell entry point - this would be implemented in shell.c but represented here
start_shell:
    // Print welcome message
    la x10, welcome_msg
    li x17, 0x01  // SYS_WRITE
    ecall

    // Main shell loop
shell_loop:
    // Print prompt
    la x10, prompt_msg
    li x17, 0x01  // SYS_WRITE
    ecall

    // Read command from user
    la x10, input_buffer
    li x11, 16    // Max length
    li x17, 0x02  // SYS_READ
    ecall

    // Parse and execute command
    // For this demo, we'll simulate a simple command execution
    // In a real system, this would parse the input and call appropriate handlers
    
    // Example: Create a file
    la x10, test_filename
    li x17, 0x10  // FS_CREATE_FILE
    ecall

    // Example: Write to the file
    la x10, test_filename
    la x11, test_data
    li x12, 12    // Length of "Hello World!"
    li x17, 0x13  // FS_WRITE_FILE
    ecall

    // Example: Read from the file
    la x10, test_filename
    la x11, read_buffer
    li x12, 100   // Buffer size
    li x17, 0x12  // FS_READ_FILE
    ecall

    // Example: List files
    li x17, 0x14  // FS_LIST_FILES
    ecall

    // Exit the demo
    li x17, 0x00  // SYS_EXIT
    ecall

.data
welcome_msg: .asciiz "HDLB0 16-bit RISC-V OS Demo\n"
prompt_msg: .asciiz "> "
input_buffer: .space 16
test_filename: .asciiz "demo.txt"
test_data: .asciiz "Hello World!"
read_buffer: .space 100