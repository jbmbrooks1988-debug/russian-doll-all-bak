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
#define STRICT_EMOJI_FILTER 1 // Set to 1 to only list dirs with emojis, 0 to include their subdirs

// --- Functionality to find top directory with 🥡️ ---

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

// --- Functionality to list directories with emoji filtering and ®™ requirement ---

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

// Function to read the path for 🗯️ from ../🔐️.txt
char *get_interact_path() {
    FILE *file = fopen("../🔐️.txt", "r");
    if (!file) {
        perror("fopen ../🔐️.txt");
        return NULL;
    }

    char line[MAX_PATH_LEN];
    char *interact_path = NULL;
    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;
        // Look for 🗯️ key
        if (strncmp(line, "🗯️", strlen("🗯️")) == 0) {
            char *value = strchr(line, ' ');
            if (value) {
                value++; // Move past the space
                interact_path = strdup(value);
            }
            break;
        }
    }

    fclose(file);
    return interact_path;
}

int main() {
    // Step 1: Find the top directory with 🥡️
    char *top_dir = find_emoji_dir("🥡️");
    if (!top_dir) {
        fprintf(stderr, "No directory with 🥡️ emoji found.\n");
        return 1;
    }
    
     if (chdir("../") != 0) {
        perror("chdir failed");
        return 1;
    }

    // Step 2: Read emojis from interact_lut.txt
    emoji_count = read_emojis("interact_lut.txt", emojis);
    if (emoji_count == 0) {
        fprintf(stderr, "No emojis found in interact_lut.txt\n");
        free(top_dir);
        return 1;
    }

    // Step 3: List directories with emojis recursively from the top directory
    list_directories_recursive(top_dir);
    free(top_dir);

    // Step 4: Write to interact_map.txt
    FILE *interact_map_file = fopen("interact_map.txt", "w");
    if (!interact_map_file) {
        perror("fopen interact_map.txt");
        return 1;
    }

    for (int i = 0; i < dir_count; i++) {
        fprintf(interact_map_file, "%s\n", dir_map[i]);
    }
    fclose(interact_map_file);

    // Step 5: Let user choose a directory, showing only the last directory name
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
        // Get path for 🗯️ from ../🔐️.txt
        char *interact_path = get_interact_path();
        char command[MAX_PATH_LEN];
        if (interact_path) {
            // Use path from 🔐️.txt
            snprintf(command, sizeof(command), "%s/+x/2.interact🗯️]b1.+x %s", interact_path, dir_map[choice]);
            free(interact_path);
        } else {
            // Fallback to current working directory
            char *cwd = getcwd(NULL, 0);
            if (!cwd) {
                perror("getcwd");
                return 1;
            }
            snprintf(command, sizeof(command), "%s/+x/2.interact🗯️]b1.+x %s", cwd, dir_map[choice]);
            free(cwd);
        }
        // Execute the command
        int result = system(command);
        if (result != 0) {
            fprintf(stderr, "Error executing command: %s\n", command);
        }
    } else {
        fprintf(stderr, "Invalid index\n");
    }

    return 0;
}
