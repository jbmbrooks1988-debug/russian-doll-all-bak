# HDLB0 Kernel for 16-bit RISC-V System with File System Support
# Enhanced kernel with file system system calls

# Define bit-width parameter (will be configured based on target system)
# For 16-bit system: BIT_WIDTH = 16

# Memory layout constants
KERNEL_BASE_ADDR    = 0x8000  # Base address for kernel (upper half of 16-bit space)
USER_BASE_ADDR      = 0x0000  # Base address for user programs (lower half)
STACK_BASE_ADDR     = 0x7FFF  # Stack grows downward from here

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
# File system system calls
SYS_FS_CREATE       = 0x10
SYS_FS_DELETE       = 0x11
SYS_FS_READ         = 0x12
SYS_FS_WRITE        = 0x13
SYS_FS_LIST         = 0x14
SYS_FS_OPEN         = 0x15
SYS_FS_CLOSE        = 0x16

# Interrupt numbers
INT_TIMER           = 0x00
INT_KEYBOARD        = 0x01
INT_SYSCALL         = 0x02

# File system constants
MAX_OPEN_FILES      = 8
MAX_FILENAME_LEN    = 8
MAX_FILE_SIZE       = 128

# Kernel data structures

# Process Control Block (PCB)
.struct PCB
    .word pid         # Process ID (16-bit)
    .word state       # Process state
    .word priority    # Process priority
    .word pc          # Program counter
    .word sp          # Stack pointer
    .word registers   # Saved registers
    .word next        # Pointer to next PCB
.endstruct

# Open file descriptor table entry
.struct FileDescriptor
    .word file_id     # File identifier
    .word position    # Current position in file
    .word flags       # File access flags
    .word owner_pid   # Process that opened the file
.endstruct

# Memory management block
.struct MemBlock
    .word addr        # Memory address
    .word size        # Size of block
    .word used        # Whether block is used
    .word next        # Pointer to next block
.endstruct

# Main kernel entry point
.kernel_start:
    # Initialize kernel data structures
    call init_kernel

    # Initialize process management
    call init_processes

    # Initialize memory management
    call init_memory

    # Initialize file system
    call init_filesystem

    # Initialize interrupt handling
    call init_interrupts

    # Load and run first process
    call scheduler

    # Should not reach here
    jmp kernel_panic

# Initialize kernel data structures
init_kernel:
    # Set up kernel stack
    mov x1, #STACK_BASE_ADDR
    mov sp, x1

    # Initialize kernel variables
    mov x1, #0
    mov x2, #kernel_initialized
    sw x1, 0(x2)

    ret

# Initialize process management
init_processes:
    # Initialize process list
    mov x1, #0
    mov x2, #current_process
    sw x1, 0(x2)
    mov x2, #process_count
    sw x1, 0(x2)

    # Set up first process (idle process)
    call create_idle_process

    ret

# Initialize memory management
init_memory:
    # Initialize memory free list
    mov x1, #KERNEL_BASE_ADDR
    mov x2, #mem_start
    sw x1, 0(x2)

    # Calculate available memory for user space
    # User space is from 0x0000 to 0x7FFF (32KB)
    mov x1, #0x7FFF
    mov x2, #mem_block_size
    sw x1, 0(x2)

    # Mark as free
    mov x1, #0
    mov x2, #mem_block_used
    sw x1, 0(x2)

    ret

# Initialize file system
init_filesystem:
    # Initialize file descriptor table
    mov x1, #0
    mov x2, #0
    mov x3, #MAX_OPEN_FILES
    mov x4, #file_descriptors
    mov x5, #SIZEOF_FD  # Size of each file descriptor entry

init_fd_loop:
    cmp x2, x3
    jge init_fd_done
    
    # Clear file descriptor entry
    sw x1, 0(x4)      # file_id = 0
    sw x1, 2(x4)      # position = 0
    sw x1, 4(x4)      # flags = 0
    sw x1, 6(x4)      # owner_pid = 0
    
    # Move to next entry
    add x4, x4, x5
    add x2, x2, #1
    jmp init_fd_loop

init_fd_done:
    # Initialize file system specific variables
    mov x1, #0
    mov x2, #next_file_id
    sw x1, 0(x2)
    
    ret

# Initialize interrupt handling
init_interrupts:
    # Set up interrupt vector table
    mov x1, #interrupt_vector_table
    mov x2, #int_table_addr
    sw x1, 0(x2)

    # Enable interrupts
    mov x1, #1
    mov x2, #interrupts_enabled
    sw x1, 0(x2)

    ret

# Process management functions

