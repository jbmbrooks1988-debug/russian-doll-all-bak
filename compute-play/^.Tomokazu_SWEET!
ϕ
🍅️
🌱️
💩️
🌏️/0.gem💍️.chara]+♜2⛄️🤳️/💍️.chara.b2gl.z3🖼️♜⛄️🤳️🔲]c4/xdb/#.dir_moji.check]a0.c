#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_LINE 256

int main() {
    DIR *dir;
    struct dirent *entry;

    // Open current directory
    dir = opendir(".");
    if (!dir) {
        printf("Error: Cannot open current directory.\n");
        return 1;
    }

    printf("Piece Emojis:\n");
    printf("-------------\n");

    while ((entry = readdir(dir)) != NULL) {
        // Check if it's a .dir directory
        if (strstr(entry->d_name, ".dir") != NULL) {
            char state_path[100];
            sprintf(state_path, "%s/state.txt", entry->d_name);

            FILE *f = fopen(state_path, "r");
            if (!f) {
                printf("%-20s -> [no state.txt]\n", entry->d_name);
                continue;
            }

            char line[MAX_LINE];
            int found = 0;
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "symbol :", 8) == 0) {
                    // Extract emoji part
                    char *emoji = line + 8;
                    while (*emoji == ' ') emoji++; // skip spaces
                    // Remove newline
                    char temp[10] = {0};
                    int i = 0;
                    while (emoji[i] && emoji[i] != '\n' && emoji[i] != '\r' && i < 9) {
                        temp[i] = emoji[i];
                        i++;
                    }
                    printf("%-20s -> %s\n", entry->d_name, temp);
                    found = 1;
                    break;
                }
            }
            fclose(f);

            if (!found) {
                printf("%-20s -> [no emoji]\n", entry->d_name);
            }
        }
    }

    closedir(dir);
    return 0;
}
