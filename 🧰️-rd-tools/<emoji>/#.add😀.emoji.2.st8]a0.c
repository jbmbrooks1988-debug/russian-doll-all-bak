#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// List of fun emojis to choose from (UTF-8 encoded)
const char* emojis[] = {
    "😀", "🎉", "🌟", "🚀", "🔥", "🌈", "🎁", "🎈", "⭐", "🦄",
    "🍕", "🎮", "🎧", "💫", "🎊", "🌻", "🐶", "🐱", "🍦", "📂️"
};
#define NUM_EMOJIS (sizeof(emojis) / sizeof(emojis[0]))

// Function to check if a path is a directory
int is_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

// Function to append random emoji to state.txt
void append_emoji_to_file(const char* filepath) {
    FILE* file = fopen(filepath, "a"); // Open in append mode
    if (!file) {
        perror("fopen append");
        return;
    }

    // Pick a random emoji
    int idx = rand() % NUM_EMOJIS;
    fprintf(file, "symbol : %s\n", emojis[idx]);
    fclose(file);

    printf("Updated: %s with emoji %s\n", filepath, emojis[idx]);
}

// Recursive function to traverse directory
void search_directory(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent* entry;
    char full_path[1024];

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full path
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (entry->d_type == DT_REG) {
            // Regular file: check if it's "state.txt"
            if (strcmp(entry->d_name, "state.txt") == 0) {
                append_emoji_to_file(full_path);
            }
        } else if (entry->d_type == DT_DIR || (entry->d_type == DT_UNKNOWN && is_directory(full_path))) {
            // Subdirectory: recurse into it
            search_directory(full_path);
        }
    }

    closedir(dir);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    const char* root_dir = argv[1];

    // Validate root directory exists and is a directory
    if (!is_directory(root_dir)) {
        fprintf(stderr, "Error: '%s' is not a valid directory\n", root_dir);
        return 1;
    }

    // Seed random number generator
    srand((unsigned int)time(NULL));

    // Start recursive search
    search_directory(root_dir);

    return 0;
}
