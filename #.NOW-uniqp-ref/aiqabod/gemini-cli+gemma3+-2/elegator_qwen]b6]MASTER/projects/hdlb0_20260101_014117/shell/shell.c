// Simple Shell for 16-bit RISC-V Processor
// Provides a command-line interface for interacting with the system through kernel

#include "../file_system/simple_fs.h"

#define MAX_CMD_LEN 16
#define MAX_ARGS 4
#define MAX_ARG_LEN 8

// Command structure
struct command {
    char name[MAX_ARG_LEN];
    int (*handler)(char args[MAX_ARGS][MAX_ARG_LEN]);
};

// Function prototypes for command handlers
int cmd_help(char args[MAX_ARGS][MAX_ARG_LEN]);
int cmd_ls(char args[MAX_ARGS][MAX_ARG_LEN]);
int cmd_cat(char args[MAX_ARGS][MAX_ARG_LEN]);
int cmd_create(char args[MAX_ARGS][MAX_ARG_LEN]);
int cmd_write(char args[MAX_ARGS][MAX_ARG_LEN]);
int cmd_delete(char args[MAX_ARGS][MAX_ARG_LEN]);
int cmd_clear(char args[MAX_ARGS][MAX_ARG_LEN]);
int cmd_exit(char args[MAX_ARGS][MAX_ARG_LEN]);

// Command table
struct command commands[] = {
    {"help", cmd_help},
    {"ls", cmd_ls},
    {"dir", cmd_ls},      // Alias for ls
    {"cat", cmd_cat},
    {"create", cmd_create},
    {"write", cmd_write},
    {"del", cmd_delete},
    {"rm", cmd_delete},   // Alias for delete
    {"clear", cmd_clear},
    {"cls", cmd_clear},   // Alias for clear
    {"exit", cmd_exit},
    {"quit", cmd_exit},   // Alias for exit
    {"", NULL}            // End marker
};

// Main shell loop
void shell_main() {
    char input[MAX_CMD_LEN];
    char cmd[MAX_ARG_LEN];
    char args[MAX_ARGS][MAX_ARG_LEN];

    // Print welcome message through kernel
    kernel_print_string("HDLB0 Shell v1.0\n");
    kernel_print_string("Type 'help' for available commands\n");

    while(1) {
        // Print prompt
        kernel_print_string("> ");

        // Get user input through kernel
        kernel_read_line(input, MAX_CMD_LEN);

        // Parse the command and arguments
        if (parse_command(input, cmd, args)) {
            // Execute the command
            execute_command(cmd, args);
        }
    }
}

// Parse command line into command and arguments
int parse_command(char* input, char* cmd, char args[MAX_ARGS][MAX_ARG_LEN]) {
    int i = 0, arg_idx = 0, char_idx = 0;
    int in_word = 0;

    // Initialize all argument strings to empty
    for (int j = 0; j < MAX_ARGS; j++) {
        for (int k = 0; k < MAX_ARG_LEN; k++) {
            args[j][k] = 0;
        }
    }

    // Skip leading spaces
    while (input[i] == ' ') i++;

    // Extract command
    int cmd_idx = 0;
    while (input[i] != ' ' && input[i] != '\0' && input[i] != '\n') {
        if (cmd_idx < MAX_ARG_LEN - 1) {
            cmd[cmd_idx++] = input[i];
        }
        i++;
    }
    cmd[cmd_idx] = '\0';

    // Skip spaces between command and arguments
    while (input[i] == ' ') i++;

    // Extract arguments
    arg_idx = 0;
    char_idx = 0;

    while (input[i] != '\0' && input[i] != '\n' && arg_idx < MAX_ARGS) {
        if (input[i] == ' ') {
            if (in_word) {
                args[arg_idx][char_idx] = '\0';
                arg_idx++;
                char_idx = 0;
                in_word = 0;
            }
        } else {
            if (!in_word) {
                in_word = 1;
            }
            if (char_idx < MAX_ARG_LEN - 1) {
                args[arg_idx][char_idx] = input[i];
                char_idx++;
            }
        }
        i++;
    }

    if (in_word && char_idx > 0) {
        args[arg_idx][char_idx] = '\0';
        arg_idx++;
    }

    // Null-terminate command if needed
    if (cmd_idx == 0) {
        cmd[0] = '\0';
        return 0;  // No command found
    }

    return 1;  // Command found
}

// Execute a command with its arguments
int execute_command(char* cmd, char args[MAX_ARGS][MAX_ARG_LEN]) {
    // Find the command in the command table
    for (int i = 0; commands[i].handler != NULL; i++) {
        if (compare_strings(commands[i].name, cmd, MAX_ARG_LEN)) {
            // Call the command handler
            return commands[i].handler(args);
        }
    }

    // Command not found
    kernel_print_string("Unknown command: ");
    kernel_print_string(cmd);
    kernel_print_string("\n");
    return -1;
}

