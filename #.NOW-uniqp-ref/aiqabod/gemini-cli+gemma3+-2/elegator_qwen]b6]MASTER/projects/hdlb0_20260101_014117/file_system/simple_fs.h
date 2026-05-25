// Simple File System Header for 16-bit RISC-V Processor
// Defines data structures and constants for the file system

#ifndef SIMPLE_FS_H
#define SIMPLE_FS_H

// File system constants
#define SECTOR_SIZE     16      // Size of each sector in bytes (for our 16-bit system)
#define MAX_FILES       8       // Maximum number of files in the system
#define MAX_FILE_NAME   8       // Maximum length of a file name
#define MAX_FILE_SIZE   128     // Maximum file size in bytes
#define FS_START_ADDR   0x100   // Starting address for file system in memory
#define FAT_START_ADDR  0x080   // Starting address for File Allocation Table
#define ROOT_DIR_ADDR   0x0C0   // Starting address for root directory

// File attributes
#define FILE_ATTR_READ_ONLY  0x01
#define FILE_ATTR_HIDDEN     0x02
#define FILE_ATTR_SYSTEM     0x04
#define FILE_ATTR_DIRECTORY  0x10

// File entry structure (16 bytes total)
struct file_entry {
    char name[MAX_FILE_NAME];   // File name (8 bytes)
    unsigned short size;        // File size in bytes (2 bytes)
    unsigned short start_block; // Starting block number (2 bytes)
    unsigned short attributes;  // File attributes (2 bytes)
    unsigned short reserved;    // Reserved for future use (2 bytes)
};

// FAT entry values
#define FAT_FREE      0x0000    // Block is free
#define FAT_RESERVED  0xFFFF    // Block is reserved
#define FAT_EOF       0xFFFF    // End of file marker

// System call numbers for file operations (as defined in kernel)
#define FS_CREATE_FILE  0x10
#define FS_DELETE_FILE  0x11
#define FS_READ_FILE    0x12
#define FS_WRITE_FILE   0x13
#define FS_LIST_FILES   0x14
#define FS_OPEN_FILE    0x15
#define FS_CLOSE_FILE   0x16

#endif // SIMPLE_FS_H