# Simple Shell for 16-bit RISC-V Processor

## Overview
This is a basic command-line shell designed for the 16-bit RISC-V processor in the HDLB0 project. It provides a user interface for interacting with the file system and executing basic commands.

## Features
- Command-line interface with prompt
- Built-in commands for file operations
- Command parsing and argument handling
- Integration with the simple file system

## Commands
- `help` - Display available commands
- `ls`/`dir` - List files in the root directory
- `cat <filename>` - Display contents of a file
- `create <filename>` - Create a new file
- `write <filename> <data>` - Write data to a file
- `del`/`rm <filename>` - Delete a file
- `clear`/`cls` - Clear the screen
- `exit`/`quit` - Exit the shell

## Architecture
- **Command Buffer**: 16 characters maximum per command line
- **Arguments**: Up to 4 arguments per command, 8 characters each
- **Command Table**: Array of command names and handler functions
- **Input/Output**: Uses kernel system calls for I/O operations

## Integration with Kernel
The shell relies on kernel functions for:
- String and character printing
- Reading input from user
- Making system calls to the file system

## Limitations
- No command history
- No tab completion
- Limited error handling
- No piping or redirection

## Memory Usage
- Small memory footprint suitable for 16-bit system
- Static allocation of command buffers
- No dynamic memory allocation required