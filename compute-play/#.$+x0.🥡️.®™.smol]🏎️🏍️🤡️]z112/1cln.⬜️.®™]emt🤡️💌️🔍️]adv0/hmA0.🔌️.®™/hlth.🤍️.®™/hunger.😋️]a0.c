#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For chdir

#define MAX_LINE 256
#define MAX_LINES 100

int main() {
   if (chdir("../") != 0) {
        perror("chdir failed");
        return 1;
    }


    FILE *file = fopen("../../state.txt", "r");
    if (!file) {
        file = fopen("../../state.txt", "w");
        if (!file) {
            perror("Cannot open state.txt");
            return 1;
        }
        fprintf(file, "hunger 0\n");
        fclose(file);
        return 0;
    }

    char lines[MAX_LINES][MAX_LINE];
    int line_count = 0;
    int hunger_found = 0;
    int hunger_value = 0;

    while (line_count < MAX_LINES && fgets(lines[line_count], MAX_LINE, file)) {
        if (strncmp(lines[line_count], "hunger ", 7) == 0) {
            hunger_found = 1;
            hunger_value = atoi(lines[line_count] + 7);
        }
        line_count++;
    }
    fclose(file);

    if (!hunger_found && line_count < MAX_LINES) {
        strcpy(lines[line_count], "hunger 0\n");
        line_count++;
    }

    file = fopen("../../state.txt", "w");
    if (!file) {
        perror("Cannot open state.txt for writing");
        return 1;
    }

    for (int i = 0; i < line_count; i++) {
        if (strncmp(lines[i], "hunger ", 7) == 0) {
            fprintf(file, "hunger %d\n", hunger_value + 1);
        } else {
            fputs(lines[i], file);
        }
    }

    fclose(file);
    return 0;
}
