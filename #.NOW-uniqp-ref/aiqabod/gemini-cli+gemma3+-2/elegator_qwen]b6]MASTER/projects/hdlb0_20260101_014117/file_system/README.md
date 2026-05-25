# Simple File System for 16-bit RISC-V Processor

## Overview
This is a simple FAT-like file system designed for the 16-bit RISC-V processor in the HDLB0 project. It provides basic file operations while maintaining compatibility with the limited resources of a 16-bit system.

## Features
- Basic file operations: create, delete, read, write
- Directory listing
- File allocation table (FAT) for block management
- Fixed-size sectors (16 bytes each)
- Support for up to 8 files in the root directory
- Maximum file size of 128 bytes

## Architecture
- **Sector Size**: 16 bytes (matches the 16-bit data path)
- **Maximum Files**: 8 files in root directory
- **Maximum File Size**: 128 bytes
- **File Allocation**: FAT-based with 16 total blocks
- **Memory Layout**:
  - File System Start: 0x100
  - FAT Start: 0x080
  - Root Directory: 0x0C0

## Data Structures
- `file_entry`: 16-byte structure containing file metadata
- `fat`: Array of 16 entries tracking block allocation
- `root_dir`: Array of 8 entries for directory entries

## System Calls
- `FS_CREATE_FILE` (0x10): Create a new file
- `FS_DELETE_FILE` (0x11): Delete an existing file
- `FS_READ_FILE` (0x12): Read data from a file
- `FS_WRITE_FILE` (0x13): Write data to a file
- `FS_LIST_FILES` (0x14): List all files in the root directory

## Memory Layout
The file system uses a simple memory-based approach where:
- FAT is stored starting at memory address 0x080
- Root directory is stored starting at memory address 0x0C0
- File data is stored starting at memory address 0x100

## Limitations
- No subdirectories
- Fixed maximum file size
- No file permissions beyond basic attributes
- No wear leveling or advanced features

## Integration with Kernel
The file system is designed to be integrated with the HDLB0 kernel through system calls. The kernel provides the memory management and interrupt handling, while this file system handles the logical file operations.