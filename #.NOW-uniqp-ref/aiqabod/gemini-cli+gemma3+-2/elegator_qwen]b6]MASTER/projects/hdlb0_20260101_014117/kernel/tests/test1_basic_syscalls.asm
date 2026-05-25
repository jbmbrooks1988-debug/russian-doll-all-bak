# HDLB0 Kernel Test Program 1: Basic System Calls
# Demonstrates write and exit system calls

# Message to print
msg:     .asciz "Hello from HDLB0 kernel test 1!"
msg_len = . - msg - 1

# Main entry point
_start:
    # Print the message using SYS_WRITE
    mov r1, #msg          # Address of message
    mov r2, #msg_len      # Length of message
    mov r3, #1            # SYS_WRITE number
    swi                   # Software interrupt to invoke system call
    
    # Exit using SYS_EXIT
    mov r3, #0            # SYS_EXIT number
    swi                   # Software interrupt to invoke system call
    
    # Should not reach here
    jmp _start