// Command handlers
int cmd_help(char args[MAX_ARGS][MAX_ARG_LEN]) {
    kernel_print_string("Available commands:\n");
    kernel_print_string("  help    - Show this help message\n");
    kernel_print_string("  ls/dir  - List files in root directory\n");
    kernel_print_string("  cat     - Display contents of a file\n");
    kernel_print_string("  create  - Create a new file\n");
    kernel_print_string("  write   - Write data to a file\n");
    kernel_print_string("  del/rm  - Delete a file\n");
    kernel_print_string("  clear/cls - Clear the screen\n");
    kernel_print_string("  exit/quit - Exit the shell\n");
    return 0;
}

int cmd_ls(char args[MAX_ARGS][MAX_ARG_LEN]) {
    // Call the file system to list files through kernel
    fs_list_files();
    return 0;
}

int cmd_cat(char args[MAX_ARGS][MAX_ARG_LEN]) {
    if (args[0][0] == '\0') {
        kernel_print_string("Usage: cat <filename>\n");
        return -1;
    }

    char buffer[MAX_FILE_SIZE];
    int bytes_read = fs_read_file(args[0], buffer, MAX_FILE_SIZE);

    if (bytes_read < 0) {
        kernel_print_string("Error: File not found or could not be read\n");
        return -1;
    }

    // Print the file contents
    for (int i = 0; i < bytes_read; i++) {
        kernel_print_char(buffer[i]);
    }
    kernel_print_string("\n");

    return 0;
}

int cmd_create(char args[MAX_ARGS][MAX_ARG_LEN]) {
    if (args[0][0] == '\0') {
        kernel_print_string("Usage: create <filename>\n");
        return -1;
    }

    int result = fs_create_file(args[0]);
    if (result < 0) {
        kernel_print_string("Error: Could not create file\n");
        return -1;
    }

    kernel_print_string("File created: ");
    kernel_print_string(args[0]);
    kernel_print_string("\n");
    return 0;
}

int cmd_write(char args[MAX_ARGS][MAX_ARG_LEN]) {
    if (args[0][0] == '\0' || args[1][0] == '\0') {
        kernel_print_string("Usage: write <filename> <data>\n");
        return -1;
    }

    int result = fs_write_file(args[0], args[1], string_length(args[1]));
    if (result < 0) {
        kernel_print_string("Error: Could not write to file\n");
        return -1;
    }

    kernel_print_string("Wrote ");
    kernel_print_int(result);
    kernel_print_string(" bytes to ");
    kernel_print_string(args[0]);
    kernel_print_string("\n");
    return 0;
}

int cmd_delete(char args[MAX_ARGS][MAX_ARG_LEN]) {
    if (args[0][0] == '\0') {
        kernel_print_string("Usage: del <filename>\n");
        return -1;
    }

    int result = fs_delete_file(args[0]);
    if (result < 0) {
        kernel_print_string("Error: Could not delete file\n");
        return -1;
    }

    kernel_print_string("File deleted: ");
    kernel_print_string(args[0]);
    kernel_print_string("\n");
    return 0;
}

int cmd_clear(char args[MAX_ARGS][MAX_ARG_LEN]) {
    // For a simple terminal, just print a bunch of newlines
    for (int i = 0; i < 24; i++) {
        kernel_print_string("\n");
    }
    return 0;
}

int cmd_exit(char args[MAX_ARGS][MAX_ARG_LEN]) {
    kernel_print_string("Exiting shell...\n");
    // In a real system, this might trigger a system call to exit
    while(1);  // Infinite loop to stop execution
    return 0;
}

// Helper function to get string length
int string_length(char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

// Kernel interface functions
void kernel_print_string(char* str) {
    // Make a system call to print a string
    register int syscall_num asm("x17") = 1;  // SYS_WRITE
    register char* buffer asm("x10") = str;
    register int length asm("x11") = string_length(str);

    asm volatile (
        "ecall"
        :
        : "r"(syscall_num), "r"(buffer), "r"(length)
        : "memory"
    );
}

void kernel_print_char(char c) {
    // Make a system call to print a character
    register int syscall_num asm("x17") = 1;  // SYS_WRITE
    register char* buffer asm("x10") = &c;
    register int length asm("x11") = 1;

    asm volatile (
        "ecall"
        :
        : "r"(syscall_num), "r"(buffer), "r"(length)
        : "memory"
    );
}

void kernel_print_int(int num) {
    // For simplicity, we'll just print a placeholder
    // In a real implementation, this would convert the integer to string and print it
    kernel_print_string("INT");
}

int kernel_read_line(char* buffer, int max_len) {
    // Make a system call to read a line from input
    register int syscall_num asm("x17") = 2;  // SYS_READ
    register char* buf asm("x10") = buffer;
    register int max_bytes asm("x11") = max_len;

    asm volatile (
        "ecall"
        : "=r"(syscall_num)
        : "r"(syscall_num), "r"(buf), "r"(max_bytes)
        : "memory"
    );

    return syscall_num;  // Return number of bytes read
}

// This function would be called by the kernel to start the shell
void start_shell() {
    shell_main();
}