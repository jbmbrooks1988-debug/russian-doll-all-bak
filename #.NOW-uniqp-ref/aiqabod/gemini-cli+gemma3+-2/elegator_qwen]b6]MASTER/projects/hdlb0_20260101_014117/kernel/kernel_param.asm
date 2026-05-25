# HDLB0 Parameterized Kernel Modules
# These modules adapt to different bit-widths (2-bit to 8-bit)

# Bit-width configuration
# This file can be preprocessed with different BIT_WIDTH values

# Define bit-width specific constants
.if BIT_WIDTH == 2
    # 2-bit system parameters
    MAX_ADDR = 0x03      # 2^2 = 4 addresses (0-3)
    REGISTER_SIZE = 2    # 2-bit registers
    MAX_PID = 0x03       # 2-bit PID values
    MAX_PRIORITY = 0x03  # 2-bit priority values
.elif BIT_WIDTH == 3
    # 3-bit system parameters
    MAX_ADDR = 0x07      # 2^3 = 8 addresses (0-7)
    REGISTER_SIZE = 3    # 3-bit registers
    MAX_PID = 0x07       # 3-bit PID values
    MAX_PRIORITY = 0x07  # 3-bit priority values
.elif BIT_WIDTH == 4
    # 4-bit system parameters
    MAX_ADDR = 0x0F      # 2^4 = 16 addresses (0-15)
    REGISTER_SIZE = 4    # 4-bit registers
    MAX_PID = 0x0F       # 4-bit PID values
    MAX_PRIORITY = 0x0F  # 4-bit priority values
.elif BIT_WIDTH == 8
    # 8-bit system parameters
    MAX_ADDR = 0xFF      # 2^8 = 256 addresses (0-255)
    REGISTER_SIZE = 8    # 8-bit registers
    MAX_PID = 0xFF       # 8-bit PID values
    MAX_PRIORITY = 0xFF  # 8-bit priority values
.endif

# Memory layout based on bit-width
KERNEL_BASE_ADDR = 0x00
.if BIT_WIDTH == 2
    USER_BASE_ADDR = 0x01  # Very limited user space in 2-bit
    STACK_BASE_ADDR = MAX_ADDR
.else
    USER_BASE_ADDR = 0x40  # More reasonable user space for higher bit-widths
    STACK_BASE_ADDR = MAX_ADDR
.endif

# Process Control Block - size varies with bit-width
.struct PCB
    .byte pid         # Process ID (size depends on bit-width)
    .byte state       # Process state (fixed 2-bit value)
    .byte priority    # Priority (size depends on bit-width)
    .byte pc          # Program counter (size depends on bit-width)
    .byte sp          # Stack pointer (size depends on bit-width)
    .byte registers   # Saved registers (size depends on bit-width)
    .byte next        # Next PCB pointer (size depends on bit-width)
.endstruct

# Calculate PCB size based on bit-width
.if BIT_WIDTH <= 4
    PCB_SIZE = 4  # Smaller PCB for lower bit-widths
.else
    PCB_SIZE = 7  # Full PCB for higher bit-widths
.endif

# Maximum number of processes based on bit-width
.if BIT_WIDTH == 2
    MAX_PROCESSES = 2  # Very limited in 2-bit
.elif BIT_WIDTH == 3
    MAX_PROCESSES = 3  # Limited in 3-bit
.elif BIT_WIDTH == 4
    MAX_PROCESSES = 4  # Reasonable in 4-bit
.else
    MAX_PROCESSES = 8  # More processes in 8-bit
.endif

# System call numbers (same across bit-widths for compatibility)
SYS_EXIT            = 0x00
SYS_WRITE           = 0x01
SYS_READ            = 0x02
SYS_FORK            = 0x03
SYS_YIELD           = 0x04

# Interrupt numbers (same across bit-widths for compatibility)
INT_TIMER           = 0x00
INT_KEYBOARD        = 0x01
INT_SYSCALL         = 0x02

# Process states (same across bit-widths for compatibility)
PROC_RUNNING        = 0x01
PROC_READY          = 0x02
PROC_WAITING        = 0x03

# Kernel data structures that adapt to bit-width

# Memory block structure
.struct MemBlock
    .byte addr        # Memory address (size depends on bit-width)
    .byte size        # Size of block (size depends on bit-width)
    .byte used        # Whether block is used (fixed 1-bit)
    .byte next        # Pointer to next block (size depends on bit-width)
