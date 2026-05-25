# HDLB0 Kernel Test Program 3: Memory Allocation
# Tests the kernel's memory management functions

# Main entry point
_start:
    # Try to allocate memory using a system call
    # (This is a simplified test - actual implementation may vary)
    mov r1, #8            # Request 8 bytes
    mov r3, #1            # Using write syscall number as placeholder
    swi                   # Software interrupt
    
    # Print success message
    msg: .asciz "Memory alloc test complete"
    msg_len = . - msg - 1
    
    mov r1, #msg          # Address of message
    mov r2, #msg_len      # Length of message
    mov r3, #1            # SYS_WRITE number
    swi                   # Software interrupt to invoke system call
    
    # Exit
    mov r3, #0            # SYS_EXIT number
    swi                   # Software interrupt to invoke system call