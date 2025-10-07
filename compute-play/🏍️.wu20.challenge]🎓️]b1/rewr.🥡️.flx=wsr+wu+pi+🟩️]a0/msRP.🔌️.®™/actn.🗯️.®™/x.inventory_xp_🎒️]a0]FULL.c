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

// --- Functionality from 0.find.🥡️.top]a0]PURE.c ---

int has_emoji(const char *name, const char *emoji) {
    return strstr(name, emoji) != NULL;
}

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


// --- Functionality from #.69.list+dirs🔦️[.]+filt]ff7.c ---

char dir_map[MAX_DIRS][MAX_PATH_LEN];
int dir_count = 0;

int is_directory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return 0;
    }
    return S_ISDIR(statbuf.st_mode);
}

void list_directories_recursive(const char *base_path) {
    if (dir_count >= MAX_DIRS) {
        return;
    }

    DIR *dir = opendir(base_path);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);

        if (is_directory(path)) {
            strncpy(dir_map[dir_count], path, MAX_PATH_LEN);
            dir_count++;
            list_directories_recursive(path);
        }
    }
    closedir(dir);
}

// --- New combined functionality ---

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

int contains_any_emoji(const char *str, char emojis[MAX_EMOJIS][10], int emoji_count) {
    for (int i = 0; i < emoji_count; i++) {
        if (strstr(str, emojis[i])) {
            return 1;
        }
    }
    return 0;
}

int main() {
    // Step 1: Find the top directory
    char *top_dir = find_emoji_dir("🥡️");
    if (!top_dir) {
        fprintf(stderr, "No directory with 🥡️ emoji found.\n");
        return 1;
    }

    // Step 2: List all directories recursively
    list_directories_recursive(top_dir);
    free(top_dir);

    // Step 3: Read emojis from inventory_lut.txt
    char emojis[MAX_EMOJIS][10];
    int emoji_count = read_emojis("inventory_lut.txt", emojis);
    if (emoji_count == 0) {
        fprintf(stderr, "No emojis found in inventory_lut.txt\n");
        return 1;
    }

    // Step 4: Filter directories and write to inventory_map.txt
    FILE *inventory_map_file = fopen("inventory_map.txt", "w");
    if (!inventory_map_file) {
        perror("fopen inventory_map.txt");
        return 1;
    }

    char inventory_dirs[MAX_DIRS][MAX_PATH_LEN];
    int inventory_dir_count = 0;

    for (int i = 0; i < dir_count; i++) {
        if (contains_any_emoji(dir_map[i], emojis, emoji_count)) {
            if (inventory_dir_count < MAX_DIRS) {
                strcpy(inventory_dirs[inventory_dir_count], dir_map[i]);
                fprintf(inventory_map_file, "%s\n", dir_map[i]);
                inventory_dir_count++;
            }
        }
    }
    fclose(inventory_map_file);

    // Step 5: Let user choose a directory
    printf("Available directories:\n");
    for (int i = 0; i < inventory_dir_count; i++) {
        printf("%d: %s\n", i, inventory_dirs[i]);
    }

    int choice;
    printf("Enter the index of the directory you want to select: ");
    scanf("%d", &choice);

    if (choice >= 0 && choice < inventory_dir_count) {
        printf("You selected: %s\n", inventory_dirs[choice]);
    } else {
        fprintf(stderr, "Invalid index\n");
    }

    return 0;
}
