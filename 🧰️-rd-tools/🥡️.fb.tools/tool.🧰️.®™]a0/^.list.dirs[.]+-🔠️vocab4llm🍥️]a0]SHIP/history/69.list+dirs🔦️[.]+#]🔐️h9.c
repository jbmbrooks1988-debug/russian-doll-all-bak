#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <time.h>

#define MAX_PATH_LEN PATH_MAX // Use system-defined max path length

// File pointer for output file
FILE* output_file = NULL;

// Simple 8-character alphanumeric hash function
void simple_hash(const char* name, long long size, char* hash_output) {
    unsigned long hash = 5381;
    int c;
    while ((c = *name++)) {
        hash = ((hash << 5) + hash) + c; // djb2 algorithm
    }
    hash = hash ^ (unsigned long)size; // Incorporate size
    char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    hash_output[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        hash_output[i] = charset[hash % 62];
        hash >>= 5;
    }
}

// Function to display usage options
void print_usage(const char* program_name) {
    fprintf(stderr, "Usage: %s <directory_path>\n", program_name);
    fprintf(stderr, "Displays directory structure with sizes and hashes.\n");
}

// Validate path to prevent shell injection or invalid characters
int is_valid_path(const char* path) {
    if (!path || strlen(path) == 0) return 0;
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

// Recursively list directories and files with hierarchical index, summing sizes
long long list_dir(const char* path, int depth, char* parent_index, int* index) {
    DIR* dir = opendir(path);
    long long total_size = 0;
    if (!dir) {
        fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
        if (output_file) fprintf(output_file, "Error opening %s: %s\n", path, strerror(errno));
        return 0;
    }

    struct dirent* entry;
    char full_path[MAX_PATH_LEN];
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (strcmp(entry->d_name, "directory_map.txt") == 0) continue; // Skip output file

        // Construct full path safely
        if (snprintf(full_path, MAX_PATH_LEN, "%s/%s", path, entry->d_name) >= MAX_PATH_LEN) {
            fprintf(stderr, "Path too long: %s/%s\n", path, entry->d_name);
            if (output_file) fprintf(output_file, "Path too long: %s/%s\n", path, entry->d_name);
            continue;
        }

        struct stat st;
        if (lstat(full_path, &st) != 0) { // Use lstat to avoid following symlinks
            fprintf(stderr, "Error getting stats for %s: %s\n", full_path, strerror(errno));
            if (output_file) fprintf(output_file, "Error getting stats for %s: %s\n", full_path, strerror(errno));
            continue;
        }

        // Build the current index (e.g., "1.2", "2.1")
        char current_index[256];
        if (depth == 1) {
            snprintf(current_index, sizeof(current_index), "%d", *index);
        } else {
            snprintf(current_index, sizeof(current_index), "%s.%d", parent_index, *index);
        }

        // Print indentation based on depth
        for (int i = 0; i < depth; i++) print_to_both("  ");
        // Print hierarchical index and name
        print_to_both("[%s] %s", current_index, entry->d_name);
        if (S_ISDIR(st.st_mode)) print_to_both("/");
        if (S_ISLNK(st.st_mode)) print_to_both(" -> symlink");
        // Always display size and hash
        char hash[9];
        simple_hash(entry->d_name, st.st_size, hash);
        print_to_both(" (%lld bytes, hash: %s)", (long long)st.st_size, hash);
        total_size += st.st_size;
        print_to_both("\n");

        if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) { // Don't follow symlinks
            // Pass the current index as the parent index for the next level
            total_size += list_dir(full_path, depth + 1, current_index, index);
        }
        (*index)++;
    }
    closedir(dir);
    return total_size;
}

int main(int argc, char* argv[]) {
    // Check arguments
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    // Validate directory path
    if (!is_valid_path(argv[1])) {
        fprintf(stderr, "Invalid directory path: %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    // Open output file
    output_file = fopen("directory_map.txt", "w");
    if (!output_file) {
        fprintf(stderr, "Error opening directory_map.txt: %s\n", strerror(errno));
        return 1;
    }

    // Print current date and time
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
    print_to_both("File Name/size hashes]Generated on: %s\n", time_str);

    // Get stats for the master directory
    struct stat st;
    if (lstat(argv[1], &st) != 0) { // Use lstat to avoid following symlinks
        fprintf(stderr, "Error getting stats for %s: %s\n", argv[1], strerror(errno));
        if (output_file) fprintf(output_file, "Error getting stats for %s: %s\n", argv[1], strerror(errno));
        fclose(output_file);
        output_file = NULL;
        return 1;
    }

    // Print directory header
    print_to_both("Directory Contents: %s/", argv[1]);
    long long total_size = st.st_size; // Start with metadata size
    char hash[9];
    simple_hash(argv[1], st.st_size, hash);
    print_to_both(" (%lld bytes, hash: %s)", (long long)st.st_size, hash);
    print_to_both("\n");

    // List directory contents and calculate total size
    int index = 1;
    total_size += list_dir(argv[1], 1, "", &index);
    // Print total recursive size
    print_to_both("Total recursive size: %lld bytes\n", total_size);

    // Flush and close output file
    if (output_file) {
        fflush(output_file); // Ensure all writes are committed
        fclose(output_file);
        output_file = NULL;
    }
    return 0;
}
