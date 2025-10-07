
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_PATH 1024
#define MAX_INPUT 256
#define MAX_MATCHES 100

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

// Custom filename completion function
void complete_filename(char *input, int *cursor_pos) {
    char *prefix = input;
    char *matches[MAX_MATCHES];
    int match_count = 0;

    // Open current directory
    DIR *dir = opendir(".");
    if (!dir) {
        perror("opendir");
        return;
    }

    // Find matching files
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue; // Skip hidden files
        if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0) {
            if (match_count < MAX_MATCHES) {
                matches[match_count] = strdup(entry->d_name);
                match_count++;
            }
        }
    }
    closedir(dir);

    if (match_count == 0) {
        printf("\nNo matches found. 😕\n");
        return;
    }

    // If one match, complete it
    if (match_count == 1) {
        strcpy(input, matches[0]);
        *cursor_pos = strlen(input);
    } else {
        // List all matches
        printf("\nMatches:\n");
        for (int i = 0; i < match_count; i++) {
            printf("  %s\n", matches[i]);
        }
        // Complete to common prefix
        int common_len = strlen(prefix);
        while (1) {
            char c = matches[0][common_len];
            if (c == '\0') break;
            int same = 1;
            for (int i = 1; i < match_count; i++) {
                if (matches[i][common_len] != c) {
                    same = 0;
                    break;
                }
            }
            if (!same) break;
            common_len++;
        }
        strncpy(input, matches[0], common_len);
        input[common_len] = '\0';
        *cursor_pos = common_len;
    }

    // Clean up
    for (int i = 0; i < match_count; i++) {
        free(matches[i]);
    }
}

int main() {
    char input[MAX_INPUT];
    char cwd[MAX_PATH];
    int cursor_pos = 0;

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
        printf("Enter index, filename, or command (Tab for completion): ");
        fflush(stdout);

        // Custom input handling
        cursor_pos = 0;
        input[0] = '\0';
        while (1) {
            int c = getchar();
            if (c == EOF || c == '\n') {
                input[cursor_pos] = '\0';
                break;
            } else if (c == '\t') { // Tab for completion
                input[cursor_pos] = '\0';
                complete_filename(input, &cursor_pos);
                printf("\rEnter index, filename, or command (Tab for completion): %s", input);
                fflush(stdout);
            } else if (c == 127 || c == '\b') { // Backspace
                if (cursor_pos > 0) {
                    cursor_pos--;
                    input[cursor_pos] = '\0';
                    printf("\rEnter index, filename, or command (Tab for completion): %s ", input);
                    fflush(stdout);
                }
            } else if (isprint(c)) {
                if (cursor_pos < MAX_INPUT - 1) {
                    input[cursor_pos++] = c;
                    input[cursor_pos] = '\0';
                    printf("\rEnter index, filename, or command (Tab for completion): %s", input);
                    fflush(stdout);
                }
            }
        }

        // Skip empty input
        if (strlen(input) == 0) continue;

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
    }
    return 0;
}

