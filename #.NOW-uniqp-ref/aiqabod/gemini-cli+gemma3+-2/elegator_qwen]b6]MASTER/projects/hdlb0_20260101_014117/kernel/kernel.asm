# HDLB0 Minimal Kernel for 2-bit to 8-bit Systems
# Designed to be backwards compatible and extensible

# Define bit-width parameter (will be configured based on target system)
# For 2-bit systems: BIT_WIDTH = 2
# For 8-bit systems: BIT_WIDTH = 8

# Memory layout constants
KERNEL_BASE_ADDR    = 0x00    # Base address for kernel
USER_BASE_ADDR      = 0x40    # Base address for user programs
STACK_BASE_ADDR     = 0x7F    # Stack grows downward from here

# Process states
PROC_RUNNING        = 0x01
PROC_READY          = 0x02
PROC_WAITING        = 0x03

# System call numbers
SYS_EXIT            = 0x00
SYS_WRITE           = 0x01
SYS_READ            = 0x02
SYS_FORK            = 0x03
SYS_YIELD           = 0x04

# Interrupt numbers
INT_TIMER           = 0x00
INT_KEYBOARD        = 0x01
INT_SYSCALL         = 0x02

# Kernel data structures

# Process Control Block (PCB)
# Size varies based on bit-width but structure remains consistent
# For 2-bit system: 4 bytes
# For 8-bit system: 8 bytes
.struct PCB
    .byte pid         # Process ID
    .byte state       # Process state
    .byte priority    # Process priority
    .byte pc          # Program counter
    .byte sp          # Stack pointer
    .byte registers   # Saved registers
    .byte next        # Pointer to next PCB
.endstruct

# Memory management block
.struct MemBlock
    .byte addr        # Memory address
    .byte size        # Size of block
    .byte used        # Whether block is used
    .byte next        # Pointer to next block
.endstruct

# Main kernel entry point
.kernel_start:
    # Initialize kernel data structures
    call init_kernel
    
    # Initialize process management
    call init_processes
    
    # Initialize memory management
    call init_memory
    
    # Initialize interrupt handling
    call init_interrupts
    
    # Load and run first process
    call scheduler
    
    # Should not reach here
    jmp kernel_panic

# Initialize kernel data structures
init_kernel:
    # Set up kernel stack
    mov r1, #STACK_BASE_ADDR
    mov sp, r1
    
    # Initialize kernel variables
    mov r1, #0
    mov kernel_initialized, r1
    
    ret

# Initialize process management
init_processes:
    # Initialize process list
    mov r1, #0
    mov current_process, r1
    mov process_count, r1
    
    # Set up first process (idle process)
    call create_idle_process
    
    ret

# Initialize memory management
init_memory:
    # Initialize memory free list
    mov r1, #KERNEL_BASE_ADDR
    mov mem_start, r1
    
    # Calculate available memory based on bit-width
    # For 2-bit: 4 bytes total, kernel uses first 2, rest for user
    # For 8-bit: 256 bytes total, kernel uses first 64, rest for user
    # This is simplified for demonstration
    
    # Initialize first memory block
    mov r1, #USER_BASE_ADDR
    mov mem_block_addr, r1
    
    # Calculate memory size based on bit-width
    # For 2-bit: 2 bytes available for user
    # For 8-bit: 192 bytes available for user (simplified)
    mov r1, #0x3F  # For 8-bit, 63 bytes for user space (simplified)
    mov mem_block_size, r1
    
    # Mark as free
    mov r1, #0
    mov mem_block_used, r1
    
    ret

# Initialize interrupt handling
init_interrupts:
    # Set up interrupt vector table
    mov r1, #interrupt_vector_table
    mov int_table_addr, r1
    
    # Enable interrupts
    mov r1, #1
    mov interrupts_enabled, r1
    
    ret

# Process management functions

# Create a new process
create_process:
    # Check if we have reached max processes
    mov r1, process_count
    cmp r1, #MAX_PROCESSES
    jge create_process_fail
    
    # Find free PCB slot
    mov r1, #pcb_table
    mov r2, #0
    mov r3, #PCB_SIZE
    