# Create a new process
create_process:
    # Check if we have reached max processes
    mov x2, #process_count
    lw x1, 0(x2)
    mov x3, #MAX_PROCESSES
    cmp x1, x3
    jge create_process_fail

    # Find free PCB slot
    mov x1, #pcb_table
    mov x2, #0
    mov x3, #SIZEOF_PCB

find_free_pcb:
    mov x4, #PCB_SIZE
    mul x5, x2, x4
    add x5, x5, x1
    add x5, x5, #2  # Offset to state field
    lw x4, 0(x5)
    mov x6, #0
    cmp x4, x6
    jeq found_free_pcb
    add x2, x2, #1
    mov x4, #MAX_PROCESSES
    cmp x2, x4
    jl find_free_pcb

    # No free PCB found
    jmp create_process_fail

found_free_pcb:
    # Initialize PCB
    sw x2, 0(x5)           # Set PID (x5 already points to the right location)
    sub x5, x5, #2          # Go back to start of PCB entry
    mov x4, #PROC_READY
    sw x4, 2(x5)           # Set state to READY
    mov x4, #1              # Default priority
    sw x4, 4(x5)           # Set priority
    # Set PC to provided address (passed in x5, but we need to get it differently)
    # For this simplified version, we'll set PC to a default value
    mov x4, #0x100
    sw x4, 6(x5)           # Set PC
    # Set SP to default value
    mov x4, #STACK_BASE_ADDR
    sub x4, x4, x2          # Different stack for each process
    sw x4, 8(x5)           # Set SP

    # Increment process count
    mov x2, #process_count
    lw x1, 0(x2)
    add x1, x1, #1
    sw x1, 0(x2)

    # Return PID
    mov x1, x2
    ret

create_process_fail:
    mov x1, #-1  # Return error code
    ret

# Context switch function
context_switch:
    # Save current process context
    push x1
    push x2
    push x3
    push x4
    push x5
    push x6
    push x7
    push x8
    push x9
    push x10
    push x11
    push x12
    push x13
    push x14
    push x15

    # Save current process SP
    mov x1, sp
    mov x2, #current_process
    lw x2, 0(x2)
    mov x3, #SIZEOF_PCB
    mul x2, x2, x3
    add x2, x2, #pcb_table
    add x2, x2, #8  # Offset to registers field (SP location)
    sw x1, 0(x2)

    # Call scheduler to select next process
    call scheduler

    # Load next process context
    mov x2, #current_process
    lw x2, 0(x2)
    mov x3, #SIZEOF_PCB
    mul x2, x2, x3
    add x2, x2, #pcb_table
    add x2, x2, #8  # Offset to registers field (SP location)
    lw x1, 0(x2)
    mov sp, x1

    pop x15
    pop x14
    pop x13
    pop x12
    pop x11
    pop x10
    pop x9
    pop x8
    pop x7
    pop x6
    pop x5
    pop x4
    pop x3
    pop x2
    pop x1

    ret

# Scheduler - simple round-robin
scheduler:
    # Find next ready process
    mov x2, #current_process
    lw x1, 0(x2)  # Get current process
    add x1, x1, #1  # Next process
    mov x2, #MAX_PROCESSES

check_next_process:
    cmp x1, x2
    jl check_current_process
    mov x1, #0  # Wrap around

check_current_process:
    mov x2, #SIZEOF_PCB
    mul x3, x1, x2
    add x3, x3, #pcb_table
    add x3, x3, #2  # Offset to state field
    lw x2, 0(x3)
    mov x4, #PROC_READY
    cmp x2, x4
    jeq found_next_process
    add x1, x1, #1
    jmp check_next_process

found_next_process:
    mov x2, #current_process
    sw x1, 0(x2)  # Set current process to found process

    # Update PCB state to RUNNING
    mov x3, #SIZEOF_PCB
    mul x4, x1, x3
    add x4, x4, #pcb_table
    add x4, x4, #2  # Offset to state field
    mov x5, #PROC_RUNNING
    sw x5, 0(x4)

    ret

# Memory management functions

# Allocate memory
malloc:
    # Check if requested size fits in available block
    mov x2, #mem_block_size
    lw x2, 0(x2)
    cmp x1, x2
    jg malloc_fail

    # Check if block is free
    mov x2, #mem_block_used
    lw x2, 0(x2)
    mov x3, #0
    cmp x2, x3
    jne malloc_fail

    # Mark block as used
    mov x2, #1
    mov x3, #mem_block_used
    sw x2, 0(x3)

    # Return address of allocated block
    mov x2, #mem_block_addr
    lw x1, 0(x2)
    ret

malloc_fail:
    mov x1, #0  # Return NULL
    ret

# Free memory
free:
    # Check if address matches our managed block
    mov x2, #mem_block_addr
    lw x2, 0(x2)
    cmp x1, x2
    jne free_invalid

    # Mark block as free
    mov x2, #0
    mov x3, #mem_block_used
    sw x2, 0(x3)

    ret

