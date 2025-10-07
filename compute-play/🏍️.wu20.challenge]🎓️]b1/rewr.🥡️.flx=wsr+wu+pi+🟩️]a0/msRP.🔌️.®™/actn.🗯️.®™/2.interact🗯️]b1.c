#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>

#define MAX_LINE 256
#define MAX_PATH 512

int main(int argc, char *argv[]) {
    char exe_path[PATH_MAX];
    realpath(argv[0], exe_path);
    char *exe_dir = dirname(exe_path);
    if (chdir(exe_dir) != 0) {
        perror("chdir failed");
        return 1;
    }

    // Check if directory path is provided
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory_path>\n", argv[0]);
        return 1;
    }

    // Construct path to fx.txt
    char fx_path[MAX_PATH];
    snprintf(fx_path, MAX_PATH, "%s/fx.txt", argv[1]);

    // Open fx.txt
    FILE *file = fopen(fx_path, "r");
    if (!file) {
        perror("Error opening fx.txt");
        return 1;
    }

    // Read and process each line
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, file)) {
        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;

        // Skip empty lines
        if (strlen(line) == 0) continue;

        // Parse line (format: function_name: command)
        char *function_name = strtok(line, ":");
        char *command = strtok(NULL, "");
        
        if (function_name && command) {
            // Remove leading/trailing whitespace from command
            while (*command == ' ') command++;
            
            char full_command[MAX_PATH];
            snprintf(full_command, MAX_PATH, "cd \"%s\" && %s", argv[1], command);
            
            // Execute the command
            int result = system(full_command);
            if (result != 0) {
                fprintf(stderr, "Error executing command: %s\n", full_command);
            }
        }
    }

    fclose(file);
    return 0;
}