#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

// Function to display usage options
void print_usage(const char* program_name) {
    printf("Usage: %s <directory_path> [filter]\n", program_name);
    printf("Available filters:\n");
    printf("  size - Display sizes of files and directories\n");
    printf("If no filter is provided, only the directory structure is shown.\n");
}

// Recursively list directories and files with hierarchical index
void list_dir(const char* path, int depth, char* parent_index, int show_size) {
    DIR* dir = opendir(path);
    if (!dir) {
        printf("Error opening %s\n", path);
        return;
    }

    struct dirent* entry;
    int index = 1; // Initialize index for this directory level
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) {
            printf("Error getting stats for %s\n", full_path);
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
        for (int i = 0; i < depth; i++) printf("  ");
        // Print hierarchical index and name
        printf("[%s] %s", current_index, entry->d_name);
        if (S_ISDIR(st.st_mode)) printf("/");

        // Display size if show_size is enabled
        if (show_size) {
            printf(" (%lld bytes)", (long long)st.st_size);
        }

        printf("\n");

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

    // Parse optional filter argument
    if (argc == 3) {
        if (strcmp(argv[2], "size") == 0) {
            show_size = 1;
        } else {
            printf("Invalid filter: %s\n", argv[2]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // Use the provided directory path directly
    printf("Directory Contents: %s/\n", argv[1]);
    list_dir(argv[1], 1, "", show_size);
    return 0;
}