free_invalid:
    # Invalid address, do nothing
    ret

# File system functions

# Find a free file descriptor
find_free_fd:
    mov x1, #0  # Counter
    mov x2, #MAX_OPEN_FILES
    mov x3, #file_descriptors
    mov x4, #SIZEOF_FD

find_fd_loop:
    cmp x1, x2
    jge find_fd_none_free  # No free file descriptors
    
    # Check if this file descriptor is free (file_id == 0)
    mul x5, x1, x4
    add x5, x5, x3
    lw x6, 0(x5)  # Get file_id
    mov x7, #0
    cmp x6, x7
    jeq found_free_fd  # Found a free file descriptor
    
    add x1, x1, #1
    jmp find_fd_loop

found_free_fd:
    # Return the index of the free file descriptor
    ret

find_fd_none_free:
    mov x1, #-1  # Return error code
    ret

# System call handlers for file system

# Create file system call handler
syscall_fs_create:
    # x10 = pointer to filename string
    # x11 = file flags (not used in simple implementation)
    
    # Find a free file descriptor
    call find_free_fd
    cmp x1, #-1
    jeq syscall_fs_create_fail
    
    # x1 now contains the index of a free file descriptor
    mov x2, x1  # Save the index
    
    # Set up the file descriptor
    mov x3, #file_descriptors
    mov x4, #SIZEOF_FD
    mul x5, x2, x4
    add x5, x5, x3  # x5 points to the file descriptor
    
    # Get next file ID
    mov x6, #next_file_id
    lw x7, 0(x6)  # Get current next_file_id
    sw x7, 0(x5)  # Set file_id in the descriptor
    
    # Increment next_file_id
    add x7, x7, #1
    sw x7, 0(x6)
    
    # Set position to 0
    mov x7, #0
    sw x7, 2(x5)  # Set position
    
    # Set flags (from x11)
    mov x7, x11
    sw x7, 4(x5)  # Set flags
    
    # Set owner PID
    mov x6, #current_process
    lw x7, 0(x6)
    sw x7, 6(x5)  # Set owner PID
    
    # Return the file descriptor index
    mov x1, x2
    ret

syscall_fs_create_fail:
    mov x1, #-1  # Return error
    ret

# Write to file system call handler
syscall_fs_write:
    # x10 = file descriptor
    # x11 = pointer to data buffer
    # x12 = number of bytes to write
    
    # Validate file descriptor
    cmp x10, #MAX_OPEN_FILES
    jge syscall_fs_write_fail
    cmp x10, #0
    jl syscall_fs_write_fail
    
    # Get the file descriptor entry
    mov x1, #file_descriptors
    mov x2, #SIZEOF_FD
    mul x3, x10, x2
    add x3, x3, x1  # x3 points to the file descriptor
    
    # Check if file is open by checking if file_id != 0
    lw x4, 0(x3)
    mov x5, #0
    cmp x4, x5
    jeq syscall_fs_write_fail  # File not open
    
    # In a real implementation, we would write to the actual file
    # For this simplified version, we'll just return the number of bytes as written
    mov x1, x12  # Return number of bytes written
    ret

syscall_fs_write_fail:
    mov x1, #-1  # Return error
    ret

# Read from file system call handler
syscall_fs_read:
    # x10 = file descriptor
    # x11 = pointer to data buffer
    # x12 = max number of bytes to read
    
    # Validate file descriptor
    cmp x10, #MAX_OPEN_FILES
    jge syscall_fs_read_fail
    cmp x10, #0
    jl syscall_fs_read_fail
    
    # Get the file descriptor entry
    mov x1, #file_descriptors
    mov x2, #SIZEOF_FD
    mul x3, x10, x2
    add x3, x3, x1  # x3 points to the file descriptor
    
    # Check if file is open by checking if file_id != 0
    lw x4, 0(x3)
    mov x5, #0
    cmp x4, x5
    jeq syscall_fs_read_fail  # File not open
    
    # In a real implementation, we would read from the actual file
    # For this simplified version, we'll just return 0 bytes read
    mov x1, #0  # Return number of bytes read
    ret

syscall_fs_read_fail:
    mov x1, #-1  # Return error
    ret

# Delete file system call handler
syscall_fs_delete:
    # x10 = pointer to filename string
    
    # In a real implementation, we would delete the actual file
    # For this simplified version, we'll just return success
    mov x1, #0  # Return success
    ret

# List files system call handler
syscall_fs_list:
    # In a real implementation, we would list the files in the file system
    # For this simplified version, we'll just return success
    mov x1, #0  # Return success
    ret

# Interrupt handling

