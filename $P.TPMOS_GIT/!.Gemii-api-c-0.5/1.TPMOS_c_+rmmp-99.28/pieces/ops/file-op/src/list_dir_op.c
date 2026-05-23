#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH 4096

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <dir_path>\n", argv[0]);
        return 1;
    }

    const char *dir_path = argv[1];
    DIR *d = opendir(dir_path);
    if (!d) {
        perror("opendir");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                printf("[DIR] %s/\n", entry->d_name);
            } else {
                long size = st.st_size;
                char size_str[32];
                if (size < 1024) snprintf(size_str, sizeof(size_str), "%ldB", size);
                else if (size < 1024 * 1024) snprintf(size_str, sizeof(size_str), "%ldKB", size / 1024);
                else snprintf(size_str, sizeof(size_str), "%.1fMB", (float)size / (1024 * 1024));
                printf("[FIL] %s (%s)\n", entry->d_name, size_str);
            }
        }
    }
    closedir(d);
    return 0;
}
