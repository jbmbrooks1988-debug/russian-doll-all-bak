#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void search_directory(const char *path, const char *word) {
    DIR *dir;
    struct dirent *ent;
    struct stat statbuf;
    char fullpath[512];
    int found = 0;

    // Open the directory
    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    printf("\nSearching in directory: %s\n", path);

    // Iterate through directory entries
    while ((ent = readdir(dir)) != NULL) {
        // Skip . and .. entries
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        // Construct full path
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);

        // Get file/directory info
        if (stat(fullpath, &statbuf) == -1) {
            perror("stat");
            continue;
        }

        // If it's a directory, recurse
        if (S_ISDIR(statbuf.st_mode)) {
            search_directory(fullpath, word);
        }
        // If it's a regular file, search it
        else if (S_ISREG(statbuf.st_mode)) {
            FILE *file = fopen(fullpath, "r");
            if (file == NULL) {
                perror("fopen");
                continue;
            }

            // Search for the word in the file
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), file) != NULL) {
                if (strstr(buffer, word) != NULL) {
                    printf("Found word in file: %s\n", fullpath);
                    found = 1;
                    break;
                }
            }
            fclose(file);
        }
    }

    closedir(dir);

    // Print message if no matches found in this directory
    if (!found) {
        printf("No files containing '%s' were found in %s\n", word, path);
    }
}

int main(int argc, char *argv[]) {
    char word[256];

    // Check for correct number of arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory_path>\n", argv[0]);
        return 1;
    }

    // Get the word from the user
    printf("Enter a word: ");
    scanf("%255s", word);

    // Start search from the specified directory
    search_directory(argv[1], word);

    return 0;
}
