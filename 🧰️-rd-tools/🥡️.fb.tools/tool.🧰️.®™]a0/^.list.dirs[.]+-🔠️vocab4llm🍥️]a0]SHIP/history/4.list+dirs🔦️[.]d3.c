#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

// Recursively list directories and files with hierarchical index
void list_dir(const char* path, int depth, char* parent_index) {
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
        stat(full_path, &st);

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

        printf("\n");

        if (S_ISDIR(st.st_mode)) {
            // Pass the current index as the parent index for the next level
            list_dir(full_path, depth + 1, current_index);
        }
        index++;
    }
    closedir(dir);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <directory_path>\n", argv[0]);
        return 1;
    }

    // Use the provided directory path directly
    printf("Directory Contents: %s/\n", argv[1]);
    list_dir(argv[1], 1, "");
    return 0;
}