find_free_pcb:
    mov r4, [r1 + r2 * r3 + 1]  # Check state field
    cmp r4, #0
    jeq found_free_pcb
    inc r2
    cmp r2, #MAX_PROCESSES
    jl find_free_pcb
    
    # No free PCB found
    jmp create_process_fail

found_free_pcb:
    # Initialize PCB
    mov [r1 + r2 * r3], r2           # Set PID
    mov r4, #PROC_READY
    mov [r1 + r2 * r3 + 1], r4       # Set state to READY
    mov r4, #1                       # Default priority
    mov [r1 + r2 * r3 + 2], r4       # Set priority
    # Set PC to provided address (passed in r5)
    mov [r1 + r2 * r3 + 3], r5       # Set PC
    # Set SP to default value
    mov r4, #STACK_BASE_ADDR
    sub r4, r2                       # Different stack for each process
    mov [r1 + r2 * r3 + 4], r4       # Set SP
    
    # Increment process count
    inc process_count
    
    # Return PID
    mov r1, r2
    ret

create_process_fail:
    mov r1, #-1  # Return error code
    ret

# Context switch function
context_switch:
    # Save current process context
    push r0
    push r1
    push r2
    push r3
    push r4
    push r5
    push r6
    push r7
    
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
    
    # Load next process context
    mov r2, current_process
    mov r3, #PCB_SIZE
    mul r2, r3
    add r2, #pcb_table
    add r2, #5  # Offset to registers field
    mov r1, [r2]
    mov sp, r1
    
    pop r7
    pop r6
    pop r5
    pop r4
    pop r3
    pop r2
    pop r1
    pop r0
    
    ret

# Scheduler - simple round-robin
scheduler:
    # Find next ready process
    mov r1, current_process
    inc r1
    mov r2, #MAX_PROCESSES
    
check_next_process:
    cmp r1, r2
    jl check_current_process
    mov r1, #0  # Wrap around
    
check_current_process:
    mov r2, #PCB_SIZE
    mul r1, r2
    add r1, #pcb_table
    add r1, #1  # Offset to state field
    mov r2, [r1]
    cmp r2, #PROC_READY
    jeq found_next_process
    inc r1
    div r1, r2  # Get back to PID
    jmp check_next_process

found_next_process:
    div r1, r2  # Get back to PID
    mov current_process, r1
    
    # Update PCB state to RUNNING
    mov r2, #PCB_SIZE
    mul r1, r2
    add r1, #pcb_table
    add r1, #1  # Offset to state field
    mov r2, #PROC_RUNNING
    mov [r1], r2
    
    # Get back to PID
    sub r1, #pcb_table
    sub r1, #1
    div r1, r2
    
    ret

# Memory management functions

# Allocate memory
malloc:
    # Check if requested size fits in available block
    cmp r1, mem_block_size
    jg malloc_fail
    
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

malloc_fail:
    mov r1, #0  # Return NULL
    ret

# Free memory
free:
    # Check if address matches our managed block
    cmp r1, mem_block_addr
    jne free_invalid
    
    # Mark block as free
    mov r2, #0
    mov mem_block_used, r2
    
    ret

free_invalid:
    # Invalid address, do nothing
    ret

# Interrupt handling

# General interrupt handler
interrupt_handler:
    push r0
    push r1
    push r2
    push r3
    
    # Determine interrupt type
    mov r1, current_interrupt
    cmp r1, #INT_TIMER
    jeq handle_timer_int
    cmp r1, #INT_KEYBOARD
    jeq handle_keyboard_int
    cmp r1, #INT_SYSCALL
    jeq handle_syscall_int
    
    # Unknown interrupt
    pop r3
    pop r2
    pop r1
    pop r0
    iret

handle_timer_int:
    # Timer interrupt - trigger context switch
    call context_switch
    jmp interrupt_done

