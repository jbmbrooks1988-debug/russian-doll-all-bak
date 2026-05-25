# HDLB0 Kernel Test Program 4: Interrupt Handling
# Tests the kernel's interrupt handling capabilities

# Counter for interrupt occurrences
interrupt_count: .byte 0

# Main entry point
_start:
    # Print initial message
    msg: .asciz "Waiting for interrupts..."
    msg_len = . - msg - 1
    
    mov r1, #msg          # Address of message
    mov r2, #msg_len      # Length of message
    mov r3, #1            # SYS_WRITE number
    swi                   # Software interrupt to invoke system call
    
wait_loop:
    # Wait in a loop, checking interrupt count
    mov r1, interrupt_count
    cmp r1, #5            # Check if we've had 5 interrupts
    jge exit_program      # If so, exit
    
    # Yield to allow other processes/interrupts to run
    mov r3, #4            # SYS_YIELD number
    swi                   # Software interrupt to invoke system call
    
    jmp wait_loop

exit_program:
    # Print exit message
    msg2: .asciz "Interrupt test complete"
    msg2_len = . - msg2 - 1
    
    mov r1, #msg2         # Address of message
    mov r2, #msg2_len     # Length of message
    mov r3, #1            # SYS_WRITE number
    swi                   # Software interrupt to invoke system call
    
    # Exit
    mov r3, #0            # SYS_EXIT number
    swi                   # Software interrupt to invoke system call