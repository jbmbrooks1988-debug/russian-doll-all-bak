#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define MAX_PATH 1024
#define MAX_EXT 10

// List of image file extensions to check📁️
const char *image_extensions[] = {".jpg", ".jpeg", ".png", ".gif", ".bmp", NULL};

// Check if a file has an image extension
int is_image_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;

    for (int i = 0; image_extensions[i]; i++) {
        if (strcasecmp(ext, image_extensions[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// Ensure the destination directory exists in the current working directory
int ensure_destination_dir() {
    char dest_dir[MAX_PATH];
    snprintf(dest_dir, MAX_PATH, "./images_moved");

    struct stat st;
    if (stat(dest_dir, &st) == -1) {
        if (mkdir(dest_dir, 0755) == -1) {
            fprintf(stderr, "Error creating directory %s: %s\n", dest_dir, strerror(errno));
            return 0;
        }
    }
    return 1;
}

// Move a single image file to the destination directory
void move_image_file(const char *src_path) {
    char dest_path[MAX_PATH];
    const char *filename = strrchr(src_path, '/');
    if (!filename) filename = src_path;
    else filename++; // Skip the '/'

    snprintf(dest_path, MAX_PATH, "./images_moved/%s", filename);

    if (rename(src_path, dest_path) == -1) {
        fprintf(stderr, "Error moving %s to %s: %s\n", src_path, dest_path, strerror(errno));
    } else {
        printf("Moved %s to %s\n", src_path, dest_path);
    }
}

// Recursively process a directory
void process_directory(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Error opening directory %s: %s\n", dir_path, strerror(errno));
        return;
    }

    struct dirent *entry;
    char full_path[MAX_PATH];

    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, MAX_PATH, "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == -1) {
            fprintf(stderr, "Error getting stats for %s: %s\n", full_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Recurse into subdirectory
            process_directory(full_path);
        } else if (S_ISREG(st.st_mode) && is_image_file(entry->d_name)) {
            // Move image file
            move_image_file(full_path);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    // Remove trailing slash if present
    char *dir_path = argv[1];
    size_t len = strlen(dir_path);
    if (len > 1 && dir_path[len - 1] == '/') {
        dir_path[len - 1] = '\0';
    }

    // Check if the directory exists
    struct stat st;
    if (stat(dir_path, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s is not a valid directory\n", dir_path);
        return 1;
    }

    // Ensure the images_moved directory exists in the current working directory
    if (!ensure_destination_dir()) {
        return 1;
    }

    // Process the directory
    process_directory(dir_path);

    return 0;
}