handle_keyboard_int:
    # Keyboard interrupt - handle input
    # For now, just acknowledge
    jmp interrupt_done

handle_syscall_int:
    # System call interrupt - dispatch to appropriate handler
    mov r1, syscall_number
    cmp r1, #SYS_WRITE
    jeq syscall_write
    cmp r1, #SYS_READ
    jeq syscall_read
    cmp r1, #SYS_EXIT
    jeq syscall_exit
    cmp r1, #SYS_YIELD
    jeq syscall_yield
    
    # Unknown system call
    jmp interrupt_done

syscall_write:
    # Write system call
    # Parameters: r1 = buffer address, r2 = length
    call kernel_write
    jmp interrupt_done

syscall_read:
    # Read system call
    # Parameters: r1 = buffer address, r2 = max length
    call kernel_read
    mov r1, r2  # Return number of bytes read
    jmp interrupt_done

syscall_exit:
    # Exit system call
    call kernel_exit
    jmp interrupt_done

syscall_yield:
    # Yield system call - trigger context switch
    call context_switch
    jmp interrupt_done

interrupt_done:
    pop r3
    pop r2
    pop r1
    pop r0
    iret

# System call interface

# Write to output
kernel_write:
    # Simplified write function
    # r1 = buffer address, r2 = length
    mov r3, #0  # Counter
    
write_loop:
    cmp r3, r2
    jge write_done
    mov r4, [r1 + r3]  # Get byte from buffer
    # Output the byte (implementation depends on hardware)
    # For HDLB0 emulator, this might be a special memory-mapped address
    inc r3
    jmp write_loop
    
write_done:
    ret

# Read from input
kernel_read:
    # Simplified read function
    # r1 = buffer address, r2 = max length
    mov r3, #0  # Counter
    
read_loop:
    # Check if input is available
    # For HDLB0 emulator, this might be a special memory-mapped address
    # For now, just fill with dummy data
    mov r4, #'A'  # Dummy character
    mov [r1 + r3], r4
    inc r3
    cmp r3, r2
    jl read_loop
    
    # Return number of bytes read
    mov r2, r3
    ret

# Process exit
kernel_exit:
    # Mark current process as finished
    mov r1, current_process
    mov r2, #PCB_SIZE
    mul r1, r2
    add r1, #pcb_table
    add r1, #1  # Offset to state field
    mov r2, #0  # Mark as unused
    mov [r1], r2
    
    # Decrement process count
    dec process_count
    
    # Yield to another process
    call context_switch
    
    ret

# Kernel panic function
kernel_panic:
    # Critical kernel error occurred
    # For HDLB0 emulator, this might set a special status
    mov r1, #0xFF
    mov [0xFF], r1  # Error indicator
    
    # Infinite loop
    jmp kernel_panic

# Create idle process
create_idle_process:
    mov r1, #idle_process_code
    call create_process
    ret

# Idle process code
idle_process_code:
    # Simple idle loop that yields control
    jmp idle_process_code

# Data section
.data

# Kernel state variables
kernel_initialized:     .byte 0
current_process:         .byte 0
process_count:           .byte 0
interrupts_enabled:      .byte 0
current_interrupt:       .byte 0
syscall_number:          .byte 0
int_table_addr:          .word 0

# Memory management variables
mem_start:               .word 0
mem_block_addr:          .byte USER_BASE_ADDR
mem_block_size:          .byte 0x3F  # For 8-bit, adjust for other widths
mem_block_used:          .byte 0

# Process management constants
MAX_PROCESSES = 4
PCB_SIZE = 7  # Size of PCB structure

# Process Control Blocks table
pcb_table:
    .space MAX_PROCESSES * PCB_SIZE

# Interrupt vector table
interrupt_vector_table:
    .word timer_interrupt_handler
    .word keyboard_interrupt_handler
    .word syscall_interrupt_handler

timer_interrupt_handler:
keyboard_interrupt_handler:
syscall_interrupt_handler:
    # Placeholder addresses for interrupt handlers
    .word 0