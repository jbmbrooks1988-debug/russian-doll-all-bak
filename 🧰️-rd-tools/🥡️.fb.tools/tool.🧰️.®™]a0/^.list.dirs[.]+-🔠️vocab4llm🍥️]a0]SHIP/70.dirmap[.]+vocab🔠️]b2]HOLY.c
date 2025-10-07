#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_PATH 1024
#define MAX_FILES 1000
#define MAX_FILENAME 256
#define MAX_LINE 1024
#define MAX_TOKEN 256
#define MAX_BUFFER 1048576 // 1MB buffer for fread

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <directory_map_file> <num_files>\n", argv[0]);
        return 1;
    }

    char *map_file = argv[1];
    int num_files = atoi(argv[2]);
    if (num_files <= 0 || num_files > MAX_FILES) {
        printf("Error: Invalid number of files (1 to %d)\n", MAX_FILES);
        return 1;
    }

    char file_list[MAX_FILES][MAX_FILENAME];
    int file_count = 0;

    // Read directory_map.txt
    FILE *map = fopen(map_file, "r");
    if (!map) {
        printf("Error: Cannot open %s\n", map_file);
        return 1;
    }

    char line[MAX_LINE];
    printf("Debug: Starting to parse %s\n", map_file);

    // Parse file names
    while (fgets(line, MAX_LINE, map) != NULL && file_count < num_files) {
        line[strcspn(line, "\n")] = '\0'; // Remove newline
        printf("Debug: Read line: '%s'\n", line);

        // Skip empty lines
        if (strlen(line) == 0) {
            printf("Debug: Skipping empty line\n");
            continue;
        }

        // Find start of '[' after whitespace
        char *index_start = line;
        while (*index_start == ' ' || *index_start == '\t') index_start++;
        if (*index_start != '[') {
            printf("Debug: Skipping line, doesn't start with '[' after whitespace\n");
            continue;
        }

        // Find closing ']'
        char *index_end = strchr(index_start, ']');
        if (!index_end) {
            printf("Debug: Skipping line, no closing ']'\n");
            continue;
        }

        // Extract index
        char index[16];
        int index_len = index_end - index_start + 1;
        if (index_len >= 16) {
            printf("Debug: Skipping line, index too long\n");
            continue;
        }
        strncpy(index, index_start, index_len);
        index[index_len] = '\0';
        printf("Debug: Found index: '%s'\n", index);

        // Get file name
        char *file_name = index_end + 1;
        while (*file_name == ' ' || *file_name == '\t') file_name++;
        if (strlen(file_name) == 0) {
            printf("Debug: Skipping line, no file name\n");
            continue;
        }
        printf("Debug: Found file name: '%s'\n", file_name);

        // Skip directories
        if (file_name[strlen(file_name) - 1] == '/') {
            printf("Debug: Skipping directory '%s'\n", file_name);
            continue;
        }

        // Handle nested files
        char cleaned_name[MAX_FILENAME];
        if (strncmp(index, "[3.", 3) == 0) {
            snprintf(cleaned_name, MAX_FILENAME, "+x/%s", file_name);
            printf("Debug: Added nested file '+x/%s'\n", file_name);
        } else if (strncmp(index, "[6.", 3) == 0) {
            snprintf(cleaned_name, MAX_FILENAME, "history/%s", file_name);
            printf("Debug: Added nested file 'history/%s'\n", file_name);
        } else {
            strncpy(cleaned_name, file_name, MAX_FILENAME - 1);
            cleaned_name[MAX_FILENAME - 1] = '\0';
            printf("Debug: Added top-level file '%s'\n", file_name);
        }

        // Add to file list
        strncpy(file_list[file_count], cleaned_name, MAX_FILENAME - 1);
        file_list[file_count][MAX_FILENAME - 1] = '\0';
        file_count++;
        printf("Debug: File %d added: '%s'\n", file_count, cleaned_name);
    }
    fclose(map);

    if (file_count == 0) {
        printf("Error: No valid files found in %s\n", map_file);
        return 1;
    }

    // Open output file
    FILE *out = fopen("vocab.txt", "w");
    if (!out) {
        printf("Error: Cannot create vocab.txt\n");
        return 1;
    }

    // Process files and write tokens
    int token_index = 1;
    char buffer[MAX_BUFFER];
    char token[MAX_TOKEN];

    for (int i = 0; i < file_count; i++) {
        // Write file name as a token
        fprintf(out, "%d %s\n", token_index++, file_list[i]);
        printf("Debug: Added token %d: '%s'\n", token_index - 1, file_list[i]);

        // Read file contents
        char file_path[MAX_PATH];
        snprintf(file_path, MAX_PATH, "./%s", file_list[i]);
        printf("Debug: Processing file '%s'\n", file_path);

        FILE *in = fopen(file_path, "r");
        if (!in) {
            printf("Debug: Failed to open file '%s'\n", file_path);
            fprintf(out, "%d Error: Could not read file %s\n", token_index++, file_path);
            continue;
        }

        // Read file contents using fread
        size_t bytes_read = fread(buffer, 1, MAX_BUFFER - 1, in);
        buffer[bytes_read] = '\0'; // Null-terminate
        fclose(in);

        // Split contents into tokens (by whitespace, similar to index_0.txt)
        char *ptr = buffer;
        char *token_start = ptr;
        int in_token = 0;

        while (*ptr) {
            if (*ptr == ' ' || *ptr == '\n' || *ptr == '\t') {
                if (in_token) {
                    // End of token
                    size_t len = ptr - token_start;
                    if (len > 0 && len < MAX_TOKEN) {
                        strncpy(token, token_start, len);
                        token[len] = '\0';
                        fprintf(out, "%d %s\n", token_index++, token);
                        printf("Debug: Added token %d: '%s'\n", token_index - 1, token);
                    }
                    in_token = 0;
                }
            } else {
                if (!in_token) {
                    token_start = ptr;
                    in_token = 1;
                }
            }
            ptr++;
        }

        // Handle last token if not empty
        if (in_token) {
            size_t len = ptr - token_start;
            if (len > 0 && len < MAX_TOKEN) {
                strncpy(token, token_start, len);
                token[len] = '\0';
                fprintf(out, "%d %s\n", token_index++, token);
                printf("Debug: Added token %d: '%s'\n", token_index - 1, token);
            }
        }
    }

    fclose(out);
    printf("Index created in vocab.txt\n");
    return 0;
}
