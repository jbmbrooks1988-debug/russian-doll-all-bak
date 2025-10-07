#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>

#define MAX_PATH_LEN PATH_MAX // Use system-defined max path length

// File pointer for output file
FILE* output_file = NULL;

// Function to display usage options
void print_usage(const char* program_name) {
    fprintf(stderr, "Usage: %s <directory_path> [filter]\n", program_name);
    fprintf(stderr, "Available filters:\n");
    fprintf(stderr, "  size - Display sizes of files and directories\n");
    fprintf(stderr, "If no filter is provided, only the directory structure is shown.\n");
}

// Validate path to prevent shell injection or invalid characters
int is_valid_path(const char* path) {
    if (!path || strlen(path) == 0) return 0;
    // Basic validation: check for null bytes or excessive length
    if (strlen(path) >= MAX_PATH_LEN) {
        fprintf(stderr, "Path too long: %s\n", path);
        return 0;
    }
    return 1;
}

// Function to print to both console and file
void print_to_both(const char* format, ...) {
    va_list args;
    // Print to console
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    // Print to file if open
    if (output_file) {
        va_start(args, format);
        vfprintf(output_file, format, args);
        va_end(args);
    }
}

// Recursively list directories and files with hierarchical index
void list_dir(const char* path, int depth, char* parent_index, int show_size) {
    DIR* dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
        if (output_file) fprintf(output_file, "Error opening %s: %s\n", path, strerror(errno));
        return;
    }

    struct dirent* entry;
    int index = 1; // Initialize index for this directory level
    char full_path[MAX_PATH_LEN];
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        // Construct full path safely
        if (snprintf(full_path, MAX_PATH_LEN, "%s/%s", path, entry->d_name) >= MAX_PATH_LEN) {
            fprintf(stderr, "Path too long: %s/%s\n", path, entry->d_name);
            if (output_file) fprintf(output_file, "Path too long: %s/%s\n", path, entry->d_name);
            continue;
        }

        struct stat st;
        if (stat(full_path, &st) != 0) {
            fprintf(stderr, "Error getting stats for %s: %s\n", full_path, strerror(errno));
            if (output_file) fprintf(output_file, "Error getting stats for %s: %s\n", full_path, strerror(errno));
            continue;
        }

        // Build the current index (e.g., "1.2", "2.1")
        char current_index[256];
        if (depth == 1) {
            snprintf(current_index, sizeof(current_index), "%d", index);
        } else {
            snprintf(current_index, sizeof(current_index), "%s.%d", parent_index, index);
        }

        // Print indentation based on depth
        for (int i = 0; i < depth; i++) print_to_both("  ");
        // Print hierarchical index and name
        print_to_both("[%s] %s", current_index, entry->d_name);
        if (S_ISDIR(st.st_mode)) print_to_both("/");
        // Display size if show_size is enabled
        if (show_size) {
            print_to_both(" (%lld bytes)", (long long)st.st_size);
        }
        print_to_both("\n");

        if (S_ISDIR(st.st_mode)) {
            // Pass the current index as the parent index for the next level
            list_dir(full_path, depth + 1, current_index, show_size);
        }
        index++;
    }
    closedir(dir);
}

int main(int argc, char* argv[]) {
    int show_size = 0;

    // Check arguments
    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return 1;
    }

    // Validate directory path
    if (!is_valid_path(argv[1])) {
        fprintf(stderr, "Invalid directory path: %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    // Parse optional filter argument
    if (argc == 3) {
        if (strcmp(argv[2], "size") == 0) {
            show_size = 1;
        } else {
            fprintf(stderr, "Invalid filter: %s\n", argv[2]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // Open output file
    output_file = fopen("directory_map.txt", "w");
    if (!output_file) {
        fprintf(stderr, "Error opening directory_map.txt: %s\n", strerror(errno));
        return 1;
    }

    // Print directory header
    print_to_both("Directory Contents: %s/\n", argv[1]);
    list_dir(argv[1], 1, "", show_size);

    // Close output file
    if (output_file) {
        fclose(output_file);
        output_file = NULL;
    }
    return 0;
}
