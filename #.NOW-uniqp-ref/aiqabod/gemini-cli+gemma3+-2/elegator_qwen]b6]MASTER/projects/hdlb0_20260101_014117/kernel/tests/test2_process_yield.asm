# HDLB0 Kernel Test Program 2: Process Yield
# Demonstrates the yield system call for cooperative multitasking

counter: .byte 0

# Main entry point
_start:
    # Increment counter
    mov r1, counter
    inc r1
    mov counter, r1
    
    # Print counter value
    mov r1, #counter      # Address of counter
    mov r2, #1            # Length of 1 byte
    mov r3, #1            # SYS_WRITE number
    swi                   # Software interrupt to invoke system call
    
    # Yield control to another process
    mov r3, #4            # SYS_YIELD number
    swi                   # Software interrupt to invoke system call
    
    # Loop back to increment again
    jmp _start