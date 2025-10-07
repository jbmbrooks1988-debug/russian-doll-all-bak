#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_PATH 1024

// Function to list files with indexes
void list_files() {
    DIR *dir = opendir(".");
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    int index = 1;
    printf("\nFiles and directories:\n");
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') { // Skip hidden files
            printf("[%d] %s\n", index++, entry->d_name);
        }
    }
    closedir(dir);
}

// Function for readline completion
char *filename_generator(const char *text, int state) {
    static DIR *dir;
    static struct dirent *entry;

    if (!state) {
        dir = opendir(".");
        if (!dir) return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue; // Skip hidden files
        if (strncmp(entry->d_name, text, strlen(text)) == 0) {
            return strdup(entry->d_name);
        }
    }
    closedir(dir);
    return NULL;
}

char **completion(const char *text, int start, int end) {
    rl_attempted_completion_over = 1; // Disable default completion
    return rl_completion_matches(text, filename_generator);
}

int main() {
    char *input;
    char cwd[MAX_PATH];

    // Set up readline completion
    rl_attempted_completion_function = completion;

    while (1) {
        // Get and display current directory
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("getcwd");
            exit(1);
        }
        printf("\nCurrent directory: %s\n", cwd);

        // List files with indexes
        list_files();

        // Prompt for input
        input = readline("Enter index or filename (or command): ");
        if (!input) break; // EOF or error
        if (strlen(input) == 0) {
            free(input);
            continue;
        }
        add_history(input); // Add to history for up/down arrow

        // Trim whitespace
        char *trimmed = input;
        while (*trimmed == ' ') trimmed++;
        char *end = trimmed + strlen(trimmed) - 1;
        while (end > trimmed && *end == ' ') *end-- = '\0';

        // Check if input is a number (index)
        char *endptr;
        long index = strtol(trimmed, &endptr, 10);
        if (*endptr == '\0' && index > 0) {
            // Convert index to filename
            DIR *dir = opendir(".");
            if (!dir) {
                perror("opendir");
                free(input);
                continue;
            }
            struct dirent *entry;
            int current = 1;
            char *filename = NULL;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                if (current == index) {
                    filename = entry->d_name;
                    break;
                }
                current++;
            }
            closedir(dir);

            if (filename) {
                if (access(filename, X_OK) == 0) { // Check if executable
                    char cmd[MAX_PATH];
                    snprintf(cmd, sizeof(cmd), "./%s", filename);
                    system(cmd);
                } else if (chdir(filename) == 0) { // Check if directory
                    continue; // Directory changed, loop continues
                } else {
                    printf("Not an executable or directory: %s\n", filename);
                }
            } else {
                printf("Invalid index: %ld\n", index);
            }
        } else {
            // Try as a filename or command
            if (access(trimmed, X_OK) == 0) { // Executable file
                char cmd[MAX_PATH];
                snprintf(cmd, sizeof(cmd), "./%s", trimmed);
                system(cmd);
            } else if (chdir(trimmed) == 0) { // Directory
                continue; // Directory changed
            } else {
                // Pass to system shell (e.g., for cd .., ls, etc.)
                int ret = system(trimmed);
                if (ret == -1) {
                    printf("Command failed: %s\n", trimmed);
                }
            }
        }
        free(input);
    }
    return 0;
}
