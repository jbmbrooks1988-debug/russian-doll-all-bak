#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#define MAX_EMOJIS 100
#define MAX_DIRS 1024
#define MAX_PATH_LEN 4096

// Check if a string contains a specific emoji
int has_emoji(const char *name, const char *emoji) {
    return strstr(name, emoji) != NULL;
}

// Find the top directory containing the specified emoji
char *find_emoji_dir(const char *emoji) {
    char *cwd = getcwd(NULL, 0);
    if (!cwd) {
        perror("getcwd");
        exit(1);
    }

    char *current_path = strdup(cwd);
    char *root = "/";

    while (strcmp(current_path, root) != 0) {
        char *dir_name = strrchr(current_path, '/');
        dir_name = dir_name ? dir_name + 1 : current_path;

        if (has_emoji(dir_name, emoji)) {
            free(cwd);
            return current_path;
        }

        char *last_slash = strrchr(current_path, '/');
        if (last_slash == current_path) { // at root
            *(last_slash + 1) = '\0';
        } else {
            *last_slash = '\0';
        }
    }

    free(current_path);
    free(cwd);
    return NULL;
}

// Read emojis from interact_lut.txt
int read_emojis(const char *filename, char emojis[MAX_EMOJIS][10]) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return 0;
    }
    int count = 0;
    while (count < MAX_EMOJIS && fscanf(file, "%s", emojis[count]) == 1) {
        count++;
    }
    fclose(file);
    return count;
}

// Check if a string contains any emoji from the list
int contains_any_emoji(const char *str, char emojis[MAX_EMOJIS][10], int emoji_count) {
    for (int i = 0; i < emoji_count; i++) {
        if (strstr(str, emojis[i])) {
            return 1;
        }
    }
    return 0;
}

// Check if a path is a directory
int is_directory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return 0;
    }
    return S_ISDIR(statbuf.st_mode);
}

int main() {
    // Read emojis from interact_lut.txt
    char emojis[MAX_EMOJIS][10];
    int emoji_count = read_emojis("interact_lut.txt", emojis);
    if (emoji_count == 0) {
        fprintf(stderr, "No emojis found in interact_lut.txt\n");
        return 1;
    }

    // Find the top directory containing 🥡️
    char *top_dir = find_emoji_dir("🥡️");
    if (!top_dir) {
        fprintf(stderr, "No directory with 🥡️ emoji found.\n");
        return 1;
    }

    // Open the top directory and check its immediate subdirectories
    DIR *dir = opendir(top_dir);
    if (!dir) {
        perror("opendir");
        free(top_dir);
        return 1;
    }

    struct dirent *entry;
    char found_dir[MAX_PATH_LEN] = {0};
    int found = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/%s", top_dir, entry->d_name);

        if (is_directory(path) && contains_any_emoji(entry->d_name, emojis, emoji_count)) {
            strncpy(found_dir, path, MAX_PATH_LEN);
            found = 1;
            break; // Stop at the first match
        }
    }
    closedir(dir);

    if (found) {
        printf("Directory: %s\n", found_dir);
    } else {
        fprintf(stderr, "No directory with emojis from interact_lut.txt found.\n");
    }

    free(top_dir);
    return 0;
}
