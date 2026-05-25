// Simple File System Interface for 16-bit RISC-V Processor
// Provides basic file operations that interface with the kernel through system calls

#include "simple_fs.h"

// File system interface functions that make kernel system calls

// Create a new file
int fs_create_file(char* name) {
    // Make a system call to create a file
    register int syscall_num asm("x17") = FS_CREATE_FILE;
    register char* filename asm("x10") = name;
    register int flags asm("x11") = 0;  // Default flags

    // Perform the system call (ECALL)
    asm volatile (
        "ecall"
        : "=r"(syscall_num)  // Output: syscall number gets replaced with return value
        : "r"(syscall_num), "r"(filename), "r"(flags)  // Input: syscall number and args
        : "memory"
    );

    return syscall_num;  // Return value from the kernel
}

// Delete a file
int fs_delete_file(char* name) {
    // Make a system call to delete a file
    register int syscall_num asm("x17") = FS_DELETE_FILE;
    register char* filename asm("x10") = name;

    // Perform the system call (ECALL)
    asm volatile (
        "ecall"
        : "=r"(syscall_num)  // Output: syscall number gets replaced with return value
        : "r"(syscall_num), "r"(filename)  // Input: syscall number and arg
        : "memory"
    );

    return syscall_num;  // Return value from the kernel
}

// Write data to a file
int fs_write_file(char* name, char* data, int size) {
    // Make a system call to write to a file
    register int syscall_num asm("x17") = FS_WRITE_FILE;
    register char* filename asm("x10") = name;
    register char* buffer asm("x11") = data;
    register int count asm("x12") = size;

    // Perform the system call (ECALL)
    asm volatile (
        "ecall"
        : "=r"(syscall_num)  // Output: syscall number gets replaced with return value
        : "r"(syscall_num), "r"(filename), "r"(buffer), "r"(count)  // Input: syscall number and args
        : "memory"
    );

    return syscall_num;  // Return value from the kernel
}

// Read data from a file
int fs_read_file(char* name, char* buffer, int max_size) {
    // Make a system call to read from a file
    register int syscall_num asm("x17") = FS_READ_FILE;
    register char* filename asm("x10") = name;
    register char* buf asm("x11") = buffer;
    register int max_bytes asm("x12") = max_size;

    // Perform the system call (ECALL)
    asm volatile (
        "ecall"
        : "=r"(syscall_num)  // Output: syscall number gets replaced with return value
        : "r"(syscall_num), "r"(filename), "r"(buf), "r"(max_bytes)  // Input: syscall number and args
        : "memory"
    );

    return syscall_num;  // Return value from the kernel
}

// List all files in the root directory
void fs_list_files() {
    // Make a system call to list files
    register int syscall_num asm("x17") = FS_LIST_FILES;

    // Perform the system call (ECALL)
    asm volatile (
        "ecall"
        : "=r"(syscall_num)  // Output: syscall number gets replaced with return value
        : "r"(syscall_num)   // Input: syscall number
        : "memory"
    );
}

// Helper function to compare strings
int compare_strings(char* str1, char* str2, int max_len) {
    for (int i = 0; i < max_len; i++) {
        if (str1[i] != str2[i]) {
            return 0;  // Strings are different
        }
        if (str1[i] == 0) {  // End of string
            return 1;  // Strings match
        }
    }
    return 1;  // Strings match up to max_len
}

// System call handler for file operations (this would be implemented in the kernel)
// The user-level functions above make system calls that are handled by the kernel