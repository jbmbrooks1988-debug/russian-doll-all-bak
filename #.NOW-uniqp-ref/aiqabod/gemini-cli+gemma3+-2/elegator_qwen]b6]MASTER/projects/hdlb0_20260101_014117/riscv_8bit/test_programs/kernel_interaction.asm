# Test Program 4: Kernel Interaction
# Tests system calls and kernel interface

# Initialize stack pointer
mov sp, #255      # Set stack pointer to top of memory

# Print a message using system call
mov r1, #msg_hello   # Address of message
mov r2, #msg_len     # Length of message
mov r3, #1           # System call number for write
swi                  # Software interrupt to invoke system call

# Yield control to other processes
mov r3, #4           # System call number for yield
swi                  # Software interrupt

# Exit the program
mov r3, #0           # System call number for exit
swi                  # Software interrupt

# Data section
.data
msg_hello: .asciz "Hello from 8-bit RISC-V!"
msg_len = . - msg_hello - 1

# End of program
loop:
    jmp loop          # Fallback infinite loop