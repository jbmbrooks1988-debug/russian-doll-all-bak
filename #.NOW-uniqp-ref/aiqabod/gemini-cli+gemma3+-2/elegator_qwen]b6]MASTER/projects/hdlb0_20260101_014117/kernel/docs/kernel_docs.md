# HDLB0 Kernel Documentation

## Overview
The HDLB0 kernel is a minimal, backwards-compatible kernel designed to work across different bit-widths (2-bit to 8-bit and beyond). It provides essential operating system services while maintaining compatibility across different implementations of the HDLB0 processor architecture.

## Architecture

### Design Philosophy
The kernel follows a minimalistic design approach, focusing on essential functionality that can scale across different bit-widths. It uses abstraction layers to handle differences between implementations while maintaining a consistent interface.

### Core Components
1. **Process Management**: Handles process creation, scheduling, and context switching
2. **Memory Management**: Provides memory allocation and management services
3. **Interrupt Handling**: Manages hardware and software interrupts
4. **System Calls Interface**: Provides a standardized interface for user programs

## Bit-Width Compatibility

### Parameterized Design
The kernel uses compile-time parameters to adapt to different bit-widths:
- `BIT_WIDTH`: Configures the kernel for 2-bit, 3-bit, 4-bit, or 8-bit operation
- Memory addresses, register sizes, and data structures scale with the bit-width
- Maximum process counts and memory allocations are adjusted accordingly

### Resource Management
- In 2-bit systems: Severe resource constraints, minimal functionality
- In 8-bit systems: More generous resource allocation, full functionality
- Intermediate bit-widths: Scaled functionality based on available resources

## System Calls

The kernel provides the following system calls:

### SYS_WRITE (0x01)
Writes data to output device
- Parameters: r1 = buffer address, r2 = length
- Returns: Number of bytes written

### SYS_READ (0x02)
Reads data from input device
- Parameters: r1 = buffer address, r2 = max length
- Returns: Number of bytes read

### SYS_EXIT (0x00)
Terminates the current process
- Parameters: None
- Returns: Does not return

### SYS_YIELD (0x04)
Yields control to another process
- Parameters: None
- Returns: Returns when process is scheduled again

### SYS_FORK (0x03)
Creates a new process (not fully implemented in minimal version)
- Parameters: None
- Returns: PID of new process or -1 on failure

## Memory Layout

### Address Space
- `0x00-0x3F`: Kernel space
- `0x40-0x7E`: User space
- `0x7F`: Stack top (grows downward)

### Memory Management
The kernel uses a simple memory management scheme:
- Single memory block allocation
- First-come, first-served allocation
- Memory is freed when explicitly released

## Process Management

### Process States
- `PROC_RUNNING (0x01)`: Currently executing
- `PROC_READY (0x02)`: Ready to execute
- `PROC_WAITING (0x03)`: Waiting for an event

### Scheduling
The kernel uses a simple round-robin scheduler:
- Processes are selected in a circular order
- Each process gets equal time slices
- Context switching occurs on system calls or interrupts

## Interrupt Handling

### Interrupt Types
- `INT_TIMER (0x00)`: Timer interrupt (triggers context switch)
- `INT_KEYBOARD (0x01)`: Keyboard input interrupt
- `INT_SYSCALL (0x02)`: System call interrupt

### Interrupt Service Routines
Each interrupt type has a dedicated handler that performs the appropriate action.

## Implementation Details

### Assembly Syntax
The kernel is written in a custom assembly format compatible with HDLB0 processors:
- Instructions follow RISC-style format
- Memory addresses are byte-addressable
- Registers are prefixed with 'r' (e.g., r1, r2)

### Data Structures
- Process Control Block (PCB): Stores process state information
- Memory Block: Tracks allocated memory regions
- Interrupt Vector Table: Maps interrupt numbers to handlers

## Extensibility

### Forward Compatibility
The kernel design allows for easy extension to higher bit-widths:
- Parameterized data structures
- Scalable algorithms
- Modular component design

### Adding New Features
New functionality can be added by:
- Implementing new system calls
- Adding new interrupt handlers
- Extending existing data structures

## Limitations

### Resource Constraints
- In 2-bit systems, functionality is severely limited
- Memory allocation is basic and not optimized
- Process management is minimal

### Security
- No memory protection between processes
- No privilege level separation
- Basic error handling only

## Usage

### Building for Different Bit-Widths
To build the kernel for a specific bit-width, define the `BIT_WIDTH` symbol:
- For 2-bit: `BIT_WIDTH = 2`
- For 8-bit: `BIT_WIDTH = 8`

### Integration
The kernel should be linked at the appropriate memory location based on the target system's memory map.

## Files

### Source Files
- `kernel.asm`: Main kernel implementation
- `kernel_param.asm`: Parameterized kernel modules

### Documentation Files
- `architecture.md`: Architecture overview
- `kernel_docs.md`: This file

## Testing

The kernel includes basic compatibility tests to verify functionality across different bit-widths. See the `tests/` and `compatibility/` directories for details.