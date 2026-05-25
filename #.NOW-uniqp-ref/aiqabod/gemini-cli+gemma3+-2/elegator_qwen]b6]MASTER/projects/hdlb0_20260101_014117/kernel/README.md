# HDLB0 Kernel

The HDLB0 kernel is a minimal, backwards-compatible kernel designed to work across different bit-widths (2-bit to 8-bit and beyond). It provides essential operating system services while maintaining compatibility across different implementations of the HDLB0 processor architecture.

## Features

- **Process Management**: Minimal process abstraction with context switching
- **Memory Management**: Basic memory allocation and management
- **Interrupt Handling**: Support for hardware and software interrupts
- **System Calls Interface**: Standardized interface for user programs
- **Bit-Width Compatibility**: Parameterized design for 2-bit to 8-bit systems
- **Extensibility**: Designed to scale to higher bit-widths

## Directory Structure

```
kernel/
├── kernel.asm              # Main kernel implementation
├── kernel_param.asm        # Parameterized kernel modules
├── docs/
│   ├── architecture.md     # Architecture overview
│   └── kernel_docs.md      # Comprehensive documentation
├── tests/
│   ├── test1_basic_syscalls.asm
│   ├── test2_process_yield.asm
│   ├── test3_memory_alloc.asm
│   └── test4_interrupts.asm
└── compatibility/
    ├── compat_test_2bit.asm
    ├── compat_test_8bit.asm
    └── compat_test_cross_width.asm
```

## System Calls

- `SYS_WRITE (0x01)`: Write data to output
- `SYS_READ (0x02)`: Read data from input
- `SYS_EXIT (0x00)`: Exit current process
- `SYS_YIELD (0x04)`: Yield control to scheduler
- `SYS_FORK (0x03)`: Create new process (not fully implemented)

## Bit-Width Configuration

The kernel can be configured for different bit-widths by setting the `BIT_WIDTH` parameter:
- 2-bit: Minimal functionality due to resource constraints
- 8-bit: Full functionality with more resources

## Building and Running

The kernel is written in HDLB0 assembly format and needs to be assembled using the HDLB0 toolchain. The resulting binary can be loaded at the appropriate memory location for execution.

## Compatibility

The kernel is designed to be backwards compatible, with the same system call interface across different bit-widths. The parameterized design allows the same source code to be compiled for different bit-widths with appropriate configuration.

## Testing

Various test programs are included to demonstrate kernel functionality:
- Basic system call tests
- Process yield/cooperative multitasking
- Memory allocation tests
- Interrupt handling tests

Compatibility tests verify that the kernel works correctly across different bit-widths.