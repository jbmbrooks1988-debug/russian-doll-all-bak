# HDLB0 Kernel Compatibility Test 2: 8-bit System
# Tests kernel functionality specifically for 8-bit systems

# Configuration for 8-bit system
BIT_WIDTH = 8

# Test message
msg: .asciz "8-bit compatibility test - full functionality"
msg_len = . - msg - 1

# Extended data for 8-bit testing
extended_data:
    .byte 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
    .byte 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF

# Main entry point
_start:
    # Print extended message
    mov r1, #msg          # Address of message
    mov r2, #msg_len      # Length of message
    mov r3, #1            # SYS_WRITE number
    swi                   # Software interrupt to invoke system call
    
    # Test more complex operations possible with 8-bit
    mov r1, #extended_data # Address of extended data
    mov r2, #8            # Length of data to write
    mov r3, #1            # SYS_WRITE number
    swi                   # Software interrupt to invoke system call
    
    # Test multiple yields
    mov r4, #0            # Counter
yield_loop:
    cmp r4, #5            # Yield 5 times
    jge continue_test
    inc r4
    
    mov r3, #4            # SYS_YIELD number
    swi                   # Software interrupt to invoke system call
    jmp yield_loop

continue_test:
    # Exit
    mov r3, #0            # SYS_EXIT number
    swi                   # Software interrupt to invoke system call