# General interrupt handler
interrupt_handler:
    push x1
    push x2
    push x3
    push x4

    # Determine interrupt type
    mov x2, #current_interrupt
    lw x1, 0(x2)
    mov x3, #INT_SYSCALL
    cmp x1, x3
    jeq handle_syscall_int

    mov x3, #INT_TIMER
    cmp x1, x3
    jeq handle_timer_int

    mov x3, #INT_KEYBOARD
    cmp x1, x3
    jeq handle_keyboard_int

    # Unknown interrupt
    pop x4
    pop x3
    pop x2
    pop x1
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
    mov x2, #syscall_number
    lw x1, 0(x2)  # Get system call number
    
    # Check for file system system calls first
    cmp x1, #SYS_FS_CREATE
    jeq syscall_fs_create
    cmp x1, #SYS_FS_WRITE
    jeq syscall_fs_write
    cmp x1, #SYS_FS_READ
    jeq syscall_fs_read
    cmp x1, #SYS_FS_DELETE
    jeq syscall_fs_delete
    cmp x1, #SYS_FS_LIST
    jeq syscall_fs_list
    
    # Check for regular system calls
    cmp x1, #SYS_WRITE
    jeq syscall_write
    cmp x1, #SYS_READ
    jeq syscall_read
    cmp x1, #SYS_EXIT
    jeq syscall_exit
    cmp x1, #SYS_YIELD
    jeq syscall_yield

    # Unknown system call
    jmp interrupt_done

syscall_write:
    # Write system call
    # Parameters: x10 = buffer address, x11 = length
    call kernel_write
    jmp interrupt_done

syscall_read:
    # Read system call
    # Parameters: x10 = buffer address, x11 = max length
    call kernel_read
    mov x1, x11  # Return number of bytes read
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
    pop x4
    pop x3
    pop x2
    pop x1
    iret

# System call interface

# Write to output
kernel_write:
    # Simplified write function
    # x10 = buffer address, x11 = length
    mov x12, #0  # Counter

write_loop:
    cmp x12, x11
    jge write_done
    add x13, x10, x12
    lb x14, 0(x13)  # Get byte from buffer
    # Output the byte (implementation depends on hardware)
    # For HDLB0 emulator, this might be a special memory-mapped address
    add x12, x12, #1
    jmp write_loop

write_done:
    ret

# Read from input
kernel_read:
    # Simplified read function
    # x10 = buffer address, x11 = max length
    mov x12, #0  # Counter

read_loop:
    # Check if input is available
    # For HDLB0 emulator, this might be a special memory-mapped address
    # For now, just fill with dummy data
    mov x13, #'A'  # Dummy character
    add x14, x10, x12
    sb x13, 0(x14)
    add x12, x12, #1
    cmp x12, x11
    jl read_loop

    # Return number of bytes read
    mov x1, x12
    ret

# Process exit
kernel_exit:
    # Mark current process as finished
    mov x1, #current_process
    lw x1, 0(x1)
    mov x2, #SIZEOF_PCB
    mul x1, x1, x2
    add x1, x1, #pcb_table
    add x1, x1, #2  # Offset to state field
    mov x2, #0  # Mark as unused
    sw x2, 0(x1)

    # Decrement process count
    mov x1, #process_count
    lw x2, 0(x1)
    sub x2, x2, #1
    sw x2, 0(x1)

    # Yield to another process
    call context_switch

    ret

# Kernel panic function
kernel_panic:
    # Critical kernel error occurred
    # For HDLB0 emulator, this might set a special status
    mov x1, #0xFFFF
    mov x2, #0xFFFE
    sw x1, 0(x2)  # Error indicator

    # Infinite loop
    jmp kernel_panic

# Create idle process
create_idle_process:
    mov x1, #idle_process_code
    call create_process
    ret

# Idle process code
idle_process_code:
    # Simple idle loop that yields control
    jmp idle_process_code

# Data section
.data

# Kernel state variables
kernel_initialized:     .word 0
current_process:         .word 0
process_count:           .word 0
interrupts_enabled:      .word 0
current_interrupt:       .word 0
syscall_number:          .word 0
int_table_addr:          .word 0

# Memory management variables
mem_start:               .word 0
mem_block_addr:          .word USER_BASE_ADDR
mem_block_size:          .word 0x7FFF  # For 16-bit, adjust for other widths
mem_block_used:          .word 0

# File system variables
next_file_id:            .word 1
file_descriptors:        .space MAX_OPEN_FILES * SIZEOF_FD

# Process management constants
MAX_PROCESSES = 4
SIZEOF_PCB = 14  # Size of PCB structure in bytes
SIZEOF_FD = 8    # Size of file descriptor entry in bytes

# Process Control Blocks table
pcb_table:
    .space MAX_PROCESSES * SIZEOF_PCB

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