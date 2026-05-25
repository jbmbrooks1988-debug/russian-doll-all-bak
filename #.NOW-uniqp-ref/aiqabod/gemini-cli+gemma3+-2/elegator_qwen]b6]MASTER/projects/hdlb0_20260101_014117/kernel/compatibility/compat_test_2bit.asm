# HDLB0 Kernel Compatibility Test 1: 2-bit System
# Tests kernel functionality specifically for 2-bit systems

# Configuration for 2-bit system
BIT_WIDTH = 2

# Test message
msg: .asciz "2-bit compatibility test"
msg_len = . - msg - 1

# Main entry point
_start:
    # Verify basic functionality works with 2-bit constraints
    mov r1, #msg          # Address of message
    mov r2, #msg_len      # Length of message (should be <= 3 for 2-bit)
    mov r3, #1            # SYS_WRITE number
    swi                   # Software interrupt to invoke system call
    
    # Test process yield
    mov r3, #4            # SYS_YIELD number
    swi                   # Software interrupt to invoke system call
    
    # Exit
    mov r3, #0            # SYS_EXIT number
    swi                   # Software interrupt to invoke system call