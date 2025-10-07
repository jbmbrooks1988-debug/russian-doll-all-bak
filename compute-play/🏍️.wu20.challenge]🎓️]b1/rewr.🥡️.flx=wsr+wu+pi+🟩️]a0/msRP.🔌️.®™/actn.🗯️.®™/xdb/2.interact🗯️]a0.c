#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 256
#define MAX_PATH 512

int main(int argc, char *argv[]) {
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
            
            // Execute the command
            int result = system(command);
            if (result != 0) {
                fprintf(stderr, "Error executing command: %s\n", command);
            }
        }
    }

    fclose(file);
    return 0;
}