.endstruct

# Bit-width specific initialization
init_bitwidth_specific:
    # Initialize based on bit-width
    .if BIT_WIDTH == 2
        # Special initialization for 2-bit system
        # Limited resources, minimal setup
        mov r1, #USER_BASE_ADDR
        mov mem_block_addr, r1
        mov r1, #0x02  # Very small user space
        mov mem_block_size, r1
    .elif BIT_WIDTH == 8
        # Full initialization for 8-bit system
        # More resources available
        mov r1, #USER_BASE_ADDR
        mov mem_block_addr, r1
        mov r1, #0x80  # Larger user space
        mov mem_block_size, r1
    .else
        # Default initialization for other bit-widths
        mov r1, #USER_BASE_ADDR
        mov mem_block_addr, r1
        mov r1, #0x20  # Moderate user space
        mov mem_block_size, r1
    .endif
    
    ret

# Memory allocation adapted to bit-width
malloc_bitwidth:
    # Check available memory based on bit-width
    .if BIT_WIDTH == 2
        # Very restrictive allocation for 2-bit
        cmp r1, #0x01  # Requesting more than 1 byte?
        jg malloc_fail
    .elif BIT_WIDTH <= 4
        # Restrictive allocation for low bit-width
        cmp r1, #0x04  # Requesting more than 4 bytes?
        jg malloc_fail
    .else
        # More permissive allocation for higher bit-width
        cmp r1, mem_block_size
        jg malloc_fail
    .endif
    
    # Rest of allocation logic
    # Check if block is free
    mov r2, mem_block_used
    cmp r2, #0
    jne malloc_fail
    
    # Mark block as used
    mov r2, #1
    mov mem_block_used, r2
    
    # Return address of allocated block
    mov r1, mem_block_addr
    ret

# Process creation adapted to bit-width
create_process_bitwidth:
    # Check max processes based on bit-width
    .if BIT_WIDTH == 2
        mov r1, process_count
        cmp r1, #2  # Max 2 processes in 2-bit
        jge create_process_fail
    .elif BIT_WIDTH <= 4
        mov r1, process_count
        cmp r1, #4  # Max 4 processes in 4-bit
        jge create_process_fail
    .else
        mov r1, process_count
        cmp r1, #8  # Max 8 processes in higher bit-widths
        jge create_process_fail
    .endif
    
    # Continue with process creation
    call create_process
    ret

# Context switching adapted to bit-width
context_switch_bitwidth:
    # Save fewer registers in lower bit-widths
    .if BIT_WIDTH <= 4
        # Save only essential registers
        push r0
        push r1
        push r2
        push r3
    .else
        # Save all registers in higher bit-widths
        push r0
        push r1
        push r2
        push r3
        push r4
        push r5
        push r6
        push r7
    .endif
    
    # Save current process SP
    mov r1, sp
    mov r2, current_process
    mov r3, #PCB_SIZE
    mul r2, r3
    add r2, #pcb_table
    add r2, #5  # Offset to registers field
    mov [r2], r1
    
    # Call scheduler to select next process
    call scheduler
    
    # Restore registers based on bit-width
    .if BIT_WIDTH <= 4
        # Restore only essential registers
        pop r3
        pop r2
        pop r1
        pop r0
    .else
        # Restore all registers in higher bit-widths
        pop r7
        pop r6
        pop r5
        pop r4
        pop r3
        pop r2
        pop r1
        pop r0
    .endif
    
    ret

# Data section with bit-width specific values
.data
# Memory management variables
mem_block_addr:          .byte USER_BASE_ADDR
.if BIT_WIDTH == 2
    mem_block_size:      .byte 0x02  # Small for 2-bit
.elif BIT_WIDTH <= 4
    mem_block_size:      .byte 0x08  # Moderate for 4-bit
.else
    mem_block_size:      .byte 0x40  # Large for 8-bit
.endif
mem_block_used:          .byte 0

# Process management variables
.if BIT_WIDTH == 2
    process_count:       .byte 0
    current_process:     .byte 0
.else
    process_count:       .byte 0
    current_process:     .byte 0
.endif

# Process Control Blocks table (size varies with bit-width)
.if BIT_WIDTH == 2
    pcb_table:
        .space 2 * PCB_SIZE  # Small table for 2-bit
.else
    pcb_table:
        .space MAX_PROCESSES * PCB_SIZE
.endif