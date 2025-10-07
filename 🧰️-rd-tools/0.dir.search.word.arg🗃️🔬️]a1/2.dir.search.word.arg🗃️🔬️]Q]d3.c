#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <locale.h>

int search_directory(const char *path, const char *word) {
    DIR *dir;
    struct dirent *ent;
    struct stat statbuf;
    char fullpath[4096];
    int found = 0;

    dir = opendir(path);
    if (dir == NULL) {
        return 0;
    }

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);

        if (stat(fullpath, &statbuf) == -1) {
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            found += search_directory(fullpath, word);
        }
        else if (S_ISREG(statbuf.st_mode)) {
            FILE *file = fopen(fullpath, "r");
            if (file == NULL) {
                continue;
            }

            char buffer[4096];
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
    return found;
}

int main(int argc, char *argv[]) {
    char word[256];

    if (setlocale(LC_ALL, "") == NULL) {
        fprintf(stderr, "Warning: Could not set locale for UTF-8 support\n");
    }

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory_path>\n", argv[0]);
        return 1;
    }

    printf("Enter a word: ");
    scanf("%255s", word);

    int found = search_directory(argv[1], word);

    if (!found) {
        printf("nothing was found\n");
    }

    return 0;
}
