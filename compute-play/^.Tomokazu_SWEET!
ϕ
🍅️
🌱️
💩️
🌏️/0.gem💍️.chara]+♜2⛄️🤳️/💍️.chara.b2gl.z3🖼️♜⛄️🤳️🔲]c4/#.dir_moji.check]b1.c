#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 256

// Check if path is a directory
int is_directory(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int main(int argc, char *argv[]) {
    const char *target_dir = "."; // default

    if (argc > 1) {
        target_dir = argv[1];
    }

    // Validate directory
    if (!is_directory(target_dir)) {
        fprintf(stderr, "Error: '%s' is not a valid directory\n", target_dir);
        return 1;
    }

    DIR *dir = opendir(target_dir);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open directory '%s'\n", target_dir);
        return 1;
    }

    printf("Piece Emojis in '%s':\n", target_dir);
    printf("----------------------------------------\n");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".dir") != NULL) {
            char full_dir_path[512];
            snprintf(full_dir_path, sizeof(full_dir_path), "%s/%s", target_dir, entry->d_name);

            // Confirm it's a directory
            if (!is_directory(full_dir_path)) continue;

            char state_path[512];
            snprintf(state_path, sizeof(state_path), "%s/state.txt", full_dir_path);

            FILE *f = fopen(state_path, "r");
            if (!f) {
                printf("%-30s -> [no state.txt]\n", entry->d_name);
                continue;
            }

            char line[MAX_LINE];
            int found = 0;
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "symbol :", 8) == 0) {
                    char *emoji = line + 8;
                    while (*emoji == ' ') emoji++; // skip spaces

                    // Strip newline
                    char temp[10] = {0};
                    int i = 0;
                    while (emoji[i] && emoji[i] != '\n' && emoji[i] != '\r' && i < 9) {
                        temp[i] = emoji[i];
                        i++;
                    }
                    printf("%-30s -> %s\n", entry->d_name, temp);
                    found = 1;
                    break;
                }
            }
            fclose(f);

            if (!found) {
                printf("%-30s -> [no emoji]\n", entry->d_name);
            }
        }
    }

    closedir(dir);
    return 0;
}
