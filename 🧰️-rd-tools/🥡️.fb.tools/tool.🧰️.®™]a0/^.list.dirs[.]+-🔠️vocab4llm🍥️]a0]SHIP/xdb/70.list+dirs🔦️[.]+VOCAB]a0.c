#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_PATH 1024
#define MAX_FILES 1000
#define MAX_FILENAME 256
#define MAX_LINE 1024

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
    char dir_path[MAX_PATH] = "";
    int dir_set = 0;

    // Parse directory_map.txt for file names and directory path
    while (fgets(line, MAX_LINE, map) != NULL && file_count < num_files) {
        // Extract directory path from first line
        if (!dir_set && strstr(line, "Directory Contents:") != NULL) {
            char *path_start = strchr(line, '.');
            if (path_start) {
                strncpy(dir_path, path_start, MAX_PATH - 1);
                dir_path[MAX_PATH - 1] = '\0';
                // Remove trailing newline or spaces
                for (int i = strlen(dir_path) - 1; i >= 0; i--) {
                    if (dir_path[i] == '\n' || dir_path[i] == ' ') {
                        dir_path[i] = '\0';
                    } else {
                        break;
                    }
                }
                dir_set = 1;
            }
            continue;
        }

        // Extract file names (skip lines that don't start with [number])
        if (line[0] == '[' && file_count < num_files) {
            char *file_name = strchr(line, ']');
            if (file_name) {
                file_name++; // Skip ']'
                while (*file_name == ' ') file_name++; // Skip spaces
                char *end = strchr(file_name, '\n');
                if (end) *end = '\0'; // Remove newline
                if (strlen(file_name) > 0 && file_count < MAX_FILES) {
                    strncpy(file_list[file_count], file_name, MAX_FILENAME - 1);
                    file_list[file_count][MAX_FILENAME - 1] = '\0';
                    file_count++;
                }
            }
        }
    }
    fclose(map);

    if (!dir_set) {
        printf("Error: Could not find directory path in %s\n", map_file);
        return 1;
    }

    // Open output file
    char output_file[] = "vocab.txt";
    FILE *out = fopen(output_file, "w");
    if (!out) {
        printf("Error: Cannot create %s\n", output_file);
        return 1;
    }

    // Write directory name
    fprintf(out, "Directory: %s\n\n", dir_path);

    // Process each file
    for (int i = 0; i < file_count; i++) {
        char file_path[MAX_PATH];
        snprintf(file_path, MAX_PATH, "%s/%s", dir_path, file_list[i]);

        // Write file index and name
        fprintf(out, "File_%d: %s\n", i, file_list[i]);
        fprintf(out, "Content:\n");

        // Read and write file contents
        FILE *in = fopen(file_path, "r");
        if (in) {
            char line_content[MAX_LINE];
            while (fgets(line_content, MAX_LINE, in) != NULL) {
                fputs(line_content, out);
            }
            fclose(in);
        } else {
            fprintf(out, "Error: Could not read file\n");
        }
        fprintf(out, "\n");
    }

    fclose(out);
    printf("Index created in %s\n", output_file);
    return 0;
}
