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
#define STRICT_EMOJI_FILTER 0 // Set to 1 to only list dirs with emojis, 0 to include their subdirs

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

// --- Modified functionality with toggleable emoji filtering and ®™ requirement ---

char dir_map[MAX_DIRS][MAX_PATH_LEN];
int dir_count = 0;
char emojis[MAX_EMOJIS][10];
int emoji_count = 0;

int is_directory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return 0;
    }
    return S_ISDIR(statbuf.st_mode);
}

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

        if (is_directory(path) && strstr(entry->d_name, "®™")) { // Require ®™ in dir name
            int parent_has_emoji = contains_any_emoji(base_path, emojis, emoji_count);
            if (contains_any_emoji(entry->d_name, emojis, emoji_count) || 
                (!STRICT_EMOJI_FILTER && parent_has_emoji)) {
                strncpy(dir_map[dir_count], path, MAX_PATH_LEN);
                dir_count++;
                list_directories_recursive(path);
            }
        }
    }
    closedir(dir);
}

// Function to read userhash from state.txt
char *get_userhash() {
    FILE *file = fopen("../../state.txt", "r");
    if (!file) {
        perror("fopen ../../state.txt");
        return NULL;
    }

    char line[256];
    char *userhash = NULL;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "userhash", 8) == 0) {
            char *value = strchr(line, ' ');
            if (value) {
                value++; // Move past the space
                userhash = strdup(value);
                // Remove trailing newline
                userhash[strcspn(userhash, "\n")] = 0;
            }
            break;
        }
    }

    fclose(file);
    return userhash;
}

int main() {
    // Step 1: Get userhash
    char *userhash = get_userhash();
    if (!userhash) {
        fprintf(stderr, "Could not get userhash.\n");
        return 1;
    }

    // Step 2: Find the top directory
    char *top_dir = find_emoji_dir("🥡️");
    if (!top_dir) {
        fprintf(stderr, "No directory with 🥡️ emoji found.\n");
        free(userhash);
        return 1;
    }

    // Step 3: Find the user's inventory directory
    char user_inventory_path[MAX_PATH_LEN] = {0};
    DIR *dir = opendir(top_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_DIR) {
                char *trademark_pos = strstr(entry->d_name, "™");
                if (trademark_pos) {
                    size_t length = trademark_pos - entry->d_name + strlen("™");
                    if (strncmp(entry->d_name, userhash, length) == 0) {
                        snprintf(user_inventory_path, sizeof(user_inventory_path), "%s/%s", top_dir, entry->d_name);
                        break;
                    }
                }
            }
        }
        closedir(dir);
    }
    free(top_dir);
    free(userhash);

    if (strlen(user_inventory_path) == 0) {
        fprintf(stderr, "User inventory not found.\n");
        return 1;
    }

    // Step 4: Read emojis from inventory_lut.txt
    emoji_count = read_emojis("inventory_lut.txt", emojis);
    if (emoji_count == 0) {
        fprintf(stderr, "No emojis found in inventory_lut.txt\n");
        return 1;
    }

    // Step 5: List directories with emojis recursively from the user's inventory
    list_directories_recursive(user_inventory_path);

    // Step 6: Write to inventory_map.txt
    FILE *inventory_map_file = fopen("inventory_map.txt", "w");
    if (!inventory_map_file) {
        perror("fopen inventory_map.txt");
        return 1;
    }

    for (int i = 0; i < dir_count; i++) {
        fprintf(inventory_map_file, "%s\n", dir_map[i]);
    }
    fclose(inventory_map_file);

    // Step 7: Let user choose a directory, showing only the last directory name
    printf("Available directories:\n");
    for (int i = 0; i < dir_count; i++) {
        char *last_dir = strrchr(dir_map[i], '/');
        last_dir = last_dir ? last_dir + 1 : dir_map[i]; // Get last dir name
        printf("%d: %s\n", i, last_dir);
    }

    int choice;
    printf("Enter the index of the directory you want to select: ");
    scanf("%d", &choice);

    if (choice >= 0 && choice < dir_count) {
        printf("You selected: %s\n", dir_map[choice]); // Show full path on selection
        // Execute ./+/2.interact🗯️]a0.+x with selected directory
        char command[MAX_PATH_LEN];
        snprintf(command, sizeof(command), "./+/2.interact🗯️]a0.+x %s", dir_map[choice]);
        int result = system(command);
        if (result != 0) {
            fprintf(stderr, "Error executing command: %s\n", command);
        }
    } else {
        fprintf(stderr, "Invalid index\n");
    }

    return 0;
}
