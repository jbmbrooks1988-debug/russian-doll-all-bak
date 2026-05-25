# HDLB0 16-bit RISC-V Operating System

## Overview
This is a complete operating system built for the 16-bit RISC-V processor in the HDLB0 project. It includes a kernel with file system support, a command-line shell, and a minimal C compiler.

## Architecture
The OS is composed of several integrated components:

1. **Kernel**: The core of the OS that manages system resources, memory, processes, and provides system services
2. **File System**: A FAT-like file system that handles file operations through kernel system calls
3. **Shell**: A command-line interface that allows users to interact with the system
4. **C Compiler**: A minimal compiler that translates C code to 16-bit RISC-V assembly

## System Call Interface
The OS provides the following system calls:

### Process Management
- `SYS_EXIT` (0x00): Exit the current process
- `SYS_WRITE` (0x01): Write data to output
- `SYS_READ` (0x02): Read data from input
- `SYS_FORK` (0x03): Create a new process
- `SYS_YIELD` (0x04): Yield processor to another process

### File System Operations
- `FS_CREATE_FILE` (0x10): Create a new file
- `FS_DELETE_FILE` (0x11): Delete an existing file
- `FS_READ_FILE` (0x12): Read data from a file
- `FS_WRITE_FILE` (0x13): Write data to a file
- `FS_LIST_FILES` (0x14): List files in the root directory
- `FS_OPEN_FILE` (0x15): Open a file for reading/writing
- `FS_CLOSE_FILE` (0x16): Close an open file

## How to Use the OS

### Running the OS
1. Assemble your program using the provided assembler:
   ```bash
   python3 riscv_16bit/assembler.py your_program.asm your_program.hlo
   ```

2. Run the program on the emulator:
   ```bash
   emulator_path -c chip_bank.txt -p your_program.hlo
   ```

### Using the Shell
The shell provides the following commands:
- `help` - Show available commands
- `ls` or `dir` - List files in the root directory
- `cat <filename>` - Display contents of a file
- `create <filename>` - Create a new file
- `write <filename> <data>` - Write data to a file
- `del <filename>` or `rm <filename>` - Delete a file
- `clear` or `cls` - Clear the screen
- `exit` or `quit` - Exit the shell

### Example Session
```
HDLB0 Shell v1.0
Type 'help' for available commands
> create test.txt
File created: test.txt
> write test.txt Hello World!
Wrote 12 bytes to test.txt
> cat test.txt
Hello World!
> ls
test.txt - 12 bytes
> del test.txt
File deleted: test.txt
> exit
```

## Memory Layout
- **Kernel Space**: 0x8000 - 0xFFFF (Upper half of 16-bit address space)
- **User Space**: 0x0000 - 0x7FFF (Lower half of 16-bit address space)
- **File System**: Starts at 0x100 in user space
- **Stack**: Grows downward from 0x7FFF

## Integration
All components work together through the kernel:
1. The shell makes system calls to the kernel for file operations
2. The file system is managed entirely by the kernel
3. The C compiler generates code that uses system calls for I/O
4. Memory management ensures proper isolation between processes

## Demonstration
Run the demo script to see the OS in action:
```bash
./demo_os.sh
```

This will compile and run a demonstration program that shows the OS capabilities including kernel initialization, file system operations, and system calls.

## Building Programs
To build a C program:
1. Write your C code (e.g., `program.c`)
2. Compile with the minimal C compiler: `./compiler/minimal_c_compiler program.c > program.asm`
3. Assemble to HDLb0 format: `python3 riscv_16bit/assembler.py program.asm program.hlo`
4. Run on emulator: `emulator_path -c chip_bank.txt -p program.hlo`