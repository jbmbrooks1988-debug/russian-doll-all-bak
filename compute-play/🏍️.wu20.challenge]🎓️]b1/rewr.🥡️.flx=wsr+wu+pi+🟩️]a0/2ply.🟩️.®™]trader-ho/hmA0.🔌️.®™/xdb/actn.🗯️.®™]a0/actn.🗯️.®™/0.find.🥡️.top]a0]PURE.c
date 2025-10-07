#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <locale.h>

// Function to print raw bytes of a string for debugging
void print_raw_bytes(const char *str) {
  //  printf("Raw bytes: ");
    for (int i = 0; str[i]; i++) {
        printf("%02x ", (unsigned char)str[i]);
    }
  //  printf("\n");
}

// Function to check if a directory name contains the 🥡 emoji (U+1F961, UTF-8: f0 9f a5 a1)
int has_emoji(const char *name) {
    const char *emoji_utf8 = "\xF0\x9F\xA5\xA1";
   // printf("Checking directory: %s\n", name);
 //   print_raw_bytes(name);
    int has_match = strstr(name, emoji_utf8) != NULL;
   // printf("Has 🥡 (UTF-8 match): %s\n", has_match ? "yes" : "no");
    return has_match;
}

// Function to get the current working directory
char *get_current_dir() {
    char *cwd = getcwd(NULL, 0);
    if (!cwd) {
        perror("getcwd");
        exit(1);
    }
    return cwd;
}

// Function to search for directory with emoji upward
char *find_emoji_dir() {
    char *current = get_current_dir();
    char *original = strdup(current); // Save original path
    DIR *dir;

    int depth = 0;
    while (1) {
        // Extract directory name from path
        char *dir_name = strrchr(current, '/');
        dir_name = dir_name ? dir_name + 1 : current;

        // Check if current directory has the emoji
        if (has_emoji(dir_name)) {
            // Construct relative path
            char *result = malloc(256);
            strcpy(result, "");
            for (int i = 0; i < depth; i++) {
                strcat(result, "../");
            }
            strcat(result, dir_name);
            strcat(result, "/");
            free(current);
            free(original);
            return result;
        }

        // Open current directory to verify accessibility
        dir = opendir(current);
        if (!dir) {
            perror("opendir");
            free(current);
            free(original);
            return NULL;
        }
        closedir(dir);

        // Move up one directory
        char *parent = strdup(current);
        char *last_slash = strrchr(parent, '/');
        if (!last_slash || last_slash == parent) {
            free(parent);
            free(current);
            free(original);
          //  printf("Reached root or error, no 🥡 directory found.\n");
            return NULL; // Reached root or error
        }
        *last_slash = '\0'; // Remove last component
        if (chdir(parent) != 0) {
            perror("chdir");
            free(parent);
            free(current);
            free(original);
            return NULL;
        }
        free(current);
        current = get_current_dir();
        free(parent);
        depth++;

        // Restore original directory for cleanup
        if (chdir(original) != 0) {
            perror("chdir restore");
        }
    }
}

int main() {
    // Set locale to UTF-8
    if (!setlocale(LC_ALL, "en_US.UTF-8")) {
        //fprintf(stderr, "Warning: Could not set locale to UTF-8\n");
    }
  //  printf("Starting directory: %s\n", get_current_dir());
    char *path = find_emoji_dir();
    if (path) {
       // printf("Found path: %s\n", path);
        printf("%s\n", path);
        free(path);
    } else {
        printf("No directory with 🥡 emoji found.\n");
    }
    return 0;
}
